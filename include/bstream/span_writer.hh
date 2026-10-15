#pragma once

#include <bstream/error.hh>
#include <bstream/pfr_adapter.hh>
#include <bstream/types.hh>

#include <array>
#include <cstring>
#include <span>
#include <string_view>
#include <vector>

namespace bstream {

    class span_writer {
    public:
        constexpr span_writer() noexcept = default;

        constexpr explicit span_writer(
            std::span<uint8_t> buffer,
            endian byte_order = endian::little) noexcept
            : buffer_(buffer), cursor_(0), endian_(byte_order) {}

        explicit span_writer(
            std::span<std::byte> buffer,
            endian byte_order = endian::little) noexcept
            : buffer_(reinterpret_cast<uint8_t*>(buffer.data()), buffer.size()),
              cursor_(0), endian_(byte_order) {}

        constexpr span_writer(
            void* buffer,
            std::size_t size,
            endian byte_order = endian::little) noexcept
            : buffer_(static_cast<uint8_t*>(buffer), size),
              cursor_(0), endian_(byte_order) {}

        [[nodiscard]] constexpr std::size_t tell() const noexcept { return cursor_; }
        [[nodiscard]] constexpr std::size_t capacity() const noexcept { return buffer_.size(); }
        [[nodiscard]] constexpr std::size_t remaining() const noexcept {
            return cursor_ < buffer_.size() ? (buffer_.size() - cursor_) : 0;
        }

        [[nodiscard]] constexpr endian byte_order() const noexcept { return endian_; }
        constexpr void set_byte_order(endian order) noexcept { endian_ = order; }

        [[nodiscard]] std::span<const uint8_t> written_span() const noexcept {
            return buffer_.subspan(0, cursor_);
        }

        constexpr void seek(std::size_t pos) {
            if (pos > buffer_.size()) {
                throw eof_error("span_writer::seek past capacity");
            }
            cursor_ = pos;
        }

        constexpr void skip(std::size_t bytes) {
            if (remaining() < bytes) {
                throw eof_error("span_writer::skip past capacity");
            }
            cursor_ += bytes;
        }

        void write_exact(const void* src, std::size_t bytes) {
            if (remaining() < bytes) {
                throw eof_error("span_writer: buffer overflow");
            }
            if (bytes > 0 && src != nullptr) {
                std::memcpy(buffer_.data() + cursor_, src, bytes);
            }
            cursor_ += bytes;
        }

        template<typename T, endian Endian>
        void write(const T& val) {
            if constexpr (scalar_type<T>) {
                T converted = to_endian(val, Endian);
                write_exact(&converted, sizeof(T));
            } else if constexpr (detail::is_pfr_reflectable_v<T>) {
                detail::write_aggregate(*this, val);
            } else {
                *this << val;
            }
        }

        template<typename T>
        void write(const T& val) {
            if constexpr (scalar_type<T>) {
                T converted = to_endian(val, endian_);
                write_exact(&converted, sizeof(T));
            } else if constexpr (detail::is_pfr_reflectable_v<T>) {
                detail::write_aggregate(*this, val);
            } else {
                *this << val;
            }
        }

        void write_cstring(std::string_view str) {
            write_exact(str.data(), str.size());
            const uint8_t null_byte = 0;
            write_exact(&null_byte, 1);
        }

        void write_fixed_string(std::string_view str, std::size_t fixed_length, char pad = '\0') {
            if (remaining() < fixed_length) {
                throw eof_error("span_writer: buffer overflow writing fixed string");
            }
            const std::size_t copy_len = std::min(str.size(), fixed_length);
            std::memcpy(buffer_.data() + cursor_, str.data(), copy_len);
            if (copy_len < fixed_length) {
                std::memset(buffer_.data() + cursor_ + copy_len, pad, fixed_length - copy_len);
            }
            cursor_ += fixed_length;
        }

        template<scalar_type LenType = uint8_t>
        void write_pascal_string(std::string_view str) {
            write<LenType>(static_cast<LenType>(str.size()));
            write_exact(str.data(), str.size());
        }

        template<scalar_type T>
        span_writer& operator<<(const T& val) {
            write<T>(val);
            return *this;
        }

        template<typename T>
            requires detail::is_pfr_reflectable_v<T>
        span_writer& operator<<(const T& val) {
            detail::write_aggregate(*this, val);
            return *this;
        }

        template<scalar_type T>
        span_writer& operator<<(const std::vector<T>& vec) {
            for (const auto& item : vec) {
                write<T>(item);
            }
            return *this;
        }

        template<typename T>
            requires (!scalar_type<T>)
        span_writer& operator<<(const std::vector<T>& vec) {
            for (const auto& item : vec) {
                *this << item;
            }
            return *this;
        }

        template<scalar_type T, std::size_t N>
        span_writer& operator<<(const std::array<T, N>& arr) {
            for (const auto& item : arr) {
                write<T>(item);
            }
            return *this;
        }

        template<typename T, std::size_t N>
            requires (!scalar_type<T>)
        span_writer& operator<<(const std::array<T, N>& arr) {
            for (const auto& item : arr) {
                *this << item;
            }
            return *this;
        }

    private:
        std::span<uint8_t> buffer_{};
        std::size_t cursor_{0};
        endian endian_{endian::little};
    };

} // namespace bstream
