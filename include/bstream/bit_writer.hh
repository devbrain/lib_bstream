/**
 * @file bit_writer.hh
 * @brief Owned and borrowed memory bit-field output.
 * @ingroup bit_io
 */

#pragma once

#include <bstream/detail/platform.hh>

#include <bstream/bit_reader.hh>
#include <bstream/error.hh>
#include <bstream/span_writer.hh>
#include <bstream/types.hh>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>
#include <vector>

namespace bstream {

    /**
     * @brief Bit-field writer to owned or borrowed memory.
     * @ingroup bit_io
     *
     * The default constructor owns a growing byte vector. Other constructors
     * append to an external vector, fill a fixed span, or write through a borrowed
     * span_writer. The default order is msb_first. Explicitly flush() or finish()
     * to observe errors; the destructor suppresses them. Output is not transactional.
     * @snippet usage.cc bits
     */
    class bit_writer {
    public:
        // ====================================================================
        // Constructors
        // ====================================================================

        // Owning constructor (writes to internal std::vector)
        /**
         * @brief Create an owning writer with a growing internal vector.
         * @param order Bit order used within bytes and fields; defaults to msb_first.
         */
        explicit bit_writer(bit_order order = bit_order::msb_first)
            : owning_buf_(), vec_(&owning_buf_), span_(), writer_(nullptr),
              bit_buffer_(0), bits_in_buffer_(0), total_bits_written_(0), order_(order) {}

        // Target std::vector constructor
        /**
         * @brief Append to a borrowed vector.
         * @param target Borrowed destination; vector mode appends, fixed-span mode starts at zero.
         * @param order Bit order used within bytes and fields; defaults to msb_first.
         */
        explicit bit_writer(std::vector<uint8_t>& target, bit_order order = bit_order::msb_first)
            : owning_buf_(), vec_(&target), span_(), writer_(nullptr),
              bit_buffer_(0), bits_in_buffer_(0), total_bits_written_(0), order_(order) {}

        // Target fixed span constructor
        /**
         * @brief Write into a borrowed fixed-size span starting at offset zero.
         * @param target Borrowed destination; vector mode appends, fixed-span mode starts at zero.
         * @param order Bit order used within bytes and fields; defaults to msb_first.
         */
        explicit bit_writer(std::span<uint8_t> target, bit_order order = bit_order::msb_first)
            : owning_buf_(), vec_(nullptr), span_(target), writer_(nullptr),
              bit_buffer_(0), bits_in_buffer_(0), total_bits_written_(0), order_(order) {}

        // Target span_writer constructor
        /**
         * @brief Write through a borrowed span_writer at its current cursor.
         * @param writer Borrowed destination writer; must outlive this wrapper.
         * @param order Bit order used within bytes and fields; defaults to msb_first.
         */
        explicit bit_writer(span_writer& writer, bit_order order = bit_order::msb_first)
            : owning_buf_(), vec_(nullptr), span_(), writer_(&writer),
              bit_buffer_(0), bits_in_buffer_(0), total_bits_written_(0), order_(order) {}

        /**
         * @brief Destroy the wrapper without releasing borrowed storage.
         * @note Attempts to flush pending output, suppressing all failures. Flush explicitly to observe errors.
         */
        ~bit_writer() {
            try {
                flush();
            } catch (...) {
            }
        }

        /**
         * @brief Copy construction is disabled; this object carries cursor or output state.
         */
        bit_writer(const bit_writer&) = delete;
        /**
         * @brief Copy assignment is disabled.
         */
        bit_writer& operator=(const bit_writer&) = delete;

        /**
         * @brief Transfer the source buffer, cursor, and pending state; leave the source empty.
         * @param other Source whose state and destination are transferred; it is left without pending output.
         */
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
            other.span_cursor_ = 0;
            other.writer_ = nullptr;
            other.bits_in_buffer_ = 0;
            other.bit_buffer_ = 0;
            other.total_bits_written_ = 0;
        }

        /**
         * @brief Flush old pending output if applicable, suppressing failures, then transfer source state.
         * @note Source positions are preserved in the destination; the source loses its buffer/pending output.
         * @param other Source whose state and destination are transferred; it is left without pending output.
         * @return *this.
         */
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
                other.span_cursor_ = 0;
                other.writer_ = nullptr;
                other.bits_in_buffer_ = 0;
                other.bit_buffer_ = 0;
                other.total_bits_written_ = 0;
            }
            return *this;
        }

        // ====================================================================
        // Queries
        // ====================================================================
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
                throw bstream_error("bit_writer: bit order change requires byte alignment");
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
         * @brief View complete emitted bytes; excludes the pending partial byte.
         * @note External vector/span_writer modes include bytes that predated this writer. Reallocation or finish() can invalidate the borrowed view.
         * @return A borrowed view of the complete output bytes.
         */
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
                throw bstream_error("bit_writer: cannot write more than 64 bits at once");
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
         * @brief Pad and emit a pending byte.
         * @throws bstream_error If output or synchronization fails.
         * @param pad_bit Zero for zero padding; any nonzero value pads with one bits.
         */
        void flush(uint8_t pad_bit = 0) {
            align_to_byte(pad_bit);
        }

        /**
         * @brief Align with padding, then write the supplied bytes unchanged.
         * @throws bstream_error If output fails; a prefix may already have been emitted.
         * @param bytes Raw byte range to append unchanged.
         * @param pad_bit Zero for zero padding; any nonzero value pads with one bits.
         */
        void write_bytes(std::span<const uint8_t> bytes, uint8_t pad_bit = 0) {
            align_to_byte(pad_bit);
            for (uint8_t b : bytes) {
                emit_byte(b);
                total_bits_written_ += 8;
            }
        }

        // Finalize writing and return buffer (for owning mode)
        /**
         * @brief Flush the final byte and move out the owned vector.
         * @note Use for owning mode. Borrowed modes flush their destination but return an empty owned vector. Treat this as the end of a writing session.
         * @throws bstream_error If final-byte output fails.
         * @param pad_bit Zero for zero padding; any nonzero value pads with one bits.
         * @return The owned output bytes, including final padding.
         */
        std::vector<uint8_t> finish(uint8_t pad_bit = 0) {
            flush(pad_bit);
            return std::move(owning_buf_);
        }

    private:
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
