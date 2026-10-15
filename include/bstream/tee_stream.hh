/**
 * @file tee_stream.hh
 * @brief Input mirroring and output broadcasting adapters.
 * @ingroup adapters
 */

#pragma once

#include <bstream/detail/platform.hh>

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
    /**
     * @brief Stream buffer that broadcasts output or mirrors prefetched input.
     * @ingroup adapters
     *
     * All registered streams are borrowed and must outlive this buffer. Output
     * failures are reported through stream-buffer return values. Input mirroring
     * prefetches up to 4096 bytes, including on a peek. Earlier destination writes
     * cannot be rolled back after another fails. Exceptions may stop later delivery.
     * Copying and moving are disabled because the input cursors refer to an internal buffer.
     * @see tee_input_stream, tee_output_stream
     * @snippet usage.cc tee_output
     */
    class tee_stream_buf : public std::basic_streambuf<char> {
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
         * @brief Create a buffer without an input source or output destinations.
         */
        tee_stream_buf() noexcept : istr_(nullptr) {
            init_in_buffer();
        }

        /**
         * @brief Borrow the supplied stream as the input source or first output destination.
         * @pre Borrowed storage and streams outlive this object and all derived views.
         * @param is Borrowed input stream; must outlive this wrapper.
         */
        explicit tee_stream_buf(std::istream& is) : istr_(&is) {
            init_in_buffer();
        }

        /**
         * @brief Borrow the supplied stream as the input source or first output destination.
         * @pre Borrowed storage and streams outlive this object and all derived views.
         * @param os Borrowed output stream; must outlive this wrapper.
         */
        explicit tee_stream_buf(std::ostream& os) : istr_(nullptr) {
            streams_.push_back(&os);
            init_in_buffer();
        }

        /**
         * @brief Destroy the wrapper without releasing borrowed storage.
         */
        ~tee_stream_buf() override = default;

        /**
         * @brief Copy construction is disabled to keep input cursors tied to their buffer.
         */
        tee_stream_buf(const tee_stream_buf&) = delete;
        /**
         * @brief Copy assignment is disabled; the buffer is also not movable.
         */
        tee_stream_buf& operator=(const tee_stream_buf&) = delete;

        /**
         * @brief Register an additional borrowed output destination.
         * @note Registration does not replay earlier bytes; register before reading/writing. The destination must outlive this wrapper.
         * @param os Borrowed output stream; must outlive this wrapper.
         */
        void add_stream(std::ostream& os) {
            streams_.push_back(&os);
        }

        /**
         * @brief Inspect registered destination pointers in delivery order.
         * @return The borrowed destination list; additions can invalidate references to its elements.
         */
        [[nodiscard]] const std::vector<std::ostream*>& streams() const noexcept {
            return streams_;
        }

    protected:
        // Output operations (writing to multiple streams)
        /**
         * @brief Write one character to the output area or each tee destination.
         * @param c Character to write/put back, or traits_type::eof().
         * @return The accepted character/non-EOF status on success, or traits_type::eof() on failure.
         */
        int_type overflow(int_type c) override {
            bool failed = false;
            if (c != traits_type::eof()) {
                char ch = traits_type::to_char_type(c);
                for (auto* os : streams_) {
                    if (os) {
                        os->put(ch);
                        failed |= !*os;
                    }
                }
            }
            return failed ? traits_type::eof() : traits_type::not_eof(c);
        }

        /**
         * @brief Transfer up to count raw characters. A tee broadcasts the whole request to each destination.
         * @param s Character buffer covering count bytes; writable for reads, readable for writes.
         * @param count Requested character count; nonpositive counts transfer nothing.
         * @return The number transferred; tee output returns zero if any destination fails.
         */
        std::streamsize xsputn(const char_type* s, std::streamsize count) override {
            if (count <= 0) return 0;
            bool failed = false;
            for (auto* os : streams_) {
                if (os) {
                    os->write(s, count);
                    failed |= !*os;
                }
            }
            return failed ? 0 : count;
        }

        /**
         * @brief Synchronize output destinations.
         * @return Zero on success, or -1 if a tee destination fails to flush.
         */
        int sync() override {
            bool failed = false;
            for (auto* os : streams_) {
                if (os) {
                    os->flush();
                    failed |= !*os;
                }
            }
            return failed ? -1 : 0;
        }

        // Input operations (reading from istr_ and teeing copy to streams_)
        /**
         * @brief Expose the next input character. Reads and mirrors a fresh chunk if necessary.
         * @throws bstream_error If an input chunk cannot be mirrored.
         * @return The next character, or traits_type::eof() at end of input.
         */
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
            if (xsputn(in_buffer_.data(), n) != n) {
                throw bstream_error("tee_stream_buf: mirror write failed");
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
    /**
     * @brief Standard input stream that mirrors fetched chunks to output streams.
     * @ingroup adapters
     *
     * The source and destinations are borrowed. Reads and peeks may prefetch and
     * mirror up to 4096 bytes before the caller consumes them. A mirror failure
     * sets the tee's failure state or throws according to its exception mask.
     * @snippet usage.cc tee_input
     */
    class tee_input_stream : private detail::tee_istreambuf_holder, public std::istream {
    public:
        /**
         * @brief Borrow the supplied stream as the input source or first output destination.
         * @pre Borrowed storage and streams outlive this object and all derived views.
         * @param is Borrowed input stream; must outlive this wrapper.
         */
        explicit tee_input_stream(std::istream& is)
            : detail::tee_istreambuf_holder(is), std::istream(&buf_) {}

        /**
         * @brief Register an additional borrowed output destination.
         * @note Registration does not replay earlier bytes; register before reading/writing. The destination must outlive this wrapper.
         * @param os Borrowed output stream; must outlive this wrapper.
         */
        void add_stream(std::ostream& os) {
            buf_.add_stream(os);
        }

        /**
         * @brief Access this stream's embedded buffer; does not transfer ownership.
         * @return A pointer to the embedded stream buffer.
         */
        [[nodiscard]] tee_stream_buf* rdbuf() noexcept { return &buf_; }
    };

    // ========================================================================
    // tee_output_stream
    // ========================================================================
    /**
     * @brief Standard output stream that broadcasts bytes to borrowed destinations.
     * @ingroup adapters
     *
     * A failed destination sets failure state on the tee; accepted output is not
     * rolled back. Standard stream exception masks still apply. With no registered
     * destinations, writes succeed and discard the bytes. Flush explicitly to check
     * all destinations. Neither destruction nor registration transfers ownership.
     * @snippet usage.cc tee_output
     */
    class tee_output_stream : private detail::tee_ostreambuf_holder, public std::ostream {
    public:
        /**
         * @brief Create a tee with no destinations; writes are discarded successfully.
         */
        tee_output_stream()
            : detail::tee_ostreambuf_holder(), std::ostream(&buf_) {}

        /**
         * @brief Borrow the supplied stream as the input source or first output destination.
         * @pre Borrowed storage and streams outlive this object and all derived views.
         * @param os Borrowed output stream; must outlive this wrapper.
         */
        explicit tee_output_stream(std::ostream& os)
            : detail::tee_ostreambuf_holder(os), std::ostream(&buf_) {}

        /**
         * @brief Register an additional borrowed output destination.
         * @note Registration does not replay earlier bytes; register before reading/writing. The destination must outlive this wrapper.
         * @param os Borrowed output stream; must outlive this wrapper.
         */
        void add_stream(std::ostream& os) {
            buf_.add_stream(os);
        }

        /**
         * @brief Access this stream's embedded buffer; does not transfer ownership.
         * @return A pointer to the embedded stream buffer.
         */
        [[nodiscard]] tee_stream_buf* rdbuf() noexcept { return &buf_; }
    };

} // namespace bstream
