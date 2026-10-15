#include <bstream/bstream.hh>
#include <doctest/doctest.h>
#include <sstream>

#if defined(BSTREAM_HAS_PFR)
#error "The no-PFR regression target must not enable Boost.PFR"
#endif

TEST_CASE("Public headers and scalar IO work without Boost PFR") {
    static_assert(!bstream::detail::is_pfr_reflectable_v<uint16_t>);
    uint8_t bytes[2]{};
    bstream::span_writer writer(bytes, sizeof bytes);
    writer.write<uint16_t>(0x1234);
    bstream::span_reader reader(bytes, sizeof bytes);
    CHECK(reader.read<uint16_t>() == 0x1234);
    std::stringstream stream;
    bstream::stream_writer sw(stream);
    sw.write<uint16_t>(0x5678);
    bstream::stream_reader sr(stream);
    CHECK(sr.read<uint16_t>() == 0x5678);
}
