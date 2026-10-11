#include "bindings.hpp"
namespace nrforge::rrc_python_sdk {
static ::nrforge::uper::Limits read_limits(py::handle h) {
 ::nrforge::uper::Limits out;if(h.is_none())return out;auto d=require_dict(h);
 check_keys(d,{"max_input_octets","max_output_octets","max_wire_bits","max_value_octets","max_collection_elements","max_bitmap_bits","max_unknown_octets","max_unknown_records","max_depth","max_work_bits","max_open_staging_octets"});
 ConversionBudget b{};
#define READ_LIMIT(x) if(d.contains(#x))out.x=Convert<::std::size_t>::from(d[#x],b);
 READ_LIMIT(max_input_octets) READ_LIMIT(max_output_octets) READ_LIMIT(max_wire_bits)
 READ_LIMIT(max_value_octets) READ_LIMIT(max_collection_elements) READ_LIMIT(max_bitmap_bits)
 READ_LIMIT(max_unknown_octets) READ_LIMIT(max_unknown_records) READ_LIMIT(max_depth)
 READ_LIMIT(max_work_bits) READ_LIMIT(max_open_staging_octets)
#undef READ_LIMIT
 return out;
}
static ConversionBudget read_conversion_limits(py::handle h) {
 ConversionBudget out{};if(h.is_none())return out;auto d=require_dict(h);
 check_keys(d,{"max_depth","max_nodes","max_bytes","max_collection_elements"});ConversionBudget b{};
#define READ_LIMIT(x) if(d.contains(#x))out.limits.x=Convert<::std::size_t>::from(d[#x],b);
 READ_LIMIT(max_depth) READ_LIMIT(max_nodes) READ_LIMIT(max_bytes) READ_LIMIT(max_collection_elements)
#undef READ_LIMIT
 return out;
}
static ::nrforge::uper::Completion read_completion(py::handle h) {
 ConversionBudget b{};auto name=read_label(h,b);
 if(name=="octets")return ::nrforge::uper::Completion::octets;
 if(name=="contained_bits")return ::nrforge::uper::Completion::contained_bits;
 if(name=="exact")return ::nrforge::uper::Completion::exact;
 throw py::value_error("unknown completion");
}
static const Binding& lookup(py::handle module,py::handle root,ConversionBudget& b) {
 auto m=read_label(module,b);auto r=read_label(root,b);
 for(const auto& entry:bindings())if(m==entry.module&&r==entry.root)return entry;
 throw py::value_error("unsupported RRC root: "+m+"."+r);
}
static py::dict identity() {
 const auto& i=::nrforge::rrc::sdk_identity();py::dict d;
 d["version"]=::std::string(i.version);d["fingerprint"]=::std::string(i.fingerprint);
 d["schema_sha256"]=::std::string(i.schema_sha256);d["runtime_sha256"]=::std::string(i.runtime_sha256);
 d["source_revision"]=::std::string(i.source_revision);return d;
}
static py::list roots() {
 py::list out;for(const auto& e:bindings()){py::dict d;d["module"]=e.module;d["root"]=e.root;out.append(d);}return out;
}
static py::dict encode(py::handle module,py::handle root,py::handle value,py::handle completion,py::handle limits,py::handle conversion_limits) {
 auto l=read_limits(limits);auto c=read_completion(completion);auto b=read_conversion_limits(conversion_limits);
 return lookup(module,root,b).encode(value,l,c,b);
}
static py::dict decode(py::handle module,py::handle root,py::handle data,py::handle logical_bits,py::handle completion,py::handle limits,py::handle conversion_limits) {
 auto l=read_limits(limits);auto c=read_completion(completion);auto b=read_conversion_limits(conversion_limits);
 const auto& entry=lookup(module,root,b);
 if(!PyBytes_Check(data.ptr()))throw py::type_error("data must be bytes");
 if(static_cast<::std::size_t>(PyBytes_GET_SIZE(data.ptr()))>l.max_input_octets)codec_error({::nrforge::uper::ErrorCode::resource_limit,0});
 auto bytes=Convert<::std::vector<::std::byte>>::from(data,b);
 auto n=Convert<::std::optional<::std::size_t>>::from(logical_bits,b);
 return entry.decode(bytes,l,n,c,b);
}
}
PYBIND11_MODULE(_native,m) {
 using namespace nrforge::rrc_python_sdk;
 const auto& actual=::nrforge::rrc::sdk_identity();
 if(actual.fingerprint!=::nrforge::rrc::header_sdk_identity.fingerprint||bindings().size()!=64)
  throw py::import_error("RRC SDK header/archive identity mismatch");
 m.def("identity",&identity);m.def("roots",&roots);
 m.def("encode",&encode,py::arg("module"),py::arg("root"),py::arg("value"),py::kw_only(),
  py::arg("completion")="octets",py::arg("limits")=py::none(),py::arg("conversion_limits")=py::none());
 m.def("decode",&decode,py::arg("module"),py::arg("root"),py::arg("data"),py::kw_only(),
  py::arg("logical_bits")=py::none(),py::arg("completion")="octets",py::arg("limits")=py::none(),py::arg("conversion_limits")=py::none());
}
