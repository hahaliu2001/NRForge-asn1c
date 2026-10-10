#include "runtime.hpp"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <new>
using namespace nrforge::aper;
#define REQUIRE(x) do { if(!(x)) { std::fprintf(stderr,"extensible_integer:%d: %s\n",__LINE__,#x); std::abort(); } } while(0)
namespace {
bool allocation_fails=false;
struct Bits {
    std::vector<std::byte> data; std::size_t n=0;
    void bit(bool v) { if(!(n%8)) data.push_back(std::byte{0}); if(v) data.back()|=static_cast<std::byte>(128u>>(n%8)); ++n; }
    void number(std::uint64_t v,unsigned width) { while(width) bit(((v>>--width)&1u)!=0); }
    void align() { while(n%8) bit(false); }
};
void model(Bits& b,std::int64_t v,std::int64_t lo,std::int64_t hi) {
    const bool extension=v<lo || v>hi; b.bit(extension);
    if(extension) {
        unsigned bytes=1;
        // Arithmetic bounds, independently of production's sign-octet stripping.
        while(bytes<8 && (v<-(std::int64_t{1}<<(bytes*8-1)) || v>=(std::int64_t{1}<<(bytes*8-1)))) ++bytes;
        b.align(); b.number(bytes,8); b.number(static_cast<std::uint64_t>(v),bytes*8); return;
    }
    const auto distance=static_cast<std::uint64_t>(hi)-static_cast<std::uint64_t>(lo);
    const auto offset=static_cast<std::uint64_t>(v)-static_cast<std::uint64_t>(lo);
    if(!distance) return;
    if(distance<255) { unsigned width=0; for(auto n=distance;n;n>>=1) ++width; b.number(offset,width); }
    else if(distance==255) { b.align(); b.number(offset,8); }
    else if(distance<=65535) { b.align(); b.number(offset,16); }
    else {
        unsigned max_bytes=1,count=1,width=0;
        for(auto n=distance>>8;n;n>>=8) ++max_bytes;
        for(auto n=offset>>8;n;n>>=8) ++count;
        for(auto n=max_bytes-1;n;n>>=1) ++width;
        b.number(count-1,width); b.align(); b.number(offset,count*8);
    }
}
template<class T> void error(const T& r,ErrorCode c,std::size_t p) { REQUIRE(!r); if(r.error().code!=c || r.error().bit_offset!=p) std::fprintf(stderr,"wanted %d@%zu got %d@%zu\n",static_cast<int>(c),p,static_cast<int>(r.error().code),r.error().bit_offset); REQUIRE(r.error().code==c); REQUIRE(r.error().bit_offset==p); }
constexpr auto min=std::numeric_limits<std::int64_t>::min(),max=std::numeric_limits<std::int64_t>::max();
void vectors() {
    const std::array<std::array<std::int64_t,2>,10> roots{{{5,5},{0,1},{0,254},{1,256},{-128,128},{0,65535},{0,65536},{1,40000000},{min,min+65536},{min,max}}};
    for(auto root:roots) for(unsigned residue=0;residue<8;++residue)
      for(auto value:std::array<std::int64_t,14>{root[0],root[1],min,max,-129,-128,-1,0,127,128,255,256,65536,40000001}) {
        Bits expected; for(unsigned i=0;i<residue;++i) expected.bit(true); model(expected,value,root[0],root[1]);
        const auto end=expected.n; expected.bit(true); expected.align();
        EncodeContext ec; BitWriter writer(ec); FieldWriter f(writer);
        for(unsigned i=0;i<residue;++i) REQUIRE(f.write_bit(true));
        REQUIRE(f.write_extensible_int(value,root[0],root[1])); REQUIRE(writer.cursor_bit()==end && ec.wire_bits()==end);
        REQUIRE(f.write_bit(true)); auto output=writer.finish(); REQUIRE(output && output.value().octets==expected.data);
        DecodeContext dc; auto made=BitReader::make(expected.data,dc); REQUIRE(made); auto reader=std::move(made).value(); FieldReader g(reader);
        for(unsigned i=0;i<residue;++i) REQUIRE(g.read_bit().value());
        auto decoded=g.read_extensible_int(root[0],root[1]); REQUIRE(decoded && decoded.value()==value); REQUIRE(reader.cursor_bit()==end && dc.wire_bits()==end);
        REQUIRE(g.read_bit().value()); REQUIRE(reader.validate_complete_value());
        // Every bit truncation leaves the operation at its original start.
        for(std::size_t cut=residue;cut<end;++cut) {
            DecodeContext context; auto bounded=BitReader::make_bounded_for_test(expected.data,cut,context); REQUIRE(bounded); auto cursor=std::move(bounded).value();
            for(unsigned i=0;i<residue;++i) REQUIRE(cursor.read_bit());
            auto failure=cursor.read_extensible_int(root[0],root[1]); error(failure,ErrorCode::truncated_input,cut);
            REQUIRE(cursor.cursor_bit()==residue && context.wire_bits()==residue);
            error(cursor.read_bit(),failure.error().code,failure.error().bit_offset);
        }
    }
}
void malformed() {
    const struct { std::initializer_list<unsigned> bytes; ErrorCode code; std::size_t offset; } cases[] = {
        {{0x80,0},ErrorCode::constraint_violation,0}, {{0x80,0x80,1},ErrorCode::constraint_violation,0},
        {{0x80,9},ErrorCode::resource_limit,0}, {{0x80,0xc1},ErrorCode::resource_limit,0},
        {{0x80,0xc0},ErrorCode::constraint_violation,0}, {{0x80,0xc5},ErrorCode::constraint_violation,0},
        {{0x80,2,0,127},ErrorCode::constraint_violation,0}, {{0x80,2,255,128},ErrorCode::constraint_violation,0},
        {{0x80,1,5},ErrorCode::constraint_violation,0}, {{0x81,1,128},ErrorCode::nonzero_padding,7},
        {{0x81,1},ErrorCode::truncated_input,16}, {{0x80,0x80},ErrorCode::truncated_input,16},
        {{0x80,0x80,128},ErrorCode::resource_limit,0}
    };
    for(const auto& test:cases) {
        std::vector<std::byte> bytes; for(auto b:test.bytes) bytes.push_back(static_cast<std::byte>(b));
        DecodeContext c; auto made=BitReader::make(bytes,c); REQUIRE(made); auto r=std::move(made).value();
        error(r.read_extensible_int(0,10),test.code,test.offset); REQUIRE(r.cursor_bit()==0 && c.wire_bits()==0);
    }
}
void state_budget_and_gap() {
    for(auto value:{std::int64_t{5},std::int64_t{-129}}) {
        Bits b; model(b,value,0,10); const auto bits=b.n; b.align();
        for(unsigned shortfall=0;shortfall<2;++shortfall) {
            Limits limits; limits.max_wire_bits=bits-shortfall;
            EncodeContext c(limits); BitWriter w(c); auto status=w.write_extensible_int(value,0,10);
            if(shortfall) { error(status,ErrorCode::resource_limit,0); REQUIRE(w.cursor_bit()==0 && c.wire_bits()==0); } else REQUIRE(status);
            DecodeContext d(limits); auto made=BitReader::make(b.data,d); REQUIRE(made); auto r=std::move(made).value(); auto got=r.read_extensible_int(0,10);
            if(shortfall) { error(got,ErrorCode::resource_limit,0); REQUIRE(r.cursor_bit()==0 && d.wire_bits()==0); } else REQUIRE(got && got.value()==value);
            limits.max_wire_bits=1000; limits.max_output_octets=b.data.size()-shortfall;
            EncodeContext o(limits); BitWriter ow(o); auto os=ow.write_extensible_int(value,0,10);
            if(shortfall) error(os,ErrorCode::resource_limit,0); else REQUIRE(os);
        }
    }
    EncodeContext c; BitWriter w(c); REQUIRE(w.write_extensible_int(5,0,10)); REQUIRE(w.finish());
    const auto cursor=w.cursor_bit(); error(w.write_extensible_int(5,10,0),ErrorCode::invalid_state,cursor); REQUIRE(!c.failed());
    BitWriter moved(std::move(w)); error(w.write_extensible_int(0,0,1),ErrorCode::invalid_state,0);
    EncodeContext failed; BitWriter fw(failed); error(fw.write_extensible_int(0,10,0),ErrorCode::invalid_argument,0); error(fw.write_extensible_int(5,0,10),ErrorCode::invalid_argument,0);
    EncodeContext alloc; BitWriter aw(alloc); allocation_fails=true; auto no_memory=aw.write_extensible_int(-129,0,10); allocation_fails=false;
    error(no_memory,ErrorCode::allocation_failure,0); REQUIRE(aw.cursor_bit()==0 && alloc.wire_bits()==0 && alloc.logical_output_octets()==0);
    constexpr IntegerInterval root[]={{1,30},{40,40},{180,181}};
    for(bool ext:{false,true}) for(std::int64_t value:{1,30,40,180,181}) {
        EncodeContext x; BitWriter a(x); REQUIRE(a.write_integer_set(value,root,ext)); auto output=a.finish(); REQUIRE(output);
        DecodeContext y; auto made=BitReader::make(output.value().octets,y); REQUIRE(made); auto r=std::move(made).value(); auto decoded=r.read_integer_set(root,ext); REQUIRE(decoded && decoded.value()==value);
    }
    EncodeContext gap; BitWriter gw(gap); error(gw.write_integer_set(31,root,true),ErrorCode::constraint_violation,0); REQUIRE(gw.cursor_bit()==0);
    Bits gapbytes; model(gapbytes,31,1,181); gapbytes.align(); DecodeContext gd; auto made=BitReader::make(gapbytes.data,gd); REQUIRE(made); auto gr=std::move(made).value(); error(gr.read_integer_set(root,true),ErrorCode::constraint_violation,0); REQUIRE(gr.cursor_bit()==0 && gd.wire_bits()==0);
    const IntegerInterval bad[]={{1,30},{30,40}}; EncodeContext invalid; BitWriter iw(invalid); error(iw.write_integer_set(1,bad,true),ErrorCode::invalid_argument,0);
}
}
void* operator new(std::size_t n) { if(allocation_fails) { allocation_fails=false; throw std::bad_alloc(); } if(void* p=std::malloc(n?n:1)) return p; throw std::bad_alloc(); }
void* operator new[](std::size_t n) { return ::operator new(n); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p,std::size_t) noexcept { std::free(p); }
void operator delete[](void* p,std::size_t) noexcept { std::free(p); }
int main() { vectors(); malformed(); state_budget_and_gap(); std::puts("PASS atomic extensible INTEGER / permitted root set"); }
