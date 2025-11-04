# OPINE

**O**ptimized **P**olicy-**I**nstantiated **N**umeric **E**ngine

A C++20 header-only library for compile-time configurable floating-point arithmetic that works everywhere—from 1 MHz 6502s to modern GPUs.

## Key Features

- **Zero Runtime Overhead**: All configuration decisions made at compile time
- **Universal Scalability**: Same design works on 8-bit microprocessors and ML accelerators
- **Format Flexibility**: Support for IEEE 754, ML formats (FP8, FP16), and custom formats
- **Policy-Based Design**: Choose exact semantics for rounding, special values, and error handling
- **Progressive Optimization**: Start with working code, optimize hot paths with assembly later
- **Cross-Compiler Support**: Works with Clang (with `_BitInt`), GCC, and MSVC

## Why OPINE?

Standard floating-point libraries assume IEEE 754 compliance, hardware support, and one-size-fits-all semantics. None of these hold universally. OPINE separates *what* (format and behavior) from *how* (implementation), generating exactly the code you need through compile-time policies.

## Quick Example

```cpp
#include <opine/opine.hpp>

using namespace opine;

// FP8 E5M2 format: 1 sign + 5 exponent + 2 mantissa bits
// Works on any platform, generates optimal code for each

void particle_update() {
  // Storage uses uint8_t
  FP8_E5M2::storage_type pos = pack<FP8_E5M2>({0, 15, 1});  // 1.0
  FP8_E5M2::storage_type vel = pack<FP8_E5M2>({0, 14, 2});  // 0.5

  // Unpack for computation
  auto pos_unpacked = unpack<FP8_E5M2>(pos);
  auto vel_unpacked = unpack<FP8_E5M2>(vel);

  // Operations happen on unpacked representation
  // (multiply/add coming soon)

  // Pack back to storage
  pos = pack<FP8_E5M2>(pos_unpacked);
}
```

## Current Status

### Implemented ✓

- **Type Selection System**: Three policies (ExactWidth, LeastWidth, Fastest) with `_BitInt` support
- **Format Descriptors**: Arbitrary bit layouts with padding support
- **Pack/Unpack Operations**: Bidirectional conversion with implicit bit handling
- **Standard Formats**: FP8 E5M2, FP8 E4M3, IEEE 754 binary16/32/64
- **Cross-Platform**: Linux, macOS, Windows with GCC, Clang, and MSVC
- **Comprehensive Tests**: Exhaustive testing for 8-bit formats

### Planned 🚧

- Arithmetic operations (add, subtract, multiply, divide)
- Rounding policies (ToNearest, TowardZero, TowardPositive, TowardNegative)
- Special value handling (NaN, Infinity, denormals)
- Conversion between formats
- Platform-specific optimizations (assembly, ROM calls, hardware instructions)
- Microscaling formats (MXFP8, MXFP6, MXFP4)

## Building and Testing

```bash
# Configure with CMake
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build build

# Run tests
cd build && ctest --output-on-failure
```

### Compiler Support

- **Clang 18+**: Full support including `_BitInt` for exact width types
- **GCC 13+**: Fallback to `uint_fast` types (no `_BitInt` support yet)
- **MSVC**: Fallback to `uint_fast` types

For best code generation (exact bit widths), use Clang.

## Documentation

Design documentation is available:

- **[Design Document](docs/design/design.md)** - Full motivation, examples, and architecture (1680 lines)
- **[Type Selection](docs/design/type_selection.md)** - How the type policy system works
- **[Pack/Unpack System](docs/design/pack_unpack.md)** - Format conversion and representation
- **[Guard/Round/Sticky Bits](docs/design/bits.md)** - Rounding implementation details

## Project Structure

```
opine/
├── include/opine/           # Header-only library
│   ├── core/               # Core types and format descriptors
│   ├── operations/         # Pack/unpack and arithmetic operations
│   └── policies/           # Type selection and rounding policies
├── tests/                  # Unit tests
├── examples/               # Usage examples
└── docs/design/            # Design documentation
```

## Philosophy

The primitive mathematical operations of addition, subtraction, multiply, and divide have a lot of flexibility in how they are implemented for floating point formats.  There are many trade-offs between accuracy and speed, depending on the target architecture.

OPINE embraces this reality. Instead of forcing one set of semantics, it lets you choose:

- **Format**: How many bits for sign, exponent, mantissa
- **Rounding**: Nearest, zero, positive, negative, or custom
- **Special values**: NaN, Infinity, denormals—enable only what you need
- **Error handling**: Silent, exceptions, status flags, or compile errors
- **Implementation**: Generic C++, assembly, ROM calls, or hardware ops

All decisions are made at compile time.  If you don't need it, then it doesn't exist.

## License

MIT License - See [LICENSE](LICENSE) for details.

## Contributing

OPINE is in active development. The core infrastructure (types, formats, pack/unpack) is stable. Arithmetic operations are next.

Contributions welcome! Please see design docs for architectural principles.
