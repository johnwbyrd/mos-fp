#pragma once

#include <opine/core/unpacked.hpp>

namespace opine::inline v1 {

// Unpack a floating point value from storage format to computational format
//
// Extracts sign, exponent, and mantissa fields from the storage bits
// and constructs an UnpackedFloat with:
// - Sign bit extracted
// - Exponent in biased encoding
// - Mantissa with implicit bit (if applicable) and space for guard bits
//
// Guard bits are initialized to zero; they will be populated during arithmetic
// operations.
//
// Denormal handling: For formats with implicit bits, when exponent == 0,
// the implicit bit is set to 0 (denormal). For normal numbers (exponent != 0),
// the implicit bit is set to 1.
template <typename Format,
          typename RoundingPolicy = rounding_policies::DefaultRoundingPolicy>
constexpr UnpackedFloat<Format, RoundingPolicy>
unpack(typename Format::storage_type bits) {
  UnpackedFloat<Format, RoundingPolicy> result{};

  // Extract sign bit
  constexpr auto sign_mask =
      (typename Format::storage_type{1} << Format::sign_bits) - 1;
  result.sign = static_cast<bool>((bits >> Format::sign_offset) & sign_mask);

  // Extract exponent
  constexpr auto exp_mask =
      (typename Format::storage_type{1} << Format::exp_bits) - 1;
  result.exponent = static_cast<typename Format::exponent_type>(
      (bits >> Format::exp_offset) & exp_mask);

  // Extract stored mantissa bits
  constexpr auto mant_mask =
      (typename Format::storage_type{1} << Format::mant_bits) - 1;
  auto mant_stored = (bits >> Format::mant_offset) & mant_mask;

  // Build mantissa with implicit bit and guard bits
  // Layout: [implicit bit (if any)][M stored bits][guard bits (zero)]
  using mantissa_type =
      typename UnpackedFloat<Format, RoundingPolicy>::mantissa_type;

  if constexpr (Format::has_implicit_bit) {
    // Check if this is a normal number (exponent != 0) or denormal (exponent ==
    // 0)
    bool is_normal = (result.exponent != 0);

    // Shift stored bits left to make room for guard bits at LSB
    auto mant_shifted = static_cast<mantissa_type>(mant_stored)
                        << RoundingPolicy::guard_bits;

    if (is_normal) {
      // Normal number: set implicit bit to 1
      constexpr int implicit_bit_pos =
          Format::mant_bits + RoundingPolicy::guard_bits;
      result.mantissa = mant_shifted | (mantissa_type{1} << implicit_bit_pos);
    } else {
      // Denormal: implicit bit is 0
      result.mantissa = mant_shifted;
    }
  } else {
    // No implicit bit - just shift for guard bits
    result.mantissa = static_cast<mantissa_type>(mant_stored)
                      << RoundingPolicy::guard_bits;
  }

  return result;
}

// Pack a floating point value from computational format to storage format
//
// Takes an UnpackedFloat (with sign, biased exponent, and wide mantissa
// including guard bits) and packs it into the storage representation.
//
// The rounding policy determines how the wide mantissa (with guard bits) is
// rounded to fit in the storage mantissa field. The implicit bit (if present)
// is removed before packing.
//
// Note: This implementation does NOT handle mantissa overflow from rounding
// (where rounding causes the mantissa to overflow and requires incrementing
// the exponent). This is deferred to future implementation.
template <typename Format,
          typename RoundingPolicy = rounding_policies::DefaultRoundingPolicy>
constexpr typename Format::storage_type
pack(const UnpackedFloat<Format, RoundingPolicy> &unpacked) {
  typename Format::storage_type result = 0;

  // Pack sign bit
  auto sign_value =
      static_cast<typename Format::storage_type>(unpacked.sign ? 1 : 0);
  result |= (sign_value << Format::sign_offset);

  // Pack exponent
  auto exp_value =
      static_cast<typename Format::storage_type>(unpacked.exponent);
  result |= (exp_value << Format::exp_offset);

  // Round mantissa (removes guard bits and implicit bit)
  auto rounded_mant = RoundingPolicy::template round_mantissa<Format>(
      unpacked.mantissa,
      unpacked.sign // Pass sign for directional rounding modes
  );

  // Pack mantissa
  auto mant_value = static_cast<typename Format::storage_type>(rounded_mant);
  result |= (mant_value << Format::mant_offset);

  return result;
}

} // namespace opine::inline v1
