#pragma once

#include <bstream/detail/platform.hh>

#include <algorithm>
#include <array>
#include <cstdint>

namespace bstream::detail {

    // An unaligned 64-bit peek can span nine bytes. Keep the original bytes so
    // peeking, alignment, and byte-boundary order changes never discard data.
    class bit_read_buffer {
    public:
        [[nodiscard]] constexpr unsigned int size() const noexcept { return size_; }
        [[nodiscard]] constexpr bool aligned() const noexcept { return offset_ == 0; }

        void append(uint8_t byte) noexcept {
            bytes_[(head_ + (offset_ + size_) / 8) % capacity] = byte;
            size_ += 8;
        }

        [[nodiscard]] uint64_t peek(unsigned int count, bool msb_first) const noexcept {
            uint64_t result = 0;
            unsigned int consumed = 0;
            unsigned int offset = offset_;
            unsigned int index = head_;
            while (consumed < count) {
                const unsigned int chunk = std::min(8 - offset, count - consumed);
                const unsigned int shift = msb_first ? 8 - offset - chunk : offset;
                const uint64_t bits = (bytes_[index] >> shift) & ((1U << chunk) - 1U);
                if (msb_first) {
                    result = (result << chunk) | bits;
                } else {
                    result |= bits << consumed;
                }
                consumed += chunk;
                offset = 0;
                index = (index + 1) % capacity;
            }
            return result;
        }

        void discard(unsigned int count) noexcept {
            head_ = (head_ + (offset_ + count) / 8) % capacity;
            offset_ = (offset_ + count) % 8;
            size_ -= count;
        }

        void align_to_byte() noexcept {
            if (offset_ != 0) discard(8 - offset_);
        }

    private:
        static constexpr unsigned int capacity = 9;
        std::array<uint8_t, capacity> bytes_{};
        unsigned int head_{0};
        unsigned int offset_{0};
        unsigned int size_{0};
    };

} // namespace bstream::detail
