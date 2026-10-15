/**
 * @file stream_copier.hh
 * @brief Buffered and unbuffered stream transfer helpers.
 * @ingroup adapters
 */

#pragma once

#include <bstream/detail/platform.hh>

#include <bstream/error.hh>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <istream>
#include <ostream>
#include <string>
#include <vector>

namespace bstream {

    /**
     * @brief Stateless helpers that transfer bytes from the current input position.
     * @ingroup adapters
     *
     * Buffered operations use an 8192-byte stack buffer, or allocate a larger
     * scratch buffer when requested. A zero chunk copies zero bytes. Destinations
     * are appended to and are not flushed. Ordinary input errors stop copying;
     * inspect input state to distinguish errors from EOF. Stream exception masks
     * and allocation exceptions still apply. See @ref stream_adapters.
     * @snippet usage.cc tee_input
     */
    struct stream_copier {
        /**
         * @brief Default stack scratch-buffer capacity in bytes.
         */
        static constexpr std::size_t default_chunk_size = 8192;

        /**
         * @brief Copy buffered chunks until input ends or fails.
         * @throws bstream_error If an attempted output write fails.
         * @note Does not flush output; stream exception masks and allocation exceptions still apply.
         * @param in Input stream, starting at its current position.
         * @param out Destination to append bytes to; existing contents are preserved.
         * @param chunk_size Scratch-buffer size in bytes; zero copies nothing.
         * @return The number of bytes successfully transferred.
         */
        static std::streamsize copy(
            std::istream& in,
            std::ostream& out,
            std::size_t chunk_size = default_chunk_size) {

            std::array<char, default_chunk_size> stack_buf;
            std::vector<char> heap_buf;
            char* buf = stack_buf.data();
            std::size_t actual_size = default_chunk_size;

            if (chunk_size != default_chunk_size) {
                if (chunk_size <= default_chunk_size) {
                    actual_size = chunk_size;
                } else {
                    heap_buf.resize(chunk_size);
                    buf = heap_buf.data();
                    actual_size = chunk_size;
                }
            }

            std::streamsize total = 0;
            while (in && out) {
                in.read(buf, static_cast<std::streamsize>(actual_size));
                std::streamsize n = in.gcount();
                if (n <= 0) break;
                out.write(buf, n);
                if (!out) {
                    throw bstream_error("stream_copier: write failed");
                }
                total += n;
            }
            return total;
        }

        /**
         * @brief Copy buffered chunks until input ends or fails. Append to the string without clearing it.
         * @note Does not flush output; stream exception masks and allocation exceptions still apply.
         * @param in Input stream, starting at its current position.
         * @param out Destination to append bytes to; existing contents are preserved.
         * @param chunk_size Scratch-buffer size in bytes; zero copies nothing.
         * @return The number of bytes successfully transferred.
         */
        static std::streamsize copy_to_string(
            std::istream& in,
            std::string& out,
            std::size_t chunk_size = default_chunk_size) {

            std::array<char, default_chunk_size> stack_buf;
            std::vector<char> heap_buf;
            char* buf = stack_buf.data();
            std::size_t actual_size = default_chunk_size;

            if (chunk_size != default_chunk_size) {
                if (chunk_size <= default_chunk_size) {
                    actual_size = chunk_size;
                } else {
                    heap_buf.resize(chunk_size);
                    buf = heap_buf.data();
                    actual_size = chunk_size;
                }
            }

            std::streamsize total = 0;
            while (in) {
                in.read(buf, static_cast<std::streamsize>(actual_size));
                std::streamsize n = in.gcount();
                if (n <= 0) break;
                out.append(buf, static_cast<std::size_t>(n));
                total += n;
            }
            return total;
        }

        /**
         * @brief Copy buffered chunks until input ends or fails. Append to the byte vector without clearing it.
         * @note Does not flush output; stream exception masks and allocation exceptions still apply.
         * @tparam ByteT One-byte destination element type with a valid byte representation.
         * @param in Input stream, starting at its current position.
         * @param out Destination to append bytes to; existing contents are preserved.
         * @param chunk_size Scratch-buffer size in bytes; zero copies nothing.
         * @return The number of bytes successfully transferred.
         */
        template<typename ByteT = uint8_t>
            requires (sizeof(ByteT) == 1)
        static std::streamsize copy_to_vector(
            std::istream& in,
            std::vector<ByteT>& out,
            std::size_t chunk_size = default_chunk_size) {

            std::array<char, default_chunk_size> stack_buf;
            std::vector<char> heap_buf;
            char* buf = stack_buf.data();
            std::size_t actual_size = default_chunk_size;

            if (chunk_size != default_chunk_size) {
                if (chunk_size <= default_chunk_size) {
                    actual_size = chunk_size;
                } else {
                    heap_buf.resize(chunk_size);
                    buf = heap_buf.data();
                    actual_size = chunk_size;
                }
            }

            std::streamsize total = 0;
            while (in) {
                in.read(buf, static_cast<std::streamsize>(actual_size));
                std::streamsize n = in.gcount();
                if (n <= 0) break;
                const auto* byte_data = reinterpret_cast<const ByteT*>(buf);
                out.insert(out.end(), byte_data, byte_data + n);
                total += n;
            }
            return total;
        }

        /**
         * @brief Copy one character at a time until input or output fails.
         * @note Does not flush output; stream exception masks and allocation exceptions still apply.
         * @param in Input stream, starting at its current position.
         * @param out Destination to append bytes to; existing contents are preserved.
         * @return The number of bytes successfully transferred.
         */
        static std::streamsize copy_unbuffered(std::istream& in, std::ostream& out) {
            char c = 0;
            std::streamsize total = 0;
            while (in.get(c) && out.put(c)) {
                ++total;
            }
            return total;
        }

        // BSW drop-in compatibility
        /**
         * @brief Copy buffered chunks until input ends or fails.
         * @throws bstream_error If an attempted output write fails.
         * @see copy
         * @note Does not flush output; stream exception masks and allocation exceptions still apply.
         * @param in Input stream, starting at its current position.
         * @param out Destination to append bytes to; existing contents are preserved.
         * @param bufferSize Scratch-buffer size in bytes; zero copies nothing.
         * @return The number of bytes successfully transferred.
         */
        static std::streamsize copy_stream(std::istream& in, std::ostream& out, std::size_t bufferSize = default_chunk_size) {
            return copy(in, out, bufferSize);
        }

        /**
         * @brief Copy one character at a time until input or output fails.
         * @see copy_unbuffered
         * @note Does not flush output; stream exception masks and allocation exceptions still apply.
         * @param in Input stream, starting at its current position.
         * @param out Destination to append bytes to; existing contents are preserved.
         * @return The number of bytes successfully transferred.
         */
        static std::streamsize copy_stream_unbuffered(std::istream& in, std::ostream& out) {
            return copy_unbuffered(in, out);
        }
    };

} // namespace bstream
