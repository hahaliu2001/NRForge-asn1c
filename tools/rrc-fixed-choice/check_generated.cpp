#include "r000.hpp"
#include "r001.hpp"
#include "r002.hpp"
#include "r003.hpp"
#include "r004.hpp"
#include "r005.hpp"
#include <cstdio>
#include <cstdlib>
using namespace nrforge::uper;
#define CHECK(x) do { if(!(x)) { std::fprintf(stderr,"fixed-choice:%d: %s\n",__LINE__,#x); std::exit(1); } } while(false)
std::vector<std::byte> bytes(std::initializer_list<unsigned> values) {
    std::vector<std::byte> result;
    for(auto v:values) result.push_back(static_cast<std::byte>(v));
    return result;
}
template<class T,class E,class D> void golden(const T& v,E encode,D decode,std::size_t bits,std::vector<std::byte> expected) {
    for(auto mode:{Completion::octets,Completion::contained_bits,Completion::exact}) {
        auto e=encode(v,{},mode);
        CHECK(e && e.value().semantic_bits==bits && e.value().octets==expected);
        auto d=decode(e.value().octets,{},mode==Completion::octets ? std::nullopt :
                      std::optional<std::size_t>{e.value().encoded_bits},mode);
        CHECK(d && d.value().value==v && d.value().semantic_bits==bits);
        if(bits) CHECK(!decode(e.value().octets,{},bits-1,Completion::exact));
    }
}
#define GOLD(N,V,B,...) golden(V,nrforge_generated::N::encode,nrforge_generated::N::decode,B,bytes({__VA_ARGS__}))
int main() {
    namespace a=nrforge_generated::r001;
    a::Root v; v.value=a::T0::A0{}; GOLD(r001,v,1,0x00);
    v.value=a::T0::A1{{3}}; GOLD(r001,v,3,0xe0);
    v.value=a::T0::A1{{4}}; CHECK(!a::encode(v));
    a::Root unset; CHECK(!a::encode(unset));
    namespace x=nrforge_generated::r004;
    static_assert(x::T0::wire0.index==1 && x::T0::wire1.index==0);
    x::Root explicit_v; explicit_v.value=x::T0::A0{}; GOLD(r004,explicit_v,1,0x80);
    explicit_v.value=x::T0::A1{{3}}; GOLD(r004,explicit_v,3,0x60);
    namespace i=nrforge_generated::r005;
    static_assert(i::T0::wire0.index==0 && i::T0::wire1.index==1);
    i::Root implicit_v; implicit_v.value=i::T0::A0{}; GOLD(r005,implicit_v,1,0x00);
    implicit_v.value=i::T0::A1{{3}}; GOLD(r005,implicit_v,3,0xe0);
    namespace e=nrforge_generated::r003;
    e::Root ext; ext.value=e::T0::A0{}; GOLD(r003,ext,2,0x00);
    ext.value=e::T0::A1{{3}}; GOLD(r003,ext,4,0x70);
    ext.value=e::T0::A2{true}; GOLD(r003,ext,24,0x80,0x01,0x80);
    ext.value=e::Unknown{1,bytes({0xaa})}; GOLD(r003,ext,24,0x81,0x01,0xaa);
    ext.value=e::Unknown{0,bytes({0xaa})}; CHECK(!e::encode(ext));
    CHECK(!e::decode(bytes({0x80,0x01}))); // truncated known addition
    Limits quota; quota.max_value_octets=0;
    CHECK(!e::decode(bytes({0x80,0x01,0x80}),quota));
    namespace r=nrforge_generated::r000;
    r::Root roots; roots.f_a.value=r::T1::A1{{2}}; roots.f_b.value=r::T2::A1{{5}};
    GOLD(r000,roots,6,0xd4);
    namespace n=nrforge_generated::r002;
    n::Root nested; nested.f_inner.value=n::T1::A1{{1}}; GOLD(r002,nested,3,0xa0);
    std::puts("six generated roots: tag order, 12 goldens, three completions, rejection/truncation/quota PASS");
}
