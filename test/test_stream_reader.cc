#include <doctest/doctest.h>
#include <bstream/bstream.hh>
#include <sstream>

TEST_SUITE("stream_reader") {

    TEST_CASE("Basic istream reading") {
        std::string bytes = "\x2A\x34\x12\x78\x56\x34\x12";
        std::istringstream iss(bytes);

        bstream::stream_reader r(iss, bstream::endian::little);
        CHECK(r.read<uint8_t>() == 42);
        CHECK(r.read<uint16_t>() == 0x1234);
        CHECK(r.read<uint32_t>() == 0x12345678);
        CHECK(r.eof());
    }

    TEST_CASE("Strings and containers over istream") {
        const char raw[] = {'T', 'e', 's', 't', '\0', 4, 'D', 'a', 't', 'a'};
        std::string bytes(raw, sizeof(raw));
        std::istringstream iss(bytes);

        bstream::stream_reader r(iss);
        CHECK(r.read_cstring() == "Test");
        CHECK(r.read_pascal_string<uint8_t>() == "Data");
    }

}
