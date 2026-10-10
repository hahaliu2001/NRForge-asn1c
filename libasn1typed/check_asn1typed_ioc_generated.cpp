#include <runtime.hpp>
#include "main_types.hpp"
#include "main_mapping.hpp"
#include "main_codec.hpp"
#include "main_adapters.hpp"
#include "empty_types.hpp"
#include "empty_mapping.hpp"
#include "empty_codec.hpp"
#include "empty_adapters.hpp"
#include "closed_types.hpp"
#include "closed_mapping.hpp"
#include "closed_codec.hpp"
#include "closed_adapters.hpp"
#include "ext_types.hpp"
#include "ext_mapping.hpp"
#include "ext_codec.hpp"
#include "ext_adapters.hpp"
#include "empty_ext_types.hpp"
#include "empty_ext_mapping.hpp"
#include "empty_ext_codec.hpp"
#include "empty_ext_adapters.hpp"
#include <cstdio>
#include <cstdlib>
#include <new>
#include <type_traits>

#define REQUIRE(x) do { if(!(x)) { std::fprintf(stderr,"%s:%d: %s\n",__FILE__,__LINE__,#x); std::abort(); } } while(0)
static long allocation_countdown = -1;
void* operator new(std::size_t count) {
    if(allocation_countdown == 0) { allocation_countdown = -1; throw std::bad_alloc(); }
    if(allocation_countdown > 0) --allocation_countdown;
    if(void* p = std::malloc(count ? count : 1)) return p;
    throw std::bad_alloc();
}
void* operator new[](std::size_t count) { return ::operator new(count); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
void operator delete[](void* p, std::size_t) noexcept { std::free(p); }
namespace main_graph = foo::nrforge::main;
namespace empty_graph = foo::nrforge::empty;
namespace closed_graph = foo::nrforge::closed;
namespace ext_graph = foo::nrforge::ext;
namespace empty_ext_graph = foo::nrforge::empty_ext;
using ::nrforge::aper::ErrorCode;
using ::nrforge::aper::Limits;
using Bytes = std::vector<std::byte>;
using Mapping = main_graph::EntryMapping;
using Policy = Mapping::criticality_type;
using First = Mapping::wrapper_0;
using Second = Mapping::wrapper_1;
using Third = Mapping::wrapper_2;
using Pair = Mapping::wrapper_3;
using Words = Mapping::wrapper_4;
static_assert(Mapping::rows.size() == 5);
static_assert(Mapping::rows[0].id == 91 && Mapping::rows[1].id == 7 && Mapping::rows[2].id == 42);
static_assert(Mapping::rows[0].expected_criticality == 0 && Mapping::rows[2].expected_criticality == 2);
static_assert(Mapping::rows[0].has_presence && Mapping::rows[0].presence == 0 && Mapping::rows[2].presence == 2);
static_assert(!std::is_same_v<First, Third>);
static_assert(std::is_same_v<decltype(First::value), Mapping::payload_type_0>);
static_assert(std::is_same_v<decltype(Third::value), Mapping::payload_type_2>);
static_assert(main_graph::ContainerMapping::lower_bound == 0 && main_graph::ContainerMapping::upper_bound == 65535);
static_assert(empty_graph::EntryMapping::rows.empty() && empty_graph::EntryMapping::object_set_extensible);
static_assert(!closed_graph::EntryMapping::object_set_extensible);
static_assert(ext_graph::ContainerMapping::lower_bound == 1);
static_assert(!ext_graph::EntryMapping::rows[0].has_presence);
static Bytes bytes(std::initializer_list<unsigned> input) {
    Bytes out; for(auto n : input) out.push_back(static_cast<std::byte>(n)); return out;
}
template<class Result> static void error(const Result& result, ErrorCode code, std::size_t bit) {
    REQUIRE(!result && result.error().code == code && result.error().bit_offset == bit);
    bool wrong_arm = false;
    try { (void)result.value(); } catch(const std::bad_variant_access&) { wrong_arm = true; }
    REQUIRE(wrong_arm);
}
template<class Result> static void encoding(const Result& result, const Bytes& expected) {
    REQUIRE(result && result.value().octets == expected);
    REQUIRE(result.value().octet_count == expected.size());
    REQUIRE(result.value().complete_encoding_bits == expected.size() * 8);
}
template<class Wrapper> static main_graph::Entry entry(std::uint64_t id, typename Policy::Known received, Wrapper value) {
    main_graph::Entry out{}; out.number = id; out.policy.value = received; out.content = std::move(value); return out;
}
static void vectors() {
    const auto first = entry(91, Policy::Known::reject, First{true});
    const auto second = entry(7, Policy::Known::ignore, Second{17});
    const auto third = entry(42, Policy::Known::notify, Third{false});
    encoding(main_graph::encode_entry(first), bytes({0,91,0,1,128}));
    encoding(main_graph::encode_entry(second), bytes({0,7,64,1,17}));
    encoding(main_graph::encode_entry(third), bytes({0,42,128,1,0}));
    Pair pair{}; pair.value.value = 17; pair.value.marker = true;
    const auto compound = entry(50, Policy::Known::reject, pair);
    encoding(main_graph::encode_entry(compound), bytes({0,50,0,3,128,17,128}));
    Words words{}; words.value.elements = {0,255};
    const auto collection = entry(80, Policy::Known::ignore, words);
    encoding(main_graph::encode_entry(collection), bytes({0,80,64,3,128,0,255}));
    auto dp = main_graph::decode_entry(bytes({0,50,0,3,128,17,128}));
    REQUIRE(dp && std::get<Pair>(dp.value().content).value.value == 17 && std::get<Pair>(dp.value().content).value.marker == true);
    auto dw = main_graph::decode_entry(bytes({0,80,64,3,128,0,255}));
    REQUIRE(dw && std::get<Words>(dw.value().content).value.elements == words.value.elements);
    main_graph::Message message{};
    encoding(main_graph::encode_message(message), bytes({0,0,0})); // Policy is not transport validation.
    message.entries.elements = {first,second,third};
    const auto expected = bytes({0,0,3,0,91,0,1,128,0,7,64,1,17,0,42,128,1,0});
    encoding(main_graph::encode_message(message), expected);
    auto decoded = main_graph::decode_message(expected);
    REQUIRE(decoded && decoded.value().entries.elements.size() == 3);
    const auto& items = decoded.value().entries.elements;
    REQUIRE(items[0].number == 91 && items[1].number == 7 && items[2].number == 42);
    REQUIRE(std::get<First>(items[0].content).value && std::get<Second>(items[1].content).value == 17 && !std::get<Third>(items[2].content).value);
    message.entries.elements = {entry(91,Policy::Known::notify,First{true}),entry(91,Policy::Known::ignore,First{false})};
    const auto duplicates = bytes({0,0,2,0,91,128,1,128,0,91,64,1,0});
    encoding(main_graph::encode_message(message), duplicates);
    auto duplicate_decode = main_graph::decode_message(duplicates);
    REQUIRE(duplicate_decode && duplicate_decode.value().entries.elements.size() == 2);
    REQUIRE(duplicate_decode.value().entries.elements[0].policy.value == Policy::Known::notify);
    REQUIRE(duplicate_decode.value().entries.elements[1].policy.value == Policy::Known::ignore);
    REQUIRE(std::get<First>(duplicate_decode.value().entries.elements[0].content).value);
    REQUIRE(!std::get<First>(duplicate_decode.value().entries.elements[1].content).value);
    auto copy = decoded.value(); auto moved = std::move(copy);
    decoded = decltype(decoded)::failure({ErrorCode::invalid_state,0});
    REQUIRE(std::get<Second>(moved.entries.elements[1].content).value == 17);
}
static void unknowns() {
    auto input = bytes({0,0,1,234,96,128,4,222,173,190,239});
    auto result = main_graph::decode_message(input);
    REQUIRE(result && result.value().entries.elements.size() == 1);
    REQUIRE(result.value().entries.elements[0].number == 60000);
    const auto expected_payload = bytes({222,173,190,239});
    REQUIRE(std::get<Mapping::unknown_type>(result.value().entries.elements[0].content).payload == expected_payload);
    auto copy = result.value(); auto moved = std::move(copy);
    input.clear(); input.shrink_to_fit();
    result = decltype(result)::failure({ErrorCode::invalid_state,0});
    REQUIRE(std::get<Mapping::unknown_type>(moved.entries.elements[0].content).payload == expected_payload);
    error(main_graph::encode_message(moved), ErrorCode::constraint_violation,24);
    error(main_graph::encode_entry(moved.entries.elements[0]), ErrorCode::constraint_violation,0);
    const auto empty_input = bytes({0,1,234,96,128,4,222,173,190,239});
    auto empty = empty_graph::decode_message(empty_input);
    REQUIRE(empty && std::get<empty_graph::EntryMapping::unknown_type>(empty.value().items.elements[0].content).payload == expected_payload);
    error(closed_graph::decode_entry(bytes({234,96,128,4,222,173,190,239})), ErrorCode::constraint_violation,18);
    Limits limits{}; limits.max_retained_unknown_payload_octets = 4; limits.max_retained_unknown_records = 1;
    REQUIRE(main_graph::decode_message(bytes({0,0,1,234,96,128,4,222,173,190,239}),limits));
    limits.max_retained_unknown_payload_octets = 3;
    error(main_graph::decode_message(bytes({0,0,1,234,96,128,4,222,173,190,239}),limits),ErrorCode::resource_limit,42);
    limits.max_retained_unknown_payload_octets = 4; limits.max_retained_unknown_records = 0;
    error(main_graph::decode_message(bytes({0,0,1,234,96,128,4,222,173,190,239}),limits),ErrorCode::resource_limit,42);
}
static void malformed_and_budgets() {
    error(main_graph::decode_entry(bytes({0,91,0,1,1})),ErrorCode::nonzero_padding,39);
    error(main_graph::decode_entry(bytes({0,91,0,2,128,0})),ErrorCode::trailing_data,40);
    error(main_graph::decode_entry(bytes({0,7,64,2,17,0})),ErrorCode::trailing_data,40);
    error(main_graph::decode_entry(bytes({0,91,0,0})),ErrorCode::constraint_violation,18);
    error(main_graph::decode_entry(bytes({0,91,192,1,128})),ErrorCode::constraint_violation,16);
    error(main_graph::encode_entry(entry(7,Policy::Known::reject,First{true})),ErrorCode::constraint_violation,0);
    const auto good = bytes({0,91,0,1,128});
    const auto value = entry(91,Policy::Known::reject,First{true});
    Limits limits{}; limits.max_retained_unknown_records = 0; limits.max_retained_unknown_payload_octets = 0;
    limits.max_known_open_staging_octets = 1; limits.max_known_open_depth = 1;
    encoding(main_graph::encode_entry(value,limits),good); REQUIRE(main_graph::decode_entry(good,limits));
    limits.max_known_open_staging_octets = 0;
    error(main_graph::encode_entry(value,limits),ErrorCode::resource_limit,18);
    error(main_graph::decode_entry(good,limits),ErrorCode::resource_limit,18);
    limits.max_known_open_staging_octets = 1; limits.max_known_open_depth = 0;
    error(main_graph::encode_entry(value,limits),ErrorCode::resource_limit,18);
    error(main_graph::decode_entry(good,limits),ErrorCode::resource_limit,18);
    limits = {}; limits.max_input_octets = 5; limits.max_output_octets = 5; limits.max_wire_bits = 40;
    encoding(main_graph::encode_entry(value,limits),good); REQUIRE(main_graph::decode_entry(good,limits));
    limits.max_input_octets = 4; error(main_graph::decode_entry(good,limits),ErrorCode::resource_limit,0);
    limits.max_input_octets = 5; limits.max_output_octets = 4;
    error(main_graph::encode_entry(value,limits),ErrorCode::resource_limit,18);
    limits.max_output_octets = 5; limits.max_wire_bits = 39;
    error(main_graph::encode_entry(value,limits),ErrorCode::resource_limit,18);
    error(main_graph::decode_entry(good,limits),ErrorCode::resource_limit,18);
    for(std::size_t n = 0; n < good.size(); ++n) REQUIRE(!main_graph::decode_entry(std::span(good).first(n)));
    auto trailing = good; trailing.push_back(std::byte{0});
    error(main_graph::decode_entry(trailing),ErrorCode::trailing_data,40);
    Words words{}; words.value.elements = {0,255};
    main_graph::Message nested{}; nested.entries.elements = {entry(80,Policy::Known::ignore,words)};
    const auto nested_bytes = bytes({0,0,1,0,80,64,3,128,0,255});
    limits = {}; limits.max_collection_elements = 3;
    encoding(main_graph::encode_message(nested,limits),nested_bytes); REQUIRE(main_graph::decode_message(nested_bytes,limits));
    limits.max_collection_elements = 2;
    error(main_graph::encode_message(nested,limits),ErrorCode::resource_limit,42);
    error(main_graph::decode_message(nested_bytes,limits),ErrorCode::resource_limit,56);
}
static void extension_transport() {
    ext_graph::Entry entry{}; entry.tag = 13;
    entry.received_policy.value = ext_graph::EntryMapping::criticality_type::Known::ignore;
    entry.extension_content = ext_graph::EntryMapping::wrapper_0{true};
    ext_graph::Message message{}; message.additions.elements.push_back(entry);
    const auto expected = bytes({0,0,0,13,64,1,128});
    encoding(ext_graph::encode_message(message),expected);
    auto result = ext_graph::decode_message(expected);
    REQUIRE(result && std::get<ext_graph::EntryMapping::wrapper_0>(result.value().additions.elements[0].extension_content).value);
    auto empty = empty_ext_graph::decode_message(expected);
    REQUIRE(empty && std::get<empty_ext_graph::EntryMapping::unknown_type>(empty.value().additions.elements[0].extension_content).payload == bytes({128}));
    error(ext_graph::encode_message(ext_graph::Message{}),ErrorCode::constraint_violation,0);
}
static void sticky_helpers() {
    const auto mismatched = entry(7,Policy::Known::reject,First{true});
    const auto valid = entry(91,Policy::Known::reject,First{true});
    auto encoded = ::nrforge::aper::encode_complete(valid, Limits{}, [&](::nrforge::aper::FieldWriter& fields) {
        const auto first = main_graph::put_entry(fields,mismatched);
        REQUIRE(!first && first.error().code == ErrorCode::constraint_violation && first.error().bit_offset == 0);
        const auto next = main_graph::put_entry(fields,valid);
        REQUIRE(!next && next.error().code == first.error().code && next.error().bit_offset == first.error().bit_offset);
        return ::nrforge::aper::Result<void>::success();
    });
    error(encoded,ErrorCode::constraint_violation,0);
    const auto input = bytes({0,91,0,1,1});
    auto decoded = ::nrforge::aper::decode_complete<main_graph::Entry>(input, Limits{}, [&](::nrforge::aper::FieldReader& fields) {
        const auto first = main_graph::get_entry(fields);
        REQUIRE(!first && first.error().code == ErrorCode::nonzero_padding && first.error().bit_offset == 39);
        const auto next = main_graph::get_entry(fields);
        REQUIRE(!next && next.error().code == first.error().code && next.error().bit_offset == first.error().bit_offset);
        return ::nrforge::aper::Result<main_graph::Entry>::success(valid);
    });
    error(decoded,ErrorCode::nonzero_padding,39);
}
static void allocation_failures() {
    Pair pair{}; pair.value.value = 17; pair.value.marker = true;
    main_graph::Message object{}; object.entries.elements.push_back(entry(50,Policy::Known::reject,pair));
    const auto input = bytes({0,0,1,0,50,0,3,128,17,128});
    for(bool encode : {false,true}) {
        long point;
        for(point = 0; point < 1000; ++point) {
            allocation_countdown = point;
            if(encode) {
                auto result = main_graph::encode_message(object); allocation_countdown = -1;
                if(result) { encoding(result,input); break; }
                REQUIRE(result.error().code == ErrorCode::allocation_failure);
            } else {
                auto result = main_graph::decode_message(input); allocation_countdown = -1;
                if(result) { REQUIRE(std::get<Pair>(result.value().entries.elements[0].content).value.marker == true); break; }
                REQUIRE(result.error().code == ErrorCode::allocation_failure);
            }
        }
        REQUIRE(point > 0 && point < 1000);
    }
}
int main() {
    vectors(); unknowns(); malformed_and_budgets(); extension_transport(); sticky_helpers(); allocation_failures();
    std::puts("PASS IOC exact vectors, ordered dispatch, owned unknowns, strict known frames, budgets and allocation failures");
}
