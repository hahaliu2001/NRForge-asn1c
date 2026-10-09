#ifndef NRFORGE_APER_SEQUENCE_EXTENSIONS_HPP
#define NRFORGE_APER_SEQUENCE_EXTENSIONS_HPP

#include <cstddef>
#include <cstdint>
#include <vector>

namespace nrforge::aper {

// Owned opaque complete encodings. These values do not authorize re-encoding.
struct UnknownSequenceAddition {
    std::uint64_t addition_index = 0;
    std::vector<std::byte> payload_octets;
};

struct SequenceExtensionData {
    std::size_t received_bitmap_bit_count = 0;
    std::vector<UnknownSequenceAddition> unknown_additions;
};

} // namespace nrforge::aper
#endif
