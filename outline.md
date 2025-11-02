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

```
FloatEngine<ConfigName>
```

Where ConfigName specifies all policies. For common cases, preset configurations are available.

**Arithmetic operations:**

Standard operators work as expected. The actual implementation depends on the policies configured:

```
a + b    // Addition
a - b    // Subtraction  
a * b    // Multiplication
a / b    // Division
```

Behind the scenes, the library dispatches to the appropriate implementation (generic C++, platform assembly, hardware instruction) based on configuration.

**Special operations:**

Additional operations available based on configuration:

```
sqrt(a)      // If operation set includes sqrt
fma(a, b, c) // Fused multiply-add: (a * b) + c
abs(a)       // Absolute value
sign(a)      // Sign extraction
```

Operations not included in the configured operation set produce compile-time errors.

**Comparison and testing:**

```
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

```
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

```
MXTensor<MXFP8_E4M3> tensor = quantize(float_array);
float value = tensor.get(index);  // Decompresses on access
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

```
StandardTraits<FormatGroup, ComplianceGroup, ArithmeticGroup, PlatformGroup>
```

**Trait bundles:**

Users typically use named bundles rather than specifying all policies:

```
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

Each operation dispatches through an implementation policy:

```
template<typename Traits>
float_multiply(a, b) {
    return Traits::implementation::multiply(a, b);
}
```

The implementation policy provides the actual multiply function. Users can provide their own:

```
struct MOS6502_FastOps {
    // Use C++ for most
    static result add(a, b) { return DefaultOps::add(a, b); }
    
    // Override multiply with assembly
    static result multiply(a, b) {
        extern result float_mul_asm(a, b);
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

6502 with lookup tables:
- 3×3 bit multiply: 64-byte table, 18 cycles
- Beats iterative multiply by 4×
- Generated automatically based on format

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

```
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

```
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

### A. Policy Reference

**Format Policies:**
- FormatPolicy: Bit allocation for sign, exponent, mantissa
- ExponentBiasPolicy: Standard or custom bias
- ImplicitBitPolicy: Hidden leading bit
- SignRepresentationPolicy: Sign bit vs two's complement

**Special Value Policies:**
- SpecialValuePolicy: Which special values exist (NaN, Inf, signed zero, denormals)
- SpecialHandlingPolicy: How to handle special values when encountered
- DenormalHandlingPolicy: Full support, flush to zero, flush inputs, flush outputs

**Arithmetic Policies:**
- RoundingPolicy: To nearest, toward zero, toward positive/negative infinity
- GuardBitsPolicy: Number of extra precision bits (0, 3, 8, 16+)
- NormalizationPolicy: Always, lazy, never

**Error Policies:**
- ErrorDetectionPolicy: Which conditions trigger errors
- ErrorReportingPolicy: None, errno, exceptions, flags, callbacks
- ConversionErrorPolicy: Error handling specific to format conversions

**Platform Policies:**
- HardwareCapabilitiesPolicy: Multiply, divide, shifts, CLZ/CTZ, SIMD
- IntegerArithmeticPolicy: Multiply/divide strategies (hardware, lookup, iterative)
- CodeGenHintsPolicy: Integer width preferences, type strategies, unrolling hints

**Operation Policies:**
- OperationSetPolicy: Which operations are available
- ImplementationPolicy: Where implementation code comes from

**Testing Policies:**
- TestingPolicy: Strategy (exhaustive, sampling, targeted), sample count

**Conversion Policies:**
- RangePolicy: Overflow and underflow handling
- SpecialMappingPolicy: Mapping special values between formats
- PrecisionPolicy: Rounding and guard bits for mantissa conversion

**Microscaling Policies:**
- MicroscalingPolicy: Element format, scale format, block size
- Quantization policies: Scale selection strategy, saturation behavior

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
