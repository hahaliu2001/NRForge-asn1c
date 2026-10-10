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
int main() {
    check<Normal>([](const auto& value) { return ::normal::nrforge::encode_envelope_evidence_envelope(value); },
                  [](const auto& bytes) { return ::normal::nrforge::decode_envelope_evidence_envelope(bytes); });
    check<Upper>([](const auto& value) { return ::upper::nrforge::encode_envelope_evidence_envelope(value); },
                 [](const auto& bytes) { return ::upper::nrforge::decode_envelope_evidence_envelope(bytes); });
    ::std::puts("PASS metadata-derived synthetic envelope and component-separated acronym body identity");
}
