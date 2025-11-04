#pragma once

#include <concepts>
#include <cstdint>

namespace opine::inline v1::type_policies {

// Feature detection for _BitInt support
namespace detail {
#if defined(__clang__)
constexpr bool has_bitint_support = true;
#else
constexpr bool has_bitint_support = false;
#endif
} // namespace detail

// Concept: A type selection policy must provide select_unsigned and
// select_signed
template <typename T>
concept TypeSelectionPolicy = requires {
  { T::template select_unsigned<8>() };
  { T::template select_signed<8>() };
};

// Policy 1: Exact width using BitInt (with fallback for compilers without
// _BitInt support)
struct ExactWidth {
  template <int Bits> static consteval auto select_unsigned() {
    static_assert(Bits > 0 && Bits <= 128,
                  "Bit width must be between 1 and 128");

    if constexpr (detail::has_bitint_support) {
#if defined(__clang__)
      using type = unsigned _BitInt(Bits);
      return type{};
#endif
    } else {
      // Emit warning via deprecated attribute
      [[maybe_unused]] [[deprecated(
          "ExactWidth policy: _BitInt not supported by this compiler, "
          "falling back to uint_fast types. Use Clang for optimal code "
          "generation.")]]
      int bitint_not_supported_warning;

      // Fallback to uint_fast types
      if constexpr (Bits <= 8)
        return uint_fast8_t{};
      else if constexpr (Bits <= 16)
        return uint_fast16_t{};
      else if constexpr (Bits <= 32)
        return uint_fast32_t{};
      else if constexpr (Bits <= 64)
        return uint_fast64_t{};
      else {
        static_assert(
            Bits <= 64,
            "Bit widths > 64 require _BitInt support. Use Clang compiler.");
      }
    }
  }

  template <int Bits> static consteval auto select_signed() {
    static_assert(Bits > 0 && Bits <= 128,
                  "Bit width must be between 1 and 128");

    if constexpr (detail::has_bitint_support) {
#if defined(__clang__)
      static_assert(Bits >= 2, "Signed _BitInt requires at least 2 bits");
      using type = _BitInt(Bits);
      return type{};
#endif
    } else {
      // Emit warning via deprecated attribute
      [[maybe_unused]] [[deprecated(
          "ExactWidth policy: _BitInt not supported by this compiler, "
          "falling back to int_fast types. Use Clang for optimal code "
          "generation.")]]
      int bitint_not_supported_warning;

      // Fallback to int_fast types (handle 1-bit edge case)
      if constexpr (Bits <= 1)
        return int_fast8_t{};
      else if constexpr (Bits <= 8)
        return int_fast8_t{};
      else if constexpr (Bits <= 16)
        return int_fast16_t{};
      else if constexpr (Bits <= 32)
        return int_fast32_t{};
      else if constexpr (Bits <= 64)
        return int_fast64_t{};
      else {
        static_assert(
            Bits <= 64,
            "Bit widths > 64 require _BitInt support. Use Clang compiler.");
      }
    }
  }
};

// Policy 2: Least width (at least N bits)
struct LeastWidth {
  template <int Bits> static consteval auto select_unsigned() {
    static_assert(Bits > 0 && Bits <= 128,
                  "Bit width must be between 1 and 128");

    if constexpr (Bits <= 8)
      return uint_least8_t{};
    else if constexpr (Bits <= 16)
      return uint_least16_t{};
    else if constexpr (Bits <= 32)
      return uint_least32_t{};
    else if constexpr (Bits <= 64)
      return uint_least64_t{};
    else {
      // For >64 bits, use _BitInt if available, otherwise error
      if constexpr (detail::has_bitint_support) {
#if defined(__clang__)
        using type = unsigned _BitInt(Bits);
        return type{};
#endif
      } else {
        static_assert(
            Bits <= 64,
            "Bit widths > 64 require _BitInt support. Use Clang compiler.");
      }
    }
  }

  template <int Bits> static consteval auto select_signed() {
    static_assert(Bits > 0 && Bits <= 128,
                  "Bit width must be between 1 and 128");

    // 1-bit signed doesn't exist, use smallest standard type
    if constexpr (Bits <= 1)
      return int_least8_t{};
    else if constexpr (Bits <= 8)
      return int_least8_t{};
    else if constexpr (Bits <= 16)
      return int_least16_t{};
    else if constexpr (Bits <= 32)
      return int_least32_t{};
    else if constexpr (Bits <= 64)
      return int_least64_t{};
    else {
      // For >64 bits, use _BitInt if available, otherwise error
      if constexpr (detail::has_bitint_support) {
#if defined(__clang__)
        using type = _BitInt(Bits);
        return type{};
#endif
      } else {
        static_assert(
            Bits <= 64,
            "Bit widths > 64 require _BitInt support. Use Clang compiler.");
      }
    }
  }
};

// Policy 3: Fastest (fastest type with at least N bits)
struct Fastest {
  template <int Bits> static consteval auto select_unsigned() {
    static_assert(Bits > 0 && Bits <= 128,
                  "Bit width must be between 1 and 128");

    if constexpr (Bits <= 8)
      return uint_fast8_t{};
    else if constexpr (Bits <= 16)
      return uint_fast16_t{};
    else if constexpr (Bits <= 32)
      return uint_fast32_t{};
    else if constexpr (Bits <= 64)
      return uint_fast64_t{};
    else {
      // For >64 bits, use _BitInt if available, otherwise error
      if constexpr (detail::has_bitint_support) {
#if defined(__clang__)
        using type = unsigned _BitInt(Bits);
        return type{};
#endif
      } else {
        static_assert(
            Bits <= 64,
            "Bit widths > 64 require _BitInt support. Use Clang compiler.");
      }
    }
  }

  template <int Bits> static consteval auto select_signed() {
    static_assert(Bits > 0 && Bits <= 128,
                  "Bit width must be between 1 and 128");

    // 1-bit signed doesn't exist, use smallest fast type
    if constexpr (Bits <= 1)
      return int_fast8_t{};
    else if constexpr (Bits <= 8)
      return int_fast8_t{};
    else if constexpr (Bits <= 16)
      return int_fast16_t{};
    else if constexpr (Bits <= 32)
      return int_fast32_t{};
    else if constexpr (Bits <= 64)
      return int_fast64_t{};
    else {
      // For >64 bits, use _BitInt if available, otherwise error
      if constexpr (detail::has_bitint_support) {
#if defined(__clang__)
        using type = _BitInt(Bits);
        return type{};
#endif
      } else {
        static_assert(
            Bits <= 64,
            "Bit widths > 64 require _BitInt support. Use Clang compiler.");
      }
    }
  }
};

} // namespace opine::inline v1::type_policies
