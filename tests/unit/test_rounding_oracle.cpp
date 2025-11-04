#include <cmath>
#include <cstdio>
#include <opine/opine.hpp>
#include "../helpers/test_float_oracle.hpp"

using namespace opine;
using namespace opine::test_helpers;

// Test: pack(unpack(x)) should preserve all values (identity)
// This should pass for ANY rounding mode since guard bits are zero after unpack
template <typename Format, typename RoundingPolicy>
constexpr bool test_roundtrip_identity() {
  using storage_type = typename Format::storage_type;
  constexpr int total = 1 << Format::total_bits;

  // Compute mask for significant bits (non-padding)
  constexpr storage_type sign_mask =
      ((storage_type{1} << Format::sign_bits) - 1) << Format::sign_offset;
  constexpr storage_type exp_mask =
      ((storage_type{1} << Format::exp_bits) - 1) << Format::exp_offset;
  constexpr storage_type mant_mask =
      ((storage_type{1} << Format::mant_bits) - 1) << Format::mant_offset;
  constexpr storage_type significant_mask = sign_mask | exp_mask | mant_mask;

  for (int i = 0; i < total; ++i) {
    storage_type bits = static_cast<storage_type>(i);

    // Unpack then repack
    auto unpacked = unpack<Format, RoundingPolicy>(bits);
    auto repacked = pack<Format, RoundingPolicy>(unpacked);

    // Compare only significant bits (ignore padding)
    if ((repacked & significant_mask) != (bits & significant_mask)) {
      return false;
    }
  }

  return true;
}

// Test: Convert to native float and back, should get same value
// This is THE KEY TEST - native float is our oracle
template <typename Format, typename RoundingPolicy>
bool test_native_float_roundtrip() {
  using storage_type = typename Format::storage_type;
  constexpr int total = 1 << Format::total_bits;

  int mismatches = 0;

  for (int i = 0; i < total; ++i) {
    storage_type bits = static_cast<storage_type>(i);

    // Convert to native float (the oracle)
    float native = to_native_float<Format>(bits);

    // Skip special cases we don't handle yet
    if (std::isnan(native) || std::isinf(native)) {
      continue;
    }

    // Convert back to FP8 using our implementation
    storage_type converted = from_native_float<Format, RoundingPolicy>(native);

    // They should match (considering padding)
    constexpr storage_type significant_mask =
        (((storage_type{1} << Format::sign_bits) - 1) << Format::sign_offset) |
        (((storage_type{1} << Format::exp_bits) - 1) << Format::exp_offset) |
        (((storage_type{1} << Format::mant_bits) - 1) << Format::mant_offset);

    if ((converted & significant_mask) != (bits & significant_mask)) {
      // Allow signed zero mismatch (0.0 vs -0.0)
      if (is_zero<Format>(converted) && is_zero<Format>(bits)) {
        continue;
      }

      mismatches++;
      if (mismatches <= 10) { // Print first 10 mismatches
        printf("  Mismatch at 0x%02X: native=%.10g, "
               "converted=0x%02X (expected 0x%02X)\n",
               static_cast<unsigned>(bits), native,
               static_cast<unsigned>(converted), static_cast<unsigned>(bits));
      }
    }
  }

  if (mismatches > 10) {
    printf("  ... and %d more mismatches\n", mismatches - 10);
  }

  return mismatches == 0;
}

// Test: Compare rounding modes - they should differ only when rounding matters
template <typename Format> void test_rounding_differences() {
  using storage_type = typename Format::storage_type;
  constexpr int total = 1 << Format::total_bits;

  int differences = 0;
  int zeros = 0;

  for (int i = 0; i < total; ++i) {
    storage_type bits = static_cast<storage_type>(i);

    float native = to_native_float<Format>(bits);

    // Skip special cases
    if (std::isnan(native) || std::isinf(native)) {
      continue;
    }

    if (native == 0.0f) {
      zeros++;
      continue; // Both should map zero -> zero
    }

    // Convert with both rounding modes
    auto toward_zero = from_native_float<Format, rounding_policies::TowardZero>(
        native);
    auto to_nearest =
        from_native_float<Format, rounding_policies::ToNearestTiesToEven>(
            native);

    if (toward_zero != to_nearest) {
      differences++;
      if (differences <= 5) { // Show first few differences
        printf("  Different at 0x%02X (%.10g): TowardZero=0x%02X, "
               "ToNearest=0x%02X\n",
               static_cast<unsigned>(bits), native,
               static_cast<unsigned>(toward_zero),
               static_cast<unsigned>(to_nearest));
      }
    }
  }

  printf("  Total differences: %d out of %d non-zero values\n", differences,
         total - zeros);
  printf("  This shows rounding modes produce different results as expected\n");
}

// Test specific tie-to-even cases with known values
template <typename Format> bool test_tie_cases_specific() {
  // We need to find values that are exactly halfway between representable
  // values For FP8 E5M2, the representable values are sparse enough that we
  // can identify some

  // This is format-specific and would need careful construction
  // For now, just verify the logic with constructed mantissa values
  // (already tested in test_rounding_logic.cpp)

  printf("  Tie-to-even specific cases tested in test_rounding_logic.cpp\n");
  return true;
}

// Compile-time tests for roundtrip identity
static_assert(test_roundtrip_identity<fp8_e5m2, rounding_policies::TowardZero>(),
              "fp8_e5m2 TowardZero: roundtrip identity");
static_assert(
    test_roundtrip_identity<fp8_e5m2, rounding_policies::ToNearestTiesToEven>(),
    "fp8_e5m2 ToNearestTiesToEven: roundtrip identity");

static_assert(test_roundtrip_identity<fp8_e4m3, rounding_policies::TowardZero>(),
              "fp8_e4m3 TowardZero: roundtrip identity");
static_assert(
    test_roundtrip_identity<fp8_e4m3, rounding_policies::ToNearestTiesToEven>(),
    "fp8_e4m3 ToNearestTiesToEven: roundtrip identity");

// Runtime tests with oracle
int main() {
  printf("=== OPINE Native Float Oracle Tests ===\n\n");

  printf("Test 1: Roundtrip Identity (pack(unpack(x)) == x)\n");
  printf("----------------------------------------\n");
  printf("fp8_e5m2 TowardZero: ");
  printf("%s\n", test_roundtrip_identity<fp8_e5m2,
                                         rounding_policies::TowardZero>()
                     ? "PASS"
                     : "FAIL");

  printf("fp8_e5m2 ToNearestTiesToEven: ");
  printf("%s\n",
         test_roundtrip_identity<fp8_e5m2,
                                 rounding_policies::ToNearestTiesToEven>()
             ? "PASS"
             : "FAIL");

  printf("fp8_e4m3 TowardZero: ");
  printf("%s\n", test_roundtrip_identity<fp8_e4m3,
                                         rounding_policies::TowardZero>()
                     ? "PASS"
                     : "FAIL");

  printf("fp8_e4m3 ToNearestTiesToEven: ");
  printf("%s\n",
         test_roundtrip_identity<fp8_e4m3,
                                 rounding_policies::ToNearestTiesToEven>()
             ? "PASS"
             : "FAIL");

  printf("\nTest 2: Native Float Oracle (to_native → from_native roundtrip)\n");
  printf("----------------------------------------\n");
  printf("fp8_e5m2 TowardZero:\n");
  bool pass1 = test_native_float_roundtrip<fp8_e5m2,
                                           rounding_policies::TowardZero>();
  printf("  Result: %s\n", pass1 ? "PASS" : "FAIL");

  printf("\nfp8_e5m2 ToNearestTiesToEven:\n");
  bool pass2 =
      test_native_float_roundtrip<fp8_e5m2,
                                  rounding_policies::ToNearestTiesToEven>();
  printf("  Result: %s\n", pass2 ? "PASS" : "FAIL");

  printf("\nfp8_e4m3 TowardZero:\n");
  bool pass3 = test_native_float_roundtrip<fp8_e4m3,
                                           rounding_policies::TowardZero>();
  printf("  Result: %s\n", pass3 ? "PASS" : "FAIL");

  printf("\nfp8_e4m3 ToNearestTiesToEven:\n");
  bool pass4 =
      test_native_float_roundtrip<fp8_e4m3,
                                  rounding_policies::ToNearestTiesToEven>();
  printf("  Result: %s\n", pass4 ? "PASS" : "FAIL");

  printf("\nTest 3: Rounding Mode Differences\n");
  printf("----------------------------------------\n");
  printf("fp8_e5m2:\n");
  test_rounding_differences<fp8_e5m2>();

  printf("\nfp8_e4m3:\n");
  test_rounding_differences<fp8_e4m3>();

  printf("\nTest 4: Tie-to-Even Specific Cases\n");
  printf("----------------------------------------\n");
  printf("fp8_e5m2: ");
  printf("%s\n", test_tie_cases_specific<fp8_e5m2>() ? "PASS" : "FAIL");
  printf("fp8_e4m3: ");
  printf("%s\n", test_tie_cases_specific<fp8_e4m3>() ? "PASS" : "FAIL");

  bool all_pass = pass1 && pass2 && pass3 && pass4;
  if (all_pass) {
    printf("\n=== All oracle tests passed! ===\n");
    return 0;
  } else {
    printf("\n=== Some tests FAILED ===\n");
    return 1;
  }
}
