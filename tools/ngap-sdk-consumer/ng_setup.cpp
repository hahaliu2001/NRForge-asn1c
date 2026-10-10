// Generated fixture example; regenerate with prepare.py. No test adapters or private codecs.
#include <ngap.hpp>
#include <messages/NgSetupRequest.hpp>
#include <messages/NgSetupResponse.hpp>
#include <messages/NgSetupFailure.hpp>
#include <cstdlib>
#include <iostream>
#include <type_traits>
#define REQUIRE(...) do { if(!(__VA_ARGS__)) { ::std::cerr << "consumer failure " << __LINE__ << "\n"; ::std::abort(); } } while(0)
static void check_NGSetupRequest() {
using namespace ::nrforge::ngap;
::nrforge::ngap::messages::NgSetupRequest::Body body{};
body.protocol_i_es.elements.resize(3);
body.protocol_i_es.elements[0].id = UINT64_C(27);
body.protocol_i_es.elements[0].criticality.value = ::nrforge::ngap::messages::NgSetupRequest::NgapCommonDataTypesCriticality::Known::reject;
::nrforge::ngap::messages::NgSetupRequest::NgapContainersProtocolIeFieldNgapPduContentsNgSetupRequestIEs_global_ran_node_id open_1{};
::nrforge::ngap::messages::NgSetupRequest::NgapIEsGlobalRanNodeId_global_gnb_id alternative_2{};
alternative_2.value.p_lmn_identity = ::std::vector<::std::byte>{::std::byte{0x00},::std::byte{0x00},::std::byte{0x00}};
::nrforge::ngap::messages::NgSetupRequest::NgapIEsGnbId_g_nb_id alternative_3{};
alternative_3.value.octets = ::std::vector<::std::byte>{::std::byte{0x00},::std::byte{0x00},::std::byte{0x00}};
alternative_3.value.bit_count = 22;
alternative_2.value.g_nb_id = ::std::move(alternative_3);
open_1.value = ::std::move(alternative_2);
body.protocol_i_es.elements[0].value = ::std::move(open_1);
body.protocol_i_es.elements[1].id = UINT64_C(102);
body.protocol_i_es.elements[1].criticality.value = ::nrforge::ngap::messages::NgSetupRequest::NgapCommonDataTypesCriticality::Known::reject;
::nrforge::ngap::messages::NgSetupRequest::NgapContainersProtocolIeFieldNgapPduContentsNgSetupRequestIEs_supported_ta_list open_4{};
open_4.value.elements.resize(1);
open_4.value.elements[0].t_ac = ::std::vector<::std::byte>{::std::byte{0x00},::std::byte{0x00},::std::byte{0x00}};
open_4.value.elements[0].broadcast_plmn_list.elements.resize(1);
open_4.value.elements[0].broadcast_plmn_list.elements[0].p_lmn_identity = ::std::vector<::std::byte>{::std::byte{0x00},::std::byte{0x00},::std::byte{0x00}};
open_4.value.elements[0].broadcast_plmn_list.elements[0].t_ai_slice_support_list.elements.resize(1);
open_4.value.elements[0].broadcast_plmn_list.elements[0].t_ai_slice_support_list.elements[0].s_nssai.s_st = ::std::vector<::std::byte>{::std::byte{0x00}};
body.protocol_i_es.elements[1].value = ::std::move(open_4);
body.protocol_i_es.elements[2].id = UINT64_C(21);
body.protocol_i_es.elements[2].criticality.value = ::nrforge::ngap::messages::NgSetupRequest::NgapCommonDataTypesCriticality::Known::ignore;
::nrforge::ngap::messages::NgSetupRequest::NgapContainersProtocolIeFieldNgapPduContentsNgSetupRequestIEs_default_paging_drx open_5{};
open_5.value.value = ::nrforge::ngap::messages::NgSetupRequest::NgapIEsPagingDrx::Known::v_32;
body.protocol_i_es.elements[2].value = ::std::move(open_5);
auto expected = make_ngap_pdu(::std::move(body), Criticality::reject);
REQUIRE(expected);
const auto native_wire = ::std::vector<::std::byte>{::std::byte{0x00},::std::byte{0x15},::std::byte{0x00},::std::byte{0x25},::std::byte{0x00},::std::byte{0x00},::std::byte{0x03},::std::byte{0x00},::std::byte{0x1b},::std::byte{0x00},::std::byte{0x08},::std::byte{0x00},::std::byte{0x00},::std::byte{0x00},::std::byte{0x00},::std::byte{0x00},::std::byte{0x00},::std::byte{0x00},::std::byte{0x00},::std::byte{0x00},::std::byte{0x66},::std::byte{0x00},::std::byte{0x0d},::std::byte{0x00},::std::byte{0x00},::std::byte{0x00},::std::byte{0x00},::std::byte{0x00},::std::byte{0x00},::std::byte{0x00},::std::byte{0x00},::std::byte{0x00},::std::byte{0x00},::std::byte{0x00},::std::byte{0x00},::std::byte{0x00},::std::byte{0x00},::std::byte{0x15},::std::byte{0x40},::std::byte{0x01},::std::byte{0x00}};
auto decoded = decode_ngap_pdu(native_wire);
REQUIRE(decoded);
REQUIRE(decoded.value().kind() == PduKind::typed);
const auto* header = decoded.value().root_header();
REQUIRE(header && static_cast<unsigned>(header->role) == 0 && header->procedure_code == 21 && header->received_criticality == Criticality::reject);
const auto* metadata = decoded.value().message_info();
REQUIRE(metadata && metadata->message == "NGSetupRequest" && metadata->module == "NGAP-PDU-Contents");
const auto* actual_body_pointer = decoded.value().body_if<::nrforge::ngap::messages::NgSetupRequest::Body>();
REQUIRE(actual_body_pointer);
const auto& actual_body = *actual_body_pointer;
REQUIRE(actual_body.sequence_extensions.received_bitmap_bit_count == 0);
REQUIRE(actual_body.sequence_extensions.unknown_additions.empty());
REQUIRE(actual_body.protocol_i_es.elements.size() == 3);
REQUIRE(actual_body.protocol_i_es.elements[0].id == UINT64_C(27));
REQUIRE(actual_body.protocol_i_es.elements[0].criticality.value == ::nrforge::ngap::messages::NgSetupRequest::NgapCommonDataTypesCriticality::Known::reject);
REQUIRE(::std::holds_alternative<::nrforge::ngap::messages::NgSetupRequest::NgapContainersProtocolIeFieldNgapPduContentsNgSetupRequestIEs_global_ran_node_id>(actual_body.protocol_i_es.elements[0].value));
REQUIRE(::std::holds_alternative<::nrforge::ngap::messages::NgSetupRequest::NgapIEsGlobalRanNodeId_global_gnb_id>(::std::get<::nrforge::ngap::messages::NgSetupRequest::NgapContainersProtocolIeFieldNgapPduContentsNgSetupRequestIEs_global_ran_node_id>(actual_body.protocol_i_es.elements[0].value).value));
REQUIRE(::std::get<::nrforge::ngap::messages::NgSetupRequest::NgapIEsGlobalRanNodeId_global_gnb_id>(::std::get<::nrforge::ngap::messages::NgSetupRequest::NgapContainersProtocolIeFieldNgapPduContentsNgSetupRequestIEs_global_ran_node_id>(actual_body.protocol_i_es.elements[0].value).value).value.sequence_extensions.received_bitmap_bit_count == 0);
REQUIRE(::std::get<::nrforge::ngap::messages::NgSetupRequest::NgapIEsGlobalRanNodeId_global_gnb_id>(::std::get<::nrforge::ngap::messages::NgSetupRequest::NgapContainersProtocolIeFieldNgapPduContentsNgSetupRequestIEs_global_ran_node_id>(actual_body.protocol_i_es.elements[0].value).value).value.sequence_extensions.unknown_additions.empty());
REQUIRE(::std::get<::nrforge::ngap::messages::NgSetupRequest::NgapIEsGlobalRanNodeId_global_gnb_id>(::std::get<::nrforge::ngap::messages::NgSetupRequest::NgapContainersProtocolIeFieldNgapPduContentsNgSetupRequestIEs_global_ran_node_id>(actual_body.protocol_i_es.elements[0].value).value).value.p_lmn_identity == ::std::vector<::std::byte>{::std::byte{0x00},::std::byte{0x00},::std::byte{0x00}});
REQUIRE(::std::holds_alternative<::nrforge::ngap::messages::NgSetupRequest::NgapIEsGnbId_g_nb_id>(::std::get<::nrforge::ngap::messages::NgSetupRequest::NgapIEsGlobalRanNodeId_global_gnb_id>(::std::get<::nrforge::ngap::messages::NgSetupRequest::NgapContainersProtocolIeFieldNgapPduContentsNgSetupRequestIEs_global_ran_node_id>(actual_body.protocol_i_es.elements[0].value).value).value.g_nb_id));
REQUIRE(::std::get<::nrforge::ngap::messages::NgSetupRequest::NgapIEsGnbId_g_nb_id>(::std::get<::nrforge::ngap::messages::NgSetupRequest::NgapIEsGlobalRanNodeId_global_gnb_id>(::std::get<::nrforge::ngap::messages::NgSetupRequest::NgapContainersProtocolIeFieldNgapPduContentsNgSetupRequestIEs_global_ran_node_id>(actual_body.protocol_i_es.elements[0].value).value).value.g_nb_id).value.octets == ::std::vector<::std::byte>{::std::byte{0x00},::std::byte{0x00},::std::byte{0x00}});
REQUIRE(::std::get<::nrforge::ngap::messages::NgSetupRequest::NgapIEsGnbId_g_nb_id>(::std::get<::nrforge::ngap::messages::NgSetupRequest::NgapIEsGlobalRanNodeId_global_gnb_id>(::std::get<::nrforge::ngap::messages::NgSetupRequest::NgapContainersProtocolIeFieldNgapPduContentsNgSetupRequestIEs_global_ran_node_id>(actual_body.protocol_i_es.elements[0].value).value).value.g_nb_id).value.bit_count == 22);
REQUIRE(::std::get<::nrforge::ngap::messages::NgSetupRequest::NgapIEsGlobalRanNodeId_global_gnb_id>(::std::get<::nrforge::ngap::messages::NgSetupRequest::NgapContainersProtocolIeFieldNgapPduContentsNgSetupRequestIEs_global_ran_node_id>(actual_body.protocol_i_es.elements[0].value).value).value.i_e_extensions.has_value() == false);
REQUIRE(actual_body.protocol_i_es.elements[1].id == UINT64_C(102));
REQUIRE(actual_body.protocol_i_es.elements[1].criticality.value == ::nrforge::ngap::messages::NgSetupRequest::NgapCommonDataTypesCriticality::Known::reject);
REQUIRE(::std::holds_alternative<::nrforge::ngap::messages::NgSetupRequest::NgapContainersProtocolIeFieldNgapPduContentsNgSetupRequestIEs_supported_ta_list>(actual_body.protocol_i_es.elements[1].value));
REQUIRE(::std::get<::nrforge::ngap::messages::NgSetupRequest::NgapContainersProtocolIeFieldNgapPduContentsNgSetupRequestIEs_supported_ta_list>(actual_body.protocol_i_es.elements[1].value).value.elements.size() == 1);
REQUIRE(::std::get<::nrforge::ngap::messages::NgSetupRequest::NgapContainersProtocolIeFieldNgapPduContentsNgSetupRequestIEs_supported_ta_list>(actual_body.protocol_i_es.elements[1].value).value.elements[0].sequence_extensions.received_bitmap_bit_count == 0);
REQUIRE(::std::get<::nrforge::ngap::messages::NgSetupRequest::NgapContainersProtocolIeFieldNgapPduContentsNgSetupRequestIEs_supported_ta_list>(actual_body.protocol_i_es.elements[1].value).value.elements[0].sequence_extensions.unknown_additions.empty());
REQUIRE(::std::get<::nrforge::ngap::messages::NgSetupRequest::NgapContainersProtocolIeFieldNgapPduContentsNgSetupRequestIEs_supported_ta_list>(actual_body.protocol_i_es.elements[1].value).value.elements[0].t_ac == ::std::vector<::std::byte>{::std::byte{0x00},::std::byte{0x00},::std::byte{0x00}});
REQUIRE(::std::get<::nrforge::ngap::messages::NgSetupRequest::NgapContainersProtocolIeFieldNgapPduContentsNgSetupRequestIEs_supported_ta_list>(actual_body.protocol_i_es.elements[1].value).value.elements[0].broadcast_plmn_list.elements.size() == 1);
REQUIRE(::std::get<::nrforge::ngap::messages::NgSetupRequest::NgapContainersProtocolIeFieldNgapPduContentsNgSetupRequestIEs_supported_ta_list>(actual_body.protocol_i_es.elements[1].value).value.elements[0].broadcast_plmn_list.elements[0].sequence_extensions.received_bitmap_bit_count == 0);
REQUIRE(::std::get<::nrforge::ngap::messages::NgSetupRequest::NgapContainersProtocolIeFieldNgapPduContentsNgSetupRequestIEs_supported_ta_list>(actual_body.protocol_i_es.elements[1].value).value.elements[0].broadcast_plmn_list.elements[0].sequence_extensions.unknown_additions.empty());
REQUIRE(::std::get<::nrforge::ngap::messages::NgSetupRequest::NgapContainersProtocolIeFieldNgapPduContentsNgSetupRequestIEs_supported_ta_list>(actual_body.protocol_i_es.elements[1].value).value.elements[0].broadcast_plmn_list.elements[0].p_lmn_identity == ::std::vector<::std::byte>{::std::byte{0x00},::std::byte{0x00},::std::byte{0x00}});
REQUIRE(::std::get<::nrforge::ngap::messages::NgSetupRequest::NgapContainersProtocolIeFieldNgapPduContentsNgSetupRequestIEs_supported_ta_list>(actual_body.protocol_i_es.elements[1].value).value.elements[0].broadcast_plmn_list.elements[0].t_ai_slice_support_list.elements.size() == 1);
REQUIRE(::std::get<::nrforge::ngap::messages::NgSetupRequest::NgapContainersProtocolIeFieldNgapPduContentsNgSetupRequestIEs_supported_ta_list>(actual_body.protocol_i_es.elements[1].value).value.elements[0].broadcast_plmn_list.elements[0].t_ai_slice_support_list.elements[0].sequence_extensions.received_bitmap_bit_count == 0);
REQUIRE(::std::get<::nrforge::ngap::messages::NgSetupRequest::NgapContainersProtocolIeFieldNgapPduContentsNgSetupRequestIEs_supported_ta_list>(actual_body.protocol_i_es.elements[1].value).value.elements[0].broadcast_plmn_list.elements[0].t_ai_slice_support_list.elements[0].sequence_extensions.unknown_additions.empty());
REQUIRE(::std::get<::nrforge::ngap::messages::NgSetupRequest::NgapContainersProtocolIeFieldNgapPduContentsNgSetupRequestIEs_supported_ta_list>(actual_body.protocol_i_es.elements[1].value).value.elements[0].broadcast_plmn_list.elements[0].t_ai_slice_support_list.elements[0].s_nssai.sequence_extensions.received_bitmap_bit_count == 0);
REQUIRE(::std::get<::nrforge::ngap::messages::NgSetupRequest::NgapContainersProtocolIeFieldNgapPduContentsNgSetupRequestIEs_supported_ta_list>(actual_body.protocol_i_es.elements[1].value).value.elements[0].broadcast_plmn_list.elements[0].t_ai_slice_support_list.elements[0].s_nssai.sequence_extensions.unknown_additions.empty());
REQUIRE(::std::get<::nrforge::ngap::messages::NgSetupRequest::NgapContainersProtocolIeFieldNgapPduContentsNgSetupRequestIEs_supported_ta_list>(actual_body.protocol_i_es.elements[1].value).value.elements[0].broadcast_plmn_list.elements[0].t_ai_slice_support_list.elements[0].s_nssai.s_st == ::std::vector<::std::byte>{::std::byte{0x00}});
REQUIRE(::std::get<::nrforge::ngap::messages::NgSetupRequest::NgapContainersProtocolIeFieldNgapPduContentsNgSetupRequestIEs_supported_ta_list>(actual_body.protocol_i_es.elements[1].value).value.elements[0].broadcast_plmn_list.elements[0].t_ai_slice_support_list.elements[0].s_nssai.s_d.has_value() == false);
REQUIRE(::std::get<::nrforge::ngap::messages::NgSetupRequest::NgapContainersProtocolIeFieldNgapPduContentsNgSetupRequestIEs_supported_ta_list>(actual_body.protocol_i_es.elements[1].value).value.elements[0].broadcast_plmn_list.elements[0].t_ai_slice_support_list.elements[0].s_nssai.i_e_extensions.has_value() == false);
REQUIRE(::std::get<::nrforge::ngap::messages::NgSetupRequest::NgapContainersProtocolIeFieldNgapPduContentsNgSetupRequestIEs_supported_ta_list>(actual_body.protocol_i_es.elements[1].value).value.elements[0].broadcast_plmn_list.elements[0].t_ai_slice_support_list.elements[0].i_e_extensions.has_value() == false);
REQUIRE(::std::get<::nrforge::ngap::messages::NgSetupRequest::NgapContainersProtocolIeFieldNgapPduContentsNgSetupRequestIEs_supported_ta_list>(actual_body.protocol_i_es.elements[1].value).value.elements[0].broadcast_plmn_list.elements[0].i_e_extensions.has_value() == false);
REQUIRE(::std::get<::nrforge::ngap::messages::NgSetupRequest::NgapContainersProtocolIeFieldNgapPduContentsNgSetupRequestIEs_supported_ta_list>(actual_body.protocol_i_es.elements[1].value).value.elements[0].i_e_extensions.has_value() == false);
REQUIRE(actual_body.protocol_i_es.elements[2].id == UINT64_C(21));
REQUIRE(actual_body.protocol_i_es.elements[2].criticality.value == ::nrforge::ngap::messages::NgSetupRequest::NgapCommonDataTypesCriticality::Known::ignore);
REQUIRE(::std::holds_alternative<::nrforge::ngap::messages::NgSetupRequest::NgapContainersProtocolIeFieldNgapPduContentsNgSetupRequestIEs_default_paging_drx>(actual_body.protocol_i_es.elements[2].value));
REQUIRE(::std::get<::nrforge::ngap::messages::NgSetupRequest::NgapIEsPagingDrx::Known>(::std::get<::nrforge::ngap::messages::NgSetupRequest::NgapContainersProtocolIeFieldNgapPduContentsNgSetupRequestIEs_default_paging_drx>(actual_body.protocol_i_es.elements[2].value).value.value) == ::nrforge::ngap::messages::NgSetupRequest::NgapIEsPagingDrx::Known::v_32);
auto encoded = encode_ngap_pdu(expected.value());
REQUIRE(encoded);
REQUIRE(encoded.value().octets.size() == native_wire.size());
REQUIRE(encoded.value().octets == native_wire);
::std::cout << "PASS NGSetupRequest ";
for(auto octet:encoded.value().octets) { const auto v=::std::to_integer<unsigned>(octet); ::std::cout << "0123456789abcdef"[v>>4] << "0123456789abcdef"[v&15]; }
::std::cout << "\n";
}
void run_NGSetupRequest() {
check_NGSetupRequest();
}
static void check_NGSetupResponse() {
using namespace ::nrforge::ngap;
::nrforge::ngap::messages::NgSetupResponse::Body body{};
body.protocol_i_es.elements.resize(4);
body.protocol_i_es.elements[0].id = UINT64_C(1);
body.protocol_i_es.elements[0].criticality.value = ::nrforge::ngap::messages::NgSetupResponse::NgapCommonDataTypesCriticality::Known::reject;
::nrforge::ngap::messages::NgSetupResponse::NgapContainersProtocolIeFieldNgapPduContentsNgSetupResponseIEs_amf_name open_1{};
open_1.value = ::std::string("\x41", 1);
body.protocol_i_es.elements[0].value = ::std::move(open_1);
body.protocol_i_es.elements[1].id = UINT64_C(96);
body.protocol_i_es.elements[1].criticality.value = ::nrforge::ngap::messages::NgSetupResponse::NgapCommonDataTypesCriticality::Known::reject;
::nrforge::ngap::messages::NgSetupResponse::NgapContainersProtocolIeFieldNgapPduContentsNgSetupResponseIEs_served_guami_list open_2{};
open_2.value.elements.resize(1);
open_2.value.elements[0].g_uami.p_lmn_identity = ::std::vector<::std::byte>{::std::byte{0x00},::std::byte{0x00},::std::byte{0x00}};
open_2.value.elements[0].g_uami.a_mf_region_id.octets = ::std::vector<::std::byte>{::std::byte{0x00}};
open_2.value.elements[0].g_uami.a_mf_region_id.bit_count = 8;
open_2.value.elements[0].g_uami.a_mf_set_id.octets = ::std::vector<::std::byte>{::std::byte{0x00},::std::byte{0x00}};
open_2.value.elements[0].g_uami.a_mf_set_id.bit_count = 10;
open_2.value.elements[0].g_uami.a_mf_pointer.octets = ::std::vector<::std::byte>{::std::byte{0x00}};
open_2.value.elements[0].g_uami.a_mf_pointer.bit_count = 6;
body.protocol_i_es.elements[1].value = ::std::move(open_2);
body.protocol_i_es.elements[2].id = UINT64_C(86);
body.protocol_i_es.elements[2].criticality.value = ::nrforge::ngap::messages::NgSetupResponse::NgapCommonDataTypesCriticality::Known::ignore;
::nrforge::ngap::messages::NgSetupResponse::NgapContainersProtocolIeFieldNgapPduContentsNgSetupResponseIEs_relative_amf_capacity open_3{};
open_3.value = UINT64_C(0);
body.protocol_i_es.elements[2].value = ::std::move(open_3);
body.protocol_i_es.elements[3].id = UINT64_C(80);
body.protocol_i_es.elements[3].criticality.value = ::nrforge::ngap::messages::NgSetupResponse::NgapCommonDataTypesCriticality::Known::reject;
::nrforge::ngap::messages::NgSetupResponse::NgapContainersProtocolIeFieldNgapPduContentsNgSetupResponseIEs_plmn_support_list open_4{};
open_4.value.elements.resize(1);
open_4.value.elements[0].p_lmn_identity = ::std::vector<::std::byte>{::std::byte{0x00},::std::byte{0x00},::std::byte{0x00}};
open_4.value.elements[0].slice_support_list.elements.resize(1);
open_4.value.elements[0].slice_support_list.elements[0].s_nssai.s_st = ::std::vector<::std::byte>{::std::byte{0x00}};
body.protocol_i_es.elements[3].value = ::std::move(open_4);
auto expected = make_ngap_pdu(::std::move(body), Criticality::reject);
REQUIRE(expected);
const auto native_wire = ::std::vector<::std::byte>{::std::byte{0x20},::std::byte{0x15},::std::byte{0x00},::std::byte{0x27},::std::byte{0x00},::std::byte{0x00},::std::byte{0x04},::std::byte{0x00},::std::byte{0x01},::std::byte{0x00},::std::byte{0x03},::std::byte{0x00},::std::byte{0x00},::std::byte{0x41},::std::byte{0x00},::std::byte{0x60},::std::byte{0x00},::std::byte{0x08},::std::byte{0x00},::std::byte{0x00},::std::byte{0x00},::std::byte{0x00},::std::byte{0x00},::std::byte{0x00},::std::byte{0x00},::std::byte{0x00},::std::byte{0x00},::std::byte{0x56},::std::byte{0x40},::std::byte{0x01},::std::byte{0x00},::std::byte{0x00},::std::byte{0x50},::std::byte{0x00},::std::byte{0x08},::std::byte{0x00},::std::byte{0x00},::std::byte{0x00},::std::byte{0x00},::std::byte{0x00},::std::byte{0x00},::std::byte{0x00},::std::byte{0x00}};
auto decoded = decode_ngap_pdu(native_wire);
REQUIRE(decoded);
REQUIRE(decoded.value().kind() == PduKind::typed);
const auto* header = decoded.value().root_header();
REQUIRE(header && static_cast<unsigned>(header->role) == 1 && header->procedure_code == 21 && header->received_criticality == Criticality::reject);
const auto* metadata = decoded.value().message_info();
REQUIRE(metadata && metadata->message == "NGSetupResponse" && metadata->module == "NGAP-PDU-Contents");
const auto* actual_body_pointer = decoded.value().body_if<::nrforge::ngap::messages::NgSetupResponse::Body>();
REQUIRE(actual_body_pointer);
const auto& actual_body = *actual_body_pointer;
REQUIRE(actual_body.sequence_extensions.received_bitmap_bit_count == 0);
REQUIRE(actual_body.sequence_extensions.unknown_additions.empty());
REQUIRE(actual_body.protocol_i_es.elements.size() == 4);
REQUIRE(actual_body.protocol_i_es.elements[0].id == UINT64_C(1));
REQUIRE(actual_body.protocol_i_es.elements[0].criticality.value == ::nrforge::ngap::messages::NgSetupResponse::NgapCommonDataTypesCriticality::Known::reject);
REQUIRE(::std::holds_alternative<::nrforge::ngap::messages::NgSetupResponse::NgapContainersProtocolIeFieldNgapPduContentsNgSetupResponseIEs_amf_name>(actual_body.protocol_i_es.elements[0].value));
REQUIRE(::std::get<::nrforge::ngap::messages::NgSetupResponse::NgapContainersProtocolIeFieldNgapPduContentsNgSetupResponseIEs_amf_name>(actual_body.protocol_i_es.elements[0].value).value == ::std::string("\x41", 1));
REQUIRE(actual_body.protocol_i_es.elements[1].id == UINT64_C(96));
REQUIRE(actual_body.protocol_i_es.elements[1].criticality.value == ::nrforge::ngap::messages::NgSetupResponse::NgapCommonDataTypesCriticality::Known::reject);
REQUIRE(::std::holds_alternative<::nrforge::ngap::messages::NgSetupResponse::NgapContainersProtocolIeFieldNgapPduContentsNgSetupResponseIEs_served_guami_list>(actual_body.protocol_i_es.elements[1].value));
REQUIRE(::std::get<::nrforge::ngap::messages::NgSetupResponse::NgapContainersProtocolIeFieldNgapPduContentsNgSetupResponseIEs_served_guami_list>(actual_body.protocol_i_es.elements[1].value).value.elements.size() == 1);
REQUIRE(::std::get<::nrforge::ngap::messages::NgSetupResponse::NgapContainersProtocolIeFieldNgapPduContentsNgSetupResponseIEs_served_guami_list>(actual_body.protocol_i_es.elements[1].value).value.elements[0].sequence_extensions.received_bitmap_bit_count == 0);
REQUIRE(::std::get<::nrforge::ngap::messages::NgSetupResponse::NgapContainersProtocolIeFieldNgapPduContentsNgSetupResponseIEs_served_guami_list>(actual_body.protocol_i_es.elements[1].value).value.elements[0].sequence_extensions.unknown_additions.empty());
REQUIRE(::std::get<::nrforge::ngap::messages::NgSetupResponse::NgapContainersProtocolIeFieldNgapPduContentsNgSetupResponseIEs_served_guami_list>(actual_body.protocol_i_es.elements[1].value).value.elements[0].g_uami.sequence_extensions.received_bitmap_bit_count == 0);
REQUIRE(::std::get<::nrforge::ngap::messages::NgSetupResponse::NgapContainersProtocolIeFieldNgapPduContentsNgSetupResponseIEs_served_guami_list>(actual_body.protocol_i_es.elements[1].value).value.elements[0].g_uami.sequence_extensions.unknown_additions.empty());
REQUIRE(::std::get<::nrforge::ngap::messages::NgSetupResponse::NgapContainersProtocolIeFieldNgapPduContentsNgSetupResponseIEs_served_guami_list>(actual_body.protocol_i_es.elements[1].value).value.elements[0].g_uami.p_lmn_identity == ::std::vector<::std::byte>{::std::byte{0x00},::std::byte{0x00},::std::byte{0x00}});
REQUIRE(::std::get<::nrforge::ngap::messages::NgSetupResponse::NgapContainersProtocolIeFieldNgapPduContentsNgSetupResponseIEs_served_guami_list>(actual_body.protocol_i_es.elements[1].value).value.elements[0].g_uami.a_mf_region_id.octets == ::std::vector<::std::byte>{::std::byte{0x00}});
REQUIRE(::std::get<::nrforge::ngap::messages::NgSetupResponse::NgapContainersProtocolIeFieldNgapPduContentsNgSetupResponseIEs_served_guami_list>(actual_body.protocol_i_es.elements[1].value).value.elements[0].g_uami.a_mf_region_id.bit_count == 8);
REQUIRE(::std::get<::nrforge::ngap::messages::NgSetupResponse::NgapContainersProtocolIeFieldNgapPduContentsNgSetupResponseIEs_served_guami_list>(actual_body.protocol_i_es.elements[1].value).value.elements[0].g_uami.a_mf_set_id.octets == ::std::vector<::std::byte>{::std::byte{0x00},::std::byte{0x00}});
REQUIRE(::std::get<::nrforge::ngap::messages::NgSetupResponse::NgapContainersProtocolIeFieldNgapPduContentsNgSetupResponseIEs_served_guami_list>(actual_body.protocol_i_es.elements[1].value).value.elements[0].g_uami.a_mf_set_id.bit_count == 10);
REQUIRE(::std::get<::nrforge::ngap::messages::NgSetupResponse::NgapContainersProtocolIeFieldNgapPduContentsNgSetupResponseIEs_served_guami_list>(actual_body.protocol_i_es.elements[1].value).value.elements[0].g_uami.a_mf_pointer.octets == ::std::vector<::std::byte>{::std::byte{0x00}});
REQUIRE(::std::get<::nrforge::ngap::messages::NgSetupResponse::NgapContainersProtocolIeFieldNgapPduContentsNgSetupResponseIEs_served_guami_list>(actual_body.protocol_i_es.elements[1].value).value.elements[0].g_uami.a_mf_pointer.bit_count == 6);
REQUIRE(::std::get<::nrforge::ngap::messages::NgSetupResponse::NgapContainersProtocolIeFieldNgapPduContentsNgSetupResponseIEs_served_guami_list>(actual_body.protocol_i_es.elements[1].value).value.elements[0].g_uami.i_e_extensions.has_value() == false);
REQUIRE(::std::get<::nrforge::ngap::messages::NgSetupResponse::NgapContainersProtocolIeFieldNgapPduContentsNgSetupResponseIEs_served_guami_list>(actual_body.protocol_i_es.elements[1].value).value.elements[0].backup_amf_name.has_value() == false);
REQUIRE(::std::get<::nrforge::ngap::messages::NgSetupResponse::NgapContainersProtocolIeFieldNgapPduContentsNgSetupResponseIEs_served_guami_list>(actual_body.protocol_i_es.elements[1].value).value.elements[0].i_e_extensions.has_value() == false);
REQUIRE(actual_body.protocol_i_es.elements[2].id == UINT64_C(86));
REQUIRE(actual_body.protocol_i_es.elements[2].criticality.value == ::nrforge::ngap::messages::NgSetupResponse::NgapCommonDataTypesCriticality::Known::ignore);
REQUIRE(::std::holds_alternative<::nrforge::ngap::messages::NgSetupResponse::NgapContainersProtocolIeFieldNgapPduContentsNgSetupResponseIEs_relative_amf_capacity>(actual_body.protocol_i_es.elements[2].value));
REQUIRE(::std::get<::nrforge::ngap::messages::NgSetupResponse::NgapContainersProtocolIeFieldNgapPduContentsNgSetupResponseIEs_relative_amf_capacity>(actual_body.protocol_i_es.elements[2].value).value == UINT64_C(0));
REQUIRE(actual_body.protocol_i_es.elements[3].id == UINT64_C(80));
REQUIRE(actual_body.protocol_i_es.elements[3].criticality.value == ::nrforge::ngap::messages::NgSetupResponse::NgapCommonDataTypesCriticality::Known::reject);
REQUIRE(::std::holds_alternative<::nrforge::ngap::messages::NgSetupResponse::NgapContainersProtocolIeFieldNgapPduContentsNgSetupResponseIEs_plmn_support_list>(actual_body.protocol_i_es.elements[3].value));
REQUIRE(::std::get<::nrforge::ngap::messages::NgSetupResponse::NgapContainersProtocolIeFieldNgapPduContentsNgSetupResponseIEs_plmn_support_list>(actual_body.protocol_i_es.elements[3].value).value.elements.size() == 1);
REQUIRE(::std::get<::nrforge::ngap::messages::NgSetupResponse::NgapContainersProtocolIeFieldNgapPduContentsNgSetupResponseIEs_plmn_support_list>(actual_body.protocol_i_es.elements[3].value).value.elements[0].sequence_extensions.received_bitmap_bit_count == 0);
REQUIRE(::std::get<::nrforge::ngap::messages::NgSetupResponse::NgapContainersProtocolIeFieldNgapPduContentsNgSetupResponseIEs_plmn_support_list>(actual_body.protocol_i_es.elements[3].value).value.elements[0].sequence_extensions.unknown_additions.empty());
REQUIRE(::std::get<::nrforge::ngap::messages::NgSetupResponse::NgapContainersProtocolIeFieldNgapPduContentsNgSetupResponseIEs_plmn_support_list>(actual_body.protocol_i_es.elements[3].value).value.elements[0].p_lmn_identity == ::std::vector<::std::byte>{::std::byte{0x00},::std::byte{0x00},::std::byte{0x00}});
REQUIRE(::std::get<::nrforge::ngap::messages::NgSetupResponse::NgapContainersProtocolIeFieldNgapPduContentsNgSetupResponseIEs_plmn_support_list>(actual_body.protocol_i_es.elements[3].value).value.elements[0].slice_support_list.elements.size() == 1);
REQUIRE(::std::get<::nrforge::ngap::messages::NgSetupResponse::NgapContainersProtocolIeFieldNgapPduContentsNgSetupResponseIEs_plmn_support_list>(actual_body.protocol_i_es.elements[3].value).value.elements[0].slice_support_list.elements[0].sequence_extensions.received_bitmap_bit_count == 0);
REQUIRE(::std::get<::nrforge::ngap::messages::NgSetupResponse::NgapContainersProtocolIeFieldNgapPduContentsNgSetupResponseIEs_plmn_support_list>(actual_body.protocol_i_es.elements[3].value).value.elements[0].slice_support_list.elements[0].sequence_extensions.unknown_additions.empty());
REQUIRE(::std::get<::nrforge::ngap::messages::NgSetupResponse::NgapContainersProtocolIeFieldNgapPduContentsNgSetupResponseIEs_plmn_support_list>(actual_body.protocol_i_es.elements[3].value).value.elements[0].slice_support_list.elements[0].s_nssai.sequence_extensions.received_bitmap_bit_count == 0);
REQUIRE(::std::get<::nrforge::ngap::messages::NgSetupResponse::NgapContainersProtocolIeFieldNgapPduContentsNgSetupResponseIEs_plmn_support_list>(actual_body.protocol_i_es.elements[3].value).value.elements[0].slice_support_list.elements[0].s_nssai.sequence_extensions.unknown_additions.empty());
REQUIRE(::std::get<::nrforge::ngap::messages::NgSetupResponse::NgapContainersProtocolIeFieldNgapPduContentsNgSetupResponseIEs_plmn_support_list>(actual_body.protocol_i_es.elements[3].value).value.elements[0].slice_support_list.elements[0].s_nssai.s_st == ::std::vector<::std::byte>{::std::byte{0x00}});
REQUIRE(::std::get<::nrforge::ngap::messages::NgSetupResponse::NgapContainersProtocolIeFieldNgapPduContentsNgSetupResponseIEs_plmn_support_list>(actual_body.protocol_i_es.elements[3].value).value.elements[0].slice_support_list.elements[0].s_nssai.s_d.has_value() == false);
REQUIRE(::std::get<::nrforge::ngap::messages::NgSetupResponse::NgapContainersProtocolIeFieldNgapPduContentsNgSetupResponseIEs_plmn_support_list>(actual_body.protocol_i_es.elements[3].value).value.elements[0].slice_support_list.elements[0].s_nssai.i_e_extensions.has_value() == false);
REQUIRE(::std::get<::nrforge::ngap::messages::NgSetupResponse::NgapContainersProtocolIeFieldNgapPduContentsNgSetupResponseIEs_plmn_support_list>(actual_body.protocol_i_es.elements[3].value).value.elements[0].slice_support_list.elements[0].i_e_extensions.has_value() == false);
REQUIRE(::std::get<::nrforge::ngap::messages::NgSetupResponse::NgapContainersProtocolIeFieldNgapPduContentsNgSetupResponseIEs_plmn_support_list>(actual_body.protocol_i_es.elements[3].value).value.elements[0].i_e_extensions.has_value() == false);
auto encoded = encode_ngap_pdu(expected.value());
REQUIRE(encoded);
REQUIRE(encoded.value().octets.size() == native_wire.size());
REQUIRE(encoded.value().octets == native_wire);
::std::cout << "PASS NGSetupResponse ";
for(auto octet:encoded.value().octets) { const auto v=::std::to_integer<unsigned>(octet); ::std::cout << "0123456789abcdef"[v>>4] << "0123456789abcdef"[v&15]; }
::std::cout << "\n";
}
void run_NGSetupResponse() {
check_NGSetupResponse();
}
static void check_NGSetupFailure() {
using namespace ::nrforge::ngap;
::nrforge::ngap::messages::NgSetupFailure::Body body{};
body.protocol_i_es.elements.resize(1);
body.protocol_i_es.elements[0].id = UINT64_C(15);
body.protocol_i_es.elements[0].criticality.value = ::nrforge::ngap::messages::NgSetupFailure::NgapCommonDataTypesCriticality::Known::ignore;
::nrforge::ngap::messages::NgSetupFailure::NgapContainersProtocolIeFieldNgapPduContentsNgSetupFailureIEs_cause open_1{};
::nrforge::ngap::messages::NgSetupFailure::NgapIEsCause_radio_network alternative_2{};
alternative_2.value.value = ::nrforge::ngap::messages::NgSetupFailure::NgapIEsCauseRadioNetwork::Known::unspecified;
open_1.value = ::std::move(alternative_2);
body.protocol_i_es.elements[0].value = ::std::move(open_1);
auto expected = make_ngap_pdu(::std::move(body), Criticality::reject);
REQUIRE(expected);
const auto native_wire = ::std::vector<::std::byte>{::std::byte{0x40},::std::byte{0x15},::std::byte{0x00},::std::byte{0x09},::std::byte{0x00},::std::byte{0x00},::std::byte{0x01},::std::byte{0x00},::std::byte{0x0f},::std::byte{0x40},::std::byte{0x02},::std::byte{0x00},::std::byte{0x00}};
auto decoded = decode_ngap_pdu(native_wire);
REQUIRE(decoded);
REQUIRE(decoded.value().kind() == PduKind::typed);
const auto* header = decoded.value().root_header();
REQUIRE(header && static_cast<unsigned>(header->role) == 2 && header->procedure_code == 21 && header->received_criticality == Criticality::reject);
const auto* metadata = decoded.value().message_info();
REQUIRE(metadata && metadata->message == "NGSetupFailure" && metadata->module == "NGAP-PDU-Contents");
const auto* actual_body_pointer = decoded.value().body_if<::nrforge::ngap::messages::NgSetupFailure::Body>();
REQUIRE(actual_body_pointer);
const auto& actual_body = *actual_body_pointer;
REQUIRE(actual_body.sequence_extensions.received_bitmap_bit_count == 0);
REQUIRE(actual_body.sequence_extensions.unknown_additions.empty());
REQUIRE(actual_body.protocol_i_es.elements.size() == 1);
REQUIRE(actual_body.protocol_i_es.elements[0].id == UINT64_C(15));
REQUIRE(actual_body.protocol_i_es.elements[0].criticality.value == ::nrforge::ngap::messages::NgSetupFailure::NgapCommonDataTypesCriticality::Known::ignore);
REQUIRE(::std::holds_alternative<::nrforge::ngap::messages::NgSetupFailure::NgapContainersProtocolIeFieldNgapPduContentsNgSetupFailureIEs_cause>(actual_body.protocol_i_es.elements[0].value));
REQUIRE(::std::holds_alternative<::nrforge::ngap::messages::NgSetupFailure::NgapIEsCause_radio_network>(::std::get<::nrforge::ngap::messages::NgSetupFailure::NgapContainersProtocolIeFieldNgapPduContentsNgSetupFailureIEs_cause>(actual_body.protocol_i_es.elements[0].value).value));
REQUIRE(::std::get<::nrforge::ngap::messages::NgSetupFailure::NgapIEsCauseRadioNetwork::Known>(::std::get<::nrforge::ngap::messages::NgSetupFailure::NgapIEsCause_radio_network>(::std::get<::nrforge::ngap::messages::NgSetupFailure::NgapContainersProtocolIeFieldNgapPduContentsNgSetupFailureIEs_cause>(actual_body.protocol_i_es.elements[0].value).value).value.value) == ::nrforge::ngap::messages::NgSetupFailure::NgapIEsCauseRadioNetwork::Known::unspecified);
auto encoded = encode_ngap_pdu(expected.value());
REQUIRE(encoded);
REQUIRE(encoded.value().octets.size() == native_wire.size());
REQUIRE(encoded.value().octets == native_wire);
::std::cout << "PASS NGSetupFailure ";
for(auto octet:encoded.value().octets) { const auto v=::std::to_integer<unsigned>(octet); ::std::cout << "0123456789abcdef"[v>>4] << "0123456789abcdef"[v&15]; }
::std::cout << "\n";
}
void run_NGSetupFailure() {
check_NGSetupFailure();
}
int main() {
const auto& library = ::nrforge::ngap::sdk_identity();
const auto& headers = ::nrforge::ngap::header_sdk_identity;
REQUIRE(library.version == headers.version && library.fingerprint == headers.fingerprint);
REQUIRE(library.schema_sha256 == headers.schema_sha256 && library.runtime_sha256 == headers.runtime_sha256);
const auto& registry = ::nrforge::ngap::ngap_registry_state();
REQUIRE(registry && registry.value().message_count() == 131);
run_NGSetupRequest(); run_NGSetupResponse(); run_NGSetupFailure();
}
