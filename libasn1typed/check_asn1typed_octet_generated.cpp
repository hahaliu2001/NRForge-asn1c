#include "runtime.hpp"
#include "types.hpp"
#include "mapping.hpp"
#include "codec.hpp"
#include <cstdlib>
#include <iostream>
#include <type_traits>
#define REQUIRE(x) do { if(!(x)) { std::cerr << __LINE__ << ": " << #x << '\n'; std::abort(); } } while(false)
using namespace octettest;
using nrforge::aper::ErrorCode;
static std::vector<std::byte> bytes(std::initializer_list<unsigned> v) {
    std::vector<std::byte> out; for(auto b:v) out.push_back(static_cast<std::byte>(b)); return out;
}
int main() {
    static_assert(std::is_same_v<Three,std::vector<std::byte>>);
    static_assert(!One_aper::payload_align_to_octet && !Two_aper::has_length_determinant);
    static_assert(Three_aper::payload_align_to_octet && !Three_aper::has_length_determinant);
    static_assert(Small_aper::has_length_determinant && Opaque_aper::unconstrained);
    const auto one=bytes({0xab}),two=bytes({0xab,0xcd}),three=bytes({0xab,0xcd,0xef});
    REQUIRE(encode_one(one).value().octets==one && decode_one(one).value()==one);
    REQUIRE(encode_two(two).value().octets==two && decode_two(two).value()==two);
    REQUIRE(encode_three(three).value().octets==three && decode_three(three).value()==three);
    auto zero=encode_zero({}); REQUIRE(zero && zero.value().octets==bytes({0}) && zero.value().empty_encoding_substitution);
    REQUIRE(decode_zero(bytes({0})).value().empty());
    auto small=encode_small(three); REQUIRE(small && small.value().octets==bytes({0xc0,0xab,0xcd,0xef}));
    REQUIRE(decode_small(small.value().octets).value()==three);
    REQUIRE(encode_small({}).value().octets==bytes({0}));
    REQUIRE(encode_wide(one).value().octets==bytes({0,0xab}));
    REQUIRE(encode_opaque(three).value().octets==bytes({3,0xab,0xcd,0xef}));
    auto overflow=encode_one(two); REQUIRE(!overflow && overflow.error().code==ErrorCode::constraint_violation && overflow.error().bit_offset==0);
    Message message{}; message.lead=true; message.first=bytes({1}); message.second=bytes({0,2}); message.third=bytes({0,0,3}); message.inline_=one;
    auto result=encode_message(message);
    const auto expected=bytes({0x40,0x40,0,0x80,0,0,3,1,0xab});
    REQUIRE(result && result.value().octets==expected);
    auto decoded=decode_message(expected); REQUIRE(decoded && decoded.value().lead && decoded.value().first==message.first && decoded.value().second==message.second && decoded.value().third==message.third && !decoded.value().data && decoded.value().inline_==one);
    message.data=two; result=encode_message(message); REQUIRE(result);
    decoded=decode_message(result.value().octets); REQUIRE(decoded && decoded.value().data==message.data && decoded.value().inline_==one);
    auto pick=encode_pick(Pick_bytes{three}); REQUIRE(pick && pick.value().octets==bytes({0,0xab,0xcd,0xef}));
    REQUIRE(std::get<Pick_bytes>(decode_pick(pick.value().octets).value()).value==three);
    REQUIRE(encode_pick(Pick_marker{true}).value().octets==bytes({0xc0}));
    auto list=encode_list(List{{one,bytes({0xcd})}}); REQUIRE(list && list.value().octets==bytes({0xaa,0xf3,0x40}));
    REQUIRE(decode_list(list.value().octets).value().elements==std::vector<One>({one,bytes({0xcd})}));
    auto bad=decode_small(bytes({0xc1,0xab,0xcd,0xef})); REQUIRE(!bad && bad.error().code==ErrorCode::nonzero_padding && bad.error().bit_offset==7);
    bad=decode_small(bytes({0xc0,0xab})); REQUIRE(!bad && bad.error().code==ErrorCode::truncated_input && bad.error().bit_offset==16);
    auto trail=three; trail.push_back(std::byte{0}); auto trailing=decode_three(trail); REQUIRE(!trailing && trailing.error().code==ErrorCode::trailing_data && trailing.error().bit_offset==24);
    std::cout << "PASS N14 generated OCTET byte vectors, payloads and error boundaries\n";
}
