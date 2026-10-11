#include "bindings.hpp"
#ifdef NRFORGE_PYTHON_F1AP
#define NRFORGE_PYTHON_REGISTRY f1ap_registry_state
#define NRFORGE_PYTHON_ENCODE encode_f1ap_pdu
#define NRFORGE_PYTHON_DECODE decode_f1ap_pdu
#define NRFORGE_PYTHON_MESSAGE_COUNT 158
#else
#define NRFORGE_PYTHON_REGISTRY ngap_registry_state
#define NRFORGE_PYTHON_ENCODE encode_ngap_pdu
#define NRFORGE_PYTHON_DECODE decode_ngap_pdu
#define NRFORGE_PYTHON_MESSAGE_COUNT 131
#endif
namespace nrforge::python_sdk {
using namespace ::nrforge::NRFORGE_PYTHON_PROTOCOL;
static const char* role_name(Role role){switch(role){case Role::initiating:return "initiatingMessage";case Role::successful:return "successfulOutcome";case Role::unsuccessful:return "unsuccessfulOutcome";}throw py::value_error("invalid role");}
static const char* criticality_name(Criticality c){switch(c){case Criticality::reject:return "reject";case Criticality::ignore:return "ignore";case Criticality::notify:return "notify";}throw py::value_error("invalid criticality");}
static ::std::optional<Criticality> read_criticality(py::handle h){if(h.is_none())return {};if(!PyUnicode_Check(h.ptr()))throw py::type_error("criticality must be str or None");if(PyUnicode_GET_LENGTH(h.ptr())>6)throw py::value_error("unknown criticality");auto s=py::cast<::std::string>(h);if(s=="reject")return Criticality::reject;if(s=="ignore")return Criticality::ignore;if(s=="notify")return Criticality::notify;throw py::value_error("unknown criticality");}
static ::nrforge::aper::Limits read_limits(py::handle h) {
    ::nrforge::aper::Limits out;if(h.is_none())return out;auto d=require_dict(h);
    check_keys(d,{"max_input_octets","max_output_octets","max_wire_bits","max_extension_bitmap_bits","max_retained_unknown_payload_octets","max_retained_unknown_records","max_collection_elements","max_known_open_staging_octets","max_known_open_depth"});
    ConversionBudget budget{};
#define READ_LIMIT(x) if(d.contains(#x))out.x=Convert<::std::size_t>::from(d[#x],budget);
    READ_LIMIT(max_input_octets) READ_LIMIT(max_output_octets) READ_LIMIT(max_wire_bits)
    READ_LIMIT(max_extension_bitmap_bits) READ_LIMIT(max_retained_unknown_payload_octets)
    READ_LIMIT(max_retained_unknown_records) READ_LIMIT(max_collection_elements)
    READ_LIMIT(max_known_open_staging_octets) READ_LIMIT(max_known_open_depth)
#undef READ_LIMIT
    return out;
}
static ConversionBudget read_conversion_limits(py::handle h) {
    ConversionBudget out{};if(h.is_none())return out;auto d=require_dict(h);check_keys(d,{"max_depth","max_nodes","max_bytes","max_collection_elements"});ConversionBudget parsing{};
#define READ_CONVERSION_LIMIT(x) if(d.contains(#x))out.limits.x=Convert<::std::size_t>::from(d[#x],parsing);
    READ_CONVERSION_LIMIT(max_depth) READ_CONVERSION_LIMIT(max_nodes) READ_CONVERSION_LIMIT(max_bytes) READ_CONVERSION_LIMIT(max_collection_elements)
#undef READ_CONVERSION_LIMIT
    return out;
}
static py::dict identity(){const auto& i=sdk_identity();py::dict d;d["version"]=::std::string(i.version);d["fingerprint"]=::std::string(i.fingerprint);d["schema_sha256"]=::std::string(i.schema_sha256);d["runtime_sha256"]=::std::string(i.runtime_sha256);d["source_revision"]=::std::string(i.source_revision);return d;}
static py::list messages(){const auto& state=NRFORGE_PYTHON_REGISTRY();if(!state)codec_error(state.error());py::list out;for(::std::size_t j=0;j<state.value().message_count();++j){const auto& i=*state.value().message_info(j);py::dict d;d["message"]=i.message;d["role"]=role_name(i.role);d["procedure_code"]=i.procedure_code;d["declared_criticality"]=criticality_name(i.declared_criticality);out.append(d);}return out;}
static py::bytes encode(py::handle message,py::handle body,py::handle criticality,py::handle limits,py::handle conversion_limits) {
    auto l=read_limits(limits);auto budget=read_conversion_limits(conversion_limits);
    auto pdu=from_body(read_label(message,budget),body,read_criticality(criticality),budget);
    auto encoded=[&](){py::gil_scoped_release release;return NRFORGE_PYTHON_ENCODE(pdu,l);}();
    auto value=unwrap(::std::move(encoded));
    return py::reinterpret_borrow<py::bytes>(Convert<::std::vector<::std::byte>>::to(value.octets,budget));
}
static py::dict decode(py::handle data,py::handle limits,py::handle conversion_limits) {
    auto l=read_limits(limits);auto budget=read_conversion_limits(conversion_limits);
    if(!PyBytes_Check(data.ptr()))throw py::type_error("data must be bytes");
    if(static_cast<::std::size_t>(PyBytes_GET_SIZE(data.ptr()))>l.max_input_octets)codec_error({::nrforge::aper::ErrorCode::resource_limit,0});
    auto bytes=Convert<::std::vector<::std::byte>>::from(data,budget);
    auto decoded=[&](){py::gil_scoped_release release;return NRFORGE_PYTHON_DECODE(bytes,l);}();auto pdu=unwrap(::std::move(decoded));py::dict out;
    if(const auto* h=pdu.root_header()){out["role"]=role_name(h->role);out["procedure_code"]=h->procedure_code;out["criticality"]=criticality_name(h->received_criticality);}
    switch(pdu.kind()){
    case PduKind::typed:out["kind"]="typed";out["message"]=pdu.message_info()->message;out["body"]=to_body(pdu,budget);break;
    case PduKind::opaque_root:out["kind"]="opaque_root";out["payload"]=Convert<::std::vector<::std::byte>>::to(pdu.opaque_root()->payload,budget);break;
    case PduKind::unknown_extension:out["kind"]="unknown_extension";out["extension_index"]=pdu.unknown_extension()->index;out["payload"]=Convert<::std::vector<::std::byte>>::to(pdu.unknown_extension()->payload,budget);break;
    case PduKind::invalid:codec_error({::nrforge::aper::ErrorCode::invalid_state,0});
    }
    return out;
}
}
PYBIND11_MODULE(_native,m) {
    using namespace nrforge::python_sdk;
    const auto& actual=::nrforge::NRFORGE_PYTHON_PROTOCOL::sdk_identity();const auto& header=::nrforge::NRFORGE_PYTHON_PROTOCOL::header_sdk_identity;
    if(actual.fingerprint!=header.fingerprint)throw py::import_error("Protocol SDK header/library identity mismatch");
    const auto& registry=::nrforge::NRFORGE_PYTHON_PROTOCOL::NRFORGE_PYTHON_REGISTRY();if(!registry || registry.value().message_count()!=NRFORGE_PYTHON_MESSAGE_COUNT)throw py::import_error("Protocol registry is not complete");
    m.def("identity",&identity);m.def("messages",&messages);
    m.def("encode",&encode,py::arg("message"),py::arg("body"),py::kw_only(),py::arg("criticality")=py::none(),py::arg("limits")=py::none(),py::arg("conversion_limits")=py::none());
    m.def("decode",&decode,py::arg("data"),py::kw_only(),py::arg("limits")=py::none(),py::arg("conversion_limits")=py::none());
    m.def("schema",[](py::handle value)->py::object{ConversionBudget budget{};auto message=read_label(value,budget);auto schemas=message_schema();if(!schemas.contains(py::str(message)))throw py::value_error("unknown message: "+message);return py::reinterpret_borrow<py::object>(schemas[py::str(message)]);});
}
