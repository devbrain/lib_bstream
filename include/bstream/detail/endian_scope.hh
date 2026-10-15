#pragma once

#include <bstream/detail/platform.hh>

#include <bstream/types.hh>

namespace bstream::detail {

    template<typename Stream>
    class endian_scope {
    public:
        endian_scope(Stream& stream, endian order) noexcept
            : stream_(stream), previous_(stream.byte_order()) {
            stream_.set_byte_order(order);
        }

        ~endian_scope() { stream_.set_byte_order(previous_); }

        endian_scope(const endian_scope&) = delete;
        endian_scope& operator=(const endian_scope&) = delete;

    private:
        Stream& stream_;
        endian previous_;
    };

    template<typename Stream>
    endian_scope(Stream&, endian) -> endian_scope<Stream>;

} // namespace bstream::detail
