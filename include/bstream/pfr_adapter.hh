#pragma once

#include <bstream/types.hh>
#include <utility>

#if __has_include(<boost/pfr/core.hpp>)
#include <boost/pfr/core.hpp>
#define BSTREAM_HAS_PFR 1
#endif

namespace bstream::detail {

#if defined(BSTREAM_HAS_PFR)
    template<typename T, typename = void>
    struct is_pfr_reflectable : std::false_type {};

    template<typename T>
    struct is_pfr_reflectable<T, std::enable_if_t<
        std::is_aggregate_v<T> && 
        !std::is_array_v<T> && 
        !scalar_type<T>
    >> : std::bool_constant<(boost::pfr::tuple_size_v<T> > 0)> {};

    template<typename T>
    inline constexpr bool is_pfr_reflectable_v = is_pfr_reflectable<T>::value;

    template<typename Reader, typename T, std::size_t... I>
    constexpr T read_aggregate_impl(Reader& reader, std::index_sequence<I...>) {
        return T{reader.template read<boost::pfr::tuple_element_t<I, T>>()...};
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
#endif

} // namespace bstream::detail
