#pragma once

#include <concepts>
#include <cstdint>

namespace opine::inline v1::type_policies {

// Concept: A type selection policy must provide select_unsigned and
// select_signed
template <typename T>
concept TypeSelectionPolicy = requires {
  { T::template select_unsigned<8>() };
  { T::template select_signed<8>() };
};

// Policy 1: Exact width using BitInt
struct ExactWidth {
  template <int Bits> static consteval auto select_unsigned() {
    static_assert(Bits > 0 && Bits <= 128,
                  "Bit width must be between 1 and 128");
    using type = unsigned _BitInt(Bits);
    return type{};
  }

  template <int Bits> static consteval auto select_signed() {
    static_assert(Bits >= 2 && Bits <= 128,
                  "Signed bit width must be between 2 and 128");
    using type = _BitInt(Bits);
    return type{};
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
      using type = unsigned _BitInt(Bits);
      return type{};
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
      using type = _BitInt(Bits);
      return type{};
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
      using type = unsigned _BitInt(Bits);
      return type{};
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
      using type = _BitInt(Bits);
      return type{};
    }
  }
};

} // namespace opine::inline v1::type_policies
