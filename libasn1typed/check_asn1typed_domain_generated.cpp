#include <runtime.hpp>
#include <sequence_extensions.hpp>
#include "types.hpp"
#include "mapping.hpp"
#include "codec.hpp"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <type_traits>
using namespace nrforge::aper;
namespace d=domains;
#define REQUIRE(x) do { if(!(x)) { std::fprintf(stderr,"domain_generated:%d: %s\n",__LINE__,#x); std::abort(); } } while(0)
static_assert(std::is_same_v<d::P0,std::int64_t>);
static_assert(std::is_same_v<decltype(d::Pick_precise::value),std::int64_t>);
static_assert(std::is_same_v<decltype(d::Packet::period),std::int64_t>);
static_assert(d::Gap_aper::lower_bound==1 && d::Gap_aper::upper_bound==181 && d::Gap_aper::root_bits==8);
static_assert(d::Gap_aper::root_intervals[8].lower==180 && d::Gap_aper::root_intervals[8].upper==181);
struct Model {
    std::vector<std::byte> bytes; std::size_t n=0;
    void bit(bool v) { if(!(n%8)) bytes.push_back(std::byte{0}); if(v) bytes.back()|=static_cast<std::byte>(128u>>(n%8)); ++n; }
    void number(std::uint64_t v,unsigned width) { while(width) bit(((v>>--width)&1u)!=0); }
    void align() { while(n%8) bit(false); }
    void integer(std::int64_t v,std::int64_t lo,std::int64_t hi) {
        bool ext=v<lo || v>hi; bit(ext);
        if(ext) {
            unsigned bytes=1; while(bytes<8 && (v<-(std::int64_t{1}<<(bytes*8-1)) || v>=(std::int64_t{1}<<(bytes*8-1)))) ++bytes;
            align(); number(bytes,8); number(static_cast<std::uint64_t>(v),bytes*8); return;
        }
        auto distance=static_cast<std::uint64_t>(hi)-static_cast<std::uint64_t>(lo),offset=static_cast<std::uint64_t>(v)-static_cast<std::uint64_t>(lo);
        unsigned width=0; for(auto n=distance;n;n>>=1) ++width;
        if(!distance) return;
        if(distance<255) number(offset,width);
        else if(distance==255) { align(); number(offset,8); }
        else if(distance<=65535) { align(); number(offset,16); }
        else { unsigned maxbytes=(width+7)/8,bytes=1; for(auto n=offset>>8;n;n>>=8) ++bytes; width=0; for(auto n=maxbytes-1;n;n>>=1) ++width; number(bytes-1,width); align(); number(offset,bytes*8); }
    }
};
static_assert(d::RootAndAddition_aper::root_intervals[1].lower == 10);
static_assert(d::RootAndAddition_aper::known_extension_intervals[1].lower == 15);
void graphs() {
    for(std::int64_t value: {0,4095,4096,2000000,2000001,-1}) {
        Model m; m.integer(value,0,4095); m.align();
        auto encoded=d::encode_max_data_burst_volume(value); REQUIRE(encoded && encoded.value().octets==m.bytes);
        auto decoded=d::decode_max_data_burst_volume(m.bytes); REQUIRE(decoded && decoded.value()==value);
    }
    for(std::int64_t value: {0,255,256,262,263,-1}) {
        Model m; m.integer(value,0,255); m.align();
        auto encoded=d::encode_prach_config(d::PrachConfig{value}); REQUIRE(encoded && encoded.value().octets==m.bytes);
        auto decoded=d::decode_prach_config(m.bytes); REQUIRE(decoded && decoded.value().prach_config_index==value);
    }
    for(std::int64_t value: {0,3,10,12,15,16,-1}) {
        auto encoded=d::encode_root_and_addition(value); REQUIRE(encoded);
        auto decoded=d::decode_root_and_addition(encoded.value().octets); REQUIRE(decoded && decoded.value()==value);
    }
    REQUIRE(!d::encode_root_and_addition(4));
    // Known additions and future unknown extensions all use signed extension layout.
    for(auto value:std::array<std::int64_t,8>{0,3,4,5,6,7,-1,128}) {
        Model m; m.integer(value,0,3); auto end=m.n; m.align();
        auto encoded=d::encode_srbid(value); REQUIRE(encoded && encoded.value().octets==m.bytes && encoded.value().last_field_end_bit==end);
        auto decoded=d::decode_srbid(m.bytes); REQUIRE(decoded && decoded.value()==value);
        auto inline_encoded=d::encode_inline_addition(d::InlineAddition{value}); REQUIRE(inline_encoded && inline_encoded.value().octets==m.bytes);
        auto inline_back=d::decode_inline_addition(m.bytes); REQUIRE(inline_back && inline_back.value().value==value);
        auto single=d::encode_single_addition(value); REQUIRE(single && single.value().octets==m.bytes);
        auto sparse=d::encode_sparse_addition(value); REQUIRE(sparse && sparse.value().octets==m.bytes);
        auto back=d::decode_sparse_addition(m.bytes); REQUIRE(back && back.value()==value);
    }
    for(auto value:std::array<std::int64_t,7>{1,30,40,180,181,-129,182}) {
        Model m; m.integer(value,1,181); const auto end=m.n; m.align();
        auto output=d::encode_gap(value); REQUIRE(output && output.value().octets==m.bytes && output.value().last_field_end_bit==end);
        auto decoded=d::decode_gap(m.bytes); REQUIRE(decoded && decoded.value()==value);
        if(value != 180) { d::InlineGap nested{value}; auto encoded_nested=d::encode_inline_gap(nested); REQUIRE(encoded_nested && encoded_nested.value().octets==m.bytes); auto decoded_nested=d::decode_inline_gap(m.bytes); REQUIRE(decoded_nested && decoded_nested.value().value==value); }
    }
    auto gap=d::encode_gap(31); REQUIRE(!gap && gap.error().code==ErrorCode::constraint_violation && gap.error().bit_offset==0);
    for(bool present:{false,true}) for(bool scaled:{false,true}) for(std::int64_t period:{-129,1,3600,3601}) {
        d::Packet value{}; value.period=period; if(present) value.time=-1;
        if(scaled) value.selection=d::Pick_scaled{48}; else value.selection=d::Pick_precise{40000001}; value.gap=40;
        Model m; m.bit(present); m.integer(period,1,3600); if(present) m.integer(-1,0,86399);
        m.bit(scaled); if(scaled) m.integer(48,32,47); else m.integer(40000001,1,40000000); m.integer(40,1,181); m.align();
        auto output=d::encode_packet(value); REQUIRE(output && output.value().octets==m.bytes);
        auto decoded=d::decode_packet(m.bytes); REQUIRE(decoded && decoded.value().period==period && decoded.value().time==value.time && decoded.value().gap==40);
        REQUIRE(decoded.value().selection.index()==value.selection.index());
        if(scaled) REQUIRE(std::get<d::Pick_scaled>(decoded.value().selection).value==48); else REQUIRE(std::get<d::Pick_precise>(decoded.value().selection).value==40000001);
        value.gap=31; auto invalid=d::encode_packet(value); REQUIRE(!invalid && invalid.error().code==ErrorCode::constraint_violation);
    }
}
#define CASE(N) case N: return d::integer_codec::put_P##N(f,value)
Result<void> put(unsigned profile,FieldWriter& f,std::int64_t value) { switch(profile) { CASE(0); CASE(1); CASE(2); CASE(3); CASE(4); CASE(5); CASE(6); CASE(7); default: return Result<void>::failure({ErrorCode::invalid_argument,0}); } }
#undef CASE
#define CASE(N) case N: return d::integer_codec::get_P##N(f)
Result<std::int64_t> get(unsigned profile,FieldReader& f) { switch(profile) { CASE(0); CASE(1); CASE(2); CASE(3); CASE(4); CASE(5); CASE(6); CASE(7); default: return Result<std::int64_t>::failure({ErrorCode::invalid_argument,0}); } }
#undef CASE
int main(int argc,char**) {
    if(argc==1) { graphs(); std::puts("PASS generated INTEGER intervals, gaps, inline CHOICE/SEQUENCE and signed extensions"); return 0; }
    unsigned profile=0,residue=0; long long input=0;
    while(std::scanf("%u %u %lld",&profile,&residue,&input)==3) {
        const auto value=static_cast<std::int64_t>(input);
        auto encoded=encode_complete(value,{},[&](FieldWriter& f) { for(unsigned i=0;i<residue;++i) { auto r=f.write_bit(true); if(!r) return r; } auto r=put(profile,f,value); return r ? f.write_bit(true) : r; }); REQUIRE(encoded);
        auto decoded=decode_complete<std::int64_t>(encoded.value().octets,{},[&](FieldReader& f) { for(unsigned i=0;i<residue;++i) { auto r=f.read_bit(); if(!r) return Result<std::int64_t>::failure(r.error()); REQUIRE(r.value()); } auto v=get(profile,f); if(!v) return v; auto last=f.read_bit(); if(!last) return Result<std::int64_t>::failure(last.error()); REQUIRE(last.value()); return v; }); REQUIRE(decoded && decoded.value()==value);
        for(auto b:encoded.value().octets) { std::printf("%02X",std::to_integer<unsigned>(b)); } std::printf(" %zu\n",encoded.value().last_field_end_bit);
    }
}
