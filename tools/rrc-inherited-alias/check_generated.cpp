#include "r000.hpp"
#include "r001.hpp"
#include "r002.hpp"
#include "r003.hpp"
#include "r004.hpp"
#include "r005.hpp"
#include "r006.hpp"
#include "r007.hpp"
#include "r008.hpp"
#include "r009.hpp"
#include <cstdio>
#include <cstdlib>
using namespace nrforge::uper;
#define CHECK(x) do { if(!(x)) { std::fprintf(stderr,"alias:%d: %s\n",__LINE__,#x); std::exit(1); } } while(false)
std::vector<std::byte> bytes(std::initializer_list<unsigned> values) {
    std::vector<std::byte> r; for(auto v:values) r.push_back(static_cast<std::byte>(v)); return r;
}
template<class T,class E,class D>
void golden(const T& v,E encode,D decode,std::size_t bits,std::vector<std::byte> expected) {
    for(auto mode:{Completion::octets,Completion::contained_bits,Completion::exact}) {
        auto e=encode(v,{},mode); CHECK(e && e.value().semantic_bits==bits && e.value().octets==expected);
        auto d=decode(expected,{},mode==Completion::octets?std::nullopt:std::optional<std::size_t>{e.value().encoded_bits},mode);
        CHECK(d && d.value().value==v && d.value().semantic_bits==bits);
        CHECK(!decode(expected,{},bits-1,Completion::exact));
    }
}
#define GOLD(N,V,B,...) golden(V,nrforge_generated::N::encode,nrforge_generated::N::decode,B,bytes({__VA_ARGS__}))
#define BOUNDED(N) { namespace n=nrforge_generated::N; n::Root v; CHECK(!n::encode(v)); v.value={{3}}; GOLD(N,v,4,0x30); v.value={{0},{1},{3}}; GOLD(N,v,8,0x87); v.value.push_back({0}); CHECK(!n::encode(v)); v.value={{4}}; CHECK(!n::encode(v)); Limits q; q.max_collection_elements=2; CHECK(!n::decode(bytes({0x87}),q)); CHECK(!n::decode(bytes({0xc0}))); }
#define TUPLE(N) { namespace n=nrforge_generated::N; n::Root v; v.value={{true,4}}; GOLD(N,v,2,0x40); v.value={{false,4},{true,4}}; GOLD(N,v,3,0xa0); v.value[0].f_singleton=3; CHECK(!n::encode(v)); }
#define EXTENDED(N) { namespace n=nrforge_generated::N; n::Root v; v.value={{3}}; GOLD(N,v,4,0x30); v.value={{0},{3}}; GOLD(N,v,6,0x4c); v.value={{0},{1},{2}}; GOLD(N,v,15,0x81,0x8c); }
#define UNBOUNDED(N) { namespace n=nrforge_generated::N; n::Root v; GOLD(N,v,8,0x00); v.value={{0},{3}}; GOLD(N,v,12,0x02,0x30); Limits q; q.max_collection_elements=1; CHECK(!n::encode(v,q)); CHECK(!n::decode(bytes({0x02,0x30}),q)); }
int main() {
    BOUNDED(r000); BOUNDED(r001); BOUNDED(r002); BOUNDED(r003);
    TUPLE(r004); TUPLE(r005); EXTENDED(r006); EXTENDED(r007);
    UNBOUNDED(r008); UNBOUNDED(r009);
    // Each direct/alias pair independently matches the same hand-derived bits.
    std::puts("ten direct/alias roots: 22 goldens, three completions, bounded/open/unbounded lengths, negatives/truncation/quota PASS");
}
