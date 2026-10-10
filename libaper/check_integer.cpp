#include "runtime.hpp"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <new>
#ifndef NDEBUG
#error "Bounded INTEGER checks deliberately run under NDEBUG."
#endif
namespace {
using namespace nrforge::aper;
bool fail_next_allocation=false;
#define REQUIRE(x) do { if(!(x)) { std::fprintf(stderr,"check_integer:%d: %s\n",__LINE__,#x); std::abort(); } } while(0)
template<class T> void error(const T& result,ErrorCode code,std::size_t offset) {
    REQUIRE(!result);
    if(result.error().code!=code || result.error().bit_offset!=offset)
        std::fprintf(stderr,"expected %d@%zu got %d@%zu\n",static_cast<int>(code),offset,static_cast<int>(result.error().code),result.error().bit_offset);
    REQUIRE(result.error().code==code && result.error().bit_offset==offset);
}
struct Bits {
    std::vector<std::byte> bytes; std::size_t count=0;
    void bit(bool v) { if(count%8==0) bytes.push_back(std::byte{0}); if(v) bytes.back()|=static_cast<std::byte>(0x80u>>(count%8)); ++count; }
    void number(std::uint64_t n,unsigned width) { for(unsigned i=width;i;--i) bit(((n>>(i-1))&1u)!=0); }
    void align() { while(count%8) bit(false); }
    void prefix(unsigned n) { for(unsigned i=0;i<n;++i) bit(true); }
};
struct Case { std::uint64_t lower,upper; unsigned width,selector; bool aligned; };
constexpr auto umax=std::numeric_limits<std::uint64_t>::max();
constexpr auto imin=std::numeric_limits<std::int64_t>::min();
constexpr auto imax=std::numeric_limits<std::int64_t>::max();
constexpr std::array<Case,17> cases{{
    {0,0,0,0,false},{umax,umax,0,0,false},{5,6,1,0,false},{1,3,2,0,false},
    {0,127,7,0,false},{5,259,8,0,false},{5,260,8,0,true},
    {5,261,16,0,true},{5,65540,16,0,true},{5,65541,0,2,true},
    {0,0xffffff,0,2,true},{0,0x1000000,0,2,true},
    {0,0xffffffff,0,2,true},{0,0x100000000,0,3,true},
    {0,0xffffffffff,0,3,true},{umax-65536,umax,0,2,true},{0,umax,0,3,true}
}};
void reference(Bits& bits,const Case& shape,std::uint64_t offset) {
    unsigned width=shape.width;
    if(shape.selector) {
        unsigned octets=1; for(auto rest=offset>>8;rest;rest>>=8) ++octets;
        bits.number(octets-1,shape.selector); width=octets*8;
    }
    if(shape.aligned) bits.align();
    bits.number(offset,width);
}
void unsigned_vectors() {
    for(const auto& shape:cases) for(unsigned residue=0;residue<8;++residue) {
        const auto maximum=shape.upper-shape.lower;
        for(auto offset:std::array<std::uint64_t,4>{0,maximum?1u:0u,maximum/2,maximum}) {
            Bits expected; expected.prefix(residue); reference(expected,shape,offset);
            const auto end=expected.count; expected.bit(true); expected.align();
            EncodeContext ec; BitWriter w(ec); FieldWriter fields(w);
            for(unsigned i=0;i<residue;++i) REQUIRE(fields.write_bit(true));
            REQUIRE(fields.write_bounded_uint(shape.lower+offset,shape.lower,shape.upper));
            REQUIRE(w.cursor_bit()==end && ec.wire_bits()==end && ec.collection_elements()==0);
            REQUIRE(fields.write_bit(true)); auto encoded=w.finish(); REQUIRE(encoded && encoded.value().octets==expected.bytes);
            DecodeContext dc; auto made=BitReader::make(expected.bytes,dc); REQUIRE(made); auto r=std::move(made).value(); FieldReader read(r);
            for(unsigned i=0;i<residue;++i) REQUIRE(read.read_bit().value());
            auto value=read.read_bounded_uint(shape.lower,shape.upper); REQUIRE(value && value.value()==shape.lower+offset);
            REQUIRE(r.cursor_bit()==end && dc.wire_bits()==end && dc.collection_elements()==0);
            REQUIRE(read.read_bit().value()); REQUIRE(r.validate_complete_value());
        }
    }
    // New zero-based intervals retain the established N1 byte contract.
    for(unsigned width:{8u,16u,32u,40u}) for(unsigned residue=0;residue<8;++residue) {
        const auto maximum=(std::uint64_t{1}<<width)-1;
        for(auto value:{std::uint64_t{0},std::uint64_t{255},maximum}) {
            EncodeContext a,b; BitWriter old(a),fresh(b);
            for(unsigned i=0;i<residue;++i) { REQUIRE(old.write_bit(true)); REQUIRE(fresh.write_bit(true)); }
            REQUIRE(old.write_constrained_uint(value,width)); REQUIRE(fresh.write_bounded_uint(value,0,maximum));
            auto x=old.finish(),y=fresh.finish(); REQUIRE(x && y && x.value().octets==y.value().octets);
        }
    }
}
void signed_vectors() {
    struct SignedCase { std::int64_t lower,upper; std::array<std::int64_t,4> values; unsigned width,selector; bool aligned; };
    const std::array<SignedCase,7> shapes{{
        {imin,imin,{imin,imin,imin,imin},0,0,false},
        {imax,imax,{imax,imax,imax,imax},0,0,false},
        {-5,5,{-5,-1,0,5},4,0,false},
        {-255,0,{-255,-1,-128,0},8,0,true},
        {-65536,-1,{-65536,-65535,-256,-1},16,0,true},
        {imin,imin+65536,{imin,imin+1,imin+65535,imin+65536},0,2,true},
        {imin,imax,{imin,-1,0,imax},0,3,true}
    }};
    for(const auto& shape:shapes) for(unsigned residue=0;residue<8;++residue) for(auto value:shape.values) {
        // Unsigned modular subtraction computes the mathematical non-negative
        // offset even for MIN..MAX; independent of production's ordered map.
        const auto offset=static_cast<std::uint64_t>(value)-static_cast<std::uint64_t>(shape.lower);
        Bits expected; expected.prefix(residue); reference(expected,{0,0,shape.width,shape.selector,shape.aligned},offset);
        const auto end=expected.count; expected.bit(true); expected.align();
        EncodeContext ec; BitWriter w(ec); FieldWriter fields(w);
        for(unsigned i=0;i<residue;++i) REQUIRE(fields.write_bit(true));
        REQUIRE(fields.write_bounded_int(value,shape.lower,shape.upper));
        REQUIRE(w.cursor_bit()==end); REQUIRE(fields.write_bit(true)); auto out=w.finish(); REQUIRE(out && out.value().octets==expected.bytes);
        DecodeContext dc; auto made=BitReader::make(expected.bytes,dc); REQUIRE(made); auto r=std::move(made).value(); FieldReader read(r);
        for(unsigned i=0;i<residue;++i) REQUIRE(read.read_bit().value());
        auto got=read.read_bounded_int(shape.lower,shape.upper); REQUIRE(got && got.value()==value && r.cursor_bit()==end); REQUIRE(read.read_bit().value()); REQUIRE(r.validate_complete_value());
    }
}
void failures() {
    // Prefix1, length-selector10 (three octets), five padding0, offset010000.
    const std::array<std::byte,4> valid{std::byte{0xc0},std::byte{1},std::byte{0},std::byte{0}};
    for(std::size_t limit=1;limit<32;++limit) {
        DecodeContext dc; auto made=BitReader::make_bounded_for_test(valid,limit,dc); REQUIRE(made); auto r=std::move(made).value(); REQUIRE(r.read_bit());
        error(r.read_bounded_uint(0,65536),ErrorCode::truncated_input,limit); REQUIRE(r.cursor_bit()==1 && dc.wire_bits()==1);
        error(r.read_bounded_int(9,1),ErrorCode::truncated_input,limit);
    }
    for(std::size_t bit=3;bit<8;++bit) {
        auto input=valid; input[0]|=static_cast<std::byte>(0x80u>>bit); DecodeContext dc; auto made=BitReader::make(input,dc); REQUIRE(made); auto r=std::move(made).value(); REQUIRE(r.read_bit());
        error(r.read_bounded_uint(0,65536),ErrorCode::nonzero_padding,bit); REQUIRE(r.cursor_bit()==1 && dc.wire_bits()==1);
    }
    {
        const std::array input{std::byte{0xe0}}; DecodeContext dc; auto made=BitReader::make(input,dc); REQUIRE(made); auto r=std::move(made).value(); REQUIRE(r.read_bit());
        error(r.read_bounded_uint(0,65536),ErrorCode::constraint_violation,1); // spare selector3, priority before payload
    }
    for(auto input:{std::vector<std::byte>{std::byte{0xc0}},std::vector<std::byte>{std::byte{0xff},std::byte{0xff}}}) {
        DecodeContext dc; auto made=BitReader::make(input,dc); REQUIRE(made); auto r=std::move(made).value();
        error(r.read_bounded_uint(0,input.size()==1?2:60000),ErrorCode::constraint_violation,0);
    }
    {
        const std::array input{std::byte{0x40},std::byte{0},std::byte{1}}; DecodeContext dc; auto made=BitReader::make(input,dc); REQUIRE(made); auto r=std::move(made).value();
        error(r.read_bounded_uint(0,65536),ErrorCode::constraint_violation,0); // nonminimal two-octet offset1
    }
    for(int budget=0;budget<2;++budget) {
        Limits limits{}; limits.max_output_octets=4; limits.max_wire_bits=32;
        if(budget==0) --limits.max_output_octets; else --limits.max_wire_bits;
        EncodeContext ec(limits); BitWriter w(ec); REQUIRE(w.write_bit(true)); error(w.write_bounded_uint(65536,0,65536),ErrorCode::resource_limit,1);
        REQUIRE(w.cursor_bit()==1 && ec.wire_bits()==1 && ec.logical_output_octets()==1);
    }
    {
        Limits limits{}; limits.max_wire_bits=31; DecodeContext dc(limits); auto made=BitReader::make(valid,dc); REQUIRE(made); auto r=std::move(made).value(); REQUIRE(r.read_bit());
        error(r.read_bounded_uint(0,65536),ErrorCode::resource_limit,1); REQUIRE(r.cursor_bit()==1 && dc.wire_bits()==1);
    }
    {
        Limits limits{}; limits.max_wire_bits=32; limits.max_output_octets=4; limits.max_input_octets=4;
        EncodeContext ec(limits); BitWriter w(ec); REQUIRE(w.write_bit(true)); REQUIRE(w.write_bounded_uint(65536,0,65536)); auto result=w.finish(); REQUIRE(result && result.value().octets==std::vector<std::byte>(valid.begin(),valid.end()));
        DecodeContext dc(limits); auto made=BitReader::make(valid,dc); REQUIRE(made); auto r=std::move(made).value(); REQUIRE(r.read_bit()); REQUIRE(r.read_bounded_uint(0,65536).value()==65536); REQUIRE(r.validate_complete_value());
    }
    {
        EncodeContext ec; BitWriter w(ec); REQUIRE(w.write_bit(true)); fail_next_allocation=true; error(w.write_bounded_uint(umax,0,umax),ErrorCode::allocation_failure,1);
        REQUIRE(!fail_next_allocation && w.cursor_bit()==1 && ec.wire_bits()==1 && ec.logical_output_octets()==1);
    }
    { EncodeContext ec; BitWriter w(ec); error(w.write_bounded_uint(1,9,1),ErrorCode::invalid_argument,0); error(w.write_bit(true),ErrorCode::invalid_argument,0); }
    { EncodeContext ec; BitWriter w(ec); error(w.write_bounded_int(0,9,-1),ErrorCode::invalid_argument,0); }
    { EncodeContext ec; BitWriter w(ec); error(w.write_bounded_uint(4,5,6),ErrorCode::constraint_violation,0); }
    { EncodeContext ec; BitWriter w(ec); error(w.write_bounded_int(imax,imin,-1),ErrorCode::constraint_violation,0); }
    { DecodeContext dc; auto made=BitReader::make(valid,dc); REQUIRE(made); auto r=std::move(made).value(); error(r.read_bounded_uint(9,1),ErrorCode::invalid_argument,0); }
}
void lifecycle_children() {
    EncodeContext ec; BitWriter original(ec); BitWriter w(std::move(original)); error(original.write_bounded_int(0,9,1),ErrorCode::invalid_state,0);
    REQUIRE(w.write_bounded_int(-5,-5,-5)); REQUIRE(w.finish()); error(w.write_bounded_uint(0,9,1),ErrorCode::invalid_state,8); REQUIRE(!ec.failed());
    const std::array input{std::byte{0}}; DecodeContext dc; auto made=BitReader::make(input,dc); REQUIRE(made); auto original_reader=std::move(made).value(); auto r=std::move(original_reader);
    error(original_reader.read_bounded_int(9,1),ErrorCode::invalid_state,0); REQUIRE(r.read_bounded_int(imin,imin).value()==imin); REQUIRE(r.validate_complete_value()); error(r.read_bounded_uint(9,1),ErrorCode::invalid_state,8); REQUIRE(!dc.failed());
    auto encoded=encode_complete(0,Limits{},[](FieldWriter& f) { return f.write_known_open_type([](FieldWriter& child){ return child.write_bounded_int(imin,imin,imax); }); });
    const std::vector<std::byte> expected{std::byte{2},std::byte{0},std::byte{0}}; REQUIRE(encoded && encoded.value().octets==expected);
    auto decoded=decode_complete<std::int64_t>(expected,Limits{},[](FieldReader& f) { return f.read_known_open_type<std::int64_t>([](FieldReader& child){ return child.read_bounded_int(imin,imax); }); }); REQUIRE(decoded && decoded.value()==imin);
    auto bad=expected; bad[1]=std::byte{1};
    auto failed=decode_complete<std::int64_t>(bad,Limits{},[](FieldReader& f) {
        auto first=f.read_known_open_type<std::int64_t>([](FieldReader& child){ return child.read_bounded_int(imin,imax); }); REQUIRE(!first);
        (void)f.read_bounded_int(imin,imax); return Result<std::int64_t>::success(0);
    }); error(failed,ErrorCode::nonzero_padding,15);
}
}
void* operator new(std::size_t n) { if(fail_next_allocation) { fail_next_allocation=false; throw std::bad_alloc(); } if(auto p=std::malloc(n?n:1)) return p; throw std::bad_alloc(); }
void* operator new[](std::size_t n) { return ::operator new(n); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p,std::size_t) noexcept { std::free(p); }
void operator delete[](void* p,std::size_t) noexcept { std::free(p); }
int main() { unsigned_vectors(); signed_vectors(); failures(); lifecycle_children(); std::puts("PASS bounded signed/unsigned INTEGER full-domain vectors, atomic errors, budgets and lifecycle"); }
