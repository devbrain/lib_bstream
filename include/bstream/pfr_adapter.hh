/**
 * @file pfr_adapter.hh
 * @brief Optional aggregate reflection detection and internal serialization support.
 * @details BSTREAM_HAS_PFR is defined when Boost.PFR is available and not disabled.
 * See @ref serialization for supported public usage; detail helpers are not API.
 */

#pragma once

#include <bstream/detail/platform.hh>

#include <bstream/types.hh>
#include <array>
#include <type_traits>
#include <utility>

#if (!defined(BSTREAM_WITH_BOOST_PFR) || BSTREAM_WITH_BOOST_PFR) && __has_include(<boost/pfr/core.hpp>)
#include <boost/pfr/core.hpp>
/** @brief Defined when automatic aggregate reflection is enabled and available. */
#define BSTREAM_HAS_PFR 1
#endif

namespace bstream::detail {

#if defined(BSTREAM_HAS_PFR)
    template<typename T>
    inline constexpr bool is_std_array_v = false;

    template<typename T, std::size_t N>
    inline constexpr bool is_std_array_v<std::array<T, N>> = true;

    template<typename T, typename = void>
    struct is_pfr_reflectable : std::false_type {};

    template<typename T>
    struct is_pfr_reflectable<T, std::enable_if_t<
        std::is_aggregate_v<T> && 
        !std::is_array_v<T> &&
        // std::array uses the container operators; its implementation layout
        // need not match the element count that PFR reflects on every compiler.
        !is_std_array_v<std::remove_cv_t<T>> &&
        !scalar_type<T>
    >> : std::bool_constant<(boost::pfr::tuple_size_v<T> > 0)> {};

    template<typename T>
    inline constexpr bool is_pfr_reflectable_v = is_pfr_reflectable<T>::value;

    template<typename Reader, typename T, std::size_t... I>
    constexpr T read_aggregate_impl(Reader& reader, std::index_sequence<I...>) {
        // Decode into mutable values, then initialize even const-qualified fields.
        return T{reader.template read<std::remove_cv_t<boost::pfr::tuple_element_t<I, T>>>()...};
    }

    template<typename Reader, typename T>
    constexpr T read_aggregate(Reader& reader) {
        return read_aggregate_impl<Reader, T>(
            reader, std::make_index_sequence<boost::pfr::tuple_size_v<T>>{});
    }

    template<typename Writer, typename T, std::size_t... I>
    constexpr void write_aggregate_impl(Writer& writer, const T& val, std::index_sequence<I...>) {
        (writer.write(boost::pfr::get<I>(val)), ...);
    }

    template<typename Writer, typename T>
    constexpr void write_aggregate(Writer& writer, const T& val) {
        write_aggregate_impl(writer, val, std::make_index_sequence<boost::pfr::tuple_size_v<T>>{});
    }
#else
    template<typename T>
    inline constexpr bool is_pfr_reflectable_v = false;

    // Qualified lookup still occurs in discarded if-constexpr branches.
    template<typename Reader, typename T>
    constexpr T read_aggregate(Reader&) = delete;

    template<typename Writer, typename T>
    constexpr void write_aggregate(Writer&, const T&) = delete;
#endif

} // namespace bstream::detail
