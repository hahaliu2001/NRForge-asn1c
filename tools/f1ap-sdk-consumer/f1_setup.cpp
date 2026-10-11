// Generated fixture example; regenerate with prepare.py. No test adapters or private codecs.
#include <nrforge/f1ap/f1ap.hpp>
#include <nrforge/f1ap/messages/F1SetupRequest.hpp>
#include <nrforge/f1ap/messages/F1SetupResponse.hpp>
#include <nrforge/f1ap/messages/F1SetupFailure.hpp>
#include <cstdlib>
#include <iostream>
#include <type_traits>
#define REQUIRE(...) do { if(!(__VA_ARGS__)) { ::std::cerr << "consumer failure " << __LINE__ << "\n"; ::std::abort(); } } while(0)
static void check_F1SetupRequest() {
using namespace ::nrforge::f1ap;
::nrforge::f1ap::messages::F1SetupRequest::Body body{};
body.protocol_i_es.elements.resize(3);
body.protocol_i_es.elements[0].id = UINT64_C(78);
body.protocol_i_es.elements[0].criticality.value = ::nrforge::f1ap::messages::F1SetupRequest::F1ApCommonDataTypesCriticality::Known::reject;
::nrforge::f1ap::messages::F1SetupRequest::F1ApContainersProtocolIeFieldF1ApPduContentsF1SetupRequestIEs_transaction_id open_1{};
open_1.value = UINT64_C(0);
body.protocol_i_es.elements[0].value = ::std::move(open_1);
body.protocol_i_es.elements[1].id = UINT64_C(42);
body.protocol_i_es.elements[1].criticality.value = ::nrforge::f1ap::messages::F1SetupRequest::F1ApCommonDataTypesCriticality::Known::reject;
::nrforge::f1ap::messages::F1SetupRequest::F1ApContainersProtocolIeFieldF1ApPduContentsF1SetupRequestIEs_g_nb_du_id open_2{};
open_2.value = UINT64_C(0);
body.protocol_i_es.elements[1].value = ::std::move(open_2);
body.protocol_i_es.elements[2].id = UINT64_C(171);
body.protocol_i_es.elements[2].criticality.value = ::nrforge::f1ap::messages::F1SetupRequest::F1ApCommonDataTypesCriticality::Known::reject;
::nrforge::f1ap::messages::F1SetupRequest::F1ApContainersProtocolIeFieldF1ApPduContentsF1SetupRequestIEs_gnb_du_rrc_version open_3{};
open_3.value.latest_rrc_version.octets = ::std::vector<::std::byte>{::std::byte{0x00}};
open_3.value.latest_rrc_version.bit_count = 3;
body.protocol_i_es.elements[2].value = ::std::move(open_3);
auto expected = make_f1ap_pdu(::std::move(body), Criticality::reject);
REQUIRE(expected);
const auto native_wire = ::std::vector<::std::byte>{::std::byte{0x00},::std::byte{0x01},::std::byte{0x00},::std::byte{0x14},::std::byte{0x00},::std::byte{0x00},::std::byte{0x03},::std::byte{0x00},::std::byte{0x4e},::std::byte{0x00},::std::byte{0x02},::std::byte{0x00},::std::byte{0x00},::std::byte{0x00},::std::byte{0x2a},::std::byte{0x00},::std::byte{0x02},::std::byte{0x00},::std::byte{0x00},::std::byte{0x00},::std::byte{0xab},::std::byte{0x00},::std::byte{0x01},::std::byte{0x00}};
auto decoded = decode_f1ap_pdu(native_wire);
REQUIRE(decoded);
REQUIRE(decoded.value().kind() == PduKind::typed);
const auto* header = decoded.value().root_header();
REQUIRE(header && static_cast<unsigned>(header->role) == 0 && header->procedure_code == 1 && header->received_criticality == Criticality::reject);
const auto* metadata = decoded.value().message_info();
REQUIRE(metadata && metadata->message == "F1SetupRequest" && metadata->module == "F1AP-PDU-Contents");
const auto* actual_body_pointer = decoded.value().body_if<::nrforge::f1ap::messages::F1SetupRequest::Body>();
REQUIRE(actual_body_pointer);
const auto& actual_body = *actual_body_pointer;
REQUIRE(actual_body.sequence_extensions.received_bitmap_bit_count == 0);
REQUIRE(actual_body.sequence_extensions.unknown_additions.empty());
REQUIRE(actual_body.protocol_i_es.elements.size() == 3);
REQUIRE(actual_body.protocol_i_es.elements[0].id == UINT64_C(78));
REQUIRE(actual_body.protocol_i_es.elements[0].criticality.value == ::nrforge::f1ap::messages::F1SetupRequest::F1ApCommonDataTypesCriticality::Known::reject);
REQUIRE(::std::holds_alternative<::nrforge::f1ap::messages::F1SetupRequest::F1ApContainersProtocolIeFieldF1ApPduContentsF1SetupRequestIEs_transaction_id>(actual_body.protocol_i_es.elements[0].value));
REQUIRE(::std::get<::nrforge::f1ap::messages::F1SetupRequest::F1ApContainersProtocolIeFieldF1ApPduContentsF1SetupRequestIEs_transaction_id>(actual_body.protocol_i_es.elements[0].value).value == UINT64_C(0));
REQUIRE(actual_body.protocol_i_es.elements[1].id == UINT64_C(42));
REQUIRE(actual_body.protocol_i_es.elements[1].criticality.value == ::nrforge::f1ap::messages::F1SetupRequest::F1ApCommonDataTypesCriticality::Known::reject);
REQUIRE(::std::holds_alternative<::nrforge::f1ap::messages::F1SetupRequest::F1ApContainersProtocolIeFieldF1ApPduContentsF1SetupRequestIEs_g_nb_du_id>(actual_body.protocol_i_es.elements[1].value));
REQUIRE(::std::get<::nrforge::f1ap::messages::F1SetupRequest::F1ApContainersProtocolIeFieldF1ApPduContentsF1SetupRequestIEs_g_nb_du_id>(actual_body.protocol_i_es.elements[1].value).value == UINT64_C(0));
REQUIRE(actual_body.protocol_i_es.elements[2].id == UINT64_C(171));
REQUIRE(actual_body.protocol_i_es.elements[2].criticality.value == ::nrforge::f1ap::messages::F1SetupRequest::F1ApCommonDataTypesCriticality::Known::reject);
REQUIRE(::std::holds_alternative<::nrforge::f1ap::messages::F1SetupRequest::F1ApContainersProtocolIeFieldF1ApPduContentsF1SetupRequestIEs_gnb_du_rrc_version>(actual_body.protocol_i_es.elements[2].value));
REQUIRE(::std::get<::nrforge::f1ap::messages::F1SetupRequest::F1ApContainersProtocolIeFieldF1ApPduContentsF1SetupRequestIEs_gnb_du_rrc_version>(actual_body.protocol_i_es.elements[2].value).value.latest_rrc_version.octets == ::std::vector<::std::byte>{::std::byte{0x00}});
REQUIRE(::std::get<::nrforge::f1ap::messages::F1SetupRequest::F1ApContainersProtocolIeFieldF1ApPduContentsF1SetupRequestIEs_gnb_du_rrc_version>(actual_body.protocol_i_es.elements[2].value).value.latest_rrc_version.bit_count == 3);
REQUIRE(::std::get<::nrforge::f1ap::messages::F1SetupRequest::F1ApContainersProtocolIeFieldF1ApPduContentsF1SetupRequestIEs_gnb_du_rrc_version>(actual_body.protocol_i_es.elements[2].value).value.i_e_extensions.has_value() == false);
auto encoded = encode_f1ap_pdu(expected.value());
REQUIRE(encoded);
REQUIRE(encoded.value().octets.size() == native_wire.size());
REQUIRE(encoded.value().octets == native_wire);
::std::cout << "PASS F1SetupRequest ";
for(auto octet:encoded.value().octets) { const auto v=::std::to_integer<unsigned>(octet); ::std::cout << "0123456789abcdef"[v>>4] << "0123456789abcdef"[v&15]; }
::std::cout << "\n";
}
void run_F1SetupRequest() {
check_F1SetupRequest();
}

static void check_F1SetupResponse() {
using namespace ::nrforge::f1ap;
::nrforge::f1ap::messages::F1SetupResponse::Body body{};
body.protocol_i_es.elements.resize(2);
body.protocol_i_es.elements[0].id = UINT64_C(78);
body.protocol_i_es.elements[0].criticality.value = ::nrforge::f1ap::messages::F1SetupResponse::F1ApCommonDataTypesCriticality::Known::reject;
::nrforge::f1ap::messages::F1SetupResponse::F1ApContainersProtocolIeFieldF1ApPduContentsF1SetupResponseIEs_transaction_id open_1{};
open_1.value = UINT64_C(0);
body.protocol_i_es.elements[0].value = ::std::move(open_1);
body.protocol_i_es.elements[1].id = UINT64_C(170);
body.protocol_i_es.elements[1].criticality.value = ::nrforge::f1ap::messages::F1SetupResponse::F1ApCommonDataTypesCriticality::Known::reject;
::nrforge::f1ap::messages::F1SetupResponse::F1ApContainersProtocolIeFieldF1ApPduContentsF1SetupResponseIEs_gnb_cu_rrc_version open_2{};
open_2.value.latest_rrc_version.octets = ::std::vector<::std::byte>{::std::byte{0x00}};
open_2.value.latest_rrc_version.bit_count = 3;
body.protocol_i_es.elements[1].value = ::std::move(open_2);
auto expected = make_f1ap_pdu(::std::move(body), Criticality::reject);
REQUIRE(expected);
const auto native_wire = ::std::vector<::std::byte>{::std::byte{0x40},::std::byte{0x01},::std::byte{0x00},::std::byte{0x0e},::std::byte{0x00},::std::byte{0x00},::std::byte{0x02},::std::byte{0x00},::std::byte{0x4e},::std::byte{0x00},::std::byte{0x02},::std::byte{0x00},::std::byte{0x00},::std::byte{0x00},::std::byte{0xaa},::std::byte{0x00},::std::byte{0x01},::std::byte{0x00}};
auto decoded = decode_f1ap_pdu(native_wire);
REQUIRE(decoded);
REQUIRE(decoded.value().kind() == PduKind::typed);
const auto* header = decoded.value().root_header();
REQUIRE(header && static_cast<unsigned>(header->role) == 1 && header->procedure_code == 1 && header->received_criticality == Criticality::reject);
const auto* metadata = decoded.value().message_info();
REQUIRE(metadata && metadata->message == "F1SetupResponse" && metadata->module == "F1AP-PDU-Contents");
const auto* actual_body_pointer = decoded.value().body_if<::nrforge::f1ap::messages::F1SetupResponse::Body>();
REQUIRE(actual_body_pointer);
const auto& actual_body = *actual_body_pointer;
REQUIRE(actual_body.sequence_extensions.received_bitmap_bit_count == 0);
REQUIRE(actual_body.sequence_extensions.unknown_additions.empty());
REQUIRE(actual_body.protocol_i_es.elements.size() == 2);
REQUIRE(actual_body.protocol_i_es.elements[0].id == UINT64_C(78));
REQUIRE(actual_body.protocol_i_es.elements[0].criticality.value == ::nrforge::f1ap::messages::F1SetupResponse::F1ApCommonDataTypesCriticality::Known::reject);
REQUIRE(::std::holds_alternative<::nrforge::f1ap::messages::F1SetupResponse::F1ApContainersProtocolIeFieldF1ApPduContentsF1SetupResponseIEs_transaction_id>(actual_body.protocol_i_es.elements[0].value));
REQUIRE(::std::get<::nrforge::f1ap::messages::F1SetupResponse::F1ApContainersProtocolIeFieldF1ApPduContentsF1SetupResponseIEs_transaction_id>(actual_body.protocol_i_es.elements[0].value).value == UINT64_C(0));
REQUIRE(actual_body.protocol_i_es.elements[1].id == UINT64_C(170));
REQUIRE(actual_body.protocol_i_es.elements[1].criticality.value == ::nrforge::f1ap::messages::F1SetupResponse::F1ApCommonDataTypesCriticality::Known::reject);
REQUIRE(::std::holds_alternative<::nrforge::f1ap::messages::F1SetupResponse::F1ApContainersProtocolIeFieldF1ApPduContentsF1SetupResponseIEs_gnb_cu_rrc_version>(actual_body.protocol_i_es.elements[1].value));
REQUIRE(::std::get<::nrforge::f1ap::messages::F1SetupResponse::F1ApContainersProtocolIeFieldF1ApPduContentsF1SetupResponseIEs_gnb_cu_rrc_version>(actual_body.protocol_i_es.elements[1].value).value.latest_rrc_version.octets == ::std::vector<::std::byte>{::std::byte{0x00}});
REQUIRE(::std::get<::nrforge::f1ap::messages::F1SetupResponse::F1ApContainersProtocolIeFieldF1ApPduContentsF1SetupResponseIEs_gnb_cu_rrc_version>(actual_body.protocol_i_es.elements[1].value).value.latest_rrc_version.bit_count == 3);
REQUIRE(::std::get<::nrforge::f1ap::messages::F1SetupResponse::F1ApContainersProtocolIeFieldF1ApPduContentsF1SetupResponseIEs_gnb_cu_rrc_version>(actual_body.protocol_i_es.elements[1].value).value.i_e_extensions.has_value() == false);
auto encoded = encode_f1ap_pdu(expected.value());
REQUIRE(encoded);
REQUIRE(encoded.value().octets.size() == native_wire.size());
REQUIRE(encoded.value().octets == native_wire);
::std::cout << "PASS F1SetupResponse ";
for(auto octet:encoded.value().octets) { const auto v=::std::to_integer<unsigned>(octet); ::std::cout << "0123456789abcdef"[v>>4] << "0123456789abcdef"[v&15]; }
::std::cout << "\n";
}
void run_F1SetupResponse() {
check_F1SetupResponse();
}

static void check_F1SetupFailure() {
using namespace ::nrforge::f1ap;
::nrforge::f1ap::messages::F1SetupFailure::Body body{};
body.protocol_i_es.elements.resize(2);
body.protocol_i_es.elements[0].id = UINT64_C(78);
body.protocol_i_es.elements[0].criticality.value = ::nrforge::f1ap::messages::F1SetupFailure::F1ApCommonDataTypesCriticality::Known::reject;
::nrforge::f1ap::messages::F1SetupFailure::F1ApContainersProtocolIeFieldF1ApPduContentsF1SetupFailureIEs_transaction_id open_1{};
open_1.value = UINT64_C(0);
body.protocol_i_es.elements[0].value = ::std::move(open_1);
body.protocol_i_es.elements[1].id = UINT64_C(0);
body.protocol_i_es.elements[1].criticality.value = ::nrforge::f1ap::messages::F1SetupFailure::F1ApCommonDataTypesCriticality::Known::ignore;
::nrforge::f1ap::messages::F1SetupFailure::F1ApContainersProtocolIeFieldF1ApPduContentsF1SetupFailureIEs_cause open_2{};
::nrforge::f1ap::messages::F1SetupFailure::F1ApIEsCause_radio_network alternative_3{};
alternative_3.value.value = ::nrforge::f1ap::messages::F1SetupFailure::F1ApIEsCauseRadioNetwork::Known::unspecified;
open_2.value = ::std::move(alternative_3);
body.protocol_i_es.elements[1].value = ::std::move(open_2);
auto expected = make_f1ap_pdu(::std::move(body), Criticality::reject);
REQUIRE(expected);
const auto native_wire = ::std::vector<::std::byte>{::std::byte{0x80},::std::byte{0x01},::std::byte{0x00},::std::byte{0x0e},::std::byte{0x00},::std::byte{0x00},::std::byte{0x02},::std::byte{0x00},::std::byte{0x4e},::std::byte{0x00},::std::byte{0x02},::std::byte{0x00},::std::byte{0x00},::std::byte{0x00},::std::byte{0x00},::std::byte{0x40},::std::byte{0x01},::std::byte{0x00}};
auto decoded = decode_f1ap_pdu(native_wire);
REQUIRE(decoded);
REQUIRE(decoded.value().kind() == PduKind::typed);
const auto* header = decoded.value().root_header();
REQUIRE(header && static_cast<unsigned>(header->role) == 2 && header->procedure_code == 1 && header->received_criticality == Criticality::reject);
const auto* metadata = decoded.value().message_info();
REQUIRE(metadata && metadata->message == "F1SetupFailure" && metadata->module == "F1AP-PDU-Contents");
const auto* actual_body_pointer = decoded.value().body_if<::nrforge::f1ap::messages::F1SetupFailure::Body>();
REQUIRE(actual_body_pointer);
const auto& actual_body = *actual_body_pointer;
REQUIRE(actual_body.sequence_extensions.received_bitmap_bit_count == 0);
REQUIRE(actual_body.sequence_extensions.unknown_additions.empty());
REQUIRE(actual_body.protocol_i_es.elements.size() == 2);
REQUIRE(actual_body.protocol_i_es.elements[0].id == UINT64_C(78));
REQUIRE(actual_body.protocol_i_es.elements[0].criticality.value == ::nrforge::f1ap::messages::F1SetupFailure::F1ApCommonDataTypesCriticality::Known::reject);
REQUIRE(::std::holds_alternative<::nrforge::f1ap::messages::F1SetupFailure::F1ApContainersProtocolIeFieldF1ApPduContentsF1SetupFailureIEs_transaction_id>(actual_body.protocol_i_es.elements[0].value));
REQUIRE(::std::get<::nrforge::f1ap::messages::F1SetupFailure::F1ApContainersProtocolIeFieldF1ApPduContentsF1SetupFailureIEs_transaction_id>(actual_body.protocol_i_es.elements[0].value).value == UINT64_C(0));
REQUIRE(actual_body.protocol_i_es.elements[1].id == UINT64_C(0));
REQUIRE(actual_body.protocol_i_es.elements[1].criticality.value == ::nrforge::f1ap::messages::F1SetupFailure::F1ApCommonDataTypesCriticality::Known::ignore);
REQUIRE(::std::holds_alternative<::nrforge::f1ap::messages::F1SetupFailure::F1ApContainersProtocolIeFieldF1ApPduContentsF1SetupFailureIEs_cause>(actual_body.protocol_i_es.elements[1].value));
REQUIRE(::std::holds_alternative<::nrforge::f1ap::messages::F1SetupFailure::F1ApIEsCause_radio_network>(::std::get<::nrforge::f1ap::messages::F1SetupFailure::F1ApContainersProtocolIeFieldF1ApPduContentsF1SetupFailureIEs_cause>(actual_body.protocol_i_es.elements[1].value).value));
REQUIRE(::std::get<::nrforge::f1ap::messages::F1SetupFailure::F1ApIEsCauseRadioNetwork::Known>(::std::get<::nrforge::f1ap::messages::F1SetupFailure::F1ApIEsCause_radio_network>(::std::get<::nrforge::f1ap::messages::F1SetupFailure::F1ApContainersProtocolIeFieldF1ApPduContentsF1SetupFailureIEs_cause>(actual_body.protocol_i_es.elements[1].value).value).value.value) == ::nrforge::f1ap::messages::F1SetupFailure::F1ApIEsCauseRadioNetwork::Known::unspecified);
auto encoded = encode_f1ap_pdu(expected.value());
REQUIRE(encoded);
REQUIRE(encoded.value().octets.size() == native_wire.size());
REQUIRE(encoded.value().octets == native_wire);
::std::cout << "PASS F1SetupFailure ";
for(auto octet:encoded.value().octets) { const auto v=::std::to_integer<unsigned>(octet); ::std::cout << "0123456789abcdef"[v>>4] << "0123456789abcdef"[v&15]; }
::std::cout << "\n";
}
void run_F1SetupFailure() {
check_F1SetupFailure();
}

int main() {
const auto& library = ::nrforge::f1ap::sdk_identity();
const auto& headers = ::nrforge::f1ap::header_sdk_identity;
REQUIRE(library.version == headers.version && library.fingerprint == headers.fingerprint);
REQUIRE(library.schema_sha256 == headers.schema_sha256 && library.runtime_sha256 == headers.runtime_sha256);
const auto& registry = ::nrforge::f1ap::f1ap_registry_state();
REQUIRE(registry && registry.value().message_count() == 158);
run_F1SetupRequest(); run_F1SetupResponse(); run_F1SetupFailure();
}
