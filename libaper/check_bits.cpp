#include "runtime.hpp"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <new>
#ifndef NDEBUG
#error "BIT STRING checks deliberately run under NDEBUG."
#endif
namespace {
using namespace nrforge::aper;
bool fail_next_allocation=false;
#define REQUIRE(x) do { if(!(x)) { std::fprintf(stderr,"check_bits:%d: %s\n",__LINE__,#x); std::abort(); } } while(0)
template<class T> void error(const T& result,ErrorCode code,std::size_t offset) {
    REQUIRE(!result);
    if(result.error().code!=code || result.error().bit_offset!=offset)
        std::fprintf(stderr,"expected %d@%zu got %d@%zu\n",static_cast<int>(code),offset,static_cast<int>(result.error().code),result.error().bit_offset);
    REQUIRE(result.error().code==code && result.error().bit_offset==offset);
}
struct Bits {
    std::vector<std::byte> bytes; std::size_t count=0;
    void bit(bool v) { if(count%8==0) bytes.push_back(std::byte{0}); if(v) bytes.back()|=static_cast<std::byte>(0x80u>>(count%8)); ++count; }
    void number(std::size_t n,unsigned width) { for(unsigned i=width;i;--i) bit(((n>>(i-1))&1u)!=0); }
    void align() { while(count%8) bit(false); }
    void prefix(unsigned n) { for(unsigned i=0;i<n;++i) bit(true); }
    void payload(const BitString& data) { for(std::size_t i=0;i<data.bit_count;++i) bit((std::to_integer<unsigned>(data.octets[i/8])&(0x80u>>(i%8)))!=0); }
};
struct Case { std::size_t lower,upper; unsigned width; bool length_align,payload_align,unconstrained; };
constexpr std::array<Case,16> layouts{{
    {0,0,0,false,false,false},{1,1,0,false,false,false},{2,2,0,false,false,false},
    {7,7,0,false,false,false},{8,8,0,false,false,false},{9,9,0,false,false,false},
    {16,16,0,false,false,false},{17,17,0,true,false,false},{65535,65535,0,true,false,false},
    {0,1,1,false,true,false},{1,3,2,false,true,false},{0,254,8,false,true,false},
    {0,255,8,true,true,false},{0,256,16,true,true,false},{0,65535,16,true,true,false},{0,0,0,true,true,true}
}};
BitString payload(std::size_t n) {
    BitString result{{},n}; result.octets.resize(n/8+(n%8!=0),std::byte{0});
    for(std::size_t i=0;i<n;++i) if(i%3!=1) result.octets[i/8]|=static_cast<std::byte>(0x80u>>(i%8));
    return result;
}
void vectors() {
    for(const auto& shape:layouts) for(unsigned residue=0;residue<8;++residue) {
        const std::array<std::size_t,3> counts=shape.unconstrained ? std::array<std::size_t,3>{0,128,16383} :
            std::array<std::size_t,3>{shape.lower,(shape.lower+shape.upper)/2,shape.upper};
        for(auto n:counts) {
            const auto data=payload(n); Bits expected; expected.prefix(residue);
            if(shape.length_align) expected.align();
            if(shape.unconstrained) expected.number(n<128?n:n|0x8000u,n<128?8:16);
            else expected.number(n-shape.lower,shape.width);
            if(shape.payload_align) expected.align();
            expected.payload(data);
            const auto end=expected.count; expected.bit(true); expected.align();
            EncodeContext ec; BitWriter w(ec); FieldWriter fields(w);
            for(unsigned i=0;i<residue;++i) REQUIRE(fields.write_bit(true));
            REQUIRE(fields.write_bit_string(data,shape.lower,shape.upper,shape.unconstrained));
            REQUIRE(w.cursor_bit()==end && ec.wire_bits()==end && ec.collection_elements()==0);
            REQUIRE(fields.write_bit(true)); auto complete=w.finish(); REQUIRE(complete && complete.value().octets==expected.bytes);
            auto input=expected.bytes; DecodeContext dc; auto made=BitReader::make(input,dc); REQUIRE(made); auto r=std::move(made).value(); FieldReader read(r);
            for(unsigned i=0;i<residue;++i) REQUIRE(read.read_bit().value());
            auto result=read.read_bit_string_owned(shape.lower,shape.upper,shape.unconstrained);
            REQUIRE(result && result.value().bit_count==n && result.value().octets==data.octets);
            REQUIRE(r.cursor_bit()==end && dc.wire_bits()==end && dc.collection_elements()==0);
            REQUIRE(read.read_bit().value()); REQUIRE(r.validate_complete_value());
            input.clear(); input.shrink_to_fit(); auto copy=result.value(); auto moved=std::move(result).value();
            if(n) moved.octets[0]^=std::byte{0x80};
            REQUIRE(copy.octets==data.octets && copy.bit_count==n);
        }
    }
    for(auto n:{std::size_t{127},std::size_t{128}}) {
        const auto data=payload(n); EncodeContext ec; BitWriter w(ec); REQUIRE(w.write_bit_string(data,0,0,true)); auto result=w.finish();
        REQUIRE(result && result.value().octets[0]==(n==127?std::byte{127}:std::byte{128}));
        if(n==128) REQUIRE(result.value().octets[1]==std::byte{128});
    }
}
void failures() {
    // Prefix 1, length 101 (5 bits), four zero alignment bits, value 10101.
    const std::array<std::byte,2> literal{std::byte{0xd0},std::byte{0xa8}};
    const BitString data{{std::byte{0xa8}},5};
    for(std::size_t limit=1;limit<13;++limit) {
        DecodeContext dc; auto made=BitReader::make_bounded_for_test(literal,limit,dc); REQUIRE(made); auto r=std::move(made).value(); REQUIRE(r.read_bit());
        error(r.read_bit_string_owned(0,7),ErrorCode::truncated_input,limit);
        REQUIRE(r.cursor_bit()==1 && dc.wire_bits()==1); error(r.read_bit(),ErrorCode::truncated_input,limit);
    }
    for(std::size_t bit=4;bit<8;++bit) {
        auto input=literal; input[0]|=static_cast<std::byte>(0x80u>>bit); DecodeContext dc; auto made=BitReader::make(input,dc); REQUIRE(made); auto r=std::move(made).value(); REQUIRE(r.read_bit());
        error(r.read_bit_string_owned(0,7),ErrorCode::nonzero_padding,bit); REQUIRE(r.cursor_bit()==1 && dc.wire_bits()==1);
    }
    for(std::size_t bit=1;bit<8;++bit) {
        const std::array<std::byte,4> base{std::byte{0x80},std::byte{0xb6},std::byte{0xdb},std::byte{0x80}};
        auto input=base; input[0]|=static_cast<std::byte>(0x80u>>bit); DecodeContext dc; auto made=BitReader::make(input,dc); REQUIRE(made); auto r=std::move(made).value(); REQUIRE(r.read_bit());
        error(r.read_bit_string_owned(17,17),ErrorCode::nonzero_padding,bit); REQUIRE(r.cursor_bit()==1 && dc.wire_bits()==1);
    }
    for(auto first:{std::byte{0xc0},std::byte{0xc1},std::byte{0xff}}) {
        const std::array input{first}; DecodeContext dc; auto made=BitReader::make(input,dc); REQUIRE(made); auto r=std::move(made).value();
        error(r.read_bit_string_owned(0,0,true),ErrorCode::resource_limit,0); REQUIRE(r.cursor_bit()==0 && dc.wire_bits()==0);
    }
    {
        const std::array input{std::byte{0x80},std::byte{0}}; DecodeContext dc; auto made=BitReader::make(input,dc); REQUIRE(made); auto r=std::move(made).value();
        error(r.read_bit_string_owned(0,0,true),ErrorCode::constraint_violation,0);
    }
    {
        const std::array input{std::byte{0x80}}; DecodeContext dc; auto made=BitReader::make(input,dc); REQUIRE(made); auto r=std::move(made).value();
        error(r.read_bit_string_owned(0,0,true),ErrorCode::truncated_input,8);
    }
    for(auto malformed:{BitString{{},1},BitString{{std::byte{0}},0},BitString{{std::byte{0x81}},1},BitString{{std::byte{0},std::byte{0}},1}}) {
        EncodeContext ec; BitWriter w(ec); REQUIRE(w.write_bit(true)); error(w.write_bit_string(malformed,0,7),ErrorCode::constraint_violation,1);
        REQUIRE(w.cursor_bit()==1 && ec.wire_bits()==1 && ec.logical_output_octets()==1); error(w.write_bit(false),ErrorCode::constraint_violation,1);
    }
    for(int bad=0;bad<3;++bad) {
        EncodeContext ec; BitWriter w(ec); error(w.write_bit_string(data,bad==0?9:0,bad==0?1:bad==1?65536:1,bad==2),ErrorCode::invalid_argument,0);
        DecodeContext dc; auto made=BitReader::make(literal,dc); REQUIRE(made); auto r=std::move(made).value(); error(r.read_bit_string_owned(bad==0?9:0,bad==0?1:bad==1?65536:1,bad==2),ErrorCode::invalid_argument,0);
    }
    { EncodeContext ec; BitWriter w(ec); error(w.write_bit_string(payload(16384),0,0,true),ErrorCode::resource_limit,0); }
    for(int budget=0;budget<2;++budget) {
        Limits limits{}; limits.max_output_octets=2; limits.max_wire_bits=13;
        if(budget==0) --limits.max_output_octets; else --limits.max_wire_bits;
        EncodeContext ec(limits); BitWriter w(ec); REQUIRE(w.write_bit(true)); error(w.write_bit_string(data,0,7),ErrorCode::resource_limit,1);
        REQUIRE(w.cursor_bit()==1 && ec.wire_bits()==1 && ec.logical_output_octets()==1);
    }
    {
        Limits limits{}; limits.max_wire_bits=12; DecodeContext dc(limits); auto made=BitReader::make(literal,dc); REQUIRE(made); auto r=std::move(made).value(); REQUIRE(r.read_bit());
        error(r.read_bit_string_owned(0,7),ErrorCode::resource_limit,1); REQUIRE(r.cursor_bit()==1 && dc.wire_bits()==1);
    }
    {
        Limits limits{}; limits.max_wire_bits=13; limits.max_output_octets=2; limits.max_input_octets=2;
        EncodeContext ec(limits); BitWriter w(ec); REQUIRE(w.write_bit(true)); REQUIRE(w.write_bit_string(data,0,7)); REQUIRE(w.cursor_bit()==13);
        // Complete padding needs three additional bits, separately budgeted.
        error(w.finish(),ErrorCode::resource_limit,13);
        DecodeContext dc(limits); auto made=BitReader::make(literal,dc); REQUIRE(made); auto r=std::move(made).value(); REQUIRE(r.read_bit()); REQUIRE(r.read_bit_string_owned(0,7));
        error(r.validate_complete_value(),ErrorCode::resource_limit,13);
    }
    {
        EncodeContext ec; BitWriter w(ec); REQUIRE(w.write_bit(true)); fail_next_allocation=true; error(w.write_bit_string(data,0,7),ErrorCode::allocation_failure,1); REQUIRE(!fail_next_allocation);
        REQUIRE(w.cursor_bit()==1 && ec.wire_bits()==1 && ec.logical_output_octets()==1);
        DecodeContext dc; auto made=BitReader::make(literal,dc); REQUIRE(made); auto r=std::move(made).value(); REQUIRE(r.read_bit()); fail_next_allocation=true;
        error(r.read_bit_string_owned(0,7),ErrorCode::allocation_failure,1); REQUIRE(!fail_next_allocation); REQUIRE(r.cursor_bit()==1 && dc.wire_bits()==1);
    }
}
void lifecycle_and_children() {
    const BitString data{{std::byte{0xa8}},5};
    EncodeContext ec; BitWriter original(ec); BitWriter w(std::move(original)); error(original.write_bit_string(data,9,1),ErrorCode::invalid_state,0);
    REQUIRE(w.write_bit_string(data,5,5)); REQUIRE(w.finish()); error(w.write_bit_string(data,9,1),ErrorCode::invalid_state,8); REQUIRE(!ec.failed());
    DecodeContext dc; auto made=BitReader::make(data.octets,dc); REQUIRE(made); auto original_reader=std::move(made).value(); auto r=std::move(original_reader);
    error(original_reader.read_bit_string_owned(9,1),ErrorCode::invalid_state,0); REQUIRE(r.read_bit_string_owned(5,5)); REQUIRE(r.validate_complete_value());
    error(r.read_bit_string_owned(9,1),ErrorCode::invalid_state,8); REQUIRE(!dc.failed());
    auto out=encode_complete(data,Limits{},[&](FieldWriter& f) { return f.write_known_open_type([&](FieldWriter& child){ return child.write_bit_string(data,0,7); }); });
    const std::vector<std::byte> expected{std::byte{2},std::byte{0xa0},std::byte{0xa8}}; REQUIRE(out && out.value().octets==expected);
    auto got=decode_complete<BitString>(expected,Limits{},[](FieldReader& f) { return f.read_known_open_type<BitString>([](FieldReader& child){ return child.read_bit_string_owned(0,7); }); });
    REQUIRE(got && got.value().octets==data.octets && got.value().bit_count==5);
    auto bad=expected; bad[1]|=std::byte{1};
    auto failed=decode_complete<BitString>(bad,Limits{},[](FieldReader& f) {
        auto first=f.read_known_open_type<BitString>([](FieldReader& child){ return child.read_bit_string_owned(0,7); }); REQUIRE(!first);
        (void)f.read_bit_string_owned(0,0); return Result<BitString>::success({});
    }); error(failed,ErrorCode::nonzero_padding,15);
}
}
void* operator new(std::size_t n) { if(fail_next_allocation) { fail_next_allocation=false; throw std::bad_alloc(); } if(auto p=std::malloc(n?n:1)) return p; throw std::bad_alloc(); }
void* operator new[](std::size_t n) { return ::operator new(n); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p,std::size_t) noexcept { std::free(p); }
void operator delete[](void* p,std::size_t) noexcept { std::free(p); }
int main() { vectors(); failures(); lifecycle_and_children(); std::puts("PASS atomic owned BIT STRING layouts, canonical storage, errors, budgets and lifecycle"); }
