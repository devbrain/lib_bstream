/**
 * @file stream_writer.hh
 * @brief Binary output to borrowed standard streams.
 * @ingroup stream_io
 */

#pragma once

#include <bstream/detail/platform.hh>

#include <bstream/error.hh>
#include <bstream/pfr_adapter.hh>
#include <bstream/types.hh>
#include <bstream/detail/endian_scope.hh>

#include <algorithm>
#include <array>
#include <cstring>
#include <limits>
#include <ostream>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace bstream {

    /**
     * @brief Binary writer borrowing a standard output stream.
     * @ingroup stream_io
     *
     * Byte order defaults to little endian. Operations do not own or close the
     * stream and can emit a prefix before failing. Open files in binary mode.
     * Call flush() to check delivery through the stream buffer. The underlying
     * stream exception mask and allocation failures may produce other exceptions.
     * @snippet usage.cc streams
     */
    class stream_writer {
    public:
        // ====================================================================
        // Constructors
        // ====================================================================
        /**
         * @brief Borrow an output stream at its current position.
         * @pre Borrowed storage and streams outlive this object and all derived views.
         * @param os Borrowed output stream; must outlive this wrapper.
         * @param byte_order Byte order used for serialized scalar fields; defaults to little endian.
         */
        explicit stream_writer(
            std::ostream& os,
            endian byte_order = endian::little) noexcept
            : os_(os), endian_(byte_order) {}

        // ====================================================================
        // Stream Queries & State
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
         * @brief Query the current byte cursor.
         * @note A failed tellg()/tellp() returns zero; inspect stream state to distinguish failure.
         * @return Current offset in bytes.
         */
        [[nodiscard]] std::size_t tell() {
            auto pos = os_.tellp();
            if (pos < 0) return 0;
            return static_cast<std::size_t>(pos);
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

        /**
         * @brief Query the underlying stream good() state.
         * @return True if no stream state flags are set.
         */
        [[nodiscard]] bool good() const { return os_.good(); }
        /**
         * @brief Query the underlying stream fail() state.
         * @return True if failbit or badbit is set.
         */
        [[nodiscard]] bool fail() const { return os_.fail(); }
        /**
         * @brief Query the underlying stream bad() state.
         * @return True if badbit is set.
         */
        [[nodiscard]] bool bad() const { return os_.bad(); }
        /**
         * @brief Query the underlying output stream's eofbit flag.
         * @return The underlying stream eof() flag.
         */
        [[nodiscard]] bool eof() const { return os_.eof(); }

        /**
         * @brief Flush the borrowed output stream.
         * @throws bstream_error If output or synchronization fails.
         */
        void flush() {
            os_.flush();
            if (!os_) {
                throw bstream_error("stream_writer: flush failed");
            }
        }

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
            os_.clear();
            os_.seekp(static_cast<std::streamoff>(pos), std::ios::beg);
            if (!os_) {
                throw bstream_error("stream_writer: seek failed");
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
            os_.seekp(static_cast<std::streamoff>(bytes), std::ios::cur);
            if (!os_) {
                throw bstream_error("stream_writer: skip failed");
            }
        }

        // ====================================================================
        // Raw byte writing
        // ====================================================================
        /**
         * @brief Write raw bytes without byte-order conversion.
         * @throws bstream_error If output fails.
         * @note For nonzero bytes a null source is rejected; bytes must fit std::streamsize. A failed write can emit a prefix.
         * @param src Readable source covering the requested byte count.
         * @param bytes Number of bytes to consume or transfer.
         */
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

        /**
         * @brief Write the supplied byte range without conversion.
         * @throws bstream_error If output fails; a prefix may already have been emitted.
         * @param bytes Raw byte range to append unchanged.
         */
        void write_bytes(std::span<const uint8_t> bytes) {
            write_exact(bytes.data(), bytes.size());
        }

        /**
         * @brief Write the supplied byte range without conversion.
         * @throws bstream_error If output fails; a prefix may already have been emitted.
         * @param bytes Raw byte range to append unchanged.
         */
        void write_bytes(std::span<const std::byte> bytes) {
            write_exact(bytes.data(), bytes.size());
        }

        /**
         * @brief Write raw bytes without byte-order conversion, a prefix, or a terminator.
         * @pre A supplied length is nonnegative and the range is readable.
         * @throws bstream_error If output fails.
         * @param buffer Borrowed mutable output storage; must remain alive and stable.
         * @param length Number of bytes in the field or raw character range.
         */
        void write_raw(const char* buffer, std::streamsize length) {
            write_exact(buffer, static_cast<std::size_t>(length));
        }

        /**
         * @brief Write raw bytes without byte-order conversion, a prefix, or a terminator.
         * @pre A supplied length is nonnegative and the range is readable.
         * @throws bstream_error If output fails.
         * @param rawData String whose complete byte range is written without a terminator or prefix.
         */
        void write_raw(const std::string& rawData) {
            write_exact(rawData.data(), rawData.size());
        }

        // ====================================================================
        // Typed Writing (Scalar, Aggregate, Custom)
        // ====================================================================
        /**
         * @brief Serialize a scalar, supported aggregate, or custom value.
         * @note An explicit Endian applies to nested fields and is restored after return or exception. See @ref serialization.
         * @throws bstream_error If output fails; earlier bytes can remain committed.
         * @tparam T Scalar, reflectable aggregate, or custom serializable value type, as constrained by this overload.
         * @tparam Endian Byte order for this operation, including nested fields; the configured order is restored.
         * @param val Value to serialize, or destination receiving a decoded value.
         */
        template<typename T, endian Endian>
        void write(const T& val) {
            detail::endian_scope scope(*this, Endian);
            write<T>(val);
        }

        /**
         * @brief Serialize a scalar, supported aggregate, or custom value.
         * @note An explicit Endian applies to nested fields and is restored after return or exception. See @ref serialization.
         * @throws bstream_error If output fails; earlier bytes can remain committed.
         * @tparam T Scalar, reflectable aggregate, or custom serializable value type, as constrained by this overload.
         * @param val Value to serialize, or destination receiving a decoded value.
         */
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

        /**
         * @brief Write scalar elements in order, converting each to the configured byte order.
         * @note No element count is written.
         * @throws bstream_error If output fails.
         * @tparam T Scalar, reflectable aggregate, or custom serializable value type, as constrained by this overload.
         * @param data Scalar sequence to serialize without a count prefix.
         */
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
        /**
         * @brief Write string bytes followed by a zero byte.
         * @throws bstream_error If output fails; the payload may already have been emitted.
         * @param str String bytes to serialize; no character encoding conversion is performed.
         */
        void write_cstring(std::string_view str) {
            write_exact(str.data(), str.size());
            const char null_byte = '\0';
            write_exact(&null_byte, 1);
        }

        /**
         * @brief Write an exactly sized string field, truncating or padding as needed.
         * @throws bstream_error If the destination cannot accept the field.
         * @param str String bytes to serialize; no character encoding conversion is performed.
         * @param fixed_length Exact width of the output field in bytes.
         * @param pad Padding byte used when the string is shorter than fixed_length.
         */
        void write_fixed_string(std::string_view str, std::size_t fixed_length, char pad = '\0') {
            const std::size_t copy_len = std::min(str.size(), fixed_length);
            write_exact(str.data(), copy_len);
            if (copy_len < fixed_length) {
                std::vector<char> padding(fixed_length - copy_len, pad);
                write_exact(padding.data(), padding.size());
            }
        }

        /**
         * @brief Write a length prefix in the configured byte order followed by string bytes.
         * @throws parse_error If the length does not fit LenType; no bytes are written in that case.
         * @throws bstream_error If output fails after length validation.
         * @tparam LenType Length-prefix type; use an unsigned integral type matching the writer/reader. Defaults to uint8_t.
         * @param str String bytes to serialize; no character encoding conversion is performed.
         */
        template<std::integral LenType = uint8_t>
        void write_pascal_string(std::string_view str) {
            if (str.size() > static_cast<std::uintmax_t>(std::numeric_limits<LenType>::max())) {
                throw parse_error("stream_writer: string length does not fit its prefix");
            }
            write<LenType>(static_cast<LenType>(str.size()));
            write_exact(str.data(), str.size());
        }

        // ====================================================================
        // Stream Insertion Operator (operator<<)
        // ====================================================================
        /**
         * @brief Serialize the supplied value or all container elements in order.
         * @note No container count is emitted. Scalar fields use the configured byte order; custom insertion errors propagate.
         * @throws bstream_error If output fails; earlier elements can already be committed.
         * @tparam T Scalar, reflectable aggregate, or custom serializable value type, as constrained by this overload.
         * @param val Value to serialize, or destination receiving a decoded value.
         * @return *this for chaining.
         */
        template<scalar_type T>
        stream_writer& operator<<(const T& val) {
            write<T>(val);
            return *this;
        }

        /**
         * @brief Serialize the supplied value or all container elements in order.
         * @note No container count is emitted. Scalar fields use the configured byte order; custom insertion errors propagate.
         * @throws bstream_error If output fails; earlier elements can already be committed.
         * @tparam T Scalar, reflectable aggregate, or custom serializable value type, as constrained by this overload.
         * @param val Value to serialize, or destination receiving a decoded value.
         * @return *this for chaining.
         */
        template<typename T>
            requires detail::is_pfr_reflectable_v<T>
        stream_writer& operator<<(const T& val) {
            detail::write_aggregate(*this, val);
            return *this;
        }

        /**
         * @brief Serialize the supplied value or all container elements in order.
         * @note No container count is emitted. Scalar fields use the configured byte order; custom insertion errors propagate.
         * @throws bstream_error If output fails; earlier elements can already be committed.
         * @tparam T Scalar, reflectable aggregate, or custom serializable value type, as constrained by this overload.
         * @param vec Container whose existing elements are transferred in order; no length prefix is used.
         * @return *this for chaining.
         */
        template<scalar_type T>
        stream_writer& operator<<(const std::vector<T>& vec) {
            for (const auto& item : vec) {
                write<T>(item);
            }
            return *this;
        }

        /**
         * @brief Serialize the supplied value or all container elements in order.
         * @note No container count is emitted. Scalar fields use the configured byte order; custom insertion errors propagate.
         * @throws bstream_error If output fails; earlier elements can already be committed.
         * @tparam T Scalar, reflectable aggregate, or custom serializable value type, as constrained by this overload.
         * @param vec Container whose existing elements are transferred in order; no length prefix is used.
         * @return *this for chaining.
         */
        template<typename T>
            requires (!scalar_type<T>)
        stream_writer& operator<<(const std::vector<T>& vec) {
            for (const auto& item : vec) {
                *this << item;
            }
            return *this;
        }

        /**
         * @brief Serialize the supplied value or all container elements in order.
         * @note No container count is emitted. Scalar fields use the configured byte order; custom insertion errors propagate.
         * @throws bstream_error If output fails; earlier elements can already be committed.
         * @tparam T Scalar, reflectable aggregate, or custom serializable value type, as constrained by this overload.
         * @tparam N Number of array elements; no count is encoded on the wire.
         * @param arr Array whose elements are transferred in order; no length prefix is used.
         * @return *this for chaining.
         */
        template<scalar_type T, std::size_t N>
        stream_writer& operator<<(const std::array<T, N>& arr) {
            for (const auto& item : arr) {
                write<T>(item);
            }
            return *this;
        }

        /**
         * @brief Serialize the supplied value or all container elements in order.
         * @note No container count is emitted. Scalar fields use the configured byte order; custom insertion errors propagate.
         * @throws bstream_error If output fails; earlier elements can already be committed.
         * @tparam T Scalar, reflectable aggregate, or custom serializable value type, as constrained by this overload.
         * @tparam N Number of array elements; no count is encoded on the wire.
         * @param arr Array whose elements are transferred in order; no length prefix is used.
         * @return *this for chaining.
         */
        template<typename T, std::size_t N>
            requires (!scalar_type<T>)
        stream_writer& operator<<(const std::array<T, N>& arr) {
            for (const auto& item : arr) {
                *this << item;
            }
            return *this;
        }

        /**
         * @brief Serialize the supplied value or all container elements in order.
         * @note No container count is emitted. Scalar fields use the configured byte order; custom insertion errors propagate.
         * @throws bstream_error If output fails; earlier elements can already be committed.
         * @tparam T Scalar, reflectable aggregate, or custom serializable value type, as constrained by this overload.
         * @tparam N Number of array elements; no count is encoded on the wire.
         * @param arr Array whose elements are transferred in order; no length prefix is used.
         * @return *this for chaining.
         */
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

    /**
     * @brief Compatibility alias for stream_writer.
     * @ingroup stream_io
     */
    using binary_writer = stream_writer;

} // namespace bstream
