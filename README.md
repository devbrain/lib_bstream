# lib_bstream

A modern C++20 zero-copy binary streaming, parsing, and bit-manipulation library.

## Features

- **Zero-Copy Memory Parsing (`span_reader`)**:
  - Direct pointer-arithmetic reads over `std::span<const uint8_t>`, `std::span<const std::byte>`, `mio::mmap_source`, `std::vector<uint8_t>`, or raw memory.
  - Zero virtual-function overhead, zero allocations.
  - Sub-span slicing (`read_span()`, `subreader()`) for partitioned files/containers.
- **Stream I/O (`stream_reader`)**:
  - Sequential reads from any `std::istream&`.
- **Bit-Level Streaming (`bit_reader` & `bit_stream_reader`)**:
  - `bit_reader` / `bit_span_reader`: In-memory zero-copy bit extraction from spans.
  - `bit_stream_reader`: Sequential bit-by-bit reading directly from `std::istream&` or `stream_reader`.
  - Read 1 to 64 bits at a time (`read_bits(N)`, `read_bit()`).
  - Configurable `bit_order::msb_first` (Huffman, Deflate, Amiga) or `bit_order::lsb_first` (LZSS variants).
  - Byte-boundary alignment (`align_to_byte()`) and bit peeking (`peek_bits(N)`).
- **Endianness Engine**:
  - Native, little-endian, and big-endian support via `std::endian`.
  - Single-cycle hardware byte swapping via compiler intrinsics.
- **Automatic Aggregate Struct Reflection (Boost.PFR)**:
  - Read plain C++ structs field-by-field automatically (`reader.read<MyHeader>()`).
  - Immune to compiler padding and ABI alignment differences.
  - Automatically byte-swaps individual fields according to stream endianness.
- **String & Container Deserialization**:
  - Null-terminated (`read_cstring`), fixed-width (`read_fixed_string`), and length-prefixed (`read_pascal_string`).
  - Containers (`std::vector`, `std::array`, C arrays).
- **Dual Error Strategies**:
  - Exception policy (`bstream::eof_error`, `bstream::parse_error`).
  - Non-throwing result policy via `failsafe::expected` (`reader.try_read<T>()`).
- **Binary Writing (`span_writer`)**:
  - Fast, bounded binary serialization into memory buffers.

## Quick Example

```cpp
#include <bstream/bstream.hh>
#include <mio/mmap.hpp>

struct Header {
    uint32_t magic;
    uint16_t version;
    uint16_t file_count;
};

void parse_package(const std::filesystem::path& path) {
    // 1. Zero-copy memory map
    mio::mmap_source file(path.string());
    
    // 2. Wrap in span_reader (defaults to little-endian)
    bstream::span_reader r(file);
    
    // 3. Automatic aggregate reflection (immune to padding)
    Header hdr = r.read<Header>();
    
    // 4. Read zero-copy subspan for file entries
    std::string filename = r.read_cstring();
    auto payload = r.read_span(hdr.file_count * 32);
}
```

## CMake Integration

```cmake
include(FetchContent)
FetchContent_Declare(bstream
    GIT_REPOSITORY https://github.com/devbrain/lib_bstream.git
    GIT_TAG master
)
FetchContent_MakeAvailable(bstream)

target_link_libraries(my_target PRIVATE neutrino::bstream)
```

## License

MIT
