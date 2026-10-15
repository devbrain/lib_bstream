#pragma once

#include <bit>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <type_traits>

namespace bstream {

    using endian = std::endian;

    template<typename T>
    concept scalar_type = std::is_arithmetic_v<T> || std::is_enum_v<T>;

    template<typename T>
    concept trivially_copyable = std::is_trivially_copyable_v<T>;

    template<typename T>
    [[nodiscard]] constexpr T byteswap(T value) noexcept {
        static_assert(scalar_type<T>, "byteswap requires arithmetic or enum types");
        if constexpr (sizeof(T) == 1) {
            return value;
        } else if constexpr (std::is_enum_v<T>) {
            using U = std::underlying_type_t<T>;
            return static_cast<T>(byteswap(static_cast<U>(value)));
        } else if constexpr (std::is_floating_point_v<T>) {
            if constexpr (sizeof(T) == sizeof(uint32_t)) {
                uint32_t u;
                std::memcpy(&u, &value, sizeof(T));
                u = byteswap(u);
                std::memcpy(&value, &u, sizeof(T));
                return value;
            } else if constexpr (sizeof(T) == sizeof(uint64_t)) {
                uint64_t u;
                std::memcpy(&u, &value, sizeof(T));
                u = byteswap(u);
                std::memcpy(&value, &u, sizeof(T));
                return value;
            } else {
                auto* bytes = reinterpret_cast<uint8_t*>(&value);
                for (std::size_t i = 0; i < sizeof(T) / 2; ++i) {
                    std::swap(bytes[i], bytes[sizeof(T) - 1 - i]);
                }
                return value;
            }
        } else if constexpr (sizeof(T) == 2) {
            return static_cast<T>(__builtin_bswap16(static_cast<uint16_t>(value)));
        } else if constexpr (sizeof(T) == 4) {
            return static_cast<T>(__builtin_bswap32(static_cast<uint32_t>(value)));
        } else if constexpr (sizeof(T) == 8) {
            return static_cast<T>(__builtin_bswap64(static_cast<uint64_t>(value)));
        } else {
            auto* bytes = reinterpret_cast<uint8_t*>(&value);
            for (std::size_t i = 0; i < sizeof(T) / 2; ++i) {
                std::swap(bytes[i], bytes[sizeof(T) - 1 - i]);
            }
            return value;
        }
    }

    template<typename T>
    [[nodiscard]] constexpr T convert_endian(T value, endian from, endian to) noexcept {
        if (from == to) {
            return value;
        }
        return byteswap(value);
    }

    template<typename T>
    [[nodiscard]] constexpr T from_endian(T value, endian order) noexcept {
        return convert_endian(value, order, endian::native);
    }

    template<typename T>
    [[nodiscard]] constexpr T to_endian(T value, endian order) noexcept {
        return convert_endian(value, endian::native, order);
    }

} // namespace bstream
