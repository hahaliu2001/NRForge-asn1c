#include "adapter.hpp"
// Reuse the accepted N10 body value grammar, ownership checks and allocation
// injector. This does not supply an envelope codec or alter the N10 driver.
#define main n10_body_driver_main
#include "../n10-body-qualification/driver.cpp"
#undef main
namespace envelope = n11_integration;

static envelope::Pdu parse_envelope(const ::std::string& model) {
    const auto first = model.find(':');
    if(first == model.npos) throw ::std::invalid_argument("PDU model");
    if(model.substr(0,first) == "x") {
        const auto second = model.find(':',first + 1);
        if(second == model.npos) throw ::std::invalid_argument("extension model");
        return envelope::make_extension(number<::std::uint64_t>(::std::string_view(model).substr(first + 1,second - first - 1)),unhex(::std::string_view(model).substr(second + 1)));
    }
    const bool opaque = model.substr(0,first) == "o";
    if(!opaque && model.substr(0,first) != "i") throw ::std::invalid_argument("PDU kind");
    auto start = first + 1;
    char root = 'i';
    if(opaque) {
        const auto separator = model.find(':',start);
        if(separator != start + 1) throw ::std::invalid_argument("opaque root");
        root = model[start]; start = separator + 1;
    }
    const auto code_end = model.find(':',start), crit_end = model.find(':',code_end + 1);
    if(code_end == model.npos || crit_end == model.npos) throw ::std::invalid_argument("PDU fields");
    const auto code = number<::std::uint64_t>(::std::string_view(model).substr(start,code_end - start));
    const auto received = criticality(::std::string_view(model).substr(code_end + 1,crit_end - code_end - 1));
    if(opaque) return envelope::make_opaque(root,code,received,unhex(::std::string_view(model).substr(crit_end + 1)));
    return envelope::make_typed(code,received,parse_model(model.substr(crit_end + 1)));
}
static ::std::string dump_envelope(const envelope::Pdu& pdu) {
    const auto root = envelope::kind(pdu);
    if(root == 'x') return "x:" + ::std::to_string(envelope::extension_index(pdu)) + ':' + hex(envelope::opaque_payload(pdu));
    const auto fields = ::std::to_string(envelope::procedure_code(pdu)) + ':' + ::std::to_string(static_cast<unsigned>(envelope::received_criticality(pdu).value)) + ':';
    if(const auto* body = envelope::typed_body(pdu)) return "i:" + fields + dump_model(*body);
    return ::std::string("o:") + root + ':' + fields + hex(envelope::opaque_payload(pdu));
}
static void expected_error(const auto& result,r::ErrorCode code,::std::size_t bit) {
    REQUIRE(!result && result.error().code == code && result.error().bit_offset == bit);
}
[[maybe_unused]] static void pdu_self_test() {
    const ::std::string model = "i:41:0:-|114:0:pair:1:2:-:-;15:1:cause:nas:k:0";
    const auto literal = unhex("002900100000020072000400010002000f400140");
    const auto value = parse_envelope(model);
    auto encoded = envelope::encode(value); REQUIRE(encoded && encoded.value().octets == literal && encoded.value().octet_count == 20);
    auto decoded = envelope::decode(literal); REQUIRE(decoded && dump_envelope(decoded.value()) == model);
    for(unsigned crit = 0; crit < 3; ++crit) {
        const auto changed_model = "i:41:" + ::std::to_string(crit) + ":-|114:0:pair:1:2:-:-;15:1:cause:nas:k:0";
        auto changed_literal = literal; changed_literal[2] = static_cast<::std::byte>(crit << 6);
        auto roundtrip = envelope::encode(parse_envelope(changed_model)); REQUIRE(roundtrip && roundtrip.value().octets == changed_literal);
        auto received = envelope::decode(changed_literal); REQUIRE(received && dump_envelope(received.value()) == changed_model);
    }
    for(const auto& [wire,text] : ::std::initializer_list<::std::pair<const char*,const char*>>{
        {"0000400180","o:i:0:1:80"},{"2029000180","o:s:41:0:80"},{"40ff800180","o:u:255:2:80"},
        {"800180","x:0:80"},{"bf0180","x:63:80"},{"c001400180","x:64:80"}}) {
        auto input = unhex(wire); auto result = envelope::decode(input); REQUIRE(result && dump_envelope(result.value()) == text);
        auto copy = result.value(); auto moved = ::std::move(result).value();
        ::std::fill(input.begin(),input.end(),::std::byte{0xff}); input.clear(); input.shrink_to_fit();
        REQUIRE(dump_envelope(copy) == text && dump_envelope(moved) == text);
        REQUIRE(envelope::damage_opaque(moved) && dump_envelope(copy) == text && dump_envelope(moved) != text);
        auto refusal = envelope::encode(copy); REQUIRE(!refusal && refusal.error().code == r::ErrorCode::constraint_violation);
    }
    auto changed = literal; changed.back() |= ::std::byte{1};
    expected_error(envelope::decode(changed),r::ErrorCode::nonzero_padding,159);
    changed = literal; changed[0] |= ::std::byte{1};
    expected_error(envelope::decode(changed),r::ErrorCode::nonzero_padding,7);
    changed = literal; changed[2] |= ::std::byte{1};
    expected_error(envelope::decode(changed),r::ErrorCode::nonzero_padding,23);
    changed = literal; changed.push_back(::std::byte{0});
    expected_error(envelope::decode(changed),r::ErrorCode::trailing_data,160);
    changed = literal; changed[3] = ::std::byte{17}; changed.push_back(::std::byte{0});
    expected_error(envelope::decode(changed),r::ErrorCode::trailing_data,160);
    changed = literal; changed[2] = ::std::byte{0xc0};
    REQUIRE(!envelope::decode(changed));
    changed = literal; changed[0] = ::std::byte{0x60}; REQUIRE(!envelope::decode(changed));
    for(::std::size_t n = 0; n < literal.size(); ++n)
        expected_error(envelope::decode(::std::span(literal).first(n)),r::ErrorCode::truncated_input,n * 8);
    auto mismatched = parse_envelope("i:40:0:-|"); auto refused = envelope::encode(mismatched);
    REQUIRE(!refused && refused.error().code == r::ErrorCode::constraint_violation);
    r::Limits exact{};
    exact.max_input_octets = 20; exact.max_output_octets = 20; exact.max_wire_bits = 160;
    exact.max_collection_elements = 2; exact.max_known_open_depth = 2; exact.max_known_open_staging_octets = 20;
    auto encode_exact = exact;
    // Decoder stages the complete 16-octet body concurrently with its 4-octet
    // UE child. Encoder's final 16-octet parent growth overlaps the live
    // one-octet Cause child, so its peak is 17.
    encode_exact.max_known_open_staging_octets = 17;
    REQUIRE(envelope::decode(literal,exact)); REQUIRE(envelope::encode(value,encode_exact));
    for(int budget = 0; budget < 6; ++budget) {
        auto limit = exact;
        if(budget == 0) --limit.max_input_octets;
        if(budget == 1) --limit.max_output_octets;
        if(budget == 2) --limit.max_wire_bits;
        if(budget == 3) --limit.max_collection_elements;
        if(budget == 4) --limit.max_known_open_depth;
        if(budget == 5) --limit.max_known_open_staging_octets;
        if(budget != 1) { auto failed = envelope::decode(literal,limit); REQUIRE(!failed && failed.error().code == r::ErrorCode::resource_limit); }
        if(budget != 0) {
            limit.max_known_open_staging_octets = budget == 5 ? 16 : 17;
            auto failed = envelope::encode(value,limit); REQUIRE(!failed && failed.error().code == r::ErrorCode::resource_limit);
        }
    }
    {
        const auto opaque = unhex("20298004deadbeef");
        r::Limits limits{}; limits.max_retained_unknown_payload_octets = 4; limits.max_retained_unknown_records = 1;
        REQUIRE(envelope::decode(opaque,limits));
        --limits.max_retained_unknown_payload_octets;
        expected_error(envelope::decode(opaque,limits),r::ErrorCode::resource_limit,18);
        ++limits.max_retained_unknown_payload_octets; --limits.max_retained_unknown_records;
        expected_error(envelope::decode(opaque,limits),r::ErrorCode::resource_limit,18);
        const auto sidecar = unhex("0029000780000002800180");
        limits = {}; limits.max_retained_unknown_payload_octets = 1; limits.max_retained_unknown_records = 1;
        limits.max_extension_bitmap_bits = 2; limits.max_known_open_depth = 1; limits.max_known_open_staging_octets = 7;
        auto owned = envelope::decode(sidecar,limits);
        REQUIRE(owned && dump_envelope(owned.value()) == "i:41:0:2/1,80|");
        --limits.max_extension_bitmap_bits;
        auto failure = envelope::decode(sidecar,limits); REQUIRE(!failure && failure.error().code == r::ErrorCode::resource_limit);
        auto refusal = envelope::encode(owned.value()); REQUIRE(!refusal && refusal.error().code == r::ErrorCode::constraint_violation);
    }
    {
        // Independent outer CHOICE extension fragment: index0, C1/16K
        // payload followed by a mandatory zero terminal determinant.
        auto fragment = unhex("80c1"); fragment.insert(fragment.end(),16384,::std::byte{0xa5}); fragment.push_back(::std::byte{0});
        r::Limits limits{}; limits.max_input_octets = fragment.size(); limits.max_wire_bits = fragment.size() * 8;
        limits.max_retained_unknown_payload_octets = 16384; limits.max_retained_unknown_records = 1;
        auto retained = envelope::decode(fragment,limits); REQUIRE(retained);
        REQUIRE(envelope::kind(retained.value()) == 'x' && envelope::extension_index(retained.value()) == 0);
        REQUIRE(envelope::opaque_payload(retained.value()) == Bytes(16384,::std::byte{0xa5}));
        auto copy = retained.value(); auto moved = ::std::move(retained).value();
        ::std::fill(fragment.begin(),fragment.end(),::std::byte{0xff}); fragment.clear(); fragment.shrink_to_fit();
        REQUIRE(envelope::opaque_payload(copy) == Bytes(16384,::std::byte{0xa5}));
        REQUIRE(envelope::damage_opaque(moved) && envelope::opaque_payload(copy).front() == ::std::byte{0xa5});
        fragment = unhex("80c1"); fragment.insert(fragment.end(),16384,::std::byte{0xa5}); fragment.push_back(::std::byte{0});
        for(int budget = 0; budget < 4; ++budget) {
            auto insufficient = limits;
            if(budget == 0) --insufficient.max_input_octets;
            if(budget == 1) --insufficient.max_wire_bits;
            if(budget == 2) --insufficient.max_retained_unknown_payload_octets;
            if(budget == 3) --insufficient.max_retained_unknown_records;
            auto failed = envelope::decode(fragment,insufficient); REQUIRE(!failed && failed.error().code == r::ErrorCode::resource_limit);
        }
        fragment.pop_back(); REQUIRE(!envelope::decode(fragment));
        auto determinant = unhex("80c0"); REQUIRE(!envelope::decode(determinant));
        determinant = unhex("80c5"); REQUIRE(!envelope::decode(determinant));
    }
    // Primitive and nested helper failures replay their first physical error.
    changed = literal; changed.back() |= ::std::byte{1};
    {
        r::DecodeContext context; auto made = r::BitReader::make(changed,context); REQUIRE(made);
        auto reader = ::std::move(made).value(); r::FieldReader fields(reader);
        auto failed = envelope::get(fields); expected_error(failed,r::ErrorCode::nonzero_padding,159);
        REQUIRE(reader.cursor_bit() == 18 && context.wire_bits() == 18);
        // The outer known-open transaction rolls back all body charges.
        REQUIRE(context.collection_elements() == 0 && context.known_open_depth() == 0 && context.known_open_staging_octets() == 0);
        auto replay = envelope::get(fields); auto primitive = fields.read_bit(); auto complete = reader.validate_complete_value();
        for(auto error : {replay.error(),primitive.error(),complete.error()}) REQUIRE(error.code == failed.error().code && error.bit_offset == failed.error().bit_offset);
        REQUIRE(reader.cursor_bit() == 18 && context.wire_bits() == 18);
    }
    {
        auto malformed = parse_envelope("i:41:0:-|114:0:pair:1099511627776:2:-:-;15:1:cause:nas:k:0");
        r::EncodeContext context; r::BitWriter writer(context); r::FieldWriter fields(writer);
        auto failed = envelope::put(fields,malformed); expected_error(failed,r::ErrorCode::constraint_violation,18);
        REQUIRE(writer.cursor_bit() == 18 && context.wire_bits() == 18 && context.logical_output_octets() == 3);
        REQUIRE(context.collection_elements() == 0 && context.known_open_depth() == 0 && context.known_open_staging_octets() == 0);
        auto replay = envelope::put(fields,value); auto primitive = fields.write_bit(false); auto complete = writer.finish();
        for(auto error : {replay.error(),primitive.error(),complete.error()}) REQUIRE(error.code == failed.error().code && error.bit_offset == failed.error().bit_offset);
        REQUIRE(writer.cursor_bit() == 18 && context.wire_bits() == 18 && context.logical_output_octets() == 3);
    }
    for(int operation = 0; operation < 2; ++operation) {
        long point;
        for(point = 0; point < 10000; ++point) {
            allocation_countdown = point;
            if(operation == 0) {
                auto result = envelope::decode(literal); allocation_countdown = -1;
                if(result) { REQUIRE(dump_envelope(result.value()) == model); break; }
                REQUIRE(result.error().code == r::ErrorCode::allocation_failure);
            } else {
                auto result = envelope::encode(value); allocation_countdown = -1;
                if(result) { REQUIRE(result.value().octets == literal); break; }
                REQUIRE(result.error().code == r::ErrorCode::allocation_failure);
            }
        }
        REQUIRE(point > 0 && point < 10000);
    }
    {
        const auto opaque = unhex("20298004deadbeef");
        long point;
        for(point = 0; point < 10000; ++point) {
            allocation_countdown = point;
            auto result = envelope::decode(opaque); allocation_countdown = -1;
            if(result) { REQUIRE(dump_envelope(result.value()) == "o:s:41:2:deadbeef"); break; }
            REQUIRE(result.error().code == r::ErrorCode::allocation_failure);
        }
        REQUIRE(point > 0 && point < 10000);
    }
    ::std::cout << "PASS complete PDU literals, framing/ownership, malformed known frames, shared budgets, sticky errors and allocation failures\n";
}
#if N11_TARGET_CODE_EXPECTED == 73
[[maybe_unused]] static void metadata_self_test() {
    // Explicit manually changed owned-descriptor regression, not a claim that
    // the untouched schema uses this procedure code or these effective tags.
    static_assert(envelope::Mapping::target_code == 73);
    static_assert(envelope::Mapping::source_ordinal_to_per_root_index[envelope::initiating_ordinal] == 2);
    const ::std::string model = "i:73:0:-|114:0:pair:1:2:-:-;15:1:cause:nas:k:0";
    const auto expected = unhex("404900100000020072000400010002000f400140");
    auto encoded = envelope::encode(parse_envelope(model)); REQUIRE(encoded && encoded.value().octets == expected);
    auto decoded = envelope::decode(expected); REQUIRE(decoded && dump_envelope(decoded.value()) == model);
    auto original_code = envelope::encode(parse_envelope("i:41:0:-|"));
    REQUIRE(!original_code && original_code.error().code == r::ErrorCode::constraint_violation);
    ::std::cout << "PASS manual descriptor code73 and reversed effective-tag dispatch\n";
}
#endif
#ifdef N11_CLOSED_TABLE_TEST
static void closed_table_self_test() {
    static_assert(!envelope::Mapping::object_set_extensible);
    static_assert(envelope::Mapping::has_procedure_code(0) && envelope::Mapping::has_procedure_code(41));
    static_assert(!envelope::Mapping::has_procedure_code(255));
    const auto baseline = unhex("002900100000020072000400010002000f400140");
    auto decoded = envelope::decode(baseline); REQUIRE(decoded && envelope::typed_body(decoded.value()));
    auto encoded = envelope::encode(decoded.value()); REQUIRE(encoded && encoded.value().octets == baseline);
    for(const auto& [wire,model] : ::std::initializer_list<::std::pair<const char*,const char*>>{
        {"0000400180","o:i:0:1:80"},{"2029000180","o:s:41:0:80"},{"800180","x:0:80"}}) {
        auto value = envelope::decode(unhex(wire)); REQUIRE(value && dump_envelope(value.value()) == model);
        auto refusal = envelope::encode(value.value()); REQUIRE(!refusal && refusal.error().code == r::ErrorCode::constraint_violation);
    }
    for(const auto* wire : {"00ff000180","20ff000180","40ff000180"}) {
        auto refusal = envelope::decode(unhex(wire)); REQUIRE(!refusal && refusal.error().code == r::ErrorCode::constraint_violation);
    }
    ::std::cout << "PASS closed procedure table refuses unlisted codes and preserves listed opaque outcomes\n";
}
#endif
int main(int argc,char** argv) {
    try {
        if(argc == 2 && ::std::string_view(argv[1]) == "self-test") {
#ifdef N11_CLOSED_TABLE_TEST
            closed_table_self_test();
#elif N11_TARGET_CODE_EXPECTED == 73
            metadata_self_test();
#else
            pdu_self_test();
#endif
            return 0;
        }
        if(argc != 3) return 2;
        ::std::string mode = argv[1], input = argv[2];
        if(input == "-") ::std::getline(::std::cin,input);
        if(mode == "encode-pdu") {
            auto result = envelope::encode(parse_envelope(input));
            ::std::cout << (result ? hex(result.value().octets) : error_text(result.error())) << '\n'; return 0;
        }
        if(mode != "decode-pdu" && mode != "refuse-pdu") return 2;
        auto bytes = unhex(input); auto result = envelope::decode(bytes);
        if(!result) { ::std::cout << error_text(result.error()) << '\n'; return 0; }
        auto copy = result.value(); auto moved = ::std::move(result).value();
        ::std::fill(bytes.begin(),bytes.end(),::std::byte{0xff}); bytes.clear(); bytes.shrink_to_fit();
        auto model = dump_envelope(copy); REQUIRE(dump_envelope(moved) == model);
        if(envelope::damage_opaque(moved)) REQUIRE(dump_envelope(copy) == model && dump_envelope(moved) != model);
        if(auto* body = envelope::mutable_typed_body(moved); body && damage_unknowns(*body)) REQUIRE(dump_envelope(copy) == model && dump_envelope(moved) != model);
        if(mode == "decode-pdu") ::std::cout << model << '\n';
        else {
            auto encoded = envelope::encode(copy);
            ::std::cout << (encoded ? "encoded:" + hex(encoded.value().octets) : error_text(encoded.error())) << '\n';
        }
        return 0;
    } catch(const ::std::exception& error) { ::std::cerr << "driver input/error: " << error.what() << '\n'; return 2; }
}
