#include <doctest/doctest.h>
#include <bstream/bstream.hh>
#include <string>
#include <vector>
#include <limits>
#include <utility>
#include <string_view>

#if defined(__unix__) || defined(__APPLE__)
#include <sys/mman.h>
#endif

TEST_SUITE("memory_stream") {

    TEST_CASE("Read-only putback rejects a different character without rewinding") {
        bstream::span_istream input("abc", 3);
        CHECK(input.get() == 'a');
        input.putback('X');
        REQUIRE(input.bad());
        input.clear();
        CHECK(input.tellg() == 1);
        CHECK(input.get() == 'b');

        input.putback('b');
        REQUIRE(input.good());
        CHECK(input.get() == 'b');
        input.unget();
        REQUIRE(input.good());
        CHECK(input.get() == 'b');

        input.seekg(0);
        input.unget();
        CHECK(input.bad());
        input.clear();
        CHECK(input.tellg() == 0);
    }

    TEST_CASE("Writable stream buffers support replacement putback") {
        char storage[] = "abc";
        bstream::span_streambuf buffer(storage, 3);
        std::istream input(&buffer);
        CHECK(input.get() == 'a');
        input.putback('X');
        REQUIRE(input.good());
        CHECK(input.tellg() == 0);
        CHECK(input.get() == 'X');
        CHECK(storage[0] == 'X');
    }

    TEST_CASE("Empty memory streams can tell and seek their sole position") {
        char storage = 'x';
        for (const bool null_backing : {false, true}) {
            const std::span<char> empty = null_backing ? std::span<char>{} : std::span<char>(&storage, 0);
            bstream::span_istream input{std::span<const char>(empty)};
            bstream::span_ostream output(empty);
            CHECK(input.tellg() == 0);
            CHECK(output.tellp() == 0);
            input.seekg(0);
            output.seekp(0);
            CHECK(input.good());
            CHECK(output.good());

            for (const auto origin : {std::ios::beg, std::ios::cur, std::ios::end}) {
                input.seekg(0, origin);
                output.seekp(0, origin);
                CHECK(input.good());
                CHECK(output.good());
                CHECK(input.tellg() == 0);
                CHECK(output.tellp() == 0);
                for (const std::streamoff offset : {-1, 1}) {
                    input.seekg(offset, origin);
                    output.seekp(offset, origin);
                    CHECK(input.fail());
                    CHECK(output.fail());
                    input.clear();
                    output.clear();
                    CHECK(input.tellg() == 0);
                    CHECK(output.tellp() == 0);
                }
            }
            CHECK(input.get() == std::char_traits<char>::eof());
            output.put('z');
            CHECK(output.bad());
            input.reset();
            output.reset();
            CHECK(input.tellg() == 0);
            CHECK(output.tellp() == 0);
            CHECK(output.written_span().empty());
            CHECK(storage == 'x');
        }
    }

    TEST_CASE("Empty stream buffer seeking still enforces open modes") {
        using mode = std::ios_base;
        const auto fail_pos = std::streampos(std::streamoff(-1));
        for (const auto open_mode : {mode::in, mode::out, mode::in | mode::out}) {
            bstream::span_streambuf buffer(std::span<char>{}, open_mode);
            for (const auto direction : {mode::in, mode::out, mode::in | mode::out}) {
                const bool enabled = (direction & open_mode) == direction;
                CHECK(buffer.pubseekoff(0, mode::beg, direction) == (enabled ? std::streampos(0) : fail_pos));
                CHECK(buffer.pubseekoff(0, mode::end, direction) == (enabled ? std::streampos(0) : fail_pos));
                const bool single_direction = direction != (mode::in | mode::out);
                CHECK(buffer.pubseekoff(0, mode::cur, direction) ==
                      (enabled && single_direction ? std::streampos(0) : fail_pos));
            }
            CHECK(buffer.pubseekoff(0, mode::beg, mode::openmode{}) == fail_pos);
        }
        bstream::span_streambuf default_buffer;
        CHECK(default_buffer.pubseekpos(0, mode::in) == 0);
        CHECK(default_buffer.pubseekpos(0, mode::out) == fail_pos);
    }

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
        bstream::span_reader reader(os.written_span());
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

TEST_CASE("Moving span_streambuf preserves independent input and output positions") {
    char bytes[] = "0123456789";
    bstream::span_streambuf original(bytes, 10);
    CHECK(original.sbumpc() == '0');
    CHECK(original.sbumpc() == '1');
    CHECK(original.sputn("ABCD", 4) == 4);

    bstream::span_streambuf moved(std::move(original));
    CHECK(original.span().empty());
    CHECK(original.sgetc() == std::char_traits<char>::eof());
    CHECK(original.sputc('x') == std::char_traits<char>::eof());
    CHECK(moved.pubseekoff(0, std::ios::cur, std::ios::in) == 2);
    CHECK(moved.pubseekoff(0, std::ios::cur, std::ios::out) == 4);
    CHECK(moved.sbumpc() == 'C');
    CHECK(moved.sputc('E') == 'E');

    char other_bytes[] = "old";
    bstream::span_streambuf assigned(other_bytes, 3);
    assigned = std::move(moved);
    CHECK(moved.span().empty());
    CHECK(assigned.pubseekoff(0, std::ios::cur, std::ios::in) == 3);
    CHECK(assigned.pubseekoff(0, std::ios::cur, std::ios::out) == 5);
    CHECK(assigned.sbumpc() == 'D');
    CHECK(assigned.sputc('F') == 'F');
    CHECK(std::string(bytes, 6) == "ABCDEF");
    CHECK(std::string(other_bytes) == "old");
}

TEST_CASE("Moving empty and single-direction span buffers preserves their modes") {
    bstream::span_streambuf empty;
    bstream::span_streambuf moved_empty(std::move(empty));
    CHECK(moved_empty.span().empty());
    const char input[] = "abc";
    bstream::span_streambuf reader(input, 3);
    CHECK(reader.sbumpc() == 'a');
    moved_empty = std::move(reader);
    CHECK(moved_empty.sbumpc() == 'b');
    CHECK(moved_empty.sputc('x') == std::char_traits<char>::eof());

    char output[3]{};
    bstream::span_streambuf writer(output, 3, std::ios::out);
    CHECK(writer.sputc('a') == 'a');
    bstream::span_streambuf moved_writer(std::move(writer));
    CHECK(moved_writer.sgetc() == std::char_traits<char>::eof());
    CHECK(moved_writer.sputc('b') == 'b');
    CHECK(moved_writer.chars_written() == 2);
    CHECK(std::string(output, 2) == "ab");
}

#if (defined(__unix__) || defined(__APPLE__)) && INTPTR_MAX > INT32_MAX
TEST_CASE("Memory streams seek and write beyond INT_MAX without narrowing positions") {
    // Reserve address space but touch only a few pages, avoiding a 2 GiB RAM
    // allocation while exercising real valid pointers at the boundary.
    const std::size_t boundary = std::size_t{1} << 31;
    const std::size_t size = boundary + 64;
    void* address = mmap(nullptr, size, PROT_READ | PROT_WRITE,
                         MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    REQUIRE(address != MAP_FAILED);
    const auto unmap = [](char* p) { munmap(p, size); };
    std::unique_ptr<char, decltype(unmap)> memory(static_cast<char*>(address), unmap);

    bstream::span_ostream output(memory.get(), size);
    output.seekp(static_cast<std::streamoff>(boundary));
    REQUIRE(output.good());
    REQUIRE(output.tellp() == static_cast<std::streamoff>(boundary));
    CHECK(output.chars_written() == static_cast<std::streamsize>(boundary));
    output.put('X');
    output.write("YZ", 2);
    CHECK(output.good());
    CHECK(output.chars_written() == static_cast<std::streamsize>(boundary + 3));
    CHECK(output.written_span().size() == boundary + 3);
    CHECK(std::string(memory.get() + boundary, 3) == "XYZ");

    output.seekp(-65, std::ios::end);
    REQUIRE(output.tellp() == static_cast<std::streamoff>(boundary - 1));
    output.write("AB", 2);
    CHECK(output.tellp() == static_cast<std::streamoff>(boundary + 1));
    CHECK(std::string(memory.get() + boundary - 1, 4) == "ABYZ");

    bstream::span_istream input(memory.get(), size);
    input.seekg(static_cast<std::streamoff>(boundary - 1));
    char bytes[4]{};
    input.read(bytes, sizeof bytes);
    CHECK(std::string(bytes, sizeof bytes) == "ABYZ");
    CHECK(input.tellg() == static_cast<std::streamoff>(boundary + 3));

    output.seekp(std::numeric_limits<std::streamoff>::max(), std::ios::cur);
    CHECK(output.fail());
    output.clear();
    CHECK(output.tellp() == static_cast<std::streamoff>(boundary + 1));
    input.seekg(std::numeric_limits<std::streamoff>::min(), std::ios::cur);
    CHECK(input.fail());
    input.clear();
    CHECK(input.tellg() == static_cast<std::streamoff>(boundary + 3));
}
#endif
