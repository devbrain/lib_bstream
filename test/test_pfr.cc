#include <doctest/doctest.h>
#include <bstream/bstream.hh>

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
