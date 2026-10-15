#pragma once

#include <bstream/error.hh>

#include <algorithm>
#include <array>
#include <cstddef>
#include <istream>
#include <ostream>
#include <string>
#include <vector>

namespace bstream {

    struct stream_copier {
        static constexpr std::size_t default_chunk_size = 8192;

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

        static std::streamsize copy_unbuffered(std::istream& in, std::ostream& out) {
            char c = 0;
            std::streamsize total = 0;
            while (in.get(c) && out.put(c)) {
                ++total;
            }
            return total;
        }

        // BSW drop-in compatibility
        static std::streamsize copy_stream(std::istream& in, std::ostream& out, std::size_t bufferSize = default_chunk_size) {
            return copy(in, out, bufferSize);
        }

        static std::streamsize copy_stream_unbuffered(std::istream& in, std::ostream& out) {
            return copy_unbuffered(in, out);
        }
    };

} // namespace bstream
