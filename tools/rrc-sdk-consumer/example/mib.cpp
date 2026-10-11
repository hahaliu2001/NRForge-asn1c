#include <iostream>
#include <nrforge/rrc/roots/NR_RRC_Definitions/MIB.hpp>
int
main() {
    namespace mib = nrforge::rrc::NR_RRC_Definitions::MIB;
    const std::vector<std::byte> wire{std::byte{0x02}, std::byte{0},
                                      std::byte{0x08}};
    auto decoded = mib::decode(wire);
    if(!decoded || decoded.value().semantic_bits != 23) return 1;
    auto encoded = mib::encode(decoded.value().value);
    if(!encoded || encoded.value().octets != wire
       || encoded.value().semantic_bits != 23)
        return 2;
    std::cout << "MIB: 23 semantic bits, 3 octets\n";
}
