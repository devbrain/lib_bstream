/**
 * @file bit_stream_writer.hh
 * @brief Streaming bit-field output.
 * @ingroup bit_io
 */

#pragma once

#include <bstream/detail/platform.hh>

#include <bstream/bit_reader.hh>
#include <bstream/error.hh>
#include <bstream/stream_writer.hh>
#include <bstream/types.hh>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <ostream>
#include <span>

namespace bstream {

    /**
     * @brief Bit-field writer borrowing a standard output stream.
     * @ingroup bit_io
     *
     * The default order is msb_first, independent of a wrapped stream_writer's
     * byte order. A partial byte stays buffered until alignment or flushing.
     * Call flush() explicitly to observe failures; the destructor suppresses them.
     * Directly interleaving writes requires first flushing this writer.
     * @snippet usage.cc bit_streams
     */
    class bit_stream_writer {
    public:
        // ====================================================================
        // Constructors
        // ====================================================================
        /**
         * @brief Borrow the output stream at its current position with no pending bits.
         * @pre Borrowed storage and streams outlive this object and all derived views.
         * @param os Borrowed output stream; must outlive this wrapper.
         * @param order Bit order used within bytes and fields; defaults to msb_first.
         */
        explicit bit_stream_writer(
            std::ostream& os,
            bit_order order = bit_order::msb_first)
            : os_(os), bit_buffer_(0), bits_in_buffer_(0),
              total_bits_written_(0), order_(order) {}

        /**
         * @brief Borrow the output stream at its current position with no pending bits.
         * @pre Borrowed storage and streams outlive this object and all derived views.
         * @param writer Writer whose underlying stream is borrowed; that stream must outlive this wrapper.
         * @param order Bit order used within bytes and fields; defaults to msb_first.
         */
        explicit bit_stream_writer(
            stream_writer& writer,
            bit_order order = bit_order::msb_first)
            : os_(writer.stream()), bit_buffer_(0), bits_in_buffer_(0),
              total_bits_written_(0), order_(order) {}

        /**
         * @brief Destroy the wrapper without releasing borrowed storage.
         * @note Attempts to flush pending output, suppressing all failures. Flush explicitly to observe errors.
         */
        ~bit_stream_writer() {
            try {
                flush();
            } catch (...) {
            }
        }

        /**
         * @brief Copy construction is disabled; this object carries cursor or output state.
         */
        bit_stream_writer(const bit_stream_writer&) = delete;
        /**
         * @brief Copy assignment is disabled.
         */
        bit_stream_writer& operator=(const bit_stream_writer&) = delete;

        // ====================================================================
        // Queries & Stream Access
        // ====================================================================
        /**
         * @brief Access the borrowed underlying standard stream.
         * @return A reference to the same underlying stream.
         */
        [[nodiscard]] std::ostream& stream() noexcept { return os_; }
        /**
         * @brief Access the borrowed underlying standard stream.
         * @return A reference to the same underlying stream.
         */
        [[nodiscard]] const std::ostream& stream() const noexcept { return os_; }

        /**
         * @brief Return the current bit order.
         * @return The current bit order.
         */
        [[nodiscard]] constexpr bit_order order() const noexcept { return order_; }
        // Finish the current byte before changing its bit order.
        /**
         * @brief Change bit order at a byte boundary. Setting the same order is always allowed.
         * @throws bstream_error If the order changes while partway through a byte.
         * @param o Requested bit order; changing it requires a byte boundary.
         */
        constexpr void set_order(bit_order o) {
            if (o != order_ && bits_in_buffer_ != 0) {
                throw bstream_error("bit_stream_writer: bit order change requires byte alignment");
            }
            order_ = o;
        }

        /**
         * @brief Count pending bits not yet emitted as a complete byte.
         * @return The pending partial-byte size, from zero to seven.
         */
        [[nodiscard]] constexpr unsigned int bits_in_buffer() const noexcept {
            return bits_in_buffer_;
        }

        /**
         * @brief Count accepted payload bits and successful byte writes, excluding padding.
         * @note Failed bulk stream byte writes do not increment this counter even if the destination accepted a prefix.
         * @return The cumulative payload-bit count.
         */
        [[nodiscard]] constexpr std::size_t total_bits_written() const noexcept {
            return total_bits_written_;
        }

        /**
         * @brief Query the underlying stream success state.
         * @return The underlying stream's boolean state.
         */
        explicit operator bool() const { return static_cast<bool>(os_); }
        /**
         * @brief Negate this wrapper's boolean state.
         * @return True when the wrapper's boolean conversion is false.
         */
        bool operator!() const { return !os_; }

        // ====================================================================
        // Bit Writing
        // ====================================================================
        /**
         * @brief Append a single bit; a completed byte is emitted immediately.
         * @throws bstream_error If emitting a completed byte fails.
         * @param bit The single bit to write.
         */
        void write_bit(bool bit) {
            write_bits(bit ? 1ULL : 0ULL, 1);
        }

        /**
         * @brief Append the low count bits in the selected order.
         * @throws bstream_error If count exceeds 64 or output fails.
         * @note Count zero is a no-op. Failure may leave an accepted prefix and valid pending bits; no rollback is performed.
         * @param value Value whose low count bits are written.
         * @param count Field width in bits, from zero through 64.
         */
        void write_bits(uint64_t value, unsigned int count) {
            if (count == 0) return;
            if (count > 64) {
                throw bstream_error("bit_stream_writer: cannot write more than 64 bits at once");
            }

            unsigned int remaining = count;
            while (remaining > 0) {
                const unsigned int chunk = std::min(8 - bits_in_buffer_, remaining);
                const unsigned int shift = order_ == bit_order::msb_first
                    ? remaining - chunk : count - remaining;
                const uint64_t bits = (value >> shift) & ((1U << chunk) - 1U);
                const uint64_t next = order_ == bit_order::msb_first
                    ? (bit_buffer_ << chunk) | bits
                    : bit_buffer_ | (bits << bits_in_buffer_);
                if (bits_in_buffer_ + chunk == 8) {
                    // Commit only after output succeeds. On failure the pending
                    // partial byte remains valid, including during destruction.
                    emit_byte(static_cast<uint8_t>(next));
                    bit_buffer_ = 0;
                    bits_in_buffer_ = 0;
                } else {
                    bit_buffer_ = next;
                    bits_in_buffer_ += chunk;
                }
                total_bits_written_ += chunk;
                remaining -= chunk;
            }
        }

        /**
         * @brief Pad and emit a pending partial byte, or do nothing if already aligned.
         * @throws bstream_error If output fails; pending state remains valid.
         * @param pad_bit Zero for zero padding; any nonzero value pads with one bits.
         */
        void align_to_byte(uint8_t pad_bit = 0) {
            if (bits_in_buffer_ == 0) return;

            const unsigned int pad_count = 8 - bits_in_buffer_;
            uint64_t next = bit_buffer_;
            if (order_ == bit_order::msb_first) {
                next <<= pad_count;
                if (pad_bit) next |= (1U << pad_count) - 1U;
            } else if (pad_bit) {
                next |= ((1U << pad_count) - 1U) << bits_in_buffer_;
            }
            emit_byte(static_cast<uint8_t>(next));

            bit_buffer_ = 0;
            bits_in_buffer_ = 0;
        }

        /**
         * @brief Pad and emit a pending byte. Also flushes the underlying stream.
         * @throws bstream_error If output or synchronization fails.
         * @param pad_bit Zero for zero padding; any nonzero value pads with one bits.
         */
        void flush(uint8_t pad_bit = 0) {
            align_to_byte(pad_bit);
            os_.flush();
            if (!os_) {
                throw bstream_error("bit_stream_writer: flush failed");
            }
        }

        /**
         * @brief Align with padding, then write the supplied bytes unchanged.
         * @throws bstream_error If output fails; a prefix may already have been emitted.
         * @param bytes Raw byte range to append unchanged.
         * @param pad_bit Zero for zero padding; any nonzero value pads with one bits.
         */
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
