# Policy-Based Floating Point Library Design

## Part I: Introduction and Motivation

### 1. The Universal Float Problem

Modern computing spans an enormous range of platforms and requirements:

**The spectrum:**
- **Retro computing**: 6502 at 1 MHz, no hardware multiply, 64KB total RAM
- **Embedded systems**: ARM Cortex-M0 at 48 MHz, hardware multiply, 32KB RAM
- **ML accelerators**: Custom formats (FP8, FP6, FP4), massive parallelism, specialized hardware
- **High-end GPUs**: Thousands of cores, mixed precision, native FP8/FP16/FP32 support

**Each platform needs different floating point:**

The 6502 game developer needs fast approximate math with minimal code size. Exact IEEE 754 compliance would consume most of available RAM and run too slowly for real-time graphics.

The ML engineer needs to experiment with custom formats - maybe E4M3 works better than E5M2 for this layer, or perhaps E3M4 offers the right precision/range tradeoff. Formats change based on empirical results.

The edge device needs to compress model weights from training (FP32) to inference (FP8 with block-level scaling) while maintaining acceptable accuracy.

The Apple II programmer has floating point routines already in ROM - why reimplement them when you can just call them?

**Standard float libraries assume:**
- IEEE 754 compliance (expensive without hardware support)
- One format suffices for all use cases
- Runtime flexibility through switches or function pointers
- Hardware multiply and divide available
- Special values (NaN, Infinity, denormals) always needed

**None of these assumptions hold universally.**

The result: developers either accept bloated inefficient libraries, or write custom implementations for each platform, duplicating effort and introducing bugs.

### 2. The Solution: Configurable Definitions

**Core insight:** The mathematical definitions of add, subtract, multiply, and divide are implementation-dependent and problem-dependent.

IEEE 754 defines one set of semantics (round to nearest, ties to even, handle NaN/Inf, gradual underflow). But:
- Games might want "truncate and saturate" (faster, simpler)
- ML inference might want "flush denormals to zero" (performance)
- Scientific computing might want "error on any data loss" (correctness)

**The solution is policy-based design:**

Separate WHAT (the format and behavior) from HOW (the implementation). Make all choices at compile time. Generate exactly the code needed, nothing more.

**What you configure:**
- **Format**: How many bits for sign, exponent, mantissa
- **Arithmetic behavior**: Rounding mode, guard bits, normalization
- **Special values**: NaN, Infinity, denormals - on or off independently
- **Error handling**: Silent, exceptions, status flags, or compile-time errors
- **Platform**: Which hardware features available
- **Implementation**: Generic C++, hand-optimized assembly, ROM calls, hardware ops

**Key benefits:**
- **Zero runtime overhead**: All decisions made at compile time
- **Scales universally**: Same design works on 6502 and GPU
- **Pluggable optimizations**: Drop in assembly or hardware implementations
- **Sensible defaults**: Works correctly out of the box
- **Progressive refinement**: Start with working code, optimize hot paths later

**The same library supports:**
- Commodore 64 with 78-cycle FP8 multiply (faster than integer!)
- Apple II calling Wozniak's ROM routines (zero code size)
- NVIDIA H100 with native MXFP8 hardware instructions
- Custom ML format experimentation (E3M4, E4M3FNUZ, user inventions)

### 3. Motivating Example: FP8 on 6502

To make this concrete, consider a Commodore 64 game with a 100-particle system.

**The challenge:** Each particle needs position updates (2 multiplies) and velocity updates (2 multiplies) = 400 multiplies per frame at 60 fps.

**Option 1: IEEE 754 binary32**
- Generic software implementation: ~700 cycles per multiply
- Total: 280,000 cycles per frame
- At 1 MHz (1,000,000 cycles/sec ÷ 60 frames/sec = 16,666 cycles/frame available)
- Result: Can't achieve 60 fps, runs at ~6 fps

**Option 2: 8×8 integer multiply**
- Software shift-and-add: ~190 cycles per multiply
- Total: 76,000 cycles per frame
- Result: Achieves 21 fps, but choppy and no fractional values

**Option 3: FP8 E5M2 with this library**
- 3-bit mantissa multiply via 64-byte lookup table: ~18 cycles
- Total operation including unpack/pack: ~78 cycles per multiply
- Total: 31,200 cycles per frame
- Result: Smooth 60 fps with fractions and wide dynamic range

**Why FP8 wins:**
- All operations fit in 8-bit registers (A, X, Y on 6502)
- Mantissa multiply uses tiny lookup table instead of iteration
- Range: 0.00006 to 57,344 (vs 0-255 for 8-bit integers)
- Precision: ~2 significant digits (sufficient for particle positions/velocities)

**The configuration to achieve this:**
User specifies format (1 sign + 5 exponent + 2 mantissa), selects "fast" policies (no special values, truncate rounding, no error checking), and library generates optimal code including the lookup table automatically.

**Key point:** The library makes FP8 faster than integer arithmetic while providing floating point semantics. This is only possible through compile-time optimization based on exact format knowledge.

## Part II: What You Can Configure

### 4. Policy Dimensions Overview

The library separates concerns into independent policy dimensions. Each dimension controls one aspect of behavior:

**Format Specification:**
- Bit allocation: sign, exponent, mantissa bits
- Exponent bias: standard or custom
- Implicit bit: hidden leading 1 or explicit
- Total size: 8, 16, 24, 32, 64 bits or custom

**Special Value Vocabulary:**
- NaN support: yes/no
- Infinity support: yes/no
- Signed zeros: yes/no  
- Denormal numbers: yes/no

**Arithmetic Behavior:**
- Rounding mode: to nearest, toward zero, toward ±infinity
- Guard bits: 0, 3 (G+R+S), 8 (byte), 16+ (extended precision)
- Normalization: always, lazy, never
- Intermediate precision: can exceed storage precision

**Special Value Handling:**
- NaN propagation: preserve, convert to max, error
- Infinity arithmetic: IEEE rules, convert to max, error
- Denormal handling: full support, flush to zero, flush inputs, flush outputs
- Overflow behavior: saturate, infinity, undefined
- Underflow behavior: gradual, flush, undefined

**Error Management:**
- Detection: which conditions trigger errors
- Reporting: none, errno, exceptions, status flags, callbacks
- Propagation: how errors flow through calculations

**Storage Format:**
- Field ordering: sign-exp-mant, mant-exp-sign, custom
- Packing: tight bitfields, byte-aligned, native
- Endianness: little, big, native
- Padding: allowed or forbidden

**Hardware Capabilities:**
- Integer multiply: hardware or software
- Integer divide: hardware or software
- Barrel shifter: single-cycle or iterative
- CLZ/CTZ: count leading/trailing zeros instruction
- SIMD: vector operations available

**Integer Arithmetic Strategy:**
- Multiply: hardware, lookup table, shift-and-add, Booth's algorithm
- Divide: hardware, reciprocal table, shift-and-subtract, Newton-Raphson
- Table size limits for lookup-based approaches

**Code Generation Hints:**
- Preferred integer width: 8, 16, 24, 32, 64 bits
- Type selection: BitInt for exact sizes or uint_least_N
- Loop unrolling: suggest to compiler or not
- Lookup tables: prefer or avoid
- Inline depth: how aggressively to inline

**Operation Set:**
- Basic: add, subtract, multiply, divide
- Extended: sqrt, fused multiply-add, remainder
- Transcendentals: sin, cos, log, exp (future)
- Conversions: between formats, to/from integers/strings
- Comparisons: ordered, unordered, NaN handling

**Implementation Source:**
- Pure C++: template-generated generic code
- Inline assembly: embedded in C++ functions
- External assembly: separate .s files
- ROM routines: platform firmware calls
- Hardware: native instructions
- External library: link to existing implementations

**Testing Strategy:**
- Exhaustive: test all input combinations
- Edge-plus-sampling: boundaries plus random
- Property-based: algebraic laws only
- Targeted: hand-picked difficult cases
- Sample count: how many random cases

**These dimensions are mostly independent.** Changing rounding mode doesn't affect error reporting. Changing format size doesn't affect special value handling. This orthogonality enables flexible configuration.

**Some interactions exist:** Rounding modes other than "toward zero" require guard bits. Denormal support requires normalization handling. The library validates policy combinations at compile time and produces clear error messages for invalid combinations.

### 5. Example Configurations

Real-world configurations demonstrate how policies compose:

**IEEE 754 Binary32 (Standard Float):**
- Format: 1 sign + 8 exponent + 23 mantissa bits
- Specials: All (NaN, Inf, signed zeros, denormals)
- Rounding: To nearest, ties to even
- Guard bits: 3 (G+R+S)
- Errors: Track all conditions, report via local flags
- Use case: Maximum compatibility and correctness

**Minimal 6502 (Fast Game Math):**
- Format: 1 + 8 + 23 (same storage as binary32)
- Specials: None
- Rounding: Toward zero (truncate)
- Guard bits: 0
- Errors: None
- Result: ~800 bytes code, very fast, approximate

**Accurate 6502 (Quality Game Math):**
- Format: 1 + 8 + 23
- Specials: Infinity only (for overflow detection)
- Rounding: To nearest
- Guard bits: 8 (full byte, easier on 6502)
- Errors: None
- Result: ~1.5KB code, correct rounding, reliable

**FP8 E5M2 (ML Wide Range):**
- Format: 1 + 5 + 2
- Specials: NaN, Infinity (no denormals)
- Rounding: To nearest
- Guard bits: 3
- Use case: ML activations needing large dynamic range

**FP8 E4M3 (ML Better Precision):**
- Format: 1 + 4 + 3
- Specials: NaN only (no Infinity, E4M3 convention)
- Rounding: To nearest
- Guard bits: 3
- Use case: ML weights needing better precision

**Apple II ROM Routines:**
- Format: 1 + 8 + 23 (Wozniak's format)
- Implementation: Calls to $E7BE (add), $E97F (multiply), etc.
- Result: Near-zero code size, fast, matches BASIC behavior

**NVIDIA H100 MXFP8:**
- Element format: 1 + 4 + 3 (E4M3)
- Block size: 32 elements
- Scale: E8M0 (power of 2)
- Implementation: Native hardware instructions
- Use case: Production ML inference

Each configuration represents different points in the precision/performance/code-size tradeoff space. The library provides sensible presets while allowing full customization.

## Part III: Using The Library

### 6. Basic Operations

**Creating float types:**

Users select a configuration (preset or custom) and the library generates the corresponding type. The interface is minimal:

```cpp
FloatEngine<ConfigName>
```

Where ConfigName specifies all policies. For common cases, preset configurations are available.

**Arithmetic operations:**

Standard operators work as expected. The actual implementation depends on the policies configured:

```cpp
a + b    // Addition
a - b    // Subtraction  
a * b    // Multiplication
a / b    // Division
```

Behind the scenes, the library dispatches to the appropriate implementation (generic C++, platform assembly, hardware instruction) based on configuration.

**Special operations:**

Additional operations available based on configuration:

```cpp
sqrt(a)      // If operation set includes sqrt
fma(a, b, c) // Fused multiply-add: (a * b) + c
abs(a)       // Absolute value
sign(a)      // Sign extraction
```

Operations not included in the configured operation set produce compile-time errors.

**Comparison and testing:**

```cpp
a < b, a <= b, a > b, a >= b, a == b, a != b
is_nan(a), is_inf(a), is_finite(a)
```

Comparison semantics depend on special value policies. If NaN is disabled, is_nan() always returns false at compile time.

### 7. Format Conversion

**The conversion problem:**

ML training uses FP32, inference uses FP8. Legacy code uses Microsoft BASIC format, modern code uses IEEE 754. Researchers experiment with E4M3 vs E5M2 vs E3M4. Format conversion is essential.

**Conversion challenges:**
- Range differences: source max value exceeds destination max
- Precision differences: 23-bit mantissa → 3-bit mantissa requires rounding
- Special value mapping: source has Inf, destination doesn't
- Signed zero handling: FNUZ formats have only one zero

**User interface:**

Conversions should be implicit when safe, explicit when needed:

```cpp
FP8 dest = fp32_source;           // Implicit conversion
dest = convert<FP8>(fp32_source); // Explicit conversion
```

The library handles all format details automatically.

**Conversion policies:**

Behavior during conversion is configurable:

- **Range handling**: What to do on overflow (saturate, error, infinity) and underflow (flush to zero, denormal, error)
- **Precision handling**: Which rounding mode to use when narrowing mantissa
- **Special value mapping**: How to map NaN, Inf, denormals when destination doesn't support them
- **Error reporting**: Silent, exceptions, status flags, or compile-time errors

**Preset conversion policies:**

Most users don't configure conversion details. Presets cover common cases:

- **SafeConversion** (default): Saturate on overflow, flush on underflow, preserve special values when possible, round to nearest
- **FastConversion**: Saturate on overflow, truncate mantissa, skip special value checks
- **StrictConversion**: Error on any potential data loss (overflow, underflow, precision loss)
- **MLInferenceConversion**: Optimized for quantizing training weights to inference formats

**Conversion implementation:**

The conversion algorithm:
1. Unpack source format
2. Handle special values (NaN, Inf, zero)
3. Adjust exponent bias
4. Check for range overflow/underflow
5. Convert mantissa (widen by shifting, narrow by rounding)
6. Handle mantissa overflow from rounding
7. Pack destination format

Each step uses policy configuration to determine behavior. Platform-specific conversions can be provided (e.g., FP8 ↔ FP8 via lookup tables).

### 8. Microscaling (MX) Formats

**What MX formats are:**

Microscaling addresses a limitation of simple per-tensor quantization. With one scale factor per tensor, all values must fit in the format's range. This forces use of low-precision but wide-range formats (E5M2) even when higher-precision formats (E4M3) would be better.

MX assigns a different scale factor to each block of values (typically 32). This dramatically improves dynamic range, allowing use of higher-precision formats.

**Structure:**
- Block of k values (typically 32)
- Each value: element format (FP8 E4M3, FP6, FP4, or INT8)
- Shared scale: E8M0 format (pure power of 2)

Effective value: vᵢ = scale × elementᵢ

**Why it matters:**

NVIDIA H100 GPUs have native MXFP8 hardware support. Production ML systems use MX formats for inference. The technique is proven and deployed.

**Standard MX formats:**
- MXFP8 (E4M3 or E5M2 elements)
- MXFP6 (E3M2 or E2M3 elements)
- MXFP4 (E2M1 elements)
- MXINT8 (8-bit integers)

All use 32-element blocks with E8M0 scales.

**User interface:**

MX formats are containers, not numeric types:

```cpp
MicroscaledArray<MXFP8_E4M3> data = quantize(float_array);
float value = data.get(index);  // Decompresses on access
```

Arithmetic happens after decompression to float or half-float, then results can be re-quantized.

**Quantization process:**
1. Divide array into blocks of 32
2. For each block, find optimal scale (max absolute value, RMS, or percentile)
3. Convert each element using that scale
4. Store scale (8 bits) and elements (8, 6, or 4 bits each)

**Integration with library:**

MX formats leverage existing infrastructure:
- Element types (E4M3, E5M2) are regular FloatEngine types
- Scale type (E8M0) is a regular FloatEngine type  
- Quantization uses existing conversion system
- Only new code: scale finding and block management

This demonstrates the design's flexibility - production formats integrate cleanly without special cases.

## Part IV: Architecture

### 9. How Policies Work

**Policy groups:**

Ten individual policies would be overwhelming. Related policies group into bundles:

- **Format group**: Bit allocation, bias, implicit bit, sign representation
- **Compliance group**: Special values, handling modes, errors
- **Arithmetic group**: Rounding, guard bits, normalization
- **Platform group**: Hardware capabilities, code generation hints

Complete configurations combine groups:

```cpp
StandardTraits<FormatGroup, ComplianceGroup, ArithmeticGroup, PlatformGroup>
```

**Trait bundles:**

Users typically use named bundles rather than specifying all policies:

```cpp
IEEE754_Binary32    // Full compliance, all features
C64_Fast           // Minimal features, maximum speed
C64_Accurate       // Correct rounding, reasonable size
```

Bundles can be customized by inheriting and overriding specific policies.

**Type selection:**

A critical optimization: the library selects the smallest integer type that can hold each field.

On 6502 with 16-bit integers:
- 1-8 bit fields → uint8_t (8-bit operations, fast)
- 9-16 bit fields → uint16_t (16-bit operations, slower)
- 17-24 bit fields → uint24_t via BitInt (custom, slower still)
- 25-32 bit fields → uint32_t (32-bit operations, very slow)

For FP8 E5M2:
- Storage: uint8_t (8 bits total)
- Exponent operations: uint8_t (5 bits fits)
- Mantissa operations: uint8_t (2+3 guard = 5 bits fits)

All arithmetic stays in fast 8-bit operations. The compiler generates optimal code because it knows exact sizes.

Here's how the library achieves this:

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

// Format policy uses this to choose storage types
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
    
    // Computation types: sized for intermediate values with guard bits
    using mantissa_compute_type = 
        typename SelectIntType<M + guard_bits>::type;
    using exponent_compute_type = 
        typename SelectIntType<E + 2>::type;  // Extra for over/underflow
};
```

The key insight: the compiler can optimize operations on uint8_t much better than operations on a generic "small int" type. By knowing exact sizes at compile time, we enable the best code generation.

**Template architecture:**

The library uses layered templates:

1. **Public interface**: User-visible operations (add, multiply, etc.)
2. **Policy extraction**: Unpacks trait bundles into individual policies
3. **Implementation selection**: Chooses generic C++, assembly, or hardware based on configuration
4. **Operation implementation**: Actual arithmetic using policies

This separation enables compile-time optimization while maintaining clean user interface.

**Compile-time validation:**

Invalid policy combinations fail at compile time with clear messages:
- "Rounding mode ToNearest requires guard bits > 0"
- "Denormal support requires normalization"
- "Platform MOS6502 does not support hardware multiply"

Users discover configuration errors during development, not at runtime.

### 10. Pluggable Implementations

**The implementation policy:**

Like other aspects, where operation code comes from is configurable:

- **Generic**: Template-generated C++ works everywhere
- **Platform-specific**: Hand-optimized assembly for performance-critical operations
- **ROM calls**: Leverage existing firmware implementations
- **Hardware**: Native instructions when available

**How it works:**

Each operation dispatches through an implementation policy. The library structure looks like this:

```cpp
template<typename Traits>
class FloatEngine {
public:
    static storage_type multiply(storage_type a, storage_type b) {
        // Dispatch to selected implementation
        return Traits::implementation::multiply(a, b);
    }
};
```

The implementation policy provides the actual multiply function. Users can provide their own:

```cpp
// Default implementation: pure C++
template<typename Traits>
struct DefaultOps {
    static storage_type add(storage_type a, storage_type b) {
        // Full C++ implementation using policies
        auto [a_sign, a_exp, a_mant] = unpack<Traits>(a);
        auto [b_sign, b_exp, b_mant] = unpack<Traits>(b);
        
        // Handle special values if enabled
        if constexpr (Traits::specials::has_nan) {
            if (is_nan(a_exp, a_mant)) return a;
            if (is_nan(b_exp, b_mant)) return b;
        }
        
        // Align exponents, add mantissas, normalize, round
        // ... implementation details ...
        
        return pack<Traits>(sign, exp, mant);
    }
    
    static storage_type multiply(storage_type a, storage_type b) {
        // Full C++ implementation
        // ...
    }
};

// Platform override: use hand-optimized assembly for specific operations
template<typename Traits>
struct MOS6502_FastOps {
    // Use C++ for most operations
    static storage_type add(storage_type a, storage_type b) {
        return DefaultOps<Traits>::add(a, b);
    }
    
    // Override multiply with assembly
    static storage_type multiply(storage_type a, storage_type b) {
        extern storage_type float_mul_asm(storage_type, storage_type);
        return float_mul_asm(a, b);
    }
};
```

**Platform examples:**

Apple II using Wozniak's ROM routines:
- JSR $E7BE for FADD (floating add)
- JSR $E97F for FMUL (floating multiply)
- JSR $EA66 for FDIV (floating divide)
- Result: ~50 bytes wrapper code, rest is in ROM

6502 with lookup tables for FP8 E5M2:

With 2-bit mantissa + implicit bit = 3-bit values (0-7), the multiply table is 3+3=6 bits = 64 entries:

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

// Usage in multiply operation:
// Extract 3-bit mantissas (with implicit bit)
// Compute index: (mantissa_a << 3) | mantissa_b
// Result: multiply_3x3_table[index]
```

In 6502 assembly, the table lookup takes approximately 18 cycles compared to ~80 cycles for a generic 8×8 software multiply - a 4.4× speedup. This table is generated automatically by the library based on the format's mantissa bit width.

The general algorithm for automatic lookup table strategy selection:

```cpp
template<typename Traits, int BitsA, int BitsB>
auto multiply_integers(uint_a, uint_b) {
    constexpr int total_bits = BitsA + BitsB;
    
    if constexpr (Traits::hardware::has_multiply) {
        // Hardware multiply available: use it
        return uint_a * uint_b;
        
    } else if constexpr (total_bits <= Traits::int_arith::max_table_bits) {
        // Small enough for lookup table
        int index = (uint_a << BitsB) | uint_b;
        return multiply_table[index];
        
    } else {
        // Too large: use software multiply
        return shift_add_multiply(uint_a, uint_b, BitsA);
    }
}
```

The policy system allows platform maintainers to configure the maximum table size (trading ROM space for speed) and the library automatically chooses the best approach.

x86 with SSE/AVX:
- Vectorize operations across multiple values
- Use native hardware float instructions
- Platform-specific implementation ~10× faster than generic

**Integration process:**

1. Write operation struct matching expected interface
2. Specialize implementation selector for your platform
3. Test against generic implementation to verify correctness
4. Build system includes platform-specific files

No core library changes required. Platform maintainers contribute optimizations independently.

### 11. Testing Strategy

**The testing challenge:**

With many formats and many policy combinations, exhaustive manual testing is impossible. The solution combines automatic enumeration with format-specific strategies.

**Exhaustive testing:**

Small formats enable complete testing:
- FP8: 256 values, 256×256 = 65,536 input pairs per operation
- Test all operations: ~260,000 total tests, runs in seconds
- Result: Mathematical proof of correctness

FP16 is tractable (4.3 billion cases, takes hours). Larger formats require sampling.

**Policy enumeration:**

Template metaprogramming generates all valid policy combinations automatically:

```cpp
CartesianProduct<
    AllFormats,
    AllRoundingModes,
    AllSpecialValues,
    AllGuardBits
>
```

Each combination instantiates test code at compile time. Invalid combinations filtered out via compile-time validation.

**Testing as a policy:**

Test strategy is itself a policy dimension:
- Format size ≤ 8 bits: Exhaustive
- Format size ≤ 16 bits: Edge cases plus sampling
- Format size > 16 bits: Targeted plus random sampling

The library selects appropriate testing automatically based on format.

**Cross-validation:**

Multiple strategies ensure correctness:
- Fast configuration tested against accurate configuration
- Accurate tested against IEEE compliant
- Platform assembly tested against generic C++
- All tests must agree

**Property testing:**

Regardless of policies, certain properties must hold:
- Commutativity: a + b = b + a
- Associativity: (a + b) + c ≈ a + (b + c) within tolerance
- Identity: a + 0 = a, a × 1 = a
- Monotonicity: if a < b then a × c < b × c (for positive c)

Properties validate that implementations are mathematically reasonable even when not bit-exact.

## Part V: Practical Guidance

### 12. Adding Platform Support

**For platform maintainers:**

Adding optimized implementations requires:

1. **Define platform traits**: Hardware capabilities, preferred integer widths, lookup table preferences
2. **Write operation structs**: Provide implementations for operations you want to optimize
3. **Specify implementation policy**: Associate your operations with your platform
4. **Validate**: Test against generic implementation, ensure bit-exact agreement
5. **Document**: Calling conventions, performance characteristics, limitations

**Calling convention considerations:**

Platform-specific code must match calling conventions:
- LLVM-MOS: First 32 bits in rc0-rc3 (zero page)
- ARM AAPCS: First 4 args in r0-r3
- x86-64 SysV: First 6 args in rdi, rsi, rdx, rcx, r8, r9

The implementation policy can enforce conventions via static_assert.

**Build integration:**

CMake or similar build systems conditionally include platform files:

```cmake
if(TARGET_PLATFORM STREQUAL "MOS6502")
    set(PLATFORM_SOURCES platforms/mos6502/float_ops.s)
    enable_language(ASM)
endif()
```

Platform code is isolated in separate directories, doesn't pollute core library.

### 13. Future Directions

**Transcendental functions:**

Extending to sin, cos, log, exp, pow requires:
- Range reduction strategies (policy choice)
- Approximation methods: polynomial, CORDIC, lookup tables
- Accuracy targets: 0.5 ulp, 1 ulp, 2 ulp (policy choice)
- Table size vs computation time tradeoffs

Same policy-based approach applies. Function-specific policies control tradeoffs.

**Fixed-point types:**

The same policy framework extends to fixed-point:
- Integer bits and fractional bits (analogous to exponent and mantissa)
- Rounding modes during operations
- Overflow handling: saturate, wrap, error
- Similar performance benefits on constrained platforms

**SIMD and vectorization:**

Where hardware supports parallel operations:
- Operations on arrays of floats
- Platform-specific implementations using intrinsics
- Maintain same policy-based interface
- Automatic vectorization as implementation policy choice

**Format conversion matrices:**

For N formats, automatic generation of N² conversion functions. Testing ensures conversions preserve properties (monotonicity, special values, rounding).

## Appendices

### A. Policy Reference with Code Examples

This appendix provides detailed code examples showing how policies are defined and composed.

#### A.1 Format Policies

**FormatPolicy - Core format specification:**

```cpp
template<int S, int E, int M>
struct FormatPolicy {
    static constexpr int sign_bits = S;
    static constexpr int exponent_bits = E;
    static constexpr int mantissa_bits = M;
    static constexpr int total_bits = S + E + M;
    
    // Type selection based on bit requirements
    using storage_type = typename SelectIntType<total_bits>::type;
    using mantissa_type = typename SelectIntType<M>::type;
    using exponent_type = typename SelectIntType<E>::type;
    
    // Computation types (with extra bits for intermediate values)
    template<int GuardBits>
    using mantissa_compute_type = typename SelectIntType<M + GuardBits>::type;
    
    using exponent_compute_type = typename SelectIntType<E + 2>::type;
};

// Example instantiations:
using FP8_Format = FormatPolicy<1, 5, 2>;      // 8-bit total
using FP16_Format = FormatPolicy<1, 5, 10>;    // 16-bit total
using FP32_Format = FormatPolicy<1, 8, 23>;    // 32-bit total
```

**ExponentBiasPolicy:**

```cpp
template<int BiasValue>
struct ExponentBiasPolicy {
    static constexpr int bias = BiasValue;
    
    // Special value -1 means "use standard bias"
    // Standard bias = 2^(exponent_bits - 1) - 1
};

// Standard bias (auto-calculated)
using StandardBias = ExponentBiasPolicy<-1>;

// Custom bias
using CustomBias = ExponentBiasPolicy<15>;
```

**ImplicitBitPolicy:**

```cpp
template<bool HasImplicitBit>
struct ImplicitBitPolicy {
    static constexpr bool has_implicit_bit = HasImplicitBit;
    
    // IEEE 754 formats have implicit leading 1
    // Some historical formats store mantissa explicitly
};

using IEEE_ImplicitBit = ImplicitBitPolicy<true>;
using Explicit_ImplicitBit = ImplicitBitPolicy<false>;
```

#### A.2 Special Value Policies

**SpecialValuePolicy - Which special values exist:**

```cpp
template<bool NaN, bool Inf, bool SignedZero, bool Denormals>
struct SpecialValuePolicy {
    static constexpr bool has_nan = NaN;
    static constexpr bool has_infinity = Inf;
    static constexpr bool has_signed_zero = SignedZero;
    static constexpr bool has_denormals = Denormals;
};

// IEEE 754 full compliance
using IEEE_Specials = SpecialValuePolicy<true, true, true, true>;

// Fast path: no special values
using NoSpecials = SpecialValuePolicy<false, false, false, false>;

// ML formats: NaN but no Inf/denormals
using ML_E4M3_Specials = SpecialValuePolicy<true, false, true, false>;
```

**SpecialHandlingPolicy - What to do with special values:**

```cpp
enum class NaNHandling { Propagate, FlushToMax, Error };
enum class InfHandling { IEEE754, Saturate, Error };
enum class DenormalMode { FullSupport, FTZ, FIZ, FOZ, None };

template<
    NaNHandling NaN_Mode,
    InfHandling Inf_Mode, 
    DenormalMode Denorm_Mode
>
struct SpecialHandlingPolicy {
    static constexpr NaNHandling nan_handling = NaN_Mode;
    static constexpr InfHandling inf_handling = Inf_Mode;
    static constexpr DenormalMode denormal_mode = Denorm_Mode;
};

// IEEE 754 compliant
using IEEE_Handling = SpecialHandlingPolicy<
    NaNHandling::Propagate,
    InfHandling::IEEE754,
    DenormalMode::FullSupport
>;

// Fast mode
using FastHandling = SpecialHandlingPolicy<
    NaNHandling::FlushToMax,
    InfHandling::Saturate,
    DenormalMode::FTZ
>;
```

#### A.3 Arithmetic Policies

**RoundingPolicy:**

```cpp
enum class RoundingMode {
    ToNearest_TiesToEven,      // IEEE 754 default
    ToNearest_TiesAwayFromZero,
    TowardZero,                 // Truncate
    TowardPositive,             // Ceiling
    TowardNegative              // Floor
};

template<RoundingMode Mode>
struct RoundingPolicy {
    static constexpr RoundingMode mode = Mode;
    static constexpr bool needs_guard_bits = (Mode != RoundingMode::TowardZero);
};

// Common aliases
using RoundToNearest = RoundingPolicy<RoundingMode::ToNearest_TiesToEven>;
using RoundTowardZero = RoundingPolicy<RoundingMode::TowardZero>;
```

**GuardBitsPolicy:**

```cpp
template<int N>
struct GuardBitsPolicy {
    static constexpr int guard_bits = N;
    
    // IEEE 754 requires 3 guard bits (Guard, Round, Sticky)
    // Some platforms prefer full byte (8 bits) for efficiency
    // High-precision algorithms may use 16 or more
};

using NoGuardBits = GuardBitsPolicy<0>;
using IEEE_GuardBits = GuardBitsPolicy<3>;
using ByteGuardBits = GuardBitsPolicy<8>;
```

**NormalizationPolicy:**

```cpp
enum class NormalizationTiming { Always, Lazy, Never };

template<NormalizationTiming Timing>
struct NormalizationPolicy {
    static constexpr NormalizationTiming timing = Timing;
    static constexpr bool normalize_on_pack = (Timing != NormalizationTiming::Never);
    static constexpr bool normalize_during_ops = (Timing == NormalizationTiming::Always);
};
```

#### A.4 Error Policies

**ErrorDetectionPolicy:**

```cpp
template<
    bool TrackInvalid,
    bool TrackDivByZero,
    bool TrackOverflow,
    bool TrackUnderflow,
    bool TrackInexact
>
struct ErrorDetectionPolicy {
    static constexpr bool track_invalid = TrackInvalid;
    static constexpr bool track_div_by_zero = TrackDivByZero;
    static constexpr bool track_overflow = TrackOverflow;
    static constexpr bool track_underflow = TrackUnderflow;
    static constexpr bool track_inexact = TrackInexact;
    
    static constexpr bool tracks_any_error = 
        TrackInvalid || TrackDivByZero || TrackOverflow || 
        TrackUnderflow || TrackInexact;
};

// No error tracking
using NoErrorDetection = ErrorDetectionPolicy<false, false, false, false, false>;

// IEEE 754 full tracking
using IEEE_ErrorDetection = ErrorDetectionPolicy<true, true, true, true, true>;

// Critical errors only
using CriticalErrorDetection = ErrorDetectionPolicy<true, true, false, false, false>;
```

**ErrorReportingPolicy:**

```cpp
enum class ErrorReportingMode {
    None,           // No reporting
    Errno,          // Set errno (not thread-safe)
    Exception,      // Throw C++ exception
    LocalFlags,     // Return status with result
    GlobalFlags,    // Thread-local status register
    Callback        // Call registered handler
};

template<ErrorReportingMode Mode>
struct ErrorReportingPolicy {
    static constexpr ErrorReportingMode mode = Mode;
    
    // For LocalFlags mode: result type becomes pair<value, status>
    // For Exception mode: requires exception support
    // For GlobalFlags mode: uses thread_local storage
};
```

#### A.5 Platform Policies

**HardwareCapabilitiesPolicy:**

```cpp
template<
    bool HasMultiply,
    bool HasDivide,
    bool HasBarrelShift,
    bool HasCLZ,        // Count leading zeros
    bool HasSIMD
>
struct HardwareCapabilitiesPolicy {
    static constexpr bool has_multiply = HasMultiply;
    static constexpr bool has_divide = HasDivide;
    static constexpr bool has_barrel_shift = HasBarrelShift;
    static constexpr bool has_clz = HasCLZ;
    static constexpr bool has_simd = HasSIMD;
};

// MOS 6502
using MOS6502_Hardware = HardwareCapabilitiesPolicy<
    false,  // No multiply
    false,  // No divide
    false,  // No barrel shift
    false,  // No CLZ
    false   // No SIMD
>;

// ARM Cortex-M4
using CortexM4_Hardware = HardwareCapabilitiesPolicy<
    true,   // 1-cycle multiply
    true,   // Hardware divide
    true,   // Barrel shifter
    true,   // CLZ instruction
    false   // No SIMD (unless FPU)
>;

// x86-64
using x86_64_Hardware = HardwareCapabilitiesPolicy<
    true,   // Fast multiply
    true,   // Fast divide
    true,   // Barrel shift
    true,   // BSR/BSF (CLZ/CTZ)
    true    // SSE/AVX
>;
```

**IntegerArithmeticPolicy:**

```cpp
enum class IntMultiplyStrategy {
    Hardware,
    LookupTable,
    ShiftAndAdd,
    Booth,
    Hybrid              // Choose based on operand size
};

enum class IntDivideStrategy {
    Hardware,
    ReciprocalLookup,
    ShiftAndSubtract,
    Newton,
    Goldschmidt
};

template<
    IntMultiplyStrategy MultStrat,
    IntDivideStrategy DivStrat,
    int MaxTableBits = 10
>
struct IntegerArithmeticPolicy {
    static constexpr IntMultiplyStrategy multiply_strategy = MultStrat;
    static constexpr IntDivideStrategy divide_strategy = DivStrat;
    static constexpr int max_table_bits = MaxTableBits;
    
    // For lookup tables: 2^max_table_bits entries maximum
    // Typical: 10 bits = 1024 entries = 1KB table
};

// 6502: prefer lookup tables
using MOS6502_IntArith = IntegerArithmeticPolicy<
    IntMultiplyStrategy::LookupTable,
    IntDivideStrategy::ShiftAndSubtract,
    8  // 256 bytes max
>;

// Modern CPU: use hardware
using Modern_IntArith = IntegerArithmeticPolicy<
    IntMultiplyStrategy::Hardware,
    IntDivideStrategy::Hardware,
    0  // No tables needed
>;
```

#### A.6 Template Architecture Examples

**Type Selection:**

```cpp
// Core type selection template
template<int N>
struct SelectIntType {
    using type = 
        std::conditional_t<(N <= 8), uint_least8_t,
        std::conditional_t<(N <= 16), uint_least16_t,
        std::conditional_t<(N <= 24), unsigned _BitInt(24),
        std::conditional_t<(N <= 32), uint_least32_t,
        uint_least64_t>>>>;
};

// Usage in FormatPolicy shown above demonstrates:
// - storage_type sized for total bits
// - Field types sized for individual fields  
// - Compute types sized for intermediate values with guard bits

// Example: FP8 E5M2 with 3 guard bits
using FP8 = FormatPolicy<1, 5, 2>;
// storage_type = uint8_t (8 bits total)
// mantissa_type = uint8_t (2 bits)
// exponent_type = uint8_t (5 bits)
// mantissa_compute_type<3> = uint8_t (2 + 3 = 5 bits)
// exponent_compute_type = uint8_t (5 + 2 = 7 bits)
// All operations use fast 8-bit arithmetic
```

**Trait Composition:**

```cpp
// Individual policies combine into trait bundles
template<
    typename FormatPolicy,
    typename SpecialValuePolicy,
    typename SpecialHandlingPolicy,
    typename RoundingPolicy,
    typename GuardBitsPolicy,
    typename NormalizationPolicy,
    typename ErrorDetectionPolicy,
    typename ErrorReportingPolicy,
    typename HardwarePolicy,
    typename IntArithPolicy,
    typename ImplementationPolicy
>
struct FloatTraits {
    using format = FormatPolicy;
    using specials = SpecialValuePolicy;
    using special_handling = SpecialHandlingPolicy;
    using rounding = RoundingPolicy;
    using guard_bits = GuardBitsPolicy;
    using normalization = NormalizationPolicy;
    using error_detection = ErrorDetectionPolicy;
    using error_reporting = ErrorReportingPolicy;
    using hardware = HardwarePolicy;
    using int_arith = IntArithPolicy;
    using implementation = ImplementationPolicy;
};

// Policy groups simplify composition
template<int S, int E, int M>
struct FormatGroup {
    using format = FormatPolicy<S, E, M>;
    using exponent_bias = ExponentBiasPolicy<-1>;
    using implicit_bit = ImplicitBitPolicy<true>;
};

struct IEEE_ComplianceGroup {
    using specials = IEEE_Specials;
    using special_handling = IEEE_Handling;
    using error_detection = IEEE_ErrorDetection;
    using error_reporting = ErrorReportingPolicy<ErrorReportingMode::LocalFlags>;
};

struct FastArithmeticGroup {
    using rounding = RoundTowardZero;
    using guard_bits = NoGuardBits;
    using normalization = NormalizationPolicy<NormalizationTiming::Lazy>;
};

template<typename HardwarePolicy>
struct PlatformGroup {
    using hardware = HardwarePolicy;
    // Auto-select int arithmetic based on hardware
    using int_arith = std::conditional_t<
        HardwarePolicy::has_multiply,
        Modern_IntArith,
        MOS6502_IntArith
    >;
};
```

**Implementation Selection:**

```cpp
// Default implementation uses pure C++
template<typename Traits>
struct DefaultOps {
    using storage_type = typename Traits::format::storage_type;
    
    static storage_type add(storage_type a, storage_type b) {
        // Unpack operands
        auto [a_sign, a_exp, a_mant] = unpack<Traits>(a);
        auto [b_sign, b_exp, b_mant] = unpack<Traits>(b);
        
        // Handle special values if enabled
        if constexpr (Traits::specials::has_nan) {
            if (is_nan(a_exp, a_mant)) return a;
            if (is_nan(b_exp, b_mant)) return b;
        }
        
        // Align exponents, add mantissas, normalize, round
        // ... full implementation using policies ...
        
        return pack<Traits>(result_sign, result_exp, result_mant);
    }
    
    static storage_type multiply(storage_type a, storage_type b);
    static storage_type divide(storage_type a, storage_type b);
    // ... other operations
};

// Platform-specific override
template<typename Traits>
struct MOS6502_OptimizedOps : DefaultOps<Traits> {
    using Base = DefaultOps<Traits>;
    using storage_type = typename Traits::format::storage_type;
    
    // Use base class for most operations
    using Base::add;
    using Base::divide;
    
    // Override multiply with hand-optimized assembly
    static storage_type multiply(storage_type a, storage_type b) {
        extern storage_type float_mul_asm(storage_type, storage_type);
        return float_mul_asm(a, b);
    }
};

// Implementation selector
template<typename Traits>
struct SelectImplementation {
    using type = DefaultOps<Traits>;
};

// Specializations for specific platforms
template<>
struct SelectImplementation<C64_Fast_Traits> {
    using type = MOS6502_OptimizedOps<C64_Fast_Traits>;
};

// FloatEngine uses selected implementation
template<typename Traits>
class FloatEngine {
private:
    using Impl = typename SelectImplementation<Traits>::type;
    
public:
    using storage_type = typename Traits::format::storage_type;
    
    static storage_type add(storage_type a, storage_type b) {
        return Impl::add(a, b);
    }
    
    static storage_type multiply(storage_type a, storage_type b) {
        return Impl::multiply(a, b);
    }
};
```

**Pack/Unpack Operations:**

```cpp
// Unpacking extracts fields from storage
template<typename Traits>
struct UnpackedFloat {
    using exponent_type = typename Traits::format::exponent_type;
    using mantissa_type = typename Traits::format::mantissa_type;
    
    bool sign;
    exponent_type exponent;
    mantissa_type mantissa;
    
    // Special value indicators
    bool is_zero;
    bool is_nan;
    bool is_inf;
    bool is_denormal;
};

template<typename Traits>
UnpackedFloat<Traits> unpack(typename Traits::format::storage_type value) {
    using Format = typename Traits::format;
    
    UnpackedFloat<Traits> result;
    
    // Extract bit fields
    constexpr int mantissa_shift = 0;
    constexpr int exponent_shift = Format::mantissa_bits;
    constexpr int sign_shift = Format::mantissa_bits + Format::exponent_bits;
    
    constexpr typename Format::mantissa_type mantissa_mask = 
        (1u << Format::mantissa_bits) - 1;
    constexpr typename Format::exponent_type exponent_mask = 
        (1u << Format::exponent_bits) - 1;
    
    result.mantissa = (value >> mantissa_shift) & mantissa_mask;
    result.exponent = (value >> exponent_shift) & exponent_mask;
    result.sign = (value >> sign_shift) & 1;
    
    // Detect special values based on policy
    constexpr int exp_max = (1 << Format::exponent_bits) - 1;
    
    result.is_zero = (result.exponent == 0 && result.mantissa == 0);
    
    if constexpr (Traits::specials::has_nan || Traits::specials::has_infinity) {
        bool exp_all_ones = (result.exponent == exp_max);
        
        if constexpr (Traits::specials::has_nan) {
            result.is_nan = exp_all_ones && (result.mantissa != 0);
        }
        
        if constexpr (Traits::specials::has_infinity) {
            result.is_inf = exp_all_ones && (result.mantissa == 0);
        }
    }
    
    if constexpr (Traits::specials::has_denormals) {
        result.is_denormal = (result.exponent == 0 && result.mantissa != 0);
    }
    
    return result;
}

// Packing assembles storage from fields
template<typename Traits>
typename Traits::format::storage_type pack(
    bool sign,
    typename Traits::format::exponent_type exponent,
    typename Traits::format::mantissa_type mantissa
) {
    using Format = typename Traits::format;
    using storage_type = typename Format::storage_type;
    
    storage_type result = 0;
    
    result |= (mantissa & ((1u << Format::mantissa_bits) - 1));
    result |= (static_cast<storage_type>(exponent & 
               ((1u << Format::exponent_bits) - 1)) << Format::mantissa_bits);
    result |= (static_cast<storage_type>(sign) << 
               (Format::mantissa_bits + Format::exponent_bits));
    
    return result;
}
```

#### A.7 Conversion Policy Examples

**Conversion Policy Bundle:**

```cpp
template<
    typename RangePolicy,
    typename SpecialMappingPolicy,
    typename PrecisionPolicy,
    typename ErrorPolicy
>
struct ConversionPolicyBundle {
    using range = RangePolicy;
    using specials = SpecialMappingPolicy;
    using precision = PrecisionPolicy;
    using errors = ErrorPolicy;
};

// Safe conversion (default)
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
        RoundToNearest,
        IEEE_GuardBits
    >;
    using errors = ConversionErrorPolicy<
        ConversionErrorMode::Silent
    >;
};

// Strict conversion: error on any loss
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
        RoundToNearest,
        IEEE_GuardBits
    >;
    using errors = ConversionErrorPolicy<
        ConversionErrorMode::Exception
    >;
};
```

**Conversion Function Signature:**

```cpp
// Generic conversion
template<
    typename DstTraits,
    typename SrcTraits,
    typename ConversionPolicy = SafeConversionPolicy
>
typename DstTraits::format::storage_type convert(
    typename SrcTraits::format::storage_type src
);

// Usage:
FloatEngine<FP32_Traits> src_value = ...;
auto dst_value = convert<FP8_E4M3_Traits>(src_value);
```

#### A.8 Microscaling Policy Examples

**MX Format Definition:**

```cpp
template<
    typename ElementTraits,
    typename ScaleTraits,
    int BlockSize
>
struct MicroscalingPolicy {
    using element_traits = ElementTraits;
    using scale_traits = ScaleTraits;
    static constexpr int block_size = BlockSize;
    
    static constexpr int element_bits = ElementTraits::format::total_bits;
    static constexpr int scale_bits = ScaleTraits::format::total_bits;
    static constexpr int bits_per_block = scale_bits + (block_size * element_bits);
    
    enum class ScaleStrategy {
        MaxAbsValue,
        RMS,
        Percentile99
    };
    static constexpr ScaleStrategy scale_strategy = ScaleStrategy::MaxAbsValue;
};

// MXFP8 E4M3 definition
using MXFP8_E4M3 = MicroscalingPolicy<
    E4M3_Traits,
    E8M0_Traits,
    32
>;
```

**Microscaling Container Interface:**

```cpp
template<typename MicroscalingPolicy>
class MicroscaledArray {
private:
    struct Block {
        FloatEngine<typename MicroscalingPolicy::scale_traits> scale;
        std::array<
            FloatEngine<typename MicroscalingPolicy::element_traits>,
            MicroscalingPolicy::block_size
        > elements;
    };
    
    std::vector<Block> blocks_;
    size_t size_;
    
public:
    MicroscaledArray(size_t size) : size_(size) {
        blocks_.resize((size + MicroscalingPolicy::block_size - 1) / 
                       MicroscalingPolicy::block_size);
    }
    
    // Dequantize: scale × element
    float get(size_t index) const {
        size_t block_idx = index / MicroscalingPolicy::block_size;
        size_t elem_idx = index % MicroscalingPolicy::block_size;
        
        auto& block = blocks_[block_idx];
        
        if (block.scale.is_nan()) {
            return std::numeric_limits<float>::quiet_NaN();
        }
        
        auto elem = block.elements[elem_idx];
        
        if (elem.is_special()) {
            return elem.to_float();
        }
        
        return block.scale.to_float() * elem.to_float();
    }
    
    // Quantize block from float array
    void set_block(size_t block_idx, std::span<const float> values) {
        assert(values.size() == MicroscalingPolicy::block_size);
        
        // Find scale factor based on policy strategy
        float max_abs = 0.0f;
        for (float v : values) {
            max_abs = std::max(max_abs, std::abs(v));
        }
        
        // Convert to scale format (e.g., E8M0)
        blocks_[block_idx].scale = /* compute scale from max_abs */;
        
        // Quantize elements
        for (size_t i = 0; i < values.size(); ++i) {
            blocks_[block_idx].elements[i] = 
                convert<typename MicroscalingPolicy::element_traits>(
                    values[i] / blocks_[block_idx].scale.to_float()
                );
        }
    }
};
```

#### A.9 Compile-Time Validation

**Policy Constraint Checking:**

```cpp
template<typename Traits>
struct ValidateTraits {
    using Format = typename Traits::format;
    using Rounding = typename Traits::rounding;
    using GuardBits = typename Traits::guard_bits;
    using Specials = typename Traits::specials;
    using Normalization = typename Traits::normalization;
    
    // Rounding modes other than TowardZero require guard bits
    static_assert(
        !Rounding::needs_guard_bits || GuardBits::guard_bits > 0,
        "Rounding modes other than TowardZero require guard bits > 0"
    );
    
    // Denormal support requires normalization
    static_assert(
        !Specials::has_denormals || Normalization::normalize_on_pack,
        "Denormal support requires normalization on pack"
    );
    
    // Sign must be exactly 1 bit
    static_assert(
        Format::sign_bits == 1,
        "Sign must be exactly 1 bit"
    );
    
    // Reasonable exponent range
    static_assert(
        Format::exponent_bits >= 2 && Format::exponent_bits <= 15,
        "Exponent must be between 2 and 15 bits"
    );
    
    // Reasonable mantissa range
    static_assert(
        Format::mantissa_bits >= 1 && Format::mantissa_bits <= 112,
        "Mantissa must be between 1 and 112 bits"
    );
    
    // Total size reasonable
    static_assert(
        Format::total_bits <= 128,
        "Total format size must not exceed 128 bits"
    );
};

// Instantiating FloatEngine triggers validation
template<typename Traits>
class FloatEngine {
    ValidateTraits<Traits> _validate;  // Triggers static_asserts
    // ...
};
```

### B. Policy Interactions and Constraints

**Required interactions:**
- Rounding mode other than TowardZero → GuardBits > 0
- Denormal support → Normalization handles denormals
- NaN support → Exponent interpretation includes all-1s
- Error reporting enabled → Error detection enabled

**Platform constraints:**
- Format total_bits > 32 on byte-oriented platforms → warning
- Hardware multiply = false → Consider lookup tables or iterative
- SIMD available → Consider vectorized implementations

**Validation:**
All constraints checked at compile time. Invalid combinations produce static_assert failures with explanatory messages.

### C. Platform Notes

**MOS 6502:**
- 8-bit accumulator, X/Y index registers
- No hardware multiply or divide (200-300 cycles in software)
- 256-byte zero page (fast access)
- LLVM-MOS uses zero page as "imaginary registers" rc0-rc31
- Prefer: byte operations, lookup tables, guard byte not bits

**ARM Cortex-M:**
- M0/M0+: No hardware divide, 1-cycle multiply
- M3/M4: Hardware divide, optional single-precision FPU
- M7: Hardware FPU with double precision
- Thumb-2 instruction set, efficient 32-bit operations
- Prefer: 32-bit operations when multiply available

**x86-64:**
- Native 64-bit, hardware FPU with 80-bit internal precision
- SSE/AVX for SIMD (4-8 floats in parallel)
- Very fast multiply, divide, sqrt
- Large caches favor computation over lookup tables
- Prefer: wide operations, aggressive inlining, SIMD

**RISC-V:**
- 32-bit (RV32) or 64-bit (RV64) base integer
- Optional multiply/divide (M extension)
- Optional single/double precision FPU (F/D extensions)
- Simple calling convention, good code density
- Highly configurable, policy choices depend on extensions

### D. Format Catalog

**IEEE 754 Standard Formats:**
- Binary16 (half): 1 + 5 + 10, range ±65504
- Binary32 (single): 1 + 8 + 23, range ±3.4×10³⁸
- Binary64 (double): 1 + 11 + 52, range ±1.8×10³⁰⁸

**ML Formats:**
- FP8 E5M2: 1 + 5 + 2, wide range, low precision
- FP8 E4M3: 1 + 4 + 3, better precision, narrower range
- FP8 E4M3FNUZ: Finite numbers, unsigned zero variant
- FP8 E5M2FNUZ: Finite numbers, unsigned zero variant
- BFloat16: 1 + 8 + 7, matches FP32 range with less precision
- FP6 E3M2 / E2M3: 6-bit formats for extreme compression
- FP4 E2M1: 4-bit format for ultra-low precision

**Historical Formats:**
- Wozniak (Apple II): 1 + 8 + 23 with non-standard encoding
- Microsoft Binary Format: Used in GW-BASIC, QuickBASIC
- Cray formats: 64-bit with varying exponent sizes

**Microscaling Formats:**
- MXFP8 (E4M3 or E5M2): 32 elements, 8-bit E8M0 scale
- MXFP6 (E3M2 or E2M3): 32 elements, 8-bit E8M0 scale
- MXFP4: 32 elements, 8-bit E8M0 scale
- MXINT8: 32 8-bit integers, 8-bit E8M0 scale

All formats supported through policy configuration. Custom formats created by specifying bit allocations and policies.
