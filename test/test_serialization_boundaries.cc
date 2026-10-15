#include <doctest/doctest.h>
#include <bstream/bstream.hh>

#include <array>
#include <sstream>
#include <string_view>

TEST_CASE("Generic array serialization uses container elements with either PFR configuration") {
    using Values = std::array<std::array<uint16_t, 2>, 2>;
    using Empty = std::array<uint16_t, 0>;
    const Values expected{{{0x1234, 0x5678}, {0x9abc, 0xdef0}}};
    for (const auto order : {bstream::endian::little, bstream::endian::big}) {
        const std::array<uint8_t, 8> wire = order == bstream::endian::little
            ? std::array<uint8_t, 8>{0x34, 0x12, 0x78, 0x56, 0xbc, 0x9a, 0xf0, 0xde}
            : std::array<uint8_t, 8>{0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0};
        std::array<uint8_t, 8> output{};
        bstream::span_writer writer(output, order);
        writer.write(Empty{});
        writer.write(expected);
        CHECK(output == wire);
        CHECK(writer.tell() == wire.size());

        bstream::span_reader reader(wire, order);
        CHECK(reader.read<Empty>().empty());
        CHECK(reader.read<Values>() == expected);
        CHECK(reader.eof());

        const std::string wire_string(reinterpret_cast<const char*>(wire.data()), wire.size());
        std::ostringstream stream;
        bstream::stream_writer stream_writer(stream, order);
        stream_writer.write(Empty{});
        stream_writer.write(expected);
        CHECK(stream.str() == wire_string);

        std::istringstream input(wire_string);
        bstream::stream_reader stream_reader(input, order);
        CHECK(stream_reader.read<Empty>().empty());
        CHECK(stream_reader.read<Values>() == expected);
        CHECK(stream_reader.tell() == wire.size());
    }
}

TEST_CASE("Empty string fields handle null-backed views without accessing memory") {
    bstream::span_writer empty_writer;
    CHECK_NOTHROW(empty_writer.write_fixed_string(std::string_view{}, 0));
    CHECK_NOTHROW(empty_writer.write_fixed_string("ignored", 0));
    CHECK(empty_writer.tell() == 0);

    std::array<uint8_t, 8> storage{};
    storage.fill(0xff);
    bstream::span_writer writer(storage);
    writer.write<uint8_t>(0x12);
    writer.write_fixed_string(std::string_view{}, 3, 'x');
    writer.write_fixed_string(std::string_view{}, 2);
    CHECK(writer.tell() == 6);
    CHECK(storage == std::array<uint8_t, 8>{0x12, 'x', 'x', 'x', 0, 0, 0xff, 0xff});

    bstream::span_reader empty_reader;
    CHECK(empty_reader.current_ptr() == nullptr);
    CHECK(empty_reader.read_span(0).empty());
    CHECK(empty_reader.read_fixed_string(0).empty());
    CHECK(empty_reader.read_fixed_string(0, false).empty());
    CHECK_THROWS_AS((void)empty_reader.read_cstring(), bstream::eof_error);
    CHECK(empty_reader.tell() == 0);

    const std::vector<uint8_t> empty_storage;
    bstream::span_reader container_reader(empty_storage);
    CHECK_THROWS_AS((void)container_reader.read_cstring(), bstream::eof_error);
    CHECK(container_reader.tell() == 0);

    const uint8_t terminator = 0;
    bstream::span_reader reader(&terminator, 1);
    CHECK(reader.read_cstring().empty());
    CHECK_THROWS_AS((void)reader.read_cstring(), bstream::eof_error);
    CHECK(reader.tell() == 1);
}

namespace {
    struct CustomRecord {
        CustomRecord() : value(0) {} // Exercise custom operators without PFR.
        uint16_t value;

        template<typename Reader>
        friend Reader& operator>>(Reader& reader, CustomRecord& record) {
            return reader >> record.value;
        }
        template<typename Writer>
        friend Writer& operator<<(Writer& writer, const CustomRecord& record) {
            return writer << record.value;
        }
    };

    template<typename Record>
    void check_explicit_endian(Record record, const std::string& big_endian_bytes) {
        std::array<uint8_t, 64> storage{};
        bstream::span_writer writer(storage);
        writer.template write<Record, bstream::endian::big>(record);
        CHECK(std::string(reinterpret_cast<const char*>(storage.data()), writer.tell()) == big_endian_bytes);
        CHECK(writer.byte_order() == bstream::endian::little);
        writer.template write<uint16_t>(0x1234);
        CHECK(storage[big_endian_bytes.size()] == 0x34);

        bstream::span_reader reader(writer.written_span());
        const auto decoded = reader.template read<Record, bstream::endian::big>();
        CHECK(decoded.value == record.value);
        CHECK(reader.byte_order() == bstream::endian::little);
        CHECK(reader.template read<uint16_t>() == 0x1234);

        std::stringstream stream;
        bstream::stream_writer sw(stream);
        sw.template write<Record, bstream::endian::big>(record);
        CHECK(stream.str() == big_endian_bytes);
        CHECK(sw.byte_order() == bstream::endian::little);
        sw.template write<uint16_t>(0x1234);
        bstream::stream_reader sr(stream);
        CHECK((sr.template read<Record, bstream::endian::big>().value == record.value));
        CHECK(sr.byte_order() == bstream::endian::little);
        CHECK(sr.template read<uint16_t>() == 0x1234);

        uint8_t short_buffer[1]{};
        bstream::span_reader short_reader(short_buffer, sizeof short_buffer);
        CHECK_THROWS_AS((void)(short_reader.template read<Record, bstream::endian::big>()), bstream::eof_error);
        CHECK(short_reader.byte_order() == bstream::endian::little);
        bstream::span_writer short_writer(short_buffer, sizeof short_buffer);
        CHECK_THROWS_AS((short_writer.template write<Record, bstream::endian::big>(record)), bstream::eof_error);
        CHECK(short_writer.byte_order() == bstream::endian::little);

        bstream::span_istream short_input(short_buffer, sizeof short_buffer);
        bstream::stream_reader short_sr(short_input);
        CHECK_THROWS_AS((void)(short_sr.template read<Record, bstream::endian::big>()), bstream::eof_error);
        CHECK(short_sr.byte_order() == bstream::endian::little);
        bstream::span_ostream short_output(short_buffer, sizeof short_buffer);
        bstream::stream_writer short_sw(short_output);
        CHECK_THROWS_AS((short_sw.template write<Record, bstream::endian::big>(record)), bstream::bstream_error);
        CHECK(short_sw.byte_order() == bstream::endian::little);
    }

    template<typename Writer>
    void write_pascal_boundaries(Writer& writer) {
        writer.template write<uint8_t>(0x5a);
        CHECK_THROWS_AS(writer.template write_pascal_string<uint8_t>(std::string(256, 'x')), bstream::parse_error);
        CHECK(writer.tell() == 1);
        CHECK_THROWS_AS(writer.template write_pascal_string<int8_t>(std::string(128, 'x')), bstream::parse_error);
        CHECK(writer.tell() == 1);
        CHECK_THROWS_AS(writer.template write_pascal_string<uint16_t>(std::string(65536, 'x')), bstream::parse_error);
        CHECK(writer.tell() == 1);
        writer.write_pascal_string("");
        writer.write_pascal_string("Hi");
        writer.template write_pascal_string<uint8_t>(std::string(255, 'a'));
        writer.template write_pascal_string<int8_t>(std::string(127, 'b'));
        writer.template write_pascal_string<uint16_t>(std::string(256, 'c'));
    }

    template<typename Reader>
    void read_pascal_boundaries(Reader& reader) {
        CHECK(reader.template read<uint8_t>() == 0x5a);
        CHECK(reader.read_pascal_string() == "");
        CHECK(reader.read_pascal_string() == "Hi");
        CHECK(reader.template read_pascal_string<uint8_t>() == std::string(255, 'a'));
        CHECK(reader.template read_pascal_string<int8_t>() == std::string(127, 'b'));
        CHECK(reader.template read_pascal_string<uint16_t>() == std::string(256, 'c'));
        CHECK(reader.eof());
    }
}

TEST_CASE("Explicit endian overrides custom fields and restores order after exceptions") {
    CustomRecord record;
    record.value = 0x1234;
    check_explicit_endian(record, std::string("\x12\x34", 2));
}

#if defined(BSTREAM_HAS_PFR)
TEST_CASE("Explicit endian overrides all nested aggregate fields") {
    struct Nested { uint16_t field; };
    struct Record { uint16_t value; Nested nested; };
    check_explicit_endian(Record{0x1234, {0x5678}}, std::string("\x12\x34\x56\x78", 4));
    const uint8_t bytes[] = {0x12, 0x34, 0x56, 0x78};
    bstream::span_reader reader(bytes, sizeof bytes);
    CHECK((reader.read<Record, bstream::endian::big>().nested.field == 0x5678));
    bstream::span_istream stream(bytes, sizeof bytes);
    bstream::stream_reader sr(stream);
    CHECK((sr.read<Record, bstream::endian::big>().nested.field == 0x5678));
}
#endif

TEST_CASE("Pascal strings share defaults and reject unrepresentable lengths before writing") {
    for (auto order : {bstream::endian::little, bstream::endian::big}) {
        std::array<uint8_t, 1024> storage{};
        bstream::span_writer writer(storage, order);
        write_pascal_boundaries(writer);
        bstream::span_reader reader(writer.written_span(), order);
        read_pascal_boundaries(reader);
        bstream::span_istream input(writer.written_span());
        bstream::stream_reader sr(input, order);
        read_pascal_boundaries(sr);

        std::stringstream stream;
        bstream::stream_writer sw(stream, order);
        write_pascal_boundaries(sw);
        const auto bytes = stream.str();
        CHECK(bytes.size() == writer.tell());
        CHECK(std::equal(writer.written_span().begin(), writer.written_span().end(),
                         reinterpret_cast<const uint8_t*>(bytes.data())));
        bstream::stream_reader stream_reader(stream, order);
        read_pascal_boundaries(stream_reader);
        bstream::span_reader span_reader(bytes, order);
        read_pascal_boundaries(span_reader);
    }
}

TEST_CASE("Container span readers expose the full byte extent") {
    const std::array<uint32_t, 2> words = {0x12345678, 0x90abcdef};
    const std::vector<uint32_t> vector(words.begin(), words.end());
    const auto check = [&](const auto& container) {
        bstream::span_reader reader(container, bstream::endian::native);
        CHECK(reader.size() == sizeof words);
        CHECK(reader.remaining() == sizeof words);
        CHECK(reader.read<uint32_t>() == words[0]);
        CHECK(reader.read<uint32_t>() == words[1]);
        CHECK(reader.eof());
    };
    check(words);
    check(vector);
    check(std::span<const uint32_t>(words));
    const std::vector<uint32_t> empty;
    CHECK(bstream::span_reader(empty).size() == 0);

    const std::array<uint8_t, 2> bytes = {1, 2};
    CHECK(bstream::span_reader(bytes).size() == 2);
    const std::array<std::byte, 2> byte_array{};
    CHECK(bstream::span_reader(byte_array).size() == 2);
    const std::string text = "abc";
    CHECK(bstream::span_reader(text).size() == 3);
}
