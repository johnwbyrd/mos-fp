# Pack/Unpack System

## Overview

The pack/unpack system provides bidirectional conversion between storage format (compact bit representation) and computational format (unpacked with explicit implicit bits and guard bits).

## Core Components

### 1. Format Descriptors (`format.hpp`)

Describes the bit layout of a floating-point format:

```cpp
template<
    int SignBits,           // Number of sign bits (typically 1)
    int SignOffset,         // Bit position from LSB
    int ExpBits,            // Number of exponent bits
    int ExpOffset,          // Bit position from LSB
    int MantBits,           // Number of mantissa bits (stored, not including implicit)
    int MantOffset,         // Bit position from LSB
    int TotalBits,          // Total storage size (may include padding)
    bool HasImplicitBit,    // true for IEEE 754-style formats
    int ExponentBias = -1,  // -1 = auto-calculate as 2^(E-1) - 1
    typename TypePolicy = DefaultTypeSelectionPolicy
>
struct FormatDescriptor;
```

**Key features**:
- Supports arbitrary bit layouts with padding
- Offsets specified from LSB (bit 0 = LSB)
- Auto-calculated exponent bias for standard formats
- Integrated with type selection system

**Convenience alias for standard IEEE 754 layouts**:
```cpp
template<int ExpBits, int MantBits, typename TypePolicy = DefaultTypeSelectionPolicy>
using IEEE_Format = FormatDescriptor<...>;

// Predefined formats
using FP8_E5M2 = IEEE_Format<5, 2>;
using FP8_E4M3 = IEEE_Format<4, 3>;
using Binary32 = IEEE_Format<8, 23>;
```

### 2. Rounding Policies (`policies/rounding.hpp`)

Defines how mantissa values are rounded when packing from wide (with guard bits) to narrow (storage) format.

**Current implementation**: TowardZero (truncation)
```cpp
struct TowardZero {
    static constexpr int guard_bits = 0;

    template<typename Format, typename MantissaType>
    static constexpr auto round_mantissa(
        MantissaType wide_mantissa,
        bool is_negative
    );
};
```

**Future rounding modes** (planned):
- `ToNearestTiesToEven` - IEEE 754 default (guard_bits = 3)
- `ToNearestTiesAwayFromZero` (guard_bits = 3)
- `TowardPositive` - ceiling (guard_bits = 1)
- `TowardNegative` - floor (guard_bits = 1)

### 3. Unpacked Float Structure (`core/unpacked.hpp`)

Computational representation of a floating-point value:

```cpp
template<typename Format, typename RoundingPolicy = DefaultRoundingPolicy>
struct UnpackedFloat {
    bool sign;                               // Sign bit
    typename Format::exponent_type exponent; // Biased exponent
    mantissa_type mantissa;                  // Includes implicit bit + guard bits
};
```

**Mantissa layout**:
```
[implicit bit (MSB, if present)][M stored bits][G guard bits (LSB)]
```

**Size calculation**:
```
mantissa_bits = Format::mant_bits +
                (Format::has_implicit_bit ? 1 : 0) +
                RoundingPolicy::guard_bits
```

**Key properties**:
- Implicit bit is made explicit (1 for normal numbers, 0 for denormals)
- Guard bits are initially zero after unpack
- Guard bits are populated during arithmetic operations
- Unpacked form depends on both format AND rounding policy

### 4. Pack/Unpack Operations (`operations/pack_unpack.hpp`)

#### Unpack Operation

Converts from storage format to computational format:

```cpp
template<typename Format, typename RoundingPolicy = DefaultRoundingPolicy>
constexpr UnpackedFloat<Format, RoundingPolicy> unpack(
    typename Format::storage_type bits
);
```

**Algorithm**:
1. Extract sign bit from storage
2. Extract exponent from storage (biased encoding)
3. Extract stored mantissa bits from storage
4. If format has implicit bit:
   - If exponent != 0 (normal): set implicit bit to 1
   - If exponent == 0 (denormal): set implicit bit to 0
5. Shift mantissa left by `guard_bits` positions (guard bits become LSBs, initialized to zero)
6. Return UnpackedFloat

**Example** (FP8 E5M2, TowardZero rounding):
```
Storage:  [S:1][E:5][M:2] = 8 bits
Unpacked: sign=bool, exponent=uint_t<5>, mantissa=uint_t<3>
          (3 bits = 2 stored + 1 implicit, 0 guard bits for TowardZero)

Input:    0b10110011 = 0xB3
          S=1, E=01100=12, M=11=3
Output:   sign=true, exponent=12, mantissa=0b111 (implicit 1 + stored 11)
```

#### Pack Operation

Converts from computational format to storage format:

```cpp
template<typename Format, typename RoundingPolicy = DefaultRoundingPolicy>
constexpr typename Format::storage_type pack(
    const UnpackedFloat<Format, RoundingPolicy>& unpacked
);
```

**Algorithm**:
1. Create empty storage value (all zeros)
2. Insert sign bit at sign_offset
3. Insert exponent at exp_offset
4. Call `RoundingPolicy::round_mantissa()` to:
   - Remove guard bits
   - Remove implicit bit
   - Apply rounding
5. Insert rounded mantissa at mant_offset
6. Return storage value

**Properties**:
- Pack produces canonical form (padding bits are zero)
- For TowardZero rounding, no precision is added or changed
- For padded formats, padding bits are always zero in output

## Design Decisions

### Denormal Handling

Currently hardcoded: `exponent == 0` indicates denormal (implicit bit = 0).

**Future**: This will become a policy choice to support formats with different denormal conventions.

### Padding Bits

**Philosophy**: Pack produces canonical form with zero padding bits.

**Rationale**:
- Padding bits have no semantic meaning
- Different bit patterns should not represent the same value
- Simplifies equality comparison
- Maintains invariant: `pack(unpack(x)) preserves significant bits`

### Mantissa Overflow from Rounding

**Current**: Not handled (deferred to future implementation).

**Why**: TowardZero rounding cannot cause mantissa overflow. When we add rounding modes that can cause overflow (e.g., ToNearestTiesToEven where 1.111... rounds to 10.000...), we'll need to:
1. Detect overflow
2. Increment exponent
3. Set mantissa to zero (or minimum value)
4. Handle exponent overflow

This will be implemented as part of adding proper rounding modes.

### Guard Bits in Unpacked Form

**Key insight**: The number of guard bits depends on the rounding policy.
- TowardZero: 0 guard bits (no rounding needed)
- ToNearestTies*: 3 guard bits (Guard, Round, Sticky)
- Toward±Infinity: 1 guard bit (just need to know if any bits lost)

**Implication**: `UnpackedFloat<Format, RoundingPolicy>` has different sizes for different rounding policies. This is intentional and correct - the computational representation must match the rounding requirements.

## Testing Strategy

### Exhaustive Testing for Small Formats

All FP8 formats (8 bits = 256 values) are tested exhaustively:
- FP8 E5M2: All 256 values
- FP8 E4M3: All 256 values
- Padded 12-bit format: All 4096 values

**Test**: `pack(unpack(x))` preserves significant bits (ignoring padding)

### Targeted Tests

- **Bit extraction**: Verify each field extracted correctly from known bit patterns
- **Implicit bit**: Verify normal (exp != 0) vs denormal (exp == 0) handling
- **Guard bits**: Verify guard bits are zero after unpack
- **Padding**: Verify pack produces canonical form

### Compile-Time Tests

All tests run at compile-time via `static_assert`, catching errors early and demonstrating constexpr correctness.

### Runtime Tests

Runtime tests provide detailed output for debugging and verification.

## Future Work

### Rounding Modes

Implement remaining IEEE 754 rounding modes:
1. ToNearestTiesToEven (guard_bits = 3)
   - Extract G, R, S bits
   - Round up if G=1 and (R=1 or S=1 or (R=0 and S=0 and LSB=1))
2. ToNearestTiesAwayFromZero (guard_bits = 3)
   - Round up if G=1 and (R=1 or S=1 or exactly halfway)
3. TowardPositive (guard_bits = 1)
   - Round up if positive and any guard bits set
4. TowardNegative (guard_bits = 1)
   - Round up if negative and any guard bits set

Each mode will be thoroughly tested with:
- Halfway cases
- More-than-halfway cases
- Less-than-halfway cases
- Sign combinations

### Mantissa Overflow Handling

When rounding causes mantissa overflow:
- Detect: mantissa >= 2^(M+1) after rounding
- Action: Increment exponent, reset mantissa
- Edge case: Exponent overflow (becomes infinity or max value, policy-dependent)

### Special Value Policies

- NaN detection and handling
- Infinity detection and handling
- Signed zero policy
- Denormal mode policies (FTZ, FIZ, FOZ, full support)

### Exponent Overflow/Underflow

- Overflow: saturate, wrap, infinity (policy choice)
- Underflow: denormals, flush to zero (policy choice)

## Usage Examples

### Basic Pack/Unpack

```cpp
#include <opine/opine.hpp>

using namespace opine;

// FP8 E5M2 format
using Format = FP8_E5M2;

// Pack a value
Format::storage_type bits = 0b10110011;  // S=1, E=12, M=3

// Unpack
auto unpacked = unpack<Format>(bits);
// unpacked.sign = true
// unpacked.exponent = 12
// unpacked.mantissa = 0b111 (implicit 1 + stored 11)

// Repack
auto repacked = pack<Format>(unpacked);
// repacked == bits (0xB3)
```

### Custom Format with Padding

```cpp
// Custom 12-bit format: [pad:3][S:1][E:4][M:3][pad:1]
using PaddedFormat = FormatDescriptor<
    1,   // SignBits
    8,   // SignOffset
    4,   // ExpBits
    4,   // ExpOffset
    3,   // MantBits
    1,   // MantOffset
    12,  // TotalBits
    true // HasImplicitBit
>;

PaddedFormat::storage_type bits = 0b101010101010;
auto unpacked = unpack<PaddedFormat>(bits);
auto repacked = pack<PaddedFormat>(unpacked);
// repacked has padding bits zeroed
// (repacked & significant_mask) == (bits & significant_mask)
```

### Future: With Different Rounding Modes

```cpp
// (Once implemented)
auto unpacked = unpack<FP8_E5M2, rounding_policies::ToNearestTiesToEven>(bits);
// unpacked.mantissa has 3 guard bits (G, R, S)

auto repacked = pack<FP8_E5M2, rounding_policies::ToNearestTiesToEven>(unpacked);
// Rounding applied based on G, R, S bits
```

## File Organization

```
include/opine/
├── core/
│   ├── format.hpp          - FormatDescriptor, IEEE_Format
│   └── unpacked.hpp        - UnpackedFloat structure
├── policies/
│   └── rounding.hpp        - Rounding policies
└── operations/
    └── pack_unpack.hpp     - pack() and unpack() functions

tests/unit/
└── test_pack_unpack.cpp    - Exhaustive and targeted tests

docs/design/
├── type_selection.md       - Type selection system
├── bits.md                 - Guard/Round/Sticky bits
└── pack_unpack.md          - This document
```

## Summary

The pack/unpack system provides:
- ✅ Flexible format descriptors with padding support
- ✅ Policy-based rounding (TowardZero implemented)
- ✅ Correct implicit bit handling (normal vs denormal)
- ✅ Guard bit infrastructure for future rounding modes
- ✅ Exhaustive testing for small formats
- ✅ Compile-time and runtime validation
- ✅ Clean separation of concerns (format, rounding, unpacking)

**Next steps**: Implement remaining rounding modes (ToNearestTiesToEven, etc.) with proper G/R/S bit handling.
