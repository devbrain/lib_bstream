<div align="center">

# lib_bstream

**Binary I/O, from bytes to bits.**

A header-only C++20 library for reading and writing binary formats in memory and standard streams.

[![CI](https://github.com/devbrain/lib_bstream/actions/workflows/compilers.yml/badge.svg)](https://github.com/devbrain/lib_bstream/actions/workflows/compilers.yml)
![C++20](https://img.shields.io/badge/C%2B%2B-20-00599C?logo=cplusplus&logoColor=white)
![Header-only](https://img.shields.io/badge/header--only-library-2ea44f)
[![License: MIT](https://img.shields.io/badge/license-MIT-blue.svg)](#license)

[Quick start](#quick-start) · [Integration](#integration) · [Examples](#examples) · [Documentation](#documentation)

</div>

---

Parse file headers, encode packets, or unpack bit fields with explicit control over
byte order and storage. Use the core library with just the C++ standard library,
and enable optional dependencies for aggregate reflection and expected results.

## At a glance

| Capability | What you get |
| --- | --- |
| **Memory I/O** | Bounded `span_reader` and `span_writer`, with zero-copy slices and subreaders. |
| **Stream I/O** | Typed reads and writes over `std::istream` and `std::ostream`. |
| **Bit fields** | Read, peek, and write 0–64 bits at any bit offset, in MSB-first or LSB-first order. |
| **Byte order** | Little, big, and native endian support, plus constexpr byte swapping. |
| **Strings & containers** | Null-terminated, fixed-width, and length-prefixed strings; arrays and vectors. |
| **Aggregate serialization** | Optional Boost.PFR support: serialize fields in declaration order, without struct padding. |
| **Error handling** | Exceptions by default; optional `try_read<T>()` results through failsafe. |
| **Stream adapters** | Memory-backed standard streams, tee streams, and stream copying. |

## Quick start

Write into a fixed buffer, then read the same bytes back. This example needs no
optional dependencies.

```cpp
#include <bstream/bstream.hh>

#include <array>
#include <cassert>
#include <cstdint>

int main() {
    std::array<std::uint8_t, 64> buffer{};

    bstream::span_writer writer(buffer, bstream::endian::big);
    writer.write<std::uint32_t>(0x12345678);
    writer.write_pascal_string("hello"); // one-byte length, then string bytes

    bstream::span_reader reader(writer.written_span(), bstream::endian::big);
    const auto magic = reader.read<std::uint32_t>();
    const auto message = reader.read_pascal_string();

    assert(magic == 0x12345678);
    assert(message == "hello");
    assert(reader.eof());
}
```

Byte readers and writers default to little endian. Views returned by
`written_span()`, `read_span()`, and `subreader()` borrow the backing storage;
keep it alive and at a stable address while those views are in use.

## Integration

Requires a **C++20 compiler** and **CMake 3.20+** when using CMake. Link the
`neutrino::bstream` target to inherit the include paths, C++ standard, and enabled
dependencies.

```cmake
include(FetchContent)
FetchContent_Declare(bstream
    GIT_REPOSITORY https://github.com/devbrain/lib_bstream.git
    GIT_TAG master
)
FetchContent_MakeAvailable(bstream)

target_link_libraries(my_target PRIVATE neutrino::bstream)
```

For reproducible builds, replace `master` with a commit hash.

| Optional dependency | CMake option | Default | Adds |
| --- | --- | --- | --- |
| Boost.PFR | `BSTREAM_WITH_BOOST_PFR` | `ON` | Automatic aggregate reads and writes. |
| failsafe | `BSTREAM_WITH_FAILSAFE` | `ON` | Expected-result APIs such as `try_read<T>()`. |

Set either option to `OFF` before `FetchContent_MakeAvailable()` to skip that
dependency. Scalar and custom-operator serialization work without Boost.PFR.

<details>
<summary><strong>Install a CMake package</strong></summary>

From a checkout, install the core library without optional dependencies:

```sh
cmake -S . -B build-install -DNEUTRINO_BSTREAM_INSTALL=ON \
  -DNEUTRINO_BSTREAM_BUILD_TESTS=OFF -DNEUTRINO_BSTREAM_BUILD_EXAMPLES=OFF \
  -DBSTREAM_WITH_FAILSAFE=OFF -DBSTREAM_WITH_BOOST_PFR=OFF
cmake --install build-install --prefix /path/to/prefix
```

Set `CMAKE_PREFIX_PATH` to the installation prefix in your consuming project, then:

```cmake
find_package(bstream 0.1 CONFIG REQUIRED)
target_link_libraries(my_target PRIVATE neutrino::bstream)
```

The package includes all headers and a relocatable CMake target. It respects
`CMAKE_INSTALL_INCLUDEDIR` and `CMAKE_INSTALL_LIBDIR`.

If you install with optional dependencies enabled, provide their CMake packages
separately: `failsafe` and Boost 1.83+ with the `pfr` component. Existing
`neutrino::failsafe` and `Boost::pfr` targets are also accepted. Fetched dependencies
are not bundled into the bstream package.

</details>

<details>
<summary><strong>Use the headers directly</strong></summary>

Add `include/` to your compiler's include path and enable C++20. Include
`<bstream/bstream.hh>` for the complete API or an individual component header.

Boost.PFR and failsafe are detected when their headers are visible. Define
`BSTREAM_WITH_BOOST_PFR=0` to disable reflection even if Boost is installed.
The CMake option `BSTREAM_WITH_BOOST_PFR=OFF` sets this definition for consumers.
Disabling `BSTREAM_WITH_FAILSAFE` stops CMake from fetching and linking failsafe;
otherwise visible failsafe headers can still enable its APIs.

</details>

## Examples

### Pack bit fields

Bit readers and writers support both memory and standard streams. Bit order is
independent of byte order and defaults to MSB-first.

```cpp
bstream::bit_writer writer(bstream::bit_order::msb_first);
writer.write_bits(5, 3);       // 101
writer.write_bits(0x2ab, 10);  // 1010101011
const auto bytes = writer.finish(); // pad the final byte with zeros

bstream::bit_reader reader(bytes, bstream::bit_order::msb_first);
const auto flags = reader.read_bits(3);   // 5
const auto value = reader.read_bits(10);  // 0x2ab
```

### Serialize a struct

With Boost.PFR enabled, read and write supported aggregates field by field.
The serialized size comes from the fields, without compiler-inserted padding.

```cpp
struct Header {
    std::uint32_t magic;
    std::uint16_t version;
};

std::array<std::uint8_t, 6> buffer{};
bstream::span_writer writer(buffer, bstream::endian::big);
writer.write(Header{0x12345678, 2});

bstream::span_reader reader(writer.written_span(), bstream::endian::big);
const auto header = reader.read<Header>();
```

See the [executable examples](docs/examples/usage.cc) for zero-copy slicing,
standard streams, custom serializers, error handling, and stream adapters.

## Documentation

The [usage guide](docs/guide.dox) covers ownership, format design, configuration,
and the public interfaces. Generate the browsable API reference with **Doxygen
1.9.8+**:

```sh
cmake -S . -B build-docs -DNEUTRINO_BSTREAM_BUILD_DOCS=ON
cmake --build build-docs --target bstream_docs
```

Open `build-docs/docs/reference/html/index.html`. The build also generates XML
and treats documentation warnings as errors.

<details>
<summary><strong>Serialization details and guarantees</strong></summary>

- Prefer fixed-width integer types and an explicit byte order for file formats.
  Containers serialize their elements without an implicit count.
- Floating-point serialization supports IEEE binary32 and binary64. Extended or
  padded `long double` representations are rejected at compile time; binary64
  `long double` remains supported.
- Explicit endian template arguments apply to nested aggregate and custom fields.
  The configured byte order is restored even when an operation throws.
- Aggregate `read<T>()` can initialize const value fields, including nested records.
  Extraction into an existing object requires an assignable type. Reference
  members are unsupported by automatic aggregate deserialization.
- Pascal strings default to a `uint8_t` length prefix for both readers and writers.
  Oversized strings throw `parse_error` before writing. Use matching wider unsigned
  types, such as `write_pascal_string<uint32_t>()`, for longer strings or the
  previous `stream_writer` default format.
- Container-backed readers expose the full byte extent, including containers with
  multi-byte elements. Strings and vectors returned by decoding own their data.
- Bit reads, peeks, and writes accept 0–64 bits at any bit offset. Peeking does not
  consume bits; byte alignment discards only the remainder of the current byte.
  Changing bit order requires a byte boundary and throws `bstream_error` otherwise.
- Bit writes may commit a prefix before an output failure. Pending partial-byte
  state remains valid for flushing or destruction. `write_bits()` counts accepted
  payload bits in `total_bits_written()`, excluding alignment padding. Call
  `flush()` or `finish()` explicitly to observe output errors.
- Tee streams report destination failures through stream state. Output already
  accepted elsewhere cannot be rolled back. Tee buffers cannot be copied or moved.
- Empty memory streams support seeking to position zero. File streams should be
  opened with `std::ios::binary`.

</details>

## Build and test

The [CI workflow](.github/workflows/compilers.yml) covers GCC and Clang on Linux,
and MSVC and clang-cl with Visual Studio 2022 on Windows. Each compiler runs with
optional dependencies enabled and disabled, with warnings treated as errors.
Public headers are also compiled independently.

```sh
cmake -S . -B build -DNEUTRINO_BSTREAM_BUILD_TESTS=ON \
  -DNEUTRINO_WARNINGS_AS_ERRORS=ON
cmake --build build --config Release
ctest --test-dir build --build-config Release --output-on-failure
```

Strict warning flags are private to the verification targets. Examples build and
run with the tests.

<details>
<summary><strong>Compiler-specific builds and standalone examples</strong></summary>

For Clang's broader diagnostic audit:

```sh
cmake -S . -B build-warnings-clang -DCMAKE_CXX_COMPILER=clang++ \
  -DNEUTRINO_BSTREAM_BUILD_TESTS=ON -DNEUTRINO_WARNINGS_AS_ERRORS=ON \
  -DCMAKE_CXX_FLAGS="-Weverything -Werror -Wno-c++98-compat -Wno-c++98-compat-pedantic -Wno-padded -Wno-weak-vtables -Wno-unsafe-buffer-usage"
```

The exclusions cover C++98 compatibility, normal layout padding, inline virtual
methods, and raw pointer operations in bounded buffer and streambuf code. Boost.PFR
and doctest headers are treated as system headers.

For native MSVC:

```sh
cmake -S . -B build-msvc -G "Visual Studio 17 2022" -A x64 \
  -DNEUTRINO_BSTREAM_BUILD_TESTS=ON -DNEUTRINO_WARNINGS_AS_ERRORS=ON
cmake --build build-msvc --config Release
ctest --test-dir build-msvc --build-config Release --output-on-failure
```

Add `-T ClangCL` to the configure command to use clang-cl in a separate directory.
MSVC-family builds use `/W4`, additional diagnostics, `/permissive-`, and `/WX`.
Every library header includes Windows macro handling: it defines `NOMINMAX` and
removes existing `min` and `max` macros, including when Windows headers come first.
No project-wide compiler definition is needed.

To build the usage examples separately:

```sh
cmake -S . -B build-examples -DNEUTRINO_BSTREAM_BUILD_TESTS=OFF \
  -DNEUTRINO_BSTREAM_BUILD_EXAMPLES=ON
cmake --build build-examples --config Release --target bstream_usage_examples
```

Run `build-examples/bin/bstream_usage_examples`, or
`build-examples/bin/Release/bstream_usage_examples.exe` with Visual Studio.

</details>

## License

MIT
