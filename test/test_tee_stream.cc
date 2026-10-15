#include <doctest/doctest.h>
#include <bstream/bstream.hh>
#include <sstream>

TEST_SUITE("tee_stream") {

    TEST_CASE("tee_output_stream writes to multiple outputs") {
        std::ostringstream out1;
        std::ostringstream out2;
        std::ostringstream out3;

        bstream::tee_output_stream tos(out1);
        tos.add_stream(out2);
        tos.add_stream(out3);

        tos << "Hello Tee Stream! " << 42;
        tos.flush();

        CHECK(out1.str() == "Hello Tee Stream! 42");
        CHECK(out2.str() == "Hello Tee Stream! 42");
        CHECK(out3.str() == "Hello Tee Stream! 42");
    }

    TEST_CASE("tee_input_stream mirrors read data to output stream") {
        std::string payload = "Data being read through tee";
        std::istringstream in(payload);
        std::ostringstream mirror;

        bstream::tee_input_stream tis(in);
        tis.add_stream(mirror);

        std::string word1, word2;
        tis >> word1 >> word2;

        CHECK(word1 == "Data");
        CHECK(word2 == "being");

        // The mirror should have received everything read into the buffer
        CHECK(!mirror.str().empty());
        CHECK(mirror.str().rfind("Data being", 0) == 0);
    }

    TEST_CASE("tee_input_stream with binary stream_reader") {
        // Prepare binary data
        std::stringstream ss;
        {
            bstream::stream_writer w(ss, bstream::endian::little);
            w.write<uint32_t>(0x12345678);
            w.write_cstring("TeeBinary");
        }

        std::string original_binary = ss.str();
        std::istringstream in(original_binary);
        std::ostringstream tee_log;

        bstream::tee_input_stream tis(in);
        tis.add_stream(tee_log);

        bstream::stream_reader reader(tis, bstream::endian::little);
        uint32_t magic = reader.read<uint32_t>();
        std::string text = reader.read_cstring();

        CHECK(magic == 0x12345678);
        CHECK(text == "TeeBinary");

        // tee_log should contain the read binary data
        CHECK(tee_log.str() == original_binary);
    }
}
