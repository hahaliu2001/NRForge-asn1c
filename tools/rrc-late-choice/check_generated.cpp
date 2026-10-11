#include "r000.hpp"
#include "r001.hpp"
#include "r002.hpp"
#include "r003.hpp"
#include "r004.hpp"
#include "r005.hpp"
#include "r006.hpp"
#include <cstdio>
#include <cstdlib>
using namespace nrforge::uper;
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"late-choice:%d: %s\n",__LINE__,#x);std::exit(1);}}while(false)
std::vector<std::byte> bytes(std::initializer_list<unsigned> values){std::vector<std::byte> r;for(auto v:values)r.push_back(static_cast<std::byte>(v));return r;}
template<class T,class E,class D> void golden(const T& v,E encode,D decode,std::size_t bits,std::vector<std::byte> expected){for(auto mode:{Completion::octets,Completion::contained_bits,Completion::exact}){auto e=encode(v,{},mode);CHECK(e&&e.value().semantic_bits==bits&&e.value().octets==expected);auto d=decode(e.value().octets,{},mode==Completion::octets?std::nullopt:std::optional<std::size_t>{e.value().encoded_bits},mode);CHECK(d&&d.value().value==v&&d.value().semantic_bits==bits);CHECK(!decode(e.value().octets,{},bits-1,Completion::exact));}}
#define GOLD(N,V,B,...) golden(V,nrforge_generated::N::encode,nrforge_generated::N::decode,B,bytes({__VA_ARGS__}))
int main(){
 namespace a=nrforge_generated::r000;a::Root v;v.value=a::T0::A0{};GOLD(r000,v,1,0x00);v.value=a::T0::A1{{3}};GOLD(r000,v,3,0xe0);v.value=a::T0::A1{{4}};CHECK(!a::encode(v));a::Root unset;CHECK(!a::encode(unset));
 namespace n=nrforge_generated::r001;n::Root nested;nested.f_inner.value=n::T1::A1{{1}};GOLD(r001,nested,3,0xa0);
 namespace p=nrforge_generated::r002;p::Root outer;outer.value=p::T0::A0{};GOLD(r002,outer,1,0x00);p::T1 inner;inner.value=p::T1::A0{};outer.value=p::T0::A1{inner};GOLD(r002,outer,2,0x80);inner.value=p::T1::A1{{3}};outer.value=p::T0::A1{inner};GOLD(r002,outer,4,0xf0);
 namespace e=nrforge_generated::r003;e::Root ext;ext.value=e::T0::A0{};GOLD(r003,ext,2,0x00);ext.value=e::T0::A1{{3}};GOLD(r003,ext,4,0x70);ext.value=e::T0::A2{true};GOLD(r003,ext,24,0x80,0x01,0x80);CHECK(!e::decode(bytes({0x80,0x01})));Limits quota;quota.max_value_octets=0;CHECK(!e::decode(bytes({0x80,0x01,0x80}),quota));
 namespace m=nrforge_generated::r004;static_assert(m::T0::wire0.index==1&&m::T0::wire1.index==0);m::Root manual;manual.value=m::T0::A0{};GOLD(r004,manual,1,0x80);manual.value=m::T0::A1{{3}};GOLD(r004,manual,3,0x60);
 namespace q=nrforge_generated::r005;q::Root qualified;qualified.value=q::T0::A1{{7}};GOLD(r005,qualified,3,0xe0);qualified.value=q::T0::A1{{3}};CHECK(!q::encode(qualified));
 namespace x=nrforge_generated::r006;static_assert(x::T0::wire0.index==1&&x::T0::wire1.index==0);x::Root explicit_v;explicit_v.value=x::T0::A0{};GOLD(r006,explicit_v,1,0x80);explicit_v.value=x::T0::A1{{7}};GOLD(r006,explicit_v,3,0x60);
 std::puts("seven late-instance roots: 14 goldens, three completions, nested/explicit indexes, negatives/truncation/quota PASS");
}
