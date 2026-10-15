/**
 * @file byte_order.hh
 * @brief Shared scalar and batch byte-order conversion API.
 * @ingroup byte_order_api
 */

#pragma once

#include <bstream/detail/platform.hh>

#include <bstream/types.hh>
#include <span>

namespace bstream {

    // ========================================================================
    // Free Functions
    // ========================================================================

    /**
     * @brief Return a scalar with its bytes reversed.
     * @param value Scalar to reverse.
     * @return The reversed value; single-byte values are unchanged.
     * @see byteswap
     * @ingroup byte_order_api
     * @tparam T Arithmetic or enum element type.
     */
    template<scalar_type T>
    [[nodiscard]] constexpr T flip_bytes(T value) noexcept {
        return byteswap(value);
    }

    /**
     * @brief Reverse one scalar's bytes in place.
     * @param value Scalar updated with its byte-reversed value.
     * @ingroup byte_order_api
     * @tparam T Arithmetic or enum element type.
     */
    template<scalar_type T>
    constexpr void flip_bytes_in_place(T& value) noexcept {
        value = byteswap(value);
    }

    /**
     * @brief Reverse bytes within each scalar of a span, preserving element order.
     * @param data Mutable scalar sequence; an empty span or single-byte elements are no-ops.
     * @ingroup byte_order_api
     * @tparam T Arithmetic or enum element type.
     */
    template<scalar_type T>
    constexpr void swap_bytes(std::span<T> data) noexcept {
        if constexpr (sizeof(T) > 1) {
            for (auto& item : data) {
                item = byteswap(item);
            }
        }
    }

    /**
     * @brief Convert a sequence from the specified order to native order.
     * @param data Mutable scalar sequence to convert in place.
     * @param order Source byte order; native order leaves the span unchanged.
     * @ingroup byte_order_api
     * @tparam T Arithmetic or enum element type.
     */
    template<scalar_type T>
    constexpr void from_endian_in_place(std::span<T> data, endian order) noexcept {
        if (order != endian::native) {
            swap_bytes(data);
        }
    }

    /**
     * @brief Convert a scalar from native order to big-endian order.
     * @param value Scalar to convert.
     * @return The value represented in the destination byte order.
     * @ingroup byte_order_api
     * @tparam T Arithmetic or enum element type.
     */
    template<scalar_type T>
    [[nodiscard]] constexpr T to_big_endian(T value) noexcept {
        return to_endian(value, endian::big);
    }

    /**
     * @brief Convert a scalar to native order from big-endian order.
     * @param value Scalar to convert.
     * @return The value represented in the destination byte order.
     * @ingroup byte_order_api
     * @tparam T Arithmetic or enum element type.
     */
    template<scalar_type T>
    [[nodiscard]] constexpr T from_big_endian(T value) noexcept {
        return from_endian(value, endian::big);
    }

    /**
     * @brief Convert a scalar from native order to little-endian order.
     * @param value Scalar to convert.
     * @return The value represented in the destination byte order.
     * @ingroup byte_order_api
     * @tparam T Arithmetic or enum element type.
     */
    template<scalar_type T>
    [[nodiscard]] constexpr T to_little_endian(T value) noexcept {
        return to_endian(value, endian::little);
    }

    /**
     * @brief Convert a scalar to native order from little-endian order.
     * @param value Scalar to convert.
     * @return The value represented in the destination byte order.
     * @ingroup byte_order_api
     * @tparam T Arithmetic or enum element type.
     */
    template<scalar_type T>
    [[nodiscard]] constexpr T from_little_endian(T value) noexcept {
        return from_endian(value, endian::little);
    }

    /**
     * @brief Convert a scalar from native order to big-endian (network) order.
     * @param value Scalar to convert.
     * @return The value represented in the destination byte order.
     * @ingroup byte_order_api
     * @tparam T Arithmetic or enum element type.
     */
    template<scalar_type T>
    [[nodiscard]] constexpr T to_network(T value) noexcept {
        return to_big_endian(value);
    }

    /**
     * @brief Convert a scalar to native order from big-endian (network) order.
     * @param value Scalar to convert.
     * @return The value represented in the destination byte order.
     * @ingroup byte_order_api
     * @tparam T Arithmetic or enum element type.
     */
    template<scalar_type T>
    [[nodiscard]] constexpr T from_network(T value) noexcept {
        return from_big_endian(value);
    }

    // ========================================================================
    // Drop-in compatibility class for bsw::byte_order
    // ========================================================================

    /**
     * @brief Static facade for the shared byte-order functions, compatible with bsw::byte_order.
     * @ingroup byte_order_api
     * @details Byte readers call this facade for scalar and container conversion.
     * @snippet usage.cc byte_order
     */
    struct byte_order {
        /**
         * @copydoc bstream::from_endian
         */
        template<scalar_type T>
        [[nodiscard]] static constexpr T from_endian(T value, endian order) noexcept {
            return bstream::from_endian(value, order);
        }

        /**
         * @copydoc bstream::from_endian_in_place
         */
        template<scalar_type T>
        static constexpr void from_endian_in_place(std::span<T> data, endian order) noexcept {
            bstream::from_endian_in_place(data, order);
        }

        /**
         * @copydoc bstream::flip_bytes
         */
        template<scalar_type T>
        [[nodiscard]] static constexpr T flip_bytes(T value) noexcept {
            return bstream::flip_bytes(value);
        }

        /**
         * @copydoc bstream::flip_bytes_in_place
         */
        template<scalar_type T>
        static constexpr void flip_bytes_in_place(T& value) noexcept {
            bstream::flip_bytes_in_place(value);
        }

        /**
         * @copydoc bstream::swap_bytes
         */
        template<scalar_type T>
        static constexpr void swap_bytes(std::span<T> data) noexcept {
            bstream::swap_bytes(data);
        }

        /**
         * @copydoc bstream::to_big_endian
         */
        template<scalar_type T>
        [[nodiscard]] static constexpr T to_big_endian(T value) noexcept {
            return bstream::to_big_endian(value);
        }

        /**
         * @copydoc bstream::from_big_endian
         */
        template<scalar_type T>
        [[nodiscard]] static constexpr T from_big_endian(T value) noexcept {
            return bstream::from_big_endian(value);
        }

        /**
         * @copydoc bstream::to_little_endian
         */
        template<scalar_type T>
        [[nodiscard]] static constexpr T to_little_endian(T value) noexcept {
            return bstream::to_little_endian(value);
        }

        /**
         * @copydoc bstream::from_little_endian
         */
        template<scalar_type T>
        [[nodiscard]] static constexpr T from_little_endian(T value) noexcept {
            return bstream::from_little_endian(value);
        }

        /**
         * @copydoc bstream::to_network
         */
        template<scalar_type T>
        [[nodiscard]] static constexpr T to_network(T value) noexcept {
            return bstream::to_network(value);
        }

        /**
         * @copydoc bstream::from_network
         */
        template<scalar_type T>
        [[nodiscard]] static constexpr T from_network(T value) noexcept {
            return bstream::from_network(value);
        }
    };

} // namespace bstream
