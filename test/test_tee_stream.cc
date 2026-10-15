#include <doctest/doctest.h>
#include <bstream/bstream.hh>
#include <sstream>
#include <type_traits>

// Copying or "moving" through a copy must not retain pointers into another
// tee buffer's internal storage after that object is destroyed.
static_assert(!std::is_copy_constructible_v<bstream::tee_stream_buf>);
static_assert(!std::is_copy_assignable_v<bstream::tee_stream_buf>);
static_assert(!std::is_move_constructible_v<bstream::tee_stream_buf>);
static_assert(!std::is_move_assignable_v<bstream::tee_stream_buf>);

namespace {
    class failing_sink : public std::streambuf {
    public:
        bool reject_writes = true;
        bool reject_flush = false;
        std::string bytes;

    protected:
        int_type overflow(int_type c) override {
            if (reject_writes) return traits_type::eof();
            if (c != traits_type::eof()) bytes.push_back(traits_type::to_char_type(c));
            return traits_type::not_eof(c);
        }
        std::streamsize xsputn(const char* s, std::streamsize count) override {
            const auto accepted = reject_writes ? count / 2 : count;
            bytes.append(s, static_cast<std::size_t>(accepted));
            return accepted;
        }
        int sync() override { return reject_flush ? -1 : 0; }
    };
}

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

TEST_CASE("Tee output propagates destination put write and flush failures") {
    failing_sink buffer;
    std::ostream failed(&buffer);
    std::ostringstream good;
    bstream::tee_output_stream tee(failed);
    tee.add_stream(good);

    SUBCASE("single character") {
        tee.put('x');
        CHECK(tee.bad());
        CHECK(failed.bad());
        CHECK(good.str() == "x");
    }
    SUBCASE("partial bulk write through binary writer") {
        bstream::stream_writer writer(tee);
        CHECK_THROWS_AS(writer.write<uint32_t>(0x12345678), bstream::bstream_error);
        CHECK(tee.bad());
        CHECK(buffer.bytes.size() == 2);
        CHECK(good.str().size() == 4);
    }
    SUBCASE("flush failure after successful writes") {
        buffer.reject_writes = false;
        buffer.reject_flush = true;
        bstream::stream_writer writer(tee);
        CHECK_NOTHROW(writer.write<uint32_t>(0x12345678));
        CHECK_THROWS_AS(writer.flush(), bstream::bstream_error);
        CHECK(tee.bad());
        CHECK(buffer.bytes == good.str());
    }
    SUBCASE("tee exception mask") {
        tee.exceptions(std::ios::badbit);
        CHECK_THROWS_AS(tee.put('x'), std::ios_base::failure);
    }
    SUBCASE("destination exception mask") {
        failed.exceptions(std::ios::badbit);
        tee.write("abcd", 4);
        CHECK(tee.bad());
    }
}

TEST_CASE("Tee input reports failed mirror writes") {
    std::istringstream input("payload");
    failing_sink buffer;
    std::ostream failed(&buffer);
    bstream::tee_input_stream tee(input);
    tee.add_stream(failed);
    char c = 0;
    tee.get(c);
    CHECK(tee.bad());
    CHECK(failed.bad());
    CHECK(tee.gcount() == 0);
}
