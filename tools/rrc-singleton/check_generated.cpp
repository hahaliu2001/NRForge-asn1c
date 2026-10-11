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
#include "r010.hpp"
#include <cstdio>
#include <cstdlib>
#include <limits>
using namespace nrforge::uper;
#define CHECK(x) do { if (!(x)) { std::fprintf(stderr,"singleton:%d: %s\n",__LINE__,#x); std::exit(1); } } while(false)

template<class T, class Encode, class Decode>
void scalar(T good, T bad, Encode encode, Decode decode) {
    for (auto form : {Completion::octets, Completion::contained_bits, Completion::exact}) {
        auto e = encode(good, form); CHECK(e);
        const auto &v=e.value();
        CHECK(v.semantic_bits==0 && v.padding_bits==0);
        CHECK(v.encoded_bits==(form==Completion::octets ? 8u : form==Completion::contained_bits ? 1u : 0u));
        CHECK(v.empty_substitution==(form!=Completion::exact));
        CHECK(v.octets==(form==Completion::exact ? std::vector<std::byte>{} : std::vector<std::byte>{std::byte{0}}));
        auto d=decode(v.octets,form==Completion::octets ? std::nullopt : std::optional<std::size_t>{v.encoded_bits},form);
        CHECK(d && d.value().value==good && d.value().semantic_bits==0);
        auto b=encode(bad,form); CHECK(!b && b.error().code==ErrorCode::constraint_violation && b.error().bit_offset==0);
    }
    std::vector<std::byte> junk{std::byte{1}};
    auto d=decode(junk,std::nullopt,Completion::octets);
    CHECK(!d && d.error().code==ErrorCode::nonzero_padding);
}
#define SCALAR(N,GOOD,BAD) scalar(nrforge_generated::N::Root{GOOD},nrforge_generated::N::Root{BAD}, \
 [](const auto& v,Completion c){return nrforge_generated::N::encode(v,{},c);}, \
 [](const auto& b,std::optional<std::size_t> n,Completion c){return nrforge_generated::N::decode(b,{},n,c);})

int main() {
    SCALAR(r000,INT64_C(0),INT64_C(1));
    SCALAR(r001,INT64_C(4),INT64_C(3));
    SCALAR(r002,INT64_C(-7),INT64_C(-6));
    SCALAR(r003,std::numeric_limits<std::int64_t>::min(),std::numeric_limits<std::int64_t>::min()+1);
    SCALAR(r004,std::numeric_limits<std::int64_t>::max(),std::numeric_limits<std::int64_t>::max()-1);
    SCALAR(r005,std::numeric_limits<std::uint64_t>::max(),std::numeric_limits<std::uint64_t>::max()-1);
    SCALAR(r006,INT64_C(4),INT64_C(5));
    namespace f=nrforge_generated::r007;
    f::Root v;v.f_direct=4;v.f_named.value=4;
    for (bool present : {false,true}) {
        v.f_optional=present ? std::optional<std::int64_t>{0} : std::nullopt;
        auto e=f::encode(v);CHECK(e && e.value().semantic_bits==1 && e.value().octets==std::vector<std::byte>{(present ? std::byte{0x80} : std::byte{0})});
        auto d=f::decode(e.value().octets);CHECK(d && d.value().value==v);
        auto bad=v;bad.f_direct=3;CHECK(!f::encode(bad));
        bad=v;bad.f_named.value=5;CHECK(!f::encode(bad));
        bad=v;bad.f_optional=1;CHECK(!f::encode(bad));
    }
    namespace c=nrforge_generated::r008;
    c::Root zero,negative;zero.value=c::T0::A0{0};negative.value=c::T0::A1{-7};
    auto ez=c::encode(zero),en=c::encode(negative);
    CHECK(ez && en && ez.value().semantic_bits==1 && en.value().semantic_bits==1);
    CHECK(ez.value().octets==std::vector<std::byte>{std::byte{0}} && en.value().octets==std::vector<std::byte>{std::byte{0x80}});
    CHECK(c::decode(ez.value().octets).value().value==zero && c::decode(en.value().octets).value().value==negative);
    zero.value=c::T0::A0{1};negative.value=c::T0::A1{-8};CHECK(!c::encode(zero) && !c::encode(negative));
    namespace d=nrforge_generated::r009;
    d::Root def;CHECK(def.f_fixed==4);def.f_marker=true;
    auto ed=d::encode(def);CHECK(ed && ed.value().semantic_bits==2 && ed.value().octets==std::vector<std::byte>{std::byte{0x40}});
    CHECK(d::decode(ed.value().octets).value().value==def);
    def.f_fixed=3;CHECK(!d::encode(def));
    // A present DEFAULT determinant is distinct from omitted DEFAULT: materialize 4.
    std::vector<std::byte> present_default{std::byte{0xc0}};
    auto dd=d::decode(present_default);CHECK(dd && dd.value().value.f_fixed==4 && dd.value().value.f_marker && dd.value().semantic_bits==2);
    namespace l=nrforge_generated::r010;
    for (std::size_t count : {std::size_t{1},std::size_t{2}}) {
        l::Root list;list.value.assign(count,l::T1{4});auto e=l::encode(list);
        CHECK(e && e.value().semantic_bits==1);
        CHECK(e.value().octets==std::vector<std::byte>{(count==1 ? std::byte{0} : std::byte{0x80})});
        CHECK(l::decode(e.value().octets).value().value==list);
        list.value[0].value=3;CHECK(!l::encode(list));
    }
    l::Root empty;CHECK(!l::encode(empty));empty.value.assign(3,l::T1{4});CHECK(!l::encode(empty));
    std::puts("PASS singleton generated codec: 7 scalar roots, 3 completion modes, optional/CHOICE/DEFAULT/named-list determinants, range rejection");
}
