#pragma once

#include <bstream/error.hh>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstring>
#include <ios>
#include <istream>
#include <ostream>
#include <streambuf>
#include <vector>

namespace bstream {

    // ========================================================================
    // tee_stream_buf
    // ========================================================================
    class tee_stream_buf : public std::basic_streambuf<char> {
    public:
        using char_type = char;
        using traits_type = std::char_traits<char>;
        using int_type = traits_type::int_type;

        tee_stream_buf() noexcept : istr_(nullptr) {
            init_in_buffer();
        }

        explicit tee_stream_buf(std::istream& is) : istr_(&is) {
            init_in_buffer();
        }

        explicit tee_stream_buf(std::ostream& os) : istr_(nullptr) {
            streams_.push_back(&os);
            init_in_buffer();
        }

        ~tee_stream_buf() override = default;

        void add_stream(std::ostream& os) {
            streams_.push_back(&os);
        }

        [[nodiscard]] const std::vector<std::ostream*>& streams() const noexcept {
            return streams_;
        }

    protected:
        // Output operations (writing to multiple streams)
        int_type overflow(int_type c) override {
            if (c != traits_type::eof()) {
                char ch = traits_type::to_char_type(c);
                for (auto* os : streams_) {
                    if (os) os->put(ch);
                }
            }
            return c;
        }

        std::streamsize xsputn(const char_type* s, std::streamsize count) override {
            if (count <= 0) return 0;
            for (auto* os : streams_) {
                if (os) os->write(s, count);
            }
            return count;
        }

        int sync() override {
            for (auto* os : streams_) {
                if (os) os->flush();
            }
            return 0;
        }

        // Input operations (reading from istr_ and teeing copy to streams_)
        int_type underflow() override {
            if (!istr_) return traits_type::eof();

            if (gptr() && gptr() < egptr()) {
                return traits_type::to_int_type(*gptr());
            }

            istr_->read(in_buffer_.data(), static_cast<std::streamsize>(in_buffer_.size()));
            std::streamsize n = istr_->gcount();
            if (n <= 0) {
                return traits_type::eof();
            }

            // Tee the chunk read to all registered output streams
            for (auto* os : streams_) {
                if (os) os->write(in_buffer_.data(), n);
            }

            setg(in_buffer_.data(), in_buffer_.data(), in_buffer_.data() + n);
            return traits_type::to_int_type(*gptr());
        }

    private:
        void init_in_buffer() {
            setg(in_buffer_.data(), in_buffer_.data(), in_buffer_.data());
        }

        static constexpr std::size_t buffer_size = 4096;
        std::array<char, buffer_size> in_buffer_{};
        std::istream* istr_{nullptr};
        std::vector<std::ostream*> streams_{};
    };

    namespace detail {
        struct tee_istreambuf_holder {
            tee_stream_buf buf_;
            explicit tee_istreambuf_holder(std::istream& is) : buf_(is) {}
        };

        struct tee_ostreambuf_holder {
            tee_stream_buf buf_;
            tee_ostreambuf_holder() = default;
            explicit tee_ostreambuf_holder(std::ostream& os) : buf_(os) {}
        };
    } // namespace detail

    // ========================================================================
    // tee_input_stream
    // ========================================================================
    class tee_input_stream : private detail::tee_istreambuf_holder, public std::istream {
    public:
        explicit tee_input_stream(std::istream& is)
            : detail::tee_istreambuf_holder(is), std::istream(&buf_) {}

        void add_stream(std::ostream& os) {
            buf_.add_stream(os);
        }

        [[nodiscard]] tee_stream_buf* rdbuf() noexcept { return &buf_; }
    };

    // ========================================================================
    // tee_output_stream
    // ========================================================================
    class tee_output_stream : private detail::tee_ostreambuf_holder, public std::ostream {
    public:
        tee_output_stream()
            : detail::tee_ostreambuf_holder(), std::ostream(&buf_) {}

        explicit tee_output_stream(std::ostream& os)
            : detail::tee_ostreambuf_holder(os), std::ostream(&buf_) {}

        void add_stream(std::ostream& os) {
            buf_.add_stream(os);
        }

        [[nodiscard]] tee_stream_buf* rdbuf() noexcept { return &buf_; }
    };

} // namespace bstream
