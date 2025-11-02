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

### 7.10 Implementation Strategy

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

## 11. Future Work

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
