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

// Default rounding policy
using DefaultRoundingPolicy = TowardZero;

/*
 * Future rounding policies (to be implemented):
 *
 * struct ToNearestTiesToEven {
 *     static constexpr int guard_bits = 3;  // G, R, S
 *
 *     template<typename Format, typename MantissaType>
 *     static constexpr auto round_mantissa(MantissaType wide_mantissa, bool
 * is_negative);
 * };
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
