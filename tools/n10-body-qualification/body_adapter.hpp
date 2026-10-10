#ifndef NRFORGE_N10_BODY_ADAPTER_HPP
#define NRFORGE_N10_BODY_ADAPTER_HPP
// Target integration only: consumes headers generated from the untouched six
// frozen modules. No handwritten body codec or NGAP-PDU envelope is supplied.
#include "runtime.hpp"
#include "body_types.hpp"
#include "body_mapping.hpp"
#include "body_codec.hpp"
#include <limits>
#include <type_traits>

namespace n10_integration {
namespace generated = ::n10::body;
using Body = generated::NgapPduContentsUeContextReleaseCommand;
using BodyMapping = generated::NgapPduContentsUeContextReleaseCommand_aper;
using Container = BodyMapping::field_0_type;
using ContainerMapping = BodyMapping::field_0_payload_mapping;
using Entry = ContainerMapping::element_type;
using EntryMapping = ContainerMapping::element_payload_mapping;
using Criticality = EntryMapping::criticality_type;
static_assert(BodyMapping::root_field_count == 1);
static_assert(ContainerMapping::lower_bound == 0 && ContainerMapping::upper_bound == 65535);
static_assert(EntryMapping::rows.size() == 2);

consteval ::std::size_t row_for_id(::std::uint64_t id) {
    for(::std::size_t i = 0; i < EntryMapping::rows.size(); ++i)
        if(EntryMapping::rows[i].id == id) return i;
    return ::std::numeric_limits<::std::size_t>::max();
}
template<::std::size_t> struct Row;
template<> struct Row<0> {
    using wrapper = EntryMapping::wrapper_0;
    using payload = EntryMapping::payload_type_0;
    using mapping = EntryMapping::payload_mapping_0;
};
template<> struct Row<1> {
    using wrapper = EntryMapping::wrapper_1;
    using payload = EntryMapping::payload_type_1;
    using mapping = EntryMapping::payload_mapping_1;
};
inline constexpr auto ue_row = row_for_id(114);
inline constexpr auto cause_row = row_for_id(15);
static_assert(ue_row != ::std::numeric_limits<::std::size_t>::max());
static_assert(cause_row != ::std::numeric_limits<::std::size_t>::max());
using UeIe = Row<ue_row>::wrapper;
using UeIds = Row<ue_row>::payload;
using UeMapping = Row<ue_row>::mapping;
using CauseIe = Row<cause_row>::wrapper;
using Cause = Row<cause_row>::payload;
using CauseMapping = Row<cause_row>::mapping;
static_assert(::std::is_same_v<UeIds, generated::NgapIEsUeNgapIDs>);
static_assert(::std::is_same_v<Cause, generated::NgapIEsCause>);
static_assert(EntryMapping::rows[ue_row].expected_criticality == 0);
static_assert(EntryMapping::rows[cause_row].expected_criticality == 1);
static_assert(EntryMapping::rows[ue_row].has_presence && EntryMapping::rows[ue_row].presence == 0);
static_assert(EntryMapping::rows[cause_row].has_presence && EntryMapping::rows[cause_row].presence == 0);
// Ordinals below select named/source wrapper identities, never PER selectors.
// The generated codec remains authoritative for storage↔wire mappings.
static_assert(UeMapping::root_count == 3 && CauseMapping::root_count == 6);
static_assert(::std::is_same_v<UeMapping::wrapper_0, generated::NgapIEsUeNgapIDs_u_e_ngap_id_pair>);
static_assert(::std::is_same_v<UeMapping::wrapper_1, generated::NgapIEsUeNgapIDs_a_mf_ue_ngap_id>);
static_assert(::std::is_same_v<UeMapping::wrapper_2, generated::NgapIEsUeNgapIDs_choice_extensions>);
static_assert(::std::is_same_v<CauseMapping::wrapper_0, generated::NgapIEsCause_radio_network>);
static_assert(::std::is_same_v<CauseMapping::wrapper_1, generated::NgapIEsCause_transport>);
static_assert(::std::is_same_v<CauseMapping::wrapper_2, generated::NgapIEsCause_nas>);
static_assert(::std::is_same_v<CauseMapping::wrapper_3, generated::NgapIEsCause_protocol>);
static_assert(::std::is_same_v<CauseMapping::wrapper_4, generated::NgapIEsCause_misc>);
static_assert(::std::is_same_v<CauseMapping::wrapper_5, generated::NgapIEsCause_choice_extensions>);
using Pair = UeMapping::payload_type_0;
using PairMapping = UeMapping::payload_mapping_0;
using PairExtensionContainer = PairMapping::field_2_type;
using PairExtensionMapping = PairMapping::field_2_payload_mapping::element_payload_mapping;
using PairExtension = PairExtensionMapping::value_type;
using UeExtension = UeMapping::payload_type_2;
using UeExtensionMapping = UeMapping::payload_mapping_2;
using CauseExtension = CauseMapping::payload_type_5;
using CauseExtensionMapping = CauseMapping::payload_mapping_5;
static_assert(PairMapping::field_2_payload_mapping::lower_bound == 1);
static_assert(PairMapping::field_2_payload_mapping::upper_bound == 65535);
static_assert(UeExtensionMapping::rows.empty() && UeExtensionMapping::object_set_extensible);
static_assert(CauseExtensionMapping::rows.empty() && CauseExtensionMapping::object_set_extensible);
static_assert(PairExtensionMapping::rows.empty() && PairExtensionMapping::object_set_extensible);

inline auto encode_body(const Body& body, const ::nrforge::aper::Limits& limits = {}) {
    return generated::encode_ngap_pdu_contents_ue_context_release_command(body, limits);
}
inline auto decode_body(::std::span<const ::std::byte> bytes, const ::nrforge::aper::Limits& limits = {}) {
    return generated::decode_ngap_pdu_contents_ue_context_release_command(bytes, limits);
}
} // namespace n10_integration
#endif
