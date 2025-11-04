# Type Selection System

## Overview

The type selection system provides compile-time selection of integer types based on bit width requirements and platform policies.

## Design

### Three Type Selection Policies

1. **ExactWidth** - Uses `_BitInt(N)` for exact bit widths (default)
2. **LeastWidth** - Uses `uint_least_N` / `int_least_N` standard types
3. **Fastest** - Uses `uint_fast_N` / `int_fast_N` for performance

### Public Interface

```cpp
#include <opine/core/types.hpp>

using namespace opine;

// Default policy (ExactWidth)
uint_t<5> a;              // unsigned _BitInt(5)
int_t<7> b;               // _BitInt(7)

// Explicit policy selection
uint_t<5, type_policies::LeastWidth> c;    // uint_least8_t
uint_t<5, type_policies::Fastest> d;       // uint_fast8_t
```

### Policy Details

#### ExactWidth Policy
- Uses `_BitInt(N)` for all bit widths from 1 to 128
- Unsigned: `unsigned _BitInt(Bits)`
- Signed: `_BitInt(Bits)` (requires Bits >= 2)
- Best for: Exact control over bit usage, ML formats, embedded systems

#### LeastWidth Policy
- Uses smallest standard type that fits N bits
- 1-8 bits → `uint_least8_t` / `int_least8_t`
- 9-16 bits → `uint_least16_t` / `int_least16_t`
- 17-32 bits → `uint_least32_t` / `int_least32_t`
- 33-64 bits → `uint_least64_t` / `int_least64_t`
- 65-128 bits → `_BitInt(Bits)`
- Best for: Maximum portability

#### Fastest Policy
- Uses fastest standard type with at least N bits
- Same size categories as LeastWidth
- Uses `uint_fast_N` / `int_fast_N` types
- Best for: Performance-critical code on modern platforms

### Edge Cases

1. **1-bit signed**: Not supported by `_BitInt`, falls back to `int_least8_t` or `int_fast8_t` for LeastWidth/Fastest policies
2. **>64 bits**: All policies use `_BitInt` for widths beyond standard types
3. **Signed 2-bit minimum**: ExactWidth enforces that signed `_BitInt` requires at least 2 bits

### Implementation Details

- All selection happens at compile-time via `consteval` functions
- Type deduction uses `decltype` on zero-initialized values
- C++20 concepts ensure policy conformance
- Versioned namespace: `opine::inline v1`

## Directory Structure

```
include/opine/
├── policies/
│   └── type_selection.hpp    # Policy implementations
├── core/
│   └── types.hpp              # Public interface (uint_t, int_t)
└── opine.hpp                  # Convenience header
```

## Testing

Comprehensive static assertions test:
- All three policies
- Both signed and unsigned types
- Common bit widths (1, 5, 8, 16, 24, 32, 64, 128)
- Realistic float format examples (FP8 E4M3, E5M2, Binary32)

Run tests:
```bash
cmake -B build -S .
cmake --build build
cd build && ctest
```

## Future Integration

This type selection system will be used by:
- FormatPolicy for storage and computation types
- Integer arithmetic strategies for intermediate calculations
- Platform-specific optimizations
