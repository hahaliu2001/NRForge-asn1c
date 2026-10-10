#ifndef NRFORGE_N11_ENVELOPE_ADAPTER_HPP
#define NRFORGE_N11_ENVELOPE_ADAPTER_HPP
#include "../n10-body-qualification/body_adapter.hpp"
#include "envelope_types.hpp"
#include "envelope_mapping.hpp"
#include "envelope_codec.hpp"
#include <stdexcept>

// Qualification value integration only. All wire operations are generated
// from the owned envelope descriptor and the actual, unchanged body graph.
namespace n11_integration {
namespace generated = ::n10::body;
using Pdu = generated::NgapPduDescriptionsNgapPdu;
using Mapping = generated::NgapPduDescriptionsNgapPdu_aper;
using Body = ::n10_integration::Body;
using Bytes = ::std::vector<::std::byte>;
using Criticality = Mapping::criticality_type;
template<::std::size_t> struct Root;
#define N11_ROOT_TRAIT(N) \
template<> struct Root<N> { \
    using type = Mapping::wrapper_##N; \
    static constexpr auto procedure = Mapping::root_##N##_procedure_member; \
    static constexpr auto criticality = Mapping::root_##N##_criticality_member; \
    static constexpr auto value = Mapping::root_##N##_value_member; \
};
N11_ROOT_TRAIT(0)
N11_ROOT_TRAIT(1)
N11_ROOT_TRAIT(2)
#undef N11_ROOT_TRAIT
consteval ::std::size_t ordinal_for_role(unsigned role) {
    for(::std::size_t i = 0; i < Mapping::root_roles.size(); ++i)
        if(Mapping::root_roles[i] == role) return i;
    return ::std::numeric_limits<::std::size_t>::max();
}
inline constexpr auto initiating_ordinal = ordinal_for_role(0);
inline constexpr auto successful_ordinal = ordinal_for_role(1);
inline constexpr auto unsuccessful_ordinal = ordinal_for_role(2);
static_assert(initiating_ordinal < 3 && successful_ordinal < 3 && unsuccessful_ordinal < 3);
static_assert(Mapping::target_root_ordinal == initiating_ordinal);
#ifndef N11_TARGET_CODE_EXPECTED
#define N11_TARGET_CODE_EXPECTED 41
#endif
static_assert(Mapping::target_code == N11_TARGET_CODE_EXPECTED); // Independent fixture expectation.
static_assert(::std::is_same_v<decltype(Mapping::target_wrapper_type::value),Body>);
static_assert([] {
    for(::std::size_t i = 0; i < 3; ++i) {
        if(Mapping::per_root_index_to_source_ordinal[Mapping::source_ordinal_to_per_root_index[i]] != i) return false;
        if(Mapping::source_ordinal_to_per_root_index[Mapping::per_root_index_to_source_ordinal[i]] != i) return false;
    }
    return true;
}());
using Initiating = Root<initiating_ordinal>;
using Successful = Root<successful_ordinal>;
using Unsuccessful = Root<unsuccessful_ordinal>;
inline Criticality received(::n10_integration::Criticality value) {
    return Criticality{static_cast<Criticality::Known>(static_cast<unsigned>(value.value))};
}
inline Pdu make_typed(::std::uint64_t code,::n10_integration::Criticality policy,Body body) {
    typename Initiating::type root{};
    root.*Initiating::procedure = code; root.*Initiating::criticality = received(policy);
    root.*Initiating::value = Mapping::target_wrapper_type{::std::move(body)};
    return Pdu{::std::move(root)};
}
template<class R> inline Pdu make_opaque_root(::std::uint64_t code,::n10_integration::Criticality policy,Bytes bytes) {
    typename R::type root{};
    root.*R::procedure = code; root.*R::criticality = received(policy);
    root.*R::value = Mapping::opaque_type{::std::move(bytes)};
    return Pdu{::std::move(root)};
}
inline Pdu make_opaque(char role,::std::uint64_t code,::n10_integration::Criticality policy,Bytes bytes) {
    if(role == 'i') return make_opaque_root<Initiating>(code,policy,::std::move(bytes));
    if(role == 's') return make_opaque_root<Successful>(code,policy,::std::move(bytes));
    if(role == 'u') return make_opaque_root<Unsuccessful>(code,policy,::std::move(bytes));
    throw ::std::invalid_argument("root role");
}
inline Pdu make_extension(::std::uint64_t index,Bytes bytes) {
    return Pdu{Mapping::unknown_extension_type{index,::std::move(bytes)}};
}
inline char kind(const Pdu& pdu) {
    return ::std::visit([](const auto& root) {
        using T = ::std::remove_cvref_t<decltype(root)>;
        if constexpr(::std::is_same_v<T,typename Initiating::type>) return 'i';
        else if constexpr(::std::is_same_v<T,typename Successful::type>) return 's';
        else if constexpr(::std::is_same_v<T,typename Unsuccessful::type>) return 'u';
        else return 'x';
    },pdu.value);
}
inline ::std::uint64_t procedure_code(const Pdu& pdu) {
    return ::std::visit([](const auto& root) -> ::std::uint64_t {
        using T = ::std::remove_cvref_t<decltype(root)>;
        if constexpr(::std::is_same_v<T,typename Initiating::type>) return root.*Initiating::procedure;
        else if constexpr(::std::is_same_v<T,typename Successful::type>) return root.*Successful::procedure;
        else if constexpr(::std::is_same_v<T,typename Unsuccessful::type>) return root.*Unsuccessful::procedure;
        else throw ::std::logic_error("extension has no procedure code");
    },pdu.value);
}
inline const Criticality& received_criticality(const Pdu& pdu) {
    return ::std::visit([](const auto& root) -> const Criticality& {
        using T = ::std::remove_cvref_t<decltype(root)>;
        if constexpr(::std::is_same_v<T,typename Initiating::type>) return root.*Initiating::criticality;
        else if constexpr(::std::is_same_v<T,typename Successful::type>) return root.*Successful::criticality;
        else if constexpr(::std::is_same_v<T,typename Unsuccessful::type>) return root.*Unsuccessful::criticality;
        else throw ::std::logic_error("extension has no criticality");
    },pdu.value);
}
inline Body* mutable_typed_body(Pdu& pdu) {
    auto* root = ::std::get_if<typename Initiating::type>(&pdu.value);
    if(!root) return nullptr;
    auto* selected = ::std::get_if<Mapping::target_wrapper_type>(&(root->*Initiating::value));
    return selected ? &selected->value : nullptr;
}
inline const Body* typed_body(const Pdu& pdu) {
    const auto* root = ::std::get_if<typename Initiating::type>(&pdu.value);
    if(!root) return nullptr;
    const auto* selected = ::std::get_if<Mapping::target_wrapper_type>(&(root->*Initiating::value));
    return selected ? &selected->value : nullptr;
}
inline ::std::uint64_t extension_index(const Pdu& pdu) {
    return ::std::get<Mapping::unknown_extension_type>(pdu.value).index;
}
inline const Bytes& opaque_payload(const Pdu& pdu) {
    return ::std::visit([](const auto& root) -> const Bytes& {
        using T = ::std::remove_cvref_t<decltype(root)>;
        if constexpr(::std::is_same_v<T,typename Initiating::type>) return ::std::get<Mapping::opaque_type>(root.*Initiating::value).payload;
        else if constexpr(::std::is_same_v<T,typename Successful::type>) return (root.*Successful::value).payload;
        else if constexpr(::std::is_same_v<T,typename Unsuccessful::type>) return (root.*Unsuccessful::value).payload;
        else return root.payload;
    },pdu.value);
}
inline bool damage_opaque(Pdu& pdu) {
    return ::std::visit([](auto& root) {
        using T = ::std::remove_cvref_t<decltype(root)>;
        Bytes* payload;
        if constexpr(::std::is_same_v<T,typename Initiating::type>) {
            auto* selected = ::std::get_if<Mapping::opaque_type>(&(root.*Initiating::value));
            if(!selected) return false;
            payload = &selected->payload;
        } else if constexpr(::std::is_same_v<T,typename Successful::type>) payload = &(root.*Successful::value).payload;
        else if constexpr(::std::is_same_v<T,typename Unsuccessful::type>) payload = &(root.*Unsuccessful::value).payload;
        else payload = &root.payload;
        if(payload->empty()) return false;
        payload->front() ^= ::std::byte{0xff}; return true;
    },pdu.value);
}
inline auto encode(const Pdu& pdu,const ::nrforge::aper::Limits& limits = {}) {
    return generated::encode_ngap_pdu_descriptions_ngap_pdu(pdu,limits);
}
inline auto decode(::std::span<const ::std::byte> input,const ::nrforge::aper::Limits& limits = {}) {
    return generated::decode_ngap_pdu_descriptions_ngap_pdu(input,limits);
}
inline auto put(::nrforge::aper::FieldWriter& fields,const Pdu& pdu) {
    return generated::envelope_codec::put_NgapPduDescriptionsNgapPdu(fields,pdu);
}
inline auto get(::nrforge::aper::FieldReader& fields) {
    return generated::envelope_codec::get_NgapPduDescriptionsNgapPdu(fields);
}
} // namespace n11_integration
#endif
