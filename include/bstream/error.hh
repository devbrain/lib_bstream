#pragma once

#include <stdexcept>
#include <string>
#include <string_view>

#if __has_include(<failsafe/expected.hh>)
#include <failsafe/expected.hh>
#define BSTREAM_HAS_FAILSAFE 1
#endif

namespace bstream {

    class bstream_error : public std::runtime_error {
    public:
        using std::runtime_error::runtime_error;
    };

    class eof_error : public bstream_error {
    public:
        eof_error() : bstream_error("Unexpected end of binary stream") {}
        explicit eof_error(std::string_view msg) : bstream_error(std::string(msg)) {}
    };

    class parse_error : public bstream_error {
    public:
        explicit parse_error(std::string_view msg) : bstream_error(std::string(msg)) {}
    };

#if defined(BSTREAM_HAS_FAILSAFE)
    template<typename T>
    using expected = failsafe::expected<T, std::string>;
    using unexpected = failsafe::unexpected<std::string>;
#endif

} // namespace bstream
