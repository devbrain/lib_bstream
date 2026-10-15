/**
 * @file bit_reader.hh
 * @brief Bit order and memory bit-field input.
 * @ingroup bit_io
 */

#pragma once

#include <bstream/detail/platform.hh>

#include <bstream/error.hh>
#include <bstream/detail/bit_read_buffer.hh>
#include <bstream/types.hh>

#include <cstddef>
#include <cstdint>
#include <span>

namespace bstream {

    /**
     * @brief Physical bit order within bytes and significance order within fields.
     * @ingroup bit_io
     * @see bit_guide
     */
    enum class bit_order {
        msb_first, ///< Read/write high remaining byte bits first; first bit is field MSB.
        lsb_first  ///< Read/write low remaining byte bits first; first bit is field LSB.
    };

    /**
     * @brief Bit-field reader over borrowed bytes with a logical bit cursor.
     * @ingroup bit_io
     *
     * Reads and peeks accept 0 through 64 bits, even across byte boundaries.
     * The default order is msb_first. Peeking can prefetch bytes without consuming
     * bits. Keep storage alive and stable; see @ref bit_guide for order semantics.
     * @snippet usage.cc bits
     */
    class bit_reader {
    public:
        /**
         * @brief Create an empty input buffer.
         */
        constexpr bit_reader() noexcept = default;

        /**
         * @brief Borrow input bytes and start at the first bit.
         * @pre Borrowed storage and streams outlive this object and all derived views.
         * @param data Borrowed contiguous input storage; must remain alive and stable.
         * @param order Bit order used within bytes and fields; defaults to msb_first.
         */
        constexpr explicit bit_reader(
            std::span<const uint8_t> data,
            bit_order order = bit_order::msb_first) noexcept
            : data_(data), cursor_(0), order_(order) {}

        /**
         * @brief Borrow input bytes and start at the first bit.
         * @pre Borrowed storage and streams outlive this object and all derived views.
         * @param data Borrowed contiguous input storage; must remain alive and stable.
         * @param size Extent of the supplied storage in bytes.
         * @param order Bit order used within bytes and fields; defaults to msb_first.
         */
        constexpr explicit bit_reader(
            const void* data,
            std::size_t size,
            bit_order order = bit_order::msb_first) noexcept
            : data_(static_cast<const uint8_t*>(data), size),
              cursor_(0), order_(order) {}

        /**
         * @brief Return the current bit order.
         * @return The current bit order.
         */
        [[nodiscard]] constexpr bit_order order() const noexcept { return order_; }
        // Order changes require a byte boundary; peeking does not consume bits.
        /**
         * @brief Change bit order at a byte boundary. Setting the same order is always allowed.
         * @throws bstream_error If the order changes while partway through a byte.
         * @param o Requested bit order; changing it requires a byte boundary.
         */
        constexpr void set_order(bit_order o) {
            if (o != order_ && !buffer_.aligned()) {
                throw bstream_error("bit_reader: bit order change requires byte alignment");
            }
            order_ = o;
        }

        /**
         * @brief Count backing bytes that have not yet been fetched into the bit buffer.
         * @return Unfetched bytes, excluding buffered bits.
         */
        [[nodiscard]] constexpr std::size_t bytes_remaining() const noexcept {
            return cursor_ < data_.size() ? (data_.size() - cursor_) : 0;
        }

        /**
         * @brief Count unread bits, including prefetched bits and all remaining backing bytes.
         * @return Total available bits.
         */
        [[nodiscard]] constexpr std::size_t total_bits_remaining() const noexcept {
            return buffer_.size() + (bytes_remaining() * 8);
        }

        /**
         * @brief Consume one bit.
         * @throws eof_error If no input bit is available.
         * @return True for one, false for zero.
         */
        [[nodiscard]] bool read_bit() {
            return read_bits(1) != 0;
        }

        /**
         * @brief Consume a field of 0 to 64 bits.
         * @throws bstream_error If count exceeds 64.
         * @throws eof_error If insufficient bits remain; fetched bytes can stay buffered.
         * @param count Field width in bits, from zero through 64.
         * @return The right-aligned field value; zero when count is zero.
         */
        uint64_t read_bits(unsigned int count) {
            if (count == 0) return 0;
            if (count > 64) {
                throw bstream_error("bit_reader: cannot read more than 64 bits at once");
            }

            ensure_bits(count);

            const uint64_t result = buffer_.peek(count, order_ == bit_order::msb_first);
            buffer_.discard(count);
            return result;
        }

        /**
         * @brief Read a field without consuming logical bits; may prefetch underlying bytes.
         * @throws bstream_error If count exceeds 64.
         * @throws eof_error If insufficient bits remain.
         * @param count Field width in bits, from zero through 64.
         * @return The right-aligned field value; zero when count is zero.
         */
        uint64_t peek_bits(unsigned int count) {
            if (count == 0) return 0;
            if (count > 64) {
                throw bstream_error("bit_reader: cannot peek more than 64 bits at once");
            }

            ensure_bits(count);

            return buffer_.peek(count, order_ == bit_order::msb_first);
        }

        /**
         * @brief Consume and discard 0 to 64 bits.
         * @throws bstream_error If count exceeds 64.
         * @throws eof_error If insufficient bits remain.
         * @param count Field width in bits, from zero through 64.
         */
        void skip_bits(unsigned int count) {
            (void)read_bits(count);
        }

        /**
         * @brief Discard only the remaining bits in the current byte, preserving prefetched whole bytes.
         */
        void align_to_byte() noexcept {
            buffer_.align_to_byte();
        }

    private:
        void ensure_bits(unsigned int count) {
            while (buffer_.size() < count) {
                if (cursor_ >= data_.size()) {
                    throw eof_error("bit_reader: unexpected EOF reading bits");
                }
                buffer_.append(data_[cursor_++]);
            }
        }

        std::span<const uint8_t> data_{};
        std::size_t cursor_{0};
        detail::bit_read_buffer buffer_{};
        bit_order order_{bit_order::msb_first};
    };

} // namespace bstream
