#include <runtime.hpp>
#include <sequence_extensions.hpp>
#include "types.hpp"
#include "mapping.hpp"
#include "codec.hpp"
#include "integer_types.hpp"
#include "integer_mapping.hpp"
#include "integer_codec.hpp"
#include <bit>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <type_traits>
#include <vector>

#define REQUIRE(x) do { if(!(x)) { std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #x); std::abort(); } } while(0)
namespace c = integertests;
namespace s = integeronly;
namespace r = ::nrforge::aper;
static_assert(std::is_same_v<c::SignedSmall, std::int64_t>);
static_assert(std::is_same_v<c::Nonzero, std::uint64_t>);
static_assert(std::is_same_v<c::FullSigned64, std::int64_t>);
static_assert(std::is_same_v<decltype(c::Pick_negative::value), std::int64_t>);
static_assert(std::is_same_v<decltype(c::Pick_narrow::value), c::Tiny>);
static_assert(std::is_same_v<decltype(c::Packet::inline_signed), std::int64_t>);
static_assert(c::FullSigned64_aper::root_bits == 64);
static_assert(c::FullSigned64_aper::cardinality_is_full);
static_assert(c::FullSigned64_aper::cardinality_minus_one == UINT64_MAX);
static_assert(c::Constant_constraint::lower_bound == -7 && c::Constant_constraint::upper_bound == -7);
static_assert(c::MinimumConstant_constraint::lower_bound == INT64_MIN);
static_assert(c::MaximumConstant_constraint::upper_bound == INT64_MAX);
static_assert(s::Whole_aper::cardinality_is_full);

// Independent bit append model, with no runtime primitive or mapping calls.
struct Model {
    std::vector<std::byte> octets;
    std::size_t bits = 0;
    void bit(bool value) {
        if(bits % 8 == 0) octets.push_back(std::byte{0});
        if(value) octets.back() |= std::byte(0x80u >> (bits % 8));
        ++bits;
    }
    void number(std::uint64_t value, unsigned width) {
        for(unsigned i = width; i != 0; --i) bit(((value >> (i - 1)) & 1u) != 0);
    }
    void align() { while(bits % 8) bit(false); }
    static unsigned width(std::uint64_t value) { unsigned n = 0; while(value) { ++n; value >>= 1; } return n; }
    void integer_offset(std::uint64_t offset, std::uint64_t last) {
        if(last == 0) return;
        if(last < 255) number(offset, width(last));
        else if(last == 255) { align(); number(offset, 8); }
        else if(last <= 65535) { align(); number(offset, 16); }
        else {
            const unsigned maximum = (width(last) + 7) / 8;
            unsigned actual = (width(offset) + 7) / 8; if(actual == 0) actual = 1;
            number(actual - 1, width(maximum - 1)); align(); number(offset, actual * 8);
        }
    }
    void uint(std::uint64_t value, std::uint64_t lo, std::uint64_t hi) { integer_offset(value - lo, hi - lo); }
    static std::uint64_t rank(std::int64_t value) { return std::bit_cast<std::uint64_t>(value) ^ (UINT64_C(1) << 63); }
    void sint(std::int64_t value, std::int64_t lo, std::int64_t hi) { integer_offset(rank(value) - rank(lo), rank(hi) - rank(lo)); }
    std::vector<std::byte> complete() { if(bits == 0) bit(false); align(); return octets; }
};
template<class R> void error(const R& result, r::ErrorCode code, std::size_t offset) {
    REQUIRE(!result); REQUIRE(result.error().code == code); REQUIRE(result.error().bit_offset == offset);
}
template<class T, class Encode, class Decode> void scalar(T value, T lo, T hi, Encode encode, Decode decode) {
    Model reference;
    if constexpr(std::is_signed_v<T>) reference.sint(value, lo, hi); else reference.uint(value, lo, hi);
    const auto field_bits = reference.bits;
    const auto wanted = reference.complete();
    auto encoded = encode(value); REQUIRE(encoded); REQUIRE(encoded.value().octets == wanted);
    REQUIRE(encoded.value().last_field_end_bit == field_bits);
    REQUIRE(encoded.value().empty_encoding_substitution == (field_bits == 0));
    REQUIRE(encoded.value().final_padding_bits == (field_bits == 0 ? 0 : (8 - field_bits % 8) % 8));
    REQUIRE(encoded.value().octet_count == wanted.size());
    auto decoded = decode(wanted); REQUIRE(decoded && decoded.value() == value);
    for(std::size_t n = 0; n < wanted.size(); ++n) {
        auto truncated = decode(std::span<const std::byte>(wanted.data(), n));
        error(truncated, r::ErrorCode::truncated_input, n * 8);
    }
    auto trailing = wanted; trailing.push_back(std::byte{0});
    error(decode(trailing), r::ErrorCode::trailing_data, wanted.size() * 8);
    r::Limits limits; limits.max_input_octets = wanted.size(); limits.max_output_octets = wanted.size(); limits.max_wire_bits = wanted.size() * 8;
    REQUIRE(encode(value, limits)); REQUIRE(decode(wanted, limits));
    limits.max_output_octets = wanted.size() - 1; error(encode(value, limits), r::ErrorCode::resource_limit, 0);
    limits.max_output_octets = wanted.size(); limits.max_input_octets = wanted.size() - 1;
    error(decode(wanted, limits), r::ErrorCode::resource_limit, 0);
    limits.max_input_octets = wanted.size(); limits.max_wire_bits = wanted.size() * 8 - 1;
    const std::size_t offset = field_bits < wanted.size() * 8 ? field_bits : 0;
    error(encode(value, limits), r::ErrorCode::resource_limit, offset);
    error(decode(wanted, limits), r::ErrorCode::resource_limit, offset);
}
#define SCALAR(Type, base, value, lo, hi) scalar<c::Type>(value, lo, hi, [](c::Type v, const r::Limits& l = {}) { return c::encode_##base(v,l); }, [](std::span<const std::byte> v, const r::Limits& l = {}) { return c::decode_##base(v,l); })

int main() {
    // Function names below are normalized Naming spellings, never wire selectors.
    auto signed_small_encode = [](c::SignedSmall v, const r::Limits& l = {}) { return c::encode_signed_small(v,l); };
    auto signed_small_decode = [](std::span<const std::byte> v, const r::Limits& l = {}) { return c::decode_signed_small(v,l); };
    for(std::int64_t v : {-5, -1, 0, 1, 5}) scalar<c::SignedSmall>(v,-5,5,signed_small_encode,signed_small_decode);
    for(std::uint64_t v : {UINT64_C(5),UINT64_C(6),UINT64_C(10)}) SCALAR(Nonzero,nonzero,v,5,10);
    SCALAR(Constant,constant,-7,-7,-7);
    for(std::uint64_t v : {UINT64_C(0),UINT64_C(1),UINT64_C(127),UINT64_C(254)}) SCALAR(Small255,small_255,v,0,254);
    for(std::uint64_t v : {UINT64_C(10),UINT64_C(11),UINT64_C(137),UINT64_C(265)}) SCALAR(Octet256,octet_256,v,10,265);
    for(std::uint64_t v : {UINT64_C(100),UINT64_C(101),UINT64_C(355),UINT64_C(356)}) SCALAR(Wide257,wide_257,v,100,356);
    for(std::uint64_t v : {UINT64_C(100),UINT64_C(355),UINT64_C(356),UINT64_C(65635)}) SCALAR(Full65536,full_65536,v,100,65635);
    for(std::uint64_t v : {UINT64_C(100),UINT64_C(355),UINT64_C(356),UINT64_C(65635),UINT64_C(65636)}) SCALAR(Large65537,large_65537,v,100,65636);
    for(std::int64_t v : {INT64_C(-2147483648),INT64_C(-1),INT64_C(0),INT64_C(2147483647)}) SCALAR(Signed32,signed_32,v,INT64_C(-2147483648),INT64_C(2147483647));
    for(std::uint64_t v : {UINT64_C(0),UINT64_C(255),UINT64_C(256),UINT64_C(4294967295),UINT64_C(4294967296)}) SCALAR(Beyond32,beyond_32,v,0,UINT64_C(4294967296));
    for(std::uint64_t v : {UINT64_C(0),UINT64_C(65535),UINT64_C(65536),UINT64_C(4294967296),UINT64_C(9223372036854775807)}) SCALAR(Positive63,positive_63,v,0,UINT64_C(9223372036854775807));
    for(std::int64_t v : {INT64_MIN,INT64_MIN+1,INT64_C(-1),INT64_C(0),INT64_MAX-1,INT64_MAX}) SCALAR(FullSigned64,full_signed_64,v,INT64_MIN,INT64_MAX);
    SCALAR(MinimumConstant,minimum_constant,INT64_MIN,INT64_MIN,INT64_MIN);
    SCALAR(MaximumConstant,maximum_constant,UINT64_C(9223372036854775807),UINT64_C(9223372036854775807),UINT64_C(9223372036854775807));
    error(c::encode_signed_small(-6),r::ErrorCode::constraint_violation,0);
    error(c::encode_signed_small(6),r::ErrorCode::constraint_violation,0);
    error(c::encode_nonzero(4),r::ErrorCode::constraint_violation,0);
    error(c::encode_nonzero(UINT64_MAX),r::ErrorCode::constraint_violation,0);
    error(c::encode_constant(-6),r::ErrorCode::constraint_violation,0);
    error(c::decode_signed_small(std::vector<std::byte>{std::byte{0xb0}}),r::ErrorCode::constraint_violation,0);
    error(c::decode_nonzero(std::vector<std::byte>{std::byte{0xc0}}),r::ErrorCode::constraint_violation,0);
    error(c::decode_wide_257(std::vector<std::byte>{std::byte{0x01},std::byte{0x01}}),r::ErrorCode::constraint_violation,0);
    auto standalone = s::encode_solo(-3); REQUIRE(standalone && standalone.value().octets == std::vector<std::byte>{std::byte{0}});
    REQUIRE(s::decode_solo(standalone.value().octets).value() == -3);
    REQUIRE(s::encode_whole(INT64_MAX)); REQUIRE(s::encode_fixed(13));
    c::Packet packet{}; packet.lead = true; packet.inline_signed = 0; packet.constrained = 5;
    packet.selection = c::Pick_negative{-2}; packet.tail = true;
    Model model; model.bit(false); model.bit(true); model.sint(0,-4,4); model.uint(5,2,7); model.bit(false); model.sint(-2,-9,-2); model.bit(true);
    auto wanted = model.complete(); auto encoded = c::encode_packet(packet); REQUIRE(encoded && encoded.value().octets == wanted);
    auto decoded = c::decode_packet(wanted); REQUIRE(decoded); REQUIRE(decoded.value().lead && decoded.value().inline_signed == 0);
    REQUIRE(decoded.value().constrained == 5 && !decoded.value().optional_signed && decoded.value().tail);
    REQUIRE(std::get<c::Pick_negative>(decoded.value().selection).value == -2);
    packet.lead = false; packet.inline_signed = -4; packet.constrained = 2; packet.optional_signed = 3; packet.selection = c::Pick_narrow{9}; packet.tail = false;
    model = Model{}; model.bit(true); model.bit(false); model.sint(-4,-4,4); model.uint(2,2,7); model.sint(3,-2,3); model.bit(true); model.uint(9,3,9); model.bit(false);
    wanted = model.complete(); encoded = c::encode_packet(packet); REQUIRE(encoded && encoded.value().octets == wanted);
    decoded = c::decode_packet(wanted); REQUIRE(decoded); REQUIRE(!decoded.value().lead && decoded.value().inline_signed == -4);
    REQUIRE(decoded.value().constrained == 2 && decoded.value().optional_signed == 3 && !decoded.value().tail);
    REQUIRE(std::get<c::Pick_narrow>(decoded.value().selection).value == 9);
    packet.constrained = 8; error(c::encode_packet(packet),r::ErrorCode::constraint_violation,6); packet.constrained = 2;
    packet.optional_signed = 4; error(c::encode_packet(packet),r::ErrorCode::constraint_violation,9); packet.optional_signed = 3;
    packet.selection = c::Pick_narrow{2}; error(c::encode_packet(packet),r::ErrorCode::constraint_violation,13);
    c::PrefixSmall small{true,254}; model = Model{}; model.bit(true); model.uint(254,0,254);
    REQUIRE(c::encode_prefix_small(small).value().octets == model.complete());
    REQUIRE(c::decode_prefix_small(c::encode_prefix_small(small).value().octets).value().value == 254);
    c::PrefixOctet octet{true,265}; model = Model{}; model.bit(true); model.uint(265,10,265);
    REQUIRE(c::encode_prefix_octet(octet).value().octets == model.complete());
    c::PrefixWide wide{true,356}; model = Model{}; model.bit(true); model.uint(356,100,356);
    REQUIRE(c::encode_prefix_wide(wide).value().octets == model.complete());
    c::PrefixLarge large{true,65636}; model = Model{}; model.bit(true); model.uint(65636,100,65636);
    REQUIRE(c::encode_prefix_large(large).value().octets == model.complete());
    c::PrefixFull full{true,INT64_MAX}; model = Model{}; model.bit(true); model.sint(INT64_MAX,INT64_MIN,INT64_MAX);
    REQUIRE(c::encode_prefix_full(full).value().octets == model.complete());
    error(c::decode_prefix_octet(std::vector<std::byte>{std::byte{0x81},std::byte{0xff}}),r::ErrorCode::nonzero_padding,7);
    error(c::decode_prefix_wide(std::vector<std::byte>{std::byte{0x80},std::byte{0x01},std::byte{0x01}}),r::ErrorCode::constraint_violation,1);
    error(c::decode_prefix_small(std::vector<std::byte>{std::byte{0xff},std::byte{0x01}}),r::ErrorCode::nonzero_padding,15);
    error(c::decode_prefix_large(std::vector<std::byte>{std::byte{0xa0},std::byte{0},std::byte{0}}),r::ErrorCode::constraint_violation,1);
    error(c::decode_prefix_large(std::vector<std::byte>{std::byte{0xe0},std::byte{0},std::byte{0},std::byte{0}}),r::ErrorCode::constraint_violation,1);
    auto sticky = r::encode_complete(0,{},[](r::FieldWriter& f) {
        REQUIRE(f.write_bit(true)); auto first = c::integer_codec::put_SignedSmall(f,6);
        error(first,r::ErrorCode::constraint_violation,1);
        error(c::integer_codec::put_Nonzero(f,5),first.error().code,first.error().bit_offset);
        error(f.write_bit(false),first.error().code,first.error().bit_offset);
        return r::Result<void>::success();
    }); error(sticky,r::ErrorCode::constraint_violation,1);
    auto read_sticky = r::decode_complete<int>(std::vector<std::byte>{std::byte{0xe0}}, {}, [](r::FieldReader& f) {
        REQUIRE(f.read_bit().value()); auto first = c::integer_codec::get_SignedSmall(f);
        error(first,r::ErrorCode::constraint_violation,1);
        error(c::integer_codec::get_Nonzero(f),first.error().code,first.error().bit_offset);
        error(f.read_bit(),first.error().code,first.error().bit_offset);
        return r::Result<int>::success(42);
    }); error(read_sticky,r::ErrorCode::constraint_violation,1);
    puts("PASS generated bounded INTEGER independent bit model, extremes, effective intervals, budgets and sticky errors");
}
