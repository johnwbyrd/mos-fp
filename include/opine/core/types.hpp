#pragma once

#include <opine/policies/type_selection.hpp>

namespace opine::inline v1 {

// Default policy
using DefaultTypeSelectionPolicy = type_policies::ExactWidth;

// Public interface for unsigned integer type selection
template<int Bits, typename Policy = DefaultTypeSelectionPolicy>
    requires type_policies::TypeSelectionPolicy<Policy>
using uint_t = decltype(Policy::template select_unsigned<Bits>());

// Public interface for signed integer type selection
template<int Bits, typename Policy = DefaultTypeSelectionPolicy>
    requires type_policies::TypeSelectionPolicy<Policy>
using int_t = decltype(Policy::template select_signed<Bits>());

} // namespace opine::v1
