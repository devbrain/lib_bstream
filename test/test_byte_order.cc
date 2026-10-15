#include <doctest/doctest.h>
#include <bstream/bstream.hh>

#include <array>
#include <bit>
#include <cfloat>
#include <type_traits>

namespace {
    template<typename T>
    concept endian_convertible = requires(T value) {
        bstream::byteswap(value);
        bstream::flip_bytes(value);
        bstream::from_endian(value, bstream::endian::big);
        bstream::to_endian(value, bstream::endian::big);
        bstream::convert_endian(value, bstream::endian::little, bstream::endian::big);
        bstream::byte_order::flip_bytes(value);
    };
}

static_assert(endian_convertible<float>);
static_assert(endian_convertible<double>);
static_assert(!bstream::scalar_type<void>);
#if LDBL_MANT_DIG == 64 || LDBL_MANT_DIG == 113
// x87 extended and binary128 cannot use the binary32/binary64 value API.
static_assert(!bstream::scalar_type<long double>);
static_assert(!endian_convertible<long double>);
#elif LDBL_MANT_DIG == 53 && LDBL_MAX_EXP == 1024
// MSVC represents long double as binary64, which remains supported.
static_assert(bstream::scalar_type<long double>);
static_assert(endian_convertible<long double>);
#endif

enum class TestColor : uint16_t {
    Red = 0x1234,
    Green = 0x5678
};

static_assert(bstream::byteswap(uint16_t{0x1234}) == uint16_t{0x3412});
static_assert(bstream::byteswap(uint32_t{0x12345678}) == uint32_t{0x78563412});
static_assert(bstream::byteswap(uint64_t{0x0123456789abcdefULL}) == uint64_t{0xefcdab8967452301ULL});
static_assert(bstream::byteswap(int16_t{-2}) == int16_t{-257});
static_assert(bstream::byteswap(int32_t{-2}) == int32_t{-16777217});
static_assert(bstream::byteswap(int64_t{-2}) == int64_t{-72057594037927937LL});
static_assert(bstream::byteswap(TestColor::Red) == static_cast<TestColor>(0x3412));
static_assert(std::bit_cast<uint32_t>(bstream::byteswap(std::bit_cast<float>(uint32_t{0x12345678}))) == 0x78563412U);
static_assert(std::bit_cast<uint64_t>(bstream::byteswap(std::bit_cast<double>(uint64_t{0x0123456789abcdefULL}))) ==
              0xefcdab8967452301ULL);

TEST_SUITE("byte_order") {

    TEST_CASE_TEMPLATE("byteswap reverses object bytes across integer and floating types", T,
                       uint16_t, int16_t, uint32_t, int32_t, uint64_t, int64_t,
                       long, unsigned long, wchar_t, char16_t, char32_t, float, double) {
        // High bits exercise signed values; byte-by-byte expectations do not
        // depend on the host byte order or the platform's width of long.
        std::array<uint8_t, sizeof(T)> original{};
        for (std::size_t i = 0; i < original.size(); ++i) {
            original[i] = static_cast<uint8_t>(0x81U + i);
        }
        const T value = std::bit_cast<T>(original);
        const auto swapped = std::bit_cast<decltype(original)>(bstream::byteswap(value));
        for (std::size_t i = 0; i < original.size(); ++i) {
            CHECK(swapped[i] == original[original.size() - 1 - i]);
        }
        CHECK(std::bit_cast<decltype(original)>(bstream::byteswap(bstream::byteswap(value))) == original);
    }

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
