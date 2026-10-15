#include <doctest/doctest.h>
#include <bstream/bstream.hh>

struct OutHeader {
    uint8_t id;
    uint16_t size;
};

TEST_SUITE("span_writer") {

    TEST_CASE("Scalar and string writing") {
        uint8_t buf[32]{};
        bstream::span_writer w(buf, sizeof(buf), bstream::endian::little);

        w.write<uint8_t>(0x55);
        w.write<uint16_t>(0x1234);
        w.write_cstring("OK");
        w.write_pascal_string<uint8_t>("Hi");

        CHECK(w.tell() == 1 + 2 + 3 + 3); // 9 bytes

        // Read it back with span_reader to verify
        bstream::span_reader r(w.written_span(), bstream::endian::little);
        CHECK(r.read<uint8_t>() == 0x55);
        CHECK(r.read<uint16_t>() == 0x1234);
        CHECK(r.read_cstring() == "OK");
        CHECK(r.read_pascal_string<uint8_t>() == "Hi");
        CHECK(r.eof());
    }

#if defined(BSTREAM_HAS_PFR)
    TEST_CASE("Aggregate writing with PFR") {
        uint8_t buf[16]{};
        bstream::span_writer w(buf, sizeof(buf), bstream::endian::big);

        OutHeader h{0x77, 0x0102};
        w << h;

        bstream::span_reader r(w.written_span(), bstream::endian::big);
        OutHeader back = r.read<OutHeader>();
        CHECK(back.id == 0x77);
        CHECK(back.size == 0x0102);
    }
#endif

}
