#pragma once

#include <bstream/types.hh>
#include <bstream/error.hh>
#include <bstream/pfr_adapter.hh>

#include <array>
#include <istream>
#include <string>
#include <string_view>
#include <vector>

namespace bstream {

    class stream_reader {
    public:
        // ====================================================================
        // Constructors
        // ====================================================================
        explicit stream_reader(
            std::istream& is,
            endian byte_order = endian::little) noexcept
            : is_(is), endian_(byte_order) {}

        // ====================================================================
        // Stream Queries & State
        // ====================================================================
        [[nodiscard]] std::istream& stream() noexcept { return is_; }
        [[nodiscard]] const std::istream& stream() const noexcept { return is_; }

        [[nodiscard]] std::size_t tell() {
            auto pos = is_.tellg();
            if (pos < 0) return 0;
            return static_cast<std::size_t>(pos);
        }

        [[nodiscard]] bool eof() {
            return is_.eof() || is_.peek() == std::char_traits<char>::eof();
        }

        explicit operator bool() { return static_cast<bool>(is_); }
        bool operator!() { return !is_; }

        [[nodiscard]] constexpr endian byte_order() const noexcept { return endian_; }
        constexpr void set_byte_order(endian order) noexcept { endian_ = order; }

        void seek(std::size_t pos) {
            is_.clear();
            is_.seekg(static_cast<std::streamoff>(pos), std::ios::beg);
            if (!is_) {
                throw eof_error("stream_reader::seek failed");
            }
        }

        void skip(std::size_t bytes) {
            is_.seekg(static_cast<std::streamoff>(bytes), std::ios::cur);
            if (!is_) {
                throw eof_error("stream_reader::skip past end of stream");
            }
        }

        // ====================================================================
        // Raw Reads
        // ====================================================================
        void read_exact(void* dst, std::size_t bytes) {
            if (bytes == 0) return;
            is_.read(static_cast<char*>(dst), static_cast<std::streamsize>(bytes));
            if (static_cast<std::size_t>(is_.gcount()) != bytes) {
                throw eof_error("stream_reader: unexpected EOF reading raw bytes");
            }
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
                return detail::read_aggregate<stream_reader, T>(*this);
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
                return detail::read_aggregate<stream_reader, T>(*this);
            } else {
                T val{};
                *this >> val;
                return val;
            }
        }

        template<scalar_type T>
        [[nodiscard]] T peek() {
            auto pos = is_.tellg();
            T val = read<T>();
            is_.seekg(pos, std::ios::beg);
            return val;
        }

        // ====================================================================
        // String Helpers
        // ====================================================================
        [[nodiscard]] std::string read_cstring() {
            std::string result;
            char ch = 0;
            while (true) {
                if (!is_.get(ch)) {
                    throw eof_error("stream_reader: null-terminator not found before EOF");
                }
                if (ch == '\0') {
                    break;
                }
                result.push_back(ch);
            }
            return result;
        }

        [[nodiscard]] std::string read_fixed_string(std::size_t length, bool strip_nulls = true) {
            std::string buf(length, '\0');
            read_exact(buf.data(), length);
            if (strip_nulls) {
                auto null_pos = buf.find('\0');
                if (null_pos != std::string::npos) {
                    buf.resize(null_pos);
                }
            }
            return buf;
        }

        template<scalar_type LenType = uint8_t>
        [[nodiscard]] std::string read_pascal_string() {
            const auto len = static_cast<std::size_t>(read<LenType>());
            std::string buf(len, '\0');
            read_exact(buf.data(), len);
            return buf;
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
        stream_reader& operator>>(T& val) {
            read_exact(&val, sizeof(T));
            val = from_endian(val, endian_);
            return *this;
        }

        template<typename T>
            requires detail::is_pfr_reflectable_v<T>
        stream_reader& operator>>(T& val) {
            val = detail::read_aggregate<stream_reader, T>(*this);
            return *this;
        }

        template<scalar_type T>
        stream_reader& operator>>(std::vector<T>& vec) {
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
        stream_reader& operator>>(std::vector<T>& vec) {
            for (auto& item : vec) {
                *this >> item;
            }
            return *this;
        }

        template<scalar_type T, std::size_t N>
        stream_reader& operator>>(std::array<T, N>& arr) {
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
        stream_reader& operator>>(std::array<T, N>& arr) {
            for (auto& item : arr) {
                *this >> item;
            }
            return *this;
        }

        template<scalar_type T, std::size_t N>
        stream_reader& operator>>(T (&arr)[N]) {
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
        stream_reader& operator>>(T (&arr)[N]) {
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
        std::istream& is_;
        endian endian_{endian::little};
    };

} // namespace bstream
