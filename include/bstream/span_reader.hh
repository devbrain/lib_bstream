#pragma once

#include <bstream/types.hh>
#include <bstream/error.hh>
#include <bstream/pfr_adapter.hh>

#include <array>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace bstream {

    class span_reader {
    public:
        // ====================================================================
        // Constructors
        // ====================================================================
        constexpr span_reader() noexcept = default;

        constexpr explicit span_reader(
            std::span<const uint8_t> data,
            endian byte_order = endian::little) noexcept
            : data_(data), cursor_(0), endian_(byte_order) {}

        explicit span_reader(
            std::span<const std::byte> data,
            endian byte_order = endian::little) noexcept
            : data_(reinterpret_cast<const uint8_t*>(data.data()), data.size()),
              cursor_(0), endian_(byte_order) {}

        explicit span_reader(
            std::span<const char> data,
            endian byte_order = endian::little) noexcept
            : data_(reinterpret_cast<const uint8_t*>(data.data()), data.size()),
              cursor_(0), endian_(byte_order) {}

        constexpr span_reader(
            const void* data,
            std::size_t size,
            endian byte_order = endian::little) noexcept
            : data_(static_cast<const uint8_t*>(data), size),
              cursor_(0), endian_(byte_order) {}

        template<typename Container>
            requires requires(const Container& c) {
                { c.data() } -> std::convertible_to<const void*>;
                { c.size() } -> std::convertible_to<std::size_t>;
            } && (!std::is_same_v<std::decay_t<Container>, span_reader>)
        constexpr explicit span_reader(
            const Container& c,
            endian byte_order = endian::little) noexcept
            : data_(static_cast<const uint8_t*>(static_cast<const void*>(c.data())), c.size()),
              cursor_(0), endian_(byte_order) {}

        // ====================================================================
        // Navigation & State
        // ====================================================================
        [[nodiscard]] constexpr std::size_t tell() const noexcept { return cursor_; }
        [[nodiscard]] constexpr std::size_t size() const noexcept { return data_.size(); }
        [[nodiscard]] constexpr std::size_t remaining() const noexcept {
            return cursor_ < data_.size() ? (data_.size() - cursor_) : 0;
        }
        [[nodiscard]] constexpr bool eof() const noexcept { return cursor_ >= data_.size(); }
        constexpr explicit operator bool() const noexcept { return !eof(); }
        constexpr bool operator!() const noexcept { return eof(); }

        [[nodiscard]] constexpr endian byte_order() const noexcept { return endian_; }
        constexpr void set_byte_order(endian order) noexcept { endian_ = order; }

        constexpr void seek(std::size_t pos) {
            if (pos > data_.size()) {
                throw eof_error("span_reader::seek past end of buffer");
            }
            cursor_ = pos;
        }

        constexpr void skip(std::size_t bytes) {
            if (remaining() < bytes) {
                throw eof_error("span_reader::skip past end of buffer");
            }
            cursor_ += bytes;
        }

        [[nodiscard]] constexpr const uint8_t* current_ptr() const noexcept {
            return data_.data() + cursor_;
        }

        // ====================================================================
        // Raw Reads & Zero-Copy Slicing
        // ====================================================================
        void read_exact(void* dst, std::size_t bytes) {
            if (remaining() < bytes) {
                throw eof_error("span_reader: unexpected EOF reading raw bytes");
            }
            if (bytes > 0 && dst != nullptr) {
                std::memcpy(dst, current_ptr(), bytes);
            }
            cursor_ += bytes;
        }

        [[nodiscard]] std::span<const uint8_t> read_span(std::size_t bytes) {
            if (remaining() < bytes) {
                throw eof_error("span_reader: unexpected EOF reading subspan");
            }
            std::span<const uint8_t> sub(current_ptr(), bytes);
            cursor_ += bytes;
            return sub;
        }

        [[nodiscard]] span_reader subreader(std::size_t bytes) {
            return span_reader(read_span(bytes), endian_);
        }

        // ====================================================================
        // Typed Reads
        // ====================================================================
        template<typename T, endian Endian>
        [[nodiscard]] T read() {
            if constexpr (scalar_type<T>) {
                T val{};
                read_exact(&val, sizeof(T));
                return from_endian(val, Endian);
            } else if constexpr (detail::is_pfr_reflectable_v<T>) {
                return detail::read_aggregate<span_reader, T>(*this);
            } else {
                T val{};
                *this >> val;
                return val;
            }
        }

        template<typename T>
        [[nodiscard]] T read() {
            if constexpr (scalar_type<T>) {
                T val{};
                read_exact(&val, sizeof(T));
                return from_endian(val, endian_);
            } else if constexpr (detail::is_pfr_reflectable_v<T>) {
                return detail::read_aggregate<span_reader, T>(*this);
            } else {
                T val{};
                *this >> val;
                return val;
            }
        }

        template<scalar_type T>
        [[nodiscard]] T peek() const {
            if (remaining() < sizeof(T)) {
                throw eof_error("span_reader: unexpected EOF peeking value");
            }
            T val{};
            std::memcpy(&val, current_ptr(), sizeof(T));
            return from_endian(val, endian_);
        }

        // ====================================================================
        // String Helpers
        // ====================================================================
        [[nodiscard]] std::string read_cstring() {
            const uint8_t* p = current_ptr();
            const std::size_t rem = remaining();
            const void* null_pos = std::memchr(p, 0, rem);
            if (!null_pos) {
                throw eof_error("span_reader: null-terminator not found before EOF");
            }
            const auto len = static_cast<std::size_t>(
                static_cast<const uint8_t*>(null_pos) - p);
            std::string s(reinterpret_cast<const char*>(p), len);
            cursor_ += (len + 1);
            return s;
        }

        [[nodiscard]] std::string read_fixed_string(std::size_t length, bool strip_nulls = true) {
            if (remaining() < length) {
                throw eof_error("span_reader: unexpected EOF reading fixed string");
            }
            const char* p = reinterpret_cast<const char*>(current_ptr());
            cursor_ += length;
            if (strip_nulls) {
                std::size_t actual_len = 0;
                while (actual_len < length && p[actual_len] != '\0') {
                    ++actual_len;
                }
                return std::string(p, actual_len);
            }
            return std::string(p, length);
        }

        template<scalar_type LenType = uint8_t>
        [[nodiscard]] std::string read_pascal_string() {
            const auto len = static_cast<std::size_t>(read<LenType>());
            if (remaining() < len) {
                throw eof_error("span_reader: unexpected EOF reading pascal string payload");
            }
            std::string s(reinterpret_cast<const char*>(current_ptr()), len);
            cursor_ += len;
            return s;
        }

        // ====================================================================
        // Container Reads
        // ====================================================================
        template<typename T, std::size_t N>
        [[nodiscard]] std::array<T, N> read_array() {
            std::array<T, N> arr{};
            *this >> arr;
            return arr;
        }

        template<typename T>
        [[nodiscard]] std::vector<T> read_vector(std::size_t count) {
            std::vector<T> vec(count);
            if (count == 0) return vec;
            if constexpr (scalar_type<T>) {
                read_exact(vec.data(), count * sizeof(T));
                if (endian_ != endian::native) {
                    for (auto& item : vec) {
                        item = byteswap(item);
                    }
                }
            } else {
                for (auto& item : vec) {
                    *this >> item;
                }
            }
            return vec;
        }

        // ====================================================================
        // Stream Extraction Operator (operator>>)
        // ====================================================================
        template<scalar_type T>
        span_reader& operator>>(T& val) {
            read_exact(&val, sizeof(T));
            val = from_endian(val, endian_);
            return *this;
        }

        template<typename T>
            requires detail::is_pfr_reflectable_v<T>
        span_reader& operator>>(T& val) {
            val = detail::read_aggregate<span_reader, T>(*this);
            return *this;
        }

        template<scalar_type T>
        span_reader& operator>>(std::vector<T>& vec) {
            if (!vec.empty()) {
                read_exact(vec.data(), vec.size() * sizeof(T));
                if (endian_ != endian::native) {
                    for (auto& item : vec) {
                        item = byteswap(item);
                    }
                }
            }
            return *this;
        }

        template<typename T>
            requires (!scalar_type<T>)
        span_reader& operator>>(std::vector<T>& vec) {
            for (auto& item : vec) {
                *this >> item;
            }
            return *this;
        }

        template<scalar_type T, std::size_t N>
        span_reader& operator>>(std::array<T, N>& arr) {
            if constexpr (N > 0) {
                read_exact(arr.data(), N * sizeof(T));
                if (endian_ != endian::native) {
                    for (auto& item : arr) {
                        item = byteswap(item);
                    }
                }
            }
            return *this;
        }

        template<typename T, std::size_t N>
            requires (!scalar_type<T>)
        span_reader& operator>>(std::array<T, N>& arr) {
            for (auto& item : arr) {
                *this >> item;
            }
            return *this;
        }

        template<scalar_type T, std::size_t N>
        span_reader& operator>>(T (&arr)[N]) {
            read_exact(arr, N * sizeof(T));
            if (endian_ != endian::native) {
                for (std::size_t i = 0; i < N; ++i) {
                    arr[i] = byteswap(arr[i]);
                }
            }
            return *this;
        }

        template<typename T, std::size_t N>
            requires (!scalar_type<T>)
        span_reader& operator>>(T (&arr)[N]) {
            for (std::size_t i = 0; i < N; ++i) {
                *this >> arr[i];
            }
            return *this;
        }

#if defined(BSTREAM_HAS_FAILSAFE)
        // ====================================================================
        // Failsafe Expected Try-Reads
        // ====================================================================
        template<typename T>
        [[nodiscard]] expected<T> try_read() noexcept {
            try {
                return read<T>();
            } catch (const std::exception& e) {
                return failsafe::make_unexpected(std::string(e.what()));
            }
        }
#endif

    private:
        std::span<const uint8_t> data_{};
        std::size_t cursor_{0};
        endian endian_{endian::little};
    };

} // namespace bstream
