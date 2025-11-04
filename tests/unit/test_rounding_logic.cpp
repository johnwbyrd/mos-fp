#include <cstdio>
#include <opine/opine.hpp>

using namespace opine;

// Test helper: Create a mantissa with specific GRS bits and stored bits
// This allows us to test the rounding logic directly
template <typename Format, typename RoundingPolicy>
constexpr typename UnpackedFloat<Format, RoundingPolicy>::mantissa_type
make_mantissa(uint32_t stored_bits, uint32_t grs_bits) {
  using mantissa_type =
      typename UnpackedFloat<Format, RoundingPolicy>::mantissa_type;

  // Layout: [implicit bit (if any)][stored bits][GRS bits]
  mantissa_type result = static_cast<mantissa_type>(stored_bits);
  result <<= RoundingPolicy::guard_bits;
  result |= static_cast<mantissa_type>(grs_bits);

  // Add implicit bit if format has one
  if constexpr (Format::has_implicit_bit) {
    constexpr int implicit_bit_pos =
        Format::mant_bits + RoundingPolicy::guard_bits;
    result |= (mantissa_type{1} << implicit_bit_pos);
  }

  return result;
}

// Test ToNearestTiesToEven rounding decisions
template <typename Format> constexpr bool test_round_to_nearest_even() {
  using RoundingPolicy = rounding_policies::ToNearestTiesToEven;
  using result_type = typename Format::mantissa_storage_type;

  // Use values that fit in the format's mantissa width
  // FP8 E5M2 has 2-bit mantissa (values 0-3)
  // FP8 E4M3 has 3-bit mantissa (values 0-7)
  constexpr result_type val_even = 2; // LSB=0 (even)
  constexpr result_type val_odd = 3;  // LSB=1 (odd)
  constexpr result_type val_small = 1;

  // Test case 1: GRS = 0b000 (less than half) - round down
  {
    auto wide_mant = make_mantissa<Format, RoundingPolicy>(val_even, 0b000);
    auto result = RoundingPolicy::round_mantissa<Format>(wide_mant, false);
    if (result != val_even) {
      return false;
    }
  }

  // Test case 2: GRS = 0b001 (less than half) - round down
  {
    auto wide_mant = make_mantissa<Format, RoundingPolicy>(val_even, 0b001);
    auto result = RoundingPolicy::round_mantissa<Format>(wide_mant, false);
    if (result != val_even) {
      return false;
    }
  }

  // Test case 3: GRS = 0b010 (less than half) - round down
  {
    auto wide_mant = make_mantissa<Format, RoundingPolicy>(val_even, 0b010);
    auto result = RoundingPolicy::round_mantissa<Format>(wide_mant, false);
    if (result != val_even) {
      return false;
    }
  }

  // Test case 4: GRS = 0b011 (less than half) - round down
  {
    auto wide_mant = make_mantissa<Format, RoundingPolicy>(val_even, 0b011);
    auto result = RoundingPolicy::round_mantissa<Format>(wide_mant, false);
    if (result != val_even) {
      return false;
    }
  }

  // Test case 5: GRS = 0b100 (exactly half, LSB=0 even) - round down
  {
    auto wide_mant = make_mantissa<Format, RoundingPolicy>(val_even, 0b100);
    auto result = RoundingPolicy::round_mantissa<Format>(wide_mant, false);
    if (result != val_even) { // LSB already even, don't round
      return false;
    }
  }

  // Test case 6: GRS = 0b100 (exactly half, LSB=1 odd) - round up to even
  {
    auto wide_mant = make_mantissa<Format, RoundingPolicy>(val_odd, 0b100);
    auto result = RoundingPolicy::round_mantissa<Format>(wide_mant, false);
    // Check if format can hold val_odd + 1 (use uint32_t for calculation)
    constexpr uint32_t max_val_calc = (uint32_t{1} << Format::mant_bits) - 1;
    constexpr result_type max_val = static_cast<result_type>(max_val_calc);
    if (val_odd < max_val) {
      constexpr result_type expected = val_odd + 1;
      if (result != expected) { // Round up to make even
        return false;
      }
    } else {
      // Skip overflow case
    }
  }

  // Test case 7: GRS = 0b101 (more than half) - round up
  {
    auto wide_mant = make_mantissa<Format, RoundingPolicy>(val_small, 0b101);
    auto result = RoundingPolicy::round_mantissa<Format>(wide_mant, false);
    constexpr result_type expected = val_small + 1;
    if (result != expected) {
      return false;
    }
  }

  // Test case 8: GRS = 0b110 (more than half) - round up
  {
    auto wide_mant = make_mantissa<Format, RoundingPolicy>(val_small, 0b110);
    auto result = RoundingPolicy::round_mantissa<Format>(wide_mant, false);
    constexpr result_type expected = val_small + 1;
    if (result != expected) {
      return false;
    }
  }

  // Test case 9: GRS = 0b111 (more than half) - round up
  {
    auto wide_mant = make_mantissa<Format, RoundingPolicy>(val_small, 0b111);
    auto result = RoundingPolicy::round_mantissa<Format>(wide_mant, false);
    constexpr result_type expected = val_small + 1;
    if (result != expected) {
      return false;
    }
  }

  return true;
}

// Test TowardZero rounding decisions (should always truncate)
template <typename Format> constexpr bool test_round_toward_zero() {
  using RoundingPolicy = rounding_policies::TowardZero;
  using result_type = typename Format::mantissa_storage_type;
  using mantissa_type = typename UnpackedFloat<Format, RoundingPolicy>::mantissa_type;

  // TowardZero has no guard bits, so we just test basic truncation

  // Test case 1: Basic truncation with implicit bit (use small value)
  {
    constexpr result_type test_val = 2; // Fits in any format
    auto wide_mant = static_cast<mantissa_type>(test_val);
    if constexpr (Format::has_implicit_bit) {
      // Add implicit bit
      constexpr int implicit_bit_pos = Format::mant_bits;
      wide_mant |= (mantissa_type{1} << implicit_bit_pos);
    }

    auto result = RoundingPolicy::round_mantissa<Format>(wide_mant, false);
    if (result != test_val) {
      return false;
    }
  }

  // Test case 2: All bits set
  {
    // Calculate max mantissa correctly - use larger type for calculation
    constexpr uint32_t max_mant_calc = (uint32_t{1} << Format::mant_bits) - 1;
    constexpr result_type max_mant = static_cast<result_type>(max_mant_calc);

    auto wide_mant = static_cast<mantissa_type>(max_mant);
    if constexpr (Format::has_implicit_bit) {
      constexpr int implicit_bit_pos = Format::mant_bits;
      wide_mant |= (mantissa_type{1} << implicit_bit_pos);
    }

    auto result = RoundingPolicy::round_mantissa<Format>(wide_mant, false);
    if (result != max_mant) {
      return false;
    }
  }

  return true;
}

// Test that ties always round to even (additional specific tests)
template <typename Format> constexpr bool test_ties_to_even_pattern() {
  using RoundingPolicy = rounding_policies::ToNearestTiesToEven;
  using result_type = typename Format::mantissa_storage_type;

  // Pattern: xxxxx0 + 0.100 (tie) = xxxxx0 (already even, round down)
  // Pattern: xxxxx1 + 0.100 (tie) = xxxxx+1 (odd, round up to even)

  constexpr int max_val = (1 << Format::mant_bits) - 1;

  for (int stored = 0; stored <= max_val; ++stored) {
    // Create mantissa with GRS = 0b100 (exactly halfway)
    auto wide_mant =
        make_mantissa<Format, RoundingPolicy>(stored, 0b100);
    auto result = RoundingPolicy::round_mantissa<Format>(wide_mant, false);

    // Expected: if LSB is 0 (even), keep it; if LSB is 1 (odd), round up
    result_type expected;
    if (stored & 1) {
      // Odd: round up
      expected = static_cast<result_type>(stored + 1);
      // Handle potential overflow (tested separately)
      if (expected > max_val) {
        continue; // Skip overflow cases for this test
      }
    } else {
      // Even: round down (keep as is)
      expected = static_cast<result_type>(stored);
    }

    if (result != expected) {
      return false;
    }
  }

  return true;
}

// Compile-time tests
static_assert(test_round_to_nearest_even<fp8_e5m2>(),
              "fp8_e5m2: ToNearestTiesToEven rounding logic test");
static_assert(test_round_to_nearest_even<fp8_e4m3>(),
              "fp8_e4m3: ToNearestTiesToEven rounding logic test");

static_assert(test_round_toward_zero<fp8_e5m2>(),
              "fp8_e5m2: TowardZero rounding logic test");
static_assert(test_round_toward_zero<fp8_e4m3>(),
              "fp8_e4m3: TowardZero rounding logic test");

static_assert(test_ties_to_even_pattern<fp8_e5m2>(),
              "fp8_e5m2: Ties to even pattern test");
static_assert(test_ties_to_even_pattern<fp8_e4m3>(),
              "fp8_e4m3: Ties to even pattern test");

// Runtime tests with output
int main() {
  printf("=== OPINE Rounding Logic Tests ===\n\n");

  printf("fp8_e5m2 ToNearestTiesToEven: ");
  printf("%s\n",
         test_round_to_nearest_even<fp8_e5m2>() ? "PASS" : "FAIL");

  printf("fp8_e4m3 ToNearestTiesToEven: ");
  printf("%s\n",
         test_round_to_nearest_even<fp8_e4m3>() ? "PASS" : "FAIL");

  printf("fp8_e5m2 TowardZero: ");
  printf("%s\n", test_round_toward_zero<fp8_e5m2>() ? "PASS" : "FAIL");

  printf("fp8_e4m3 TowardZero: ");
  printf("%s\n", test_round_toward_zero<fp8_e4m3>() ? "PASS" : "FAIL");

  printf("fp8_e5m2 Ties to even pattern: ");
  printf("%s\n", test_ties_to_even_pattern<fp8_e5m2>() ? "PASS" : "FAIL");

  printf("fp8_e4m3 Ties to even pattern: ");
  printf("%s\n", test_ties_to_even_pattern<fp8_e4m3>() ? "PASS" : "FAIL");

  printf("\n=== All rounding logic tests passed! ===\n");
  return 0;
}
