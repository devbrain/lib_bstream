#pragma once

#include <bstream/types.hh>
#include <span>

namespace bstream {

    // ========================================================================
    // Free Functions
    // ========================================================================

    template<scalar_type T>
    [[nodiscard]] constexpr T flip_bytes(T value) noexcept {
        return byteswap(value);
    }

    template<scalar_type T>
    constexpr void flip_bytes_in_place(T& value) noexcept {
        value = byteswap(value);
    }

    template<scalar_type T>
    constexpr void swap_bytes(std::span<T> data) noexcept {
        if constexpr (sizeof(T) > 1) {
            for (auto& item : data) {
                item = byteswap(item);
            }
        }
    }

    template<scalar_type T>
    [[nodiscard]] constexpr T to_big_endian(T value) noexcept {
        return to_endian(value, endian::big);
    }

    template<scalar_type T>
    [[nodiscard]] constexpr T from_big_endian(T value) noexcept {
        return from_endian(value, endian::big);
    }

    template<scalar_type T>
    [[nodiscard]] constexpr T to_little_endian(T value) noexcept {
        return to_endian(value, endian::little);
    }

    template<scalar_type T>
    [[nodiscard]] constexpr T from_little_endian(T value) noexcept {
        return from_endian(value, endian::little);
    }

    template<scalar_type T>
    [[nodiscard]] constexpr T to_network(T value) noexcept {
        return to_big_endian(value);
    }

    template<scalar_type T>
    [[nodiscard]] constexpr T from_network(T value) noexcept {
        return from_big_endian(value);
    }

    // ========================================================================
    // Drop-in compatibility class for bsw::byte_order
    // ========================================================================

    struct byte_order {
        template<scalar_type T>
        [[nodiscard]] static constexpr T flip_bytes(T value) noexcept {
            return bstream::flip_bytes(value);
        }

        template<scalar_type T>
        static constexpr void flip_bytes_in_place(T& value) noexcept {
            bstream::flip_bytes_in_place(value);
        }

        template<scalar_type T>
        static constexpr void swap_bytes(std::span<T> data) noexcept {
            bstream::swap_bytes(data);
        }

        template<scalar_type T>
        [[nodiscard]] static constexpr T to_big_endian(T value) noexcept {
            return bstream::to_big_endian(value);
        }

        template<scalar_type T>
        [[nodiscard]] static constexpr T from_big_endian(T value) noexcept {
            return bstream::from_big_endian(value);
        }

        template<scalar_type T>
        [[nodiscard]] static constexpr T to_little_endian(T value) noexcept {
            return bstream::to_little_endian(value);
        }

        template<scalar_type T>
        [[nodiscard]] static constexpr T from_little_endian(T value) noexcept {
            return bstream::from_little_endian(value);
        }

        template<scalar_type T>
        [[nodiscard]] static constexpr T to_network(T value) noexcept {
            return bstream::to_network(value);
        }

        template<scalar_type T>
        [[nodiscard]] static constexpr T from_network(T value) noexcept {
            return bstream::from_network(value);
        }
    };

} // namespace bstream
