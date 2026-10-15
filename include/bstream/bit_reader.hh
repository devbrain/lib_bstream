#pragma once

#include <bstream/error.hh>
#include <bstream/types.hh>

#include <cstddef>
#include <cstdint>
#include <span>

namespace bstream {

    enum class bit_order {
        msb_first, // Most significant bit read first (standard in Huffman, Deflate, Amiga)
        lsb_first  // Least significant bit read first (common in LZSS variants)
    };

    class bit_reader {
    public:
        constexpr bit_reader() noexcept = default;

        constexpr explicit bit_reader(
            std::span<const uint8_t> data,
            bit_order order = bit_order::msb_first) noexcept
            : data_(data), cursor_(0), bit_buffer_(0), bits_in_buffer_(0), order_(order) {}

        constexpr explicit bit_reader(
            const void* data,
            std::size_t size,
            bit_order order = bit_order::msb_first) noexcept
            : data_(static_cast<const uint8_t*>(data), size),
              cursor_(0), bit_buffer_(0), bits_in_buffer_(0), order_(order) {}

        [[nodiscard]] constexpr bit_order order() const noexcept { return order_; }
        constexpr void set_order(bit_order o) noexcept { order_ = o; }

        [[nodiscard]] constexpr std::size_t bytes_remaining() const noexcept {
            return cursor_ < data_.size() ? (data_.size() - cursor_) : 0;
        }

        [[nodiscard]] constexpr std::size_t total_bits_remaining() const noexcept {
            return bits_in_buffer_ + (bytes_remaining() * 8);
        }

        [[nodiscard]] bool read_bit() {
            return read_bits(1) != 0;
        }

        uint64_t read_bits(unsigned int count) {
            if (count == 0) return 0;
            if (count > 64) {
                throw bstream_error("bit_reader: cannot read more than 64 bits at once");
            }

            ensure_bits(count);

            uint64_t result = 0;
            if (order_ == bit_order::msb_first) {
                result = (bit_buffer_ >> (bits_in_buffer_ - count)) & mask(count);
            } else {
                result = bit_buffer_ & mask(count);
                bit_buffer_ >>= count;
            }
            bits_in_buffer_ -= count;
            return result;
        }

        uint64_t peek_bits(unsigned int count) {
            if (count == 0) return 0;
            if (count > 64) {
                throw bstream_error("bit_reader: cannot peek more than 64 bits at once");
            }

            ensure_bits(count);

            if (order_ == bit_order::msb_first) {
                return (bit_buffer_ >> (bits_in_buffer_ - count)) & mask(count);
            } else {
                return bit_buffer_ & mask(count);
            }
        }

        void skip_bits(unsigned int count) {
            (void)read_bits(count);
        }

        void align_to_byte() noexcept {
            if (order_ == bit_order::msb_first) {
                const unsigned int remainder = bits_in_buffer_ % 8;
                bits_in_buffer_ -= remainder;
            } else {
                const unsigned int remainder = bits_in_buffer_ % 8;
                bit_buffer_ >>= remainder;
                bits_in_buffer_ -= remainder;
            }
            // Discard unread buffered bits to sync cursor to next raw byte
            bits_in_buffer_ = 0;
            bit_buffer_ = 0;
        }

    private:
        static constexpr uint64_t mask(unsigned int count) noexcept {
            if (count >= 64) return ~0ULL;
            return (1ULL << count) - 1ULL;
        }

        void ensure_bits(unsigned int count) {
            while (bits_in_buffer_ < count) {
                if (cursor_ >= data_.size()) {
                    throw eof_error("bit_reader: unexpected EOF reading bits");
                }
                const uint64_t next_byte = data_[cursor_++];
                if (order_ == bit_order::msb_first) {
                    bit_buffer_ = (bit_buffer_ << 8) | next_byte;
                } else {
                    bit_buffer_ |= (next_byte << bits_in_buffer_);
                }
                bits_in_buffer_ += 8;
            }
        }

        std::span<const uint8_t> data_{};
        std::size_t cursor_{0};
        uint64_t bit_buffer_{0};
        unsigned int bits_in_buffer_{0};
        bit_order order_{bit_order::msb_first};
    };

} // namespace bstream
