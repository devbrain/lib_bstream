/**
 * @file memory_stream.hh
 * @brief Fixed memory buffers exposed as standard C++ streams.
 * @ingroup adapters
 */

#pragma once

#include <bstream/detail/platform.hh>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <ios>
#include <istream>
#include <limits>
#include <memory>
#include <ostream>
#include <span>
#include <streambuf>
#include <string>
#include <string_view>
#include <vector>

namespace bstream {

    // ========================================================================
    // span_streambuf
    // ========================================================================
    /**
     * @brief Fixed-capacity stream buffer borrowing a character range.
     * @ingroup adapters
     *
     * Input and output maintain independent positions. Open modes select which
     * areas are available. Storage must stay alive and stable. Const storage must
     * only be used in input mode. Seeking is limited to the original range; moving
     * preserves positions and leaves the source empty. No allocation or growth is
     * performed by this buffer. See @ref stream_adapters.
     * @snippet usage.cc memory_streams
     */
    class span_streambuf : public std::basic_streambuf<char> {
    public:
        /**
         * @brief Character type stored by this buffer.
         */
        using char_type = char;
        /**
         * @brief Character traits used by this buffer.
         */
        using traits_type = std::char_traits<char>;
        /**
         * @brief Character-or-EOF representation.
         */
        using int_type = traits_type::int_type;
        /**
         * @brief Absolute stream-position type.
         */
        using pos_type = traits_type::pos_type;
        /**
         * @brief Signed stream-offset type.
         */
        using off_type = traits_type::off_type;

        /**
         * @brief Create an empty input buffer.
         */
        span_streambuf() noexcept
            : buffer_(nullptr), size_(0), mode_(std::ios_base::in) {}

        /**
         * @brief Borrow the supplied range and initialize the enabled cursors at its beginning.
         * @pre Borrowed storage and streams outlive this object and all derived views.
         * @param data Borrowed backing range; must remain alive and stable. Output requires mutable storage.
         * @param size Extent of the supplied storage in bytes.
         * @param mode Input/output areas to enable; const storage must not enable output.
         */
        span_streambuf(const char* data, std::size_t size, std::ios_base::openmode mode = std::ios_base::in) noexcept
            : buffer_(const_cast<char*>(data)), size_(size), mode_(mode) {
            init_pointers();
        }

        /**
         * @brief Borrow the supplied range and initialize the enabled cursors at its beginning.
         * @pre Borrowed storage and streams outlive this object and all derived views.
         * @param data Borrowed backing range; must remain alive and stable. Output requires mutable storage.
         * @param size Extent of the supplied storage in bytes.
         * @param mode Input/output areas to enable; const storage must not enable output.
         */
        span_streambuf(char* data, std::size_t size, std::ios_base::openmode mode = std::ios_base::in | std::ios_base::out) noexcept
            : buffer_(data), size_(size), mode_(mode) {
            init_pointers();
        }

        // Deduce spans so (array, length) cannot select (span, mode) on platforms
        // where openmode is an integer type.
        /**
         * @brief Borrow the supplied range and initialize the enabled cursors at its beginning.
         * @pre Borrowed storage and streams outlive this object and all derived views.
         * @tparam Extent Static span extent or std::dynamic_extent.
         * @param s Borrowed backing range; must remain alive and stable. Output requires mutable storage.
         * @param mode Input/output areas to enable; const storage must not enable output.
         */
        template<std::size_t Extent>
        explicit span_streambuf(std::span<const char, Extent> s, std::ios_base::openmode mode = std::ios_base::in) noexcept
            : span_streambuf(s.data(), s.size(), mode) {}

        /**
         * @brief Borrow the supplied range and initialize the enabled cursors at its beginning.
         * @pre Borrowed storage and streams outlive this object and all derived views.
         * @tparam Extent Static span extent or std::dynamic_extent.
         * @param s Borrowed backing range; must remain alive and stable. Output requires mutable storage.
         * @param mode Input/output areas to enable; const storage must not enable output.
         */
        template<std::size_t Extent>
        explicit span_streambuf(std::span<char, Extent> s, std::ios_base::openmode mode = std::ios_base::in | std::ios_base::out) noexcept
            : span_streambuf(s.data(), s.size(), mode) {}

        /**
         * @brief Copy construction is disabled; this object carries cursor or output state.
         */
        span_streambuf(const span_streambuf&) = delete;
        /**
         * @brief Copy assignment is disabled.
         */
        span_streambuf& operator=(const span_streambuf&) = delete;

        /**
         * @brief Transfer the source buffer, cursor, and pending state; leave the source empty.
         * @param other Source buffer; positions are preserved and its pointers are cleared.
         */
        span_streambuf(span_streambuf&& other) noexcept
            : std::basic_streambuf<char>(other),
              buffer_(other.buffer_), size_(other.size_), mode_(other.mode_) {
            other.buffer_ = nullptr;
            other.size_ = 0;
            other.setg(nullptr, nullptr, nullptr);
            other.setp(nullptr, nullptr);
        }

        /**
         * @brief Transfer the source storage, modes, and independent get/put positions.
         * @note The old borrowed buffer is released without modifying its bytes; source pointers are cleared.
         * @param other Source buffer; positions are preserved and its pointers are cleared.
         * @return *this.
         */
        span_streambuf& operator=(span_streambuf&& other) noexcept {
            if (this != &other) {
                std::basic_streambuf<char>::operator=(other);
                buffer_ = other.buffer_;
                size_ = other.size_;
                mode_ = other.mode_;
                other.buffer_ = nullptr;
                other.size_ = 0;
                other.setg(nullptr, nullptr, nullptr);
                other.setp(nullptr, nullptr);
            }
            return *this;
        }

        /**
         * @brief Destroy the wrapper without releasing borrowed storage.
         */
        ~span_streambuf() override = default;

        /**
         * @brief View the entire original backing range, independent of the cursor.
         * @return A borrowed view of the full buffer.
         */
        [[nodiscard]] std::span<const char> span() const noexcept {
            return {buffer_, size_};
        }

        /**
         * @brief View the entire original backing range, independent of the cursor.
         * @return A borrowed view of the full buffer.
         */
        [[nodiscard]] std::span<char> span() noexcept {
            return {buffer_, size_};
        }

        /**
         * @brief Return the current output cursor relative to the buffer start.
         * @note Seeking backwards reduces this count; it is not a high-water mark.
         * @return The current byte position, or zero for an absent output area.
         */
        [[nodiscard]] std::streamsize chars_written() const noexcept {
            return pptr() ? pptr() - pbase() : 0;
        }

        /**
         * @brief View bytes from the buffer start to the current output cursor.
         * @note This borrows storage and shrinks after a backwards seek.
         * @return A borrowed view ending at the current cursor.
         */
        [[nodiscard]] std::span<const char> written_span() const noexcept {
            return {pbase(), static_cast<std::size_t>(chars_written())};
        }

        /**
         * @brief Rewind the enabled get/put areas without modifying buffer bytes.
         */
        void reset() noexcept {
            init_pointers();
        }

    protected:
        /**
         * @brief Expose the next input character.
         * @return The next character, or traits_type::eof() at end of input.
         */
        int_type underflow() override {
            if (gptr() && gptr() < egptr()) {
                return traits_type::to_int_type(*gptr());
            }
            return traits_type::eof();
        }

        /**
         * @brief Write one character to the output area or each tee destination.
         * @param c Character to write/put back, or traits_type::eof().
         * @return The accepted character/non-EOF status on success, or traits_type::eof() on failure.
         */
        int_type overflow(int_type c) override {
            if (!(mode_ & std::ios_base::out)) {
                return traits_type::eof();
            }
            if (pptr() && pptr() < epptr()) {
                if (c != traits_type::eof()) {
                    *pptr() = traits_type::to_char_type(c);
                    pbump(1);
                    return c;
                }
                return traits_type::not_eof(c);
            }
            return traits_type::eof();
        }

        /**
         * @brief Back up one character when possible; reject replacement unless output is enabled.
         * @param c Character to write/put back, or traits_type::eof().
         * @return The character at the new input position, or traits_type::eof() on failure.
         */
        int_type pbackfail(int_type c) override {
            if (gptr() && gptr() > eback()) {
                if (c != traits_type::eof() && !(mode_ & std::ios_base::out) &&
                    !traits_type::eq(traits_type::to_char_type(c), gptr()[-1])) {
                    return traits_type::eof();
                }
                gbump(-1);
                if (c != traits_type::eof() && (mode_ & std::ios_base::out)) {
                    *gptr() = traits_type::to_char_type(c);
                }
                return traits_type::to_int_type(*gptr());
            }
            return traits_type::eof();
        }

        /**
         * @brief Transfer up to count raw characters.
         * @param s Borrowed backing range; must remain alive and stable. Output requires mutable storage.
         * @param count Requested character count; nonpositive counts transfer nothing.
         * @return The number transferred; tee output returns zero if any destination fails.
         */
        std::streamsize xsgetn(char_type* s, std::streamsize count) override {
            if (!gptr() || count <= 0) return 0;
            std::streamsize avail = egptr() - gptr();
            std::streamsize n = std::min(avail, count);
            std::memcpy(s, gptr(), static_cast<std::size_t>(n));
            setg(eback(), gptr() + n, egptr());
            return n;
        }

        /**
         * @brief Transfer up to count raw characters.
         * @param s Borrowed backing range; must remain alive and stable. Output requires mutable storage.
         * @param count Requested character count; nonpositive counts transfer nothing.
         * @return The number transferred; tee output returns zero if any destination fails.
         */
        std::streamsize xsputn(const char_type* s, std::streamsize count) override {
            if (!pptr() || count <= 0 || !(mode_ & std::ios_base::out)) return 0;
            std::streamsize avail = epptr() - pptr();
            std::streamsize n = std::min(avail, count);
            std::memcpy(pptr(), s, static_cast<std::size_t>(n));
            advance_put(n);
            return n;
        }

        /**
         * @brief Seek within the fixed backing range.
         * @note Seeking input and output together relative to the current position is rejected. No positions change on a bounds failure.
         * @param off Signed displacement relative to way.
         * @param way Origin: beginning, current position, or end.
         * @param which Input and/or output positions to change.
         * @return The new position, or pos_type(off_type(-1)) on failure.
         */
        pos_type seekoff(off_type off, std::ios_base::seekdir way,
                        std::ios_base::openmode which = std::ios_base::in | std::ios_base::out) override {
            const pos_type fail_pos = off_type(-1);
            const auto directions = which & (std::ios_base::in | std::ios_base::out);
            if (directions == std::ios_base::openmode{} || (directions & mode_) != directions ||
                (way != std::ios_base::beg && way != std::ios_base::cur && way != std::ios_base::end) ||
                (way == std::ios_base::cur && directions == (std::ios_base::in | std::ios_base::out))) {
                return fail_pos;
            }
            // Empty enabled areas have a valid position zero even with null cursors.
            if (size_ == 0) return off == 0 ? pos_type(0) : fail_pos;

            off_type target_in = -1;
            off_type target_out = -1;

            if (which & std::ios_base::in) {
                if (!gptr()) return fail_pos;
                off_type base_pos = 0;
                if (way == std::ios_base::beg) {
                    base_pos = 0;
                } else if (way == std::ios_base::cur) {
                    base_pos = gptr() - eback();
                } else {
                    base_pos = egptr() - eback();
                }
                if (off < -base_pos || off > (egptr() - eback()) - base_pos) {
                    return fail_pos;
                }
                target_in = base_pos + off;
            }

            if (which & std::ios_base::out) {
                if (!pptr()) return fail_pos;
                off_type base_pos = 0;
                if (way == std::ios_base::beg) {
                    base_pos = 0;
                } else if (way == std::ios_base::cur) {
                    base_pos = pptr() - pbase();
                } else {
                    base_pos = epptr() - pbase();
                }
                if (off < -base_pos || off > (epptr() - pbase()) - base_pos) {
                    return fail_pos;
                }
                target_out = base_pos + off;
            }

            if (target_in != -1) {
                setg(eback(), eback() + target_in, egptr());
            }
            if (target_out != -1) {
                setp(pbase(), epptr());
                advance_put(target_out);
            }

            return (target_in != -1) ? pos_type(target_in) : pos_type(target_out);
        }

        /**
         * @brief Seek within the fixed backing range.
         * @note Seeking input and output together relative to the current position is rejected. No positions change on a bounds failure.
         * @param sp Absolute target stream position.
         * @param which Input and/or output positions to change.
         * @return The new position, or pos_type(off_type(-1)) on failure.
         */
        pos_type seekpos(pos_type sp,
                         std::ios_base::openmode which = std::ios_base::in | std::ios_base::out) override {
            return seekoff(off_type(sp), std::ios_base::beg, which);
        }

        /**
         * @brief Synchronize this memory buffer (no operation).
         * @return Zero on success, or -1 if a tee destination fails to flush.
         */
        int sync() override {
            return 0;
        }

    private:
        // basic_streambuf::pbump takes int even when stream offsets are wider.
        void advance_put(std::streamsize count) noexcept {
            constexpr auto chunk = std::numeric_limits<int>::max();
            while (count > chunk) {
                pbump(chunk);
                count -= chunk;
            }
            pbump(static_cast<int>(count));
        }

        void init_pointers() noexcept {
            if (buffer_ && size_ > 0) {
                if (mode_ & std::ios_base::in) {
                    setg(buffer_, buffer_, buffer_ + size_);
                } else {
                    setg(nullptr, nullptr, nullptr);
                }
                if (mode_ & std::ios_base::out) {
                    setp(buffer_, buffer_ + size_);
                } else {
                    setp(nullptr, nullptr);
                }
            } else {
                setg(nullptr, nullptr, nullptr);
                setp(nullptr, nullptr);
            }
        }

        char* buffer_;
        std::size_t size_;
        std::ios_base::openmode mode_;
    };

    // ========================================================================
    // detail::span_streambuf_holder
    // ========================================================================
    namespace detail {
        struct span_istreambuf_holder {
            span_streambuf buf_;
            explicit span_istreambuf_holder(std::span<const char> s) noexcept
                : buf_(s, std::ios_base::in) {}
        };

        struct span_ostreambuf_holder {
            span_streambuf buf_;
            explicit span_ostreambuf_holder(std::span<char> s) noexcept
                : buf_(s, std::ios_base::out) {}
        };
    } // namespace detail

    // ========================================================================
    // span_istream (C++20 zero-copy istream over span / buffer)
    // ========================================================================
    /**
     * @brief Standard input stream backed by borrowed contiguous bytes.
     * @ingroup adapters
     *
     * Constructors do not copy their input. Keep the buffer, string, or vector
     * alive and at a stable address. Seeking is supported within the full range.
     * Stream state and exception masks follow std::istream conventions.
     * @snippet usage.cc memory_streams
     */
    class span_istream : private detail::span_istreambuf_holder, public std::istream {
    public:
        /**
         * @brief Borrow the supplied range and initialize the enabled cursors at its beginning.
         * @pre Borrowed storage and streams outlive this object and all derived views.
         * @param s Borrowed backing range; must remain alive and stable. Output requires mutable storage.
         */
        explicit span_istream(std::span<const char> s) noexcept
            : detail::span_istreambuf_holder(s), std::istream(&buf_) {}

        /**
         * @brief Borrow the supplied range and initialize the enabled cursors at its beginning.
         * @pre Borrowed storage and streams outlive this object and all derived views.
         * @param s Borrowed backing range; must remain alive and stable. Output requires mutable storage.
         */
        explicit span_istream(std::span<const std::byte> s) noexcept
            : span_istream(std::span<const char>(reinterpret_cast<const char*>(s.data()), s.size())) {}

        /**
         * @brief Borrow the supplied range and initialize the enabled cursors at its beginning.
         * @pre Borrowed storage and streams outlive this object and all derived views.
         * @param s Borrowed backing range; must remain alive and stable. Output requires mutable storage.
         */
        explicit span_istream(std::span<const uint8_t> s) noexcept
            : span_istream(std::span<const char>(reinterpret_cast<const char*>(s.data()), s.size())) {}

        /**
         * @brief Borrow the supplied range and initialize the enabled cursors at its beginning.
         * @pre Borrowed storage and streams outlive this object and all derived views.
         * @param sv Borrowed backing range; must remain alive and stable. Output requires mutable storage.
         */
        explicit span_istream(std::string_view sv) noexcept
            : span_istream(std::span<const char>(sv.data(), sv.size())) {}

        /**
         * @brief Borrow the supplied range and initialize the enabled cursors at its beginning.
         * @pre Borrowed storage and streams outlive this object and all derived views.
         * @param str Borrowed backing range; must remain alive and stable. Output requires mutable storage.
         */
        explicit span_istream(const std::string& str) noexcept
            : span_istream(std::string_view(str)) {}

        /**
         * @brief Borrow the supplied range and initialize the enabled cursors at its beginning.
         * @pre Borrowed storage and streams outlive this object and all derived views.
         * @param data Borrowed backing range; must remain alive and stable. Output requires mutable storage.
         * @param size Extent of the supplied storage in bytes.
         */
        span_istream(const char* data, std::size_t size) noexcept
            : span_istream(std::span<const char>(data, size)) {}

        /**
         * @brief Borrow the supplied range and initialize the enabled cursors at its beginning.
         * @pre Borrowed storage and streams outlive this object and all derived views.
         * @param data Borrowed backing range; must remain alive and stable. Output requires mutable storage.
         * @param size Extent of the supplied storage in bytes.
         */
        span_istream(const void* data, std::size_t size) noexcept
            : span_istream(std::span<const char>(static_cast<const char*>(data), size)) {}

        /**
         * @brief Borrow the supplied range and initialize the enabled cursors at its beginning.
         * @pre Borrowed storage and streams outlive this object and all derived views.
         * @tparam Alloc Allocator type of the borrowed vector.
         * @param vec Borrowed backing range; must remain alive and stable. Output requires mutable storage.
         */
        template<typename Alloc>
        explicit span_istream(const std::vector<char, Alloc>& vec) noexcept
            : span_istream(std::span<const char>(vec.data(), vec.size())) {}

        /**
         * @brief Borrow the supplied range and initialize the enabled cursors at its beginning.
         * @pre Borrowed storage and streams outlive this object and all derived views.
         * @tparam Alloc Allocator type of the borrowed vector.
         * @param vec Borrowed backing range; must remain alive and stable. Output requires mutable storage.
         */
        template<typename Alloc>
        explicit span_istream(const std::vector<uint8_t, Alloc>& vec) noexcept
            : span_istream(std::span<const uint8_t>(vec.data(), vec.size())) {}

        /**
         * @brief Borrow the supplied range and initialize the enabled cursors at its beginning.
         * @pre Borrowed storage and streams outlive this object and all derived views.
         * @tparam Alloc Allocator type of the borrowed vector.
         * @param vec Borrowed backing range; must remain alive and stable. Output requires mutable storage.
         */
        template<typename Alloc>
        explicit span_istream(const std::vector<std::byte, Alloc>& vec) noexcept
            : span_istream(std::span<const std::byte>(vec.data(), vec.size())) {}

        /**
         * @brief Access this stream's embedded buffer; does not transfer ownership.
         * @return A pointer to the embedded stream buffer.
         */
        [[nodiscard]] span_streambuf* rdbuf() noexcept { return &buf_; }
        /**
         * @brief Access this stream's embedded buffer; does not transfer ownership.
         * @return A pointer to the embedded stream buffer.
         */
        [[nodiscard]] const span_streambuf* rdbuf() const noexcept { return &buf_; }

        /**
         * @brief View the entire original backing range, independent of the cursor.
         * @return A borrowed view of the full buffer.
         */
        [[nodiscard]] std::span<const char> span() const noexcept { return buf_.span(); }
        /**
         * @brief Rewind this stream and clear its state flags without modifying buffer bytes.
         */
        void reset() noexcept { buf_.reset(); clear(); }
    };

    // ========================================================================
    // span_ostream (C++20 zero-copy ostream over span / buffer)
    // ========================================================================
    /**
     * @brief Standard output stream backed by borrowed fixed-capacity bytes.
     * @ingroup adapters
     *
     * Constructors do not allocate or resize the destination. The supplied vector's
     * size, not capacity, is the writable extent. Seeking backwards shortens written
     * views; those views reflect the put position, not a high-water mark. Overflow
     * sets stream failure state. Keep all backing storage alive and stable.
     * @snippet usage.cc memory_streams
     */
    class span_ostream : private detail::span_ostreambuf_holder, public std::ostream {
    public:
        /**
         * @brief Borrow the supplied range and initialize the enabled cursors at its beginning.
         * @pre Borrowed storage and streams outlive this object and all derived views.
         * @param s Borrowed backing range; must remain alive and stable. Output requires mutable storage.
         */
        explicit span_ostream(std::span<char> s) noexcept
            : detail::span_ostreambuf_holder(s), std::ostream(&buf_) {}

        /**
         * @brief Borrow the supplied range and initialize the enabled cursors at its beginning.
         * @pre Borrowed storage and streams outlive this object and all derived views.
         * @param s Borrowed backing range; must remain alive and stable. Output requires mutable storage.
         */
        explicit span_ostream(std::span<std::byte> s) noexcept
            : span_ostream(std::span<char>(reinterpret_cast<char*>(s.data()), s.size())) {}

        /**
         * @brief Borrow the supplied range and initialize the enabled cursors at its beginning.
         * @pre Borrowed storage and streams outlive this object and all derived views.
         * @param s Borrowed backing range; must remain alive and stable. Output requires mutable storage.
         */
        explicit span_ostream(std::span<uint8_t> s) noexcept
            : span_ostream(std::span<char>(reinterpret_cast<char*>(s.data()), s.size())) {}

        /**
         * @brief Borrow the supplied range and initialize the enabled cursors at its beginning.
         * @pre Borrowed storage and streams outlive this object and all derived views.
         * @param data Borrowed backing range; must remain alive and stable. Output requires mutable storage.
         * @param size Extent of the supplied storage in bytes.
         */
        span_ostream(char* data, std::size_t size) noexcept
            : span_ostream(std::span<char>(data, size)) {}

        /**
         * @brief Borrow the supplied range and initialize the enabled cursors at its beginning.
         * @pre Borrowed storage and streams outlive this object and all derived views.
         * @param data Borrowed backing range; must remain alive and stable. Output requires mutable storage.
         * @param size Extent of the supplied storage in bytes.
         */
        span_ostream(void* data, std::size_t size) noexcept
            : span_ostream(std::span<char>(static_cast<char*>(data), size)) {}

        /**
         * @brief Borrow the supplied range and initialize the enabled cursors at its beginning.
         * @pre Borrowed storage and streams outlive this object and all derived views.
         * @tparam Alloc Allocator type of the borrowed vector.
         * @param vec Borrowed backing range; must remain alive and stable. Output requires mutable storage.
         */
        template<typename Alloc>
        explicit span_ostream(std::vector<char, Alloc>& vec) noexcept
            : span_ostream(std::span<char>(vec.data(), vec.size())) {}

        /**
         * @brief Borrow the supplied range and initialize the enabled cursors at its beginning.
         * @pre Borrowed storage and streams outlive this object and all derived views.
         * @tparam Alloc Allocator type of the borrowed vector.
         * @param vec Borrowed backing range; must remain alive and stable. Output requires mutable storage.
         */
        template<typename Alloc>
        explicit span_ostream(std::vector<uint8_t, Alloc>& vec) noexcept
            : span_ostream(std::span<uint8_t>(vec.data(), vec.size())) {}

        /**
         * @brief Borrow the supplied range and initialize the enabled cursors at its beginning.
         * @pre Borrowed storage and streams outlive this object and all derived views.
         * @tparam Alloc Allocator type of the borrowed vector.
         * @param vec Borrowed backing range; must remain alive and stable. Output requires mutable storage.
         */
        template<typename Alloc>
        explicit span_ostream(std::vector<std::byte, Alloc>& vec) noexcept
            : span_ostream(std::span<std::byte>(vec.data(), vec.size())) {}

        /**
         * @brief Access this stream's embedded buffer; does not transfer ownership.
         * @return A pointer to the embedded stream buffer.
         */
        [[nodiscard]] span_streambuf* rdbuf() noexcept { return &buf_; }
        /**
         * @brief Access this stream's embedded buffer; does not transfer ownership.
         * @return A pointer to the embedded stream buffer.
         */
        [[nodiscard]] const span_streambuf* rdbuf() const noexcept { return &buf_; }

        /**
         * @brief Return the current output cursor relative to the buffer start.
         * @note Seeking backwards reduces this count; it is not a high-water mark.
         * @return The current byte position, or zero for an absent output area.
         */
        [[nodiscard]] std::streamsize chars_written() const noexcept {
            return buf_.chars_written();
        }

        /**
         * @brief View bytes from the buffer start to the current output cursor.
         * @note This borrows storage and shrinks after a backwards seek.
         * @return A borrowed view ending at the current cursor.
         */
        [[nodiscard]] std::span<const char> written_span() const noexcept {
            return buf_.written_span();
        }

        /**
         * @brief View output bytes as characters, including any embedded zeros.
         * @return A borrowed string view ending at the current output cursor.
         */
        [[nodiscard]] std::string_view written_string_view() const noexcept {
            return {buf_.written_span().data(), buf_.written_span().size()};
        }

        /**
         * @brief Rewind this stream and clear its state flags without modifying buffer bytes.
         */
        void reset() noexcept { buf_.reset(); clear(); }
    };

    // Drop-in aliases for bsw::io
    /**
     * @brief Compatibility alias for span_streambuf.
     * @ingroup adapters
     */
    using memory_stream_buf = span_streambuf;
    /**
     * @brief Compatibility alias for span_istream.
     * @ingroup adapters
     */
    using memory_input_stream = span_istream;
    /**
     * @brief Compatibility alias for span_ostream.
     * @ingroup adapters
     */
    using memory_output_stream = span_ostream;

} // namespace bstream
