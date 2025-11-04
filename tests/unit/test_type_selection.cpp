#include <cstdint>
#include <opine/opine.hpp>
#include <type_traits>

using namespace opine;

// ============================================================================
// Compiler-Agnostic Tests (work with GCC and Clang)
// ============================================================================

// LeastWidth Policy Tests - Standard Types (<=64 bits)

// Unsigned types
static_assert(
    std::is_same_v<uint_t<1, type_policies::LeastWidth>, uint_least8_t>,
    "LeastWidth: 1 bit should give uint_least8_t");
static_assert(
    std::is_same_v<uint_t<5, type_policies::LeastWidth>, uint_least8_t>,
    "LeastWidth: 5 bits should give uint_least8_t");
static_assert(
    std::is_same_v<uint_t<8, type_policies::LeastWidth>, uint_least8_t>,
    "LeastWidth: 8 bits should give uint_least8_t");
static_assert(
    std::is_same_v<uint_t<9, type_policies::LeastWidth>, uint_least16_t>,
    "LeastWidth: 9 bits should give uint_least16_t");
static_assert(
    std::is_same_v<uint_t<16, type_policies::LeastWidth>, uint_least16_t>,
    "LeastWidth: 16 bits should give uint_least16_t");
static_assert(
    std::is_same_v<uint_t<17, type_policies::LeastWidth>, uint_least32_t>,
    "LeastWidth: 17 bits should give uint_least32_t");
static_assert(
    std::is_same_v<uint_t<24, type_policies::LeastWidth>, uint_least32_t>,
    "LeastWidth: 24 bits should give uint_least32_t");
static_assert(
    std::is_same_v<uint_t<32, type_policies::LeastWidth>, uint_least32_t>,
    "LeastWidth: 32 bits should give uint_least32_t");
static_assert(
    std::is_same_v<uint_t<33, type_policies::LeastWidth>, uint_least64_t>,
    "LeastWidth: 33 bits should give uint_least64_t");
static_assert(
    std::is_same_v<uint_t<64, type_policies::LeastWidth>, uint_least64_t>,
    "LeastWidth: 64 bits should give uint_least64_t");

// Signed types
static_assert(std::is_same_v<int_t<1, type_policies::LeastWidth>, int_least8_t>,
              "LeastWidth: 1 bit signed should give int_least8_t");
static_assert(std::is_same_v<int_t<7, type_policies::LeastWidth>, int_least8_t>,
              "LeastWidth: 7 bits signed should give int_least8_t");
static_assert(std::is_same_v<int_t<8, type_policies::LeastWidth>, int_least8_t>,
              "LeastWidth: 8 bits signed should give int_least8_t");
static_assert(
    std::is_same_v<int_t<9, type_policies::LeastWidth>, int_least16_t>,
    "LeastWidth: 9 bits signed should give int_least16_t");
static_assert(
    std::is_same_v<int_t<32, type_policies::LeastWidth>, int_least32_t>,
    "LeastWidth: 32 bits signed should give int_least32_t");

// Fastest Policy Tests - Standard Types (<=64 bits)

// Unsigned types
static_assert(std::is_same_v<uint_t<1, type_policies::Fastest>, uint_fast8_t>,
              "Fastest: 1 bit should give uint_fast8_t");
static_assert(std::is_same_v<uint_t<5, type_policies::Fastest>, uint_fast8_t>,
              "Fastest: 5 bits should give uint_fast8_t");
static_assert(std::is_same_v<uint_t<8, type_policies::Fastest>, uint_fast8_t>,
              "Fastest: 8 bits should give uint_fast8_t");
static_assert(std::is_same_v<uint_t<9, type_policies::Fastest>, uint_fast16_t>,
              "Fastest: 9 bits should give uint_fast16_t");
static_assert(std::is_same_v<uint_t<16, type_policies::Fastest>, uint_fast16_t>,
              "Fastest: 16 bits should give uint_fast16_t");
static_assert(std::is_same_v<uint_t<17, type_policies::Fastest>, uint_fast32_t>,
              "Fastest: 17 bits should give uint_fast32_t");
static_assert(std::is_same_v<uint_t<24, type_policies::Fastest>, uint_fast32_t>,
              "Fastest: 24 bits should give uint_fast32_t");
static_assert(std::is_same_v<uint_t<32, type_policies::Fastest>, uint_fast32_t>,
              "Fastest: 32 bits should give uint_fast32_t");
static_assert(std::is_same_v<uint_t<33, type_policies::Fastest>, uint_fast64_t>,
              "Fastest: 33 bits should give uint_fast64_t");
static_assert(std::is_same_v<uint_t<64, type_policies::Fastest>, uint_fast64_t>,
              "Fastest: 64 bits should give uint_fast64_t");

// Signed types
static_assert(std::is_same_v<int_t<1, type_policies::Fastest>, int_fast8_t>,
              "Fastest: 1 bit signed should give int_fast8_t");
static_assert(std::is_same_v<int_t<7, type_policies::Fastest>, int_fast8_t>,
              "Fastest: 7 bits signed should give int_fast8_t");
static_assert(std::is_same_v<int_t<8, type_policies::Fastest>, int_fast8_t>,
              "Fastest: 8 bits signed should give int_fast8_t");
static_assert(std::is_same_v<int_t<9, type_policies::Fastest>, int_fast16_t>,
              "Fastest: 9 bits signed should give int_fast16_t");
static_assert(std::is_same_v<int_t<32, type_policies::Fastest>, int_fast32_t>,
              "Fastest: 32 bits signed should give int_fast32_t");

// Realistic Format Type Aliases

// fp8_e5m2: 1 sign + 5 exponent + 2 mantissa = 8 bits
using fp8_e5m2_storage_t = uint_t<8>;
using fp8_e5m2_exp_t = uint_t<5>;
using fp8_e5m2_mant_t = uint_t<2>;

// fp8_e4m3: 1 sign + 4 exponent + 3 mantissa = 8 bits
using fp8_e4m3_storage_t = uint_t<8>;
using fp8_e4m3_exp_t = uint_t<4>;
using fp8_e4m3_mant_t = uint_t<3>;

// fp32_e8m23: 1 sign + 8 exponent + 23 mantissa = 32 bits
using fp32_e8m23_storage_t = uint_t<32>;
using fp32_e8m23_exp_t = uint_t<8>;
using fp32_e8m23_mant_t = uint_t<23>;

// With guard bits: 23-bit mantissa + 3 guard bits = 26 bits
using fp32_mant_with_guards = uint_t<26>;

// ExactWidth Fallback Tests (non-Clang compilers)

#if !defined(__clang__)

// Unsigned types
static_assert(std::is_same_v<uint_t<1>, uint_fast8_t>,
              "ExactWidth fallback: 1 bit should give uint_fast8_t");
static_assert(std::is_same_v<uint_t<5>, uint_fast8_t>,
              "ExactWidth fallback: 5 bits should give uint_fast8_t");
static_assert(std::is_same_v<uint_t<8>, uint_fast8_t>,
              "ExactWidth fallback: 8 bits should give uint_fast8_t");
static_assert(std::is_same_v<uint_t<9>, uint_fast16_t>,
              "ExactWidth fallback: 9 bits should give uint_fast16_t");
static_assert(std::is_same_v<uint_t<16>, uint_fast16_t>,
              "ExactWidth fallback: 16 bits should give uint_fast16_t");
static_assert(std::is_same_v<uint_t<17>, uint_fast32_t>,
              "ExactWidth fallback: 17 bits should give uint_fast32_t");
static_assert(std::is_same_v<uint_t<24>, uint_fast32_t>,
              "ExactWidth fallback: 24 bits should give uint_fast32_t");
static_assert(std::is_same_v<uint_t<32>, uint_fast32_t>,
              "ExactWidth fallback: 32 bits should give uint_fast32_t");
static_assert(std::is_same_v<uint_t<33>, uint_fast64_t>,
              "ExactWidth fallback: 33 bits should give uint_fast64_t");
static_assert(std::is_same_v<uint_t<64>, uint_fast64_t>,
              "ExactWidth fallback: 64 bits should give uint_fast64_t");

// Signed types
static_assert(std::is_same_v<int_t<1>, int_fast8_t>,
              "ExactWidth fallback: 1 bit signed should give int_fast8_t");
static_assert(std::is_same_v<int_t<2>, int_fast8_t>,
              "ExactWidth fallback: 2 bits signed should give int_fast8_t");
static_assert(std::is_same_v<int_t<7>, int_fast8_t>,
              "ExactWidth fallback: 7 bits signed should give int_fast8_t");
static_assert(std::is_same_v<int_t<9>, int_fast16_t>,
              "ExactWidth fallback: 9 bits signed should give int_fast16_t");
static_assert(std::is_same_v<int_t<32>, int_fast32_t>,
              "ExactWidth fallback: 32 bits signed should give int_fast32_t");
static_assert(std::is_same_v<int_t<64>, int_fast64_t>,
              "ExactWidth fallback: 64 bits signed should give int_fast64_t");

// Explicit policy specification
static_assert(
    std::is_same_v<uint_t<5, type_policies::ExactWidth>, uint_fast8_t>,
    "Explicit ExactWidth fallback: 5 bits should give uint_fast8_t");
static_assert(
    std::is_same_v<int_t<7, type_policies::ExactWidth>, int_fast8_t>,
    "Explicit ExactWidth fallback: 7 bits signed should give int_fast8_t");

#endif // !defined(__clang__)

// ============================================================================
// Exact Width Tests (Clang only - requires _BitInt support)
// ============================================================================

#if defined(__clang__)

// ExactWidth Policy Tests (Default Policy)

// Unsigned types
static_assert(std::is_same_v<uint_t<1>, unsigned _BitInt(1)>,
              "uint_t<1> should be unsigned _BitInt(1)");
static_assert(std::is_same_v<uint_t<5>, unsigned _BitInt(5)>,
              "uint_t<5> should be unsigned _BitInt(5)");
static_assert(std::is_same_v<uint_t<8>, unsigned _BitInt(8)>,
              "uint_t<8> should be unsigned _BitInt(8)");
static_assert(std::is_same_v<uint_t<24>, unsigned _BitInt(24)>,
              "uint_t<24> should be unsigned _BitInt(24)");
static_assert(std::is_same_v<uint_t<32>, unsigned _BitInt(32)>,
              "uint_t<32> should be unsigned _BitInt(32)");
static_assert(std::is_same_v<uint_t<64>, unsigned _BitInt(64)>,
              "uint_t<64> should be unsigned _BitInt(64)");
static_assert(std::is_same_v<uint_t<128>, unsigned _BitInt(128)>,
              "uint_t<128> should be unsigned _BitInt(128)");

// Signed types (note: signed _BitInt requires at least 2 bits)
static_assert(std::is_same_v<int_t<2>, _BitInt(2)>,
              "int_t<2> should be _BitInt(2)");
static_assert(std::is_same_v<int_t<5>, _BitInt(5)>,
              "int_t<5> should be _BitInt(5)");
static_assert(std::is_same_v<int_t<7>, signed _BitInt(7)>,
              "int_t<7> should be signed _BitInt(7)");
static_assert(std::is_same_v<int_t<16>, signed _BitInt(16)>,
              "int_t<16> should be signed _BitInt(16)");
static_assert(std::is_same_v<int_t<32>, signed _BitInt(32)>,
              "int_t<32> should be signed _BitInt(32)");

// Explicit policy specification
static_assert(
    std::is_same_v<uint_t<5, type_policies::ExactWidth>, unsigned _BitInt(5)>,
    "Explicit ExactWidth should give _BitInt(5)");
static_assert(
    std::is_same_v<int_t<7, type_policies::ExactWidth>, signed _BitInt(7)>,
    "Explicit ExactWidth should give _BitInt(7)");

// LeastWidth Policy Tests - Extended Types (>64 bits)

// Unsigned types
static_assert(
    std::is_same_v<uint_t<65, type_policies::LeastWidth>, unsigned _BitInt(65)>,
    "LeastWidth: 65 bits should give _BitInt(65)");
static_assert(std::is_same_v<uint_t<128, type_policies::LeastWidth>,
                             unsigned _BitInt(128)>,
              "LeastWidth: 128 bits should give _BitInt(128)");

// Signed types
static_assert(
    std::is_same_v<int_t<65, type_policies::LeastWidth>, signed _BitInt(65)>,
    "LeastWidth: 65 bits signed should give _BitInt(65)");

// Fastest Policy Tests - Extended Types (>64 bits)

// Unsigned types
static_assert(
    std::is_same_v<uint_t<65, type_policies::Fastest>, unsigned _BitInt(65)>,
    "Fastest: 65 bits should give _BitInt(65)");
static_assert(
    std::is_same_v<uint_t<128, type_policies::Fastest>, unsigned _BitInt(128)>,
    "Fastest: 128 bits should give _BitInt(128)");

// Signed types
static_assert(
    std::is_same_v<int_t<65, type_policies::Fastest>, signed _BitInt(65)>,
    "Fastest: 65 bits signed should give _BitInt(65)");

// Realistic Format Type Verification

static_assert(std::is_same_v<fp8_e5m2_storage_t, unsigned _BitInt(8)>);
static_assert(std::is_same_v<fp8_e5m2_exp_t, unsigned _BitInt(5)>);
static_assert(std::is_same_v<fp8_e5m2_mant_t, unsigned _BitInt(2)>);

static_assert(std::is_same_v<fp8_e4m3_storage_t, unsigned _BitInt(8)>);
static_assert(std::is_same_v<fp8_e4m3_exp_t, unsigned _BitInt(4)>);
static_assert(std::is_same_v<fp8_e4m3_mant_t, unsigned _BitInt(3)>);

static_assert(std::is_same_v<fp32_e8m23_storage_t, unsigned _BitInt(32)>);
static_assert(std::is_same_v<fp32_e8m23_exp_t, unsigned _BitInt(8)>);
static_assert(std::is_same_v<fp32_e8m23_mant_t, unsigned _BitInt(23)>);

static_assert(std::is_same_v<fp32_mant_with_guards, unsigned _BitInt(26)>);

#endif // defined(__clang__)

// ============================================================================
// Runtime Instantiation Tests
// ============================================================================

int main() {
  // ExactWidth (default)
  uint_t<5> a = 0;
  int_t<7> b = 0;

  // LeastWidth
  uint_t<5, type_policies::LeastWidth> c = 0;
  int_t<7, type_policies::LeastWidth> d = 0;

  // Fastest
  uint_t<5, type_policies::Fastest> e = 0;
  int_t<7, type_policies::Fastest> f = 0;

  // Realistic format types
  fp8_e5m2_storage_t g = 0;
  fp32_mant_with_guards h = 0;

  // Suppress unused variable warnings
  (void)a;
  (void)b;
  (void)c;
  (void)d;
  (void)e;
  (void)f;
  (void)g;
  (void)h;

  return 0;
}
