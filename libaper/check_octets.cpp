#include "runtime.hpp"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <new>
#include <limits>
#ifndef NDEBUG
#error "OCTET STRING checks deliberately run under NDEBUG."
#endif
namespace {
using namespace nrforge::aper;
bool fail_next_allocation = false;
#define REQUIRE(x) do { if(!(x)) { std::fprintf(stderr,"check_octets:%d: %s\n",__LINE__,#x); std::abort(); } } while(0)
template<class T> void error(const T& result,ErrorCode code,std::size_t offset) {
    REQUIRE(!result);
    if(result.error().code != code || result.error().bit_offset != offset)
        std::fprintf(stderr,"expected %d@%zu got %d@%zu\n",static_cast<int>(code),offset,static_cast<int>(result.error().code),result.error().bit_offset);
    REQUIRE(result.error().code == code && result.error().bit_offset == offset);
}
struct Bits {
    std::vector<std::byte> bytes;
    std::size_t count = 0;
    void bit(bool v) { if(count % 8 == 0) bytes.push_back(std::byte{0}); if(v) bytes.back() |= static_cast<std::byte>(0x80u >> (count % 8)); ++count; }
    void number(std::size_t n,unsigned width) { for(unsigned i=width;i;--i) bit(((n >> (i-1)) & 1u) != 0); }
    void align() { while(count % 8) bit(false); }
    void prefix(unsigned n) { for(unsigned i=0;i<n;++i) bit(true); }
    void payload(const std::vector<std::byte>& data) { for(auto b:data) number(std::to_integer<unsigned>(b),8); }
    void complete() { if(!count) bit(false); align(); }
};
struct Case { std::size_t lower,upper; unsigned width; bool length_align,payload_align,unconstrained; };
// Normative boundary layouts supplied independently of the runtime algorithm.
constexpr std::array<Case,13> layouts{{
    {0,0,0,false,false,false},{1,1,0,false,false,false},{2,2,0,false,false,false},
    {3,3,0,true,false,false},{65535,65535,0,true,false,false},
    {0,1,1,false,true,false},{1,3,2,false,true,false},
    {0,254,8,false,true,false},{0,255,8,true,true,false},
    {0,256,16,true,true,false},{65530,65535,3,false,true,false},
    {0,65535,16,true,true,false},{0,0,0,true,true,true}
}};
std::vector<std::byte> payload(std::size_t n) {
    std::vector<std::byte> result(n);
    for(std::size_t i=0;i<n;++i) result[i]=static_cast<std::byte>((i*37+0xa5)&255u);
    return result;
}
void vectors() {
    for(const auto& shape:layouts) for(unsigned residue=0;residue<8;++residue) {
        const std::array<std::size_t,3> counts = shape.unconstrained ? std::array<std::size_t,3>{0,128,16383} :
            std::array<std::size_t,3>{shape.lower,(shape.lower+shape.upper)/2,shape.upper};
        for(auto n:counts) {
            const auto data=payload(n); Bits expected; expected.prefix(residue);
            if(shape.length_align) expected.align();
            if(shape.unconstrained) expected.number(n < 128 ? n : n|0x8000u,n < 128 ? 8 : 16);
            else expected.number(n-shape.lower,shape.width);
            if(shape.payload_align) expected.align();
            expected.payload(data); const auto end=expected.count; expected.complete();
            EncodeContext encoded; BitWriter writer(encoded); FieldWriter fields(writer);
            for(unsigned i=0;i<residue;++i) REQUIRE(fields.write_bit(true));
            REQUIRE(fields.write_octet_string(data,shape.lower,shape.upper,shape.unconstrained));
            REQUIRE(writer.cursor_bit()==end && encoded.wire_bits()==end && encoded.collection_elements()==0);
            auto finished=writer.finish(); REQUIRE(finished && finished.value().octets==expected.bytes);
            auto input=expected.bytes; DecodeContext decoded; auto made=BitReader::make(input,decoded); REQUIRE(made);
            auto reader=std::move(made).value(); FieldReader readfields(reader);
            for(unsigned i=0;i<residue;++i) REQUIRE(readfields.read_bit().value());
            auto got=readfields.read_octet_string_owned(shape.lower,shape.upper,shape.unconstrained); REQUIRE(got && got.value()==data);
            REQUIRE(reader.cursor_bit()==end && decoded.wire_bits()==end && decoded.collection_elements()==0);
            REQUIRE(reader.validate_complete_value());
            std::fill(input.begin(),input.end(),std::byte{0}); input.clear(); REQUIRE(got.value()==data);
            auto copy=got.value(); auto moved=std::move(got).value(); if(n) moved.front() ^= std::byte{1}; REQUIRE(copy==data);
        }
    }
    // One-octet determinant's last value and two-octet determinant's first.
    for(auto n:{std::size_t{127},std::size_t{128}}) {
        auto data=payload(n); EncodeContext ctx; BitWriter w(ctx); REQUIRE(w.write_octet_string(data,0,0,true));
        auto out=w.finish(); REQUIRE(out && out.value().octets.size()==n+(n==127?1:2));
        REQUIRE(out.value().octets[0]==(n==127?std::byte{0x7f}:std::byte{0x80}));
        if(n==128) REQUIRE(out.value().octets[1]==std::byte{0x80});
    }
}
void failures() {
    const auto data=payload(3);
    // Prefix + two-bit constrained length + alignment + three-octet payload.
    const std::array<std::byte,4> literal{std::byte{0xe0},std::byte{0xa5},std::byte{0xca},std::byte{0xef}};
    for(std::size_t limit=1;limit<32;++limit) {
        DecodeContext c; auto made=BitReader::make_bounded_for_test(literal,limit,c); REQUIRE(made);
        auto r=std::move(made).value(); REQUIRE(r.read_bit().value());
        error(r.read_octet_string_owned(0,3),ErrorCode::truncated_input,limit);
        REQUIRE(r.cursor_bit()==1 && c.wire_bits()==1);
        error(r.read_bit(),ErrorCode::truncated_input,limit);
    }
    for(std::size_t bit=3;bit<8;++bit) {
        auto bad=literal; bad[0] |= static_cast<std::byte>(0x80u>>bit);
        DecodeContext c; auto made=BitReader::make(bad,c); REQUIRE(made); auto r=std::move(made).value(); REQUIRE(r.read_bit());
        error(r.read_octet_string_owned(0,3),ErrorCode::nonzero_padding,bit); REQUIRE(r.cursor_bit()==1 && c.wire_bits()==1);
    }
    for(auto first:{std::byte{0xc0},std::byte{0xc1},std::byte{0xff}}) {
        const std::array input{first}; DecodeContext c; auto made=BitReader::make(input,c); REQUIRE(made); auto r=std::move(made).value();
        error(r.read_octet_string_owned(0,0,true),ErrorCode::resource_limit,0); REQUIRE(r.cursor_bit()==0 && c.wire_bits()==0);
    }
    {
        const std::array input{std::byte{0x80},std::byte{0}};
        DecodeContext c; auto made=BitReader::make(input,c); REQUIRE(made); auto r=std::move(made).value();
        error(r.read_octet_string_owned(0,0,true),ErrorCode::constraint_violation,0);
    }
    for(int budget=0;budget<2;++budget) {
        Limits limits{}; limits.max_output_octets=4; limits.max_wire_bits=32;
        if(budget==0) --limits.max_output_octets; else --limits.max_wire_bits;
        EncodeContext c(limits); BitWriter w(c); REQUIRE(w.write_bit(true));
        error(w.write_octet_string(data,0,3),ErrorCode::resource_limit,1);
        REQUIRE(w.cursor_bit()==1 && c.wire_bits()==1 && c.logical_output_octets()==1);
    }
    {
        Limits limits{}; limits.max_input_octets=4; limits.max_output_octets=4; limits.max_wire_bits=32;
        EncodeContext ec(limits); BitWriter w(ec); REQUIRE(w.write_bit(true)); REQUIRE(w.write_octet_string(data,0,3));
        auto complete=w.finish(); REQUIRE(complete && complete.value().octets==std::vector<std::byte>(literal.begin(),literal.end()));
        DecodeContext dc(limits); auto made=BitReader::make(literal,dc); REQUIRE(made); auto r=std::move(made).value(); REQUIRE(r.read_bit());
        REQUIRE(r.read_octet_string_owned(0,3).value()==data); REQUIRE(r.validate_complete_value());
    }
    {
        Limits limits{}; limits.max_wire_bits=31; DecodeContext c(limits); auto made=BitReader::make(literal,c); REQUIRE(made);
        auto r=std::move(made).value(); REQUIRE(r.read_bit()); error(r.read_octet_string_owned(0,3),ErrorCode::resource_limit,1);
        REQUIRE(r.cursor_bit()==1 && c.wire_bits()==1);
    }
    {
        Limits limits{}; limits.max_input_octets=3; DecodeContext c(limits); error(BitReader::make(literal,c),ErrorCode::resource_limit,0);
    }
    for(int invalid=0;invalid<3;++invalid) {
        EncodeContext c; BitWriter w(c); REQUIRE(w.write_bit(true));
        error(w.write_octet_string(data,invalid==0?4:0,invalid==0?3:invalid==1?65536:1,invalid==2),ErrorCode::invalid_argument,1);
        DecodeContext dc; auto made=BitReader::make(literal,dc); REQUIRE(made); auto r=std::move(made).value(); REQUIRE(r.read_bit());
        error(r.read_octet_string_owned(invalid==0?4:0,invalid==0?3:invalid==1?65536:1,invalid==2),ErrorCode::invalid_argument,1);
        error(r.read_bit(),ErrorCode::invalid_argument,1);
    }
    {
        const std::array input{std::byte{0xc0}}; DecodeContext dc; auto made=BitReader::make(input,dc); REQUIRE(made); auto r=std::move(made).value();
        // Domain SIZE(1..3) has a spare two-bit offset 3.
        error(r.read_octet_string_owned(1,3),ErrorCode::constraint_violation,0);
        const std::array long_prefix{std::byte{0x80}}; DecodeContext truncated; auto short_made=BitReader::make(long_prefix,truncated); REQUIRE(short_made);
        auto short_reader=std::move(short_made).value(); error(short_reader.read_octet_string_owned(0,0,true),ErrorCode::truncated_input,8);
    }
    {
        EncodeContext c; BitWriter w(c); error(w.write_octet_string(data,1,2),ErrorCode::constraint_violation,0);
        error(w.write_bit(false),ErrorCode::constraint_violation,0);
    }
    {
        const auto large=payload(16384); EncodeContext c; BitWriter w(c);
        error(w.write_octet_string(large,0,0,true),ErrorCode::resource_limit,0);
    }
    // Actual allocation failures occur after all checks and before publication.
    {
        EncodeContext c; BitWriter w(c); REQUIRE(w.write_bit(true)); fail_next_allocation=true;
        error(w.write_octet_string(data,0,3),ErrorCode::allocation_failure,1); REQUIRE(!fail_next_allocation);
        REQUIRE(w.cursor_bit()==1 && c.wire_bits()==1 && c.logical_output_octets()==1);
    }
    {
        DecodeContext c; auto made=BitReader::make(literal,c); REQUIRE(made); auto r=std::move(made).value(); REQUIRE(r.read_bit());
        fail_next_allocation=true; error(r.read_octet_string_owned(0,3),ErrorCode::allocation_failure,1); REQUIRE(!fail_next_allocation);
        REQUIRE(r.cursor_bit()==1 && c.wire_bits()==1);
    }
}
void lifecycle_and_children() {
    const auto data=payload(1);
    EncodeContext c; BitWriter original(c); BitWriter w(std::move(original));
    error(original.write_octet_string(data,9,1),ErrorCode::invalid_state,0); REQUIRE(!c.failed());
    REQUIRE(w.write_octet_string(data,1,1)); REQUIRE(w.finish());
    error(w.write_octet_string(data,9,1),ErrorCode::invalid_state,8); REQUIRE(!c.failed());
    DecodeContext dc; auto made=BitReader::make(data,dc); REQUIRE(made); auto original_reader=std::move(made).value(); auto r=std::move(original_reader);
    error(original_reader.read_octet_string_owned(9,1),ErrorCode::invalid_state,0); REQUIRE(r.read_octet_string_owned(1,1)); REQUIRE(r.validate_complete_value());
    error(r.read_octet_string_owned(9,1),ErrorCode::invalid_state,8); REQUIRE(!dc.failed());
    auto encoded=encode_complete(data,Limits{},[&](FieldWriter& f) {
        return f.write_known_open_type([&](FieldWriter& child){ return child.write_octet_string(data,0,3); });
    });
    const std::vector<std::byte> expected{std::byte{2},std::byte{0x40},std::byte{0xa5}};
    REQUIRE(encoded && encoded.value().octets==expected);
    auto decoded=decode_complete<std::vector<std::byte>>(expected,Limits{},[](FieldReader& f) {
        return f.read_known_open_type<std::vector<std::byte>>([](FieldReader& child){ return child.read_octet_string_owned(0,3); });
    }); REQUIRE(decoded && decoded.value()==data);
    auto bad=expected; bad[1] |= std::byte{1};
    auto failed=decode_complete<std::vector<std::byte>>(bad,Limits{},[](FieldReader& f) {
        auto first=f.read_known_open_type<std::vector<std::byte>>([](FieldReader& child){ return child.read_octet_string_owned(0,3); });
        REQUIRE(!first); (void)f.read_octet_string_owned(0,0); return Result<std::vector<std::byte>>::success({});
    }); error(failed,ErrorCode::nonzero_padding,15);
}
}
void* operator new(std::size_t n) { if(fail_next_allocation) { fail_next_allocation=false; throw std::bad_alloc(); } if(auto p=std::malloc(n?n:1)) return p; throw std::bad_alloc(); }
void* operator new[](std::size_t n) { return ::operator new(n); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p,std::size_t) noexcept { std::free(p); }
void operator delete[](void* p,std::size_t) noexcept { std::free(p); }
int main() { vectors(); failures(); lifecycle_and_children(); std::puts("PASS atomic owned OCTET STRING vectors, boundaries, errors, budgets and lifecycle"); }
