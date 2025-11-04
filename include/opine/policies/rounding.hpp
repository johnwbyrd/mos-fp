#pragma once

#include <concepts>
#include <opine/core/types.hpp>

namespace opine::inline v1::rounding_policies {

// Concept: A rounding policy must provide guard_bits and round_mantissa method
template <typename T>
concept RoundingPolicy = requires {
  { T::guard_bits } -> std::convertible_to<int>;
};

// Round toward zero (truncate)
// Simplest rounding mode - no guard bits needed, just discard precision
struct TowardZero {
  static constexpr int guard_bits = 0;

  // Round mantissa by removing guard bits and implicit bit
  // For TowardZero with 0 guard bits, this just removes the implicit bit if
  // present
  template <typename Format, typename MantissaType>
  static constexpr auto round_mantissa(MantissaType wide_mantissa,
                                       bool is_negative // Unused for TowardZero
  ) {
    using result_type = typename Format::mantissa_storage_type;

    // No guard bits to remove, just extract the stored mantissa bits
    if constexpr (Format::has_implicit_bit) {
      // Mantissa layout: [implicit bit][M stored bits]
      // Mask off the implicit bit (MSB), keep only the M stored bits
      constexpr MantissaType mant_mask =
          (MantissaType{1} << Format::mant_bits) - 1;
      return static_cast<result_type>(wide_mantissa & mant_mask);
    } else {
      // No implicit bit, mantissa is already in correct form
      return static_cast<result_type>(wide_mantissa);
    }
  }
};

// Round to nearest, ties to even (IEEE 754 default)
//
// This is the most commonly used rounding mode and the IEEE 754 default.
// When a value is exactly halfway between two representable values, it rounds
// to the one with an even least significant bit.
//
// Requires 3 guard bits: Guard (G), Round (R), and Sticky (S)
//
// Rounding decision table:
//   G=0, R=x, S=x: Less than halfway → Round down (truncate)
//   G=1, R=0, S=0: Exactly halfway → Round to even (check LSB of mantissa)
//   G=1, R=1, S=x: More than halfway → Round up
//   G=1, R=0, S=1: More than halfway → Round up
//
// This can be simplified to comparing the 3-bit value GRS (0-7):
//   GRS < 4 (G=0): Round down
//   GRS = 4 (G=1, R=0, S=0): Exactly halfway, round to even
//   GRS > 4 (G=1, at least one of R or S set): Round up
//
// See docs/design/bits.md for detailed explanation of guard bits.
struct ToNearestTiesToEven {
  static constexpr int guard_bits = 3; // G, R, S bits required

  // Round mantissa from wide format (with guard bits) to storage format
  //
  // Input mantissa layout:
  //   [implicit bit (if any)][M stored mantissa bits][G bit][R bit][S bit]
  //
  // Output: M stored mantissa bits, with implicit bit removed
  //
  // Note: This implementation does NOT handle mantissa overflow.
  // When rounding causes the mantissa to overflow (e.g., rounding 1.111...
  // to 10.000...), the exponent should be incremented and mantissa reset.
  // This overflow handling must be done in pack() - see TODO below.
  //
  // TODO: Modify pack() to detect mantissa overflow (rounded_mant > max_mant)
  // and handle it by incrementing exponent and resetting mantissa to 0.
  template <typename Format, typename MantissaType>
  static constexpr auto round_mantissa(
      MantissaType wide_mantissa,
      bool is_negative // Unused for round-to-nearest modes
  ) {
    using result_type = typename Format::mantissa_storage_type;

    // Step 1: Extract the stored mantissa bits by shifting away guard bits
    //
    // Before shift: [...stored bits...][G][R][S]
    // After shift:  [...stored bits...]
    //
    // For formats with implicit bit, this includes the implicit bit in the
    // shifted value, which we'll mask off in the next step.
    MantissaType stored_bits = wide_mantissa >> guard_bits;

    // Step 2: Remove implicit bit if format has one
    //
    // For IEEE 754 formats, the implicit bit (leading 1 for normalized numbers)
    // is not stored. We need to mask it off to get only the M stored bits.
    //
    // Mask: (2^M - 1) = bits 0 through M-1 set
    if constexpr (Format::has_implicit_bit) {
      constexpr MantissaType mant_mask =
          (MantissaType{1} << Format::mant_bits) - 1;
      stored_bits &= mant_mask;
    }

    // Step 3: Extract the 3-bit guard value (GRS) using appropriately-sized type
    //
    // This is the core of OPINE's philosophy: use EXACTLY the number of bits
    // needed. We need 3 bits to represent values 0-7, so we use uint_t<3>.
    //
    // On Clang with _BitInt: uint_t<3> = unsigned _BitInt(3) - exactly 3 bits
    // On GCC/MSVC fallback: uint_t<3> = uint_fast8_t - at least 3 bits
    //
    // This extracts the low 3 bits (GRS) from wide_mantissa before the shift.
    using guard_bits_type = uint_t<3, typename Format::type_policy>;
    guard_bits_type grs = static_cast<guard_bits_type>(wide_mantissa & 0b111);

    // Step 4: Determine if we should round up
    //
    // Rounding logic (see table in struct comment):
    //   grs > 4: More than halfway, always round up
    //   grs = 4: Exactly halfway, round to even (check LSB of stored_bits)
    //   grs < 4: Less than halfway, round down (don't increment)
    //
    // For the "round to even" case (grs == 4), we check if the LSB of
    // stored_bits is 1. If so, round up to make it even (LSB becomes 0).
    // If LSB is already 0 (even), don't round up.
    bool round_up = (grs > guard_bits_type{4}) ||
                    (grs == guard_bits_type{4} && (stored_bits & 1));

    // Step 5: Apply rounding
    if (round_up) {
      stored_bits += 1;

      // Check for mantissa overflow
      //
      // When all mantissa bits are 1 and we round up, we get overflow:
      // Example: 1.111 + 0.001 (round up) = 10.000
      //
      // In this case, the mantissa should become 0 and the exponent should
      // increment by 1. However, we can't modify the exponent here - that
      // must be handled by pack().
      //
      // TODO: This overflow detection is here for documentation, but the
      // actual handling must be in pack(). Either:
      //   1. Modify pack() to detect stored_bits > max_mant after rounding
      //   2. Change round_mantissa to return {mantissa, overflow_flag}
      //   3. Saturate here (wrong, but safe) until proper handling is added
      //
      // For now, we let the overflow propagate and rely on the caller to
      // handle it correctly.
      constexpr MantissaType max_mant =
          (MantissaType{1} << Format::mant_bits) - 1;
      if (stored_bits > max_mant) {
        // Mantissa overflowed - this should trigger exponent increment in pack()
        // For now, document this is NOT handled yet
        //
        // WARNING: This case is not yet properly handled. The packed result
        // will be incorrect if mantissa overflow occurs. This typically
        // happens when rounding the maximum representable mantissa value.
      }
    }

    // Step 6: Return the rounded mantissa value
    //
    // Cast to result_type (mantissa_storage_type), which is sized for exactly
    // M bits (the stored mantissa width, without implicit bit or guard bits).
    return static_cast<result_type>(stored_bits);
  }
};

// Default rounding policy
using DefaultRoundingPolicy = TowardZero;

/*
 * Future rounding policies (to be implemented):
 *
 * struct ToNearestTiesAwayFromZero {
 *     static constexpr int guard_bits = 3;  // G, R, S
 *
 *     template<typename Format, typename MantissaType>
 *     static constexpr auto round_mantissa(MantissaType wide_mantissa, bool
 * is_negative);
 * };
 *
 * struct TowardPositive {
 *     static constexpr int guard_bits = 1;
 *
 *     template<typename Format, typename MantissaType>
 *     static constexpr auto round_mantissa(MantissaType wide_mantissa, bool
 * is_negative);
 * };
 *
 * struct TowardNegative {
 *     static constexpr int guard_bits = 1;
 *
 *     template<typename Format, typename MantissaType>
 *     static constexpr auto round_mantissa(MantissaType wide_mantissa, bool
 * is_negative);
 * };
 */

} // namespace opine::inline v1::rounding_policies
