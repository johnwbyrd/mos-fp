#include <cstdio>
#include <opine/opine.hpp>

using namespace opine;

// Test helper: verify pack(unpack(x)) == x for all values
// For formats with padding, this tests that pack produces canonical form
// (padding bits are zero) and that the significant bits are preserved
template <typename Format> constexpr bool test_identity_exhaustive() {
  using storage_type = typename Format::storage_type;
  constexpr int total_values = 1 << Format::total_bits;

  // Create mask for all non-padding bits (significant bits)
  constexpr storage_type sign_mask =
      ((storage_type{1} << Format::sign_bits) - 1) << Format::sign_offset;
  constexpr storage_type exp_mask = ((storage_type{1} << Format::exp_bits) - 1)
                                    << Format::exp_offset;
  constexpr storage_type mant_mask =
      ((storage_type{1} << Format::mant_bits) - 1) << Format::mant_offset;
  constexpr storage_type significant_mask = sign_mask | exp_mask | mant_mask;

  for (int i = 0; i < total_values; ++i) {
    storage_type bits = static_cast<storage_type>(i);
    auto unpacked = unpack<Format>(bits);
    auto repacked = pack<Format>(unpacked);

    // Compare only the significant bits (ignore padding)
    // pack() should produce canonical form with padding bits zero
    if ((repacked & significant_mask) != (bits & significant_mask)) {
      return false;
    }

    // Also verify that pack produces canonical form (padding bits are zero)
    if (repacked != (repacked & significant_mask)) {
      return false;
    }
  }

  return true;
}

// Test helper: verify specific bit field extraction
template <typename Format>
constexpr bool
test_bit_extraction(typename Format::storage_type bits, bool expected_sign,
                    typename Format::exponent_type expected_exp,
                    typename Format::mantissa_storage_type expected_mant) {
  auto unpacked = unpack<Format>(bits);

  // Check sign
  if (unpacked.sign != expected_sign) {
    return false;
  }

  // Check exponent
  if (unpacked.exponent != expected_exp) {
    return false;
  }

  // Check mantissa (extract just the stored bits, not implicit or guard)
  using mantissa_type = typename UnpackedFloat<Format>::mantissa_type;
  constexpr int shift = rounding_policies::DefaultRoundingPolicy::guard_bits;
  constexpr mantissa_type mant_mask =
      (mantissa_type{1} << Format::mant_bits) - 1;
  auto extracted_mant = (unpacked.mantissa >> shift) & mant_mask;

  if (extracted_mant != expected_mant) {
    return false;
  }

  return true;
}

// Test helper: verify implicit bit handling
template <typename Format> constexpr bool test_implicit_bit() {
  using storage_type = typename Format::storage_type;
  using mantissa_type = typename UnpackedFloat<Format>::mantissa_type;

  if constexpr (Format::has_implicit_bit) {
    // Test normal number (exp != 0): implicit bit should be 1
    {
      storage_type bits = 0;
      // Set exponent to non-zero (e.g., 1)
      bits |= (storage_type{1} << Format::exp_offset);

      auto unpacked = unpack<Format>(bits);

      // Check that implicit bit is set
      constexpr int implicit_bit_pos =
          Format::mant_bits +
          rounding_policies::DefaultRoundingPolicy::guard_bits;
      mantissa_type implicit_bit = (unpacked.mantissa >> implicit_bit_pos) & 1;

      if (implicit_bit != 1) {
        return false;
      }
    }

    // Test denormal (exp == 0): implicit bit should be 0
    {
      storage_type bits = 0;
      // Set mantissa to non-zero but exponent to 0
      bits |= (storage_type{1} << Format::mant_offset);

      auto unpacked = unpack<Format>(bits);

      // Check that implicit bit is NOT set
      constexpr int implicit_bit_pos =
          Format::mant_bits +
          rounding_policies::DefaultRoundingPolicy::guard_bits;
      mantissa_type implicit_bit = (unpacked.mantissa >> implicit_bit_pos) & 1;

      if (implicit_bit != 0) {
        return false;
      }
    }
  }

  return true;
}

// Test helper: verify guard bits are zero after unpack
template <typename Format> constexpr bool test_guard_bits_zero() {
  using storage_type = typename Format::storage_type;
  using mantissa_type = typename UnpackedFloat<Format>::mantissa_type;

  constexpr int guard_bits =
      rounding_policies::DefaultRoundingPolicy::guard_bits;

  if constexpr (guard_bits > 0) {
    // Test with various bit patterns
    storage_type bits = 0xFF; // All bits set
    auto unpacked = unpack<Format>(bits);

    // Extract guard bits (low bits of mantissa)
    mantissa_type guard_mask = (mantissa_type{1} << guard_bits) - 1;
    mantissa_type guard_value = unpacked.mantissa & guard_mask;

    if (guard_value != 0) {
      return false;
    }
  }

  return true;
}

// Compile-time tests
// FP8 E5M2 exhaustive identity test
static_assert(test_identity_exhaustive<FP8_E5M2>(),
              "FP8 E5M2: pack(unpack(x)) must equal x for all values");

// FP8 E4M3 exhaustive identity test
static_assert(test_identity_exhaustive<FP8_E4M3>(),
              "FP8 E4M3: pack(unpack(x)) must equal x for all values");

// Bit extraction tests for FP8 E5M2
// Format: [S:1][E:5][M:2]
// Bit pattern: 0b10110011 = 0xB3
// S=1, E=01100=12, M=11=3
static_assert(test_bit_extraction<FP8_E5M2>(0xB3, true, 12, 3),
              "FP8 E5M2: bit extraction test 1");

// Bit pattern: 0b00000001 = 0x01
// S=0, E=00000=0, M=01=1
static_assert(test_bit_extraction<FP8_E5M2>(0x01, false, 0, 1),
              "FP8 E5M2: bit extraction test 2 (denormal)");

// Bit pattern: 0b01111100 = 0x7C
// S=0, E=11111=31, M=00=0
static_assert(test_bit_extraction<FP8_E5M2>(0x7C, false, 31, 0),
              "FP8 E5M2: bit extraction test 3 (max exp)");

// Bit extraction tests for FP8 E4M3
// Format: [S:1][E:4][M:3]
// Bit pattern: 0b10110101 = 0xB5
// S=1, E=0110=6, M=101=5
static_assert(test_bit_extraction<FP8_E4M3>(0xB5, true, 6, 5),
              "FP8 E4M3: bit extraction test 1");

// Bit pattern: 0b00000111 = 0x07
// S=0, E=0000=0, M=111=7
static_assert(test_bit_extraction<FP8_E4M3>(0x07, false, 0, 7),
              "FP8 E4M3: bit extraction test 2 (denormal)");

// Implicit bit tests
static_assert(test_implicit_bit<FP8_E5M2>(), "FP8 E5M2: implicit bit handling");
static_assert(test_implicit_bit<FP8_E4M3>(), "FP8 E4M3: implicit bit handling");

// Guard bits tests
static_assert(test_guard_bits_zero<FP8_E5M2>(),
              "FP8 E5M2: guard bits are zero after unpack");
static_assert(test_guard_bits_zero<FP8_E4M3>(),
              "FP8 E4M3: guard bits are zero after unpack");

// Padded format test
// Create a 12-bit format with padding: [pad:3][S:1][E:4][M:3][pad:1]
using PaddedFormat = FormatDescriptor<1,   // SignBits
                                      8,   // SignOffset (bit 8)
                                      4,   // ExpBits
                                      4,   // ExpOffset (bits 4-7)
                                      3,   // MantBits
                                      1,   // MantOffset (bits 1-3)
                                      12,  // TotalBits
                                      true // HasImplicitBit
                                      >;

// Test that padded format works
static_assert(test_identity_exhaustive<PaddedFormat>(),
              "Padded format: pack(unpack(x)) must equal x for all values");

// Runtime tests with output
int main() {
  printf("=== OPINE Pack/Unpack Tests ===\n\n");

  // FP8 E5M2 tests
  printf("FP8 E5M2 exhaustive test: ");
  if (test_identity_exhaustive<FP8_E5M2>()) {
    printf("PASS (all 256 values)\n");
  } else {
    printf("FAIL\n");
    return 1;
  }

  // FP8 E4M3 tests
  printf("FP8 E4M3 exhaustive test: ");
  if (test_identity_exhaustive<FP8_E4M3>()) {
    printf("PASS (all 256 values)\n");
  } else {
    printf("FAIL\n");
    return 1;
  }

  // Bit extraction tests
  printf("FP8 E5M2 bit extraction: ");
  bool extraction_ok = true;
  extraction_ok &= test_bit_extraction<FP8_E5M2>(0xB3, true, 12, 3);
  extraction_ok &= test_bit_extraction<FP8_E5M2>(0x01, false, 0, 1);
  extraction_ok &= test_bit_extraction<FP8_E5M2>(0x7C, false, 31, 0);
  printf("%s\n", extraction_ok ? "PASS" : "FAIL");
  if (!extraction_ok)
    return 1;

  printf("FP8 E4M3 bit extraction: ");
  extraction_ok = true;
  extraction_ok &= test_bit_extraction<FP8_E4M3>(0xB5, true, 6, 5);
  extraction_ok &= test_bit_extraction<FP8_E4M3>(0x07, false, 0, 7);
  printf("%s\n", extraction_ok ? "PASS" : "FAIL");
  if (!extraction_ok)
    return 1;

  // Implicit bit tests
  printf("Implicit bit handling: ");
  bool implicit_ok = true;
  implicit_ok &= test_implicit_bit<FP8_E5M2>();
  implicit_ok &= test_implicit_bit<FP8_E4M3>();
  printf("%s\n", implicit_ok ? "PASS" : "FAIL");
  if (!implicit_ok)
    return 1;

  // Guard bits tests
  printf("Guard bits zero check: ");
  bool guard_ok = true;
  guard_ok &= test_guard_bits_zero<FP8_E5M2>();
  guard_ok &= test_guard_bits_zero<FP8_E4M3>();
  printf("%s\n", guard_ok ? "PASS" : "FAIL");
  if (!guard_ok)
    return 1;

  // Padded format test
  printf("Padded format test: ");
  if (test_identity_exhaustive<PaddedFormat>()) {
    printf("PASS (all 4096 values)\n");
  } else {
    printf("FAIL\n");
    return 1;
  }

  printf("\n=== All tests passed! ===\n");
  return 0;
}
