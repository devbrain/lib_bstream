#pragma once

#include <bstream/bit_reader.hh>
#include <bstream/error.hh>
#include <bstream/span_writer.hh>
#include <bstream/types.hh>

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace bstream {

    class bit_writer {
    public:
        // ====================================================================
        // Constructors
        // ====================================================================

        // Owning constructor (writes to internal std::vector)
        explicit bit_writer(bit_order order = bit_order::msb_first)
            : owning_buf_(), vec_(&owning_buf_), span_(), writer_(nullptr),
              bit_buffer_(0), bits_in_buffer_(0), total_bits_written_(0), order_(order) {}

        // Target std::vector constructor
        explicit bit_writer(std::vector<uint8_t>& target, bit_order order = bit_order::msb_first)
            : owning_buf_(), vec_(&target), span_(), writer_(nullptr),
              bit_buffer_(0), bits_in_buffer_(0), total_bits_written_(0), order_(order) {}

        // Target fixed span constructor
        explicit bit_writer(std::span<uint8_t> target, bit_order order = bit_order::msb_first)
            : owning_buf_(), vec_(nullptr), span_(target), writer_(nullptr),
              bit_buffer_(0), bits_in_buffer_(0), total_bits_written_(0), order_(order) {}

        // Target span_writer constructor
        explicit bit_writer(span_writer& writer, bit_order order = bit_order::msb_first)
            : owning_buf_(), vec_(nullptr), span_(), writer_(&writer),
              bit_buffer_(0), bits_in_buffer_(0), total_bits_written_(0), order_(order) {}

        ~bit_writer() {
            try {
                flush();
            } catch (...) {
            }
        }

        bit_writer(const bit_writer&) = delete;
        bit_writer& operator=(const bit_writer&) = delete;

        bit_writer(bit_writer&& other) noexcept
            : owning_buf_(std::move(other.owning_buf_)),
              vec_(other.vec_ == &other.owning_buf_ ? &owning_buf_ : other.vec_),
              span_(other.span_),
              span_cursor_(other.span_cursor_),
              writer_(other.writer_),
              bit_buffer_(other.bit_buffer_),
              bits_in_buffer_(other.bits_in_buffer_),
              total_bits_written_(other.total_bits_written_),
              order_(other.order_) {
            other.vec_ = nullptr;
            other.span_ = {};
            other.writer_ = nullptr;
            other.bits_in_buffer_ = 0;
            other.bit_buffer_ = 0;
        }

        bit_writer& operator=(bit_writer&& other) noexcept {
            if (this != &other) {
                try {
                    flush();
                } catch (...) {
                }
                owning_buf_ = std::move(other.owning_buf_);
                vec_ = (other.vec_ == &other.owning_buf_ ? &owning_buf_ : other.vec_);
                span_ = other.span_;
                span_cursor_ = other.span_cursor_;
                writer_ = other.writer_;
                bit_buffer_ = other.bit_buffer_;
                bits_in_buffer_ = other.bits_in_buffer_;
                total_bits_written_ = other.total_bits_written_;
                order_ = other.order_;

                other.vec_ = nullptr;
                other.span_ = {};
                other.writer_ = nullptr;
                other.bits_in_buffer_ = 0;
                other.bit_buffer_ = 0;
            }
            return *this;
        }

        // ====================================================================
        // Queries
        // ====================================================================
        [[nodiscard]] constexpr bit_order order() const noexcept { return order_; }
        constexpr void set_order(bit_order o) noexcept { order_ = o; }

        [[nodiscard]] constexpr unsigned int bits_in_buffer() const noexcept {
            return bits_in_buffer_;
        }

        [[nodiscard]] constexpr std::size_t total_bits_written() const noexcept {
            return total_bits_written_;
        }

        [[nodiscard]] std::span<const uint8_t> data() const noexcept {
            if (vec_) {
                return *vec_;
            }
            if (writer_) {
                return writer_->written_span();
            }
            return span_.subspan(0, span_cursor_);
        }

        // ====================================================================
        // Bit Writing
        // ====================================================================
        void write_bit(bool bit) {
            write_bits(bit ? 1ULL : 0ULL, 1);
        }

        void write_bits(uint64_t value, unsigned int count) {
            if (count == 0) return;
            if (count > 64) {
                throw bstream_error("bit_writer: cannot write more than 64 bits at once");
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
        }

        void write_bytes(std::span<const uint8_t> bytes, uint8_t pad_bit = 0) {
            align_to_byte(pad_bit);
            for (uint8_t b : bytes) {
                emit_byte(b);
            }
            total_bits_written_ += bytes.size() * 8;
        }

        // Finalize writing and return buffer (for owning mode)
        std::vector<uint8_t> finish(uint8_t pad_bit = 0) {
            flush(pad_bit);
            return std::move(owning_buf_);
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
            if (vec_) {
                vec_->push_back(b);
            } else if (writer_) {
                writer_->write<uint8_t>(b);
            } else if (span_.data()) {
                if (span_cursor_ >= span_.size()) {
                    throw eof_error("bit_writer: buffer overflow");
                }
                span_[span_cursor_++] = b;
            } else {
                throw bstream_error("bit_writer: no valid output destination");
            }
        }

        std::vector<uint8_t> owning_buf_;
        std::vector<uint8_t>* vec_{nullptr};
        std::span<uint8_t> span_{};
        std::size_t span_cursor_{0};
        span_writer* writer_{nullptr};

        uint64_t bit_buffer_{0};
        unsigned int bits_in_buffer_{0};
        std::size_t total_bits_written_{0};
        bit_order order_{bit_order::msb_first};
    };

} // namespace bstream
