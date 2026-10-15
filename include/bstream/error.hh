/**
 * @file error.hh
 * @brief Exception types and optional failsafe result aliases.
 * @ingroup errors_api
 */

#pragma once

#include <bstream/detail/platform.hh>

#include <stdexcept>
#include <string>
#include <string_view>

#if __has_include(<failsafe/expected.hh>)
#include <failsafe/expected.hh>
/** @brief Defined when failsafe/expected.hh is available to this translation unit. */
#define BSTREAM_HAS_FAILSAFE 1
#endif

namespace bstream {

    /**
     * @brief Base exception for binary input/output and bit-operation errors.
     * @ingroup errors_api
     * @details Derives from std::runtime_error and inherits its message constructors.
     * @see error_guide
     */
    class bstream_error : public std::runtime_error {
    public:
        /**
         * @brief Inherit constructors that store a diagnostic message.
         */
        using std::runtime_error::runtime_error;
    };

    /**
     * @brief Insufficient input or exhaustion of a bounded output buffer.
     * @ingroup errors_api
     * @snippet usage.cc errors
     */
    class eof_error : public bstream_error {
    public:
        /**
         * @brief Construct with the default unexpected-end-of-stream message.
         */
        eof_error() : bstream_error("Unexpected end of binary stream") {}
        /**
         * @brief Construct with a copied diagnostic message.
         * @param msg Error description.
         */
        explicit eof_error(std::string_view msg) : bstream_error(std::string(msg)) {}
    };

    /**
     * @brief Invalid or unrepresentable format value, such as a Pascal string length.
     * @ingroup errors_api
     * @see error_guide
     */
    class parse_error : public bstream_error {
    public:
        /**
         * @brief Construct with a copied diagnostic message.
         * @param msg Error description.
         */
        explicit parse_error(std::string_view msg) : bstream_error(std::string(msg)) {}
    };

#if defined(BSTREAM_HAS_FAILSAFE)
    /**
     * @brief Optional failsafe result holding T or an owned error string.
     * @ingroup errors_api
     * @tparam T Successful result type.
     * @note Available only when BSTREAM_HAS_FAILSAFE is defined.
     * @see expected_results
     */
    template<typename T>
    using expected = failsafe::expected<T, std::string>;
    /**
     * @brief Optional failsafe unexpected result containing an owned error string.
     * @ingroup errors_api
     * @note Available only when BSTREAM_HAS_FAILSAFE is defined.
     */
    using unexpected = failsafe::unexpected<std::string>;
#endif

} // namespace bstream
