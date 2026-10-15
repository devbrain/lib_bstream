#include <doctest/doctest.h>
#include <bstream/bstream.hh>

enum class TestColor : uint16_t {
    Red = 0x1234,
    Green = 0x5678
};

TEST_SUITE("byte_order") {

    TEST_CASE("flip_bytes scalar types") {
        CHECK(bstream::flip_bytes<uint16_t>(0x1234) == 0x3412);
        CHECK(bstream::flip_bytes<int16_t>(0x1234) == 0x3412);
        CHECK(bstream::flip_bytes<uint32_t>(0x12345678) == 0x78563412);
        CHECK(bstream::flip_bytes<int32_t>(0x12345678) == 0x78563412);
        CHECK(bstream::flip_bytes<uint64_t>(0x0102030405060708ULL) == 0x0807060504030201ULL);
        CHECK(bstream::flip_bytes<uint8_t>(0x42) == 0x42); // single byte is no-op

        // Enum flip
        CHECK(bstream::flip_bytes(TestColor::Red) == static_cast<TestColor>(0x3412));

        // Compile-time constexpr test
        constexpr uint32_t compile_time_flip = bstream::flip_bytes<uint32_t>(0xAABBCCDD);
        static_assert(compile_time_flip == 0xDDCCBBAA);
    }

    TEST_CASE("flip_bytes float and double") {
        float f = 1.0f;
        float f_flipped = bstream::flip_bytes(f);
        float f_back = bstream::flip_bytes(f_flipped);
        CHECK(f == f_back);

        double d = 123.456;
        double d_flipped = bstream::flip_bytes(d);
        double d_back = bstream::flip_bytes(d_flipped);
        CHECK(d == d_back);
    }

    TEST_CASE("flip_bytes_in_place") {
        uint32_t val = 0x11223344;
        bstream::flip_bytes_in_place(val);
        CHECK(val == 0x44332211);
        bstream::flip_bytes_in_place(val);
        CHECK(val == 0x11223344);
    }

    TEST_CASE("swap_bytes on span (batch swapping)") {
        uint16_t data[] = {0x1234, 0x5678, 0xABCD};
        bstream::swap_bytes(std::span<uint16_t>(data));

        CHECK(data[0] == 0x3412);
        CHECK(data[1] == 0x7856);
        CHECK(data[2] == 0xCDAB);

        // Swap back
        bstream::swap_bytes(std::span<uint16_t>(data));
        CHECK(data[0] == 0x1234);
        CHECK(data[1] == 0x5678);
        CHECK(data[2] == 0xABCD);
    }

    TEST_CASE("Endian conversion functions") {
        const uint16_t host_val = 0x1234;
        const uint16_t flipped_val = 0x3412;

        if constexpr (std::endian::native == std::endian::little) {
            CHECK(bstream::to_little_endian(host_val) == 0x1234);
            CHECK(bstream::from_little_endian(host_val) == 0x1234);
            CHECK(bstream::to_big_endian(host_val) == flipped_val);
            CHECK(bstream::from_big_endian(flipped_val) == 0x1234);
            CHECK(bstream::to_network(host_val) == flipped_val);
            CHECK(bstream::from_network(flipped_val) == 0x1234);
        } else {
            CHECK(bstream::to_big_endian(host_val) == 0x1234);
            CHECK(bstream::from_big_endian(host_val) == 0x1234);
            CHECK(bstream::to_little_endian(host_val) == flipped_val);
            CHECK(bstream::from_little_endian(flipped_val) == 0x1234);
            CHECK(bstream::to_network(host_val) == 0x1234);
            CHECK(bstream::from_network(host_val) == 0x1234);
        }
    }

    TEST_CASE("Drop-in bsw::byte_order class compatibility") {
        uint32_t val = 0xAABBCCDD;
        CHECK(bstream::byte_order::flip_bytes(val) == 0xDDCCBBAA);

        uint16_t be = bstream::byte_order::to_big_endian<uint16_t>(0x1234);
        CHECK(bstream::byte_order::from_big_endian(be) == 0x1234);

        uint32_t net = bstream::byte_order::to_network<uint32_t>(0x12345678);
        CHECK(bstream::byte_order::from_network(net) == 0x12345678);
    }

}
