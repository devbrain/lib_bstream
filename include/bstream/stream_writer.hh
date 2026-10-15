#pragma once

#include <bstream/error.hh>
#include <bstream/pfr_adapter.hh>
#include <bstream/types.hh>

#include <array>
#include <cstring>
#include <ostream>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace bstream {

    class stream_writer {
    public:
        // ====================================================================
        // Constructors
        // ====================================================================
        explicit stream_writer(
            std::ostream& os,
            endian byte_order = endian::little) noexcept
            : os_(os), endian_(byte_order) {}

        // ====================================================================
        // Stream Queries & State
        // ====================================================================
        [[nodiscard]] std::ostream& stream() noexcept { return os_; }
        [[nodiscard]] const std::ostream& stream() const noexcept { return os_; }

        [[nodiscard]] std::size_t tell() {
            auto pos = os_.tellp();
            if (pos < 0) return 0;
            return static_cast<std::size_t>(pos);
        }

        explicit operator bool() const { return static_cast<bool>(os_); }
        bool operator!() const { return !os_; }

        [[nodiscard]] bool good() const { return os_.good(); }
        [[nodiscard]] bool fail() const { return os_.fail(); }
        [[nodiscard]] bool bad() const { return os_.bad(); }
        [[nodiscard]] bool eof() const { return os_.eof(); }

        void flush() {
            os_.flush();
            if (!os_) {
                throw bstream_error("stream_writer: flush failed");
            }
        }

        [[nodiscard]] constexpr endian byte_order() const noexcept { return endian_; }
        constexpr void set_byte_order(endian order) noexcept { endian_ = order; }

        void seek(std::size_t pos) {
            os_.clear();
            os_.seekp(static_cast<std::streamoff>(pos), std::ios::beg);
            if (!os_) {
                throw bstream_error("stream_writer: seek failed");
            }
        }

        void skip(std::size_t bytes) {
            os_.seekp(static_cast<std::streamoff>(bytes), std::ios::cur);
            if (!os_) {
                throw bstream_error("stream_writer: skip failed");
            }
        }

        // ====================================================================
        // Raw byte writing
        // ====================================================================
        void write_exact(const void* src, std::size_t bytes) {
            if (bytes == 0) return;
            if (src == nullptr) {
                throw bstream_error("stream_writer: null source buffer");
            }
            os_.write(static_cast<const char*>(src), static_cast<std::streamsize>(bytes));
            if (!os_) {
                throw bstream_error("stream_writer: write failed");
            }
        }

        void write_bytes(std::span<const uint8_t> bytes) {
            write_exact(bytes.data(), bytes.size());
        }

        void write_bytes(std::span<const std::byte> bytes) {
            write_exact(bytes.data(), bytes.size());
        }

        void write_raw(const char* buffer, std::streamsize length) {
            write_exact(buffer, static_cast<std::size_t>(length));
        }

        void write_raw(const std::string& rawData) {
            write_exact(rawData.data(), rawData.size());
        }

        // ====================================================================
        // Typed Writing (Scalar, Aggregate, Custom)
        // ====================================================================
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

        template<scalar_type T>
        void write_span(std::span<const T> data) {
            if constexpr (sizeof(T) == 1) {
                write_exact(data.data(), data.size());
            } else {
                for (const auto& item : data) {
                    write<T>(item);
                }
            }
        }

        // ====================================================================
        // String Writing Helpers
        // ====================================================================
        void write_cstring(std::string_view str) {
            write_exact(str.data(), str.size());
            const char null_byte = '\0';
            write_exact(&null_byte, 1);
        }

        void write_fixed_string(std::string_view str, std::size_t fixed_length, char pad = '\0') {
            const std::size_t copy_len = std::min(str.size(), fixed_length);
            write_exact(str.data(), copy_len);
            if (copy_len < fixed_length) {
                std::vector<char> padding(fixed_length - copy_len, pad);
                write_exact(padding.data(), padding.size());
            }
        }

        template<scalar_type LenType = uint32_t>
        void write_pascal_string(std::string_view str) {
            write<LenType>(static_cast<LenType>(str.size()));
            write_exact(str.data(), str.size());
        }

        // ====================================================================
        // Stream Insertion Operator (operator<<)
        // ====================================================================
        template<scalar_type T>
        stream_writer& operator<<(const T& val) {
            write<T>(val);
            return *this;
        }

        template<typename T>
            requires detail::is_pfr_reflectable_v<T>
        stream_writer& operator<<(const T& val) {
            detail::write_aggregate(*this, val);
            return *this;
        }

        template<scalar_type T>
        stream_writer& operator<<(const std::vector<T>& vec) {
            for (const auto& item : vec) {
                write<T>(item);
            }
            return *this;
        }

        template<typename T>
            requires (!scalar_type<T>)
        stream_writer& operator<<(const std::vector<T>& vec) {
            for (const auto& item : vec) {
                *this << item;
            }
            return *this;
        }

        template<scalar_type T, std::size_t N>
        stream_writer& operator<<(const std::array<T, N>& arr) {
            for (const auto& item : arr) {
                write<T>(item);
            }
            return *this;
        }

        template<typename T, std::size_t N>
            requires (!scalar_type<T>)
        stream_writer& operator<<(const std::array<T, N>& arr) {
            for (const auto& item : arr) {
                *this << item;
            }
            return *this;
        }

        template<scalar_type T, std::size_t N>
        stream_writer& operator<<(const T (&arr)[N]) {
            for (std::size_t i = 0; i < N; ++i) {
                write<T>(arr[i]);
            }
            return *this;
        }

    private:
        std::ostream& os_;
        endian endian_{endian::little};
    };

    using binary_writer = stream_writer;

} // namespace bstream
