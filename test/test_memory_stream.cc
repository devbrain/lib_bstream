#include <doctest/doctest.h>
#include <bstream/bstream.hh>
#include <string>
#include <vector>

TEST_SUITE("memory_stream") {

    TEST_CASE("span_istream reading") {
        const std::string text = "Hello World 123";
        bstream::span_istream is(text);

        std::string w1, w2;
        int num = 0;
        is >> w1 >> w2 >> num;

        CHECK(w1 == "Hello");
        CHECK(w2 == "World");
        CHECK(num == 123);
    }

    TEST_CASE("span_istream binary reading with stream_reader") {
        const uint8_t binary_data[] = {0x78, 0x56, 0x34, 0x12, 0x05, 'H', 'e', 'l', 'l', 'o'};
        bstream::span_istream is(binary_data, sizeof(binary_data));

        bstream::stream_reader reader(is, bstream::endian::little);
        CHECK(reader.read<uint32_t>() == 0x12345678);
        CHECK(reader.read_pascal_string<uint8_t>() == "Hello");
        CHECK(reader.eof());
    }

    TEST_CASE("span_istream seek and tell") {
        const char data[] = "0123456789";
        bstream::span_istream is(data, 10);

        CHECK(is.tellg() == 0);
        char c = 0;
        is >> c;
        CHECK(c == '0');
        CHECK(is.tellg() == 1);

        is.seekg(5, std::ios_base::beg);
        CHECK(is.tellg() == 5);
        is >> c;
        CHECK(c == '5');

        is.seekg(2, std::ios_base::cur);
        CHECK(is.tellg() == 8);
        is >> c;
        CHECK(c == '8');

        is.seekg(-2, std::ios_base::end);
        CHECK(is.tellg() == 8);
        is >> c;
        CHECK(c == '8');
    }

    TEST_CASE("span_ostream writing") {
        char buffer[64] = {};
        bstream::span_ostream os(buffer, sizeof(buffer));

        os << "Answer: " << 42;
        CHECK(os.chars_written() == 10);
        CHECK(os.written_string_view() == "Answer: 42");
        CHECK(std::string(buffer, 10) == "Answer: 42");
    }

    TEST_CASE("span_ostream with stream_writer") {
        uint8_t buffer[32] = {};
        bstream::span_ostream os(buffer, sizeof(buffer));

        bstream::stream_writer writer(os, bstream::endian::little);
        writer.write<uint32_t>(0xAABBCCDD);
        writer.write_cstring("OK");

        CHECK(os.chars_written() == 7);

        // Read back with span_reader
        bstream::span_reader reader(std::span<const uint8_t>(buffer, os.chars_written()));
        CHECK(reader.read<uint32_t>() == 0xAABBCCDD);
        CHECK(reader.read_cstring() == "OK");
    }

    TEST_CASE("span_ostream seek and overwrite") {
        char buffer[16] = "xxxxxxxx";
        bstream::span_ostream os(buffer, 8);

        os.seekp(2, std::ios_base::beg);
        os << "AB";
        CHECK(os.tellp() == 4);
        CHECK(buffer[2] == 'A');
        CHECK(buffer[3] == 'B');
    }

    TEST_CASE("bsw memory_input_stream and memory_output_stream compatibility") {
        const char src[] = "Testing BSW";
        bstream::memory_input_stream mis(src, 11);
        char dest[32] = {};
        mis.read(dest, 7);
        CHECK(std::string(dest, 7) == "Testing");

        char out_buf[32] = {};
        bstream::memory_output_stream mos(out_buf, sizeof(out_buf));
        mos << "Output test";
        CHECK(mos.chars_written() == 11);
        CHECK(mos.written_string_view() == "Output test");
    }
}
