/* Optional frozen-source replay: headers are produced by
 * check_asn1typed_envelope_render --actual MODULE_LIST ASN1_ROOT OUTPUT_DIR.
 * Empty protocolIEs exercises codec framing, not mandatory-IE policy. */
#include <runtime.hpp>
#include "frozen_success_body_types.hpp"
#include "frozen_success_body_mapping.hpp"
#include "frozen_success_body_codec.hpp"
#include "frozen_success_envelope_types.hpp"
#include "frozen_success_envelope_mapping.hpp"
#include "frozen_success_envelope_codec.hpp"
#include "frozen_failure_body_types.hpp"
#include "frozen_failure_body_mapping.hpp"
#include "frozen_failure_body_codec.hpp"
#include "frozen_failure_envelope_types.hpp"
#include "frozen_failure_envelope_mapping.hpp"
#include "frozen_failure_envelope_codec.hpp"
#include <cstdio>
#include <cstdlib>
#define REQUIRE(x) do { if(!(x)) { std::fprintf(stderr,"%s:%d: %s\n",__FILE__,__LINE__,#x); std::abort(); } } while(0)
using Success = frozen::success::NgapPduDescriptionsNgapPdu_aper;
using Failure = frozen::failure::NgapPduDescriptionsNgapPdu_aper;
static_assert(Success::target_root_ordinal == 1 && Success::target_code == 41);
static_assert(Failure::target_root_ordinal == 2 && Failure::target_code == 0);
template<class M, class Root, auto Code, auto Policy, auto Value, class Encode, class Decode>
static void check(Encode encode, Decode decode, unsigned selector, unsigned code) {
    for(unsigned policy = 0; policy < 3; ++policy) {
        Root root{};
        root.*Code = M::target_code;
        root.*Policy = typename M::criticality_type{static_cast<typename M::criticality_type::Known>(policy)};
        root.*Value = typename M::target_wrapper_type{};
        const std::vector<std::byte> expected{std::byte{static_cast<unsigned char>(selector)},std::byte{static_cast<unsigned char>(code)},std::byte{static_cast<unsigned char>(policy << 6)},std::byte{3},std::byte{0},std::byte{0},std::byte{0}};
        auto encoded = encode(typename M::value_type{root});
        REQUIRE(encoded && encoded.value().octets == expected && encoded.value().octet_count == expected.size());
        auto decoded = decode(expected); REQUIRE(decoded);
        const auto& received = std::get<Root>(decoded.value().value);
        REQUIRE(received.*Code == code && static_cast<unsigned>((received.*Policy).value) == policy);
        REQUIRE(std::get<typename M::target_wrapper_type>(received.*Value).value.protocol_i_es.elements.empty());
        for(std::size_t n = 0; n < expected.size(); ++n) REQUIRE(!decode(std::span<const std::byte>{expected.data(),n}));
        auto trailing = expected; trailing.push_back(std::byte{0});
        auto bad = decode(trailing); REQUIRE(!bad && bad.error().code == nrforge::aper::ErrorCode::trailing_data && bad.error().bit_offset == 56);
    }
}
int main() {
    check<Success,Success::wrapper_1,Success::root_1_procedure_member,Success::root_1_criticality_member,Success::root_1_value_member>(
        [](const auto& v) { return frozen::success::encode_ngap_pdu_descriptions_ngap_pdu(v); },
        [](const auto& b) { return frozen::success::decode_ngap_pdu_descriptions_ngap_pdu(b); },0x20,41);
    check<Failure,Failure::wrapper_2,Failure::root_2_procedure_member,Failure::root_2_criticality_member,Failure::root_2_value_member>(
        [](const auto& v) { return frozen::failure::encode_ngap_pdu_descriptions_ngap_pdu(v); },
        [](const auto& b) { return frozen::failure::decode_ngap_pdu_descriptions_ngap_pdu(b); },0x40,0);
    std::puts("PASS frozen successful/unsuccessful complete-PDU vectors, policies, typed payload, truncation and trailing data");
}
