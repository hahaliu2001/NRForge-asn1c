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
#define CHECK(x) do { if(!(x)) { std::fprintf(stderr,"opaque:%d: %s\n",__LINE__,#x); std::exit(1); } } while(false)
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
#define OCTETS(N) { namespace n=nrforge_generated::N; n::Root v; GOLD(N,v,8,0x00); v.value=bytes({0xff,0x00}); GOLD(N,v,24,0x02,0xff,0x00); Limits q; q.max_value_octets=1; CHECK(!n::encode(v,q)); CHECK(!n::decode(bytes({0x02,0xff,0x00}),q)); CHECK(!n::decode(bytes({0x02,0xff}))); }
int main() {
    OCTETS(r000); OCTETS(r001); OCTETS(r005); OCTETS(r006);
    namespace a=nrforge_generated::r002; a::Root inline_v;
    inline_v.value=a::T0::A0{}; GOLD(r002,inline_v,1,0x00);
    inline_v.value=a::T0::A1{}; GOLD(r002,inline_v,9,0x80,0x00);
    inline_v.value=a::T0::A1{bytes({0xff,0x00})}; GOLD(r002,inline_v,25,0x81,0x7f,0x80,0x00);
    CHECK(!a::encode(a::Root{})); CHECK(!a::decode(bytes({0x81,0x7f,0x80,0x01})));
    namespace n=nrforge_generated::r003; n::Root named_v;
    named_v.value=n::T0::A0{}; GOLD(r003,named_v,1,0x00);
    named_v.value=n::T0::A1{}; GOLD(r003,named_v,9,0x80,0x00);
    named_v.value=n::T0::A1{{bytes({0xff,0x00})}}; GOLD(r003,named_v,25,0x81,0x7f,0x80,0x00);
    namespace s=nrforge_generated::r004; s::Root sequence;
    GOLD(r004,sequence,9,0x00,0x00);
    sequence.f_required.value=bytes({0xff,0x00}); sequence.f_optional=bytes({0xa5});
    GOLD(r004,sequence,41,0x81,0x7f,0x80,0x00,0xd2,0x80);
    Limits quota; quota.max_value_octets=1;
    CHECK(!a::encode(inline_v,quota)); CHECK(!a::decode(bytes({0x81,0x7f,0x80,0x00}),quota));
    // ff00 need not be a valid inner INTEGER encoding: only carrier bytes qualify.
    std::puts("seven opaque roots: 16 goldens, three completions, arbitrary bytes, truncation/padding/quota PASS");
}
