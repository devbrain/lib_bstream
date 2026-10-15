#include <doctest/doctest.h>
#include <bstream/bstream.hh>
#include <sstream>

namespace {
    struct Player {
        uint32_t id;
        uint16_t score;
        uint8_t lives;
    };
}

TEST_SUITE("stream_writer") {

    TEST_CASE("write basic scalars with endianness") {
        std::stringstream ss;
        bstream::stream_writer writer(ss, bstream::endian::little);

        writer.write<uint32_t>(0x12345678);
        writer.write<uint16_t>(0xAABB);
        writer.write<uint8_t>(0x42);

        std::string str = ss.str();
        REQUIRE(str.size() == 7);
        CHECK(static_cast<uint8_t>(str[0]) == 0x78);
        CHECK(static_cast<uint8_t>(str[1]) == 0x56);
        CHECK(static_cast<uint8_t>(str[2]) == 0x34);
        CHECK(static_cast<uint8_t>(str[3]) == 0x12);
        CHECK(static_cast<uint8_t>(str[4]) == 0xBB);
        CHECK(static_cast<uint8_t>(str[5]) == 0xAA);
        CHECK(static_cast<uint8_t>(str[6]) == 0x42);

        // Read back with stream_reader
        bstream::stream_reader reader(ss, bstream::endian::little);
        CHECK(reader.read<uint32_t>() == 0x12345678);
        CHECK(reader.read<uint16_t>() == 0xAABB);
        CHECK(reader.read<uint8_t>() == 0x42);
    }

    TEST_CASE("write big endian") {
        std::stringstream ss;
        bstream::stream_writer writer(ss, bstream::endian::big);

        writer.write<uint32_t>(0x12345678);
        writer.write<uint16_t>(0xAABB);

        std::string str = ss.str();
        REQUIRE(str.size() == 6);
        CHECK(static_cast<uint8_t>(str[0]) == 0x12);
        CHECK(static_cast<uint8_t>(str[1]) == 0x34);
        CHECK(static_cast<uint8_t>(str[2]) == 0x56);
        CHECK(static_cast<uint8_t>(str[3]) == 0x78);
        CHECK(static_cast<uint8_t>(str[4]) == 0xAA);
        CHECK(static_cast<uint8_t>(str[5]) == 0xBB);
    }

    TEST_CASE("write strings") {
        std::stringstream ss;
        bstream::stream_writer writer(ss);

        writer.write_cstring("Hello");
        writer.write_fixed_string("World", 8, '_');
        writer.write_pascal_string<uint8_t>("Short");

        std::string str = ss.str();
        // "Hello\0" (6) + "World___" (8) + "\x05Short" (6) = 20
        REQUIRE(str.size() == 20);

        bstream::stream_reader reader(ss);
        CHECK(reader.read_cstring() == "Hello");
        CHECK(reader.read_fixed_string(8) == "World___");
        CHECK(reader.read_pascal_string<uint8_t>() == "Short");
    }

    TEST_CASE("write containers and operator<<") {
        std::stringstream ss;
        bstream::stream_writer writer(ss, bstream::endian::little);

        std::vector<uint16_t> vec = {0x1111, 0x2222, 0x3333};
        std::array<uint8_t, 3> arr = {1, 2, 3};

        writer << vec;
        writer << arr;
        writer << 0x12345678U;

        bstream::stream_reader reader(ss, bstream::endian::little);
        CHECK(reader.read<uint16_t>() == 0x1111);
        CHECK(reader.read<uint16_t>() == 0x2222);
        CHECK(reader.read<uint16_t>() == 0x3333);
        CHECK(reader.read<uint8_t>() == 1);
        CHECK(reader.read<uint8_t>() == 2);
        CHECK(reader.read<uint8_t>() == 3);
        CHECK(reader.read<uint32_t>() == 0x12345678U);
    }

#if defined(BSTREAM_HAS_PFR)
    TEST_CASE("write aggregate struct via PFR reflection") {
        std::stringstream ss;
        bstream::stream_writer writer(ss, bstream::endian::little);

        Player p1{0x10203040, 0x5060, 9};
        writer << p1;

        bstream::stream_reader reader(ss, bstream::endian::little);
        auto p2 = reader.read<Player>();

        CHECK(p2.id == p1.id);
        CHECK(p2.score == p1.score);
        CHECK(p2.lives == p1.lives);
    }
#endif

    TEST_CASE("write seek, tell, skip") {
        std::stringstream ss;
        bstream::stream_writer writer(ss, bstream::endian::little);

        writer.write<uint32_t>(0);
        writer.write<uint32_t>(0x22222222);
        CHECK(writer.tell() == 8);

        writer.seek(0);
        CHECK(writer.tell() == 0);
        writer.write<uint32_t>(0x11111111);

        bstream::stream_reader reader(ss, bstream::endian::little);
        CHECK(reader.read<uint32_t>() == 0x11111111);
        CHECK(reader.read<uint32_t>() == 0x22222222);
    }

    TEST_CASE("binary_writer alias") {
        std::stringstream ss;
        bstream::binary_writer writer(ss, bstream::endian::little);
        writer << static_cast<uint16_t>(0x1234);
        CHECK(ss.str().size() == 2);
    }
}
