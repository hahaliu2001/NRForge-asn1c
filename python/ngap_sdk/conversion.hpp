#ifndef NRFORGE_PYTHON_CONVERSION_HPP
#define NRFORGE_PYTHON_CONVERSION_HPP
#include <pybind11/pybind11.h>
#include <ngap.hpp>
#include <sequence_extensions.hpp>
#include <algorithm>
#include <limits>
#include <string>
#include <type_traits>
namespace nrforge::python_sdk {
namespace py = pybind11;
using Pdu = ::nrforge::ngap::Pdu;
using Criticality = ::nrforge::ngap::Criticality;
inline const char* error_name(::nrforge::aper::ErrorCode code) {
    using E=::nrforge::aper::ErrorCode;
    switch(code) {
#define ERROR_NAME(x) case E::x: return #x;
    ERROR_NAME(invalid_argument) ERROR_NAME(constraint_violation) ERROR_NAME(truncated_input)
    ERROR_NAME(nonzero_padding) ERROR_NAME(trailing_data) ERROR_NAME(resource_limit)
    ERROR_NAME(allocation_failure) ERROR_NAME(invalid_state)
#undef ERROR_NAME
    }
    return "invalid_state";
}
[[noreturn]] inline void codec_error(::nrforge::aper::Error error) {
    auto cls=py::module_::import("nrforge_ngap").attr("CodecError");
    auto value=cls(error_name(error.code),py::int_(error.bit_offset));
    PyErr_SetObject(cls.ptr(),value.ptr());
    throw py::error_already_set();
}
template<class T> T unwrap(::nrforge::aper::Result<T> value) {
    if(!value) codec_error(value.error());
    return ::std::move(value).value();
}
inline py::dict require_dict(py::handle value) {
    if(!PyDict_CheckExact(value.ptr())) throw py::type_error("expected dict");
    return py::reinterpret_borrow<py::dict>(value);
}
inline void check_keys(const py::dict& d,::std::initializer_list<const char*> keys) {
    if(static_cast<::std::size_t>(py::len(d))>keys.size()) throw py::value_error("unknown fields");
    for(auto item:d) {
        if(!PyUnicode_CheckExact(item.first.ptr())) throw py::type_error("dict keys must be str");
        const bool known=::std::any_of(keys.begin(),keys.end(),[&](const char* k){return PyUnicode_CompareWithASCIIString(item.first.ptr(),k)==0;});
        if(!known) throw py::value_error("unknown field");
    }
}
inline py::object required(const py::dict& d,const char* key) {
    if(!d.contains(key)) throw py::value_error(::std::string("missing field: ")+key);
    return py::reinterpret_borrow<py::object>(d[key]);
}
inline py::object optional_value(const py::dict& d,const char* key) {
    return d.contains(key)?py::reinterpret_borrow<py::object>(d[key]):py::none();
}
struct ConversionLimits {
    ::std::size_t max_depth=256;
    ::std::size_t max_nodes=1000000;
    ::std::size_t max_bytes=16777216;
    ::std::size_t max_collection_elements=65536;
};
struct ConversionBudget {
    ConversionLimits limits;
    ::std::size_t depth=0,nodes=0,bytes=0,elements=0;
    struct Scope {
        ConversionBudget* owner;
        Scope(const Scope&)=delete;
        Scope& operator=(const Scope&)=delete;
        explicit Scope(ConversionBudget* b):owner(b) {}
        ~Scope(){--owner->depth;}
    };
    [[noreturn]] static void exceeded(){throw py::value_error("Python conversion resource limit exceeded");}
    Scope enter() {
        if(depth>=limits.max_depth || nodes>=limits.max_nodes) exceeded();
        ++depth;++nodes;return Scope(this);
    }
    void octets(::std::size_t n) {
        if(n>limits.max_bytes-bytes) exceeded();
        bytes+=n;
    }
    void items(::std::size_t n) {
        if(n>limits.max_collection_elements-elements) exceeded();
        elements+=n;
    }
};
inline ::std::string read_label(py::handle h,ConversionBudget& b) {
    if(!PyUnicode_Check(h.ptr()))throw py::type_error("expected str label");
    if(PyUnicode_GET_LENGTH(h.ptr())>256)throw py::value_error("label exceeds 256 characters");
    Py_ssize_t n=0;const char* p=PyUnicode_AsUTF8AndSize(h.ptr(),&n);
    if(!p)throw py::error_already_set();
    b.octets(static_cast<::std::size_t>(n));
    return ::std::string(p,static_cast<::std::size_t>(n));
}
inline py::object named_label(const char* name,ConversionBudget& b) {
    b.octets(::std::char_traits<char>::length(name));return py::str(name);
}
template<class T,class Enable=void> struct Convert;
template<class T> struct TypeName;
template<class T> struct Convert<T,::std::enable_if_t<::std::is_integral_v<T> && !::std::is_same_v<T,bool>>> {
    static py::object to(T v,ConversionBudget& b) {auto scope=b.enter();return py::int_(v);}
    static T from(py::handle h,ConversionBudget& b) {
        auto scope=b.enter();
        if(!PyLong_Check(h.ptr()) || PyBool_Check(h.ptr())) throw py::type_error("expected integer (not bool)");
        if constexpr(::std::is_unsigned_v<T>) {
            auto n=PyLong_AsUnsignedLongLong(h.ptr());if(PyErr_Occurred()) throw py::error_already_set();
            if(n>(::std::numeric_limits<T>::max)()) throw py::value_error("integer exceeds storage range");
            return static_cast<T>(n);
        } else {
            auto n=PyLong_AsLongLong(h.ptr());if(PyErr_Occurred()) throw py::error_already_set();
            if(n<(::std::numeric_limits<T>::min)() || n>(::std::numeric_limits<T>::max)()) throw py::value_error("integer exceeds storage range");
            return static_cast<T>(n);
        }
    }
};
template<> struct Convert<bool> {
    static py::object to(bool v,ConversionBudget& b){auto scope=b.enter();return py::bool_(v);}
    static bool from(py::handle h,ConversionBudget& b){auto scope=b.enter();if(!PyBool_Check(h.ptr()))throw py::type_error("expected bool");return h.ptr()==Py_True;}
};
template<> struct Convert<::std::string> {
    static py::object to(const ::std::string& v,ConversionBudget& b){auto scope=b.enter();b.octets(v.size());return py::str(v);}
    static ::std::string from(py::handle h,ConversionBudget& b){auto scope=b.enter();if(!PyUnicode_Check(h.ptr()))throw py::type_error("expected str");if(static_cast<::std::size_t>(PyUnicode_GET_LENGTH(h.ptr()))>b.limits.max_bytes-b.bytes)ConversionBudget::exceeded();Py_ssize_t n=0;const char* p=PyUnicode_AsUTF8AndSize(h.ptr(),&n);if(!p)throw py::error_already_set();b.octets(static_cast<::std::size_t>(n));return ::std::string(p,static_cast<::std::size_t>(n));}
};
template<> struct Convert<::std::vector<::std::byte>> {
    static py::object to(const ::std::vector<::std::byte>& v,ConversionBudget& b){auto scope=b.enter();b.octets(v.size());return py::bytes(reinterpret_cast<const char*>(v.data()),v.size());}
    static ::std::vector<::std::byte> from(py::handle h,ConversionBudget& b){auto scope=b.enter();if(!PyBytes_Check(h.ptr()))throw py::type_error("expected bytes");const auto n=static_cast<::std::size_t>(PyBytes_GET_SIZE(h.ptr()));b.octets(n);const auto* p=reinterpret_cast<const ::std::byte*>(PyBytes_AS_STRING(h.ptr()));return {p,p+n};}
};
template<> struct Convert<::std::monostate> {
    static py::object to(const ::std::monostate&,ConversionBudget& b){auto scope=b.enter();return py::none();}
    static ::std::monostate from(py::handle h,ConversionBudget& b){auto scope=b.enter();if(!h.is_none())throw py::type_error("NULL expects None");return {};}
};
template<class T> struct Convert<::std::optional<T>> {
    static py::object to(const ::std::optional<T>& v,ConversionBudget& b){auto scope=b.enter();return v?Convert<T>::to(*v,b):py::none();}
    static ::std::optional<T> from(py::handle h,ConversionBudget& b){auto scope=b.enter();return h.is_none()?::std::optional<T>{}:Convert<T>::from(h,b);}
};
template<class T> struct Convert<::std::vector<T>> {
    static py::object to(const ::std::vector<T>& v,ConversionBudget& b){auto scope=b.enter();b.items(v.size());py::list out;for(const auto& item:v)out.append(Convert<T>::to(item,b));return out;}
    static ::std::vector<T> from(py::handle h,ConversionBudget& b){auto scope=b.enter();if(!PyList_CheckExact(h.ptr()))throw py::type_error("expected list");const auto n=static_cast<::std::size_t>(PyList_GET_SIZE(h.ptr()));b.items(n);::std::vector<T> out;out.reserve(n);for(::std::size_t j=0;j<n;++j){if(static_cast<::std::size_t>(PyList_GET_SIZE(h.ptr()))!=n)throw py::value_error("list changed during conversion");auto item=py::reinterpret_borrow<py::object>(PyList_GET_ITEM(h.ptr(),static_cast<Py_ssize_t>(j)));out.push_back(Convert<T>::from(item,b));}return out;}
};
template<class... T> struct Convert<::std::variant<T...>> {
    using V=::std::variant<T...>;
    static py::object to(const V& v,ConversionBudget& b){auto scope=b.enter();if(v.valueless_by_exception())throw py::value_error("valueless variant");return ::std::visit([&](const auto& item)->py::object {using A=::std::decay_t<decltype(item)>;py::dict d;d["type"]=named_label(TypeName<A>::name(),b);d["value"]=Convert<A>::to(item,b);return d;},v);}
    template<::std::size_t I=0> static V select(const ::std::string& tag,py::handle value,ConversionBudget& b) {
        if constexpr(I==sizeof...(T)) {throw py::value_error("unknown variant type: "+tag);} else {
            using A=::std::variant_alternative_t<I,V>;
            if(tag==TypeName<A>::name())return V(::std::in_place_index<I>,Convert<A>::from(value,b));
            return select<I+1>(tag,value,b);
        }
    }
    static V from(py::handle h,ConversionBudget& b){auto scope=b.enter();auto d=require_dict(h);check_keys(d,{"type","value"});auto type=required(d,"type");if(!PyUnicode_Check(type.ptr()))throw py::type_error("variant type must be str");return select(read_label(type,b),required(d,"value"),b);}
};
template<> struct Convert<::nrforge::aper::BitString> {
    using T=::nrforge::aper::BitString;
    static py::object to(const T& v,ConversionBudget& b){auto scope=b.enter();py::dict d;d["octets"]=Convert<::std::vector<::std::byte>>::to(v.octets,b);d["bit_count"]=Convert<::std::size_t>::to(v.bit_count,b);return d;}
    static T from(py::handle h,ConversionBudget& b){auto scope=b.enter();auto d=require_dict(h);check_keys(d,{"octets","bit_count"});return {Convert<::std::vector<::std::byte>>::from(required(d,"octets"),b),Convert<::std::size_t>::from(required(d,"bit_count"),b)};}
};
template<> struct Convert<::nrforge::aper::UnknownSequenceAddition> {
    using T=::nrforge::aper::UnknownSequenceAddition;
    static py::object to(const T& v,ConversionBudget& b){auto scope=b.enter();py::dict d;d["addition_index"]=Convert<::std::uint64_t>::to(v.addition_index,b);d["payload_octets"]=Convert<::std::vector<::std::byte>>::to(v.payload_octets,b);return d;}
    static T from(py::handle h,ConversionBudget& b){auto scope=b.enter();auto d=require_dict(h);check_keys(d,{"addition_index","payload_octets"});return {Convert<::std::uint64_t>::from(required(d,"addition_index"),b),Convert<::std::vector<::std::byte>>::from(required(d,"payload_octets"),b)};}
};
template<> struct Convert<::nrforge::aper::SequenceExtensionData> {
    using T=::nrforge::aper::SequenceExtensionData;
    static py::object to(const T& v,ConversionBudget& b){auto scope=b.enter();py::dict d;d["received_bitmap_bit_count"]=Convert<::std::size_t>::to(v.received_bitmap_bit_count,b);d["unknown_additions"]=Convert<::std::vector<::nrforge::aper::UnknownSequenceAddition>>::to(v.unknown_additions,b);return d;}
    static T from(py::handle h,ConversionBudget& b){auto scope=b.enter();auto d=require_dict(h);check_keys(d,{"received_bitmap_bit_count","unknown_additions"});return {Convert<::std::size_t>::from(required(d,"received_bitmap_bit_count"),b),Convert<::std::vector<::nrforge::aper::UnknownSequenceAddition>>::from(required(d,"unknown_additions"),b)};}
};
} // namespace nrforge::python_sdk
#endif
