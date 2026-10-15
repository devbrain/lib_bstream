#include <doctest/doctest.h>
#include <bstream/bstream.hh>

TEST_SUITE("span_reader") {

    TEST_CASE("Scalar reads with endianness") {
        // 0x1234 in little-endian: [0x34, 0x12]
        // 0x12345678 in little-endian: [0x78, 0x56, 0x34, 0x12]
        const uint8_t raw[] = {
            0x2A,                   // uint8: 42
            0x34, 0x12,             // uint16_le: 0x1234
            0x78, 0x56, 0x34, 0x12, // uint32_le: 0x12345678
            0x12, 0x34              // uint16_be: 0x1234
        };

        bstream::span_reader r(raw, sizeof(raw), bstream::endian::little);
        CHECK(r.tell() == 0);
        CHECK(r.size() == 9);
        CHECK(r.remaining() == 9);

        CHECK(r.read<uint8_t>() == 42);
        CHECK(r.tell() == 1);

        CHECK(r.read<uint16_t>() == 0x1234);
        CHECK(r.tell() == 3);

        CHECK(r.read<uint32_t>() == 0x12345678);
        CHECK(r.tell() == 7);

        // Explicit big-endian read
        CHECK((r.read<uint16_t, bstream::endian::big>()) == 0x1234);
        CHECK(r.eof());
        CHECK_THROWS_AS((void)r.read<uint8_t>(), bstream::eof_error);
    }

    TEST_CASE("Navigation and zero-copy sub-spans") {
        const uint8_t raw[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
        bstream::span_reader r(raw, sizeof(raw));

        CHECK(r.tell() == 0);
        r.skip(3);
        CHECK(r.tell() == 3);
        CHECK(r.read<uint8_t>() == 3);

        r.seek(1);
        CHECK(r.tell() == 1);
        CHECK(r.read<uint8_t>() == 1);

        auto sub = r.read_span(4);
        CHECK(sub.size() == 4);
        CHECK(sub[0] == 2);
        CHECK(sub[3] == 5);
        CHECK(r.tell() == 6);

        auto subr = r.subreader(3);
        CHECK(subr.size() == 3);
        CHECK(subr.read<uint8_t>() == 6);
        CHECK(subr.read<uint8_t>() == 7);
        CHECK(subr.read<uint8_t>() == 8);
        CHECK(subr.eof());
    }

    TEST_CASE("String parsing helpers") {
        const char raw[] = "Hello\0FixedStr\0\0\0\x05World";
        bstream::span_reader r(raw, sizeof(raw) - 1);

        CHECK(r.read_cstring() == "Hello");
        CHECK(r.read_fixed_string(11, true) == "FixedStr");
        CHECK(r.read_pascal_string<uint8_t>() == "World");
    }

    TEST_CASE("Operator>> with containers") {
        const uint8_t raw[] = {
            10, 20, 30, 40,
            1, 0, 2, 0, 3, 0
        };
        bstream::span_reader r(raw, sizeof(raw), bstream::endian::little);

        uint8_t c_arr[4]{};
        r >> c_arr;
        CHECK(c_arr[0] == 10);
        CHECK(c_arr[3] == 40);

        std::array<uint16_t, 3> arr{};
        r >> arr;
        CHECK(arr[0] == 1);
        CHECK(arr[1] == 2);
        CHECK(arr[2] == 3);
        CHECK(r.eof());
    }

#if defined(BSTREAM_HAS_FAILSAFE)
    TEST_CASE("try_read with failsafe::expected") {
        const uint8_t raw[] = {1, 2};
        bstream::span_reader r(raw, sizeof(raw));

        auto ok_res = r.try_read<uint16_t>();
        CHECK(ok_res.has_value());

        auto err_res = r.try_read<uint16_t>();
        CHECK(!err_res.has_value());
    }
#endif

}
