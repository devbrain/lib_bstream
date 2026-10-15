/**
 * @file stream_reader.hh
 * @brief Binary input from borrowed standard streams.
 * @ingroup stream_io
 */

#pragma once

#include <bstream/detail/platform.hh>

#include <bstream/byte_order.hh>
#include <bstream/detail/endian_scope.hh>
#include <bstream/error.hh>
#include <bstream/pfr_adapter.hh>

#include <array>
#include <istream>
#include <string>
#include <string_view>
#include <vector>

namespace bstream {

    /**
     * @brief Binary reader borrowing a standard input stream.
     * @ingroup stream_io
     *
     * Ordinary reads support sequential streams; seek(), skip(), and peek() require
     * seeking support. Byte order defaults to little endian. Input may be partly
     * consumed on failure, and the underlying stream exception mask still applies.
     * Open files in binary mode. See @ref error_guide for position and error semantics.
     * @snippet usage.cc streams
     */
    class stream_reader {
    public:
        // ====================================================================
        // Constructors
        // ====================================================================
        /**
         * @brief Borrow an input stream at its current position.
         * @pre Borrowed storage and streams outlive this object and all derived views.
         * @param is Borrowed input stream; must outlive this wrapper.
         * @param byte_order Byte order used for serialized scalar fields; defaults to little endian.
         */
        explicit stream_reader(
            std::istream& is,
            endian byte_order = endian::little) noexcept
            : is_(is), endian_(byte_order) {}

        // ====================================================================
        // Stream Queries & State
        // ====================================================================
        /**
         * @brief Access the borrowed underlying standard stream.
         * @return A reference to the same underlying stream.
         */
        [[nodiscard]] std::istream& stream() noexcept { return is_; }
        /**
         * @brief Access the borrowed underlying standard stream.
         * @return A reference to the same underlying stream.
         */
        [[nodiscard]] const std::istream& stream() const noexcept { return is_; }

        /**
         * @brief Query the current byte cursor.
         * @note A failed tellg()/tellp() returns zero; inspect stream state to distinguish failure.
         * @return Current offset in bytes.
         */
        [[nodiscard]] std::size_t tell() {
            auto pos = is_.tellg();
            if (pos < 0) return 0;
            return static_cast<std::size_t>(pos);
        }

        /**
         * @brief Check for exhausted input. May probe the stream and set stream flags.
         * @return True if no readable input remains.
         */
        [[nodiscard]] bool eof() {
            return is_.eof() || is_.peek() == std::char_traits<char>::eof();
        }

        /**
         * @brief Query the underlying stream success state.
         * @return The underlying stream's boolean state.
         */
        explicit operator bool() { return static_cast<bool>(is_); }
        /**
         * @brief Negate this wrapper's boolean state.
         * @return True when the wrapper's boolean conversion is false.
         */
        bool operator!() { return !is_; }

        /**
         * @brief Return the configured scalar byte order.
         * @return The current byte order.
         */
        [[nodiscard]] constexpr endian byte_order() const noexcept { return endian_; }
        /**
         * @brief Set the byte order for subsequent typed operations.
         * @param order Byte order for subsequent scalar fields.
         */
        constexpr void set_byte_order(endian order) noexcept { endian_ = order; }

        /**
         * @brief Set the absolute byte cursor.
         * @pre The stream supports seeking and the offset fits std::streamoff.
         * @throws bstream_error If seeking fails.
         * @note seek() clears stream flags first; skip() retains them.
         * @param pos Absolute byte offset from the start of the buffer or stream.
         */
        void seek(std::size_t pos) {
            is_.clear();
            is_.seekg(static_cast<std::streamoff>(pos), std::ios::beg);
            if (!is_) {
                throw eof_error("stream_reader::seek failed");
            }
        }

        /**
         * @brief Advance the cursor by bytes without transferring data.
         * @pre The stream supports seeking and the offset fits std::streamoff.
         * @throws bstream_error If seeking fails.
         * @note seek() clears stream flags first; skip() retains them.
         * @param bytes Number of bytes to consume or transfer.
         */
        void skip(std::size_t bytes) {
            is_.seekg(static_cast<std::streamoff>(bytes), std::ios::cur);
            if (!is_) {
                throw eof_error("stream_reader::skip past end of stream");
            }
        }

        // ====================================================================
        // Raw Reads
        // ====================================================================
        /**
         * @brief Consume exactly the requested number of raw bytes without byte-order conversion.
         * @throws eof_error If the complete range cannot be read.
         * @note For nonzero bytes, dst must be writable and bytes must fit std::streamsize. A short read can modify dst and consume a prefix.
         * @param dst Writable destination covering the requested byte count.
         * @param bytes Number of bytes to consume or transfer.
         */
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
        /**
         * @brief Decode one scalar, supported aggregate, or custom value.
         * @note An explicit Endian template argument applies to nested fields and restores the configured order even on failure. See @ref serialization for type dispatch.
         * @throws eof_error If input is exhausted; custom/operator and allocation exceptions propagate.
         * @tparam T Scalar, reflectable aggregate, or custom serializable value type, as constrained by this overload.
         * @tparam Endian Byte order for this operation, including nested fields; the configured order is restored.
         * @return The decoded value.
         */
        template<typename T, endian Endian>
        [[nodiscard]] T read() {
            detail::endian_scope scope(*this, Endian);
            return read<T>();
        }

        /**
         * @brief Decode one scalar, supported aggregate, or custom value.
         * @note An explicit Endian template argument applies to nested fields and restores the configured order even on failure. See @ref serialization for type dispatch.
         * @throws eof_error If input is exhausted; custom/operator and allocation exceptions propagate.
         * @tparam T Scalar, reflectable aggregate, or custom serializable value type, as constrained by this overload.
         * @return The decoded value.
         */
        template<typename T>
        [[nodiscard]] T read() {
            if constexpr (scalar_type<T>) {
                T val{};
                read_exact(&val, sizeof(T));
                return bstream::byte_order::from_endian(val, endian_);
            } else if constexpr (detail::is_pfr_reflectable_v<T>) {
                return detail::read_aggregate<stream_reader, T>(*this);
            } else {
                T val{};
                *this >> val;
                return val;
            }
        }

        /**
         * @brief Decode a scalar without consuming it on success.
         * @throws eof_error If insufficient input is available.
         * @pre The stream supports tellg()/seekg(). Failed reads can advance input; restoration is attempted only after a successful read.
         * @tparam T Scalar, reflectable aggregate, or custom serializable value type, as constrained by this overload.
         * @return The decoded scalar in the configured byte order.
         */
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
        /**
         * @brief Read through a zero terminator and consume it.
         * @throws eof_error If no terminator is found before the input ends.
         * @return Owned string bytes preceding the first zero.
         */
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

        /**
         * @brief Consume an exactly sized string field.
         * @throws eof_error If the complete field is unavailable.
         * @param length Number of bytes in the field or raw character range.
         * @param strip_nulls If true, return only the bytes before the first zero; always consume the full field.
         * @return An owned string, optionally truncated at its first zero.
         */
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

        /**
         * @brief Read a length prefix in the configured byte order, then its payload.
         * @pre The decoded prefix is nonnegative and fits size_t; prefer unsigned integral prefixes.
         * @throws eof_error If the prefix or payload is incomplete.
         * @note The length prefix can already be consumed if reading the payload fails.
         * @tparam LenType Length-prefix type; use an unsigned integral type matching the writer/reader. Defaults to uint8_t.
         * @return An owned string of the decoded length.
         */
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
        /**
         * @brief Read exactly N elements without consuming an encoded count.
         * @throws eof_error If input is exhausted; custom extraction errors propagate.
         * @tparam T Scalar, reflectable aggregate, or custom serializable value type, as constrained by this overload.
         * @tparam N Number of array elements; no count is encoded on the wire.
         * @return An array of decoded elements.
         */
        template<typename T, std::size_t N>
        [[nodiscard]] std::array<T, N> read_array() {
            std::array<T, N> arr{};
            *this >> arr;
            return arr;
        }

        /**
         * @brief Allocate and read count elements without consuming an encoded count.
         * @throws eof_error If input is exhausted. Allocation and custom extraction errors propagate.
         * @note Scalar elements use contiguous bulk reads; std::vector<bool> is not supported.
         * @tparam T Scalar, reflectable aggregate, or custom serializable value type, as constrained by this overload.
         * @param count Number of elements to read.
         * @return A vector owning the decoded elements.
         */
        template<typename T>
        [[nodiscard]] std::vector<T> read_vector(std::size_t count) {
            std::vector<T> vec(count);
            if (count == 0) return vec;
            if constexpr (scalar_type<T>) {
                read_exact(vec.data(), count * sizeof(T));
                bstream::byte_order::from_endian_in_place(std::span<T>(vec), endian_);
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
        /**
         * @brief Extract into the supplied value or existing container elements.
         * @note No container count is consumed and vectors are not resized. Scalar fields use the configured byte order; custom extraction errors propagate.
         * @throws eof_error If input is exhausted; earlier elements may already be changed.
         * @tparam T Scalar, reflectable aggregate, or custom serializable value type, as constrained by this overload.
         * @param val Value to serialize, or destination receiving a decoded value.
         * @return *this for chaining.
         */
        template<scalar_type T>
        stream_reader& operator>>(T& val) {
            read_exact(&val, sizeof(T));
            val = bstream::byte_order::from_endian(val, endian_);
            return *this;
        }

        /**
         * @brief Extract into the supplied value or existing container elements.
         * @note No container count is consumed and vectors are not resized. Scalar fields use the configured byte order; custom extraction errors propagate.
         * @throws eof_error If input is exhausted; earlier elements may already be changed.
         * @tparam T Scalar, reflectable aggregate, or custom serializable value type, as constrained by this overload.
         * @param val Value to serialize, or destination receiving a decoded value.
         * @return *this for chaining.
         */
        template<typename T>
            requires detail::is_pfr_reflectable_v<T>
        stream_reader& operator>>(T& val) {
            val = detail::read_aggregate<stream_reader, T>(*this);
            return *this;
        }

        /**
         * @brief Extract into the supplied value or existing container elements.
         * @note No container count is consumed and vectors are not resized. Scalar fields use the configured byte order; custom extraction errors propagate.
         * @throws eof_error If input is exhausted; earlier elements may already be changed.
         * @tparam T Scalar, reflectable aggregate, or custom serializable value type, as constrained by this overload.
         * @param vec Container whose existing elements are transferred in order; no length prefix is used.
         * @return *this for chaining.
         */
        template<scalar_type T>
        stream_reader& operator>>(std::vector<T>& vec) {
            if (!vec.empty()) {
                read_exact(vec.data(), vec.size() * sizeof(T));
                bstream::byte_order::from_endian_in_place(std::span<T>(vec), endian_);
            }
            return *this;
        }

        /**
         * @brief Extract into the supplied value or existing container elements.
         * @note No container count is consumed and vectors are not resized. Scalar fields use the configured byte order; custom extraction errors propagate.
         * @throws eof_error If input is exhausted; earlier elements may already be changed.
         * @tparam T Scalar, reflectable aggregate, or custom serializable value type, as constrained by this overload.
         * @param vec Container whose existing elements are transferred in order; no length prefix is used.
         * @return *this for chaining.
         */
        template<typename T>
            requires (!scalar_type<T>)
        stream_reader& operator>>(std::vector<T>& vec) {
            for (auto& item : vec) {
                *this >> item;
            }
            return *this;
        }

        /**
         * @brief Extract into the supplied value or existing container elements.
         * @note No container count is consumed and vectors are not resized. Scalar fields use the configured byte order; custom extraction errors propagate.
         * @throws eof_error If input is exhausted; earlier elements may already be changed.
         * @tparam T Scalar, reflectable aggregate, or custom serializable value type, as constrained by this overload.
         * @tparam N Number of array elements; no count is encoded on the wire.
         * @param arr Array whose elements are transferred in order; no length prefix is used.
         * @return *this for chaining.
         */
        template<scalar_type T, std::size_t N>
        stream_reader& operator>>(std::array<T, N>& arr) {
            if constexpr (N > 0) {
                read_exact(arr.data(), N * sizeof(T));
                bstream::byte_order::from_endian_in_place(std::span<T>(arr), endian_);
            }
            return *this;
        }

        /**
         * @brief Extract into the supplied value or existing container elements.
         * @note No container count is consumed and vectors are not resized. Scalar fields use the configured byte order; custom extraction errors propagate.
         * @throws eof_error If input is exhausted; earlier elements may already be changed.
         * @tparam T Scalar, reflectable aggregate, or custom serializable value type, as constrained by this overload.
         * @tparam N Number of array elements; no count is encoded on the wire.
         * @param arr Array whose elements are transferred in order; no length prefix is used.
         * @return *this for chaining.
         */
        template<typename T, std::size_t N>
            requires (!scalar_type<T>)
        stream_reader& operator>>(std::array<T, N>& arr) {
            for (auto& item : arr) {
                *this >> item;
            }
            return *this;
        }

        /**
         * @brief Extract into the supplied value or existing container elements.
         * @note No container count is consumed and vectors are not resized. Scalar fields use the configured byte order; custom extraction errors propagate.
         * @throws eof_error If input is exhausted; earlier elements may already be changed.
         * @tparam T Scalar, reflectable aggregate, or custom serializable value type, as constrained by this overload.
         * @tparam N Number of array elements; no count is encoded on the wire.
         * @param arr Array whose elements are transferred in order; no length prefix is used.
         * @return *this for chaining.
         */
        template<scalar_type T, std::size_t N>
        stream_reader& operator>>(T (&arr)[N]) {
            read_exact(arr, N * sizeof(T));
            bstream::byte_order::from_endian_in_place(std::span<T>(arr), endian_);
            return *this;
        }

        /**
         * @brief Extract into the supplied value or existing container elements.
         * @note No container count is consumed and vectors are not resized. Scalar fields use the configured byte order; custom extraction errors propagate.
         * @throws eof_error If input is exhausted; earlier elements may already be changed.
         * @tparam T Scalar, reflectable aggregate, or custom serializable value type, as constrained by this overload.
         * @tparam N Number of array elements; no count is encoded on the wire.
         * @param arr Array whose elements are transferred in order; no length prefix is used.
         * @return *this for chaining.
         */
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
        /**
         * @brief Perform read<T>() and return a value or a caught std::exception message.
         * @note Available only with failsafe. Partial-progress semantics are unchanged. Non-standard exceptions or failure to allocate the error string terminate because this function is noexcept.
         * @tparam T Scalar, reflectable aggregate, or custom serializable value type, as constrained by this overload.
         * @return The decoded value, or an error string.
         */
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
