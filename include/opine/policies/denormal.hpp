#pragma once

#include <concepts>

namespace opine::inline v1::denormal_policies {

// Concept: A denormal policy must provide supports_denormals flag
template <typename T>
concept DenormalPolicy = requires {
  { T::supports_denormals } -> std::convertible_to<bool>;
};

// Full IEEE 754 denormal support (gradual underflow)
//
// Denormal numbers (exponent=0, mantissa≠0) are fully supported.
// This provides gradual underflow as specified by IEEE 754.
//
// Behavior:
// - Unpacking: Correctly interprets exp=0, mant≠0 as denormal
// - Packing: Creates denormal encoding for values below normal range
// - Arithmetic: Produces denormal results when appropriate
//
// Use case: IEEE 754 compliance, scientific computing, maximum accuracy
struct FullSupport {
  static constexpr bool supports_denormals = true;

  // Description for debugging/documentation
  static constexpr const char *name = "FullSupport";
};

// Flush To Zero (FTZ) - output flushing
//
// Any result that would be denormal is flushed to zero (sign preserved).
// Input denormals are processed normally.
//
// This matches the behavior of x86 FTZ (Flush To Zero) mode and is common
// in GPU and ML accelerator hardware for performance.
//
// Behavior:
// - Unpacking: Interprets denormals normally
// - Packing: Flushes denormal-range results to zero
// - Arithmetic: Flushes denormal results to zero
//
// Use case: GPU-style computation, ML inference, performance-critical code
struct FlushToZero {
  static constexpr bool supports_denormals = false;

  static constexpr const char *name = "FlushToZero";
};

// Flush Inputs to Zero (FIZ) - input flushing
//
// Denormal inputs are treated as zero before operations.
// May produce denormal outputs.
//
// This matches x86 DAZ (Denormals Are Zero) mode behavior.
//
// Behavior:
// - Unpacking: Converts denormals to zero
// - Packing: May create denormal encodings
// - Arithmetic: Treats denormal inputs as zero
//
// Use case: Audio DSP, systems where denormal inputs cause slowdown
struct FlushInputsToZero {
  static constexpr bool supports_denormals = false;

  static constexpr const char *name = "FlushInputsToZero";
};

// Flush On Zero (FOZ) - both inputs and outputs
//
// Denormal inputs are treated as zero.
// Denormal outputs are flushed to zero.
// Combines FTZ and FIZ for maximum performance.
//
// Behavior:
// - Unpacking: Converts denormals to zero
// - Packing: Flushes denormal-range results to zero
// - Arithmetic: No denormals anywhere
//
// Use case: Maximum performance mode, game math, approximate calculations
struct FlushOnZero {
  static constexpr bool supports_denormals = false;

  static constexpr const char *name = "FlushOnZero";
};

// No denormal representation in this format
//
// The format does not use exponent=0 to encode denormals.
// All exponent=0 values are treated as zero.
//
// Behavior:
// - Unpacking: exp=0 always means zero (regardless of mantissa)
// - Packing: Never creates exp=0 with non-zero mantissa
// - Arithmetic: No denormal numbers exist
//
// Use case: Custom formats, formats without implicit bit, simplified
// implementations
struct None {
  static constexpr bool supports_denormals = false;

  static constexpr const char *name = "None";
};

// Default denormal policy
//
// Currently matches the behavior of existing pack/unpack code, which correctly
// handles denormals during unpacking. This provides IEEE 754 gradual underflow.
using DefaultDenormalPolicy = FullSupport;

} // namespace opine::inline v1::denormal_policies
