/**
 * @file bit_stream_reader.hh
 * @brief Streaming bit-field input and memory-reader aliases.
 * @ingroup bit_io
 */

#pragma once

#include <bstream/detail/platform.hh>

#include <bstream/bit_reader.hh>
#include <bstream/error.hh>
#include <bstream/detail/bit_read_buffer.hh>
#include <bstream/stream_reader.hh>
#include <bstream/types.hh>

#include <cstddef>
#include <cstdint>
#include <istream>

namespace bstream {

    /**
     * @brief Bit-field reader borrowing a standard input stream.
     * @ingroup bit_io
     *
     * The default order is msb_first, independent of a wrapped stream_reader's
     * byte order. Peeks and reads can prefetch bytes; do not read the underlying
     * stream independently while buffered bits remain. See @ref bit_guide.
     * @snippet usage.cc bit_streams
     */
    class bit_stream_reader {
    public:
        /**
         * @brief Borrow the input stream at its current position with an empty bit buffer.
         * @pre Borrowed storage and streams outlive this object and all derived views.
         * @param is Borrowed input stream; must outlive this wrapper.
         * @param order Bit order used within bytes and fields; defaults to msb_first.
         */
        explicit bit_stream_reader(
            std::istream& is,
            bit_order order = bit_order::msb_first) noexcept
            : is_(is), order_(order) {}

        /**
         * @brief Borrow the input stream at its current position with an empty bit buffer.
         * @pre Borrowed storage and streams outlive this object and all derived views.
         * @param reader Byte reader whose underlying stream is borrowed; its endian setting is not used.
         * @param order Bit order used within bytes and fields; defaults to msb_first.
         */
        explicit bit_stream_reader(
            stream_reader& reader,
            bit_order order = bit_order::msb_first) noexcept
            : is_(reader.stream()), order_(order) {}

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
                throw bstream_error("bit_stream_reader: bit order change requires byte alignment");
            }
            order_ = o;
        }

        /**
         * @brief Count buffered unread bits.
         * @return Number of buffered unread bits, including whole prefetched bytes.
         */
        [[nodiscard]] constexpr unsigned int bits_in_buffer() const noexcept {
            return buffer_.size();
        }

        /**
         * @brief Check for exhausted input. May probe the stream and set stream flags.
         * @return True if no readable input remains.
         */
        [[nodiscard]] bool eof() {
            if (buffer_.size() > 0) return false;
            return is_.eof() || is_.peek() == std::char_traits<char>::eof();
        }

        /**
         * @brief Test whether input remains.
         * @return True when input remains.
         */
        explicit operator bool() { return !eof(); }
        /**
         * @brief Negate this wrapper's boolean state.
         * @return True when the wrapper's boolean conversion is false.
         */
        bool operator!() { return eof(); }

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
                throw bstream_error("bit_stream_reader: cannot read more than 64 bits at once");
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
                throw bstream_error("bit_stream_reader: cannot peek more than 64 bits at once");
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

#if defined(BSTREAM_HAS_FAILSAFE)
        /**
         * @brief Perform read_bit() and return a value or a caught std::exception message.
         * @note Requires failsafe; the noexcept limitation described in @ref error_guide applies.
         * @return The bit, or an error string.
         */
        [[nodiscard]] expected<bool> try_read_bit() noexcept {
            try {
                return read_bit();
            } catch (const std::exception& e) {
                return failsafe::make_unexpected(std::string(e.what()));
            }
        }

        /**
         * @brief Perform read_bits(count) and return a value or a caught std::exception message.
         * @note Requires failsafe; the noexcept limitation described in @ref error_guide applies.
         * @param count Field width in bits, from zero through 64.
         * @return The field value, or an error string.
         */
        [[nodiscard]] expected<uint64_t> try_read_bits(unsigned int count) noexcept {
            try {
                return read_bits(count);
            } catch (const std::exception& e) {
                return failsafe::make_unexpected(std::string(e.what()));
            }
        }
#endif

    private:
        void ensure_bits(unsigned int count) {
            while (buffer_.size() < count) {
                const int ch = is_.get();
                if (ch == std::char_traits<char>::eof()) {
                    throw eof_error("bit_stream_reader: unexpected EOF reading bits from stream");
                }
                buffer_.append(static_cast<uint8_t>(ch));
            }
        }

        std::istream& is_;
        detail::bit_read_buffer buffer_{};
        bit_order order_{bit_order::msb_first};
    };

    /**
     * @brief Compatibility alias for the memory-based bit_reader.
     * @ingroup bit_io
     */
    using bit_span_reader = bit_reader;
    /**
     * @brief Compatibility alias for the memory-based bit_reader.
     * @ingroup bit_io
     */
    using span_bit_reader = bit_reader;

} // namespace bstream
