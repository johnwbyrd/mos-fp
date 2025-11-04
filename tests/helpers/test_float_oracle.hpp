#pragma once

#include <cmath>
#include <opine/opine.hpp>

namespace opine::test_helpers {

// Convert OPINE format storage bits to native float
//
// This serves as our ORACLE - native float arithmetic is proven correct by
// CPU vendors and extensive testing. We trust native float as the reference
// implementation.
//
// Limitations:
// - Does not handle NaN or Infinity yet (TODO)
// - Assumes IEEE 754 binary32 native float
//
// Template parameters:
//   Format: The OPINE format descriptor (e.g., fp8_e5m2)
template <typename Format>
float to_native_float(typename Format::storage_type bits) {
  // Unpack the bits to get sign, exponent, and mantissa
  auto unpacked = unpack<Format>(bits);

  // Extract sign
  bool sign = unpacked.sign;

  // Extract biased exponent as signed integer for arithmetic
  // We need more than Format::exp_bits to hold intermediate values
  int biased_exp = static_cast<int>(unpacked.exponent);

  // Compute true (unbiased) exponent
  int true_exp = biased_exp - Format::exp_bias;

  // Extract mantissa bits (remove guard bits if any)
  // Guard bits are always the LSBs after unpacking
  constexpr int guard_shift = rounding_policies::DefaultRoundingPolicy::guard_bits;

  // Mantissa as integer (without implicit bit yet)
  using mant_int_type = uint_t<Format::mant_bits + 1, typename Format::type_policy>;
  mant_int_type mant_int = static_cast<mant_int_type>(
      unpacked.mantissa >> guard_shift);

  // Handle denormal vs normal numbers
  float mantissa_value;

  if (biased_exp == 0) {
    // Denormal number: exponent is min, no implicit bit
    // Mantissa is 0.fraction (leading zero)
    if constexpr (Format::has_implicit_bit) {
      // Remove the implicit bit position (which should be 0 for denormals)
      mant_int &= (mant_int_type{1} << Format::mant_bits) - 1;
    }
    mantissa_value = static_cast<float>(mant_int);

    // Denormals use the minimum exponent value
    true_exp = 1 - Format::exp_bias;
  } else {
    // Normal number: implicit bit is 1
    if constexpr (Format::has_implicit_bit) {
      // Remove the implicit bit from the unpacked mantissa to get stored bits
      mant_int &= (mant_int_type{1} << Format::mant_bits) - 1;
      // Add implicit 1 as integer: 1.fraction = (2^M + fraction)
      mant_int |= (mant_int_type{1} << Format::mant_bits);
    }
    mantissa_value = static_cast<float>(mant_int);
  }

  // Normalize mantissa to [1.0, 2.0) by dividing by 2^M
  mantissa_value /= static_cast<float>(mant_int_type{1} << Format::mant_bits);

  // Compute final value: mantissa * 2^exp
  float value = mantissa_value * std::pow(2.0f, static_cast<float>(true_exp));

  // Apply sign
  return sign ? -value : value;
}

// Convert native float to OPINE format storage bits with specified rounding
//
// This is the inverse of to_native_float(). It performs the conversion that
// would happen if you cast a native float to the OPINE format.
//
// This is used to test: does pack(from_components(...)) produce the same
// result as casting native float → OPINE format?
//
// Limitations:
// - Does not handle NaN or Infinity yet (TODO)
// - Does not handle denormals yet (TODO: implement gradual underflow)
// - Saturates on overflow instead of producing Inf
//
// Template parameters:
//   Format: The OPINE format descriptor
//   RoundingPolicy: The rounding policy to use (TowardZero, ToNearestTiesToEven)
template <typename Format, typename RoundingPolicy>
typename Format::storage_type from_native_float(float value) {
  // Handle zero specially (including signed zero)
  if (value == 0.0f) {
    bool sign = std::signbit(value);
    typename Format::storage_type result = 0;
    if (sign) {
      result |= (typename Format::storage_type{1} << Format::sign_offset);
    }
    return result;
  }

  // Extract sign and work with absolute value
  bool sign = value < 0.0f;
  if (sign) {
    value = -value;
  }

  // Use frexp to extract mantissa and exponent
  // frexp returns mantissa in [0.5, 1.0) and exponent such that value = mantissa * 2^exp
  int exp_frexp;
  float mantissa_frac = std::frexp(value, &exp_frexp);

  // Adjust to get mantissa in [1.0, 2.0) range
  mantissa_frac *= 2.0f;
  exp_frexp -= 1;

  // Check for exponent overflow
  constexpr int max_biased_exp = (1 << Format::exp_bits) - 1;
  constexpr int max_true_exp = max_biased_exp - Format::exp_bias;

  if (exp_frexp > max_true_exp) {
    // Overflow: saturate to maximum representable value
    // TODO: should produce Inf if Format supports it
    typename Format::storage_type result = 0;

    // Sign bit
    if (sign) {
      result |= (typename Format::storage_type{1} << Format::sign_offset);
    }

    // Max exponent (not all 1s, which might be reserved for Inf/NaN)
    result |= (typename Format::storage_type{max_biased_exp - 1}
               << Format::exp_offset);

    // Max mantissa
    result |= (((typename Format::storage_type{1} << Format::mant_bits) - 1)
               << Format::mant_offset);

    return result;
  }

  // Check for exponent underflow
  constexpr int min_true_exp = 1 - Format::exp_bias;

  if (exp_frexp < min_true_exp) {
    // Underflow: flush to zero
    // TODO: implement gradual underflow (denormals)
    typename Format::storage_type result = 0;
    if (sign) {
      result |= (typename Format::storage_type{1} << Format::sign_offset);
    }
    return result;
  }

  // Normal number: compute biased exponent
  int biased_exp = exp_frexp + Format::exp_bias;

  // Extract mantissa fraction (remove implicit leading 1)
  mantissa_frac -= 1.0f;

  // Scale mantissa to integer, including guard bits for rounding
  // Total bits needed: M stored bits + G guard bits
  constexpr int total_mant_bits = Format::mant_bits + RoundingPolicy::guard_bits;

  // Scale factor: 2^(M + G)
  float scale = static_cast<float>(uint32_t{1} << total_mant_bits);

  // Convert to integer (rounding in native float happens here)
  // We add 0.5f to round to nearest integer during the cast
  uint32_t mantissa_int = static_cast<uint32_t>(mantissa_frac * scale + 0.5f);

  // Build UnpackedFloat structure
  UnpackedFloat<Format, RoundingPolicy> unpacked;
  unpacked.sign = sign;
  unpacked.exponent = static_cast<typename Format::exponent_type>(biased_exp);

  // Set mantissa with implicit bit (if format has one)
  if constexpr (Format::has_implicit_bit) {
    // Add the implicit bit at position (M + G)
    mantissa_int |= (uint32_t{1} << total_mant_bits);
  }

  // Cast to the exact mantissa type needed by UnpackedFloat
  unpacked.mantissa = static_cast<typename decltype(unpacked)::mantissa_type>(
      mantissa_int);

  // Pack with the specified rounding policy
  return pack<Format, RoundingPolicy>(unpacked);
}

// Helper: Compare two FP8 values considering they might be NaN
// Returns true if they are equal OR both are NaN
template <typename Format>
bool fp_equal_or_both_nan(typename Format::storage_type a,
                          typename Format::storage_type b) {
  if (a == b) {
    return true;
  }

  // Check if both are NaN (all exponent bits set, mantissa non-zero)
  // TODO: This is format-specific, need to generalize
  // For now, just return false if not equal
  return false;
}

// Helper: Check if a value is zero (positive or negative)
template <typename Format>
bool is_zero(typename Format::storage_type bits) {
  // Zero has exponent=0 and mantissa=0 (sign can be either)
  auto unpacked = unpack<Format>(bits);
  return unpacked.exponent == 0 && unpacked.mantissa == 0;
}

} // namespace opine::test_helpers
