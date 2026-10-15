#include <doctest/doctest.h>
#include <bstream/bstream.hh>
#include <array>
#include <sstream>

struct SimpleHeader {
    uint8_t magic;
    uint16_t version;
    uint32_t payload_len;
};

struct NestedRecord {
    SimpleHeader header;
    uint16_t flags;
};

TEST_SUITE("pfr_aggregate_reflection") {

#if defined(BSTREAM_HAS_PFR)
    TEST_CASE("Read constructs records with const scalar and nested value fields") {
        struct Nested { const uint16_t value; };
        struct Record {
            const uint16_t version;
            const Nested nested;
            const std::array<uint16_t, 2> values;
            uint8_t flags;
        };
        const Record expected{0x1234, {0x5678}, {0x9abc, 0xdef0}, 0x42};
        const auto check = [&](const Record& record) {
            CHECK(record.version == expected.version);
            CHECK(record.nested.value == expected.nested.value);
            CHECK(record.values == expected.values);
            CHECK(record.flags == expected.flags);
        };
        for (const auto order : {bstream::endian::little, bstream::endian::big}) {
            const std::array<uint8_t, 9> wire = order == bstream::endian::little
                ? std::array<uint8_t, 9>{0x34, 0x12, 0x78, 0x56, 0xbc, 0x9a, 0xf0, 0xde, 0x42}
                : std::array<uint8_t, 9>{0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0, 0x42};
            bstream::span_reader reader(wire, order);
            check(reader.read<Record>());
            CHECK(reader.tell() == wire.size());
            bstream::span_istream input(wire);
            bstream::stream_reader stream_reader(input, order);
            check(stream_reader.read<Record>());
            CHECK(stream_reader.tell() == wire.size());

            std::array<uint8_t, 9> output{};
            bstream::span_writer writer(output, order);
            writer.write(expected);
            CHECK(output == wire);
            std::ostringstream stream;
            bstream::stream_writer stream_writer(stream, order);
            stream_writer.write(expected);
            CHECK(stream.str() == std::string(reinterpret_cast<const char*>(wire.data()), wire.size()));
        }
    }

    TEST_CASE("Automatic aggregate struct reading") {
        const uint8_t raw[] = {
            0xAA,                   // magic: 0xAA
            0x01, 0x00,             // version: 1 (le)
            0x10, 0x00, 0x00, 0x00  // payload_len: 16 (le)
        };

        bstream::span_reader r(raw, sizeof(raw), bstream::endian::little);
        SimpleHeader hdr = r.read<SimpleHeader>();

        CHECK(hdr.magic == 0xAA);
        CHECK(hdr.version == 1);
        CHECK(hdr.payload_len == 16);
        CHECK(r.eof());
    }

    TEST_CASE("Aggregate reading with big-endian byte swapping") {
        const uint8_t raw[] = {
            0xAA,                   // magic: 0xAA
            0x00, 0x02,             // version: 2 (be)
            0x00, 0x00, 0x01, 0x00  // payload_len: 256 (be)
        };

        bstream::span_reader r(raw, sizeof(raw), bstream::endian::big);
        SimpleHeader hdr = r.read<SimpleHeader>();

        CHECK(hdr.magic == 0xAA);
        CHECK(hdr.version == 2);
        CHECK(hdr.payload_len == 256);
    }

    TEST_CASE("Nested aggregate reading") {
        const uint8_t raw[] = {
            0xBB,                   // magic
            0x05, 0x00,             // version: 5
            0x20, 0x00, 0x00, 0x00, // payload_len: 32
            0xFF, 0x00              // flags: 0x00FF
        };

        bstream::span_reader r(raw, sizeof(raw), bstream::endian::little);
        NestedRecord rec = r.read<NestedRecord>();

        CHECK(rec.header.magic == 0xBB);
        CHECK(rec.header.version == 5);
        CHECK(rec.header.payload_len == 32);
        CHECK(rec.flags == 0x00FF);
    }
#else
    TEST_CASE("PFR not enabled") {
        CHECK(true);
    }
#endif

}
