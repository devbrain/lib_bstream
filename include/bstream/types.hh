/**
 * @file types.hh
 * @brief Scalar concepts and portable endian conversion primitives.
 * @ingroup byte_order_api
 */

#pragma once

#include <bstream/detail/platform.hh>

#include <bit>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <type_traits>
#include <utility>

namespace bstream {

    /**
     * @brief Native, little, and big byte-order identifiers from C++20.
     * @ingroup byte_order_api
     */
    using endian = std::endian;

    /**
     * @brief Integral, enum, and IEEE binary32/binary64 types accepted by scalar serialization.
     * @ingroup byte_order_api
     * @details T is the type to classify.
     * @note Other floating-point representations, including padded long double,
     * are excluded because byte-reversed values cannot safely be passed by value.
     * The concept does not guarantee a portable wire representation; see @ref format.
     */
    template<typename T>
    concept scalar_type = std::is_integral_v<T> || std::is_enum_v<T> ||
        (std::is_floating_point_v<T> && std::numeric_limits<T>::is_iec559 &&
         std::numeric_limits<T>::radix == 2 &&
         ((sizeof(T) == sizeof(uint32_t) && std::numeric_limits<T>::digits == 24 &&
           std::numeric_limits<T>::max_exponent == 128) ||
          (sizeof(T) == sizeof(uint64_t) && std::numeric_limits<T>::digits == 53 &&
           std::numeric_limits<T>::max_exponent == 1024)));

    /**
     * @brief True for types whose object representation is trivially copyable.
     * @ingroup byte_order_api
     * @details T is the type to classify.
     * @note This alone does not enable raw-object or aggregate serialization.
     */
    template<typename T>
    concept trivially_copyable = std::is_trivially_copyable_v<T>;

    /**
     * @brief Reverse the byte order of a scalar value.
     * @ingroup byte_order_api
     * @tparam T Supported scalar type; see scalar_type.
     * @param value Scalar whose object bytes are reversed.
     * @return The reversed value; single-byte values are unchanged.
     * @note Integer widths 16/32/64 and matching float/double representations support
     * constant evaluation. Other integral sizes use a runtime representation reversal.
     * This does not normalize floating-point formats or padding across platforms.
     * @see flip_bytes, format
     */
    template<scalar_type T>
    [[nodiscard]] constexpr T byteswap(T value) noexcept {
        if constexpr (sizeof(T) == 1) {
            return value;
        } else if constexpr (std::is_enum_v<T>) {
            using U = std::underlying_type_t<T>;
            return static_cast<T>(byteswap(static_cast<U>(value)));
        } else if constexpr (std::is_floating_point_v<T>) {
            if constexpr (sizeof(T) == sizeof(uint32_t)) {
                return std::bit_cast<T>(byteswap(std::bit_cast<uint32_t>(value)));
            } else {
                return std::bit_cast<T>(byteswap(std::bit_cast<uint64_t>(value)));
            }
        } else if constexpr (sizeof(T) == 2) {
            const auto u = static_cast<uint16_t>(value);
            return static_cast<T>((u >> 8) | (u << 8));
        } else if constexpr (sizeof(T) == 4) {
            const auto u = static_cast<uint32_t>(value);
            return static_cast<T>((u >> 24) | ((u >> 8) & 0x0000ff00U) |
                                  ((u << 8) & 0x00ff0000U) | (u << 24));
        } else if constexpr (sizeof(T) == 8) {
            const auto u = static_cast<uint64_t>(value);
            const auto low = byteswap(static_cast<uint32_t>(u));
            const auto high = byteswap(static_cast<uint32_t>(u >> 32));
            return static_cast<T>((static_cast<uint64_t>(low) << 32) | high);
        } else {
            auto* bytes = reinterpret_cast<uint8_t*>(&value);
            for (std::size_t i = 0; i < sizeof(T) / 2; ++i) {
                std::swap(bytes[i], bytes[sizeof(T) - 1 - i]);
            }
            return value;
        }
    }

    /**
     * @brief Convert a scalar between two byte orders.
     * @ingroup byte_order_api
     * @tparam T Arithmetic or enum type.
     * @param value Value represented in from order.
     * @param from Source byte order.
     * @param to Destination byte order.
     * @return value unchanged when orders match; otherwise byteswap(value).
     * @pre Native representations are uniformly little or big endian.
     */
    template<scalar_type T>
    [[nodiscard]] constexpr T convert_endian(T value, endian from, endian to) noexcept {
        if (from == to) {
            return value;
        }
        return byteswap(value);
    }

    /**
     * @brief Convert a scalar from the specified byte order to native order.
     * @ingroup byte_order_api
     * @tparam T Arithmetic or enum type.
     * @param value Value in the specified order.
     * @param order Source byte order.
     * @return The value in native byte order.
     */
    template<scalar_type T>
    [[nodiscard]] constexpr T from_endian(T value, endian order) noexcept {
        return convert_endian(value, order, endian::native);
    }

    /**
     * @brief Convert a native scalar to the specified byte order.
     * @ingroup byte_order_api
     * @tparam T Arithmetic or enum type.
     * @param value Value in native order.
     * @param order Destination byte order.
     * @return The value represented in the requested byte order.
     */
    template<scalar_type T>
    [[nodiscard]] constexpr T to_endian(T value, endian order) noexcept {
        return convert_endian(value, endian::native, order);
    }

} // namespace bstream
