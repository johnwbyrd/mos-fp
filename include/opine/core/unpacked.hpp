#pragma once

#include <opine/core/format.hpp>
#include <opine/policies/rounding.hpp>

namespace opine::inline v1 {

// Unpacked floating point representation for computation
// This structure holds the components of a floating point value in a form
// suitable for arithmetic operations, with space for guard bits.
//
// Mantissa layout: [implicit bit (MSB if present)][M stored bits][G guard bits
// (LSB)]
//
// The mantissa field is sized to hold:
// - The implicit leading bit (if format has one)
// - The stored mantissa bits (M bits)
// - Guard bits for rounding (G bits, determined by rounding policy)
//
// Guard bits are initially zero after unpacking and get populated during
// arithmetic operations (multiply, add, etc.)
template <typename Format,
          typename RoundingPolicy = rounding_policies::DefaultRoundingPolicy>
struct UnpackedFloat {
  // Sign: true = negative, false = positive
  bool sign;

  // Exponent: stored in biased encoding
  // (subtract Format::exp_bias to get true exponent)
  typename Format::exponent_type exponent;

  // Mantissa: includes space for implicit bit (if any) and guard bits
  static constexpr int mantissa_bits = Format::mant_bits +
                                       (Format::has_implicit_bit ? 1 : 0) +
                                       RoundingPolicy::guard_bits;

  using mantissa_type = uint_t<mantissa_bits, typename Format::type_policy>;

  mantissa_type mantissa;

  // Helper: Get the position of the implicit bit (if present)
  static constexpr int implicit_bit_position() {
    if constexpr (Format::has_implicit_bit) {
      return Format::mant_bits + RoundingPolicy::guard_bits;
    } else {
      return -1; // No implicit bit
    }
  }

  // Helper: Get mask for the implicit bit
  static constexpr mantissa_type implicit_bit_mask() {
    if constexpr (Format::has_implicit_bit) {
      return mantissa_type{1} << implicit_bit_position();
    } else {
      return 0;
    }
  }

  // Helper: Get mask for stored mantissa bits (excluding implicit bit and guard
  // bits)
  static constexpr mantissa_type stored_bits_mask() {
    constexpr mantissa_type all_bits =
        (mantissa_type{1} << Format::mant_bits) - 1;
    return all_bits << RoundingPolicy::guard_bits;
  }

  // Helper: Get mask for guard bits
  static constexpr mantissa_type guard_bits_mask() {
    if constexpr (RoundingPolicy::guard_bits > 0) {
      return (mantissa_type{1} << RoundingPolicy::guard_bits) - 1;
    } else {
      return 0;
    }
  }
};

} // namespace opine::inline v1
