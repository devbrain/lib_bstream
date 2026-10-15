/**
 * @file span_reader.hh
 * @brief Bounded, zero-copy binary input over borrowed memory.
 * @ingroup span_io
 */

#pragma once

#include <bstream/detail/platform.hh>

#include <bstream/byte_order.hh>
#include <bstream/detail/endian_scope.hh>
#include <bstream/error.hh>
#include <bstream/pfr_adapter.hh>

#include <array>
#include <cstring>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace bstream {

    /**
     * @brief Bounded binary reader over borrowed contiguous memory.
     * @ingroup span_io
     *
     * The backing memory must remain alive and unchanged in location. The cursor
     * starts at zero and byte order defaults to little endian. Raw reads and slices
     * check their full extent before advancing. Typed records and strings can make
     * partial progress before a later field fails. See @ref serialization and
     * @ref error_guide. Scalar and container endian conversion uses byte_order.hh.
     * @snippet usage.cc span_roundtrip
     */
    class span_reader {
    public:
        // ====================================================================
        // Constructors
        // ====================================================================
        /**
         * @brief Create an empty input buffer.
         */
        constexpr span_reader() noexcept = default;

        /**
         * @brief Borrow contiguous input storage and start reading at offset zero.
         * @pre Borrowed storage and streams outlive this object and all derived views.
         * @param data Borrowed contiguous input storage; must remain alive and stable.
         * @param byte_order Byte order used for serialized scalar fields; defaults to little endian.
         */
        constexpr explicit span_reader(
            std::span<const uint8_t> data,
            endian byte_order = endian::little) noexcept
            : data_(data), cursor_(0), endian_(byte_order) {}

        /**
         * @brief Borrow contiguous input storage and start reading at offset zero.
         * @pre Borrowed storage and streams outlive this object and all derived views.
         * @param data Borrowed contiguous input storage; must remain alive and stable.
         * @param byte_order Byte order used for serialized scalar fields; defaults to little endian.
         */
        explicit span_reader(
            std::span<const std::byte> data,
            endian byte_order = endian::little) noexcept
            : data_(reinterpret_cast<const uint8_t*>(data.data()), data.size()),
              cursor_(0), endian_(byte_order) {}

        /**
         * @brief Borrow contiguous input storage and start reading at offset zero.
         * @pre Borrowed storage and streams outlive this object and all derived views.
         * @param data Borrowed contiguous input storage; must remain alive and stable.
         * @param byte_order Byte order used for serialized scalar fields; defaults to little endian.
         */
        explicit span_reader(
            std::span<const char> data,
            endian byte_order = endian::little) noexcept
            : data_(reinterpret_cast<const uint8_t*>(data.data()), data.size()),
              cursor_(0), endian_(byte_order) {}

        /**
         * @brief Borrow contiguous input storage and start reading at offset zero.
         * @pre Borrowed storage and streams outlive this object and all derived views.
         * @param data Borrowed contiguous input storage; must remain alive and stable.
         * @param size Extent of the supplied storage in bytes.
         * @param byte_order Byte order used for serialized scalar fields; defaults to little endian.
         */
        constexpr span_reader(
            const void* data,
            std::size_t size,
            endian byte_order = endian::little) noexcept
            : data_(static_cast<const uint8_t*>(data), size),
              cursor_(0), endian_(byte_order) {}

        /**
         * @brief Borrow contiguous input storage and start reading at offset zero.
         * @pre Borrowed storage and streams outlive this object and all derived views.
         * @tparam Container Contiguous container with data() and size(); its storage is borrowed.
         * @param c Borrowed contiguous container; its byte extent is size() times sizeof(*data()).
         * @param byte_order Byte order used for serialized scalar fields; defaults to little endian.
         */
        template<typename Container>
            requires requires(const Container& c) {
                { c.data() } -> std::convertible_to<const void*>;
                { c.size() } -> std::convertible_to<std::size_t>;
                requires std::is_pointer_v<decltype(c.data())>;
                requires std::is_object_v<std::remove_pointer_t<decltype(c.data())>>;
            } && (!std::is_same_v<std::decay_t<Container>, span_reader>)
        constexpr explicit span_reader(
            const Container& c,
            endian byte_order = endian::little) noexcept
            : data_(static_cast<const uint8_t*>(static_cast<const void*>(c.data())),
                    c.size() * sizeof(*c.data())),
              cursor_(0), endian_(byte_order) {}

        // ====================================================================
        // Navigation & State
        // ====================================================================
        /**
         * @brief Query the current byte cursor.
         * @return Current offset in bytes.
         */
        [[nodiscard]] constexpr std::size_t tell() const noexcept { return cursor_; }
        /**
         * @brief Return the full input extent in bytes.
         * @return The original buffer extent.
         */
        [[nodiscard]] constexpr std::size_t size() const noexcept { return data_.size(); }
        /**
         * @brief Return the number of bytes between the cursor and the end.
         * @return The remaining byte count.
         */
        [[nodiscard]] constexpr std::size_t remaining() const noexcept {
            return cursor_ < data_.size() ? (data_.size() - cursor_) : 0;
        }
        /**
         * @brief Check for exhausted input.
         * @return True if no readable input remains.
         */
        [[nodiscard]] constexpr bool eof() const noexcept { return cursor_ >= data_.size(); }
        /**
         * @brief Test whether input remains.
         * @return True when input remains.
         */
        constexpr explicit operator bool() const noexcept { return !eof(); }
        /**
         * @brief Negate this wrapper's boolean state.
         * @return True when the wrapper's boolean conversion is false.
         */
        constexpr bool operator!() const noexcept { return eof(); }

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
         * @throws eof_error If the new cursor exceeds the fixed range; the cursor is unchanged.
         * @param pos Absolute byte offset from the start of the buffer or stream.
         */
        constexpr void seek(std::size_t pos) {
            if (pos > data_.size()) {
                throw eof_error("span_reader::seek past end of buffer");
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
                throw eof_error("span_reader::skip past end of buffer");
            }
            cursor_ += bytes;
        }

        /**
         * @brief Return a borrowed pointer at the current input cursor.
         * @note The pointer may be one-past-the-end; it does not establish readable capacity.
         * @return The current pointer into the backing storage.
         */
        [[nodiscard]] constexpr const uint8_t* current_ptr() const noexcept {
            return cursor_ == 0 ? data_.data() : data_.data() + cursor_;
        }

        // ====================================================================
        // Raw Reads & Zero-Copy Slicing
        // ====================================================================
        /**
         * @brief Consume exactly the requested number of raw bytes without byte-order conversion.
         * @throws eof_error If the complete range cannot be read.
         * @note A null destination discards bytes. Bounds failure leaves the cursor unchanged.
         * @param dst Writable destination covering the requested byte count, or null to discard the bytes.
         * @param bytes Number of bytes to consume or transfer.
         */
        void read_exact(void* dst, std::size_t bytes) {
            if (remaining() < bytes) {
                throw eof_error("span_reader: unexpected EOF reading raw bytes");
            }
            if (bytes > 0 && dst != nullptr) {
                std::memcpy(dst, current_ptr(), bytes);
            }
            cursor_ += bytes;
        }

        /**
         * @brief Return a zero-copy slice and advance by its size.
         * @throws eof_error If the full slice is unavailable; the cursor is unchanged.
         * @note The view borrows the original storage.
         * @param bytes Number of bytes to consume or transfer.
         * @return A borrowed view of the requested bytes.
         */
        [[nodiscard]] std::span<const uint8_t> read_span(std::size_t bytes) {
            if (remaining() < bytes) {
                throw eof_error("span_reader: unexpected EOF reading subspan");
            }
            std::span<const uint8_t> sub(current_ptr(), bytes);
            cursor_ += bytes;
            return sub;
        }

        /**
         * @brief Create a bounded child reader over the next bytes and advance the parent.
         * @throws eof_error If the requested extent is unavailable.
         * @note The child has its own cursor, inherits byte order, and borrows the same storage.
         * @param bytes Number of bytes to consume or transfer.
         * @return A reader restricted to the requested slice.
         */
        [[nodiscard]] span_reader subreader(std::size_t bytes) {
            return span_reader(read_span(bytes), endian_);
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
                return detail::read_aggregate<span_reader, T>(*this);
            } else {
                T val{};
                *this >> val;
                return val;
            }
        }

        /**
         * @brief Decode a scalar without consuming it on success.
         * @throws eof_error If insufficient input is available.
         * @tparam T Scalar, reflectable aggregate, or custom serializable value type, as constrained by this overload.
         * @return The decoded scalar in the configured byte order.
         */
        template<scalar_type T>
        [[nodiscard]] T peek() const {
            if (remaining() < sizeof(T)) {
                throw eof_error("span_reader: unexpected EOF peeking value");
            }
            T val{};
            std::memcpy(&val, current_ptr(), sizeof(T));
            return bstream::byte_order::from_endian(val, endian_);
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
            const std::size_t rem = remaining();
            if (rem == 0) {
                throw eof_error("span_reader: null-terminator not found before EOF");
            }
            const uint8_t* p = current_ptr();
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

        /**
         * @brief Consume an exactly sized string field.
         * @throws eof_error If the complete field is unavailable.
         * @param length Number of bytes in the field or raw character range.
         * @param strip_nulls If true, return only the bytes before the first zero; always consume the full field.
         * @return An owned string, optionally truncated at its first zero.
         */
        [[nodiscard]] std::string read_fixed_string(std::size_t length, bool strip_nulls = true) {
            if (length == 0) return {};
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
        span_reader& operator>>(T& val) {
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
        span_reader& operator>>(T& val) {
            val = detail::read_aggregate<span_reader, T>(*this);
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
        span_reader& operator>>(std::vector<T>& vec) {
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
        span_reader& operator>>(std::vector<T>& vec) {
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
        span_reader& operator>>(std::array<T, N>& arr) {
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
        span_reader& operator>>(std::array<T, N>& arr) {
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
        span_reader& operator>>(T (&arr)[N]) {
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
        std::span<const uint8_t> data_{};
        std::size_t cursor_{0};
        endian endian_{endian::little};
    };

} // namespace bstream
