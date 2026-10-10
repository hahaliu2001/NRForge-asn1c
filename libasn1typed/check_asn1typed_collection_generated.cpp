#include <runtime.hpp>
#include <fstream>
#include <string>
#include <sequence_extensions.hpp>
#include "types.hpp"
#include "mapping.hpp"
#include "codec.hpp"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <new>
#include <type_traits>

#define REQUIRE(x) do { if(!(x)) { std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #x); std::abort(); } } while(0)
namespace c = collectiontest;
using namespace nrforge::aper;
namespace {
long allocation_countdown = -1;
static_assert(std::is_same_v<decltype(c::Bits::elements), std::vector<bool>>);
static_assert(std::is_same_v<c::Pairs_aper::element_type, c::Pair>);
static_assert(std::is_same_v<c::Pairs_aper::element_payload_mapping, c::Pair_aper>);
static_assert(std::is_same_v<c::Words_aper::element_type, c::Word>);
static_assert(std::is_same_v<c::Words_aper::element_payload_mapping, c::Word_aper>);
static_assert(std::is_same_v<c::Nested_aper::element_type, c::Bits>);
static_assert(c::Bits_aper::elements_member == &c::Bits::elements);
static_assert(c::Bits_aper::lower_bound == 0 && c::Bits_aper::upper_bound == 3);
static_assert(c::Bits_aper::length_bits == 2 && !c::Bits_aper::length_align_to_octet);
static_assert(c::FixedThree_aper::length_bits == 0 && !c::FixedThree_aper::length_align_to_octet);
static_assert(c::Empty_aper::length_bits == 0 && c::Empty_aper::upper_bound == 0);
static_assert(c::ShortEight_aper::length_bits == 8 && !c::ShortEight_aper::length_align_to_octet);
static_assert(c::OctetEight_aper::length_bits == 8 && c::OctetEight_aper::length_align_to_octet);
static_assert(c::WideSixteen_aper::length_bits == 16 && c::WideSixteen_aper::length_align_to_octet);
static_assert(c::FullSixteen_aper::upper_bound == 65535 && !c::FullSixteen_aper::extensible);

template<class R> void error(const R& result, ErrorCode code, std::size_t offset) {
    REQUIRE(!result);
    if(result.error().code != code || result.error().bit_offset != offset)
        std::fprintf(stderr, "expected (%d,%zu), got (%d,%zu)\n", static_cast<int>(code), offset,
                     static_cast<int>(result.error().code), result.error().bit_offset);
    REQUIRE(result.error().code == code && result.error().bit_offset == offset);
}
std::vector<std::byte> octets(std::initializer_list<unsigned> values) {
    std::vector<std::byte> result;
    for(auto value : values) result.push_back(static_cast<std::byte>(value));
    return result;
}
// Independent count/bit model. Never invokes the generated mapping/runtime.
struct Bits {
    std::vector<bool> bits;
    void put(std::uint64_t value, unsigned width) {
        for(unsigned i = width; i > 0; --i) bits.push_back(((value >> (i - 1)) & 1u) != 0);
    }
    void align() { while(bits.size() % 8) bits.push_back(false); }
    void count(std::size_t count, std::size_t lower, std::size_t upper) {
        REQUIRE(count >= lower && count <= upper);
        const auto range = upper - lower + 1;
        if(range == 1) return;
        unsigned width = 0;
        if(range < 256) { for(auto v = range - 1; v; v >>= 1) ++width; }
        else { align(); width = range == 256 ? 8u : 16u; }
        put(count - lower, width);
    }
    void word(std::uint64_t value) { align(); put(value, 8); }
    std::vector<std::byte> finish() {
        if(bits.empty()) bits.resize(8, false);
        align();
        std::vector<std::byte> out(bits.size() / 8, std::byte{0});
        for(std::size_t i = 0; i < bits.size(); ++i)
            if(bits[i]) out[i / 8] |= static_cast<std::byte>(0x80u >> (i % 8));
        return out;
    }
};
template<class T> void model_list(Bits& bits, const T& value, std::size_t lower, std::size_t upper) {
    bits.count(value.elements.size(), lower, upper);
    for(bool element : value.elements) bits.put(element, 1);
}
void model(Bits& bits, const c::Bits& value) { model_list(bits, value, 0, 3); }
void model(Bits& bits, const c::Nested& value) {
    bits.count(value.elements.size(), 0, 2);
    for(const auto& child : value.elements) model(bits, child);
}
void model(Bits& bits, const c::Pair& value) {
    bits.put(value.marker.has_value(), 1); bits.word(value.value);
    if(value.marker) bits.put(*value.marker, 1);
}
void model(Bits& bits, const c::Pairs& value) {
    bits.count(value.elements.size(), 1, 2);
    for(const auto& child : value.elements) model(bits, child);
}
void model(Bits& bits, const c::Picks& value) {
    bits.count(value.elements.size(), 0, 3);
    for(const auto& pick : value.elements) {
        bits.put(pick.index(), 1);
        if(pick.index() == 0) bits.put(std::get<c::Pick_flag>(pick).value, 1);
        else model(bits, std::get<c::Pick_pair>(pick).value);
    }
}
bool equal(const c::Pair& a, const c::Pair& b) { return a.value == b.value && a.marker == b.marker; }
bool equal(const c::Picks& a, const c::Picks& b) {
    if(a.elements.size() != b.elements.size()) return false;
    for(std::size_t i = 0; i < a.elements.size(); ++i) {
        const auto& x = a.elements[i]; const auto& y = b.elements[i];
        if(x.index() != y.index()) return false;
        if(x.index() == 0) { if(std::get<c::Pick_flag>(x).value != std::get<c::Pick_flag>(y).value) return false; }
        else if(!equal(std::get<c::Pick_pair>(x).value, std::get<c::Pick_pair>(y).value)) return false;
    }
    return true;
}
template<class T, class Encoder, class Decoder, class Equal>
void check(const T& value, Encoder encode, Decoder decode, const std::vector<std::byte>& expected, Equal equals) {
    auto encoded = encode(value, Limits{});
    REQUIRE(encoded && encoded.value().octets == expected && encoded.value().octet_count == expected.size());
    auto decoded = decode(expected, Limits{});
    REQUIRE(decoded && equals(value, decoded.value()));
    auto input = expected;
    auto owned = decode(input, Limits{});
    REQUIRE(owned);
    input.clear(); input.shrink_to_fit();
    T copy = owned.value();
    T moved = std::move(owned).value();
    REQUIRE(equals(copy, moved));
    if(expected.size() <= 128) {
        for(std::size_t n = 0; n < expected.size(); ++n)
            REQUIRE(!decode(std::span<const std::byte>(expected.data(), n), Limits{}));
    } else {
        for(auto n : {std::size_t{0}, std::size_t{1}, expected.size() / 2, expected.size() - 1})
            REQUIRE(!decode(std::span<const std::byte>(expected.data(), n), Limits{}));
    }
    auto trailing = expected; trailing.push_back(std::byte{0xff});
    error(decode(trailing, Limits{}), ErrorCode::trailing_data, expected.size() * 8);
    Limits limits;
    limits.max_input_octets = expected.size(); REQUIRE(decode(expected, limits));
    limits.max_input_octets = expected.size() - 1; REQUIRE(!decode(expected, limits));
    limits = {}; limits.max_output_octets = expected.size(); REQUIRE(encode(value, limits));
    limits.max_output_octets = expected.size() - 1; REQUIRE(!encode(value, limits));
    limits = {}; limits.max_wire_bits = expected.size() * 8; REQUIRE(encode(value, limits) && decode(expected, limits));
    --limits.max_wire_bits; REQUIRE(!encode(value, limits) && !decode(expected, limits));
}
template<class T, class Encoder, class Decoder>
void bool_check(const T& value, Encoder encode, Decoder decode, std::size_t lower, std::size_t upper) {
    Bits reference; model_list(reference, value, lower, upper);
    check(value, encode, decode, reference.finish(), [](const T& a, const T& b) { return a.elements == b.elements; });
}
void vectors() {
    for(std::size_t n = 0; n <= 3; ++n) {
        for(unsigned mask = 0; mask < (1u << n); ++mask) {
            c::Bits value;
            for(std::size_t i = 0; i < n; ++i) value.elements.push_back((mask & (1u << i)) != 0);
            bool_check(value, c::encode_bits, c::decode_bits, 0, 3);
        }
    }
    const c::Bits example{{true, false, true}};
    REQUIRE(c::encode_bits(example).value().octets == octets({0xe8}));
    const c::Words words{{0, 17, 255}};
    Bits word_model; word_model.count(words.elements.size(), 0, 3);
    for(const auto value : words.elements) word_model.word(value);
    REQUIRE(word_model.finish() == octets({0xc0, 0x00, 0x11, 0xff}));
    check(words, c::encode_words, c::decode_words, word_model.finish(), [](const c::Words& a, const c::Words& b) {
        return a.elements == b.elements;
    });
    const c::Nested nested{{c::Bits{{true}}, c::Bits{{false, true}}}};
    Bits reference; model(reference, nested);
    REQUIRE(reference.finish() == octets({0x9c, 0x80}));
    check(nested, c::encode_nested, c::decode_nested, reference.finish(), [](const c::Nested& a, const c::Nested& b) {
        if(a.elements.size() != b.elements.size()) return false;
        for(std::size_t i = 0; i < a.elements.size(); ++i) if(a.elements[i].elements != b.elements[i].elements) return false;
        return true;
    });
    const c::Pairs pairs{{c::Pair{12, {}}, c::Pair{34, true}}};
    Bits pair_model; model(pair_model, pairs);
    REQUIRE(pair_model.finish() == octets({0x80, 0x0c, 0x80, 0x22, 0x80}));
    check(pairs, c::encode_pairs, c::decode_pairs, pair_model.finish(), [](const c::Pairs& a, const c::Pairs& b) {
        if(a.elements.size() != b.elements.size()) return false;
        for(std::size_t i = 0; i < a.elements.size(); ++i) if(!equal(a.elements[i], b.elements[i])) return false;
        return true;
    });
    const c::Picks picks{{c::Pick{c::Pick_flag{true}}, c::Pick{c::Pick_pair{c::Pair{7, false}}}}};
    Bits pick_model; model(pick_model, picks);
    check(picks, c::encode_picks, c::decode_picks, pick_model.finish(), [](const c::Picks& a, const c::Picks& b) { return equal(a, b); });
    bool_check(c::FixedThree{{true, false, true}}, c::encode_fixed_three, c::decode_fixed_three, 3, 3);
    bool_check(c::Empty{}, c::encode_empty, c::decode_empty, 0, 0);
    REQUIRE(c::encode_empty({}).value().octets == octets({0x00}));
    REQUIRE(c::encode_empty({}).value().empty_encoding_substitution);
    c::ZeroBitTriple zero_bit_triple; zero_bit_triple.elements.resize(3);
    check(zero_bit_triple, c::encode_zero_bit_triple, c::decode_zero_bit_triple, octets({0x00}),
          [](const c::ZeroBitTriple& a, const c::ZeroBitTriple& b) { return a.elements.size() == b.elements.size(); });
    REQUIRE(c::encode_zero_bit_triple(zero_bit_triple).value().empty_encoding_substitution);
    for(const auto n : {std::size_t{0}, std::size_t{1}, std::size_t{127}, std::size_t{128}, std::size_t{254}}) {
        c::ShortEight value; value.elements.assign(n, true);
        bool_check(value, c::encode_short_eight, c::decode_short_eight, 0, 254);
    }
    for(const auto n : {std::size_t{0}, std::size_t{1}, std::size_t{255}}) {
        c::OctetEight value; value.elements.assign(n, true);
        bool_check(value, c::encode_octet_eight, c::decode_octet_eight, 0, 255);
    }
    for(const auto n : {std::size_t{0}, std::size_t{1}, std::size_t{256}}) {
        c::WideSixteen value; value.elements.assign(n, true);
        bool_check(value, c::encode_wide_sixteen, c::decode_wide_sixteen, 0, 256);
    }
    for(const auto n : {std::size_t{0}, std::size_t{257}, std::size_t{65535}}) {
        c::FullSixteen value; value.elements.assign(n, false);
        bool_check(value, c::encode_full_sixteen, c::decode_full_sixteen, 0, 65535);
    }
    REQUIRE(c::encode_prefix_short({true, c::ShortEight{{true}}}).value().octets == octets({0x80, 0xc0}));
    REQUIRE(c::encode_prefix_octet({true, c::OctetEight{{true}}}).value().octets == octets({0x80, 0x01, 0x80}));
    REQUIRE(c::encode_prefix_wide({true, c::WideSixteen{{true}}}).value().octets == octets({0x80, 0x00, 0x01, 0x80}));
    auto prefix_short = c::decode_prefix_short(octets({0x80, 0xc0}));
    auto prefix_octet = c::decode_prefix_octet(octets({0x80, 0x01, 0x80}));
    auto prefix_wide = c::decode_prefix_wide(octets({0x80, 0x00, 0x01, 0x80}));
    REQUIRE(prefix_short && prefix_octet && prefix_wide && prefix_short.value().prefix && prefix_octet.value().prefix && prefix_wide.value().prefix);
    REQUIRE(prefix_short.value().values.elements == std::vector<bool>{true});
    REQUIRE(prefix_octet.value().values.elements == std::vector<bool>{true});
    REQUIRE(prefix_wide.value().values.elements == std::vector<bool>{true});
    c::Envelope envelope{true, example, pairs, nested, {}};
    Bits envelope_model;
    envelope_model.put(0, 1); // root-only extension bit
    envelope_model.put(1, 1); // pairs presence
    envelope_model.put(envelope.prefix, 1);
    model(envelope_model, envelope.bits);
    model(envelope_model, *envelope.pairs);
    model(envelope_model, envelope.nested);
    check(envelope, c::encode_envelope, c::decode_envelope, envelope_model.finish(), [](const c::Envelope& a, const c::Envelope& b) {
        if(a.prefix != b.prefix || a.bits.elements != b.bits.elements || a.pairs.has_value() != b.pairs.has_value() ||
           a.nested.elements.size() != b.nested.elements.size()) return false;
        if(a.pairs) {
            if(a.pairs->elements.size() != b.pairs->elements.size()) return false;
            for(std::size_t i = 0; i < a.pairs->elements.size(); ++i)
                if(!equal(a.pairs->elements[i], b.pairs->elements[i])) return false;
        }
        for(std::size_t i = 0; i < a.nested.elements.size(); ++i)
            if(a.nested.elements[i].elements != b.nested.elements[i].elements) return false;
        return b.sequence_extensions.received_bitmap_bit_count == 0 && b.sequence_extensions.unknown_additions.empty();
    });
}
void errors_and_budgets() {
    error(c::encode_bits(c::Bits{{true, false, true, false}}), ErrorCode::constraint_violation, 0);
    error(c::encode_pairs(c::Pairs{}), ErrorCode::constraint_violation, 0);
    error(c::encode_fixed_three(c::FixedThree{}), ErrorCode::constraint_violation, 0);
    error(c::decode_nested(octets({0xc0})), ErrorCode::constraint_violation, 0);
    error(c::decode_prefix_octet(octets({0x81, 0x01, 0x80})), ErrorCode::nonzero_padding, 7);
    error(c::decode_prefix_wide(octets({0x80, 0x01, 0x01})), ErrorCode::constraint_violation, 1);
    error(c::decode_bits(octets({0xe9})), ErrorCode::nonzero_padding, 7);
    const c::Nested nested{{c::Bits{{true}}, c::Bits{{false, true}}}};
    const auto encoded = octets({0x9c, 0x80});
    Limits limits; limits.max_collection_elements = 5;
    REQUIRE(c::encode_nested(nested, limits) && c::decode_nested(encoded, limits));
    limits.max_collection_elements = 4;
    error(c::encode_nested(nested, limits), ErrorCode::resource_limit, 5);
    error(c::decode_nested(encoded, limits), ErrorCode::resource_limit, 5);
    DecodeContext dc(limits);
    auto reader = BitReader::make(encoded, dc); REQUIRE(reader);
    FieldReader field(reader.value());
    error(c::compound_codec::get_Nested(field), ErrorCode::resource_limit, 5);
    REQUIRE(reader.value().cursor_bit() == 5 && dc.collection_elements() == 3 && dc.wire_bits() == 5);
    error(c::compound_codec::get_Empty(field), ErrorCode::resource_limit, 5);
    EncodeContext ec(limits); BitWriter writer(ec); FieldWriter out(writer);
    error(c::compound_codec::put_Nested(out, nested), ErrorCode::resource_limit, 5);
    REQUIRE(writer.cursor_bit() == 5 && ec.collection_elements() == 3 && ec.wire_bits() == 5);
    error(c::compound_codec::put_Empty(out, c::Empty{}), ErrorCode::resource_limit, 5);
    limits.max_collection_elements = 2;
    error(c::encode_fixed_three(c::FixedThree{{false, false, false}}, limits), ErrorCode::resource_limit, 0);
    error(c::decode_fixed_three(octets({0x00}), limits), ErrorCode::resource_limit, 0);
    limits.max_collection_elements = 0;
    REQUIRE(c::encode_empty({}, limits) && c::decode_empty(octets({0x00}), limits));
    c::ZeroBitTriple zero_bit_triple; zero_bit_triple.elements.resize(3);
    limits.max_collection_elements = 3;
    REQUIRE(c::encode_zero_bit_triple(zero_bit_triple, limits));
    REQUIRE(c::decode_zero_bit_triple(octets({0x00}), limits).value().elements.size() == 3);
    limits.max_collection_elements = 2;
    error(c::encode_zero_bit_triple(zero_bit_triple, limits), ErrorCode::resource_limit, 0);
    error(c::decode_zero_bit_triple(octets({0x00}), limits), ErrorCode::resource_limit, 0);
    DecodeContext zero_context(limits);
    const auto zero_wire = octets({0x00});
    auto zero_reader = BitReader::make(zero_wire, zero_context); REQUIRE(zero_reader);
    FieldReader zero_field(zero_reader.value());
    error(c::compound_codec::get_ZeroBitTriple(zero_field), ErrorCode::resource_limit, 0);
    REQUIRE(zero_context.collection_elements() == 0 && zero_reader.value().cursor_bit() == 0);
}
void allocation_and_ownership() {
    const auto encoded = octets({0x9c, 0x80});
    unsigned failures = 0;
    for(long point = 0; point < 20; ++point) {
        allocation_countdown = point;
        auto result = c::decode_nested(encoded);
        allocation_countdown = -1;
        if(result) { REQUIRE(result.value().elements.size() == 2); break; }
        REQUIRE(result.error().code == ErrorCode::allocation_failure); ++failures;
    }
    REQUIRE(failures >= 3);
    const auto bits = octets({0xe8});
    DecodeContext context;
    auto reader = BitReader::make(bits, context); REQUIRE(reader);
    FieldReader field(reader.value());
    allocation_countdown = 0;
    auto result = c::compound_codec::get_Bits(field);
    allocation_countdown = -1;
    error(result, ErrorCode::allocation_failure, 2);
    REQUIRE(context.collection_elements() == 3 && reader.value().cursor_bit() == 2);
    error(c::compound_codec::get_Empty(field), ErrorCode::allocation_failure, 2);
    // Unknown extension bytes within a collection element remain independently owned.
    auto wire = octets({0x60, 0x07, 0x01, 0x01, 0x80});
    auto extended = c::decode_ext_pairs(wire); REQUIRE(extended && extended.value().elements.size() == 1);
    wire.clear(); wire.shrink_to_fit();
    auto copy = extended.value();
    auto moved = std::move(extended).value();
    auto& sidecar = moved.elements[0].sequence_extensions;
    REQUIRE(moved.elements[0].value == 7 && sidecar.received_bitmap_bit_count == 1);
    REQUIRE(sidecar.unknown_additions.size() == 1 && sidecar.unknown_additions[0].addition_index == 0);
    REQUIRE(sidecar.unknown_additions[0].payload_octets == octets({0x80}));
    sidecar.unknown_additions[0].payload_octets[0] = std::byte{0};
    REQUIRE(copy.elements[0].sequence_extensions.unknown_additions[0].payload_octets == octets({0x80}));
    error(c::encode_ext_pairs(copy), ErrorCode::constraint_violation, 2);
    copy.elements[0].sequence_extensions = {};
    REQUIRE(c::encode_ext_pairs(copy).value().octets == octets({0x40, 0x07}));
}
void fragments() {
    static_assert(c::Large_aper::fragmented_supported);
    for(std::size_t count : {1u,127u,128u,16383u,16384u,32768u,65535u,65536u}) {
        c::Large value; value.elements.resize(count,false);
        // Independent X.691 determinant/payload model, all BOOLEAN values zero.
        std::vector<std::byte> model;
        std::size_t remaining=count;
        bool more;
        do {
            more=remaining >= 16384;
            const auto chunk=more ? (remaining/16384)*16384 : remaining;
            if(more) model.push_back(static_cast<std::byte>(0xc0u+chunk/16384));
            else if(chunk < 128) model.push_back(static_cast<std::byte>(chunk));
            else { model.push_back(static_cast<std::byte>(0x80u+(chunk>>8))); model.push_back(static_cast<std::byte>(chunk&255)); }
            model.resize(model.size()+(chunk+7)/8,std::byte{0});
            remaining-=chunk;
        } while(more);
        auto encoded=c::encode_large(value); REQUIRE(encoded && encoded.value().octets==model);
        if(const auto directory=std::getenv("FRAGMENT_VECTOR_DIR")) {
            std::ofstream output(::std::string(directory)+"/"+::std::to_string(count)+".bin",::std::ios::binary);
            output.write(reinterpret_cast<const char*>(encoded.value().octets.data()),static_cast<::std::streamsize>(encoded.value().octets.size())); REQUIRE(output.good());
        }
        auto decoded=c::decode_large(model); REQUIRE(decoded && decoded.value().elements==value.elements);
        if(count >= 16384 && count % 16384 == 0) {
            REQUIRE(model.back()==std::byte{0});
            model.pop_back(); auto bad=c::decode_large(model); REQUIRE(!bad && bad.error().code==ErrorCode::truncated_input);
        }
        Limits limits; limits.max_collection_elements=count-1;
        auto bad=c::encode_large(value,limits); REQUIRE(!bad && bad.error().code==ErrorCode::resource_limit);
        limits={}; limits.max_output_octets=encoded.value().octets.size()-1;
        bad=c::encode_large(value,limits); REQUIRE(!bad && bad.error().code==ErrorCode::resource_limit);
        limits={}; limits.max_wire_bits=encoded.value().complete_encoding_bits-1;
        bad=c::encode_large(value,limits); REQUIRE(!bad && bad.error().code==ErrorCode::resource_limit);
        allocation_countdown=0; auto oom=c::decode_large(encoded.value().octets); allocation_countdown=-1;
        REQUIRE(!oom && oom.error().code==ErrorCode::allocation_failure);
    }
    static_assert(c::Huge_aper::upper_bound == 67108864);
    // Repeated 64K fragments and a nonzero tail, independent determinant model.
    for(std::size_t count : {65537u,131072u}) {
        c::Huge value; value.elements.resize(count,false);
        std::vector<std::byte> model;
        std::size_t remaining=count;
        while(remaining >= 65536) {
            model.push_back(std::byte{0xc4});
            model.resize(model.size()+8192,std::byte{0});
            remaining-=65536;
        }
        model.push_back(static_cast<std::byte>(remaining));
        if(remaining) model.push_back(std::byte{0});
        Limits large_limits; large_limits.max_collection_elements=count;
        auto encoded=c::encode_huge(value,large_limits); REQUIRE(encoded && encoded.value().octets==model);
        auto decoded=c::decode_huge(model,large_limits); REQUIRE(decoded && decoded.value().elements==value.elements);
    }
    Limits huge_limits; huge_limits.max_collection_elements=65535;
    // A valid maximum fragment declaration is rejected by budget before reserve.
    const auto declared=octets({0xc4});
    allocation_countdown=0;
    auto denied=c::decode_huge(declared,huge_limits);
    allocation_countdown=-1;
    REQUIRE(!denied && denied.error().code==ErrorCode::resource_limit);
    error(c::encode_large({}),ErrorCode::constraint_violation,0);
    auto invalid=c::decode_large(octets({0xc0})); REQUIRE(!invalid && invalid.error().code==ErrorCode::constraint_violation);
}
} // namespace
void *operator new(std::size_t size) {
    if(allocation_countdown == 0) { allocation_countdown = -1; throw std::bad_alloc(); }
    if(allocation_countdown > 0) --allocation_countdown;
    if(void *pointer = std::malloc(size ? size : 1)) return pointer;
    throw std::bad_alloc();
}
void *operator new[](std::size_t size) { return ::operator new(size); }
void operator delete(void *pointer) noexcept { std::free(pointer); }
void operator delete[](void *pointer) noexcept { std::free(pointer); }
void operator delete(void *pointer, std::size_t) noexcept { std::free(pointer); }
void operator delete[](void *pointer, std::size_t) noexcept { std::free(pointer); }
int main() {
    vectors(); errors_and_budgets(); allocation_and_ownership(); fragments();
    std::puts("PASS N8 generated collections, count boundaries, owned values and shared budgets");
}
