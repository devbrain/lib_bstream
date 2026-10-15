#include <doctest/doctest.h>
#include <bstream/bstream.hh>

TEST_SUITE("bit_reader") {

    TEST_CASE("MSB-first bit reading") {
        // 0b10110010 = 0xB2
        const uint8_t raw[] = {0xB2};
        bstream::bit_reader r(raw, sizeof(raw), bstream::bit_order::msb_first);

        CHECK(r.read_bit() == true);   // 1
        CHECK(r.read_bit() == false);  // 0
        CHECK(r.read_bits(2) == 0b11); // 11
        CHECK(r.read_bits(4) == 0b0010); // 0010
        CHECK(r.total_bits_remaining() == 0);
        CHECK_THROWS_AS((void)r.read_bit(), bstream::eof_error);
    }

    TEST_CASE("LSB-first bit reading") {
        // 0b10110010 = 0xB2
        // LSB order reads lowest bits first: 0, 1, 0, 0, 1, 1, 0, 1
        const uint8_t raw[] = {0xB2};
        bstream::bit_reader r(raw, sizeof(raw), bstream::bit_order::lsb_first);

        CHECK(r.read_bit() == false); // bit 0: 0
        CHECK(r.read_bit() == true);  // bit 1: 1
        CHECK(r.read_bits(2) == 0b00); // bits 2,3: 0, 0
        CHECK(r.read_bits(4) == 0b1011); // bits 4..7: 1011 (11)
        CHECK(r.total_bits_remaining() == 0);
    }

    TEST_CASE("Peeking bits and byte alignment") {
        const uint8_t raw[] = {0xFF, 0x00};
        bstream::bit_reader r(raw, sizeof(raw), bstream::bit_order::msb_first);

        CHECK(r.peek_bits(4) == 0xF);
        CHECK(r.read_bits(3) == 0x7);
        r.align_to_byte();
        // Cursor is now aligned to 2nd byte (0x00)
        CHECK(r.read_bits(8) == 0x00);
    }

}
