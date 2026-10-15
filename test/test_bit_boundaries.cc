#include <doctest/doctest.h>
#include <bstream/bstream.hh>

#include <array>
#include <limits>
#include <sstream>
#include <vector>

namespace {
    using bstream::bit_order;

    // Independent, bit-by-bit oracle; reader/writer round trips alone can hide
    // matching bugs in the two implementations.
    uint64_t reference_read(std::span<const uint8_t> bytes, unsigned int start,
                            unsigned int count, bit_order order) {
        uint64_t value = 0;
        for (unsigned int i = 0; i < count; ++i) {
            const auto pos = start + i;
            const auto shift = order == bit_order::msb_first ? 7 - pos % 8 : pos % 8;
            const uint64_t bit = (bytes[pos / 8] >> shift) & 1U;
            if (order == bit_order::msb_first) value = (value << 1) | bit;
            else value |= bit << i;
        }
        return value;
    }

    template<typename Check>
    void check_readers(std::span<const uint8_t> bytes, bit_order order, Check check) {
        {
            INFO("span reader");
            bstream::bit_reader reader(bytes, order);
            check(reader);
        }
        {
            INFO("stream reader");
            std::istringstream input(std::string(reinterpret_cast<const char*>(bytes.data()), bytes.size()));
            bstream::bit_stream_reader reader(input, order);
            check(reader);
        }
    }

    void reference_write(std::vector<uint8_t>& bytes, unsigned int& pos,
                         uint64_t value, unsigned int count, bit_order order) {
        for (unsigned int i = 0; i < count; ++i, ++pos) {
            if (pos % 8 == 0) bytes.push_back(0);
            const auto source = order == bit_order::msb_first ? count - 1 - i : i;
            const auto target = order == bit_order::msb_first ? 7 - pos % 8 : pos % 8;
            bytes[pos / 8] |= static_cast<uint8_t>(((value >> source) & 1U) << target);
        }
    }

    template<typename Write>
    void check_writers(bit_order order, const std::vector<uint8_t>& expected, Write write) {
        {
            INFO("span writer");
            bstream::bit_writer writer(order);
            write(writer);
            CHECK(writer.finish() == expected);
        }
        {
            INFO("stream writer");
            std::ostringstream output;
            bstream::bit_stream_writer writer(output, order);
            write(writer);
            writer.flush();
            const auto bytes = output.str();
            CHECK(std::vector<uint8_t>(bytes.begin(), bytes.end()) == expected);
        }
    }
}

TEST_CASE("Bit readers support every width at every bit offset") {
    const std::array<uint8_t, 16> bytes = {
        0xff, 0x81, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd,
        0xef, 0x00, 0x55, 0xaa, 0x01, 0x80, 0x0f, 0xf0
    };
    for (auto order : {bit_order::msb_first, bit_order::lsb_first}) {
        for (unsigned int offset = 0; offset < 8; ++offset) {
            for (unsigned int count = 0; count <= 64; ++count) {
                CAPTURE(order);
                CAPTURE(offset);
                CAPTURE(count);
                check_readers(bytes, order, [&](auto& reader) {
                    reader.skip_bits(offset);
                    const auto expected = reference_read(bytes, offset, count, order);
                    CHECK(reader.peek_bits(count) == expected);
                    CHECK(reader.peek_bits(count) == expected);
                    CHECK(reader.read_bits(count) == expected);
                    CHECK(reader.read_bits(8) == reference_read(bytes, offset + count, 8, order));
                });
            }
        }
    }
}

TEST_CASE("Bit readers preserve data across repeated buffer refills") {
    std::array<uint8_t, 256> bytes{};
    for (std::size_t i = 0; i < bytes.size(); ++i) bytes[i] = static_cast<uint8_t>(i * 73 + 19);
    for (auto order : {bit_order::msb_first, bit_order::lsb_first}) {
        check_readers(bytes, order, [&](auto& reader) {
            unsigned int pos = 0;
            for (unsigned int i = 0; i < 50; ++i) {
                const auto count = (i * 37) % 65;
                CHECK(reader.peek_bits(64) == reference_read(bytes, pos, 64, order));
                CHECK(reader.read_bits(count) == reference_read(bytes, pos, count, order));
                pos += count;
            }
        });
    }
}

TEST_CASE("Bit alignment preserves whole bytes fetched by peeking") {
    const uint8_t bytes[] = {0xaa, 0xbb, 0xcc, 0xdd};
    for (auto order : {bit_order::msb_first, bit_order::lsb_first}) {
        check_readers(bytes, order, [&](auto& reader) {
            (void)reader.peek_bits(24);
            reader.align_to_byte(); // Already aligned: consume nothing.
            CHECK(reader.read_bits(3) == reference_read(bytes, 0, 3, order));
            reader.align_to_byte();
            CHECK(reader.read_bits(8) == 0xbb);
            CHECK(reader.read_bits(16) == reference_read(bytes, 16, 16, order));
            CHECK_THROWS_AS(reader.read_bits(1), bstream::eof_error);
        });
    }
}

TEST_CASE("Failed bit peeks retain available data and reject invalid widths") {
    const uint8_t bytes[] = {0xaa, 0xbb};
    for (auto order : {bit_order::msb_first, bit_order::lsb_first}) {
        check_readers(bytes, order, [&](auto& reader) {
            CHECK_THROWS_AS(reader.read_bits(65), bstream::bstream_error);
            CHECK_THROWS_AS(reader.peek_bits(65), bstream::bstream_error);
            CHECK_THROWS_AS(reader.peek_bits(64), bstream::eof_error);
            CHECK(reader.read_bits(16) == reference_read(bytes, 0, 16, order));
            CHECK(reader.peek_bits(0) == 0);
        });
    }
}

TEST_CASE("Bit writers support every width at every bit offset") {
    for (auto order : {bit_order::msb_first, bit_order::lsb_first}) {
        for (auto value : {uint64_t{0}, UINT64_MAX, uint64_t{0x8123456789abcdef}}) {
            for (unsigned int offset = 0; offset < 8; ++offset) {
                for (unsigned int count = 0; count <= 64; ++count) {
                    CAPTURE(order);
                    CAPTURE(value);
                    CAPTURE(offset);
                    CAPTURE(count);
                    std::vector<uint8_t> expected;
                    unsigned int pos = 0;
                    reference_write(expected, pos, 0xff, offset, order);
                    reference_write(expected, pos, value, count, order);
                    reference_write(expected, pos, 0, 64, order);
                    reference_write(expected, pos, 0xa5, 8, order);
                    check_writers(order, expected, [&](auto& writer) {
                        writer.write_bits(0xff, offset);
                        writer.write_bits(value, count);
                        writer.write_bits(0, 64);
                        writer.write_bits(0xa5, 8);
                        CHECK(writer.total_bits_written() == pos);
                    });
                }
            }
        }
    }
}

TEST_CASE("Bit order changes are safe at byte boundaries including after peeking") {
    const uint8_t bytes[] = {0xff, 0x00, 0x96};
    for (auto order : {bit_order::msb_first, bit_order::lsb_first}) {
        const auto other = order == bit_order::msb_first ? bit_order::lsb_first : bit_order::msb_first;
        check_readers(bytes, order, [&](auto& reader) {
            CHECK(reader.read_bits(8) == 0xff);
            (void)reader.peek_bits(16);
            reader.set_order(other);
            CHECK(reader.read_bits(8) == 0);
            CHECK(reader.read_bits(3) == reference_read(bytes, 16, 3, other));
            CHECK_NOTHROW(reader.set_order(other));
            CHECK_THROWS_AS(reader.set_order(order), bstream::bstream_error);
            CHECK(reader.order() == other);
            CHECK(reader.read_bits(5) == reference_read(bytes, 19, 5, other));
        });
        check_writers(order, {0xff, 0x00, 0x96}, [&](auto& writer) {
            writer.write_bits(0xff, 8);
            writer.set_order(other);
            writer.write_bits(0, 8);
            writer.write_bits(reference_read(bytes, 16, 3, other), 3);
            CHECK_NOTHROW(writer.set_order(other));
            CHECK_THROWS_AS(writer.set_order(order), bstream::bstream_error);
            CHECK(writer.order() == other);
            writer.write_bits(reference_read(bytes, 19, 5, other), 5);
        });
    }
}

TEST_CASE("Failed bit writes leave valid state for flushing and destruction") {
    for (auto order : {bit_order::msb_first, bit_order::lsb_first}) {
        uint8_t bytes[1]{};
        {
            bstream::bit_writer writer(std::span<uint8_t>{bytes}, order);
            CHECK_THROWS_AS(writer.write_bits(0xffffffff, 32), bstream::eof_error);
            CHECK(writer.bits_in_buffer() == 0);
            CHECK(writer.total_bits_written() == 8);
            CHECK_NOTHROW(writer.flush());
        }
        CHECK(bytes[0] == 0xff);
        {
            bstream::span_ostream output(bytes, sizeof bytes);
            bstream::bit_stream_writer writer(output, order);
            CHECK_THROWS_AS(writer.write_bits(0xffffffff, 32), bstream::bstream_error);
            CHECK(writer.bits_in_buffer() == 0);
            CHECK(writer.total_bits_written() == 8);
            CHECK_THROWS_AS(writer.flush(), bstream::bstream_error);
        } // Destruction must remain safe even though output is still failed.
    }
}

TEST_CASE("Failed bit padding can be retried without shifting the pending bits twice") {
    for (auto order : {bit_order::msb_first, bit_order::lsb_first}) {
        const uint8_t expected = order == bit_order::msb_first ? 0xa0 : 0x05;
        uint8_t bytes[1]{};
        bstream::span_writer output(bytes, sizeof bytes);
        bstream::bit_writer writer(output, order);
        writer.write_bits(5, 3);
        output.seek(1);
        CHECK_THROWS_AS(writer.flush(), bstream::eof_error);
        CHECK(writer.bits_in_buffer() == 3);
        output.seek(0);
        writer.flush();
        CHECK(bytes[0] == expected);

        bstream::span_ostream stream(bytes, sizeof bytes);
        bstream::bit_stream_writer stream_writer(stream, order);
        stream_writer.write_bits(5, 3);
        stream.seekp(1);
        CHECK_THROWS_AS(stream_writer.flush(), bstream::bstream_error);
        CHECK(stream_writer.bits_in_buffer() == 3);
        stream.clear();
        stream.seekp(0);
        stream_writer.flush();
        CHECK(bytes[0] == expected);
    }
}
