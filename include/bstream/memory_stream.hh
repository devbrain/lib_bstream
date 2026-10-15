#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <ios>
#include <istream>
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
    class span_streambuf : public std::basic_streambuf<char> {
    public:
        using char_type = char;
        using traits_type = std::char_traits<char>;
        using int_type = traits_type::int_type;
        using pos_type = traits_type::pos_type;
        using off_type = traits_type::off_type;

        span_streambuf() noexcept
            : buffer_(nullptr), size_(0), mode_(std::ios_base::in) {}

        span_streambuf(const char* data, std::size_t size, std::ios_base::openmode mode = std::ios_base::in) noexcept
            : buffer_(const_cast<char*>(data)), size_(size), mode_(mode) {
            init_pointers();
        }

        span_streambuf(char* data, std::size_t size, std::ios_base::openmode mode = std::ios_base::in | std::ios_base::out) noexcept
            : buffer_(data), size_(size), mode_(mode) {
            init_pointers();
        }

        explicit span_streambuf(std::span<const char> s, std::ios_base::openmode mode = std::ios_base::in) noexcept
            : span_streambuf(s.data(), s.size(), mode) {}

        explicit span_streambuf(std::span<char> s, std::ios_base::openmode mode = std::ios_base::in | std::ios_base::out) noexcept
            : span_streambuf(s.data(), s.size(), mode) {}

        span_streambuf(const span_streambuf&) = delete;
        span_streambuf& operator=(const span_streambuf&) = delete;

        span_streambuf(span_streambuf&& other) noexcept
            : buffer_(other.buffer_), size_(other.size_), mode_(other.mode_) {
            init_pointers();
            other.buffer_ = nullptr;
            other.size_ = 0;
            other.setg(nullptr, nullptr, nullptr);
            other.setp(nullptr, nullptr);
        }

        span_streambuf& operator=(span_streambuf&& other) noexcept {
            if (this != &other) {
                buffer_ = other.buffer_;
                size_ = other.size_;
                mode_ = other.mode_;
                init_pointers();
                other.buffer_ = nullptr;
                other.size_ = 0;
                other.setg(nullptr, nullptr, nullptr);
                other.setp(nullptr, nullptr);
            }
            return *this;
        }

        ~span_streambuf() override = default;

        [[nodiscard]] std::span<const char> span() const noexcept {
            return {buffer_, size_};
        }

        [[nodiscard]] std::span<char> span() noexcept {
            return {buffer_, size_};
        }

        [[nodiscard]] std::streamsize chars_written() const noexcept {
            return pptr() ? static_cast<std::streamsize>(pptr() - pbase()) : 0;
        }

        [[nodiscard]] std::span<const char> written_span() const noexcept {
            return {pbase(), static_cast<std::size_t>(chars_written())};
        }

        void reset() noexcept {
            init_pointers();
        }

    protected:
        int_type underflow() override {
            if (gptr() && gptr() < egptr()) {
                return traits_type::to_int_type(*gptr());
            }
            return traits_type::eof();
        }

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

        int_type pbackfail(int_type c) override {
            if (gptr() && gptr() > eback()) {
                gbump(-1);
                if (c != traits_type::eof() && (mode_ & std::ios_base::out)) {
                    *gptr() = traits_type::to_char_type(c);
                }
                return traits_type::to_int_type(*gptr());
            }
            return traits_type::eof();
        }

        std::streamsize xsgetn(char_type* s, std::streamsize count) override {
            if (!gptr() || count <= 0) return 0;
            std::streamsize avail = egptr() - gptr();
            std::streamsize n = std::min(avail, count);
            std::memcpy(s, gptr(), static_cast<std::size_t>(n));
            gbump(static_cast<int>(n));
            return n;
        }

        std::streamsize xsputn(const char_type* s, std::streamsize count) override {
            if (!pptr() || count <= 0 || !(mode_ & std::ios_base::out)) return 0;
            std::streamsize avail = epptr() - pptr();
            std::streamsize n = std::min(avail, count);
            std::memcpy(pptr(), s, static_cast<std::size_t>(n));
            pbump(static_cast<int>(n));
            return n;
        }

        pos_type seekoff(off_type off, std::ios_base::seekdir way,
                        std::ios_base::openmode which = std::ios_base::in | std::ios_base::out) override {
            const pos_type fail_pos = off_type(-1);
            off_type target_in = -1;
            off_type target_out = -1;

            if (which & std::ios_base::in) {
                if (!gptr()) return fail_pos;
                off_type base_pos = 0;
                if (way == std::ios_base::beg) {
                    base_pos = 0;
                } else if (way == std::ios_base::cur) {
                    if ((which & std::ios_base::out) && (mode_ & std::ios_base::out)) {
                        return fail_pos; // cur undefined if seeking both in and out simultaneously
                    }
                    base_pos = gptr() - eback();
                } else if (way == std::ios_base::end) {
                    base_pos = egptr() - eback();
                } else {
                    return fail_pos;
                }
                target_in = base_pos + off;
                if (target_in < 0 || target_in > (egptr() - eback())) {
                    return fail_pos;
                }
            }

            if (which & std::ios_base::out) {
                if (!pptr()) return fail_pos;
                off_type base_pos = 0;
                if (way == std::ios_base::beg) {
                    base_pos = 0;
                } else if (way == std::ios_base::cur) {
                    if ((which & std::ios_base::in) && (mode_ & std::ios_base::in)) {
                        return fail_pos;
                    }
                    base_pos = pptr() - pbase();
                } else if (way == std::ios_base::end) {
                    base_pos = epptr() - pbase();
                } else {
                    return fail_pos;
                }
                target_out = base_pos + off;
                if (target_out < 0 || target_out > (epptr() - pbase())) {
                    return fail_pos;
                }
            }

            if (target_in != -1) {
                setg(eback(), eback() + target_in, egptr());
            }
            if (target_out != -1) {
                setp(pbase(), epptr());
                pbump(static_cast<int>(target_out));
            }

            return (target_in != -1) ? pos_type(target_in) : pos_type(target_out);
        }

        pos_type seekpos(pos_type sp,
                         std::ios_base::openmode which = std::ios_base::in | std::ios_base::out) override {
            return seekoff(off_type(sp), std::ios_base::beg, which);
        }

        int sync() override {
            return 0;
        }

    private:
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
    class span_istream : private detail::span_istreambuf_holder, public std::istream {
    public:
        explicit span_istream(std::span<const char> s) noexcept
            : detail::span_istreambuf_holder(s), std::istream(&buf_) {}

        explicit span_istream(std::span<const std::byte> s) noexcept
            : span_istream(std::span<const char>(reinterpret_cast<const char*>(s.data()), s.size())) {}

        explicit span_istream(std::span<const uint8_t> s) noexcept
            : span_istream(std::span<const char>(reinterpret_cast<const char*>(s.data()), s.size())) {}

        explicit span_istream(std::string_view sv) noexcept
            : span_istream(std::span<const char>(sv.data(), sv.size())) {}

        explicit span_istream(const std::string& str) noexcept
            : span_istream(std::string_view(str)) {}

        span_istream(const char* data, std::size_t size) noexcept
            : span_istream(std::span<const char>(data, size)) {}

        span_istream(const void* data, std::size_t size) noexcept
            : span_istream(std::span<const char>(static_cast<const char*>(data), size)) {}

        template<typename Alloc>
        explicit span_istream(const std::vector<char, Alloc>& vec) noexcept
            : span_istream(std::span<const char>(vec.data(), vec.size())) {}

        template<typename Alloc>
        explicit span_istream(const std::vector<uint8_t, Alloc>& vec) noexcept
            : span_istream(std::span<const uint8_t>(vec.data(), vec.size())) {}

        template<typename Alloc>
        explicit span_istream(const std::vector<std::byte, Alloc>& vec) noexcept
            : span_istream(std::span<const std::byte>(vec.data(), vec.size())) {}

        [[nodiscard]] span_streambuf* rdbuf() noexcept { return &buf_; }
        [[nodiscard]] const span_streambuf* rdbuf() const noexcept { return &buf_; }

        [[nodiscard]] std::span<const char> span() const noexcept { return buf_.span(); }
        void reset() noexcept { buf_.reset(); clear(); }
    };

    // ========================================================================
    // span_ostream (C++20 zero-copy ostream over span / buffer)
    // ========================================================================
    class span_ostream : private detail::span_ostreambuf_holder, public std::ostream {
    public:
        explicit span_ostream(std::span<char> s) noexcept
            : detail::span_ostreambuf_holder(s), std::ostream(&buf_) {}

        explicit span_ostream(std::span<std::byte> s) noexcept
            : span_ostream(std::span<char>(reinterpret_cast<char*>(s.data()), s.size())) {}

        explicit span_ostream(std::span<uint8_t> s) noexcept
            : span_ostream(std::span<char>(reinterpret_cast<char*>(s.data()), s.size())) {}

        span_ostream(char* data, std::size_t size) noexcept
            : span_ostream(std::span<char>(data, size)) {}

        span_ostream(void* data, std::size_t size) noexcept
            : span_ostream(std::span<char>(static_cast<char*>(data), size)) {}

        template<typename Alloc>
        explicit span_ostream(std::vector<char, Alloc>& vec) noexcept
            : span_ostream(std::span<char>(vec.data(), vec.size())) {}

        template<typename Alloc>
        explicit span_ostream(std::vector<uint8_t, Alloc>& vec) noexcept
            : span_ostream(std::span<uint8_t>(vec.data(), vec.size())) {}

        template<typename Alloc>
        explicit span_ostream(std::vector<std::byte, Alloc>& vec) noexcept
            : span_ostream(std::span<std::byte>(vec.data(), vec.size())) {}

        [[nodiscard]] span_streambuf* rdbuf() noexcept { return &buf_; }
        [[nodiscard]] const span_streambuf* rdbuf() const noexcept { return &buf_; }

        [[nodiscard]] std::streamsize chars_written() const noexcept {
            return buf_.chars_written();
        }

        [[nodiscard]] std::span<const char> written_span() const noexcept {
            return buf_.written_span();
        }

        [[nodiscard]] std::string_view written_string_view() const noexcept {
            return {buf_.written_span().data(), buf_.written_span().size()};
        }

        void reset() noexcept { buf_.reset(); clear(); }
    };

    // Drop-in aliases for bsw::io
    using memory_stream_buf = span_streambuf;
    using memory_input_stream = span_istream;
    using memory_output_stream = span_ostream;

} // namespace bstream
