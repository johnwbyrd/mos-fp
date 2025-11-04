#pragma once

#include <opine/core/types.hpp>

namespace opine::inline v1 {

// Format descriptor for IEEE 754-like floating point formats
// Supports arbitrary bit layouts with optional padding
template <int SignBits,        // Number of sign bits (typically 1)
          int SignOffset,      // Bit position of sign field (from LSB)
          int ExpBits,         // Number of exponent bits
          int ExpOffset,       // Bit position of exponent field (from LSB)
          int MantBits,        // Number of mantissa bits (stored, not including
                               // implicit)
          int MantOffset,      // Bit position of mantissa field (from LSB)
          int TotalBits,       // Total storage size in bits
          bool HasImplicitBit, // true if format has implicit leading 1
          int ExponentBias = -1, // Exponent bias (-1 = auto: 2^(ExpBits-1) - 1)
          typename TypePolicy = DefaultTypeSelectionPolicy>
struct FormatDescriptor {
  // Format specification
  static constexpr int sign_bits = SignBits;
  static constexpr int sign_offset = SignOffset;
  static constexpr int exp_bits = ExpBits;
  static constexpr int exp_offset = ExpOffset;
  static constexpr int mant_bits = MantBits;
  static constexpr int mant_offset = MantOffset;
  static constexpr int total_bits = TotalBits;
  static constexpr bool has_implicit_bit = HasImplicitBit;

  // Exponent bias: auto-calculate if not specified
  static constexpr int exp_bias =
      (ExponentBias == -1) ? ((1 << (ExpBits - 1)) - 1) : ExponentBias;

  // Type policy for type selection
  using type_policy = TypePolicy;

  // Storage and field types
  using storage_type = uint_t<TotalBits, TypePolicy>;
  using exponent_type = uint_t<ExpBits, TypePolicy>;
  using mantissa_storage_type = uint_t<MantBits, TypePolicy>;

  // Helper: Check if this is standard IEEE 754 layout (no padding)
  static constexpr bool is_standard_layout() {
    return SignBits == 1 && SignOffset == ExpOffset + ExpBits &&
           ExpOffset == MantOffset + MantBits && MantOffset == 0 &&
           TotalBits == SignBits + ExpBits + MantBits;
  }

  // Compile-time validation
  static_assert(SignBits > 0, "Sign must have at least 1 bit");
  static_assert(ExpBits > 0, "Exponent must have at least 1 bit");
  static_assert(MantBits > 0, "Mantissa must have at least 1 bit");
  static_assert(TotalBits >= SignBits + ExpBits + MantBits,
                "Total bits must be at least sum of field bits");
  static_assert(SignOffset + SignBits <= TotalBits,
                "Sign field extends beyond total bits");
  static_assert(ExpOffset + ExpBits <= TotalBits,
                "Exponent field extends beyond total bits");
  static_assert(MantOffset + MantBits <= TotalBits,
                "Mantissa field extends beyond total bits");
};

// Convenience alias for standard IEEE 754 layouts (no padding)
// Bit layout: [Sign (MSB)][Exponent][Mantissa (LSB)]
template <int ExpBits, int MantBits,
          typename TypePolicy = DefaultTypeSelectionPolicy>
using IEEE_Format = FormatDescriptor<1,                  // SignBits
                                     ExpBits + MantBits, // SignOffset (at MSB)
                                     ExpBits,            // ExpBits
                                     MantBits,           // ExpOffset
                                     MantBits,           // MantBits
                                     0,                  // MantOffset (at LSB)
                                     1 + ExpBits + MantBits, // TotalBits
                                     true, // HasImplicitBit (standard IEEE)
                                     -1,   // Auto bias
                                     TypePolicy>;

// Common format definitions
using FP8_E5M2 = IEEE_Format<5, 2>;
using FP8_E4M3 = IEEE_Format<4, 3>;
using FP16 = IEEE_Format<5, 10>;
using Binary32 = IEEE_Format<8, 23>;
using Binary64 = IEEE_Format<11, 52>;

} // namespace opine::inline v1
