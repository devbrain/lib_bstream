#pragma once

#include <bstream/bit_reader.hh>
#include <bstream/error.hh>
#include <bstream/stream_writer.hh>
#include <bstream/types.hh>

#include <cstddef>
#include <cstdint>
#include <ostream>
#include <span>

namespace bstream {

    class bit_stream_writer {
    public:
        // ====================================================================
        // Constructors
        // ====================================================================
        explicit bit_stream_writer(
            std::ostream& os,
            bit_order order = bit_order::msb_first)
            : os_(os), bit_buffer_(0), bits_in_buffer_(0),
              total_bits_written_(0), order_(order) {}

        explicit bit_stream_writer(
            stream_writer& writer,
            bit_order order = bit_order::msb_first)
            : os_(writer.stream()), bit_buffer_(0), bits_in_buffer_(0),
              total_bits_written_(0), order_(order) {}

        ~bit_stream_writer() {
            try {
                flush();
            } catch (...) {
            }
        }

        bit_stream_writer(const bit_stream_writer&) = delete;
        bit_stream_writer& operator=(const bit_stream_writer&) = delete;

        // ====================================================================
        // Queries & Stream Access
        // ====================================================================
        [[nodiscard]] std::ostream& stream() noexcept { return os_; }
        [[nodiscard]] const std::ostream& stream() const noexcept { return os_; }

        [[nodiscard]] constexpr bit_order order() const noexcept { return order_; }
        constexpr void set_order(bit_order o) noexcept { order_ = o; }

        [[nodiscard]] constexpr unsigned int bits_in_buffer() const noexcept {
            return bits_in_buffer_;
        }

        [[nodiscard]] constexpr std::size_t total_bits_written() const noexcept {
            return total_bits_written_;
        }

        explicit operator bool() const { return static_cast<bool>(os_); }
        bool operator!() const { return !os_; }

        // ====================================================================
        // Bit Writing
        // ====================================================================
        void write_bit(bool bit) {
            write_bits(bit ? 1ULL : 0ULL, 1);
        }

        void write_bits(uint64_t value, unsigned int count) {
            if (count == 0) return;
            if (count > 64) {
                throw bstream_error("bit_stream_writer: cannot write more than 64 bits at once");
            }

            value &= mask(count);

            if (order_ == bit_order::msb_first) {
                if (count > 56 && bits_in_buffer_ > 0) {
                    unsigned int first_chunk = 8 - bits_in_buffer_;
                    unsigned int shift = count - first_chunk;
                    uint64_t high_bits = value >> shift;
                    push_bits_msb(high_bits, first_chunk);
                    push_bits_msb(value & mask(shift), shift);
                } else {
                    push_bits_msb(value, count);
                }
            } else {
                if (count > 56 && bits_in_buffer_ > 0) {
                    unsigned int first_chunk = 8 - bits_in_buffer_;
                    uint64_t low_bits = value & mask(first_chunk);
                    push_bits_lsb(low_bits, first_chunk);
                    push_bits_lsb(value >> first_chunk, count - first_chunk);
                } else {
                    push_bits_lsb(value, count);
                }
            }

            total_bits_written_ += count;
        }

        void align_to_byte(uint8_t pad_bit = 0) {
            if (bits_in_buffer_ == 0) return;

            const unsigned int remainder = bits_in_buffer_;
            const unsigned int pad_count = 8 - remainder;

            if (order_ == bit_order::msb_first) {
                bit_buffer_ <<= pad_count;
                if (pad_bit) {
                    bit_buffer_ |= ((1ULL << pad_count) - 1ULL);
                }
                emit_byte(static_cast<uint8_t>(bit_buffer_ & 0xFF));
            } else {
                if (pad_bit) {
                    bit_buffer_ |= (((1ULL << pad_count) - 1ULL) << remainder);
                }
                emit_byte(static_cast<uint8_t>(bit_buffer_ & 0xFF));
            }

            bit_buffer_ = 0;
            bits_in_buffer_ = 0;
        }

        void flush(uint8_t pad_bit = 0) {
            align_to_byte(pad_bit);
            os_.flush();
            if (!os_) {
                throw bstream_error("bit_stream_writer: flush failed");
            }
        }

        void write_bytes(std::span<const uint8_t> bytes, uint8_t pad_bit = 0) {
            align_to_byte(pad_bit);
            if (!bytes.empty()) {
                os_.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
                if (!os_) {
                    throw bstream_error("bit_stream_writer: write_bytes failed");
                }
                total_bits_written_ += bytes.size() * 8;
            }
        }

    private:
        static constexpr uint64_t mask(unsigned int count) noexcept {
            if (count >= 64) return ~0ULL;
            return (1ULL << count) - 1ULL;
        }

        void push_bits_msb(uint64_t value, unsigned int count) {
            bit_buffer_ = (bit_buffer_ << count) | value;
            bits_in_buffer_ += count;

            while (bits_in_buffer_ >= 8) {
                uint8_t byte = static_cast<uint8_t>((bit_buffer_ >> (bits_in_buffer_ - 8)) & 0xFF);
                emit_byte(byte);
                bits_in_buffer_ -= 8;
            }
        }

        void push_bits_lsb(uint64_t value, unsigned int count) {
            bit_buffer_ |= (value << bits_in_buffer_);
            bits_in_buffer_ += count;

            while (bits_in_buffer_ >= 8) {
                uint8_t byte = static_cast<uint8_t>(bit_buffer_ & 0xFF);
                emit_byte(byte);
                bit_buffer_ >>= 8;
                bits_in_buffer_ -= 8;
            }
        }

        void emit_byte(uint8_t b) {
            os_.put(static_cast<char>(b));
            if (!os_) {
                throw bstream_error("bit_stream_writer: failed to write byte to stream");
            }
        }

        std::ostream& os_;
        uint64_t bit_buffer_{0};
        unsigned int bits_in_buffer_{0};
        std::size_t total_bits_written_{0};
        bit_order order_{bit_order::msb_first};
    };

} // namespace bstream
