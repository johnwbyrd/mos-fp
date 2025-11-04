#include <opine/opine.hpp>
#include <type_traits>
#include <cstdint>

using namespace opine;

// ============================================================================
// ExactWidth Policy Tests (Default)
// ============================================================================

// Test basic unsigned types with ExactWidth (default policy)
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

// Test basic signed types with ExactWidth (default policy)
// Note: signed _BitInt requires at least 2 bits
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

// Test explicit ExactWidth policy
static_assert(std::is_same_v<uint_t<5, type_policies::ExactWidth>, unsigned _BitInt(5)>,
    "Explicit ExactWidth should give _BitInt(5)");
static_assert(std::is_same_v<int_t<7, type_policies::ExactWidth>, signed _BitInt(7)>,
    "Explicit ExactWidth should give _BitInt(7)");

// ============================================================================
// LeastWidth Policy Tests
// ============================================================================

// Test unsigned LeastWidth types
static_assert(std::is_same_v<uint_t<1, type_policies::LeastWidth>, uint_least8_t>,
    "LeastWidth: 1 bit should give uint_least8_t");
static_assert(std::is_same_v<uint_t<5, type_policies::LeastWidth>, uint_least8_t>,
    "LeastWidth: 5 bits should give uint_least8_t");
static_assert(std::is_same_v<uint_t<8, type_policies::LeastWidth>, uint_least8_t>,
    "LeastWidth: 8 bits should give uint_least8_t");
static_assert(std::is_same_v<uint_t<9, type_policies::LeastWidth>, uint_least16_t>,
    "LeastWidth: 9 bits should give uint_least16_t");
static_assert(std::is_same_v<uint_t<16, type_policies::LeastWidth>, uint_least16_t>,
    "LeastWidth: 16 bits should give uint_least16_t");
static_assert(std::is_same_v<uint_t<17, type_policies::LeastWidth>, uint_least32_t>,
    "LeastWidth: 17 bits should give uint_least32_t");
static_assert(std::is_same_v<uint_t<24, type_policies::LeastWidth>, uint_least32_t>,
    "LeastWidth: 24 bits should give uint_least32_t");
static_assert(std::is_same_v<uint_t<32, type_policies::LeastWidth>, uint_least32_t>,
    "LeastWidth: 32 bits should give uint_least32_t");
static_assert(std::is_same_v<uint_t<33, type_policies::LeastWidth>, uint_least64_t>,
    "LeastWidth: 33 bits should give uint_least64_t");
static_assert(std::is_same_v<uint_t<64, type_policies::LeastWidth>, uint_least64_t>,
    "LeastWidth: 64 bits should give uint_least64_t");
static_assert(std::is_same_v<uint_t<65, type_policies::LeastWidth>, unsigned _BitInt(65)>,
    "LeastWidth: 65 bits should give _BitInt(65)");
static_assert(std::is_same_v<uint_t<128, type_policies::LeastWidth>, unsigned _BitInt(128)>,
    "LeastWidth: 128 bits should give _BitInt(128)");

// Test signed LeastWidth types
static_assert(std::is_same_v<int_t<1, type_policies::LeastWidth>, int_least8_t>,
    "LeastWidth: 1 bit signed should give int_least8_t");
static_assert(std::is_same_v<int_t<7, type_policies::LeastWidth>, int_least8_t>,
    "LeastWidth: 7 bits signed should give int_least8_t");
static_assert(std::is_same_v<int_t<8, type_policies::LeastWidth>, int_least8_t>,
    "LeastWidth: 8 bits signed should give int_least8_t");
static_assert(std::is_same_v<int_t<9, type_policies::LeastWidth>, int_least16_t>,
    "LeastWidth: 9 bits signed should give int_least16_t");
static_assert(std::is_same_v<int_t<32, type_policies::LeastWidth>, int_least32_t>,
    "LeastWidth: 32 bits signed should give int_least32_t");
static_assert(std::is_same_v<int_t<65, type_policies::LeastWidth>, signed _BitInt(65)>,
    "LeastWidth: 65 bits signed should give _BitInt(65)");

// ============================================================================
// Fastest Policy Tests
// ============================================================================

// Test unsigned Fastest types
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
static_assert(std::is_same_v<uint_t<65, type_policies::Fastest>, unsigned _BitInt(65)>,
    "Fastest: 65 bits should give _BitInt(65)");
static_assert(std::is_same_v<uint_t<128, type_policies::Fastest>, unsigned _BitInt(128)>,
    "Fastest: 128 bits should give _BitInt(128)");

// Test signed Fastest types
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
static_assert(std::is_same_v<int_t<65, type_policies::Fastest>, signed _BitInt(65)>,
    "Fastest: 65 bits signed should give _BitInt(65)");

// ============================================================================
// Realistic Format Tests
// ============================================================================

// FP8 E5M2: 1 sign + 5 exponent + 2 mantissa = 8 bits
using fp8_e5m2_storage = uint_t<8>;  // ExactWidth: _BitInt(8)
using fp8_e5m2_exp = uint_t<5>;      // ExactWidth: _BitInt(5)
using fp8_e5m2_mant = uint_t<2>;     // ExactWidth: _BitInt(2)

static_assert(std::is_same_v<fp8_e5m2_storage, unsigned _BitInt(8)>);
static_assert(std::is_same_v<fp8_e5m2_exp, unsigned _BitInt(5)>);
static_assert(std::is_same_v<fp8_e5m2_mant, unsigned _BitInt(2)>);

// FP8 E4M3: 1 sign + 4 exponent + 3 mantissa = 8 bits
using fp8_e4m3_storage = uint_t<8>;
using fp8_e4m3_exp = uint_t<4>;
using fp8_e4m3_mant = uint_t<3>;

static_assert(std::is_same_v<fp8_e4m3_storage, unsigned _BitInt(8)>);
static_assert(std::is_same_v<fp8_e4m3_exp, unsigned _BitInt(4)>);
static_assert(std::is_same_v<fp8_e4m3_mant, unsigned _BitInt(3)>);

// IEEE 754 Binary32: 1 sign + 8 exponent + 23 mantissa = 32 bits
using fp32_storage = uint_t<32>;
using fp32_exp = uint_t<8>;
using fp32_mant = uint_t<23>;

static_assert(std::is_same_v<fp32_storage, unsigned _BitInt(32)>);
static_assert(std::is_same_v<fp32_exp, unsigned _BitInt(8)>);
static_assert(std::is_same_v<fp32_mant, unsigned _BitInt(23)>);

// With guard bits: 23-bit mantissa + 3 guard bits = 26 bits
using fp32_mant_with_guards = uint_t<26>;
static_assert(std::is_same_v<fp32_mant_with_guards, unsigned _BitInt(26)>);

// ============================================================================
// Compile-time instantiation test
// ============================================================================

// This ensures the code actually compiles and can be instantiated
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
    fp8_e5m2_storage g = 0;
    fp32_mant_with_guards h = 0;

    // Suppress unused variable warnings
    (void)a; (void)b; (void)c; (void)d; (void)e; (void)f; (void)g; (void)h;

    return 0;
}
