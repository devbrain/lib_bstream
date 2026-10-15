#pragma once

#include <bstream/bit_reader.hh>
#include <bstream/error.hh>
#include <bstream/stream_reader.hh>
#include <bstream/types.hh>

#include <cstddef>
#include <cstdint>
#include <istream>

namespace bstream {

    class bit_stream_reader {
    public:
        explicit bit_stream_reader(
            std::istream& is,
            bit_order order = bit_order::msb_first) noexcept
            : is_(is), bit_buffer_(0), bits_in_buffer_(0), order_(order) {}

        explicit bit_stream_reader(
            stream_reader& reader,
            bit_order order = bit_order::msb_first) noexcept
            : is_(reader.stream()), bit_buffer_(0), bits_in_buffer_(0), order_(order) {}

        [[nodiscard]] constexpr bit_order order() const noexcept { return order_; }
        constexpr void set_order(bit_order o) noexcept { order_ = o; }

        [[nodiscard]] constexpr unsigned int bits_in_buffer() const noexcept {
            return bits_in_buffer_;
        }

        [[nodiscard]] bool eof() {
            if (bits_in_buffer_ > 0) return false;
            return is_.eof() || is_.peek() == std::char_traits<char>::eof();
        }

        explicit operator bool() { return !eof(); }
        bool operator!() { return eof(); }

        [[nodiscard]] bool read_bit() {
            return read_bits(1) != 0;
        }

        uint64_t read_bits(unsigned int count) {
            if (count == 0) return 0;
            if (count > 64) {
                throw bstream_error("bit_stream_reader: cannot read more than 64 bits at once");
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
                throw bstream_error("bit_stream_reader: cannot peek more than 64 bits at once");
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
            bits_in_buffer_ = 0;
            bit_buffer_ = 0;
        }

#if defined(BSTREAM_HAS_FAILSAFE)
        [[nodiscard]] expected<bool> try_read_bit() noexcept {
            try {
                return read_bit();
            } catch (const std::exception& e) {
                return failsafe::make_unexpected(std::string(e.what()));
            }
        }

        [[nodiscard]] expected<uint64_t> try_read_bits(unsigned int count) noexcept {
            try {
                return read_bits(count);
            } catch (const std::exception& e) {
                return failsafe::make_unexpected(std::string(e.what()));
            }
        }
#endif

    private:
        static constexpr uint64_t mask(unsigned int count) noexcept {
            if (count >= 64) return ~0ULL;
            return (1ULL << count) - 1ULL;
        }

        void ensure_bits(unsigned int count) {
            while (bits_in_buffer_ < count) {
                const int ch = is_.get();
                if (ch == std::char_traits<char>::eof()) {
                    throw eof_error("bit_stream_reader: unexpected EOF reading bits from stream");
                }
                const auto next_byte = static_cast<uint64_t>(static_cast<uint8_t>(ch));
                if (order_ == bit_order::msb_first) {
                    bit_buffer_ = (bit_buffer_ << 8) | next_byte;
                } else {
                    bit_buffer_ |= (next_byte << bits_in_buffer_);
                }
                bits_in_buffer_ += 8;
            }
        }

        std::istream& is_;
        uint64_t bit_buffer_{0};
        unsigned int bits_in_buffer_{0};
        bit_order order_{bit_order::msb_first};
    };

    using bit_span_reader = bit_reader;
    using span_bit_reader = bit_reader;

} // namespace bstream
