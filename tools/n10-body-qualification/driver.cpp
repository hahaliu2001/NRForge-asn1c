#include "body_adapter.hpp"
#include <algorithm>
#include <charconv>
#include <cstdlib>
#include <iostream>
#include <new>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>

// Active focused allocation-failure injection; disabled for normal native CLI.
static long allocation_countdown = -1;
void* operator new(::std::size_t size) {
    if(allocation_countdown >= 0) {
        if(!allocation_countdown) { allocation_countdown = -1; throw ::std::bad_alloc(); }
        --allocation_countdown;
    }
    if(void* p = ::std::malloc(size ? size : 1)) return p;
    throw ::std::bad_alloc();
}
void* operator new[](::std::size_t size) { return ::operator new(size); }
void operator delete(void* p) noexcept { ::std::free(p); }
void operator delete[](void* p) noexcept { ::std::free(p); }
void operator delete(void* p, ::std::size_t) noexcept { ::std::free(p); }
void operator delete[](void* p, ::std::size_t) noexcept { ::std::free(p); }
#define REQUIRE(x) do { if(!(x)) { ::std::cerr << __LINE__ << ": " << #x << '\n'; ::std::abort(); } } while(0)
namespace a = n10_integration;
namespace r = ::nrforge::aper;
using Bytes = ::std::vector<::std::byte>;

static ::std::vector<::std::string> split(::std::string_view text, char separator) {
    ::std::vector<::std::string> result;
    for(::std::size_t start = 0;;) {
        auto end = text.find(separator, start);
        if(end == text.npos) { result.emplace_back(text.substr(start)); return result; }
        result.emplace_back(text.substr(start, end - start)); start = end + 1;
    }
}
template<class T> static T number(::std::string_view text) {
    T result{};
    auto parsed = ::std::from_chars(text.data(), text.data() + text.size(), result);
    if(parsed.ec != ::std::errc{} || parsed.ptr != text.data() + text.size()) throw ::std::invalid_argument("invalid number");
    return result;
}
static unsigned nibble(char c) {
    if(c >= '0' && c <= '9') return static_cast<unsigned>(c - '0');
    if(c >= 'a' && c <= 'f') return static_cast<unsigned>(c - 'a') + 10u;
    if(c >= 'A' && c <= 'F') return static_cast<unsigned>(c - 'A') + 10u;
    throw ::std::invalid_argument("invalid hex");
}
static Bytes unhex(::std::string_view text) {
    if(text.size() % 2) throw ::std::invalid_argument("odd hex");
    Bytes result; result.reserve(text.size() / 2);
    for(::std::size_t i = 0; i < text.size(); i += 2)
        result.push_back(static_cast<::std::byte>((nibble(text[i]) << 4u) | nibble(text[i + 1])));
    return result;
}
static ::std::string hex(::std::span<const ::std::byte> bytes) {
    constexpr char digits[] = "0123456789abcdef";
    ::std::string result; result.reserve(bytes.size() * 2);
    for(auto b : bytes) { auto n = ::std::to_integer<unsigned>(b); result += digits[n >> 4u]; result += digits[n & 15u]; }
    return result;
}
static const char* error_name(r::ErrorCode code) {
    switch(code) {
    case r::ErrorCode::invalid_argument: return "invalid_argument";
    case r::ErrorCode::constraint_violation: return "constraint_violation";
    case r::ErrorCode::truncated_input: return "truncated_input";
    case r::ErrorCode::nonzero_padding: return "nonzero_padding";
    case r::ErrorCode::trailing_data: return "trailing_data";
    case r::ErrorCode::resource_limit: return "resource_limit";
    case r::ErrorCode::allocation_failure: return "allocation_failure";
    case r::ErrorCode::invalid_state: return "invalid_state";
    }
    return "unknown_error";
}
static ::std::string error_text(const r::Error& error) {
    return ::std::string("error:") + error_name(error.code) + ':' + ::std::to_string(error.bit_offset);
}
static a::Criticality criticality(::std::string_view text) {
    auto n = number<unsigned>(text);
    if(n > 2) throw ::std::invalid_argument("criticality");
    return a::Criticality{static_cast<a::Criticality::Known>(n)};
}
static ::std::string sequence_text(const r::SequenceExtensionData& data) {
    if(!data.received_bitmap_bit_count && data.unknown_additions.empty()) return "-";
    ::std::string result = ::std::to_string(data.received_bitmap_bit_count) + '/';
    bool first = true;
    for(const auto& item : data.unknown_additions) {
        if(!first) result += '+';
        first = false;
        result += ::std::to_string(item.addition_index) + ',' + hex(item.payload_octets);
    }
    return result;
}
static r::SequenceExtensionData sequence_value(::std::string_view text) {
    r::SequenceExtensionData result{};
    if(text == "-") return result;
    auto pieces = split(text, '/');
    if(pieces.size() != 2) throw ::std::invalid_argument("sequence sidecar");
    result.received_bitmap_bit_count = number<::std::size_t>(pieces[0]);
    if(!pieces[1].empty()) for(const auto& item : split(pieces[1], '+')) {
        auto fields = split(item, ',');
        if(fields.size() != 2) throw ::std::invalid_argument("sequence addition");
        result.unknown_additions.push_back({number<::std::uint64_t>(fields[0]), unhex(fields[1])});
    }
    return result;
}
template<class E, class M> static E opaque_entry(const ::std::vector<::std::string>& fields, ::std::size_t start) {
    if(fields.size() != start + 3) throw ::std::invalid_argument("opaque entry");
    E result{}; result.id = number<::std::uint64_t>(fields[start]); result.criticality = criticality(fields[start + 1]);
    typename M::unknown_type payload{unhex(fields[start + 2])};
    if constexpr(requires { result.extension_value; }) result.extension_value = ::std::move(payload);
    else result.value = ::std::move(payload);
    return result;
}
template<class E, class M> static ::std::string opaque_text(const E& entry) {
    const auto& payload = [&]() -> const auto& {
        if constexpr(requires { entry.extension_value; }) return ::std::get<typename M::unknown_type>(entry.extension_value).payload;
        else return ::std::get<typename M::unknown_type>(entry.value).payload;
    }();
    return ::std::to_string(entry.id) + ':' + ::std::to_string(static_cast<unsigned>(entry.criticality.value)) + ':' + hex(payload);
}
static ::std::string pair_extensions_text(const a::Pair& pair) {
    if(!pair.i_e_extensions) return "-";
    ::std::string result;
    bool first = true;
    for(const auto& entry : pair.i_e_extensions->elements) {
        if(!first) result += '+';
        first = false;
        auto fields = opaque_text<a::PairExtension, a::PairExtensionMapping>(entry);
        ::std::replace(fields.begin(), fields.end(), ':', ','); result += fields;
    }
    return result.empty() ? "empty" : result;
}
template<class E, class M> static E enum_value(const ::std::string& kind, const ::std::string& text) {
    E result{};
    if(kind == "k") {
        auto assigned = number<::std::int64_t>(text);
        bool found = false;
        for(const auto& item : M::entries) if(item.assigned_number == assigned) { result.value = item.value; found = true; break; }
        if(!found) throw ::std::invalid_argument("unknown assigned enum number");
    } else if(kind == "x") {
        if constexpr(M::extensible) result.value = typename E::UnknownExtension{number<::std::uint64_t>(text)};
        else throw ::std::invalid_argument("nonextensible enum");
    } else throw ::std::invalid_argument("enum kind");
    return result;
}
template<class E, class M> static ::std::string enum_text(const E& value) {
    if constexpr(M::extensible) {
        if(auto known = ::std::get_if<typename E::Known>(&value.value)) return "k:" + ::std::to_string(static_cast<::std::int64_t>(*known));
        return "x:" + ::std::to_string(::std::get<typename E::UnknownExtension>(value.value).index);
    } else return "k:" + ::std::to_string(static_cast<::std::int64_t>(value.value));
}
static a::Body parse_model(const ::std::string& model) {
    auto body_fields = split(model, '|');
    if(body_fields.size() != 2) throw ::std::invalid_argument("body model");
    a::Body result{};
    result.*a::BodyMapping::extension_data_member = sequence_value(body_fields[0]);
    if(body_fields[1].empty()) return result;
    for(const auto& text : split(body_fields[1], ';')) {
        auto fields = split(text, ':');
        if(fields.size() < 4) throw ::std::invalid_argument("IE model");
        a::Entry entry{};
        entry.id = number<::std::uint64_t>(fields[0]); entry.criticality = criticality(fields[1]);
        const auto& kind = fields[2];
        if(kind == "pair") {
            if(fields.size() != 7) throw ::std::invalid_argument("pair model");
            a::Pair pair{};
            pair.a_mf_ue_ngap_id = number<::std::uint64_t>(fields[3]); pair.r_an_ue_ngap_id = number<::std::uint64_t>(fields[4]);
            if(fields[5] != "-") {
                pair.i_e_extensions.emplace();
                if(fields[5] != "empty") for(const auto& extra : split(fields[5], '+'))
                    pair.i_e_extensions->elements.push_back(opaque_entry<a::PairExtension, a::PairExtensionMapping>(split(extra, ','), 0));
            }
            pair.*a::PairMapping::extension_data_member = sequence_value(fields[6]);
            entry.value = a::UeIe{a::UeIds{a::UeMapping::wrapper_0{::std::move(pair)}}};
        } else if(kind == "amf") {
            if(fields.size() != 4) throw ::std::invalid_argument("AMF model");
            entry.value = a::UeIe{a::UeIds{a::UeMapping::wrapper_1{number<::std::uint64_t>(fields[3])}}};
        } else if(kind == "ue_ext") {
            entry.value = a::UeIe{a::UeIds{a::UeMapping::wrapper_2{opaque_entry<a::UeExtension, a::UeExtensionMapping>(fields, 3)}}};
        } else if(kind == "cause_ext") {
            entry.value = a::CauseIe{a::Cause{a::CauseMapping::wrapper_5{opaque_entry<a::CauseExtension, a::CauseExtensionMapping>(fields, 3)}}};
        } else if(kind == "cause") {
            if(fields.size() != 6) throw ::std::invalid_argument("cause model");
            a::Cause cause{};
#define CAUSE_VALUE(branch, ordinal) if(fields[3] == branch) cause = a::CauseMapping::wrapper_##ordinal{enum_value<a::CauseMapping::payload_type_##ordinal, a::CauseMapping::payload_mapping_##ordinal>(fields[4], fields[5])}
            CAUSE_VALUE("radio_network", 0);
            else CAUSE_VALUE("transport", 1);
            else CAUSE_VALUE("nas", 2);
            else CAUSE_VALUE("protocol", 3);
            else CAUSE_VALUE("misc", 4);
            else throw ::std::invalid_argument("cause branch");
#undef CAUSE_VALUE
            entry.value = a::CauseIe{::std::move(cause)};
        } else if(kind == "unknown") {
            if(fields.size() != 4) throw ::std::invalid_argument("unknown IE model");
            entry.value = a::EntryMapping::unknown_type{unhex(fields[3])};
        } else throw ::std::invalid_argument("IE kind");
        result.protocol_i_es.elements.push_back(::std::move(entry));
    }
    return result;
}
static ::std::string dump_model(const a::Body& body) {
    ::std::string result = sequence_text(body.*a::BodyMapping::extension_data_member) + '|';
    bool first = true;
    for(const auto& entry : body.protocol_i_es.elements) {
        if(!first) result += ';';
        first = false;
        result += ::std::to_string(entry.id) + ':' + ::std::to_string(static_cast<unsigned>(entry.criticality.value)) + ':';
        ::std::visit([&](const auto& payload) {
            using T = ::std::remove_cvref_t<decltype(payload)>;
            if constexpr(::std::is_same_v<T, a::EntryMapping::unknown_type>) result += "unknown:" + hex(payload.payload);
            else if constexpr(::std::is_same_v<T, a::UeIe>) ::std::visit([&](const auto& choice) {
                using C = ::std::remove_cvref_t<decltype(choice)>;
                if constexpr(::std::is_same_v<C, a::UeMapping::wrapper_0>)
                    result += "pair:" + ::std::to_string(choice.value.a_mf_ue_ngap_id) + ':' + ::std::to_string(choice.value.r_an_ue_ngap_id) + ':' + pair_extensions_text(choice.value) + ':' + sequence_text(choice.value.*a::PairMapping::extension_data_member);
                else if constexpr(::std::is_same_v<C, a::UeMapping::wrapper_1>) result += "amf:" + ::std::to_string(choice.value);
                else result += "ue_ext:" + opaque_text<a::UeExtension, a::UeExtensionMapping>(choice.value);
            }, payload.value);
            else ::std::visit([&](const auto& choice) {
                using C = ::std::remove_cvref_t<decltype(choice)>;
#define CAUSE_TEXT(branch, ordinal) if constexpr(::std::is_same_v<C, a::CauseMapping::wrapper_##ordinal>) result += "cause:" branch ":" + enum_text<a::CauseMapping::payload_type_##ordinal, a::CauseMapping::payload_mapping_##ordinal>(choice.value)
                CAUSE_TEXT("radio_network", 0);
                else CAUSE_TEXT("transport", 1);
                else CAUSE_TEXT("nas", 2);
                else CAUSE_TEXT("protocol", 3);
                else CAUSE_TEXT("misc", 4);
                else result += "cause_ext:" + opaque_text<a::CauseExtension, a::CauseExtensionMapping>(choice.value);
#undef CAUSE_TEXT
            }, payload.value);
        }, entry.value);
    }
    return result;
}
static bool damage_bytes(Bytes& bytes) {
    if(bytes.empty()) return false;
    bytes.front() ^= ::std::byte{0xff}; return true;
}
static bool damage_sequence(r::SequenceExtensionData& data) {
    bool changed = false;
    for(auto& item : data.unknown_additions) changed = damage_bytes(item.payload_octets) || changed;
    return changed;
}
static bool damage_unknowns(a::Body& body) {
    bool changed = damage_sequence(body.*a::BodyMapping::extension_data_member);
    for(auto& entry : body.protocol_i_es.elements) ::std::visit([&](auto& payload) {
        using T = ::std::remove_cvref_t<decltype(payload)>;
        if constexpr(::std::is_same_v<T, a::EntryMapping::unknown_type>) changed = damage_bytes(payload.payload) || changed;
        else if constexpr(::std::is_same_v<T, a::UeIe>) ::std::visit([&](auto& choice) {
            using C = ::std::remove_cvref_t<decltype(choice)>;
            if constexpr(::std::is_same_v<C, a::UeMapping::wrapper_0>) {
                changed = damage_sequence(choice.value.*a::PairMapping::extension_data_member) || changed;
                if(choice.value.i_e_extensions) for(auto& extra : choice.value.i_e_extensions->elements)
                    changed = damage_bytes(::std::get<a::PairExtensionMapping::unknown_type>(extra.extension_value).payload) || changed;
            } else if constexpr(::std::is_same_v<C, a::UeMapping::wrapper_2>)
                changed = damage_bytes(::std::get<a::UeExtensionMapping::unknown_type>(choice.value.value).payload) || changed;
        }, payload.value);
        else ::std::visit([&](auto& choice) {
            using C = ::std::remove_cvref_t<decltype(choice)>;
            if constexpr(::std::is_same_v<C, a::CauseMapping::wrapper_5>)
                changed = damage_bytes(::std::get<a::CauseExtensionMapping::unknown_type>(choice.value.value).payload) || changed;
        }, payload.value);
    }, entry.value);
    return changed;
}

static void roundtrip(const ::std::string& model) {
    auto body = parse_model(model); auto encoded = a::encode_body(body); REQUIRE(encoded);
    auto decoded = a::decode_body(encoded.value().octets); REQUIRE(decoded);
    REQUIRE(dump_model(decoded.value()) == model);
}
template<class Mapping> static ::std::size_t enum_cases(const char* branch) {
    for(const auto& item : Mapping::entries)
        roundtrip(::std::string("-|15:1:cause:") + branch + ":k:" + ::std::to_string(item.assigned_number));
    for(auto index : {static_cast<::std::uint64_t>(Mapping::known_addition_count), ::std::uint64_t{63}, ::std::uint64_t{64}, ::std::uint64_t{255}})
        roundtrip(::std::string("-|15:1:cause:") + branch + ":x:" + ::std::to_string(index));
    return Mapping::entries.size();
}
static void self_test() {
    const ::std::string baseline = "-|114:0:pair:1:2:-:-;15:1:cause:nas:k:0";
    const auto literal = unhex("0000020072000400010002000f400140");
    auto body = parse_model(baseline);
    auto encoded = a::encode_body(body); REQUIRE(encoded && encoded.value().octets == literal);
    auto decoded = a::decode_body(literal); REQUIRE(decoded && dump_model(decoded.value()) == baseline);
    roundtrip("-|"); roundtrip("-|15:2:cause:transport:k:0");
    roundtrip("-|15:2:cause:transport:k:0;114:1:amf:1099511627775;15:0:cause:misc:x:64");
    roundtrip("-|114:0:pair:1099511627775:4294967295:-:-");
    ::std::size_t known = 0;
    known += enum_cases<a::CauseMapping::payload_mapping_0>("radio_network");
    known += enum_cases<a::CauseMapping::payload_mapping_1>("transport");
    known += enum_cases<a::CauseMapping::payload_mapping_2>("nas");
    known += enum_cases<a::CauseMapping::payload_mapping_3>("protocol");
    known += enum_cases<a::CauseMapping::payload_mapping_4>("misc"); REQUIRE(known == 81);
    for(const auto& model : {"-|114:0:amf:1099511627776", "-|114:0:pair:0:4294967296:-:-", "-|15:0:amf:1", "-|114:0:pair:1:2:empty:-", "-|65535:1:unknown:80", "-|114:0:ue_ext:1:2:80", "-|15:1:cause_ext:2:0:80", "-|114:0:pair:1:2:7,1,80:-"}) {
        auto value = parse_model(model); auto refusal = a::encode_body(value);
        REQUIRE(!refusal && refusal.error().code == r::ErrorCode::constraint_violation);
    }
    auto invalid = literal; invalid.back() |= ::std::byte{1};
    auto bad = a::decode_body(invalid); REQUIRE(!bad && bad.error().code == r::ErrorCode::nonzero_padding && bad.error().bit_offset == 127);
    // Generated BODY callbacks use the same live context as their known
    // children. Child failure rolls back to the outer known field (after
    // its received criticality), while its first physical error is sticky.
    {
        r::DecodeContext context;
        auto made = r::BitReader::make(invalid, context); REQUIRE(made);
        auto reader = ::std::move(made).value(); r::FieldReader fields(reader);
        auto failed = a::generated::compound_codec::get_NgapPduContentsUeContextReleaseCommand(fields);
        REQUIRE(!failed && failed.error().code == r::ErrorCode::nonzero_padding && failed.error().bit_offset == 127);
        REQUIRE(reader.cursor_bit() == 106 && context.wire_bits() == 106 && context.collection_elements() == 2);
        REQUIRE(context.known_open_depth() == 0 && context.known_open_staging_octets() == 0);
        auto replay = a::generated::compound_codec::get_NgapPduContentsUeContextReleaseCommand(fields);
        auto primitive = fields.read_bit(); auto complete = reader.validate_complete_value();
        for(const auto error : {replay.error(), primitive.error(), complete.error()})
            REQUIRE(error.code == failed.error().code && error.bit_offset == failed.error().bit_offset);
        REQUIRE(reader.cursor_bit() == 106 && context.wire_bits() == 106 && context.collection_elements() == 2);
    }
    {
        auto malformed = parse_model("-|114:0:pair:1099511627776:2:-:-;15:1:cause:nas:k:0");
        r::EncodeContext context; r::BitWriter writer(context); r::FieldWriter fields(writer);
        auto failed = a::generated::compound_codec::put_NgapPduContentsUeContextReleaseCommand(fields, malformed);
        REQUIRE(!failed && failed.error().code == r::ErrorCode::constraint_violation && failed.error().bit_offset == 42);
        REQUIRE(writer.cursor_bit() == 42 && context.wire_bits() == 42 && context.logical_output_octets() == 6);
        REQUIRE(context.collection_elements() == 2 && context.known_open_depth() == 0 && context.known_open_staging_octets() == 0);
        auto replay = a::generated::compound_codec::put_NgapPduContentsUeContextReleaseCommand(fields, body);
        auto primitive = fields.write_bit(false); auto complete = writer.finish();
        for(const auto error : {replay.error(), primitive.error(), complete.error()})
            REQUIRE(error.code == failed.error().code && error.bit_offset == failed.error().bit_offset);
        REQUIRE(writer.cursor_bit() == 42 && context.wire_bits() == 42 && context.logical_output_octets() == 6);
    }
    invalid = literal; invalid[0] |= ::std::byte{1};
    bad = a::decode_body(invalid); REQUIRE(!bad && bad.error().code == r::ErrorCode::nonzero_padding && bad.error().bit_offset == 7);
    invalid = literal; invalid.push_back(::std::byte{0});
    bad = a::decode_body(invalid); REQUIRE(!bad && bad.error().code == r::ErrorCode::trailing_data && bad.error().bit_offset == 128);
    invalid = literal; invalid.pop_back();
    bad = a::decode_body(invalid); REQUIRE(!bad && bad.error().code == r::ErrorCode::truncated_input);
    invalid = literal; invalid[6] = ::std::byte{0xc0};
    bad = a::decode_body(invalid); REQUIRE(!bad && bad.error().code == r::ErrorCode::constraint_violation && bad.error().bit_offset == 48);
    invalid = literal; invalid[6] = ::std::byte{5}; invalid.insert(invalid.begin() + 11, ::std::byte{0});
    bad = a::decode_body(invalid); REQUIRE(!bad && bad.error().code == r::ErrorCode::trailing_data && bad.error().bit_offset == 88);
    invalid = literal; invalid[7] = ::std::byte{0xc0};
    bad = a::decode_body(invalid); REQUIRE(!bad && bad.error().code == r::ErrorCode::constraint_violation);
    // Cause.protocol has seven root values: selector 3, enum root bit 0,
    // then reserved three-bit root index 7 (011 0 111 0).
    invalid = literal; invalid.back() = ::std::byte{0x6e};
    bad = a::decode_body(invalid); REQUIRE(!bad && bad.error().code == r::ErrorCode::constraint_violation);
    for(int kind = 0; kind < 5; ++kind) {
        r::Limits limits{};
        if(kind == 0) limits.max_input_octets = literal.size() - 1;
        if(kind == 1) limits.max_wire_bits = literal.size() * 8 - 1;
        if(kind == 2) limits.max_collection_elements = 1;
        if(kind == 3) limits.max_known_open_depth = 0;
        if(kind == 4) limits.max_known_open_staging_octets = 3;
        auto failure = a::decode_body(literal, limits); REQUIRE(!failure && failure.error().code == r::ErrorCode::resource_limit);
    }
    r::Limits exact{}; exact.max_input_octets = literal.size(); exact.max_output_octets = literal.size(); exact.max_wire_bits = literal.size() * 8;
    exact.max_collection_elements = 2; exact.max_known_open_depth = 1; exact.max_known_open_staging_octets = 4;
    REQUIRE(a::decode_body(literal, exact)); REQUIRE(a::encode_body(body, exact));
    --exact.max_output_octets; auto failure = a::encode_body(body, exact); REQUIRE(!failure && failure.error().code == r::ErrorCode::resource_limit);
    for(int kind = 0; kind < 4; ++kind) {
        r::Limits limits{};
        if(kind == 0) limits.max_wire_bits = literal.size() * 8 - 1;
        if(kind == 1) limits.max_collection_elements = 1;
        if(kind == 2) limits.max_known_open_depth = 0;
        if(kind == 3) limits.max_known_open_staging_octets = 3;
        auto failed = a::encode_body(body, limits);
        REQUIRE(!failed && failed.error().code == r::ErrorCode::resource_limit);
    }
    // Independent literals: not claims about pycrate's unknown SEQUENCE API.
    auto root_unknown = unhex("80000002800180");
    auto retained = a::decode_body(root_unknown); REQUIRE(retained && dump_model(retained.value()) == "2/1,80|");
    auto refused = a::encode_body(retained.value()); REQUIRE(!refused && refused.error().code == r::ErrorCode::constraint_violation && refused.error().bit_offset == 0);
    auto pair_unknown = literal; pair_unknown[6] = ::std::byte{7}; pair_unknown[7] |= ::std::byte{0x20};
    const Bytes suffix{::std::byte{1}, ::std::byte{1}, ::std::byte{0x80}}; pair_unknown.insert(pair_unknown.begin() + 11, suffix.begin(), suffix.end());
    retained = a::decode_body(pair_unknown); REQUIRE(retained && dump_model(retained.value()) == "-|114:0:pair:1:2:-:1/0,80;15:1:cause:nas:k:0");
    refused = a::encode_body(retained.value()); REQUIRE(!refused && refused.error().code == r::ErrorCode::constraint_violation);
    auto copy = retained.value(); auto moved = ::std::move(retained).value();
    auto before = dump_model(copy); REQUIRE(damage_unknowns(moved)); REQUIRE(dump_model(copy) == before && dump_model(moved) != before);
    for(int kind = 0; kind < 3; ++kind) {
        r::Limits limits{};
        if(kind == 0) limits.max_retained_unknown_payload_octets = 0;
        if(kind == 1) limits.max_retained_unknown_records = 0;
        if(kind == 2) limits.max_extension_bitmap_bits = 1;
        auto failed = a::decode_body(kind == 2 ? root_unknown : pair_unknown, limits);
        REQUIRE(!failed && failed.error().code == r::ErrorCode::resource_limit);
    }
    r::Limits retained_exact{}; retained_exact.max_retained_unknown_payload_octets = 1; retained_exact.max_retained_unknown_records = 1; retained_exact.max_extension_bitmap_bits = 2;
    REQUIRE(a::decode_body(root_unknown, retained_exact)); REQUIRE(a::decode_body(pair_unknown, retained_exact));
    // Independent nested fragment literal. UE choice selector 2 is followed
    // by alignment, ID 65535, criticality reject, C1/16K opaque octets and
    // mandatory zero terminal determinant. The known UE outer open frame
    // itself fragments: C1 first 16K, then six remaining child octets.
    Bytes child = unhex("80ffff00c1");
    child.insert(child.end(), 16384, ::std::byte{0xa5}); child.push_back(::std::byte{0});
    REQUIRE(child.size() == 16390);
    Bytes nested_fragment = unhex("000001007200c1");
    nested_fragment.insert(nested_fragment.end(), child.begin(), child.begin() + 16384);
    nested_fragment.push_back(::std::byte{6});
    nested_fragment.insert(nested_fragment.end(), child.begin() + 16384, child.end());
    r::Limits fragment_exact{};
    fragment_exact.max_known_open_depth = 1;
    fragment_exact.max_known_open_staging_octets = child.size();
    fragment_exact.max_retained_unknown_payload_octets = 16384;
    fragment_exact.max_retained_unknown_records = 1;
    auto fragment = a::decode_body(nested_fragment, fragment_exact); REQUIRE(fragment);
    auto fragment_model = dump_model(fragment.value());
    REQUIRE(fragment_model == "-|114:0:ue_ext:65535:0:" + hex(Bytes(16384, ::std::byte{0xa5})));
    auto fragment_copy = fragment.value(); auto fragment_move = ::std::move(fragment).value();
    ::std::fill(nested_fragment.begin(), nested_fragment.end(), ::std::byte{0xff});
    REQUIRE(dump_model(fragment_copy) == fragment_model && dump_model(fragment_move) == fragment_model);
    REQUIRE(damage_unknowns(fragment_move) && dump_model(fragment_copy) == fragment_model);
    auto opaque_refusal = a::encode_body(fragment_copy);
    REQUIRE(!opaque_refusal && opaque_refusal.error().code == r::ErrorCode::constraint_violation);
    // Restore physical literal after the ownership overwrite check.
    nested_fragment = unhex("000001007200c1");
    nested_fragment.insert(nested_fragment.end(), child.begin(), child.begin() + 16384);
    nested_fragment.push_back(::std::byte{6});
    nested_fragment.insert(nested_fragment.end(), child.begin() + 16384, child.end());
    for(int kind = 0; kind < 4; ++kind) {
        auto limits = fragment_exact;
        if(kind == 0) --limits.max_known_open_depth;
        if(kind == 1) --limits.max_known_open_staging_octets;
        if(kind == 2) --limits.max_retained_unknown_payload_octets;
        if(kind == 3) --limits.max_retained_unknown_records;
        auto failed = a::decode_body(nested_fragment, limits);
        REQUIRE(!failed && failed.error().code == r::ErrorCode::resource_limit);
    }
    for(int operation = 0; operation < 2; ++operation) {
        long point;
        for(point = 0; point < 10000; ++point) {
            allocation_countdown = point;
            if(operation == 0) {
                auto attempt = a::decode_body(literal); allocation_countdown = -1;
                if(attempt) { REQUIRE(dump_model(attempt.value()) == baseline); break; }
                REQUIRE(attempt.error().code == r::ErrorCode::allocation_failure);
            } else {
                auto attempt = a::encode_body(body); allocation_countdown = -1;
                if(attempt) { REQUIRE(attempt.value().octets == literal); break; }
                REQUIRE(attempt.error().code == r::ErrorCode::allocation_failure);
            }
        }
        REQUIRE(point > 0 && point < 10000);
    }
    ::std::cout << "PASS actual body literals, all81known enums, scalar extensions, owned sidecars, strict failures, limits and allocations\n";
}
int main(int argc, char** argv) {
    try {
        if(argc == 2 && ::std::string_view(argv[1]) == "self-test") { self_test(); return 0; }
        if(argc != 3) return 2;
        ::std::string mode = argv[1], input = argv[2];
        if(input == "-") ::std::getline(::std::cin, input);
        if(mode == "encode-body") {
            auto result = a::encode_body(parse_model(input));
            ::std::cout << (result ? hex(result.value().octets) : error_text(result.error())) << '\n'; return 0;
        }
        if(mode != "decode-body" && mode != "refuse-body") return 2;
        auto bytes = unhex(input); auto result = a::decode_body(bytes);
        if(!result) { ::std::cout << error_text(result.error()) << '\n'; return 0; }
        // Every native case checks Parser-independent owned values and input
        // destruction/copy/move, including nested opaque payloads and sidecars.
        ::std::fill(bytes.begin(), bytes.end(), ::std::byte{0xff}); bytes.clear(); bytes.shrink_to_fit();
        auto copied = result.value(); auto moved = ::std::move(result).value();
        auto model = dump_model(copied); REQUIRE(dump_model(moved) == model);
        if(damage_unknowns(moved)) REQUIRE(dump_model(copied) == model && dump_model(moved) != model);
        if(mode == "decode-body") ::std::cout << model << '\n';
        else {
            auto encoded = a::encode_body(copied);
            ::std::cout << (encoded ? "encoded:" + hex(encoded.value().octets) : error_text(encoded.error())) << '\n';
        }
        return 0;
    } catch(const ::std::exception& error) { ::std::cerr << "driver input/error: " << error.what() << '\n'; return 2; }
}
