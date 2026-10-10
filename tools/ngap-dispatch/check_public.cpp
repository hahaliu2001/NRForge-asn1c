#include "ngap.hpp"
#include <array>
#include <cstdio>
#include <cstdlib>

namespace {
void require(bool value, int line) {
    if(!value) { std::fprintf(stderr,"check_public:%d\n",line); std::abort(); }
}
#define REQUIRE(x) require(static_cast<bool>(x),__LINE__)
using namespace nrforge::ngap;
std::array<std::byte,5> root(unsigned role, unsigned code, unsigned criticality, unsigned payload) {
    return {static_cast<std::byte>(role << 5),static_cast<std::byte>(code),
        static_cast<std::byte>(criticality << 6),std::byte{1},static_cast<std::byte>(payload)};
}
}
int main() {
    const auto& state = ngap_registry_state(); REQUIRE(state);
    REQUIRE(state.value().message_count() == 131);
    unsigned unknown = 0;
    for(;unknown < 256;++unknown) {
        bool registered = false;
        for(std::size_t i=0;i<state.value().message_count();++i)
            if(state.value().message_info(i)->procedure_code == unknown) registered = true;
        if(!registered) break;
    }
    REQUIRE(unknown < 256);
    for(unsigned role=0;role<3;++role) {
        auto wire = root(role,unknown,2,0xa5);
        auto received = decode_ngap_pdu(wire); REQUIRE(received);
        REQUIRE(received.value().kind() == PduKind::opaque_root);
        REQUIRE(received.value().root_header()->procedure_code == unknown);
        REQUIRE(static_cast<unsigned>(received.value().root_header()->role) == role);
        REQUIRE(received.value().root_header()->received_criticality == Criticality::notify);
        wire.back() = std::byte{};
        REQUIRE(received.value().opaque_root()->payload[0] == std::byte{0xa5});
        auto refused = encode_ngap_pdu(received.value()); REQUIRE(!refused);
        REQUIRE(refused.error().code == nrforge::aper::ErrorCode::constraint_violation);
        REQUIRE(refused.error().bit_offset == 0);
        nrforge::aper::Limits limits; limits.max_retained_unknown_payload_octets = 0;
        auto limited = decode_ngap_pdu(root(role,unknown,2,0xa5),limits);
        REQUIRE(!limited);
        REQUIRE(limited.error().code == nrforge::aper::ErrorCode::resource_limit);
        REQUIRE(limited.error().bit_offset == 18);
    }
    std::size_t absent_slots = 0;
    for(unsigned code=0;code<256;++code) {
        std::array<bool,3> present{};
        for(std::size_t i=0;i<state.value().message_count();++i) {
            const auto* info = state.value().message_info(i);
            if(info->procedure_code == code) present[static_cast<unsigned>(info->role)] = true;
        }
        if(!present[0]) continue;
        for(unsigned role=1;role<3;++role) if(!present[role]) {
            auto decoded = decode_ngap_pdu(root(role,code,1,0x5a)); REQUIRE(decoded);
            REQUIRE(decoded.value().kind() == PduKind::opaque_root);
            REQUIRE(decoded.value().root_header()->procedure_code == code);
            REQUIRE(static_cast<unsigned>(decoded.value().root_header()->role) == role);
            REQUIRE(decoded.value().root_header()->received_criticality == Criticality::ignore);
            REQUIRE(decoded.value().opaque_root()->payload.size() == 1);
            REQUIRE(decoded.value().opaque_root()->payload[0] == std::byte{0x5a});
            REQUIRE(!encode_ngap_pdu(decoded.value())); ++absent_slots;
        }
    }
    REQUIRE(absent_slots == 112);
    const std::array extension{std::byte{0x80},std::byte{1},std::byte{0xaa}};
    auto extended = decode_ngap_pdu(extension); REQUIRE(extended);
    REQUIRE(extended.value().kind() == PduKind::unknown_extension);
    REQUIRE(extended.value().unknown_extension()->index == 0);
    REQUIRE(extended.value().unknown_extension()->payload[0] == std::byte{0xaa});
    REQUIRE(!encode_ngap_pdu(extended.value()));
    std::puts("PASS complete registry unknown-code roots, all112 absent outcome slots and outer extension receive-only policy");
}
