#include <doctest/doctest.h>
#include <bstream/bstream.hh>

TEST_SUITE("bit_writer") {

    TEST_CASE("bit_writer msb_first round-trip with bit_reader") {
        bstream::bit_writer writer(bstream::bit_order::msb_first);

        writer.write_bit(true);                 // 1
        writer.write_bits(0b101, 3);            // 3 bits
        writer.write_bits(0x0A, 4);             // 4 bits -> 8 bits total = 1 byte (0b11011010 = 0xDA)
        writer.write_bits(0x12345678, 32);      // 32 bits = 4 bytes
        writer.write_bits(0x07, 3);             // 3 bits
        writer.align_to_byte(0);                // pad 5 zeros to complete byte

        auto data = writer.finish();
        REQUIRE(data.size() == 6);
        CHECK(data[0] == 0xDA);

        bstream::bit_reader reader(data, bstream::bit_order::msb_first);
        CHECK(reader.read_bit() == true);
        CHECK(reader.read_bits(3) == 0b101);
        CHECK(reader.read_bits(4) == 0x0A);
        CHECK(reader.read_bits(32) == 0x12345678);
        CHECK(reader.read_bits(3) == 0x07);
        CHECK(reader.read_bits(5) == 0x00); // the padding bits
    }

    TEST_CASE("bit_writer lsb_first round-trip with bit_reader") {
        bstream::bit_writer writer(bstream::bit_order::lsb_first);

        writer.write_bit(true);                 // 1 (bit 0)
        writer.write_bits(0b101, 3);            // 3 bits (bits 1..3) -> 0b1011 = 0x0B
        writer.write_bits(0x0A, 4);             // 4 bits (bits 4..7) -> (0x0A << 4) | 0x0B = 0xAB
        writer.write_bits(0x12345678, 32);      // 32 bits
        writer.write_bits(0x05, 3);             // 3 bits (0b101)
        writer.align_to_byte(1);                // pad 5 ones (0b11111)

        auto data = writer.finish();
        REQUIRE(data.size() == 6);
        CHECK(data[0] == 0xAB);

        bstream::bit_reader reader(data, bstream::bit_order::lsb_first);
        CHECK(reader.read_bit() == true);
        CHECK(reader.read_bits(3) == 0b101);
        CHECK(reader.read_bits(4) == 0x0A);
        CHECK(reader.read_bits(32) == 0x12345678);
        CHECK(reader.read_bits(3) == 0x05);
        CHECK(reader.read_bits(5) == 0x1F); // 5 ones
    }

    TEST_CASE("bit_writer 64-bit writing") {
        bstream::bit_writer writer_msb(bstream::bit_order::msb_first);
        const uint64_t val = 0x0123456789ABCDEFULL;
        writer_msb.write_bits(val, 64);
        auto data_msb = writer_msb.finish();
        REQUIRE(data_msb.size() == 8);

        bstream::bit_reader reader_msb(data_msb, bstream::bit_order::msb_first);
        CHECK(reader_msb.read_bits(64) == val);

        bstream::bit_writer writer_lsb(bstream::bit_order::lsb_first);
        writer_lsb.write_bits(val, 64);
        auto data_lsb = writer_lsb.finish();
        REQUIRE(data_lsb.size() == 8);

        bstream::bit_reader reader_lsb(data_lsb, bstream::bit_order::lsb_first);
        CHECK(reader_lsb.read_bits(64) == val);
    }

    TEST_CASE("bit_writer to fixed span and span_writer") {
        uint8_t buffer[16] = {};
        {
            bstream::bit_writer writer(std::span<uint8_t>(buffer, sizeof(buffer)), bstream::bit_order::msb_first);
            writer.write_bits(0xFF, 8);
            writer.write_bits(0x55, 8);
            writer.flush();
        }
        CHECK(buffer[0] == 0xFF);
        CHECK(buffer[1] == 0x55);

        uint8_t span_buf[16] = {};
        bstream::span_writer sw(span_buf, sizeof(span_buf));
        {
            bstream::bit_writer bw(sw, bstream::bit_order::lsb_first);
            bw.write_bits(0x42, 8);
            bw.flush();
        }
        CHECK(sw.tell() == 1);
        CHECK(span_buf[0] == 0x42);
    }

    TEST_CASE("bit_writer write_bytes") {
        bstream::bit_writer writer(bstream::bit_order::msb_first);
        writer.write_bits(0b11, 2);
        const uint8_t raw[] = {0xAA, 0xBB};
        writer.write_bytes(raw); // aligns first (pads 6 zeros), then writes 0xAA, 0xBB

        auto data = writer.finish();
        REQUIRE(data.size() == 3);
        CHECK(data[0] == 0b11000000);
        CHECK(data[1] == 0xAA);
        CHECK(data[2] == 0xBB);
    }
}
