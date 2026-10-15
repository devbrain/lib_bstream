#include <bstream/bstream.hh>

#include <array>
#include <cstdint>

#if BSTREAM_WITH_BOOST_PFR != EXPECT_PFR
#error "Installed target did not preserve the PFR option"
#endif
#if EXPECT_PFR && !defined(BSTREAM_HAS_PFR)
#error "Installed target did not propagate the PFR dependency"
#endif
#if EXPECT_FAILSAFE && !defined(BSTREAM_HAS_FAILSAFE)
#error "Installed target did not propagate the failsafe dependency"
#endif

int main() {
    std::array<uint8_t, 2> bytes{};
    bstream::span_writer writer(bytes);
    writer.write<uint16_t>(0x1234);
    bstream::span_reader reader(bytes);
    if (reader.read<uint16_t>() != 0x1234) return 1;

#if EXPECT_PFR
    struct Header { const uint16_t value; };
    reader.seek(0);
    if (reader.read<Header>().value != 0x1234) return 2;
#endif
#if EXPECT_FAILSAFE
    reader.seek(0);
    const auto value = reader.try_read<uint16_t>();
    if (!value || *value != 0x1234) return 3;
#endif
    return 0;
}
