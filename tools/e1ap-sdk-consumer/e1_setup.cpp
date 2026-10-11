// Generated fixture example; regenerate with prepare.py. No test adapters or private codecs.
#include <nrforge/e1ap/e1ap.hpp>
#include <nrforge/e1ap/messages/GnbCuUpE1SetupRequest.hpp>
#include <nrforge/e1ap/messages/GnbCuUpE1SetupResponse.hpp>
#include <nrforge/e1ap/messages/GnbCuUpE1SetupFailure.hpp>
#include <nrforge/e1ap/messages/GnbCuCpE1SetupRequest.hpp>
#include <nrforge/e1ap/messages/GnbCuCpE1SetupResponse.hpp>
#include <nrforge/e1ap/messages/GnbCuCpE1SetupFailure.hpp>
#include <cstdlib>
#include <iostream>
#include <type_traits>
#define REQUIRE(...) do { if(!(__VA_ARGS__)) { ::std::cerr << "consumer failure " << __LINE__ << "\n"; ::std::abort(); } } while(0)
static void check_GnbCuUpE1SetupRequest() {
using namespace ::nrforge::e1ap;
::nrforge::e1ap::messages::GnbCuUpE1SetupRequest::Body body{};
body.protocol_i_es.elements.resize(4);
body.protocol_i_es.elements[0].id = UINT64_C(57);
body.protocol_i_es.elements[0].criticality.value = ::nrforge::e1ap::messages::GnbCuUpE1SetupRequest::E1ApCommonDataTypesCriticality::Known::reject;
::nrforge::e1ap::messages::GnbCuUpE1SetupRequest::E1ApContainersProtocolIeFieldE1ApPduContentsGnbCuUpE1SetupRequestIEs_transaction_id open_1{};
open_1.value = UINT64_C(0);
body.protocol_i_es.elements[0].value = ::std::move(open_1);
body.protocol_i_es.elements[1].id = UINT64_C(7);
body.protocol_i_es.elements[1].criticality.value = ::nrforge::e1ap::messages::GnbCuUpE1SetupRequest::E1ApCommonDataTypesCriticality::Known::reject;
::nrforge::e1ap::messages::GnbCuUpE1SetupRequest::E1ApContainersProtocolIeFieldE1ApPduContentsGnbCuUpE1SetupRequestIEs_g_nb_cu_up_id open_2{};
open_2.value = UINT64_C(0);
body.protocol_i_es.elements[1].value = ::std::move(open_2);
body.protocol_i_es.elements[2].id = UINT64_C(10);
body.protocol_i_es.elements[2].criticality.value = ::nrforge::e1ap::messages::GnbCuUpE1SetupRequest::E1ApCommonDataTypesCriticality::Known::reject;
::nrforge::e1ap::messages::GnbCuUpE1SetupRequest::E1ApContainersProtocolIeFieldE1ApPduContentsGnbCuUpE1SetupRequestIEs_cn_support open_3{};
open_3.value.value = ::nrforge::e1ap::messages::GnbCuUpE1SetupRequest::E1ApIEsCnSupport::Known::c_epc;
body.protocol_i_es.elements[2].value = ::std::move(open_3);
body.protocol_i_es.elements[3].id = UINT64_C(11);
body.protocol_i_es.elements[3].criticality.value = ::nrforge::e1ap::messages::GnbCuUpE1SetupRequest::E1ApCommonDataTypesCriticality::Known::reject;
::nrforge::e1ap::messages::GnbCuUpE1SetupRequest::E1ApContainersProtocolIeFieldE1ApPduContentsGnbCuUpE1SetupRequestIEs_supported_plm_ns open_4{};
open_4.value.elements.resize(1);
open_4.value.elements[0].p_lmn_identity = ::std::vector<::std::byte>{::std::byte{0x00},::std::byte{0x00},::std::byte{0x00}};
body.protocol_i_es.elements[3].value = ::std::move(open_4);
auto expected = make_e1ap_pdu(::std::move(body), Criticality::reject);
REQUIRE(expected);
const auto native_wire = ::std::vector<::std::byte>{::std::byte{0x00},::std::byte{0x03},::std::byte{0x00},::std::byte{0x1d},::std::byte{0x00},::std::byte{0x00},::std::byte{0x04},::std::byte{0x00},::std::byte{0x39},::std::byte{0x00},::std::byte{0x02},::std::byte{0x00},::std::byte{0x00},::std::byte{0x00},::std::byte{0x07},::std::byte{0x00},::std::byte{0x02},::std::byte{0x00},::std::byte{0x00},::std::byte{0x00},::std::byte{0x0a},::std::byte{0x00},::std::byte{0x01},::std::byte{0x00},::std::byte{0x00},::std::byte{0x0b},::std::byte{0x00},::std::byte{0x05},::std::byte{0x00},::std::byte{0x00},::std::byte{0x00},::std::byte{0x00},::std::byte{0x00}};
auto decoded = decode_e1ap_pdu(native_wire);
REQUIRE(decoded);
REQUIRE(decoded.value().kind() == PduKind::typed);
const auto* header = decoded.value().root_header();
REQUIRE(header && static_cast<unsigned>(header->role) == 0 && header->procedure_code == 3 && header->received_criticality == Criticality::reject);
const auto* metadata = decoded.value().message_info();
REQUIRE(metadata && metadata->message == "GNB-CU-UP-E1SetupRequest" && metadata->module == "E1AP-PDU-Contents");
const auto* actual_body_pointer = decoded.value().body_if<::nrforge::e1ap::messages::GnbCuUpE1SetupRequest::Body>();
REQUIRE(actual_body_pointer);
const auto& actual_body = *actual_body_pointer;
REQUIRE(actual_body.sequence_extensions.received_bitmap_bit_count == 0);
REQUIRE(actual_body.sequence_extensions.unknown_additions.empty());
REQUIRE(actual_body.protocol_i_es.elements.size() == 4);
REQUIRE(actual_body.protocol_i_es.elements[0].id == UINT64_C(57));
REQUIRE(actual_body.protocol_i_es.elements[0].criticality.value == ::nrforge::e1ap::messages::GnbCuUpE1SetupRequest::E1ApCommonDataTypesCriticality::Known::reject);
REQUIRE(::std::holds_alternative<::nrforge::e1ap::messages::GnbCuUpE1SetupRequest::E1ApContainersProtocolIeFieldE1ApPduContentsGnbCuUpE1SetupRequestIEs_transaction_id>(actual_body.protocol_i_es.elements[0].value));
REQUIRE(::std::get<::nrforge::e1ap::messages::GnbCuUpE1SetupRequest::E1ApContainersProtocolIeFieldE1ApPduContentsGnbCuUpE1SetupRequestIEs_transaction_id>(actual_body.protocol_i_es.elements[0].value).value == UINT64_C(0));
REQUIRE(actual_body.protocol_i_es.elements[1].id == UINT64_C(7));
REQUIRE(actual_body.protocol_i_es.elements[1].criticality.value == ::nrforge::e1ap::messages::GnbCuUpE1SetupRequest::E1ApCommonDataTypesCriticality::Known::reject);
REQUIRE(::std::holds_alternative<::nrforge::e1ap::messages::GnbCuUpE1SetupRequest::E1ApContainersProtocolIeFieldE1ApPduContentsGnbCuUpE1SetupRequestIEs_g_nb_cu_up_id>(actual_body.protocol_i_es.elements[1].value));
REQUIRE(::std::get<::nrforge::e1ap::messages::GnbCuUpE1SetupRequest::E1ApContainersProtocolIeFieldE1ApPduContentsGnbCuUpE1SetupRequestIEs_g_nb_cu_up_id>(actual_body.protocol_i_es.elements[1].value).value == UINT64_C(0));
REQUIRE(actual_body.protocol_i_es.elements[2].id == UINT64_C(10));
REQUIRE(actual_body.protocol_i_es.elements[2].criticality.value == ::nrforge::e1ap::messages::GnbCuUpE1SetupRequest::E1ApCommonDataTypesCriticality::Known::reject);
REQUIRE(::std::holds_alternative<::nrforge::e1ap::messages::GnbCuUpE1SetupRequest::E1ApContainersProtocolIeFieldE1ApPduContentsGnbCuUpE1SetupRequestIEs_cn_support>(actual_body.protocol_i_es.elements[2].value));
REQUIRE(::std::get<::nrforge::e1ap::messages::GnbCuUpE1SetupRequest::E1ApIEsCnSupport::Known>(::std::get<::nrforge::e1ap::messages::GnbCuUpE1SetupRequest::E1ApContainersProtocolIeFieldE1ApPduContentsGnbCuUpE1SetupRequestIEs_cn_support>(actual_body.protocol_i_es.elements[2].value).value.value) == ::nrforge::e1ap::messages::GnbCuUpE1SetupRequest::E1ApIEsCnSupport::Known::c_epc);
REQUIRE(actual_body.protocol_i_es.elements[3].id == UINT64_C(11));
REQUIRE(actual_body.protocol_i_es.elements[3].criticality.value == ::nrforge::e1ap::messages::GnbCuUpE1SetupRequest::E1ApCommonDataTypesCriticality::Known::reject);
REQUIRE(::std::holds_alternative<::nrforge::e1ap::messages::GnbCuUpE1SetupRequest::E1ApContainersProtocolIeFieldE1ApPduContentsGnbCuUpE1SetupRequestIEs_supported_plm_ns>(actual_body.protocol_i_es.elements[3].value));
REQUIRE(::std::get<::nrforge::e1ap::messages::GnbCuUpE1SetupRequest::E1ApContainersProtocolIeFieldE1ApPduContentsGnbCuUpE1SetupRequestIEs_supported_plm_ns>(actual_body.protocol_i_es.elements[3].value).value.elements.size() == 1);
REQUIRE(::std::get<::nrforge::e1ap::messages::GnbCuUpE1SetupRequest::E1ApContainersProtocolIeFieldE1ApPduContentsGnbCuUpE1SetupRequestIEs_supported_plm_ns>(actual_body.protocol_i_es.elements[3].value).value.elements[0].sequence_extensions.received_bitmap_bit_count == 0);
REQUIRE(::std::get<::nrforge::e1ap::messages::GnbCuUpE1SetupRequest::E1ApContainersProtocolIeFieldE1ApPduContentsGnbCuUpE1SetupRequestIEs_supported_plm_ns>(actual_body.protocol_i_es.elements[3].value).value.elements[0].sequence_extensions.unknown_additions.empty());
REQUIRE(::std::get<::nrforge::e1ap::messages::GnbCuUpE1SetupRequest::E1ApContainersProtocolIeFieldE1ApPduContentsGnbCuUpE1SetupRequestIEs_supported_plm_ns>(actual_body.protocol_i_es.elements[3].value).value.elements[0].p_lmn_identity == ::std::vector<::std::byte>{::std::byte{0x00},::std::byte{0x00},::std::byte{0x00}});
REQUIRE(::std::get<::nrforge::e1ap::messages::GnbCuUpE1SetupRequest::E1ApContainersProtocolIeFieldE1ApPduContentsGnbCuUpE1SetupRequestIEs_supported_plm_ns>(actual_body.protocol_i_es.elements[3].value).value.elements[0].slice_support_list.has_value() == false);
REQUIRE(::std::get<::nrforge::e1ap::messages::GnbCuUpE1SetupRequest::E1ApContainersProtocolIeFieldE1ApPduContentsGnbCuUpE1SetupRequestIEs_supported_plm_ns>(actual_body.protocol_i_es.elements[3].value).value.elements[0].n_r_cgi_support_list.has_value() == false);
REQUIRE(::std::get<::nrforge::e1ap::messages::GnbCuUpE1SetupRequest::E1ApContainersProtocolIeFieldE1ApPduContentsGnbCuUpE1SetupRequestIEs_supported_plm_ns>(actual_body.protocol_i_es.elements[3].value).value.elements[0].qo_s_parameters_support_list.has_value() == false);
REQUIRE(::std::get<::nrforge::e1ap::messages::GnbCuUpE1SetupRequest::E1ApContainersProtocolIeFieldE1ApPduContentsGnbCuUpE1SetupRequestIEs_supported_plm_ns>(actual_body.protocol_i_es.elements[3].value).value.elements[0].i_e_extensions.has_value() == false);
auto encoded = encode_e1ap_pdu(expected.value());
REQUIRE(encoded);
REQUIRE(encoded.value().octets.size() == native_wire.size());
REQUIRE(encoded.value().octets == native_wire);
::std::cout << "PASS GNB-CU-UP-E1SetupRequest ";
for(auto octet:encoded.value().octets) { const auto v=::std::to_integer<unsigned>(octet); ::std::cout << "0123456789abcdef"[v>>4] << "0123456789abcdef"[v&15]; }
::std::cout << "\n";
}
void run_GnbCuUpE1SetupRequest() {
check_GnbCuUpE1SetupRequest();
}

static void check_GnbCuUpE1SetupResponse() {
using namespace ::nrforge::e1ap;
::nrforge::e1ap::messages::GnbCuUpE1SetupResponse::Body body{};
body.protocol_i_es.elements.resize(1);
body.protocol_i_es.elements[0].id = UINT64_C(57);
body.protocol_i_es.elements[0].criticality.value = ::nrforge::e1ap::messages::GnbCuUpE1SetupResponse::E1ApCommonDataTypesCriticality::Known::reject;
::nrforge::e1ap::messages::GnbCuUpE1SetupResponse::E1ApContainersProtocolIeFieldE1ApPduContentsGnbCuUpE1SetupResponseIEs_transaction_id open_1{};
open_1.value = UINT64_C(0);
body.protocol_i_es.elements[0].value = ::std::move(open_1);
auto expected = make_e1ap_pdu(::std::move(body), Criticality::reject);
REQUIRE(expected);
const auto native_wire = ::std::vector<::std::byte>{::std::byte{0x20},::std::byte{0x03},::std::byte{0x00},::std::byte{0x09},::std::byte{0x00},::std::byte{0x00},::std::byte{0x01},::std::byte{0x00},::std::byte{0x39},::std::byte{0x00},::std::byte{0x02},::std::byte{0x00},::std::byte{0x00}};
auto decoded = decode_e1ap_pdu(native_wire);
REQUIRE(decoded);
REQUIRE(decoded.value().kind() == PduKind::typed);
const auto* header = decoded.value().root_header();
REQUIRE(header && static_cast<unsigned>(header->role) == 1 && header->procedure_code == 3 && header->received_criticality == Criticality::reject);
const auto* metadata = decoded.value().message_info();
REQUIRE(metadata && metadata->message == "GNB-CU-UP-E1SetupResponse" && metadata->module == "E1AP-PDU-Contents");
const auto* actual_body_pointer = decoded.value().body_if<::nrforge::e1ap::messages::GnbCuUpE1SetupResponse::Body>();
REQUIRE(actual_body_pointer);
const auto& actual_body = *actual_body_pointer;
REQUIRE(actual_body.sequence_extensions.received_bitmap_bit_count == 0);
REQUIRE(actual_body.sequence_extensions.unknown_additions.empty());
REQUIRE(actual_body.protocol_i_es.elements.size() == 1);
REQUIRE(actual_body.protocol_i_es.elements[0].id == UINT64_C(57));
REQUIRE(actual_body.protocol_i_es.elements[0].criticality.value == ::nrforge::e1ap::messages::GnbCuUpE1SetupResponse::E1ApCommonDataTypesCriticality::Known::reject);
REQUIRE(::std::holds_alternative<::nrforge::e1ap::messages::GnbCuUpE1SetupResponse::E1ApContainersProtocolIeFieldE1ApPduContentsGnbCuUpE1SetupResponseIEs_transaction_id>(actual_body.protocol_i_es.elements[0].value));
REQUIRE(::std::get<::nrforge::e1ap::messages::GnbCuUpE1SetupResponse::E1ApContainersProtocolIeFieldE1ApPduContentsGnbCuUpE1SetupResponseIEs_transaction_id>(actual_body.protocol_i_es.elements[0].value).value == UINT64_C(0));
auto encoded = encode_e1ap_pdu(expected.value());
REQUIRE(encoded);
REQUIRE(encoded.value().octets.size() == native_wire.size());
REQUIRE(encoded.value().octets == native_wire);
::std::cout << "PASS GNB-CU-UP-E1SetupResponse ";
for(auto octet:encoded.value().octets) { const auto v=::std::to_integer<unsigned>(octet); ::std::cout << "0123456789abcdef"[v>>4] << "0123456789abcdef"[v&15]; }
::std::cout << "\n";
}
void run_GnbCuUpE1SetupResponse() {
check_GnbCuUpE1SetupResponse();
}

static void check_GnbCuUpE1SetupFailure() {
using namespace ::nrforge::e1ap;
::nrforge::e1ap::messages::GnbCuUpE1SetupFailure::Body body{};
body.protocol_i_es.elements.resize(2);
body.protocol_i_es.elements[0].id = UINT64_C(57);
body.protocol_i_es.elements[0].criticality.value = ::nrforge::e1ap::messages::GnbCuUpE1SetupFailure::E1ApCommonDataTypesCriticality::Known::reject;
::nrforge::e1ap::messages::GnbCuUpE1SetupFailure::E1ApContainersProtocolIeFieldE1ApPduContentsGnbCuUpE1SetupFailureIEs_transaction_id open_1{};
open_1.value = UINT64_C(0);
body.protocol_i_es.elements[0].value = ::std::move(open_1);
body.protocol_i_es.elements[1].id = UINT64_C(0);
body.protocol_i_es.elements[1].criticality.value = ::nrforge::e1ap::messages::GnbCuUpE1SetupFailure::E1ApCommonDataTypesCriticality::Known::ignore;
::nrforge::e1ap::messages::GnbCuUpE1SetupFailure::E1ApContainersProtocolIeFieldE1ApPduContentsGnbCuUpE1SetupFailureIEs_cause open_2{};
::nrforge::e1ap::messages::GnbCuUpE1SetupFailure::E1ApIEsCause_radio_network alternative_3{};
alternative_3.value.value = ::nrforge::e1ap::messages::GnbCuUpE1SetupFailure::E1ApIEsCauseRadioNetwork::Known::unspecified;
open_2.value = ::std::move(alternative_3);
body.protocol_i_es.elements[1].value = ::std::move(open_2);
auto expected = make_e1ap_pdu(::std::move(body), Criticality::reject);
REQUIRE(expected);
const auto native_wire = ::std::vector<::std::byte>{::std::byte{0x40},::std::byte{0x03},::std::byte{0x00},::std::byte{0x0f},::std::byte{0x00},::std::byte{0x00},::std::byte{0x02},::std::byte{0x00},::std::byte{0x39},::std::byte{0x00},::std::byte{0x02},::std::byte{0x00},::std::byte{0x00},::std::byte{0x00},::std::byte{0x00},::std::byte{0x40},::std::byte{0x02},::std::byte{0x00},::std::byte{0x00}};
auto decoded = decode_e1ap_pdu(native_wire);
REQUIRE(decoded);
REQUIRE(decoded.value().kind() == PduKind::typed);
const auto* header = decoded.value().root_header();
REQUIRE(header && static_cast<unsigned>(header->role) == 2 && header->procedure_code == 3 && header->received_criticality == Criticality::reject);
const auto* metadata = decoded.value().message_info();
REQUIRE(metadata && metadata->message == "GNB-CU-UP-E1SetupFailure" && metadata->module == "E1AP-PDU-Contents");
const auto* actual_body_pointer = decoded.value().body_if<::nrforge::e1ap::messages::GnbCuUpE1SetupFailure::Body>();
REQUIRE(actual_body_pointer);
const auto& actual_body = *actual_body_pointer;
REQUIRE(actual_body.sequence_extensions.received_bitmap_bit_count == 0);
REQUIRE(actual_body.sequence_extensions.unknown_additions.empty());
REQUIRE(actual_body.protocol_i_es.elements.size() == 2);
REQUIRE(actual_body.protocol_i_es.elements[0].id == UINT64_C(57));
REQUIRE(actual_body.protocol_i_es.elements[0].criticality.value == ::nrforge::e1ap::messages::GnbCuUpE1SetupFailure::E1ApCommonDataTypesCriticality::Known::reject);
REQUIRE(::std::holds_alternative<::nrforge::e1ap::messages::GnbCuUpE1SetupFailure::E1ApContainersProtocolIeFieldE1ApPduContentsGnbCuUpE1SetupFailureIEs_transaction_id>(actual_body.protocol_i_es.elements[0].value));
REQUIRE(::std::get<::nrforge::e1ap::messages::GnbCuUpE1SetupFailure::E1ApContainersProtocolIeFieldE1ApPduContentsGnbCuUpE1SetupFailureIEs_transaction_id>(actual_body.protocol_i_es.elements[0].value).value == UINT64_C(0));
REQUIRE(actual_body.protocol_i_es.elements[1].id == UINT64_C(0));
REQUIRE(actual_body.protocol_i_es.elements[1].criticality.value == ::nrforge::e1ap::messages::GnbCuUpE1SetupFailure::E1ApCommonDataTypesCriticality::Known::ignore);
REQUIRE(::std::holds_alternative<::nrforge::e1ap::messages::GnbCuUpE1SetupFailure::E1ApContainersProtocolIeFieldE1ApPduContentsGnbCuUpE1SetupFailureIEs_cause>(actual_body.protocol_i_es.elements[1].value));
REQUIRE(::std::holds_alternative<::nrforge::e1ap::messages::GnbCuUpE1SetupFailure::E1ApIEsCause_radio_network>(::std::get<::nrforge::e1ap::messages::GnbCuUpE1SetupFailure::E1ApContainersProtocolIeFieldE1ApPduContentsGnbCuUpE1SetupFailureIEs_cause>(actual_body.protocol_i_es.elements[1].value).value));
REQUIRE(::std::get<::nrforge::e1ap::messages::GnbCuUpE1SetupFailure::E1ApIEsCauseRadioNetwork::Known>(::std::get<::nrforge::e1ap::messages::GnbCuUpE1SetupFailure::E1ApIEsCause_radio_network>(::std::get<::nrforge::e1ap::messages::GnbCuUpE1SetupFailure::E1ApContainersProtocolIeFieldE1ApPduContentsGnbCuUpE1SetupFailureIEs_cause>(actual_body.protocol_i_es.elements[1].value).value).value.value) == ::nrforge::e1ap::messages::GnbCuUpE1SetupFailure::E1ApIEsCauseRadioNetwork::Known::unspecified);
auto encoded = encode_e1ap_pdu(expected.value());
REQUIRE(encoded);
REQUIRE(encoded.value().octets.size() == native_wire.size());
REQUIRE(encoded.value().octets == native_wire);
::std::cout << "PASS GNB-CU-UP-E1SetupFailure ";
for(auto octet:encoded.value().octets) { const auto v=::std::to_integer<unsigned>(octet); ::std::cout << "0123456789abcdef"[v>>4] << "0123456789abcdef"[v&15]; }
::std::cout << "\n";
}
void run_GnbCuUpE1SetupFailure() {
check_GnbCuUpE1SetupFailure();
}

static void check_GnbCuCpE1SetupRequest() {
using namespace ::nrforge::e1ap;
::nrforge::e1ap::messages::GnbCuCpE1SetupRequest::Body body{};
body.protocol_i_es.elements.resize(1);
body.protocol_i_es.elements[0].id = UINT64_C(57);
body.protocol_i_es.elements[0].criticality.value = ::nrforge::e1ap::messages::GnbCuCpE1SetupRequest::E1ApCommonDataTypesCriticality::Known::reject;
::nrforge::e1ap::messages::GnbCuCpE1SetupRequest::E1ApContainersProtocolIeFieldE1ApPduContentsGnbCuCpE1SetupRequestIEs_transaction_id open_1{};
open_1.value = UINT64_C(0);
body.protocol_i_es.elements[0].value = ::std::move(open_1);
auto expected = make_e1ap_pdu(::std::move(body), Criticality::reject);
REQUIRE(expected);
const auto native_wire = ::std::vector<::std::byte>{::std::byte{0x00},::std::byte{0x04},::std::byte{0x00},::std::byte{0x09},::std::byte{0x00},::std::byte{0x00},::std::byte{0x01},::std::byte{0x00},::std::byte{0x39},::std::byte{0x00},::std::byte{0x02},::std::byte{0x00},::std::byte{0x00}};
auto decoded = decode_e1ap_pdu(native_wire);
REQUIRE(decoded);
REQUIRE(decoded.value().kind() == PduKind::typed);
const auto* header = decoded.value().root_header();
REQUIRE(header && static_cast<unsigned>(header->role) == 0 && header->procedure_code == 4 && header->received_criticality == Criticality::reject);
const auto* metadata = decoded.value().message_info();
REQUIRE(metadata && metadata->message == "GNB-CU-CP-E1SetupRequest" && metadata->module == "E1AP-PDU-Contents");
const auto* actual_body_pointer = decoded.value().body_if<::nrforge::e1ap::messages::GnbCuCpE1SetupRequest::Body>();
REQUIRE(actual_body_pointer);
const auto& actual_body = *actual_body_pointer;
REQUIRE(actual_body.sequence_extensions.received_bitmap_bit_count == 0);
REQUIRE(actual_body.sequence_extensions.unknown_additions.empty());
REQUIRE(actual_body.protocol_i_es.elements.size() == 1);
REQUIRE(actual_body.protocol_i_es.elements[0].id == UINT64_C(57));
REQUIRE(actual_body.protocol_i_es.elements[0].criticality.value == ::nrforge::e1ap::messages::GnbCuCpE1SetupRequest::E1ApCommonDataTypesCriticality::Known::reject);
REQUIRE(::std::holds_alternative<::nrforge::e1ap::messages::GnbCuCpE1SetupRequest::E1ApContainersProtocolIeFieldE1ApPduContentsGnbCuCpE1SetupRequestIEs_transaction_id>(actual_body.protocol_i_es.elements[0].value));
REQUIRE(::std::get<::nrforge::e1ap::messages::GnbCuCpE1SetupRequest::E1ApContainersProtocolIeFieldE1ApPduContentsGnbCuCpE1SetupRequestIEs_transaction_id>(actual_body.protocol_i_es.elements[0].value).value == UINT64_C(0));
auto encoded = encode_e1ap_pdu(expected.value());
REQUIRE(encoded);
REQUIRE(encoded.value().octets.size() == native_wire.size());
REQUIRE(encoded.value().octets == native_wire);
::std::cout << "PASS GNB-CU-CP-E1SetupRequest ";
for(auto octet:encoded.value().octets) { const auto v=::std::to_integer<unsigned>(octet); ::std::cout << "0123456789abcdef"[v>>4] << "0123456789abcdef"[v&15]; }
::std::cout << "\n";
}
void run_GnbCuCpE1SetupRequest() {
check_GnbCuCpE1SetupRequest();
}

static void check_GnbCuCpE1SetupResponse() {
using namespace ::nrforge::e1ap;
::nrforge::e1ap::messages::GnbCuCpE1SetupResponse::Body body{};
body.protocol_i_es.elements.resize(4);
body.protocol_i_es.elements[0].id = UINT64_C(57);
body.protocol_i_es.elements[0].criticality.value = ::nrforge::e1ap::messages::GnbCuCpE1SetupResponse::E1ApCommonDataTypesCriticality::Known::reject;
::nrforge::e1ap::messages::GnbCuCpE1SetupResponse::E1ApContainersProtocolIeFieldE1ApPduContentsGnbCuCpE1SetupResponseIEs_transaction_id open_1{};
open_1.value = UINT64_C(0);
body.protocol_i_es.elements[0].value = ::std::move(open_1);
body.protocol_i_es.elements[1].id = UINT64_C(7);
body.protocol_i_es.elements[1].criticality.value = ::nrforge::e1ap::messages::GnbCuCpE1SetupResponse::E1ApCommonDataTypesCriticality::Known::reject;
::nrforge::e1ap::messages::GnbCuCpE1SetupResponse::E1ApContainersProtocolIeFieldE1ApPduContentsGnbCuCpE1SetupResponseIEs_g_nb_cu_up_id open_2{};
open_2.value = UINT64_C(0);
body.protocol_i_es.elements[1].value = ::std::move(open_2);
body.protocol_i_es.elements[2].id = UINT64_C(10);
body.protocol_i_es.elements[2].criticality.value = ::nrforge::e1ap::messages::GnbCuCpE1SetupResponse::E1ApCommonDataTypesCriticality::Known::reject;
::nrforge::e1ap::messages::GnbCuCpE1SetupResponse::E1ApContainersProtocolIeFieldE1ApPduContentsGnbCuCpE1SetupResponseIEs_cn_support open_3{};
open_3.value.value = ::nrforge::e1ap::messages::GnbCuCpE1SetupResponse::E1ApIEsCnSupport::Known::c_epc;
body.protocol_i_es.elements[2].value = ::std::move(open_3);
body.protocol_i_es.elements[3].id = UINT64_C(11);
body.protocol_i_es.elements[3].criticality.value = ::nrforge::e1ap::messages::GnbCuCpE1SetupResponse::E1ApCommonDataTypesCriticality::Known::reject;
::nrforge::e1ap::messages::GnbCuCpE1SetupResponse::E1ApContainersProtocolIeFieldE1ApPduContentsGnbCuCpE1SetupResponseIEs_supported_plm_ns open_4{};
open_4.value.elements.resize(1);
open_4.value.elements[0].p_lmn_identity = ::std::vector<::std::byte>{::std::byte{0x00},::std::byte{0x00},::std::byte{0x00}};
body.protocol_i_es.elements[3].value = ::std::move(open_4);
auto expected = make_e1ap_pdu(::std::move(body), Criticality::reject);
REQUIRE(expected);
const auto native_wire = ::std::vector<::std::byte>{::std::byte{0x20},::std::byte{0x04},::std::byte{0x00},::std::byte{0x1d},::std::byte{0x00},::std::byte{0x00},::std::byte{0x04},::std::byte{0x00},::std::byte{0x39},::std::byte{0x00},::std::byte{0x02},::std::byte{0x00},::std::byte{0x00},::std::byte{0x00},::std::byte{0x07},::std::byte{0x00},::std::byte{0x02},::std::byte{0x00},::std::byte{0x00},::std::byte{0x00},::std::byte{0x0a},::std::byte{0x00},::std::byte{0x01},::std::byte{0x00},::std::byte{0x00},::std::byte{0x0b},::std::byte{0x00},::std::byte{0x05},::std::byte{0x00},::std::byte{0x00},::std::byte{0x00},::std::byte{0x00},::std::byte{0x00}};
auto decoded = decode_e1ap_pdu(native_wire);
REQUIRE(decoded);
REQUIRE(decoded.value().kind() == PduKind::typed);
const auto* header = decoded.value().root_header();
REQUIRE(header && static_cast<unsigned>(header->role) == 1 && header->procedure_code == 4 && header->received_criticality == Criticality::reject);
const auto* metadata = decoded.value().message_info();
REQUIRE(metadata && metadata->message == "GNB-CU-CP-E1SetupResponse" && metadata->module == "E1AP-PDU-Contents");
const auto* actual_body_pointer = decoded.value().body_if<::nrforge::e1ap::messages::GnbCuCpE1SetupResponse::Body>();
REQUIRE(actual_body_pointer);
const auto& actual_body = *actual_body_pointer;
REQUIRE(actual_body.sequence_extensions.received_bitmap_bit_count == 0);
REQUIRE(actual_body.sequence_extensions.unknown_additions.empty());
REQUIRE(actual_body.protocol_i_es.elements.size() == 4);
REQUIRE(actual_body.protocol_i_es.elements[0].id == UINT64_C(57));
REQUIRE(actual_body.protocol_i_es.elements[0].criticality.value == ::nrforge::e1ap::messages::GnbCuCpE1SetupResponse::E1ApCommonDataTypesCriticality::Known::reject);
REQUIRE(::std::holds_alternative<::nrforge::e1ap::messages::GnbCuCpE1SetupResponse::E1ApContainersProtocolIeFieldE1ApPduContentsGnbCuCpE1SetupResponseIEs_transaction_id>(actual_body.protocol_i_es.elements[0].value));
REQUIRE(::std::get<::nrforge::e1ap::messages::GnbCuCpE1SetupResponse::E1ApContainersProtocolIeFieldE1ApPduContentsGnbCuCpE1SetupResponseIEs_transaction_id>(actual_body.protocol_i_es.elements[0].value).value == UINT64_C(0));
REQUIRE(actual_body.protocol_i_es.elements[1].id == UINT64_C(7));
REQUIRE(actual_body.protocol_i_es.elements[1].criticality.value == ::nrforge::e1ap::messages::GnbCuCpE1SetupResponse::E1ApCommonDataTypesCriticality::Known::reject);
REQUIRE(::std::holds_alternative<::nrforge::e1ap::messages::GnbCuCpE1SetupResponse::E1ApContainersProtocolIeFieldE1ApPduContentsGnbCuCpE1SetupResponseIEs_g_nb_cu_up_id>(actual_body.protocol_i_es.elements[1].value));
REQUIRE(::std::get<::nrforge::e1ap::messages::GnbCuCpE1SetupResponse::E1ApContainersProtocolIeFieldE1ApPduContentsGnbCuCpE1SetupResponseIEs_g_nb_cu_up_id>(actual_body.protocol_i_es.elements[1].value).value == UINT64_C(0));
REQUIRE(actual_body.protocol_i_es.elements[2].id == UINT64_C(10));
REQUIRE(actual_body.protocol_i_es.elements[2].criticality.value == ::nrforge::e1ap::messages::GnbCuCpE1SetupResponse::E1ApCommonDataTypesCriticality::Known::reject);
REQUIRE(::std::holds_alternative<::nrforge::e1ap::messages::GnbCuCpE1SetupResponse::E1ApContainersProtocolIeFieldE1ApPduContentsGnbCuCpE1SetupResponseIEs_cn_support>(actual_body.protocol_i_es.elements[2].value));
REQUIRE(::std::get<::nrforge::e1ap::messages::GnbCuCpE1SetupResponse::E1ApIEsCnSupport::Known>(::std::get<::nrforge::e1ap::messages::GnbCuCpE1SetupResponse::E1ApContainersProtocolIeFieldE1ApPduContentsGnbCuCpE1SetupResponseIEs_cn_support>(actual_body.protocol_i_es.elements[2].value).value.value) == ::nrforge::e1ap::messages::GnbCuCpE1SetupResponse::E1ApIEsCnSupport::Known::c_epc);
REQUIRE(actual_body.protocol_i_es.elements[3].id == UINT64_C(11));
REQUIRE(actual_body.protocol_i_es.elements[3].criticality.value == ::nrforge::e1ap::messages::GnbCuCpE1SetupResponse::E1ApCommonDataTypesCriticality::Known::reject);
REQUIRE(::std::holds_alternative<::nrforge::e1ap::messages::GnbCuCpE1SetupResponse::E1ApContainersProtocolIeFieldE1ApPduContentsGnbCuCpE1SetupResponseIEs_supported_plm_ns>(actual_body.protocol_i_es.elements[3].value));
REQUIRE(::std::get<::nrforge::e1ap::messages::GnbCuCpE1SetupResponse::E1ApContainersProtocolIeFieldE1ApPduContentsGnbCuCpE1SetupResponseIEs_supported_plm_ns>(actual_body.protocol_i_es.elements[3].value).value.elements.size() == 1);
REQUIRE(::std::get<::nrforge::e1ap::messages::GnbCuCpE1SetupResponse::E1ApContainersProtocolIeFieldE1ApPduContentsGnbCuCpE1SetupResponseIEs_supported_plm_ns>(actual_body.protocol_i_es.elements[3].value).value.elements[0].sequence_extensions.received_bitmap_bit_count == 0);
REQUIRE(::std::get<::nrforge::e1ap::messages::GnbCuCpE1SetupResponse::E1ApContainersProtocolIeFieldE1ApPduContentsGnbCuCpE1SetupResponseIEs_supported_plm_ns>(actual_body.protocol_i_es.elements[3].value).value.elements[0].sequence_extensions.unknown_additions.empty());
REQUIRE(::std::get<::nrforge::e1ap::messages::GnbCuCpE1SetupResponse::E1ApContainersProtocolIeFieldE1ApPduContentsGnbCuCpE1SetupResponseIEs_supported_plm_ns>(actual_body.protocol_i_es.elements[3].value).value.elements[0].p_lmn_identity == ::std::vector<::std::byte>{::std::byte{0x00},::std::byte{0x00},::std::byte{0x00}});
REQUIRE(::std::get<::nrforge::e1ap::messages::GnbCuCpE1SetupResponse::E1ApContainersProtocolIeFieldE1ApPduContentsGnbCuCpE1SetupResponseIEs_supported_plm_ns>(actual_body.protocol_i_es.elements[3].value).value.elements[0].slice_support_list.has_value() == false);
REQUIRE(::std::get<::nrforge::e1ap::messages::GnbCuCpE1SetupResponse::E1ApContainersProtocolIeFieldE1ApPduContentsGnbCuCpE1SetupResponseIEs_supported_plm_ns>(actual_body.protocol_i_es.elements[3].value).value.elements[0].n_r_cgi_support_list.has_value() == false);
REQUIRE(::std::get<::nrforge::e1ap::messages::GnbCuCpE1SetupResponse::E1ApContainersProtocolIeFieldE1ApPduContentsGnbCuCpE1SetupResponseIEs_supported_plm_ns>(actual_body.protocol_i_es.elements[3].value).value.elements[0].qo_s_parameters_support_list.has_value() == false);
REQUIRE(::std::get<::nrforge::e1ap::messages::GnbCuCpE1SetupResponse::E1ApContainersProtocolIeFieldE1ApPduContentsGnbCuCpE1SetupResponseIEs_supported_plm_ns>(actual_body.protocol_i_es.elements[3].value).value.elements[0].i_e_extensions.has_value() == false);
auto encoded = encode_e1ap_pdu(expected.value());
REQUIRE(encoded);
REQUIRE(encoded.value().octets.size() == native_wire.size());
REQUIRE(encoded.value().octets == native_wire);
::std::cout << "PASS GNB-CU-CP-E1SetupResponse ";
for(auto octet:encoded.value().octets) { const auto v=::std::to_integer<unsigned>(octet); ::std::cout << "0123456789abcdef"[v>>4] << "0123456789abcdef"[v&15]; }
::std::cout << "\n";
}
void run_GnbCuCpE1SetupResponse() {
check_GnbCuCpE1SetupResponse();
}

static void check_GnbCuCpE1SetupFailure() {
using namespace ::nrforge::e1ap;
::nrforge::e1ap::messages::GnbCuCpE1SetupFailure::Body body{};
body.protocol_i_es.elements.resize(2);
body.protocol_i_es.elements[0].id = UINT64_C(57);
body.protocol_i_es.elements[0].criticality.value = ::nrforge::e1ap::messages::GnbCuCpE1SetupFailure::E1ApCommonDataTypesCriticality::Known::reject;
::nrforge::e1ap::messages::GnbCuCpE1SetupFailure::E1ApContainersProtocolIeFieldE1ApPduContentsGnbCuCpE1SetupFailureIEs_transaction_id open_1{};
open_1.value = UINT64_C(0);
body.protocol_i_es.elements[0].value = ::std::move(open_1);
body.protocol_i_es.elements[1].id = UINT64_C(0);
body.protocol_i_es.elements[1].criticality.value = ::nrforge::e1ap::messages::GnbCuCpE1SetupFailure::E1ApCommonDataTypesCriticality::Known::ignore;
::nrforge::e1ap::messages::GnbCuCpE1SetupFailure::E1ApContainersProtocolIeFieldE1ApPduContentsGnbCuCpE1SetupFailureIEs_cause open_2{};
::nrforge::e1ap::messages::GnbCuCpE1SetupFailure::E1ApIEsCause_radio_network alternative_3{};
alternative_3.value.value = ::nrforge::e1ap::messages::GnbCuCpE1SetupFailure::E1ApIEsCauseRadioNetwork::Known::unspecified;
open_2.value = ::std::move(alternative_3);
body.protocol_i_es.elements[1].value = ::std::move(open_2);
auto expected = make_e1ap_pdu(::std::move(body), Criticality::reject);
REQUIRE(expected);
const auto native_wire = ::std::vector<::std::byte>{::std::byte{0x40},::std::byte{0x04},::std::byte{0x00},::std::byte{0x0f},::std::byte{0x00},::std::byte{0x00},::std::byte{0x02},::std::byte{0x00},::std::byte{0x39},::std::byte{0x00},::std::byte{0x02},::std::byte{0x00},::std::byte{0x00},::std::byte{0x00},::std::byte{0x00},::std::byte{0x40},::std::byte{0x02},::std::byte{0x00},::std::byte{0x00}};
auto decoded = decode_e1ap_pdu(native_wire);
REQUIRE(decoded);
REQUIRE(decoded.value().kind() == PduKind::typed);
const auto* header = decoded.value().root_header();
REQUIRE(header && static_cast<unsigned>(header->role) == 2 && header->procedure_code == 4 && header->received_criticality == Criticality::reject);
const auto* metadata = decoded.value().message_info();
REQUIRE(metadata && metadata->message == "GNB-CU-CP-E1SetupFailure" && metadata->module == "E1AP-PDU-Contents");
const auto* actual_body_pointer = decoded.value().body_if<::nrforge::e1ap::messages::GnbCuCpE1SetupFailure::Body>();
REQUIRE(actual_body_pointer);
const auto& actual_body = *actual_body_pointer;
REQUIRE(actual_body.sequence_extensions.received_bitmap_bit_count == 0);
REQUIRE(actual_body.sequence_extensions.unknown_additions.empty());
REQUIRE(actual_body.protocol_i_es.elements.size() == 2);
REQUIRE(actual_body.protocol_i_es.elements[0].id == UINT64_C(57));
REQUIRE(actual_body.protocol_i_es.elements[0].criticality.value == ::nrforge::e1ap::messages::GnbCuCpE1SetupFailure::E1ApCommonDataTypesCriticality::Known::reject);
REQUIRE(::std::holds_alternative<::nrforge::e1ap::messages::GnbCuCpE1SetupFailure::E1ApContainersProtocolIeFieldE1ApPduContentsGnbCuCpE1SetupFailureIEs_transaction_id>(actual_body.protocol_i_es.elements[0].value));
REQUIRE(::std::get<::nrforge::e1ap::messages::GnbCuCpE1SetupFailure::E1ApContainersProtocolIeFieldE1ApPduContentsGnbCuCpE1SetupFailureIEs_transaction_id>(actual_body.protocol_i_es.elements[0].value).value == UINT64_C(0));
REQUIRE(actual_body.protocol_i_es.elements[1].id == UINT64_C(0));
REQUIRE(actual_body.protocol_i_es.elements[1].criticality.value == ::nrforge::e1ap::messages::GnbCuCpE1SetupFailure::E1ApCommonDataTypesCriticality::Known::ignore);
REQUIRE(::std::holds_alternative<::nrforge::e1ap::messages::GnbCuCpE1SetupFailure::E1ApContainersProtocolIeFieldE1ApPduContentsGnbCuCpE1SetupFailureIEs_cause>(actual_body.protocol_i_es.elements[1].value));
REQUIRE(::std::holds_alternative<::nrforge::e1ap::messages::GnbCuCpE1SetupFailure::E1ApIEsCause_radio_network>(::std::get<::nrforge::e1ap::messages::GnbCuCpE1SetupFailure::E1ApContainersProtocolIeFieldE1ApPduContentsGnbCuCpE1SetupFailureIEs_cause>(actual_body.protocol_i_es.elements[1].value).value));
REQUIRE(::std::get<::nrforge::e1ap::messages::GnbCuCpE1SetupFailure::E1ApIEsCauseRadioNetwork::Known>(::std::get<::nrforge::e1ap::messages::GnbCuCpE1SetupFailure::E1ApIEsCause_radio_network>(::std::get<::nrforge::e1ap::messages::GnbCuCpE1SetupFailure::E1ApContainersProtocolIeFieldE1ApPduContentsGnbCuCpE1SetupFailureIEs_cause>(actual_body.protocol_i_es.elements[1].value).value).value.value) == ::nrforge::e1ap::messages::GnbCuCpE1SetupFailure::E1ApIEsCauseRadioNetwork::Known::unspecified);
auto encoded = encode_e1ap_pdu(expected.value());
REQUIRE(encoded);
REQUIRE(encoded.value().octets.size() == native_wire.size());
REQUIRE(encoded.value().octets == native_wire);
::std::cout << "PASS GNB-CU-CP-E1SetupFailure ";
for(auto octet:encoded.value().octets) { const auto v=::std::to_integer<unsigned>(octet); ::std::cout << "0123456789abcdef"[v>>4] << "0123456789abcdef"[v&15]; }
::std::cout << "\n";
}
void run_GnbCuCpE1SetupFailure() {
check_GnbCuCpE1SetupFailure();
}

int main() {
const auto& library = ::nrforge::e1ap::sdk_identity();
const auto& headers = ::nrforge::e1ap::header_sdk_identity;
REQUIRE(library.version == headers.version && library.fingerprint == headers.fingerprint);
REQUIRE(library.schema_sha256 == headers.schema_sha256 && library.runtime_sha256 == headers.runtime_sha256);
const auto& registry = ::nrforge::e1ap::e1ap_registry_state();
REQUIRE(registry && registry.value().message_count() == 72);
run_GnbCuUpE1SetupRequest(); run_GnbCuUpE1SetupResponse(); run_GnbCuUpE1SetupFailure(); run_GnbCuCpE1SetupRequest(); run_GnbCuCpE1SetupResponse(); run_GnbCuCpE1SetupFailure();
}
