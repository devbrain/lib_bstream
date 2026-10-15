#if defined(_WIN32)
#include <windows.h>
#endif

// Reproduce Windows macros on every platform, including when the build
// environment already defines NOMINMAX.
#if !defined(min)
#define min(a, b) (((a) < (b)) ? (a) : (b))
#endif
#if !defined(max)
#define max(a, b) (((a) > (b)) ? (a) : (b))
#endif

#include <bstream/detail/platform.hh>

#if defined(min) || defined(max)
#error "bstream headers must remove Windows min/max macros"
#endif
#if defined(_WIN32) && !defined(NOMINMAX)
#error "bstream headers must define NOMINMAX on Windows"
#endif

#include <bstream/bstream.hh>
#include <doctest/doctest.h>
#include <array>
#include <sstream>
#include <string>

TEST_CASE("Headers and templates tolerate Windows min and max macros") {
    std::array<uint8_t, 16> bytes{};
    bstream::span_writer writer(bytes);
    writer.write_fixed_string("abcd", 2);
    writer.write_pascal_string("ef");
    bstream::span_reader reader(writer.written_span());
    CHECK(reader.read_fixed_string(2) == "ab");
    CHECK(reader.read_pascal_string() == "ef");

    std::stringstream stream;
    bstream::stream_writer stream_writer(stream);
    stream_writer.write_fixed_string("abcd", 2);
    stream_writer.write_pascal_string("ef");
    CHECK(stream.str() == std::string("ab\x02" "ef", 5));
}
