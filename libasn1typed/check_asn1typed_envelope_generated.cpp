#include <runtime.hpp>
#include "normal_body_types.hpp"
#include "normal_body_mapping.hpp"
#include "normal_body_codec.hpp"
#include "normal_envelope_types.hpp"
#include "normal_envelope_mapping.hpp"
#include "normal_envelope_codec.hpp"
#include "upper_body_types.hpp"
#include "upper_body_mapping.hpp"
#include "upper_body_codec.hpp"
#include "upper_envelope_types.hpp"
#include "upper_envelope_mapping.hpp"
#include "upper_envelope_codec.hpp"
#include "normal_success_body_types.hpp"
#include "normal_success_body_mapping.hpp"
#include "normal_success_body_codec.hpp"
#include "normal_success_envelope_types.hpp"
#include "normal_success_envelope_mapping.hpp"
#include "normal_success_envelope_codec.hpp"
#include "normal_failure_body_types.hpp"
#include "normal_failure_body_mapping.hpp"
#include "normal_failure_body_codec.hpp"
#include "normal_failure_envelope_types.hpp"
#include "normal_failure_envelope_mapping.hpp"
#include "normal_failure_envelope_codec.hpp"
#include "normal_four_body_types.hpp"
#include "normal_four_body_mapping.hpp"
#include "normal_four_body_codec.hpp"
#include "normal_four_envelope_types.hpp"
#include "normal_four_envelope_mapping.hpp"
#include "normal_four_envelope_codec.hpp"
#include <cstdio>
#include <cstdlib>
#include <type_traits>
#define REQUIRE(x) do { if(!(x)) { ::std::fprintf(stderr,"%s:%d: %s\n",__FILE__,__LINE__,#x); ::std::abort(); } } while(0)
using Normal = ::normal::nrforge::EnvelopeEvidenceEnvelope_aper;
using Upper = ::upper::nrforge::EnvelopeEvidenceEnvelope_aper;
static_assert(::std::is_same_v<Normal::body_type,::normal::nrforge::IocCppGenerationDispatchMessage>);
static_assert(::std::is_same_v<Upper::body_type,::upper::nrforge::AbcDef>);
static_assert(Normal::target_code == 73 && Upper::target_code == 73);
static_assert(Normal::target_root_ordinal == 1 && Upper::target_root_ordinal == 1);
static_assert(Normal::source_ordinal_to_per_root_index[1] == 1);
static_assert(Upper::source_ordinal_to_per_root_index[1] == 1);
template<class Mapping,class Encode,class Decode> static void check(Encode encode,Decode decode) {
    typename Mapping::wrapper_1 root{};
    root.*Mapping::root_1_procedure_member = Mapping::target_code;
    root.*Mapping::root_1_criticality_member = typename Mapping::criticality_type{Mapping::criticality_type::Known::reject};
    root.*Mapping::root_1_value_member = typename Mapping::target_wrapper_type{};
    typename Mapping::value_type pdu{::std::move(root)};
    const ::std::vector<::std::byte> expected{::std::byte{0x20},::std::byte{0x49},::std::byte{0},::std::byte{3},::std::byte{0},::std::byte{0},::std::byte{0}};
    auto encoded = encode(pdu); REQUIRE(encoded && encoded.value().octets == expected && encoded.value().octet_count == expected.size());
    auto decoded = decode(expected); REQUIRE(decoded);
    const auto& owned_root = ::std::get<typename Mapping::wrapper_1>(decoded.value().value);
    REQUIRE(owned_root.*Mapping::root_1_procedure_member == 73);
    REQUIRE(::std::get<typename Mapping::target_wrapper_type>(owned_root.*Mapping::root_1_value_member).value.entries.elements.empty());
}
template<class M, class Root, auto Code, auto Policy, auto Value, class Encode, class Decode>
static void check_outcome(Encode encode, Decode decode, unsigned selector) {
    Root root{};
    root.*Code = M::target_code;
    root.*Policy = typename M::criticality_type{M::criticality_type::Known::reject};
    root.*Value = typename M::target_wrapper_type{};
    typename M::value_type pdu{root};
    const std::vector<std::byte> expected{std::byte{static_cast<unsigned char>(selector)},std::byte{0x49},std::byte{0},std::byte{3},std::byte{0},std::byte{0},std::byte{0}};
    auto encoded = encode(pdu);
    REQUIRE(encoded && encoded.value().octets == expected && encoded.value().octet_count == expected.size());
    auto decoded = decode(expected); REQUIRE(decoded);
    const auto& received = std::get<Root>(decoded.value().value);
    REQUIRE(received.*Code == 73 && (received.*Policy).value == M::criticality_type::Known::reject);
    REQUIRE(std::get<typename M::target_wrapper_type>(received.*Value).value.entries.elements.empty());
    root.*Code = 72;
    auto wrong_code = encode(typename M::value_type{root});
    REQUIRE(!wrong_code && wrong_code.error().code == nrforge::aper::ErrorCode::constraint_violation && wrong_code.error().bit_offset == 0);
    root.*Code = 73; root.*Value = typename M::opaque_type{};
    auto opaque_target = encode(typename M::value_type{root});
    REQUIRE(!opaque_target && opaque_target.error().code == nrforge::aper::ErrorCode::constraint_violation);
    auto trailing = expected; trailing.push_back(std::byte{0});
    auto rejected = decode(trailing); REQUIRE(!rejected && rejected.error().code == nrforge::aper::ErrorCode::trailing_data);
}
using Success = outcome::success::EnvelopeEvidenceEnvelope_aper;
using Failure = outcome::failure::EnvelopeEvidenceEnvelope_aper;
static_assert(Success::target_root_ordinal == 0 && Failure::target_root_ordinal == 2);
static_assert(Success::source_ordinal_to_per_root_index[0] == 2 && Failure::source_ordinal_to_per_root_index[2] == 0);
using Four = four::nrforge::EnvelopeEvidenceFourEnvelope_aper;
static_assert(Four::root_count == 4 && !Four::extensible);
static_assert(Four::target_root_ordinal == 1 && Four::source_ordinal_to_per_root_index[1] == 1);
static_assert(Four::root_roles[3] == 3 && Four::source_ordinal_to_per_root_index[3] == 3);
static void check_four_root() {
    check_outcome<Four,Four::wrapper_1,Four::root_1_procedure_member,Four::root_1_criticality_member,Four::root_1_value_member>(
        [](const auto& v) { return four::nrforge::encode_envelope_evidence_four_envelope(v); },
        [](const auto& b) { return four::nrforge::decode_envelope_evidence_four_envelope(b); },0x40);
    const std::vector<std::byte> fourth{std::byte{0xc0}};
    auto unsupported = four::nrforge::decode_envelope_evidence_four_envelope(fourth);
    REQUIRE(!unsupported && unsupported.error().code == nrforge::aper::ErrorCode::constraint_violation && unsupported.error().bit_offset == 2);
    using FourthRoot = std::variant_alternative_t<3,decltype(Four::value_type{}.value)>;
    auto forbidden_encoding = four::nrforge::encode_envelope_evidence_four_envelope(Four::value_type{FourthRoot{}});
    REQUIRE(!forbidden_encoding && forbidden_encoding.error().code == nrforge::aper::ErrorCode::constraint_violation && forbidden_encoding.error().bit_offset == 0);
    // Declared three-octet known payload, but only two octets supplied.
    const std::vector<std::byte> malformed_known{std::byte{0x40},std::byte{0x49},std::byte{0},std::byte{3},std::byte{0},std::byte{0}};
    auto malformed = four::nrforge::decode_envelope_evidence_four_envelope(malformed_known);
    REQUIRE(!malformed && malformed.error().code == nrforge::aper::ErrorCode::truncated_input);
    const std::vector<std::byte> dirty_alignment{std::byte{0x41},std::byte{0x49},std::byte{0},std::byte{3},std::byte{0},std::byte{0},std::byte{0}};
    auto padding = four::nrforge::decode_envelope_evidence_four_envelope(dirty_alignment);
    REQUIRE(!padding && padding.error().code == nrforge::aper::ErrorCode::nonzero_padding);
}
int main() {
    check_four_root();
    check<Normal>([](const auto& value) { return ::normal::nrforge::encode_envelope_evidence_envelope(value); },
                  [](const auto& bytes) { return ::normal::nrforge::decode_envelope_evidence_envelope(bytes); });
    check<Upper>([](const auto& value) { return ::upper::nrforge::encode_envelope_evidence_envelope(value); },
                 [](const auto& bytes) { return ::upper::nrforge::decode_envelope_evidence_envelope(bytes); });
    check_outcome<Success,Success::wrapper_0,Success::root_0_procedure_member,Success::root_0_criticality_member,Success::root_0_value_member>(
        [](const auto& v) { return outcome::success::encode_envelope_evidence_envelope(v); },
        [](const auto& b) { return outcome::success::decode_envelope_evidence_envelope(b); }, 0x40);
    check_outcome<Failure,Failure::wrapper_2,Failure::root_2_procedure_member,Failure::root_2_criticality_member,Failure::root_2_value_member>(
        [](const auto& v) { return outcome::failure::encode_envelope_evidence_envelope(v); },
        [](const auto& b) { return outcome::failure::decode_envelope_evidence_envelope(b); }, 0x00);
    ::std::puts("PASS metadata-derived three/four-root envelope framing, unsupported-root rejection and acronym body identity");
}
