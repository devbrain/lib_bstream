#include <doctest/doctest.h>
#include <bstream/bstream.hh>
#include <sstream>

TEST_SUITE("bit_stream_writer") {

    TEST_CASE("bit_stream_writer msb_first round-trip with bit_stream_reader") {
        std::stringstream ss;
        {
            bstream::bit_stream_writer writer(ss, bstream::bit_order::msb_first);
            writer.write_bit(false);               // 0
            writer.write_bits(0b111, 3);            // 3 bits
            writer.write_bits(0x0C, 4);             // 4 bits -> 8 bits total = 0b01111100 = 0x7C
            writer.write_bits(0xDEADBEEF, 32);      // 32 bits
            writer.write_bits(0x03, 2);             // 2 bits
            writer.align_to_byte(0);                // pad 6 zeros
        } // destructor flushes

        std::string str = ss.str();
        REQUIRE(str.size() == 6);
        CHECK(static_cast<uint8_t>(str[0]) == 0x7C);

        bstream::bit_stream_reader reader(ss, bstream::bit_order::msb_first);
        CHECK(reader.read_bit() == false);
        CHECK(reader.read_bits(3) == 0b111);
        CHECK(reader.read_bits(4) == 0x0C);
        CHECK(reader.read_bits(32) == 0xDEADBEEF);
        CHECK(reader.read_bits(2) == 0x03);
        CHECK(reader.read_bits(6) == 0x00);
    }

    TEST_CASE("bit_stream_writer lsb_first round-trip with bit_stream_reader") {
        std::stringstream ss;
        {
            bstream::bit_stream_writer writer(ss, bstream::bit_order::lsb_first);
            writer.write_bit(true);                 // 1
            writer.write_bits(0b101, 3);            // 3 bits
            writer.write_bits(0x0F, 4);             // 4 bits -> 0xFB
            writer.write_bits(0xFEEDFACE, 32);      // 32 bits
            writer.flush();
        }

        bstream::bit_stream_reader reader(ss, bstream::bit_order::lsb_first);
        CHECK(reader.read_bit() == true);
        CHECK(reader.read_bits(3) == 0b101);
        CHECK(reader.read_bits(4) == 0x0F);
        CHECK(reader.read_bits(32) == 0xFEEDFACE);
    }

    TEST_CASE("bit_stream_writer with stream_writer") {
        std::stringstream ss;
        bstream::stream_writer sw(ss, bstream::endian::little);
        sw.write<uint32_t>(0x11223344);

        {
            bstream::bit_stream_writer bsw(sw, bstream::bit_order::msb_first);
            bsw.write_bits(0xAA, 8);
            bsw.flush();
        }

        sw.write<uint16_t>(0x5566);

        bstream::stream_reader sr(ss, bstream::endian::little);
        CHECK(sr.read<uint32_t>() == 0x11223344);
        CHECK(sr.read<uint8_t>() == 0xAA);
        CHECK(sr.read<uint16_t>() == 0x5566);
    }

    TEST_CASE("bit_stream_writer write_bytes") {
        std::stringstream ss;
        {
            bstream::bit_stream_writer writer(ss, bstream::bit_order::msb_first);
            writer.write_bits(0b101, 3);
            const uint8_t data[] = {10, 20, 30};
            writer.write_bytes(data); // pads 5 bits (zeros), then writes 3 bytes
            writer.flush();
        }

        std::string res = ss.str();
        REQUIRE(res.size() == 4);
        CHECK(static_cast<uint8_t>(res[0]) == 0b10100000);
        CHECK(static_cast<uint8_t>(res[1]) == 10);
        CHECK(static_cast<uint8_t>(res[2]) == 20);
        CHECK(static_cast<uint8_t>(res[3]) == 30);
    }
}
