# Policy-Based Floating Point Library Design

## 1. Introduction

This document describes the design of a floating point library for C++ that uses policy-based design to support arbitrary float formats on resource-constrained and specialized platforms.

### What This Library Does

Provides floating point arithmetic operations (add, subtract, multiply, divide) for formats that may differ from IEEE 754 standard floats. Supports platforms ranging from 8-bit microprocessors with no hardware multiply to modern processors with SIMD units.

### Target Use Cases

- **Embedded systems**: 6502, 6809, Z80, AVR, ARM Cortex-M0
- **Retro computing**: Commodore 64, Apple II, TRS-80, Atari 8-bit
- **Machine learning**: FP8, bfloat16, custom precision formats
- **Legacy code integration**: Systems with existing float libraries or ROM routines

### Core Problem

No single floating point implementation meets all requirements. IEEE 754 binary32 requires ~8KB of code and assumes certain hardware capabilities. A Commodore 64 has 64KB total RAM. Google's TPU uses 8-bit floats. Apple II has floating point routines in ROM. Each case needs different tradeoffs.

### Motivating Example: FP8 on 6502

To illustrate the potential of policy-based design, consider FP8 E5M2 format (1 sign + 5 exponent + 2 mantissa bits) on a 6502:

**IEEE 754 binary32 multiply:** ~700 cycles
**FP8 E5M2 multiply with policies:** ~78 cycles

The speedup comes from:
- All operations in 8-bit registers (no multi-byte arithmetic)
- 3×3 bit mantissa multiply via 64-byte lookup table (15 cycles)
- 5-bit exponent addition (7 cycles)
- No special value checks, no guard bits, no normalization

**Result:** A 1 MHz 6502 can perform ~12,800 FP8 multiplies per frame at 60fps. This enables:
- 100-particle systems with full physics in ~5,000 cycles
- Simple 3D wireframe engines (50-100 vertices) at 60fps
- Game physics that would be impossible with float32

The precision (2 significant decimal digits, range ~0.0001 to ~57,000) is sufficient for particle positions, velocity calculations, and approximate physics on retro hardware.

This performance is only possible because the type system knows the exact bit widths and generates optimal code - not 8-bit or 32-bit generic routines, but exactly 3-bit × 3-bit multiplication.

## 2. The Problem

### Code Size Constraints

A complete IEEE 754 binary32 software implementation requires:
- Addition/subtraction: ~800 bytes
- Multiplication: ~600 bytes
- Division: ~1200 bytes
- Special value handling: ~400 bytes
- Total: ~3KB minimum

On a system with 64KB total memory where the application needs 60KB, this is unacceptable. Even removing features you don't need (denormals, all rounding modes, NaN) requires modifying the library source.

### Performance Constraints

Multiplication on 6502 (no hardware multiply):
- 8×8→16 bit: ~80 cycles
- 16×16→32 bit: ~250 cycles
- 24-bit mantissa multiply: ~400 cycles

Division is worse. Every operation counts. Using lookup tables trades ROM/flash space for speed. Knowing the platform capabilities at compile time enables better algorithm selection.

### Format Flexibility

IEEE 754 binary32 is not always correct:
- FP8 E4M3: 1 sign + 4 exponent + 3 mantissa (ML inference)
- FP8 E5M2: 1 sign + 5 exponent + 2 mantissa (ML training)
- bfloat16: 1 sign + 8 exponent + 7 mantissa (ML, same range as binary32)
- Microsoft binary format: Used in older BASIC interpreters
- Wozniak's format: Apple II ROM routines

### Integration with Existing Code

Some platforms have optimized implementations already:
- Apple II: Wozniak's floating point routines in ROM
- TI calculators: Built-in float operations
- Custom ASICs: Hardware float units
- Hand-optimized assembly: Platform-specific libraries

Wrapping these in a consistent interface without losing their benefits requires flexible integration points.

## 3. Design Goals

### Compile-Time Decisions

All format choices, special value handling, rounding modes, and error reporting strategies are resolved at compile time. The compiled code contains only the operations needed with the exact behavior specified. No runtime conditionals for "does this platform support denormals?"

### Pay Only For What You Use

If you don't enable NaN support, zero code is generated for NaN handling. If you select truncate rounding, no guard bits are computed. If you disable error reporting, no error checking code exists. Dead code elimination happens through template instantiation and link-time optimization, not through manual #ifdef blocks.

### Platform-Specific Optimization

Platform maintainers can provide optimized implementations without modifying the core library. A hand-written 6502 assembly multiply function integrates through the same interface as the C++ version. The library selects the right implementation based on configuration.

### Arbitrary Format Support

Any format with reasonable bit allocations works: sign (1 bit), exponent (2-11 bits typical), mantissa (2-52 bits typical). The library handles bit field extraction, type selection for intermediate values, and packing/unpacking automatically.

## 4. Core Concept: Policy-Based Design

### What Is a Policy?

A policy is a compile-time decision about one aspect of the floating point type's behavior. Examples:

- **Rounding policy**: How to round results (to nearest, toward zero, etc.)
- **Error policy**: How to report errors (exceptions, flags, silent)
- **Format policy**: How many bits for sign, exponent, mantissa

### Why Separate Policies?

Consider changing the rounding mode in a traditional library:
1. Find all rounding code
2. Modify conditional logic
3. Test all combinations
4. Hope you didn't break anything

With policies:
1. Change one template parameter
2. Compiler generates new code automatically
3. Old code still exists for other configurations

### Policy Composition

Policies combine to create a complete float type:

```cpp
// Each policy controls one aspect
FormatPolicy<1, 8, 23>              // 32-bit format
RoundingPolicy<ToNearest>           // IEEE 754 rounding
ErrorPolicy<None>                   // No error checking
SpecialValuePolicy<false, false>    // No NaN, no Inf

// Combined into trait bundle
struct FastFloat_Traits {
    using format = FormatPolicy<1, 8, 23>;
    using rounding = RoundingPolicy<ToNearest>;
    using errors = ErrorPolicy<None>;
    using specials = SpecialValuePolicy<false, false>;
};

// Creates complete float type
FloatEngine<FastFloat_Traits>
```

### Independence and Interaction

Most policies are independent:
- Changing rounding mode doesn't affect error reporting
- Changing format (16-bit vs 32-bit) doesn't affect special value handling
- Changing storage layout doesn't affect arithmetic behavior

Some policies interact:
- Rounding mode "to nearest" requires guard bits
- Denormal support requires normalization handling
- NaN support requires all-ones exponent interpretation

The library validates policy combinations at compile time.

## 5. Configurable Aspects (Overview)

### Number Format

How many bits for each field:
- Sign: typically 1 bit
- Exponent: 2-11 bits (determines range)
- Mantissa: 2-52 bits (determines precision)
- Implicit leading bit: yes/no
- Exponent bias: standard or custom

### Special Values

Which exceptional values exist:
- NaN (Not a Number)
- Infinity (positive and negative)
- Signed zeros (+0, -0)
- Denormals (subnormal numbers)

Each can be enabled or disabled independently.

### Arithmetic Behavior

How operations are performed:
- Rounding mode: to nearest, toward zero, toward ±infinity
- Guard bits: extra precision bits (0, 3, 8, 16, ...)
- Normalization: always, lazy, never
- Intermediate precision: can exceed storage precision

### Error Handling

How errors are detected and reported:
- Detection: which conditions trigger errors
- Reporting: none, errno, exceptions, status flags, callbacks
- Propagation: how errors flow through calculations

### Memory Layout

Physical bit arrangement:
- Field order: sign-exp-mant, mant-exp-sign, or others
- Packing: tight bitfields vs byte-aligned
- Endianness: little, big, or native
- Padding: allowed or forbidden

### Hardware Capabilities

Platform features that affect implementation:
- Integer multiply: hardware or software
- Integer divide: hardware or software
- Barrel shifter: single-cycle multi-bit shifts
- CLZ/CTZ: count leading/trailing zeros
- SIMD: vector operations

### Implementation Source

Where operation code comes from:
- Pure C++: template-generated code
- Inline assembly: embedded in C++
- External assembly: linked object files
- ROM routines: platform firmware calls
- Hardware: builtin operations

## 6. Example Configurations

### IEEE 754 Binary32

Standard 32-bit float with full IEEE 754 compliance:

```cpp
struct IEEE754_Binary32_Traits {
    using format = FormatPolicy<1, 8, 23>;
    using specials = SpecialValuePolicy<
        true,   // NaN
        true,   // Infinity
        true,   // Signed zeros
        true    // Denormals
    >;
    using rounding = RoundingPolicy<ToNearest>;
    using guard_bits = GuardBitsPolicy<3>;  // G, R, S bits
    using errors = ErrorPolicy<LocalFlags>;
    using normalization = NormalizationPolicy<Always>;
    using implementation = ImplementationPolicy<DefaultOps>;
};
```

**Use case**: Maximum compatibility with standard libraries and interchange formats.

**Tradeoffs**: Largest code size (~3KB), handles all edge cases, slowest on platforms without hardware support.

### Minimal 6502 Float

Smallest possible code size for Commodore 64:

```cpp
struct C64_Minimal_Traits {
    using format = FormatPolicy<1, 8, 23>;
    using specials = SpecialValuePolicy<
        false,  // No NaN
        false,  // No Infinity
        false,  // Single zero
        false   // No denormals
    >;
    using rounding = RoundingPolicy<TowardZero>;  // Truncate
    using guard_bits = GuardBitsPolicy<0>;
    using errors = ErrorPolicy<None>;
    using normalization = NormalizationPolicy<Lazy>;
    using implementation = ImplementationPolicy<DefaultOps>;
};
```

**Use case**: Games, demos, applications where code size is critical and perfect accuracy isn't required.

**Tradeoffs**: ~800 bytes total code, no special cases, truncation errors accumulate, overflow/underflow produce garbage.

### Accurate 6502 Float

Better accuracy while keeping reasonable code size:

```cpp
struct C64_Accurate_Traits {
    using format = FormatPolicy<1, 8, 23>;
    using specials = SpecialValuePolicy<
        false,  // No NaN
        true,   // Infinity (for overflow)
        false,  // Single zero
        false   // No denormals
    >;
    using rounding = RoundingPolicy<ToNearest>;
    using guard_bits = GuardBitsPolicy<8>;  // Full byte
    using errors = ErrorPolicy<None>;
    using normalization = NormalizationPolicy<Always>;
    using implementation = ImplementationPolicy<DefaultOps>;
};
```

**Use case**: Scientific calculations, financial software, applications needing reliable results.

**Tradeoffs**: ~1.5KB code size, proper rounding, handles overflow gracefully, about 20% slower than minimal version.

### Apple II with Wozniak ROM

Use the floating point routines built into Apple II ROM:

```cpp
struct AppleII_ROM_Traits {
    using format = FormatPolicy<1, 8, 23>;  // Wozniak's format
    using specials = SpecialValuePolicy<false, false, false, false>;
    using rounding = RoundingPolicy<ToNearest>;  // ROM does this
    using guard_bits = GuardBitsPolicy<8>;       // ROM uses guard byte
    using errors = ErrorPolicy<None>;
    using normalization = NormalizationPolicy<Always>;
    using implementation = ImplementationPolicy<AppleII_WozOps>;
};

// Implementation calls ROM
template<typename Traits>
struct AppleII_WozOps {
    static uint32_t add(uint32_t a, uint32_t b) {
        // Set up FAC and ARG, JSR $E7BE (FADD)
    }
    static uint32_t mul(uint32_t a, uint32_t b) {
        // JSR $E97F (FMUL)
    }
    // ... etc
};
```

**Use case**: Apple II software that wants consistent float behavior with BASIC.

**Tradeoffs**: Near-zero code size (just calling wrappers), fast (ROM is optimized), limited to Wozniak's format and behavior.

### FP8 E4M3 for Machine Learning

8-bit float for neural network inference:

```cpp
struct FP8_E4M3_Traits {
    using format = FormatPolicy<1, 4, 3>;
    using specials = SpecialValuePolicy<
        true,   // NaN (E4M3 convention)
        false,  // No Infinity (E4M3 convention)
        true,   // Signed zeros
        false   // No denormals (E4M3 convention)
    >;
    using rounding = RoundingPolicy<ToNearest>;
    using guard_bits = GuardBitsPolicy<3>;
    using errors = ErrorPolicy<None>;
    using normalization = NormalizationPolicy<Always>;
    using exponent_bias = ExponentBiasPolicy<7>;  // Non-standard
    using implementation = ImplementationPolicy<DefaultOps>;
};
```

**Use case**: ML model inference on edge devices, training with reduced precision.

**Tradeoffs**: Tiny memory footprint (1 byte per value), limited range and precision, fast operations due to small size.

## 7. Policy Dimensions (Detailed)

### 7.1 Number System Structure

Defines the mathematical structure of the number representation.

**Components:**

- **Bit allocation**: Number of bits for sign, exponent, mantissa
- **Exponent bias**: How zero exponent is encoded (typically 2^(E-1) - 1)
- **Implicit bit**: Whether mantissa has hidden leading 1
- **Sign representation**: Sign bit, two's complement, or sign-magnitude

**Why these cluster together**: These four aspects define the encoding. You cannot interpret the bits without knowing all four. They are inseparable.

**Available choices:**

Sign bits: Always 1 (no known formats use more)
Exponent bits: 2-11 typical (determines range: ~10^±1 to ~10^±308)
Mantissa bits: 2-52 typical (determines precision: ~3-16 decimal digits)

Implicit bit: Usually true for normalized formats (saves 1 bit of storage)
Exponent bias: Usually 2^(E-1)-1, but ML formats may differ

**Examples:**

```cpp
// IEEE 754 binary32: 1 + 8 + 23 bits
FormatPolicy<1, 8, 23>  
// Bias = 127, implicit bit = true

// FP8 E5M2: 1 + 5 + 2 bits  
FormatPolicy<1, 5, 2>   
// Bias = 15, implicit bit = true

// FP8 E4M3: 1 + 4 + 3 bits
FormatPolicy<1, 4, 3>
// Bias = 7 (non-standard!), implicit bit = true
```

**Interactions**: Format determines storage type selection. An 8-bit format uses `uint8_t`, 32-bit uses `uint32_t`. Intermediate calculations may need larger types (32-bit format multiplication may need 64-bit intermediate).

### 7.2 Special Value Vocabulary

Defines which exceptional values can be represented.

**Why separate from Number System Structure**: Special values are optional features. Same bit layout can support or not support NaN. The encoding reserves certain bit patterns (exponent=0, exponent=max) but the semantics are policy choices.

**Available choices:**

- **NaN (Not a Number)**: Result of 0/0, sqrt(-1), inf-inf
- **Infinity**: Result of overflow or 1/0
- **Signed zeros**: Distinguish +0 from -0
- **Denormals**: Numbers smaller than the smallest normalized value

Each can be enabled independently, though some combinations are unusual (NaN without Infinity is rare).

**Encoding implications:**

When enabled:
- NaN: exponent = all 1s, mantissa ≠ 0
- Infinity: exponent = all 1s, mantissa = 0
- Denormals: exponent = 0, mantissa ≠ 0
- Zero: exponent = 0, mantissa = 0

When disabled, these bit patterns may represent normal numbers or be undefined.

**Examples:**

```cpp
// Full IEEE 754 special values
SpecialValuePolicy<true, true, true, true>

// Minimal (no special values at all)
SpecialValuePolicy<false, false, false, false>

// Infinity only (overflow detection without NaN complexity)
SpecialValuePolicy<false, true, false, false>

// FP8 E4M3 convention (NaN but no Infinity)
SpecialValuePolicy<true, false, true, false>
```

**Interactions**: 
- NaN support adds code for every operation to check and propagate NaN
- Denormal support requires special handling in normalization
- Without special values, exponent=0 and exponent=max are normal numbers

### 7.3 Arithmetic Behavior

Defines how operations compute results.

**Components:**

- **Rounding mode**: How to handle results that don't fit exactly
- **Guard bits**: Extra precision during computation
- **Normalization strategy**: When to normalize mantissa
- **Intermediate precision**: Can exceed storage precision

**Why these cluster together**: These affect numeric quality and performance of operations. They are independent of the format itself.

**Rounding modes:**

- **ToNearest (ties to even)**: Round to closest representable value, ties choose even mantissa. IEEE 754 default. Most accurate.
- **TowardZero (truncate)**: Round toward zero (ceiling for negative, floor for positive). Simplest, fastest.
- **TowardPositive (ceiling)**: Always round up.
- **TowardNegative (floor)**: Always round down.
- **ToNearest (ties away from zero)**: Alternative tie-breaking rule, not IEEE 754 standard.

**Guard bits:**

Number of extra precision bits maintained during computation:
- 0: No guard bits. Truncate results. Fast but inaccurate.
- 3: Guard, Round, Sticky bits. IEEE 754 minimum. Correct rounding.
- 8: Full guard byte. Easier arithmetic on 8-bit platforms.
- 16+: Extra precision for iterative algorithms.

**Normalization:**

- **Always**: Maintain normalized form (leading 1) throughout computation
- **Lazy**: Normalize only when packing result
- **Never**: Allow denormalized intermediate values

**Examples:**

```cpp
// IEEE 754 compliant
RoundingPolicy<ToNearest>
GuardBitsPolicy<3>
NormalizationPolicy<Always>

// Fast 6502
RoundingPolicy<TowardZero>
GuardBitsPolicy<0>
NormalizationPolicy<Lazy>

// High precision
RoundingPolicy<ToNearest>
GuardBitsPolicy<16>
NormalizationPolicy<Always>
```

**Interactions**:
- Rounding modes except TowardZero require guard bits
- Guard bits increase intermediate storage requirements
- Lazy normalization reduces operations but complicates packing

### 7.4 Special Value Handling

Defines what happens when special values are encountered.

**Why separate from Special Value Vocabulary**: A format can have NaN encoding but flush all NaNs to zero. The vocabulary says "NaN exists as a bit pattern," the handling says "what do we do when we see one?"

**Components:**

- **NaN propagation**: Return NaN, throw error, or flush to zero
- **Infinity arithmetic**: Follow IEEE rules or treat as error
- **Denormal handling**: Full support, flush to zero (FTZ), flush inputs (FIZ), flush outputs (FOZ)
- **Overflow behavior**: Return infinity, saturate to max, or undefined
- **Underflow behavior**: Gradual (denormals), flush to zero, or undefined

**Denormal handling modes:**

- **Full support**: Gradual underflow per IEEE 754. Expensive.
- **FTZ (Flush To Zero)**: Treat denormal inputs as zero, flush denormal outputs to zero. Common optimization.
- **FIZ (Flush Inputs to Zero)**: Treat denormal inputs as zero, but can produce denormal outputs.
- **FOZ (Flush Outputs to Zero)**: Accept denormal inputs, but flush denormal outputs to zero.
- **No support**: Denormal inputs/outputs produce undefined behavior.

**Examples:**

```cpp
// IEEE 754 compliant
struct IEEE_SpecialHandling {
    static constexpr bool propagate_nan = true;
    static constexpr bool infinity_arithmetic = true;
    static constexpr DenormalMode denormals = DenormalMode::FullSupport;
    static constexpr OverflowMode overflow = OverflowMode::ToInfinity;
};

// Fast path (no special handling)
struct Fast_SpecialHandling {
    static constexpr bool propagate_nan = false;
    static constexpr bool infinity_arithmetic = false;
    static constexpr DenormalMode denormals = DenormalMode::FlushToZero;
    static constexpr OverflowMode overflow = OverflowMode::Undefined;
};
```

**Interactions**:
- Denormal handling mode only matters if denormals are in vocabulary
- NaN propagation only relevant if NaN is in vocabulary
- Flush-to-zero modes save significant code size and cycles

### 7.5 Error Management

Defines how errors are detected and reported.

**Components:**

- **Error detection**: Which conditions are tracked
- **Error reporting mechanism**: How errors reach the user
- **Error propagation**: How errors flow through calculations

**Error conditions:**

- **Invalid operation**: 0/0, sqrt(-1), inf-inf
- **Division by zero**: x/0 where x ≠ 0
- **Overflow**: Result magnitude too large to represent
- **Underflow**: Result magnitude too small to represent (or flushes to zero)
- **Inexact**: Result was rounded (not exactly representable)

**Reporting mechanisms:**

- **None**: No error checking. Smallest code size. Errors produce garbage.
- **errno**: Set global `errno` variable. C standard approach. Not thread-safe.
- **Exceptions**: Throw C++ exceptions. Allows stack unwinding. Large code size.
- **Local flags**: Return status alongside result. Thread-safe. Caller must check.
- **Global flags**: Status register (like FPU status). Not thread-safe but fast.
- **Callbacks**: Call registered error handler. Flexible but complex.

**Examples:**

```cpp
// No error checking (smallest code)
ErrorPolicy<ErrorHandling::None>

// IEEE 754 style (track all errors in flags)
struct IEEE_ErrorPolicy {
    static constexpr bool track_invalid = true;
    static constexpr bool track_div_by_zero = true;
    static constexpr bool track_overflow = true;
    static constexpr bool track_underflow = true;
    static constexpr bool track_inexact = true;
    using mechanism = LocalFlags;
};

// Exceptions for critical errors only
struct SafetyErrorPolicy {
    static constexpr bool track_invalid = true;
    static constexpr bool track_div_by_zero = true;
    static constexpr bool track_overflow = false;
    static constexpr bool track_underflow = false;
    static constexpr bool track_inexact = false;
    using mechanism = Exceptions;
};
```

**Interactions**:
- Error detection adds checks to every operation
- Exception mechanism requires C++ exception support
- Local flags require modified function signatures (return struct with status)

### 7.6 Storage Format

Defines the physical bit layout in memory.

**Components:**

- **Field order**: Which field occupies which bits
- **Packing**: Tight bitfields or byte-aligned
- **Endianness**: Little, big, or native byte order
- **Padding**: Allowed between fields or not

**Why this matters**: Binary interchange requires matching layouts. Performance may benefit from particular orderings. Some platforms prefer byte-aligned access.

**Field order examples:**

```
IEEE 754 standard bit numbering (MSB to LSB):
[sign][exponent][mantissa]
Bit 31: sign, Bits 30-23: exponent, Bits 22-0: mantissa

Reversed for some platforms:
[mantissa][exponent][sign]
Easier to mask off mantissa as low bits

Split sign:
[exponent][mantissa][sign]
Uncommon but simplifies some operations
```

**Packing strategies:**

- **Tight**: Use bitfields to pack exactly. No wasted bits.
- **Byte-aligned**: Each field starts on byte boundary. Wastes bits but faster access.
- **Platform native**: Let compiler decide. May add padding.

**Examples:**

```cpp
// IEEE 754 standard layout
struct StorageLayout_IEEE {
    enum class FieldOrder { SignExpMant };  // MSB to LSB
    enum class Packing { Tight };
    enum class Endian { Little };
};

// Byte-aligned for faster access
struct StorageLayout_Fast {
    enum class FieldOrder { SignExpMant };
    enum class Packing { ByteAligned };
    enum class Endian { Native };
};
```

**Interactions**:
- Endianness matters for binary file I/O
- Packing affects structure size (may not match format bit count)
- Field order is cosmetic unless doing binary I/O

### 7.7 Hardware Capabilities

Describes what the target platform provides in hardware.

**Components:**

- **Integer multiply**: Hardware or software
- **Integer divide**: Hardware or software
- **Barrel shifter**: Single-cycle multi-bit shifts
- **CLZ/CTZ**: Count leading/trailing zeros
- **SIMD**: Vector operations

**Why this matters**: Algorithm selection depends on available hardware. Software multiply on 6502 takes 200+ cycles. Hardware multiply on ARM takes 1 cycle. Different algorithms are optimal.

**Platform examples:**

```cpp
// 6502: minimal hardware support
struct MOS6502_Hardware {
    static constexpr bool has_multiply = false;
    static constexpr bool has_divide = false;
    static constexpr bool has_barrel_shift = false;
    static constexpr bool has_clz = false;
    static constexpr bool has_simd = false;
};

// ARM Cortex-M4: moderate support
struct ARM_M4_Hardware {
    static constexpr bool has_multiply = true;   // 1 cycle
    static constexpr bool has_divide = true;     // 2-12 cycles
    static constexpr bool has_barrel_shift = true;
    static constexpr bool has_clz = true;
    static constexpr bool has_simd = false;
};

// x86-64: full support
struct x86_64_Hardware {
    static constexpr bool has_multiply = true;
    static constexpr bool has_divide = true;
    static constexpr bool has_barrel_shift = true;
    static constexpr bool has_clz = true;
    static constexpr bool has_simd = true;  // SSE/AVX
};
```

**Algorithm implications:**

Without hardware multiply:
- Use shift-and-add algorithms
- Lookup tables for partial products
- Avoid multiplication where possible

With hardware multiply:
- Direct multiply instructions
- More straightforward algorithms

Without CLZ:
- Linear search for leading 1
- Or lookup table

With CLZ:
- Direct normalization in O(1)

**Interactions**:
- Hardware capabilities affect code generation hints
- May influence guard bit choice (byte vs bit)
- Determines feasibility of certain algorithms

### 7.8 Code Generation Hints

Guides the compiler toward better code generation.

**Components:**

- **Preferred integer width**: 8, 16, 24, 32, or 64 bit operations
- **Type selection strategy**: Use `_BitInt(N)` for exact sizes, or `uint_least_N`
- **Loop unroll hints**: Whether to suggest unrolling
- **Lookup table preference**: Trade ROM/flash for speed
- **Inline depth**: How aggressively to inline

**Why these are hints, not requirements**: The compiler makes final decisions. These are suggestions based on platform characteristics.

**Type selection:**

```cpp
// Prefer exact-width BitInt types
struct TypePolicy_Exact {
    template<int Bits>
    using uint_type = unsigned _BitInt(Bits);
    
    // Use exactly: uint _BitInt(24) for 24-bit values
};

// Prefer standard types (more portable)
struct TypePolicy_Standard {
    template<int Bits>
    using uint_type = 
        std::conditional_t<Bits <= 8, uint_least8_t,
        std::conditional_t<Bits <= 16, uint_least16_t,
        std::conditional_t<Bits <= 32, uint_least32_t,
        uint_least64_t>>>;
    
    // Use: uint_least8_t, uint_least16_t, uint_least32_t
};
```

**Platform-specific hints:**

```cpp
// 6502: prefer byte operations, avoid unrolling
struct CodeGenHints_6502 {
    static constexpr int preferred_int_width = 8;
    static constexpr bool use_bitint = false;
    static constexpr bool hint_unroll = false;
    static constexpr bool use_lookup_tables = true;  // ROM is cheap
    static constexpr int inline_depth = 2;
};

// x86-64: prefer wide operations, aggressive unrolling
struct CodeGenHints_x86_64 {
    static constexpr int preferred_int_width = 64;
    static constexpr bool use_bitint = true;
    static constexpr bool hint_unroll = true;
    static constexpr bool use_lookup_tables = false;  // Cache misses
    static constexpr int inline_depth = 8;
};
```

**Loop unrolling example:**

```cpp
template<typename Hints>
void shift_loop(uint32_t& mantissa, int count) {
    if constexpr (Hints::hint_unroll) {
        #pragma GCC unroll 4
        for (int i = 0; i < count; i++) {
            mantissa >>= 1;
        }
    } else {
        for (int i = 0; i < count; i++) {
            mantissa >>= 1;
        }
    }
}
```

**Interactions**:
- Hardware capabilities inform hints
- Type selection affects intermediate storage size
- Lookup table preference trades memory for speed

### 7.9 Operation Set

Defines which operations are available.

**Basic operations:**
- Addition, subtraction, multiplication, division

**Extended operations:**
- Square root
- Fused multiply-add (FMA)
- Remainder/modulo

**Transcendental functions:**
- sin, cos, tan
- log, log10, log2
- exp, exp2, exp10
- pow, hypot

**Conversions:**
- To/from integers
- Between float formats
- To/from strings

**Comparisons:**
- Ordered comparisons (<, <=, >, >=, ==, !=)
- Unordered comparisons (handling NaN)

**Why configurable**: Not all operations are needed. Embedded systems may only need basic arithmetic. Including unused operations wastes code space.

**Example:**

```cpp
// Minimal operation set
struct MinimalOps {
    static constexpr bool has_add = true;
    static constexpr bool has_sub = true;
    static constexpr bool has_mul = true;
    static constexpr bool has_div = true;
    static constexpr bool has_sqrt = false;
    static constexpr bool has_fma = false;
    static constexpr bool has_transcendentals = false;
};

// Extended operation set
struct ExtendedOps {
    static constexpr bool has_add = true;
    static constexpr bool has_sub = true;
    static constexpr bool has_mul = true;
    static constexpr bool has_div = true;
    static constexpr bool has_sqrt = true;
    static constexpr bool has_fma = true;
    static constexpr bool has_transcendentals = false;
};
```

**Interactions**:
- Some operations require others (tan needs sin and cos)
- sqrt may need extra guard bits for accuracy
- FMA (fused multiply-add) benefits from extra intermediate precision

### 7.10 Integer Arithmetic Strategy

Defines how integer multiply and divide operations are performed during floating point calculations.

**Why this matters**: Floating point multiplication requires mantissa multiplication. On platforms without hardware multiply, this is the dominant cost. The strategy depends critically on operand bit width.

**Multiply strategies:**

- **Hardware**: Single instruction (1-3 cycles on most platforms)
- **Lookup table**: Pre-computed table for small bit widths
- **Shift-and-add**: Iterative algorithm, N iterations for N-bit multiply
- **Booth's algorithm**: Fewer iterations than naive shift-and-add
- **Russian peasant**: Alternative iterative approach
- **Hybrid**: Choose based on operand size at compile time

**Divide strategies:**

- **Hardware**: Single instruction (typically 12-40 cycles)
- **Reciprocal lookup + multiply**: Table of 1/x values
- **Shift-and-subtract**: Long division algorithm
- **Newton-Raphson**: Iterative reciprocal approximation
- **Goldschmidt**: Alternative iterative method

**Policy definition:**

```cpp
enum class IntMultiplyStrategy {
    Hardware,
    Lookup,
    ShiftAndAdd,
    Booth,
    Hybrid
};

enum class IntDivideStrategy {
    Hardware,
    ReciprocalLookup,
    ShiftAndSubtract,
    Newton,
    Goldschmidt
};

template<IntMultiplyStrategy MultStrat, IntDivideStrategy DivStrat>
struct IntegerArithmeticPolicy {
    static constexpr IntMultiplyStrategy multiply_strategy = MultStrat;
    static constexpr IntDivideStrategy divide_strategy = DivStrat;
    
    // Lookup table size limits
    static constexpr int max_multiply_table_bits = 10;  // Max 1024 entries
    static constexpr int max_divide_table_bits = 8;     // Max 256 entries
};
```

**Automatic strategy selection:**

```cpp
template<typename Traits, int BitsA, int BitsB>
auto multiply_integers(uint_a, uint_b) {
    constexpr int total_bits = BitsA + BitsB;
    
    if constexpr (Traits::hardware::has_multiply) {
        return uint_a * uint_b;  // Hardware instruction
        
    } else if constexpr (total_bits <= Traits::int_arith::max_multiply_table_bits) {
        // Use lookup table
        int index = (uint_a << BitsB) | uint_b;
        return multiply_table[index];
        
    } else {
        // Fall back to shift-and-add
        return shift_add_multiply(uint_a, uint_b, BitsA);
    }
}
```

**Lookup table example for FP8 E5M2:**

With 2-bit mantissa + implicit bit = 3-bit values (0-7), multiply table is 3+3=6 bits = 64 entries:

```cpp
// 64-byte table for 3×3 bit multiplication
const uint8_t multiply_3x3_table[64] = {
    // Index = (a << 3) | b, value = a * b
    0,  0,  0,  0,  0,  0,  0,  0,   // 0×0 through 0×7
    0,  1,  2,  3,  4,  5,  6,  7,   // 1×0 through 1×7
    0,  2,  4,  6,  8, 10, 12, 14,   // 2×0 through 2×7
    0,  3,  6,  9, 12, 15, 18, 21,   // 3×0 through 3×7
    0,  4,  8, 12, 16, 20, 24, 28,   // 4×0 through 4×7
    0,  5, 10, 15, 20, 25, 30, 35,   // 5×0 through 5×7
    0,  6, 12, 18, 24, 30, 36, 42,   // 6×0 through 6×7
    0,  7, 14, 21, 28, 35, 42, 49    // 7×0 through 7×7
};

// Usage (6502 assembly):
// lda mantissa_a    ; 3 cycles
// asl               ; 2 cycles
// asl               ; 2 cycles  
// asl               ; 2 cycles
// ora mantissa_b    ; 3 cycles
// tax               ; 2 cycles
// lda multiply_3x3_table,x  ; 4 cycles
// ; Result in A, total: 18 cycles
```

**Performance comparison for E5M2 on 6502:**

- Generic 8×8 multiply (shift-and-add): ~80 cycles
- 3×3 multiply with lookup: ~18 cycles
- **Speedup: 4.4×**

This 18-cycle multiply enables the ~78 cycle total for FP8 E5M2 multiplication, making floating point competitive with integer-only code on 8-bit platforms.

**Interactions:**
- Lookup table strategy requires ROM/flash space
- Table size grows as 2^(BitsA + BitsB)
- Hardware capability policy determines if lookup is needed
- Mantissa bit width from format policy determines table size

### 7.11 Implementation Strategy

Defines where operation implementations come from.

**This is the key to platform integration.**

**Available strategies:**

- **Pure C++**: Template-generated code (default)
- **Inline assembly**: Assembly embedded in C++ functions
- **External assembly**: Separate .s files linked in
- **ROM routines**: Platform firmware calls
- **Intrinsics**: Compiler built-ins
- **Hardware**: Native float operations
- **External library**: Link to existing libm or similar
- **Hybrid**: Mix strategies per operation

**Implementation policy:**

```cpp
template<typename OperationSet>
struct ImplementationPolicy {
    using operations = OperationSet;
};

// Default: pure C++ implementation
template<typename Traits>
struct DefaultOps {
    static storage_type add(storage_type a, storage_type b) {
        // Full C++ implementation using policies
    }
    
    static storage_type mul(storage_type a, storage_type b) {
        // Full C++ implementation
    }
    
    // ... all operations
};
```

**Platform override example (hand-optimized assembly):**

```cpp
// 6502 with hand-optimized multiply
template<typename Traits>
struct MOS6502_FastOps {
    // Use C++ for most operations
    using Base = DefaultOps<Traits>;
    
    static storage_type add(storage_type a, storage_type b) {
        return Base::add(a, b);  // Use C++
    }
    
    static storage_type sub(storage_type a, storage_type b) {
        return Base::sub(a, b);  // Use C++
    }
    
    // Override multiply with assembly
    static storage_type mul(storage_type a, storage_type b) {
        extern storage_type float_mul_asm(storage_type, storage_type);
        return float_mul_asm(a, b);
    }
    
    static storage_type div(storage_type a, storage_type b) {
        return Base::div(a, b);  // Use C++
    }
};
```

**ROM routine example (Apple II):**

```cpp
// Apple II using Wozniak's ROM routines
template<typename Traits>
struct AppleII_WozOps {
    static storage_type add(storage_type a, storage_type b) {
        // Set FAC = a, ARG = b
        // JSR $E7BE (FADD in ROM)
        // Return result from FAC
        __asm__ volatile (
            "lda %1+0\n"
            "sta $9D\n"  // FAC
            "lda %1+1\n"
            "sta $9E\n"
            "lda %1+2\n"
            "sta $9F\n"
            "lda %1+3\n"
            "sta $A0\n"
            "lda %2+0\n"
            "sta $A5\n"  // ARG
            "lda %2+1\n"
            "sta $A6\n"
            "lda %2+2\n"
            "sta $A7\n"
            "lda %2+3\n"
            "sta $A8\n"
            "jsr $E7BE\n"  // FADD
            "lda $9D\n"
            "sta %0+0\n"
            "lda $9E\n"
            "sta %0+1\n"
            "lda $9F\n"
            "sta %0+2\n"
            "lda $A0\n"
            "sta %0+3\n"
            : "=m" (result)
            : "m" (a), "m" (b)
            : "a"
        );
        return result;
    }
    
    static storage_type mul(storage_type a, storage_type b) {
        // Similar, JSR $E97F (FMUL)
    }
    
    // ... etc
};
```

**Why this works:**

The implementation policy is just another template parameter. Platform maintainers provide their own operation structs. The library selects them through traits. No core library changes needed.

**Calling convention requirements:**

Platform-specific implementations must match the calling convention:
- Parameter passing (registers, stack, zero-page)
- Return value location
- Preserved registers
- Stack alignment

The trait system can encode these requirements and validate at compile time.

**Testing strategy:**

Every platform-specific implementation should be validated against the C++ reference:

```cpp
template<typename Traits>
void validate_implementation() {
    using CPP = DefaultOps<Traits>;
    using Platform = /* platform ops */;
    
    // Test all input combinations
    for (auto a : test_values) {
        for (auto b : test_values) {
            auto cpp_result = CPP::mul(a, b);
            auto platform_result = Platform::mul(a, b);
            assert(cpp_result == platform_result);
        }
    }
}
```

## 8. Policy Grouping

### The Problem

A complete configuration requires 10 policy dimensions:

```cpp
FloatEngine<
    FormatPolicy<1, 8, 23>,
    SpecialValuePolicy<true, true, true, true>,
    RoundingPolicy<ToNearest>,
    GuardBitsPolicy<3>,
    ErrorPolicy<LocalFlags>,
    StorageLayoutPolicy<IEEE>,
    HardwarePolicy<MOS6502>,
    CodeGenHintsPolicy<MOS6502>,
    OperationSetPolicy<BasicOps>,
    ImplementationPolicy<DefaultOps>
>
```

This is unworkable. Template error messages are illegible. Changing one policy requires retyping everything. Users make mistakes in parameter order.

### Solution: Trait Bundles

Group related policies into trait structs:

```cpp
struct IEEE754_Binary32_Traits {
    // Format cluster
    using format = FormatPolicy<1, 8, 23>;
    
    // Special values cluster
    using specials = SpecialValuePolicy<true, true, true, true>;
    
    // Arithmetic behavior cluster
    using rounding = RoundingPolicy<ToNearest>;
    using guard_bits = GuardBitsPolicy<3>;
    using normalization = NormalizationPolicy<Always>;
    
    // Error management
    using errors = ErrorPolicy<LocalFlags>;
    
    // Storage (usually default)
    using storage = StorageLayoutPolicy<IEEE>;
    
    // Platform (must specify)
    using hardware = HardwarePolicy<x86_64>;
    using codegen = CodeGenHintsPolicy<x86_64>;
    
    // Operations
    using operations = OperationSetPolicy<BasicOps>;
    using implementation = ImplementationPolicy<DefaultOps>;
};

// Usage: single parameter
FloatEngine<IEEE754_Binary32_Traits>
```

### Policy Groups

Further simplify by grouping policies that commonly change together:

```cpp
// Format group: structure + vocabulary
template<int S, int E, int M>
struct FormatGroup {
    using structure = FormatPolicy<S, E, M>;
    using implicit_bit = ImplicitBitPolicy<true>;
    using exponent_bias = ExponentBiasPolicy<-1>;  // Standard
};

// Compliance group: special values + handling + errors
struct ComplianceGroup_Full {
    using specials = SpecialValuePolicy<true, true, true, true>;
    using special_handling = SpecialHandlingPolicy<IEEE754>;
    using errors = ErrorPolicy<LocalFlags>;
};

struct ComplianceGroup_Minimal {
    using specials = SpecialValuePolicy<false, false, false, false>;
    using special_handling = SpecialHandlingPolicy<None>;
    using errors = ErrorPolicy<None>;
};

// Arithmetic group: rounding + guard bits + normalization
struct ArithmeticGroup_Accurate {
    using rounding = RoundingPolicy<ToNearest>;
    using guard_bits = GuardBitsPolicy<8>;
    using normalization = NormalizationPolicy<Always>;
};

struct ArithmeticGroup_Fast {
    using rounding = RoundingPolicy<TowardZero>;
    using guard_bits = GuardBitsPolicy<0>;
    using normalization = NormalizationPolicy<Lazy>;
};

// Platform group: hardware + codegen + implementation
template<typename Arch>
struct PlatformGroup {
    using hardware = HardwarePolicy<Arch>;
    using codegen = CodeGenHintsPolicy<Arch>;
    using operations = OperationSetPolicy<BasicOps>;
    using implementation = ImplementationPolicy<DefaultOps<Arch>>;
};
```

**Simplified trait definition:**

```cpp
template<
    typename Format,
    typename Compliance,
    typename Arithmetic,
    typename Platform
>
struct StandardTraits {
    // Unpack groups
    using format = typename Format::structure;
    using implicit_bit = typename Format::implicit_bit;
    using exponent_bias = typename Format::exponent_bias;
    
    using specials = typename Compliance::specials;
    using special_handling = typename Compliance::special_handling;
    using errors = typename Compliance::errors;
    
    using rounding = typename Arithmetic::rounding;
    using guard_bits = typename Arithmetic::guard_bits;
    using normalization = typename Arithmetic::normalization;
    
    using hardware = typename Platform::hardware;
    using codegen = typename Platform::codegen;
    using operations = typename Platform::operations;
    using implementation = typename Platform::implementation;
};

// Usage: 4 parameters instead of 10+
using MyFloat = FloatEngine<StandardTraits<
    FormatGroup<1, 8, 23>,
    ComplianceGroup_Minimal,
    ArithmeticGroup_Fast,
    PlatformGroup<MOS6502>
>>;
```

### Named Configurations

Define complete preset configurations:

```cpp
// IEEE 754 binary32
using IEEE754_Binary32 = StandardTraits<
    FormatGroup<1, 8, 23>,
    ComplianceGroup_Full,
    ArithmeticGroup_Accurate,
    PlatformGroup<NativePlatform>
>;

// Fast 6502 float
using C64_Fast = StandardTraits<
    FormatGroup<1, 8, 23>,
    ComplianceGroup_Minimal,
    ArithmeticGroup_Fast,
    PlatformGroup<MOS6502>
>;

// Usage: single name
FloatEngine<IEEE754_Binary32>
FloatEngine<C64_Fast>
```

### Selective Override

Inherit from base trait and override specific policies:

```cpp
// Start with IEEE 754 binary32
struct MyCustomFloat : IEEE754_Binary32_Traits {
    // Override: disable denormals for speed
    using specials = SpecialValuePolicy<
        true,   // Keep NaN
        true,   // Keep Infinity
        true,   // Keep signed zeros
        false   // No denormals
    >;
    
    // Override: flush denormals to zero
    using special_handling = SpecialHandlingPolicy<FTZ>;
};

FloatEngine<MyCustomFloat>
```

### Strategy Summary

1. **Individual policies**: Fine-grained control, verbose
2. **Policy groups**: Cluster related policies, moderate verbosity
3. **Trait bundles**: Named complete configurations, concise
4. **Selective override**: Start with preset, modify specific aspects

Use trait bundles for common cases. Use selective override for variations. Reserve individual policy specification for unusual requirements.

## 9. Architecture

### Template Structure

The library consists of four layers:

**Layer 1: Public Interface**

```cpp
template<typename Traits>
class FloatEngine {
public:
    using storage_type = typename Traits::format::storage_type;
    
    static storage_type add(storage_type a, storage_type b);
    static storage_type sub(storage_type a, storage_type b);
    static storage_type mul(storage_type a, storage_type b);
    static storage_type div(storage_type a, storage_type b);
    
    // Additional operations based on Traits::operations
};
```

Users interact only with this layer. All complexity is hidden.

**Layer 2: Policy Extraction**

```cpp
template<typename Traits>
class FloatEngine {
private:
    // Extract all policies from traits
    using format = typename Traits::format;
    using rounding = typename Traits::rounding;
    using guard_bits = typename Traits::guard_bits;
    using specials = typename Traits::specials;
    using errors = typename Traits::errors;
    using implementation = typename Traits::implementation;
    // ... etc
    
    // Derived types
    using storage_type = typename format::storage_type;
    using mantissa_type = typename format::mantissa_type;
    using exponent_type = typename format::exponent_type;
};
```

This layer unpacks trait bundles into individual policies. Type aliases computed from policies.

**Layer 3: Operation Dispatch**

```cpp
template<typename Traits>
storage_type FloatEngine<Traits>::add(storage_type a, storage_type b) {
    // Dispatch to selected implementation
    return implementation::operations::add(a, b);
}
```

Operations delegate to the implementation selected by traits. This enables platform-specific optimization.

**Layer 4: Implementation**

```cpp
template<typename Traits>
struct DefaultOps {
    static storage_type add(storage_type a, storage_type b) {
        // Unpack operands
        auto [a_sign, a_exp, a_mant] = unpack<Traits>(a);
        auto [b_sign, b_exp, b_mant] = unpack<Traits>(b);
        
        // Handle special values if enabled
        if constexpr (Traits::specials::has_nan) {
            if (is_nan(a_exp, a_mant)) return a;
            if (is_nan(b_exp, b_mant)) return b;
        }
        
        // Align exponents
        // Add mantissas
        // Normalize
        // Round based on Traits::rounding
        // Pack result
        
        return pack<Traits>(sign, exp, mant);
    }
};
```

Actual arithmetic. Uses `if constexpr` for compile-time branches. Only code for enabled features is generated.

### Type Selection

Type selection based on bit requirements is critical for efficiency:

```cpp
// Select smallest integer type that fits N bits
template<int N>
struct SelectIntType {
    using type = 
        std::conditional_t<(N <= 8), uint_least8_t,
        std::conditional_t<(N <= 16), uint_least16_t,
        std::conditional_t<(N <= 24), unsigned _BitInt(24),
        std::conditional_t<(N <= 32), uint_least32_t,
        uint_least64_t>>>>;
};

// Format policy uses this
template<int S, int E, int M>
struct FormatPolicy {
    static constexpr int sign_bits = S;
    static constexpr int exponent_bits = E;
    static constexpr int mantissa_bits = M;
    static constexpr int total_bits = S + E + M;
    
    // Storage type: exactly sized for total bits
    using storage_type = typename SelectIntType<total_bits>::type;
    
    // Field types: sized for individual fields
    using mantissa_type = typename SelectIntType<M>::type;
    using exponent_type = typename SelectIntType<E>::type;
    
    // Computation types: sized for intermediate values
    using mantissa_compute_type = 
        typename SelectIntType<M + guard_bits>::type;
    using exponent_compute_type = 
        typename SelectIntType<E + 2>::type;  // Extra for over/underflow
};
```

**Why this matters:**

On 6502 (8-bit operations fast, 16-bit slow, 32-bit very slow):
- FP8 operations use only 8-bit arithmetic
- 16-bit float uses 8-bit and 16-bit operations
- 32-bit float requires some 32-bit operations

Selecting the right types means the compiler generates optimal code for each format.

**Example: FP8 E5M2**

```cpp
using FP8 = FormatPolicy<1, 5, 2>;

// Generates:
// storage_type = uint8_t (8 bits total)
// mantissa_type = uint8_t (2 bits fit in 8)
// exponent_type = uint8_t (5 bits fit in 8)
// mantissa_compute_type = uint8_t (2 + 3 guard bits = 5, fits in 8)
// exponent_compute_type = uint8_t (5 + 2 = 7, fits in 8)
```

All operations stay in 8-bit arithmetic. On 6502, this is extremely fast.

**Example: IEEE 754 Binary32**

```cpp
using Binary32 = FormatPolicy<1, 8, 23>;

// Generates:
// storage_type = uint32_t (32 bits total)
// mantissa_type = uint32_t (23 bits needs 32-bit)
// exponent_type = uint8_t (8 bits fits in 8)
// mantissa_compute_type = uint32_t (23 + 3 = 26, needs 32-bit)
// exponent_compute_type = uint16_t (8 + 2 = 10, needs 16-bit)
```

Mantissa operations are 32-bit, exponent operations are 8-bit or 16-bit. Optimal for each field.

### Implementation Selection

How platform-specific implementations are chosen:

```cpp
// Default: use C++ implementation
template<typename Traits>
struct SelectImplementation {
    using type = DefaultOps<Traits>;
};

// Specialization for specific platform + traits
template<>
struct SelectImplementation<C64_Fast_Traits> {
    using type = MOS6502_FastOps<C64_Fast_Traits>;
};

// FloatEngine uses selected implementation
template<typename Traits>
class FloatEngine {
private:
    using Impl = typename SelectImplementation<Traits>::type;
    
public:
    static storage_type add(storage_type a, storage_type b) {
        return Impl::add(a, b);
    }
};
```

Platform maintainers add specializations for their platform. No core library changes.

### Compile-Time Validation

Policies are validated at compile time:

```cpp
template<typename Traits>
struct ValidateTraits {
    // Check policy interactions
    static_assert(
        !Traits::rounding::needs_rounding || 
        Traits::guard_bits::guard_bits > 0,
        "Rounding modes other than TowardZero require guard bits"
    );
    
    static_assert(
        !Traits::specials::has_denormals || 
        Traits::normalization::normalize_on_pack,
        "Denormal support requires normalization on pack"
    );
    
    static_assert(
        Traits::format::sign_bits == 1,
        "Sign must be exactly 1 bit"
    );
    
    static_assert(
        Traits::format::exponent_bits >= 2,
        "Exponent must be at least 2 bits"
    );
    
    // Many more checks...
};

// Validate when instantiating FloatEngine
template<typename Traits>
class FloatEngine {
    ValidateTraits<Traits> _validate;
    // ...
};
```

Invalid configurations produce compiler errors with clear messages.

## 10. Adding Platform Implementations

### Writing Operation Structs

Platform-specific operations are provided as structs with static functions:

```cpp
template<typename Traits>
struct MyPlatform_Ops {
    using storage_type = typename Traits::format::storage_type;
    
    static storage_type add(storage_type a, storage_type b) {
        // Platform-specific implementation
    }
    
    static storage_type mul(storage_type a, storage_type b) {
        // Platform-specific implementation
    }
    
    // Implement all required operations
    // Or inherit from DefaultOps and override selectively
};
```

**Selective override:**

```cpp
template<typename Traits>
struct MyPlatform_Ops : DefaultOps<Traits> {
    using Base = DefaultOps<Traits>;
    
    // Use C++ for most operations
    using Base::add;
    using Base::sub;
    using Base::div;
    
    // Override just multiplication
    static storage_type mul(storage_type a, storage_type b) {
        // Hand-optimized version
    }
};
```

### Calling Conventions

Platform implementations must match calling conventions:

**LLVM-MOS (6502):**
- First 32 bits in rc0-rc3 (zero page)
- Return value in rc0-rc3
- Additional params on stack
- Preserved: X, Y registers

**Example:**

```cpp
// C++ wrapper
template<typename Traits>
struct MOS6502_Ops {
    static uint32_t mul(uint32_t a, uint32_t b) {
        uint32_t result;
        __asm__ volatile (
            // a already in rc0-rc3
            // b already in rc4-rc7
            "jsr float_mul_asm\n"
            // result in rc0-rc3
            : "=r" (result)
            : "r" (a), "r" (b)
        );
        return result;
    }
};

// Assembly implementation (float_mul_asm.s)
// .global float_mul_asm
// float_mul_asm:
//     ; rc0-rc3 = a
//     ; rc4-rc7 = b
//     ; ... multiply algorithm ...
//     ; result in rc0-rc3
//     rts
```

**ARM AAPCS:**
- First 4 args in r0-r3
- Return in r0 (or r0-r1 for 64-bit)
- Preserved: r4-r11

**x86-64 SysV:**
- First 6 args in rdi, rsi, rdx, rcx, r8, r9
- Return in rax (or xmm0 for float)
- Preserved: rbx, rbp, r12-r15

The operation struct must encode these requirements:

```cpp
template<typename Traits>
struct ARM_Ops {
    static_assert(
        sizeof(typename Traits::format::storage_type) <= 8,
        "ARM AAPCS passes up to 8 bytes in registers"
    );
    
    // Implementation...
};
```

### Testing Against C++ Reference

Every platform implementation should be validated:

```cpp
template<typename Traits>
bool validate_platform_ops() {
    using CPP = DefaultOps<Traits>;
    using Platform = /* platform ops */;
    using storage_type = typename Traits::format::storage_type;
    
    // Test data covering edge cases
    std::vector<storage_type> test_values = {
        0,                    // Zero
        encode_max(),         // Max value
        encode_min(),         // Min normalized
        encode_denorm_min(),  // Min denormal (if supported)
        // ... many more cases
    };
    
    bool all_pass = true;
    
    // Test all operations
    for (auto a : test_values) {
        for (auto b : test_values) {
            // Addition
            auto cpp_add = CPP::add(a, b);
            auto plat_add = Platform::add(a, b);
            if (cpp_add != plat_add) {
                log_mismatch("add", a, b, cpp_add, plat_add);
                all_pass = false;
            }
            
            // Multiplication
            auto cpp_mul = CPP::mul(a, b);
            auto plat_mul = Platform::mul(a, b);
            if (cpp_mul != plat_mul) {
                log_mismatch("mul", a, b, cpp_mul, plat_mul);
                all_pass = false;
            }
            
            // ... test other operations
        }
    }
    
    return all_pass;
}
```

Run this test suite before merging platform-specific code.

### Build Integration

CMake example:

```cmake
# Platform-specific sources
if(TARGET_PLATFORM STREQUAL "MOS6502")
    set(PLATFORM_SOURCES
        platforms/mos6502/float_ops_asm.s
        platforms/mos6502/float_ops_wrapper.cpp
    )
    enable_language(ASM)
    add_compile_definitions(HAVE_MOS6502_ASM)
    
elseif(TARGET_PLATFORM STREQUAL "ARM")
    set(PLATFORM_SOURCES
        platforms/arm/float_ops_neon.cpp
    )
    add_compile_definitions(HAVE_ARM_NEON)
    
endif()

# Library
add_library(float_engine
    float_engine.cpp
    float_impl_default.cpp
    ${PLATFORM_SOURCES}
)

# LTO for dead code elimination
set_target_properties(float_engine PROPERTIES
    INTERPROCEDURAL_OPTIMIZATION TRUE
)
```

Configuration header:

```cpp
// float_config.h (generated by CMake)
#ifndef FLOAT_CONFIG_H
#define FLOAT_CONFIG_H

#cmakedefine HAVE_MOS6502_ASM
#cmakedefine HAVE_ARM_NEON
#cmakedefine HAVE_X86_SSE

#endif
```

Platform-specific selection:

```cpp
#include "float_config.h"

template<typename Traits>
struct SelectImplementation {
    #ifdef HAVE_MOS6502_ASM
        using type = MOS6502_Ops<Traits>;
    #elif defined(HAVE_ARM_NEON)
        using type = ARM_NEON_Ops<Traits>;
    #else
        using type = DefaultOps<Traits>;
    #endif
};
```

### Contributing Platform Code

To add a new platform:

1. Create `platforms/myplatform/` directory
2. Implement operation struct in `myplatform_ops.cpp`
3. Add assembly files if needed
4. Create test file validating against C++ reference
5. Update CMakeLists.txt to detect platform
6. Document calling convention and requirements
7. Submit PR with test results

No core library changes required. Platform code is isolated.

## 11. Format Conversion

### The Conversion Problem

Converting between floating point formats is critical for modern applications:

**Machine learning workflows:**
- Training in FP32, inference in FP8
- Mixed-precision training (FP32 ↔ FP16)
- Quantization (FP32 → FP8/FP6/FP4)

**Legacy integration:**
- Converting between IEEE 754 and historical formats
- Apple II format ↔ modern formats
- Microsoft BASIC format ↔ standard formats

**Format experimentation:**
- Researchers testing E4M3 vs E5M2 vs E3M4
- Evaluating custom formats for specific domains
- Optimizing format choice per layer in neural networks

Conversion is complex because formats differ in:
- **Range** (different exponent bits)
- **Precision** (different mantissa bits)
- **Special values** (NaN, Inf, denormals present or absent)
- **Conventions** (signed zeros, exponent bias, FNUZ variants)

### User Interface

Conversions should require minimal code:

```cpp
// Implicit conversion (uses default policy)
FloatEngine<FP32_Traits> src = ...;
FloatEngine<FP8_E4M3_Traits> dst = src;  // Just works

// Explicit conversion
auto dst = static_cast<FloatEngine<FP8_E4M3_Traits>>(src);

// With custom conversion policy
auto dst = src.convert_to<FP8_E4M3_Traits, StrictConversionPolicy>();

// Free function form
auto dst = convert<FP8_E4M3_Traits>(src);
auto dst = convert<FP8_E4M3_Traits, CustomPolicy>(src);
```

### Conversion Policies

Conversion behavior is controlled by policies covering four dimensions:

**1. Range Handling**

What happens when source value exceeds destination range:

```cpp
enum class OverflowMode {
    Saturate,     // Clip to destination max value
    ToInfinity,   // Convert to Inf (if dest supports it)
    Error         // Signal error via chosen mechanism
};

enum class UnderflowMode {
    FlushToZero,  // Convert to zero
    ToDenormal,   // Use denormal representation (if dest supports it)
    Error         // Signal error
};

template<OverflowMode Overflow, UnderflowMode Underflow>
struct RangePolicy {
    static constexpr OverflowMode overflow = Overflow;
    static constexpr UnderflowMode underflow = Underflow;
};
```

**2. Special Value Mapping**

How to map special values when destination doesn't support them:

```cpp
struct SpecialMappingPolicy {
    enum class NaNMapping {
        Preserve,   // Keep as NaN if dest supports it
        ToMax,      // Convert to max representable value
        ToZero,     // Convert to zero
        Error       // Signal error
    };
    
    enum class InfMapping {
        Preserve,   // Keep as Inf if dest supports it
        ToMax,      // Convert to max representable value
        Error       // Signal error
    };
    
    enum class DenormalMapping {
        Preserve,   // Keep as denormal if dest supports it
        FlushToZero, // Convert to zero
        Error       // Signal error
    };
    
    static constexpr NaNMapping nan_mapping = ...;
    static constexpr InfMapping inf_mapping = ...;
    static constexpr DenormalMapping denormal_mapping = ...;
};
```

**3. Precision Handling**

When narrowing mantissa (e.g., FP32 23 bits → FP8 3 bits):

```cpp
struct PrecisionPolicy {
    using rounding = RoundingPolicy<...>;      // How to round
    using guard_bits = GuardBitsPolicy<...>;   // Intermediate precision
};
```

Typically uses destination format's rounding policy, but can be overridden.

**4. Error Handling**

How to report conversion errors:

```cpp
enum class ConversionErrorMode {
    Silent,         // Map to valid value, continue
    SetFlag,        // Set thread-local status flag
    Exception,      // Throw C++ exception
    CompileError    // static_assert (when detectable at compile time)
};

template<ConversionErrorMode Mode>
struct ConversionErrorPolicy {
    static constexpr ConversionErrorMode mode = Mode;
    
    // What conditions trigger errors
    static constexpr bool nan_is_error = false;
    static constexpr bool inf_is_error = false;
    static constexpr bool overflow_is_error = false;
    static constexpr bool underflow_is_error = false;
};
```

### Preset Conversion Policy Bundles

Most users won't configure individual policies. Preset bundles cover common cases:

```cpp
// Safe: correctness over speed (default)
struct SafeConversionPolicy {
    using range = RangePolicy<
        OverflowMode::Saturate,
        UnderflowMode::FlushToZero
    >;
    using specials = SpecialMappingPolicy<
        NaNMapping::Preserve,
        InfMapping::Preserve,
        DenormalMapping::Preserve
    >;
    using precision = PrecisionPolicy<
        RoundingPolicy<RoundingMode::ToNearest>,
        GuardBitsPolicy<3>
    >;
    using error_policy = ConversionErrorPolicy<
        ConversionErrorMode::Silent
    >;
};

// Strict: error on any potential data loss
struct StrictConversionPolicy {
    using range = RangePolicy<
        OverflowMode::Error,
        UnderflowMode::Error
    >;
    using specials = SpecialMappingPolicy<
        NaNMapping::Error,
        InfMapping::Error,
        DenormalMapping::Error
    >;
    using precision = PrecisionPolicy<
        RoundingPolicy<RoundingMode::ToNearest>,
        GuardBitsPolicy<3>
    >;
    using error_policy = ConversionErrorPolicy<
        ConversionErrorMode::Exception
    >;
};

// Fast: performance over exactness
struct FastConversionPolicy {
    using range = RangePolicy<
        OverflowMode::Saturate,
        UnderflowMode::FlushToZero
    >;
    using specials = SpecialMappingPolicy<
        NaNMapping::ToMax,        // Skip NaN checks
        InfMapping::ToMax,        // Skip Inf checks
        DenormalMapping::FlushToZero
    >;
    using precision = PrecisionPolicy<
        RoundingPolicy<RoundingMode::TowardZero>,  // Truncate
        GuardBitsPolicy<0>
    >;
    using error_policy = ConversionErrorPolicy<
        ConversionErrorMode::Silent
    >;
};

// ML inference: training weights → inference weights
struct MLInferenceConversionPolicy {
    using range = RangePolicy<
        OverflowMode::Saturate,   // Clip outliers
        UnderflowMode::FlushToZero
    >;
    using specials = SpecialMappingPolicy<
        NaNMapping::Error,        // NaN in weights indicates bug
        InfMapping::ToMax,
        DenormalMapping::FlushToZero
    >;
    using precision = PrecisionPolicy<
        RoundingPolicy<RoundingMode::ToNearest>,
        GuardBitsPolicy<3>
    >;
    using error_policy = ConversionErrorPolicy<
        ConversionErrorMode::Exception
    >;
};
```

### Conversion Algorithm

The generic conversion algorithm:

1. **Unpack source** - extract sign, exponent, mantissa, special flags
2. **Handle special values** - NaN, Inf, zero (policy-driven)
3. **Adjust exponent bias** - convert from source bias to dest bias
4. **Check range** - detect overflow/underflow (policy-driven)
5. **Convert mantissa** - widen (shift left) or narrow (round)
6. **Handle mantissa overflow** - rounding can cause mantissa overflow
7. **Pack destination** - assemble result in destination format

**Critical cases:**

**Widening mantissa (FP8 → FP32):**
- Shift left, pad with zeros
- Exact conversion, no information loss

**Narrowing mantissa (FP32 → FP8):**
- Extract guard, round, sticky bits
- Apply rounding policy
- Check for mantissa overflow after rounding
- If mantissa overflows, increment exponent and check for exponent overflow

**Exponent adjustment:**
- Unbias source exponent: `exp - source_bias`
- Rebias for destination: `(exp - source_bias) + dest_bias`
- Check result against destination exponent range

**Special value mapping:**
- Source NaN → Dest NaN (if supported), else max value or error
- Source Inf → Dest Inf (if supported), else max value or error
- Source denormal → Dest denormal (if supported), else flush to zero
- Signed zero handling: map ±0 appropriately for FNUZ formats

### Platform-Specific Conversion Implementations

Like arithmetic operations, conversions can have platform-specific optimizations:

```cpp
// Generic conversion (works everywhere)
template<typename SrcTraits, typename DstTraits, typename ConvPolicy>
struct GenericConversion {
    static typename DstTraits::format::storage_type
    convert(typename SrcTraits::format::storage_type src) {
        // Full algorithm: unpack, convert, pack
    }
};

// Platform-optimized conversion
template<typename SrcTraits, typename DstTraits, typename ConvPolicy>
struct MOS6502_E5M2_to_E4M3_Conversion {
    static uint8_t convert(uint8_t src) {
        // Lookup table approach
        // E5M2 → E4M3 is 256-entry table
        extern const uint8_t e5m2_to_e4m3_table[256];
        return e5m2_to_e4m3_table[src];
    }
};

// Conversion implementation selector
template<typename SrcTraits, typename DstTraits, typename ConvPolicy>
struct ConversionImplementation {
    using type = GenericConversion<SrcTraits, DstTraits, ConvPolicy>;
};

// Specialization for optimized case
template<>
struct ConversionImplementation<
    E5M2_Traits, 
    E4M3_Traits, 
    SafeConversionPolicy
> {
    using type = MOS6502_E5M2_to_E4M3_Conversion<
        E5M2_Traits, 
        E4M3_Traits, 
        SafeConversionPolicy
    >;
};
```

**Benefits:**
- FP8 ↔ FP8 conversions can use lookup tables (256 or 65K entries)
- Platform-specific bit manipulation tricks
- SIMD implementations for batch conversions
- No changes to user code

### Error Handling Mechanisms

**Silent mode (default):**
```cpp
FP8 dst = fp32_src;  // Overflow → saturate to max, no error signal
```

**Exception mode:**
```cpp
try {
    auto dst = src.convert_to<FP8_Traits, StrictConversionPolicy>();
} catch (const ConversionException& e) {
    // Handle overflow, underflow, NaN conversion, etc.
}
```

**Status flag mode:**
```cpp
ConversionStatus::clear();
FP8 dst = fp32_src;  // Uses policy with SetFlag mode

if (ConversionStatus::get_error() != ConversionStatus::Error::None) {
    // Handle error
}
```

**Compile-time error mode:**
```cpp
// If source always has NaN and dest never does, and policy treats as error:
struct NeverNaN_Traits {
    using specials = SpecialValuePolicy<false, false, false, false>;
};

struct AlwaysNaN_Source {
    using specials = SpecialValuePolicy<true, true, true, true>;
};

// This would fail at compile time with appropriate policy:
// NeverNaN dst = always_nan_src;  // static_assert fires
```

### Conversion Testing

Conversions must be tested as thoroughly as operations:

**Exhaustive testing for FP8:**
- All 256 values in source format
- Convert to destination format
- Verify against reference implementation
- Test all policy combinations

**Round-trip testing:**
```cpp
// FP32 → FP8 → FP32 should be "close"
for (auto original : test_values) {
    auto fp8 = convert<FP8_Traits>(original);
    auto back = convert<FP32_Traits>(fp8);
    assert(approximately_equal(original, back, tolerance));
}
```

**Monotonicity testing:**
```cpp
// If a < b, then convert(a) ≤ convert(b)
assert(convert(a) <= convert(b));
```

**Property testing:**
- Conversion preserves sign (except for special cases)
- Conversion preserves ordering (monotonicity)
- Special values map correctly

**Cross-validation:**
- Generic implementation vs platform-optimized implementation
- Different policy combinations produce expected different results

### Conversion Matrix

For N formats, need N² conversion functions. These are generated automatically:

```cpp
using AllFormats = std::tuple<
    FP8_E4M3_Traits,
    FP8_E5M2_Traits,
    FP8_E4M3FNUZ_Traits,
    FP8_E5M2FNUZ_Traits,
    FP16_Traits,
    BFloat16_Traits,
    FP32_Traits,
    FP64_Traits
>;

// Generates 8×8 = 64 conversion functions
// (including identity conversions which are no-ops)
template<typename FormatList>
struct GenerateAllConversions;
```

Each conversion function is instantiated on first use. Dead conversions eliminated by linker.

### Real-World Use Cases

**ML model quantization:**
```cpp
// Load FP32 training weights
std::vector<FP32> training_weights = load_model();

// Quantize to FP8 for inference
std::vector<FP8_E4M3> inference_weights;
for (auto w : training_weights) {
    inference_weights.push_back(convert<FP8_E4M3_Traits>(w));
}
```

**Format experimentation:**
```cpp
// Test different formats for a layer
auto accuracy_e4m3 = benchmark_layer<FP8_E4M3>(data);
auto accuracy_e5m2 = benchmark_layer<FP8_E5M2>(data);
auto accuracy_e3m4 = benchmark_layer<FP8_E3M4>(data);

// Pick best format
using BestFormat = /* format with highest accuracy */;
```

**Mixed-precision inference:**
```cpp
// Some layers in FP8, some in FP16
FP8_E4M3 conv_output = conv_layer_fp8(input);
FP16 attention_output = attention_layer_fp16(
    convert<FP16_Traits>(conv_output)
);
FP8_E4M3 final = convert<FP8_E4M3_Traits>(attention_output);
```

### Format Compatibility Notes

**FNUZ variants (Finite Numbers, Unsigned Zero):**
- No infinity: overflow saturates to max value
- No negative zero: only one zero representation
- Non-standard exponent bias
- Requires special handling in conversions

**Conversion examples:**
```cpp
// IEEE 754 → FNUZ: Inf becomes max value
FP8_E4M3FNUZ dst = fp32_inf;  // → max representable value

// FNUZ → IEEE 754: can add Inf if overflow
FP32 dst = fp8_fnuz_max;  // → stays at max (no Inf unless overflow)

// Signed zero → unsigned zero
FP8_E4M3FNUZ dst = fp32_negative_zero;  // → positive zero
```

The conversion system handles these variants through policy configuration and format trait specifications.

## 12. Microscaling (MX) Formats

### The Microscaling Concept

Microscaling (MX) is a compression technique that groups values into blocks with a shared scale factor. Instead of each value having its own exponent, a block of values shares a single exponent while maintaining individual mantissas.

**Key insight:** Most tensor values in a local region have similar magnitudes. Sharing an exponent across a block saves bits while maintaining acceptable precision.

**MX Format Structure:**
- **Element type**: The data type for individual values (e.g., FP8 E4M3, FP6, INT8)
- **Scale type**: The data type for the shared scale factor (typically E8M0)
- **Block size**: Number of consecutive elements sharing one scale (typically 32)

**Example: MXFP8 with E4M3 elements**
- 32 FP8 E4M3 values: 32 × 8 = 256 bits
- 1 shared E8M0 scale: 8 bits
- Total: 264 bits for 32 values
- Compared to individual E4M3: 32 × 8 = 256 bits (but worse dynamic range)
- Compared to FP32: 32 × 32 = 1024 bits

### MX Format Specification

An MX-compliant format consists of:
- Scale (X) data type / encoding (w bits)
- Private elements (Pᵢ) data type / encoding (d bits each)
- Scaling block size (k)

Each block of k elements is encoded in (w + kd) bits.

**Decoding values:**
Given scale X and elements P₁, P₂, ..., Pₖ:

- If X = NaN, then vᵢ = NaN for all i (entire block is NaN)
- If X ≠ NaN:
  - If Pᵢ ∈ {Inf, NaN}, then vᵢ = Pᵢ (element special values preserved)
  - Otherwise, vᵢ = X × Pᵢ (scale times element value)

If X × Pᵢ exceeds float32 range, behavior is implementation-defined (saturate, error, or Inf).

### Standard MX Formats

| Format Name | Element Type | Element Bits | Block Size | Scale Type | Scale Bits |
|-------------|-------------|--------------|------------|------------|------------|
| MXFP8 (E5M2) | FP8 E5M2 | 8 | 32 | E8M0 | 8 |
| MXFP8 (E4M3) | FP8 E4M3 | 8 | 32 | E8M0 | 8 |
| MXFP6 (E3M2) | FP6 E3M2 | 6 | 32 | E8M0 | 8 |
| MXFP6 (E2M3) | FP6 E2M3 | 6 | 32 | E8M0 | 8 |
| MXFP4 | FP4 E2M1 | 4 | 32 | E8M0 | 8 |
| MXINT8 | INT8 | 8 | 32 | E8M0 | 8 |

**Note:** MXFP8 has two variants (E5M2 and E4M3) which are considered different formats.

### E8M0 Scale Format

The E8M0 format represents pure powers of 2:
- 8-bit exponent, 0-bit mantissa
- No sign bit (scales are always positive powers of 2)
- Value = 2^(exponent - bias), where bias = 127
- Can represent scales from 2^-127 to 2^128

```cpp
struct E8M0_Traits {
    using format = FormatPolicy<0, 8, 0>;  // No sign, 8-bit exp, no mantissa
    static constexpr int exponent_bias = 127;
};

// E8M0 represents a power-of-2 scale factor
float scale_value = std::pow(2.0f, exponent - 127);
```

### Microscaling Policy

MX formats are defined by three parameters:

```cpp
template<
    typename ElementTraits,    // Type of individual elements
    typename ScaleTraits,      // Type of shared scale factor
    int BlockSize              // Elements per block
>
struct MicroscalingPolicy {
    using element_traits = ElementTraits;
    using scale_traits = ScaleTraits;
    static constexpr int block_size = BlockSize;
    
    // Derived properties
    static constexpr int element_bits = ElementTraits::format::total_bits;
    static constexpr int scale_bits = ScaleTraits::format::total_bits;
    static constexpr int bits_per_block = scale_bits + (block_size * element_bits);
    
    // Scale selection strategy
    enum class ScaleStrategy {
        MaxAbsValue,     // Scale to fit largest absolute value
        RMS,             // Root mean square
        Percentile99     // 99th percentile (ignore outliers)
    };
    static constexpr ScaleStrategy scale_strategy = ScaleStrategy::MaxAbsValue;
};
```

**Standard format definitions:**

```cpp
// MXFP8 with E4M3 elements
using MXFP8_E4M3 = MicroscalingPolicy<
    E4M3_Traits,
    E8M0_Traits,
    32
>;

// MXFP8 with E5M2 elements
using MXFP8_E5M2 = MicroscalingPolicy<
    E5M2_Traits,
    E8M0_Traits,
    32
>;

// MXFP6 with E3M2 elements
using MXFP6_E3M2 = MicroscalingPolicy<
    E3M2_Traits,
    E8M0_Traits,
    32
>;

// MXFP4
using MXFP4 = MicroscalingPolicy<
    E2M1_Traits,
    E8M0_Traits,
    32
>;
```

### MX Tensor Container

MX formats are storage/compression schemes, not numeric types. They are implemented as containers:

```cpp
template<typename MXPolicy>
class MXTensor {
private:
    struct Block {
        FloatEngine<typename MXPolicy::scale_traits> scale;
        FloatEngine<typename MXPolicy::element_traits> 
            elements[MXPolicy::block_size];
    };
    
    std::vector<Block> blocks_;
    size_t size_;
    
public:
    MXTensor(size_t size);
    
    // Access individual elements (decompresses on read)
    float get(size_t index) const;
    
    // Set individual elements (may trigger re-quantization)
    void set(size_t index, float value);
    
    // Batch operations
    void set_block(size_t block_idx, std::span<const float> values);
    std::span<const float> get_block(size_t block_idx) const;
    
    size_t size() const { return size_; }
    size_t num_blocks() const { return blocks_.size(); }
};
```

**Dequantization algorithm (get):**

```cpp
template<typename MXPolicy>
float MXTensor<MXPolicy>::get(size_t index) const {
    size_t block_idx = index / MXPolicy::block_size;
    size_t elem_idx = index % MXPolicy::block_size;
    
    auto& block = blocks_[block_idx];
    
    // Handle NaN scale: entire block is NaN
    if (block.scale.is_nan()) {
        return std::numeric_limits<float>::quiet_NaN();
    }
    
    auto elem = block.elements[elem_idx];
    
    // Preserve element special values
    if (elem.is_inf() || elem.is_nan()) {
        return elem.to_float();
    }
    
    // Compute v_i = X × P_i
    float scale_value = std::pow(2.0f, block.scale.exponent() - 127);
    float elem_value = elem.to_float();
    float result = scale_value * elem_value;
    
    // Handle overflow (implementation-defined)
    if (std::abs(result) > std::numeric_limits<float>::max()) {
        return std::copysign(std::numeric_limits<float>::max(), result);
    }
    
    return result;
}
```

**Quantization algorithm (set_block):**

```cpp
template<typename MXPolicy>
void MXTensor<MXPolicy>::set_block(
    size_t block_idx, 
    std::span<const float> values
) {
    // Step 1: Find optimal scale for block
    float max_abs = 0.0f;
    for (float v : values) {
        if (std::isfinite(v)) {
            max_abs = std::max(max_abs, std::abs(v));
        }
    }
    
    // Step 2: Determine scale as power of 2
    // Scale chosen so max value fits in element range
    using ElemTraits = typename MXPolicy::element_traits;
    constexpr float elem_max = /* max representable in element type */;
    
    int scale_exp = std::ceil(std::log2(max_abs / elem_max)) + 127;
    scale_exp = std::clamp(scale_exp, 0, 255);
    
    blocks_[block_idx].scale = 
        FloatEngine<typename MXPolicy::scale_traits>::from_bits(scale_exp);
    
    // Step 3: Quantize each element
    float scale_value = std::pow(2.0f, scale_exp - 127);
    
    for (size_t i = 0; i < values.size(); i++) {
        float scaled = values[i] / scale_value;
        blocks_[block_idx].elements[i] = 
            convert<ElemTraits>(scaled);
    }
}
```

### Quantization and Dequantization Functions

```cpp
// Quantize float array to MX format
template<typename MXPolicy, typename QuantPolicy = DefaultQuantPolicy>
MXTensor<MXPolicy> quantize(std::span<const float> values) {
    MXTensor<MXPolicy> result(values.size());
    
    size_t num_blocks = (values.size() + MXPolicy::block_size - 1) 
                       / MXPolicy::block_size;
    
    for (size_t b = 0; b < num_blocks; b++) {
        size_t start = b * MXPolicy::block_size;
        size_t end = std::min(start + MXPolicy::block_size, values.size());
        
        result.set_block(b, values.subspan(start, end - start));
    }
    
    return result;
}

// Dequantize MX format to float array
template<typename MXPolicy>
std::vector<float> dequantize(const MXTensor<MXPolicy>& tensor) {
    std::vector<float> result(tensor.size());
    
    for (size_t i = 0; i < tensor.size(); i++) {
        result[i] = tensor.get(i);
    }
    
    return result;
}

// Convert between MX formats
template<typename DstMXPolicy, typename SrcMXPolicy>
MXTensor<DstMXPolicy> convert_mx(const MXTensor<SrcMXPolicy>& src) {
    // Dequantize to float, re-quantize to new format
    auto float_values = dequantize(src);
    return quantize<DstMXPolicy>(float_values);
}
```

### Real-World Use Cases

**ML Model Quantization:**

```cpp
// Training weights in FP32
std::vector<float> training_weights = load_model();

// Quantize to MXFP8 for inference
MXTensor<MXFP8_E4M3> inference_weights = quantize<MXFP8_E4M3>(training_weights);

// Later: dequantize for computation
auto weights_fp32 = dequantize(inference_weights);
```

**Mixed Precision Storage:**

```cpp
// Store activations in MXFP6, weights in MXFP8
MXTensor<MXFP6_E3M2> activations = quantize<MXFP6_E3M2>(activation_data);
MXTensor<MXFP8_E4M3> weights = quantize<MXFP8_E4M3>(weight_data);

// Compute in FP16 or FP32
auto act_fp32 = dequantize(activations);
auto wgt_fp32 = dequantize(weights);
// ... perform computation ...
```

**Format Experimentation:**

```cpp
// Test different MX formats for a layer
auto loss_e4m3 = evaluate_layer(quantize<MXFP8_E4M3>(weights));
auto loss_e5m2 = evaluate_layer(quantize<MXFP8_E5M2>(weights));
auto loss_fp6  = evaluate_layer(quantize<MXFP6_E3M2>(weights));

// Choose format with best accuracy
```

### Advantages of MX Formats

**Improved dynamic range:**
- FP8 with per-tensor scale: all values must fit in FP8 range
- MXFP8 with per-block scale: each block can use different scale
- Allows E4M3 (better precision) instead of E5M2 (better range)

**Compression efficiency:**
- Better than per-tensor quantization (more scales)
- Less overhead than per-element full floats
- Sweet spot for tensor data with local magnitude similarity

**Hardware efficiency:**
- Shared exponent simplifies hardware
- NVIDIA H100 has native MXFP8 support
- Vectorized operations benefit from uniform block structure

### Integration with Library

MX formats leverage existing infrastructure:

**Element types use existing FloatEngine:**
- E4M3, E5M2, E3M2, E2M1 are already supported
- No new float operations needed

**Scale type is just another format:**
- E8M0 is a valid (if unusual) float format
- Uses same FormatPolicy structure

**Conversion system handles quantization:**
- Quantization uses existing convert<> infrastructure
- Scale finding is the only new logic

**Testing applies:**
- Round-trip testing: float → MX → float
- Block-level exhaustive testing for small formats
- Property testing: monotonicity, scale correctness

### Custom MX Formats

Users can define custom MX formats using the policy system:

```cpp
// Custom: 16-bit elements, 64 per block
using CustomMX = MicroscalingPolicy<
    FP16_Traits,    // 16-bit elements
    E8M0_Traits,    // Standard scale
    64              // Larger blocks
>;

// Custom: 4-bit elements, smaller blocks
using TinyMX = MicroscalingPolicy<
    FP4_E2M1_Traits,
    E8M0_Traits,
    16              // Smaller blocks for better adaptation
>;

// Use like standard formats
MXTensor<CustomMX> my_tensor = quantize<CustomMX>(data);
```

The microscaling policy system provides production-ready support for NVIDIA's MX formats while maintaining flexibility for experimentation and custom formats.

## 13. Testing and Validation

### The Testing Challenge

A policy-based float library presents unique testing challenges:

- **Format variations**: FP8 E5M2, E4M3, FP16, FP32, custom formats
- **Policy combinations**: Rounding modes, special values, guard bits, error handling
- **Platform implementations**: C++ reference vs assembly vs ROM routines
- **Semantic variations**: IEEE compliant vs fast-and-wrong configurations

Testing every permutation manually is infeasible. The solution combines exhaustive testing for small formats with automated policy enumeration.

### Testing as a Policy Dimension

Testing strategy is itself a policy that varies by format:

```cpp
enum class TestStrategy {
    Exhaustive,        // Test all possible input combinations
    EdgePlusSampling,  // Boundary cases + random sampling
    PropertyBased,     // Algebraic properties only
    Targeted,          // Hand-picked difficult cases
    None               // No automated tests
};

template<TestStrategy Strategy, size_t SampleCount = 10000>
struct TestingPolicy {
    static constexpr TestStrategy strategy = Strategy;
    static constexpr size_t sample_count = SampleCount;
    
    // Edge case coverage
    static constexpr bool test_zero = true;
    static constexpr bool test_inf = true;
    static constexpr bool test_nan = true;
    static constexpr bool test_denormals = true;
    static constexpr bool test_powers_of_two = true;
    static constexpr bool test_near_one = true;
    
    // Sampling strategy
    static constexpr bool uniform_sampling = true;
    static constexpr bool logarithmic_sampling = false;
};

// Format size determines feasible test strategy
template<typename Format>
struct DefaultTestingPolicy {
    static constexpr TestStrategy strategy = 
        (Format::total_bits <= 8)  ? TestStrategy::Exhaustive :
        (Format::total_bits <= 16) ? TestStrategy::EdgePlusSampling :
                                     TestStrategy::Targeted;
    
    static constexpr size_t sample_count = 
        (Format::total_bits <= 16) ? 1000000 :  // 1M samples for FP16
        (Format::total_bits <= 32) ? 100000 :   // 100K for FP32
                                     10000;     // 10K for FP64
};
```

Testing policy is included in trait bundles:

```cpp
struct IEEE754_Binary32_Traits {
    using format = FormatPolicy<1, 8, 23>;
    using rounding = RoundingPolicy<ToNearest>;
    // ... other policies
    
    using testing = TestingPolicy<
        TestStrategy::EdgePlusSampling,
        100000
    >;
};
```

### Exhaustive Testing for Small Formats

**Key insight**: FP8 and FP16 formats have small enough value spaces to test exhaustively.

**FP8 formats (E5M2, E4M3):**
- 256 possible values
- 256 × 256 = 65,536 input pairs per binary operation
- All operations (add, sub, mul, div) = ~260,000 total tests
- Runs in seconds on modern hardware
- **Provides mathematical proof of correctness**

**FP16 (binary16):**
- 65,536 possible values
- 4.3 billion input pairs per operation
- Takes hours but is feasible
- Can parallelize across CPU cores

**Exhaustive test implementation:**

```cpp
template<typename Traits>
void test_exhaustive_fp8() {
    using Float = FloatEngine<Traits>;
    using storage_type = typename Traits::format::storage_type;
    
    size_t failures = 0;
    
    // Test every possible input combination
    for (uint16_t a_bits = 0; a_bits < 256; a_bits++) {
        for (uint16_t b_bits = 0; b_bits < 256; b_bits++) {
            auto a = static_cast<storage_type>(a_bits);
            auto b = static_cast<storage_type>(b_bits);
            
            // Test multiplication
            auto result = Float::mul(a, b);
            auto expected = reference_mul<Traits>(a, b);
            
            if (result != expected) {
                log_failure("mul", a, b, result, expected);
                failures++;
            }
            
            // Test addition
            result = Float::add(a, b);
            expected = reference_add<Traits>(a, b);
            
            if (result != expected) {
                log_failure("add", a, b, result, expected);
                failures++;
            }
            
            // ... test other operations
        }
    }
    
    if (failures == 0) {
        std::cout << "PASS: All " << (256*256) << " cases correct\n";
    } else {
        std::cout << "FAIL: " << failures << " incorrect results\n";
    }
}
```

**What exhaustive testing proves:**

If exhaustive testing passes for a given trait configuration:
- The implementation is mathematically correct for those policies
- No edge cases or corner cases remain untested
- The code can be trusted for production use

This is much stronger than statistical sampling or random testing.

### Test Execution Based on Policy

The test framework uses the testing policy to select appropriate strategy:

```cpp
template<typename Traits>
void run_tests() {
    if constexpr (Traits::testing::strategy == TestStrategy::Exhaustive) {
        test_exhaustive<Traits>();
        
    } else if constexpr (Traits::testing::strategy == TestStrategy::EdgePlusSampling) {
        test_edge_cases<Traits>();
        test_random_samples<Traits>(Traits::testing::sample_count);
        
    } else if constexpr (Traits::testing::strategy == TestStrategy::PropertyBased) {
        test_commutativity<Traits>();
        test_associativity<Traits>();
        test_identity<Traits>();
        
    } else if constexpr (Traits::testing::strategy == TestStrategy::Targeted) {
        test_boundary_values<Traits>();
        test_powers_of_two<Traits>();
        test_difficult_cases<Traits>();
    }
    
    // Property tests always run as sanity checks
    if constexpr (Traits::testing::strategy != TestStrategy::None) {
        verify_basic_properties<Traits>();
    }
}
```

### Compile-Time Policy Enumeration

For larger formats, test representative policy combinations by generating all valid configurations at compile time.

**Cartesian product of policy lists:**

```cpp
// Define policy dimensions
using AllFormats = std::tuple<
    FormatPolicy<1, 5, 2>,   // FP8 E5M2
    FormatPolicy<1, 4, 3>,   // FP8 E4M3
    FormatPolicy<1, 5, 10>,  // FP16
    FormatPolicy<1, 8, 23>   // FP32
>;

using AllRoundingModes = std::tuple<
    RoundingPolicy<RoundingMode::ToNearest>,
    RoundingPolicy<RoundingMode::TowardZero>
>;

using AllSpecialValues = std::tuple<
    SpecialValuePolicy<false, false, false, false>,  // No specials
    SpecialValuePolicy<false, true, false, false>,   // Infinity only
    SpecialValuePolicy<true, true, true, true>       // Full IEEE 754
>;

using AllGuardBits = std::tuple<
    GuardBitsPolicy<0>,
    GuardBitsPolicy<3>,
    GuardBitsPolicy<8>
>;

// Cartesian product: all combinations
using AllCombinations = CartesianProduct<
    AllFormats,
    AllRoundingModes,
    AllSpecialValues,
    AllGuardBits
    // ... more policy dimensions
>;
// Results in: 4 × 2 × 3 × 3 = 72 configurations
```

**The `CartesianProduct` template** can be implemented in ~50-100 lines of template metaprogramming, or use Boost.MP11's `mp_product`. The technique is proven and works in standard C++.

**Filter invalid combinations:**

```cpp
// Check if a trait bundle is valid
template<typename Traits>
struct IsValidConfiguration {
    static constexpr bool value = 
        // Rounding constraint
        (!Traits::rounding::needs_rounding || 
         Traits::guard_bits::guard_bits > 0) &&
        
        // Denormal constraint  
        (!Traits::specials::has_denormals || 
         Traits::normalization::normalize_on_pack) &&
        
        // Add more constraint checks
        true;
};

// Filter to only valid configurations
template<typename ConfigList>
using FilterValid = /* filter using IsValidConfiguration */;

using ValidConfigurations = FilterValid<AllCombinations>;
```

**Compile-time test instantiation:**

```cpp
// Iterate over all valid configurations at compile time
template<typename Tuple, typename Func, size_t... Is>
void for_each_type_impl(Func&& f, std::index_sequence<Is...>) {
    // C++17 fold expression: call f for each type
    (f.template operator()<std::tuple_element_t<Is, Tuple>>(), ...);
}

template<typename Tuple, typename Func>
void for_each_type(Func&& f) {
    for_each_type_impl<Tuple>(
        std::forward<Func>(f), 
        std::make_index_sequence<std::tuple_size_v<Tuple>>{}
    );
}

// Test runner
template<typename ValidConfigs>
struct TestRunner {
    static void run_all() {
        for_each_type<ValidConfigs>([]<typename Config>() {
            // Instantiate test for this configuration
            test_configuration<Config>();
        });
    }
};

// Usage
int main() {
    TestRunner<ValidConfigurations>::run_all();
    // Compiler instantiates test for EVERY valid configuration
    // Runtime executes them all
}
```

**Benefits:**

- All valid configurations tested automatically
- Invalid configurations fail at compile time (can't accidentally test broken configs)
- Adding a new policy dimension automatically tests all combinations
- No manual test case maintenance

**Cost:**

- Compilation time increases (72 configs × test code per config)
- Binary size increases (each config instantiates ~10KB of test code)
- For test suites, this is acceptable

### Testing Strategy Summary

**Tier 1: Exhaustive (FP8 formats)**
- Test every possible input combination
- Provides mathematical proof of correctness
- Required for all FP8 trait configurations

**Tier 2: Extensive sampling (FP16)**
- Exhaustive testing feasible but slow
- Optional: run overnight or on CI server
- Provides very high confidence

**Tier 3: Targeted testing (FP32, FP64)**
- Boundary values (zero, min, max, inf, nan)
- Powers of 2
- Values near 1.0
- Difficult cases (Kahan summation edge cases)
- Random sampling (10,000-100,000 cases)

**Tier 4: Cross-validation**
- Fast config tested against accurate config
- Accurate config tested against IEEE config  
- Platform assembly tested against C++ reference
- Each tier builds confidence in the next

**Tier 5: Property testing**
- Commutativity: a + b == b + a
- Associativity: (a + b) + c ≈ a + (b + c) within tolerance
- Identity: a + 0 == a, a × 1 == a, a × 0 == 0
- Monotonicity: if a < b and c > 0, then a×c < b×c
- Sign preservation: sign(a × b) == sign(a) × sign(b)

Properties hold regardless of precision or rounding mode (within tolerance).

### Reference Implementations

Each test needs a reference implementation to compare against:

**For IEEE-compliant configurations:**
- Compare against language built-in float/double (if same format)
- Compare against MPFR (arbitrary precision library)
- Compare against SoftFloat (Berkeley software float)

**For non-IEEE configurations:**
- C++ template implementation serves as reference
- Higher precision configuration serves as reference (FP8 → FP16 → FP32)
- Mathematically computed expected values for simple cases

**For platform-specific implementations:**
- Assembly version tested against C++ reference
- Both should produce identical bit patterns for same traits

### Continuous Integration

Recommended CI pipeline:

```yaml
# .github/workflows/test.yml (example)
- name: Exhaustive FP8 tests
  run: |
    ./run_tests --exhaustive --format=fp8
    # ~260K tests, runs in seconds

- name: Extensive FP16 tests  
  run: |
    ./run_tests --extensive --format=fp16
    # Billions of tests, runs for hours
  # Only on nightly builds or release branches

- name: Policy enumeration tests
  run: |
    ./run_tests --all-configs --format=fp32
    # All valid policy combinations

- name: Platform validation
  run: |
    ./run_tests --platform=mos6502 --validate-asm
    # Compare assembly against C++ reference
```

### Test Organization

Tests are automatically generated from policy combinations:

```
tests/
  generated_tests.cpp       # All trait combinations generated via CartesianProduct
  reference/
    mpfr_reference.cpp      # High-precision reference for validation
    softfloat_reference.cpp # IEEE 754 software reference
  platform/
    validate_asm.cpp        # Platform-specific implementation validation
  properties/
    arithmetic_laws.cpp     # Algebraic property tests (always run)
```

Each trait configuration includes its testing policy, so the test framework automatically:
- Uses exhaustive testing for FP8 formats
- Uses edge-plus-sampling for FP16
- Uses targeted testing for FP32 and larger
- Validates platform implementations against C++ reference

No manual test file creation needed - everything is generated from the Cartesian product of policies.

### Validation Confidence Levels

After running the full test suite:

**FP8 formats with exhaustive testing:**
- Confidence: Mathematical proof
- Can be used in production without reservation

**FP16 with extensive testing:**  
- Confidence: Extremely high (billions of test cases)
- Suitable for production use

**FP32 with targeted testing:**
- Confidence: High (boundary cases + sampling + properties)
- Suitable for production with standard validation practices

**Platform-specific implementations:**
- Confidence: Depends on coverage
- Assembly must match C++ reference bit-for-bit
- Any mismatch is a bug in assembly or calling convention

The combination of exhaustive testing (where feasible), automated policy enumeration, and cross-validation provides strong confidence in library correctness across the entire policy space.

## 14. Future Work

### Transcendental Functions

Functions like sin, cos, log, exp require:
- Range reduction algorithms
- Polynomial approximations or CORDIC
- Accuracy vs performance tradeoffs
- Table-driven vs computed approaches

Policy dimensions needed:
- Accuracy target (ulp error bound)
- Range reduction strategy
- Approximation method
- Table size limits

### Fixed-Point Types

Apply same policy-based design to fixed-point arithmetic:
- Integer bits and fractional bits (configurable)
- Rounding modes
- Overflow handling (saturate, wrap, error)
- Similar trait bundles

Potential for unified numeric library covering float and fixed.

### Format Conversions

Converting between formats:
- Widening (FP8 → Binary32)
- Narrowing (Binary32 → FP8)
- Same width different encoding (Binary32 → bfloat16)

Issues:
- Rounding modes during conversion
- Special value mapping
- Precision loss handling

### SIMD Operations

Where hardware supports it:
- Vectorized operations (4x float32, 8x float16, etc.)
- Platform-specific intrinsics
- Alignment requirements
- Fallback to scalar

Would require additional policy dimension for vector width.

## Appendices

### A. Policy Reference

**Number System Structure**

- `FormatPolicy<S, E, M>`: Bit allocation (sign, exponent, mantissa)
- `ExponentBiasPolicy<B>`: Bias value (-1 for standard, or explicit value)
- `ImplicitBitPolicy<bool>`: Hidden leading bit
- `SignRepresentationPolicy<Mode>`: Sign bit vs two's complement

**Special Values**

- `SpecialValuePolicy<NaN, Inf, SignedZero, Denormals>`: Which specials exist
- `SpecialHandlingPolicy<Mode>`: How to handle specials (propagate, flush, error)
- `DenormalHandlingPolicy<Mode>`: FTZ, FIZ, FOZ, full support, or none

**Arithmetic**

- `RoundingPolicy<Mode>`: ToNearest, TowardZero, TowardPositive, TowardNegative
- `GuardBitsPolicy<N>`: Number of extra precision bits (0, 3, 8, 16, ...)
- `NormalizationPolicy<When>`: Always, lazy, never

**Error Management**

- `ErrorDetectionPolicy<Flags>`: Which errors to detect
- `ErrorReportingPolicy<Mechanism>`: None, errno, exceptions, local flags, global flags
- `ErrorPropagationPolicy<Mode>`: How errors flow through calculations

**Storage**

- `StorageLayoutPolicy<Order, Packing, Endian>`: Physical memory layout
- `FieldOrderPolicy<Order>`: SignExpMant, MantExpSign, etc.
- `PackingPolicy<Mode>`: Tight, byte-aligned, native

**Platform**

- `HardwareCapabilitiesPolicy<Features>`: Multiply, divide, shifter, CLZ, SIMD
- `CodeGenHintsPolicy<Hints>`: Integer width preference, type strategy, unroll hints
- `CallingConventionPolicy<Conv>`: Register usage, stack layout

**Operations**

- `OperationSetPolicy<Ops>`: Which operations are available
- `ImplementationPolicy<Ops>`: Where implementations come from

**Testing**

- `TestingPolicy<Strategy, SampleCount>`: Testing approach and coverage
- `TestStrategy`: Exhaustive, EdgePlusSampling, PropertyBased, Targeted, None
- Default strategy automatically selected based on format size

**Conversion**

- `ConversionPolicy`: Bundle of conversion-specific policies
- `RangePolicy<Overflow, Underflow>`: Overflow/underflow handling
- `SpecialMappingPolicy<NaN, Inf, Denormal>`: Special value mapping
- `PrecisionPolicy<Rounding, GuardBits>`: Mantissa conversion precision
- `ConversionErrorPolicy<Mode>`: Error detection and reporting
- Preset bundles: SafeConversionPolicy, StrictConversionPolicy, FastConversionPolicy, MLInferenceConversionPolicy

**Microscaling**

- `MicroscalingPolicy<ElementTraits, ScaleTraits, BlockSize>`: MX format definition
- Element type: The numeric format for individual values (E4M3, E5M2, E3M2, E2M1, INT8, etc.)
- Scale type: Usually E8M0 (pure power-of-2 exponent)
- Block size: Typically 32 elements
- Standard formats: MXFP8_E4M3, MXFP8_E5M2, MXFP6_E3M2, MXFP6_E2M3, MXFP4, MXINT8
- Container: MXTensor<MXPolicy> for compressed storage
- Operations: quantize<MXPolicy>(), dequantize(), convert_mx()

### B. Policy Interactions

**Required Interactions:**

- Rounding mode != TowardZero → guard_bits > 0
- has_denormals = true → normalization handles denormals
- has_nan = true → exponent interpretation includes all-1s encoding
- error_reporting != None → error_detection must be enabled

**Optional Interactions:**

- Hardware multiply = false → prefer smaller guard byte over bits
- Platform prefer 8-bit → prefer formats ≤ 32 bits total
- Operation includes sqrt → may want extra guard bits

**Validation:**

All interactions checked at compile time with `static_assert`. Invalid combinations produce clear error messages.

### C. Platform Notes

**MOS 6502**

- 8-bit accumulator, 8-bit X/Y index registers
- No hardware multiply or divide
- 256 bytes zero page (fast access)
- LLVM-MOS uses zero page for "imaginary registers" rc0-rc31
- Calling convention: first 32 bits in rc0-rc3
- Multiplication: 200-300 cycles in software
- Prefer: byte operations, lookup tables, guard byte not bits

**ARM Cortex-M**

- M0/M0+: No hardware divide, 1-cycle multiply
- M3/M4: Hardware divide, 1-cycle multiply, optional FPU
- M7: Hardware FPU with double precision
- Thumb-2 instruction set
- AAPCS calling convention
- Prefer: 32-bit operations, hardware multiply when available

**x86-64**

- 64-bit registers
- Hardware FPU with 80-bit internal precision
- SSE/AVX for SIMD
- SysV ABI (Linux) or Microsoft ABI (Windows)
- Prefer: wide operations, aggressive inlining, avoid lookup tables

**RISC-V**

- 32-bit (RV32) or 64-bit (RV64) base
- Optional multiply/divide extension (M)
- Optional single-precision (F) or double-precision (D) FPU
- Simple calling convention: a0-a7 for args, a0-a1 for return
- Prefer: depends on extensions available

**AVR**

- 8-bit architecture
- No multiply on basic AVR, available on AVR-Enhanced
- 32 general-purpose registers
- Harvard architecture (separate code/data)
- Calling convention: r25-r8 for args
- Prefer: byte operations, code size critical
