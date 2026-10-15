#include <doctest/doctest.h>
#include <bstream/bstream.hh>
#include <sstream>

TEST_SUITE("stream_copier") {

    TEST_CASE("copy stream to stream") {
        std::string sample = "Hello, this is a test of stream_copier!";
        std::istringstream in(sample);
        std::ostringstream out;

        auto copied = bstream::stream_copier::copy(in, out, 16);
        CHECK(copied == static_cast<std::streamsize>(sample.size()));
        CHECK(out.str() == sample);
    }

    TEST_CASE("copy large stream across chunks") {
        std::string large(20000, 'Z');
        std::istringstream in(large);
        std::ostringstream out;

        auto copied = bstream::stream_copier::copy(in, out);
        CHECK(copied == 20000);
        CHECK(out.str() == large);
    }

    TEST_CASE("copy to string") {
        std::string sample = "Stream to string copy test";
        std::istringstream in(sample);
        std::string out = "Prefix: ";

        auto copied = bstream::stream_copier::copy_to_string(in, out, 8);
        CHECK(copied == static_cast<std::streamsize>(sample.size()));
        CHECK(out == "Prefix: Stream to string copy test");
    }

    TEST_CASE("copy to vector") {
        std::string sample = "Vector data";
        std::istringstream in(sample);
        std::vector<uint8_t> vec;

        auto copied = bstream::stream_copier::copy_to_vector(in, vec, 4);
        CHECK(copied == static_cast<std::streamsize>(sample.size()));
        REQUIRE(vec.size() == sample.size());
        CHECK(std::string(reinterpret_cast<char*>(vec.data()), vec.size()) == sample);
    }

    TEST_CASE("copy unbuffered and BSW compatibility") {
        std::string sample = "Quick brown fox";
        std::istringstream in(sample);
        std::ostringstream out;

        auto copied = bstream::stream_copier::copy_stream_unbuffered(in, out);
        CHECK(copied == static_cast<std::streamsize>(sample.size()));
        CHECK(out.str() == sample);

        std::istringstream in2(sample);
        std::ostringstream out2;
        auto copied2 = bstream::stream_copier::copy_stream(in2, out2);
        CHECK(copied2 == static_cast<std::streamsize>(sample.size()));
        CHECK(out2.str() == sample);
    }
}
