#include <bstream/bstream.hh>

#include <array>
#include <cstdint>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
    // Keep example checks active in Release builds as well as Debug builds.
    void require(bool condition) {
        if (!condition) throw std::runtime_error("Documentation example failed");
    }

    void spans() {
        //! [span_roundtrip]
        std::array<uint8_t, 64> storage{};
        bstream::span_writer writer(storage, bstream::endian::big);
        writer.write<uint32_t>(0x12345678);
        writer.write_pascal_string("hello"); // uint8_t length prefix

        bstream::span_reader reader(writer.written_span(), bstream::endian::big);
        require(reader.peek<uint32_t>() == 0x12345678); // does not consume
        require(reader.read<uint32_t>() == 0x12345678);
        require(reader.read_pascal_string() == "hello");
        require(reader.eof());
        //! [span_roundtrip]
    }

    void slicing() {
        //! [slicing]
        const std::array<uint8_t, 6> packet{0x00, 0x04, 10, 20, 30, 40};
        bstream::span_reader reader(packet, bstream::endian::big);
        const auto length = reader.read<uint16_t>();
        auto payload = reader.subreader(length); // advances parent by length
        const auto bytes = payload.read_span(2); // view into packet, no copy
        require(bytes[0] == 10 && bytes[1] == 20);
        require(payload.remaining() == 2 && reader.eof());
        //! [slicing]
    }

    void streams() {
        //! [streams]
        std::stringstream transport;
        bstream::stream_writer writer(transport, bstream::endian::little);
        writer.write_fixed_string("OK", 4); // pads with two zero bytes
        writer << std::array<uint16_t, 3>{10, 20, 30}; // no container length
        writer.flush();

        bstream::stream_reader reader(transport, bstream::endian::little);
        require(reader.read_fixed_string(4) == "OK");
        const auto values = reader.read_vector<uint16_t>(3);
        require(values == std::vector<uint16_t>{10, 20, 30});
        //! [streams]
    }

    void byte_order() {
        //! [byte_order]
        static_assert(bstream::flip_bytes(uint32_t{0x12345678}) == 0x78563412U);
        const auto network = bstream::to_network(uint16_t{0x1234});
        require(bstream::from_network(network) == 0x1234);

        std::array<uint16_t, 2> values{0x1234, 0x5678};
        bstream::byte_order::swap_bytes(std::span<uint16_t>(values));
        require(values[0] == 0x3412 && values[1] == 0x7856);
        //! [byte_order]
    }

    //! [custom_type]
    struct Record {
        Record() : code(0) {} // non-aggregate: selects custom operators
        uint16_t code;

        template<typename Reader>
        friend Reader& operator>>(Reader& reader, Record& value) {
            return reader >> value.code;
        }
        template<typename Writer>
        friend Writer& operator<<(Writer& writer, const Record& value) {
            return writer << value.code;
        }
    };
    //! [custom_type]

    void custom_records() {
        //! [custom_records]
        std::array<uint8_t, 2> storage{};
        Record record;
        record.code = 0x1234;
        bstream::span_writer writer(storage); // configured little endian
        writer.write<Record, bstream::endian::big>(record);
        require(storage[0] == 0x12 && storage[1] == 0x34);
        require(writer.byte_order() == bstream::endian::little); // restored

        bstream::span_reader reader(writer.written_span());
        const auto decoded = reader.read<Record, bstream::endian::big>();
        require(decoded.code == record.code);
        //! [custom_records]
    }

#if defined(BSTREAM_HAS_PFR)
    void aggregates() {
        //! [aggregates]
        struct Header { uint32_t magic; uint16_t version; };
        std::array<uint8_t, 6> storage{}; // serialized fields, no struct padding
        bstream::span_writer writer(storage, bstream::endian::big);
        writer.write(Header{0x12345678, 2});
        bstream::span_reader reader(writer.written_span(), bstream::endian::big);
        const auto header = reader.read<Header>();
        require(header.magic == 0x12345678 && header.version == 2);
        //! [aggregates]
    }
#endif

    void bits() {
        //! [bits]
        bstream::bit_writer writer(bstream::bit_order::msb_first);
        writer.write_bits(5, 3);       // 101
        writer.write_bits(0x2ab, 10); // 1010101011
        const auto bytes = writer.finish(); // pad final byte with zero bits
        require(bytes.size() == 2);

        bstream::bit_reader reader(bytes, bstream::bit_order::msb_first);
        require(reader.peek_bits(3) == 5);
        require(reader.read_bits(3) == 5);
        require(reader.read_bits(10) == 0x2ab);
        reader.align_to_byte(); // discard the three padding bits
        reader.set_order(bstream::bit_order::lsb_first); // now at a boundary
        require(reader.total_bits_remaining() == 0);
        //! [bits]
    }

    void bit_streams() {
        //! [bit_streams]
        std::stringstream transport;
        bstream::bit_stream_writer writer(transport, bstream::bit_order::lsb_first);
        writer.write_bits(3, 2);
        writer.write_bits(5, 3);
        writer.flush(); // check errors explicitly; destructor suppresses them

        bstream::bit_stream_reader reader(transport, bstream::bit_order::lsb_first);
        require(reader.read_bits(2) == 3);
        require(reader.read_bits(3) == 5);
        reader.align_to_byte();
        require(reader.eof());
        //! [bit_streams]
    }

    void memory_streams() {
        //! [memory_streams]
        std::array<uint8_t, 16> storage{};
        bstream::span_ostream output{std::span<uint8_t>(storage)};
        bstream::stream_writer writer(output, bstream::endian::big);
        writer.write<uint16_t>(0x1234);
        writer.flush();

        bstream::span_istream input(output.written_span());
        bstream::stream_reader reader(input, bstream::endian::big);
        require(reader.read<uint16_t>() == 0x1234);
        require(output.chars_written() == 2);
        //! [memory_streams]
    }

    void tees_and_copying() {
        //! [tee_output]
        std::ostringstream first, second;
        bstream::tee_output_stream output(first);
        output.add_stream(second);
        output << "hello";
        output.flush();
        require(output.good() && first.str() == second.str());
        //! [tee_output]

        //! [tee_input]
        std::istringstream source("payload");
        std::ostringstream audit, destination;
        bstream::tee_input_stream input(source);
        input.add_stream(audit);
        const auto copied = bstream::stream_copier::copy(input, destination);
        require(copied == 7 && destination.str() == "payload");
        require(audit.str() == destination.str());
        //! [tee_input]
    }

    void errors() {
        //! [errors]
        const std::array<uint8_t, 1> truncated{0x12};
        bstream::span_reader reader(truncated);
        bool caught = false;
        try {
            (void)reader.read<uint32_t>();
        } catch (const bstream::eof_error&) {
            caught = true;
        }
        require(caught && reader.tell() == 0);
        //! [errors]

#if defined(BSTREAM_HAS_FAILSAFE)
        //! [expected]
        const auto result = reader.try_read<uint32_t>();
        require(!result.has_value());
        require(!result.error().empty()); // exception message as std::string
        //! [expected]
#endif
    }
}

int main() {
    spans();
    slicing();
    streams();
    byte_order();
    custom_records();
#if defined(BSTREAM_HAS_PFR)
    aggregates();
#endif
    bits();
    bit_streams();
    memory_streams();
    tees_and_copying();
    errors();
}
