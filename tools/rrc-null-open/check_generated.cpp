#include "r000.hpp"
#include "r001.hpp"
#include "r002.hpp"
#include "r003.hpp"
#include "r004.hpp"
#include "r005.hpp"
#include <cstdio>
#include <cstdlib>
using namespace nrforge::uper;
#define CHECK(x) do { if(!(x)) { std::fprintf(stderr,"null-open:%d: %s\n",__LINE__,#x);std::exit(1); } } while(false)
std::vector<std::byte> bytes(std::initializer_list<unsigned> xs) {std::vector<std::byte> b;for(auto x:xs)b.push_back(static_cast<std::byte>(x));return b;}
template<class T,class E,class D> void golden(const T& v,E encode,D decode,std::size_t bits,std::vector<std::byte> b) {
    for(auto c:{Completion::octets,Completion::contained_bits,Completion::exact}) {
        auto e=encode(v,{},c);CHECK(e && e.value().semantic_bits==bits && e.value().octets==b);
        auto d=decode(b,{},c==Completion::octets?std::nullopt:std::optional<std::size_t>{e.value().encoded_bits},c);
        CHECK(d && d.value().value==v && d.value().semantic_bits==bits);
        CHECK(!decode(b,{},bits-1,Completion::exact));
    }
}
#define GOLD(N,V,B,...) golden(V,nrforge_generated::N::encode,nrforge_generated::N::decode,B,bytes({__VA_ARGS__}))
int main() {
    namespace s=nrforge_generated::r000;s::Root v;GOLD(r000,v,2,0x00);v.f_added=std::monostate{};GOLD(r000,v,26,0x80,0x40,0x40,0x00);
    namespace c=nrforge_generated::r001;c::Root choice;choice.value=c::T0::A0{false};GOLD(r001,choice,2,0x00);choice.value=c::T0::A1{};GOLD(r001,choice,24,0x80,0x01,0x00);
    namespace b=nrforge_generated::r002;b::Root boolean;GOLD(r002,boolean,2,0x00);boolean.f_added=true;GOLD(r002,boolean,26,0x80,0x40,0x60,0x00);
    namespace d=nrforge_generated::r003;d::Root pick;pick.value=d::T0::A0{false};GOLD(r003,pick,2,0x00);pick.value=d::T0::A1{true};GOLD(r003,pick,24,0x80,0x01,0x80);
    namespace n=nrforge_generated::r004;n::Root named;GOLD(r004,named,2,0x00);named.f_added=n::T1{};GOLD(r004,named,26,0x80,0x40,0x40,0x00);
    namespace p=nrforge_generated::r005;p::Root named_pick;named_pick.value=p::T0::A0{false};GOLD(r005,named_pick,2,0x00);named_pick.value=p::T0::A1{};GOLD(r005,named_pick,24,0x80,0x01,0x00);
    CHECK(!c::decode(bytes({0x80,0x00}))); CHECK(!c::decode(bytes({0x80,0x01,0x01})));
    CHECK(!c::decode(bytes({0x80,0x02,0x00,0x00}))); CHECK(!c::decode(bytes({0x80,0x01})));
    CHECK(!s::decode(bytes({0x80,0x40,0x40,0x01})));
    Limits q;q.max_value_octets=0;CHECK(!c::encode(choice,q));CHECK(!c::decode(bytes({0x80,0x01,0x00}),q));
    std::puts("six NULL/BOOL/named-NULL roots: 12 goldens, three completions, empty-body framing/padding/trailing/truncation/quota PASS");
}
