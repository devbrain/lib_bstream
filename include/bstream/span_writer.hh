/**
 * @file span_writer.hh
 * @brief Bounded binary output to borrowed memory.
 * @ingroup span_io
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
#include <span>
#include <string_view>
#include <vector>

namespace bstream {

    /**
     * @brief Bounded binary writer over borrowed mutable memory.
     * @ingroup span_io
     *
     * The destination never grows and must outlive the writer and returned views.
     * Byte order defaults to little endian. Seeking/skipping does not initialize
     * bytes. The written view ends at the current cursor, not the furthest position
     * previously written. Compound writes can commit a prefix before failure.
     * @snippet usage.cc span_roundtrip
     */
    class span_writer {
    public:
        /**
         * @brief Create a writer with an empty destination and cursor zero.
         */
        constexpr span_writer() noexcept = default;

        /**
         * @brief Borrow mutable storage and start writing at offset zero.
         * @pre Borrowed storage and streams outlive this object and all derived views.
         * @param buffer Borrowed mutable output storage; must remain alive and stable.
         * @param byte_order Byte order used for serialized scalar fields; defaults to little endian.
         */
        constexpr explicit span_writer(
            std::span<uint8_t> buffer,
            endian byte_order = endian::little) noexcept
            : buffer_(buffer), cursor_(0), endian_(byte_order) {}

        /**
         * @brief Borrow mutable storage and start writing at offset zero.
         * @pre Borrowed storage and streams outlive this object and all derived views.
         * @param buffer Borrowed mutable output storage; must remain alive and stable.
         * @param byte_order Byte order used for serialized scalar fields; defaults to little endian.
         */
        explicit span_writer(
            std::span<std::byte> buffer,
            endian byte_order = endian::little) noexcept
            : buffer_(reinterpret_cast<uint8_t*>(buffer.data()), buffer.size()),
              cursor_(0), endian_(byte_order) {}

        /**
         * @brief Borrow mutable storage and start writing at offset zero.
         * @pre Borrowed storage and streams outlive this object and all derived views.
         * @param buffer Borrowed mutable output storage; must remain alive and stable.
         * @param size Extent of the supplied storage in bytes.
         * @param byte_order Byte order used for serialized scalar fields; defaults to little endian.
         */
        constexpr span_writer(
            void* buffer,
            std::size_t size,
            endian byte_order = endian::little) noexcept
            : buffer_(static_cast<uint8_t*>(buffer), size),
              cursor_(0), endian_(byte_order) {}

        /**
         * @brief Query the current byte cursor.
         * @return Current offset in bytes.
         */
        [[nodiscard]] constexpr std::size_t tell() const noexcept { return cursor_; }
        /**
         * @brief Return the fixed output capacity in bytes.
         * @return The original buffer extent.
         */
        [[nodiscard]] constexpr std::size_t capacity() const noexcept { return buffer_.size(); }
        /**
         * @brief Return the number of bytes between the cursor and the end.
         * @return The remaining byte count.
         */
        [[nodiscard]] constexpr std::size_t remaining() const noexcept {
            return cursor_ < buffer_.size() ? (buffer_.size() - cursor_) : 0;
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
         * @brief View bytes from the buffer start to the current output cursor.
         * @note This borrows storage and shrinks after a backwards seek.
         * @return A borrowed view ending at the current cursor.
         */
        [[nodiscard]] std::span<const uint8_t> written_span() const noexcept {
            return buffer_.subspan(0, cursor_);
        }

        /**
         * @brief Set the absolute byte cursor.
         * @throws eof_error If the new cursor exceeds the fixed range; the cursor is unchanged.
         * @param pos Absolute byte offset from the start of the buffer or stream.
         */
        constexpr void seek(std::size_t pos) {
            if (pos > buffer_.size()) {
                throw eof_error("span_writer::seek past capacity");
            }
            cursor_ = pos;
        }

        /**
         * @brief Advance the cursor by bytes without transferring data.
         * @throws eof_error If the new cursor exceeds the fixed range; the cursor is unchanged.
         * @param bytes Number of bytes to consume or transfer.
         */
        constexpr void skip(std::size_t bytes) {
            if (remaining() < bytes) {
                throw eof_error("span_writer::skip past capacity");
            }
            cursor_ += bytes;
        }

        /**
         * @brief Write raw bytes without byte-order conversion.
         * @throws bstream_error If output fails.
         * @note A null source advances without initializing bytes; bounds failure leaves the cursor unchanged.
         * @param src Readable source covering the requested byte count, or null to advance without copying.
         * @param bytes Number of bytes to consume or transfer.
         */
        void write_exact(const void* src, std::size_t bytes) {
            if (remaining() < bytes) {
                throw eof_error("span_writer: buffer overflow");
            }
            if (bytes > 0 && src != nullptr) {
                std::memcpy(buffer_.data() + cursor_, src, bytes);
            }
            cursor_ += bytes;
        }

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
         * @brief Write string bytes followed by a zero byte.
         * @throws bstream_error If output fails; the payload may already have been emitted.
         * @param str String bytes to serialize; no character encoding conversion is performed.
         */
        void write_cstring(std::string_view str) {
            write_exact(str.data(), str.size());
            const uint8_t null_byte = 0;
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
            if (remaining() < fixed_length) {
                throw eof_error("span_writer: buffer overflow writing fixed string");
            }
            const std::size_t copy_len = std::min(str.size(), fixed_length);
            if (copy_len > 0) {
                std::memcpy(buffer_.data() + cursor_, str.data(), copy_len);
            }
            if (copy_len < fixed_length) {
                std::memset(buffer_.data() + cursor_ + copy_len, pad, fixed_length - copy_len);
            }
            cursor_ += fixed_length;
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
                throw parse_error("span_writer: string length does not fit its prefix");
            }
            write<LenType>(static_cast<LenType>(str.size()));
            write_exact(str.data(), str.size());
        }

        /**
         * @brief Serialize the supplied value or all container elements in order.
         * @note No container count is emitted. Scalar fields use the configured byte order; custom insertion errors propagate.
         * @throws bstream_error If output fails; earlier elements can already be committed.
         * @tparam T Scalar, reflectable aggregate, or custom serializable value type, as constrained by this overload.
         * @param val Value to serialize, or destination receiving a decoded value.
         * @return *this for chaining.
         */
        template<scalar_type T>
        span_writer& operator<<(const T& val) {
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
        span_writer& operator<<(const T& val) {
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
        span_writer& operator<<(const std::vector<T>& vec) {
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
        span_writer& operator<<(const std::vector<T>& vec) {
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
        span_writer& operator<<(const std::array<T, N>& arr) {
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
