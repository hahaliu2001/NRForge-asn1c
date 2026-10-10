#include "runtime.hpp"
#include "types.hpp"
#include "mapping.hpp"
#include "codec.hpp"
#include <cstdlib>
#include <iostream>
#include <type_traits>
#ifndef NDEBUG
#error "Checks must remain active under NDEBUG"
#endif
#define REQUIRE(x) do { if(!(x)) { std::cerr << __LINE__ << ": " << #x << '\n'; std::abort(); } } while(false)
using namespace bittests;
using nrforge::aper::BitString;
using nrforge::aper::ErrorCode;
static std::vector<std::byte> bytes(std::initializer_list<unsigned> v) {
    std::vector<std::byte> out; for(auto b:v)out.push_back(static_cast<std::byte>(b)); return out;
}
static BitString bits(std::size_t n) {
    BitString out{}; out.bit_count=n; out.octets.assign((n+7)/8,std::byte{0xaa});
    if(n%8)out.octets.back() &= static_cast<std::byte>(0xffu<<(8-n%8));
    return out;
}
static bool same(const BitString& a,const BitString& b) { return a.bit_count==b.bit_count && a.octets==b.octets; }
// Independent bit assembler: no production layout helpers or encode calls.
struct Model {
    std::vector<std::byte> wire; std::size_t count=0;
    void bit(bool v) { if(count%8==0)wire.push_back(std::byte{0}); if(v)wire.back()|=static_cast<std::byte>(0x80u>>(count%8)); ++count; }
    void number(std::size_t v,unsigned width) { for(unsigned i=width;i>0;--i)bit(((v>>(i-1))&1u)!=0); }
    void align() { while(count%8)bit(false); }
    void payload(const BitString& v) { for(std::size_t i=0;i<v.bit_count;++i)bit((std::to_integer<unsigned>(v.octets[i/8])&(0x80u>>(i%8)))!=0); }
    std::vector<std::byte> complete() { if(count==0)bit(false); align(); return wire; }
};
int main() {
    static_assert(std::is_same_v<One,BitString> && std::is_same_v<Opaque,BitString>);
    const auto zero=bits(0),one=bits(1),sixteen=bits(16),seventeen=bits(17);
    auto z=encode_zero(zero); REQUIRE(z && z.value().octets==bytes({0}) && z.value().empty_encoding_substitution);
    REQUIRE(same(decode_zero(bytes({0})).value(),zero));
    REQUIRE(encode_one(one).value().octets==bytes({0x80}));
    REQUIRE(encode_sixteen(sixteen).value().octets==bytes({0xaa,0xaa}));
    REQUIRE(encode_seventeen(seventeen).value().octets==bytes({0xaa,0xaa,0x80}));
    for(std::size_t n=1;n<=20;++n) {
        const auto value=bits(n); Model model; model.number(n-1,5); model.align(); model.payload(value);
        const auto expected=model.complete(); auto encoded=encode_small(value); REQUIRE(encoded && encoded.value().octets==expected);
        auto owned=expected; auto decoded=decode_small(owned); REQUIRE(decoded && same(decoded.value(),value));
        owned.assign(owned.size(),std::byte{0}); REQUIRE(same(decoded.value(),value));
        auto copied=decoded.value(); copied.octets[0]=std::byte{0}; REQUIRE(same(decoded.value(),value));
        auto moved=std::move(decoded).value(); REQUIRE(same(moved,value));
        nrforge::aper::Limits limits; limits.max_input_octets=expected.size(); limits.max_output_octets=expected.size(); limits.max_wire_bits=expected.size()*8;
        REQUIRE(encode_small(value,limits) && decode_small(expected,limits));
        limits.max_wire_bits=expected.size()*8-1; REQUIRE(!encode_small(value,limits) && !decode_small(expected,limits));
    }
    REQUIRE(encode_opaque(seventeen).value().octets==bytes({17,0xaa,0xaa,0x80}));
    auto invalid=encode_one(sixteen); REQUIRE(!invalid && invalid.error().code==ErrorCode::constraint_violation);
    auto dirty=one; dirty.octets[0]=std::byte{0x81}; REQUIRE(!encode_one(dirty));
    auto tail=decode_one(bytes({0x81})); REQUIRE(!tail && tail.error().code==ErrorCode::nonzero_padding);
    auto trailing=decode_one(bytes({0x80,0})); REQUIRE(!trailing && trailing.error().code==ErrorCode::trailing_data);
    Message message{}; message.lead=true; message.named=one; message.inline_bits=seventeen;
    message.named_octets=bytes({0x12}); message.inline_octets=bytes({0x34,0x56});
    Model model; model.bit(true); model.number(0,5); model.align(); model.payload(one); model.align(); model.payload(seventeen);
    model.number(0,2); model.align(); model.number(0x12,8); model.number(0x34,8); model.number(0x56,8);
    const auto expected=model.complete(); auto encoded=encode_message(message); REQUIRE(encoded && encoded.value().octets==expected);
    auto decoded=decode_message(expected); REQUIRE(decoded && decoded.value().lead && same(decoded.value().named,one) && same(decoded.value().inline_bits,seventeen) && decoded.value().named_octets==message.named_octets && decoded.value().inline_octets==message.inline_octets);
    message.named=bits(21); REQUIRE(!encode_message(message));
    auto pick=encode_pick(Pick_bits{one}); Model pm; pm.bit(false); pm.number(0,5); pm.align(); pm.payload(one); REQUIRE(pick && pick.value().octets==pm.complete());
    REQUIRE(same(std::get<Pick_bits>(decode_pick(pick.value().octets).value()).value,one));
    auto octet_pick=encode_pick(Pick_octets{bytes({0x12})}); Model om; om.bit(true); om.number(0,2); om.align(); om.number(0x12,8); REQUIRE(octet_pick && octet_pick.value().octets==om.complete());
    static_assert(Refined_aper::field_0_payload_mapping::lower_bound==8 && Refined_aper::field_0_payload_mapping::upper_bound==16);
    static_assert(Refined_aper::field_1_payload_mapping::lower_bound==0 && Refined_aper::field_1_payload_mapping::upper_bound==32);
    static_assert(RefinedPick_aper::payload_mapping_0::lower_bound==8 && RefinedPick_aper::payload_mapping_0::upper_bound==16);
    Refined refined{}; refined.narrow=bits(8); refined.wide=bits(32); refined.optional_bits=one;
    Model rm; rm.bit(true); rm.number(0,4); rm.align(); rm.payload(refined.narrow); rm.number(32,6); rm.align(); rm.payload(refined.wide); rm.payload(one);
    auto refined_wire=rm.complete(); auto refined_encoded=encode_refined(refined); REQUIRE(refined_encoded && refined_encoded.value().octets==refined_wire);
    auto refined_decoded=decode_refined(refined_wire); REQUIRE(refined_decoded && same(refined_decoded.value().narrow,refined.narrow) && same(refined_decoded.value().wide,refined.wide) && refined_decoded.value().optional_bits && same(*refined_decoded.value().optional_bits,one));
    refined_wire.assign(refined_wire.size(),std::byte{0}); REQUIRE(same(refined_decoded.value().wide,refined.wide));
    refined.narrow=bits(7); auto refined_bad=encode_refined(refined); REQUIRE(!refined_bad && refined_bad.error().code==ErrorCode::constraint_violation && refined_bad.error().bit_offset==1);
    auto pick_bad=encode_refined_pick(RefinedPick_bits{bits(17)}); REQUIRE(!pick_bad && pick_bad.error().code==ErrorCode::constraint_violation && pick_bad.error().bit_offset==1);
    nrforge::aper::EncodeContext sticky_context; nrforge::aper::BitWriter sticky_writer(sticky_context); nrforge::aper::FieldWriter sticky_fields(sticky_writer);
    auto sticky=compound_codec::put_RefinedPick(sticky_fields,RefinedPick_bits{bits(7)}); REQUIRE(!sticky && sticky.error().bit_offset==1);
    auto ignored=sticky_fields.write_bit(true); REQUIRE(!ignored && ignored.error().code==sticky.error().code && ignored.error().bit_offset==1);
    BitList list{{one,bits(1)}}; auto list_encoded=encode_bit_list(list); REQUIRE(list_encoded && list_encoded.value().octets==bytes({0xb0}));
    auto list_decoded=decode_bit_list(bytes({0xb0})); REQUIRE(list_decoded && list_decoded.value().elements.size()==2 && same(list_decoded.value().elements[0],one));
    auto list_copy=list_decoded.value(); list_copy.elements[0].octets[0]=std::byte{0}; REQUIRE(same(list_decoded.value().elements[0],one));
    std::cout << "PASS N15 generated BIT vectors, effective field constraints and ownership\n";
}
