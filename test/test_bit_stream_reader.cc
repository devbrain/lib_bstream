#include <doctest/doctest.h>
#include <bstream/bstream.hh>
#include <sstream>

TEST_SUITE("bit_stream_reader") {

    TEST_CASE("MSB-first bit reading from istream") {
        // 0b10110010 = 0xB2, 0b11000000 = 0xC0
        const char raw[] = {static_cast<char>(0xB2), static_cast<char>(0xC0)};
        std::string s(raw, sizeof(raw));
        std::istringstream iss(s);

        bstream::bit_stream_reader r(iss, bstream::bit_order::msb_first);

        CHECK(r.read_bit() == true);     // 1
        CHECK(r.read_bit() == false);    // 0
        CHECK(r.read_bits(2) == 0b11);   // 11
        CHECK(r.read_bits(4) == 0b0010); // 0010

        // Next byte: 0xC0
        CHECK(r.read_bits(2) == 0b11);
        r.align_to_byte();
        CHECK(r.eof());
    }

    TEST_CASE("LSB-first bit reading from istream") {
        const char raw[] = {static_cast<char>(0xB2)};
        std::string s(raw, sizeof(raw));
        std::istringstream iss(s);

        bstream::bit_stream_reader r(iss, bstream::bit_order::lsb_first);

        CHECK(r.read_bit() == false);    // 0
        CHECK(r.read_bit() == true);     // 1
        CHECK(r.read_bits(2) == 0b00);   // 00
        CHECK(r.read_bits(4) == 0b1011); // 1011
        CHECK(r.eof());
    }

    TEST_CASE("Wrapping stream_reader") {
        const char raw[] = {static_cast<char>(0xAA), static_cast<char>(0x55)};
        std::string s(raw, sizeof(raw));
        std::istringstream iss(s);

        bstream::stream_reader sr(iss);
        bstream::bit_stream_reader r(sr, bstream::bit_order::msb_first);

        CHECK(r.peek_bits(4) == 0xA);
        CHECK(r.read_bits(4) == 0xA);
        CHECK(r.read_bits(4) == 0xA);
        CHECK(r.read_bits(8) == 0x55);
        CHECK(r.eof());
    }

#if defined(BSTREAM_HAS_FAILSAFE)
    TEST_CASE("try_read_bit and try_read_bits with failsafe::expected") {
        const char raw[] = {static_cast<char>(0x80)};
        std::string s(raw, sizeof(raw));
        std::istringstream iss(s);

        bstream::bit_stream_reader r(iss, bstream::bit_order::msb_first);

        auto b1 = r.try_read_bit();
        CHECK(b1.has_value());
        CHECK(*b1 == true);

        auto rem = r.try_read_bits(7);
        CHECK(rem.has_value());
        CHECK(*rem == 0);

        auto eof_res = r.try_read_bit();
        CHECK(!eof_res.has_value());
    }
#endif

}
