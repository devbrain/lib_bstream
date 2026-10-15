#include <bstream/span_reader.hh>
#include <bstream/stream_reader.hh>
#include <doctest/doctest.h>

#include <array>
#include <bit>
#include <cstdint>
#include <sstream>
#include <string>
#include <type_traits>
#include <vector>

namespace {
    enum class WireValue : uint16_t {};

    template<typename T, typename Reader>
    void check_reader_paths(Reader& reader, const std::array<T, 3>& expected) {
        CHECK(reader.template read_vector<T>(0).empty());
        CHECK((reader.template read_array<T, 0>().empty()));
        CHECK(reader.tell() == 0);

        CHECK(reader.template peek<T>() == expected[0]);
        CHECK(reader.tell() == 0);
        for (const T value : expected) {
            CHECK(reader.template read<T>() == value);
        }
        CHECK(reader.tell() == sizeof(T) * expected.size());

        reader.seek(0);
        for (const T value : expected) {
            T decoded{};
            reader >> decoded;
            CHECK(decoded == value);
        }

        reader.seek(0);
        const std::vector<T> expected_vector(expected.begin(), expected.end());
        CHECK(reader.template read_vector<T>(expected.size()) == expected_vector);

        reader.seek(0);
        std::vector<T> vector(expected.size());
        reader >> vector;
        CHECK(vector == expected_vector);

        reader.seek(0);
        CHECK((reader.template read_array<T, 3>() == expected));

        reader.seek(0);
        std::array<T, 3> array{};
        reader >> array;
        CHECK(array == expected);

        reader.seek(0);
        T raw_array[3]{};
        reader >> raw_array;
        for (std::size_t i = 0; i < expected.size(); ++i) {
            CHECK(raw_array[i] == expected[i]);
        }
        CHECK(reader.tell() == sizeof(T) * expected.size());
    }
}

TEST_CASE_TEMPLATE("Reader scalar and container paths decode both byte orders", T,
                   uint8_t, int16_t, uint32_t, int64_t, float, double, WireValue) {
    std::array<T, 3> expected{static_cast<T>(1), static_cast<T>(42), static_cast<T>(127)};
    if constexpr (std::is_signed_v<T>) {
        expected[1] = static_cast<T>(-42);
    }

    for (const auto order : {bstream::endian::little, bstream::endian::big, bstream::endian::native}) {
        // Construct wire bytes independently of the library's endian helpers.
        std::array<uint8_t, sizeof(T) * 3> wire{};
        for (std::size_t i = 0; i < expected.size(); ++i) {
            const auto bytes = std::bit_cast<std::array<uint8_t, sizeof(T)>>(expected[i]);
            for (std::size_t j = 0; j < sizeof(T); ++j) {
                const auto source = order == bstream::endian::native ? j : sizeof(T) - 1 - j;
                wire[i * sizeof(T) + j] = bytes[source];
            }
        }

        bstream::span_reader memory_reader(wire, order);
        check_reader_paths(memory_reader, expected);

        std::istringstream input(std::string(reinterpret_cast<const char*>(wire.data()), wire.size()));
        bstream::stream_reader stream_reader(input, order);
        check_reader_paths(stream_reader, expected);
    }
}
