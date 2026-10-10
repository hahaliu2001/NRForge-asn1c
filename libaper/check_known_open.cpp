#include "runtime.hpp"
#include <cstdio>
#include <cstdlib>
#include <new>
#include <stdexcept>
#ifndef NDEBUG
#error "Checks must remain active under NDEBUG"
#endif
using namespace nrforge::aper;
namespace {
int allocation_countdown = -1;
#define REQUIRE(x) do { if(!(x)) { std::fprintf(stderr,"known_open:%d: %s\n",__LINE__,#x); std::abort(); } } while(0)
template<class R> void error(const R& r, ErrorCode c, std::size_t p) {
    REQUIRE(!r);
    if(r.error().code!=c || r.error().bit_offset!=p)
        std::fprintf(stderr,"expected %d@%zu got %d@%zu\n",static_cast<int>(c),p,static_cast<int>(r.error().code),r.error().bit_offset);
    REQUIRE(r.error().code==c && r.error().bit_offset==p);
}
std::vector<std::byte> frame(std::size_t n) {
    std::vector<std::byte> out;
    while(n>=16384) {
        const auto blocks=std::min<std::size_t>(4,n/16384);
        out.push_back(static_cast<std::byte>(0xc0u+blocks));
        out.insert(out.end(),blocks*16384,std::byte{0x5a});
        n-=blocks*16384;
    }
    if(n<128) out.push_back(static_cast<std::byte>(n));
    else { out.push_back(static_cast<std::byte>(0x80u+(n>>8))); out.push_back(static_cast<std::byte>(n&255)); }
    out.insert(out.end(),n,std::byte{0x5a});
    return out;
}
void vectors() {
    for(auto n:{std::size_t{1},127ul,128ul,16383ul,16384ul,16385ul,32768ul,49152ul,65536ul,65537ul}) {
        const auto wire=frame(n);
        for(unsigned residue=0;residue<8;++residue) {
            std::vector<std::byte> input;
            if(residue) input.push_back(static_cast<std::byte>(0xffu<<(8-residue)));
            input.insert(input.end(),wire.begin(),wire.end());
            DecodeContext dc; auto made=BitReader::make(input,dc); REQUIRE(made); auto r=std::move(made).value();
            for(unsigned i=0;i<residue;++i) REQUIRE(r.read_bit().value());
            auto decoded=r.read_known_open_type<std::size_t>([&](FieldReader& f) {
                REQUIRE(dc.known_open_depth()==1 && dc.known_open_staging_octets()==n);
                for(std::size_t i=0;i<n;++i) { auto v=f.read_constrained_uint(8); if(!v)return Result<std::size_t>::failure(v.error()); REQUIRE(v.value()==0x5a); }
                return Result<std::size_t>::success(n);
            });
            REQUIRE(decoded && decoded.value()==n && r.validate_complete_value());
            REQUIRE(dc.wire_bits()==input.size()*8 && dc.known_open_depth()==0 && dc.known_open_staging_octets()==0);
            EncodeContext ec; BitWriter w(ec);
            for(unsigned i=0;i<residue;++i) REQUIRE(w.write_bit(true));
            REQUIRE(w.write_known_open_type([&](FieldWriter& f) {
                for(std::size_t i=0;i<n;++i) { auto v=f.write_constrained_uint(0x5a,8); if(!v)return v; }
                return Result<void>::success();
            }));
            auto encoded=w.finish(); REQUIRE(encoded && encoded.value().octets==input);
            REQUIRE(ec.wire_bits()==input.size()*8 && ec.known_open_depth()==0 && ec.known_open_staging_octets()==0);
        }
    }
}
void transactions() {
    const std::vector<std::byte> wire{std::byte{1},std::byte{0x80}};
    for(bool encoding:{false,true}) {
        Limits limits; limits.max_collection_elements=3;
        if(encoding) {
            EncodeContext c(limits); BitWriter w(c); REQUIRE(w.write_bit(true));
            auto result=w.write_known_open_type([&](FieldWriter& f) {
                REQUIRE(f.write_bounded_collection_length(3,3,3));
                REQUIRE(f.write_bit(true));
                return f.record_failure({ErrorCode::constraint_violation,f.cursor_bit()});
            });
            error(result,ErrorCode::constraint_violation,1);
            REQUIRE(w.cursor_bit()==1 && c.collection_elements()==0 && c.wire_bits()==1 && c.known_open_depth()==0 && c.known_open_staging_octets()==0);
        } else {
            DecodeContext c(limits); auto made=BitReader::make(wire,c); auto r=std::move(made).value();
            auto result=r.read_known_open_type<bool>([&](FieldReader& f) {
                REQUIRE(f.read_bounded_collection_length(3,3)); REQUIRE(f.read_bit());
                return Result<bool>::failure({ErrorCode::constraint_violation,f.cursor_bit()});
            });
            error(result,ErrorCode::constraint_violation,9);
            REQUIRE(r.cursor_bit()==0 && c.collection_elements()==0 && c.wire_bits()==0 && c.known_open_depth()==0 && c.known_open_staging_octets()==0);
        }
    }
    for(auto bytes:{std::vector<std::byte>{std::byte{1},std::byte{0x81}},std::vector<std::byte>{std::byte{2},std::byte{0x80},std::byte{0}}}) {
        DecodeContext c; auto made=BitReader::make(bytes,c); auto r=std::move(made).value();
        auto result=r.read_known_open_type<bool>([](FieldReader& f){return f.read_bit();});
        REQUIRE(!result && r.cursor_bit()==0 && c.wire_bits()==0);
        REQUIRE(result.error().code==(bytes.size()==2?ErrorCode::nonzero_padding:ErrorCode::trailing_data));
    }
    EncodeContext c; BitWriter w(c);
    try { (void)w.write_known_open_type([](FieldWriter&)->Result<void>{throw std::runtime_error("test");}); REQUIRE(false); }
    catch(const std::runtime_error&) {}
    error(w.write_bit(false),ErrorCode::invalid_state,0);
    REQUIRE(c.known_open_depth()==0 && c.known_open_staging_octets()==0 && w.cursor_bit()==0);
}
void nesting_aliases_limits() {
    const std::vector<std::byte> wire{std::byte{2},std::byte{1},std::byte{0x80}};
    DecodeContext dc; auto made=BitReader::make(wire,dc); auto r=std::move(made).value();
    auto result=r.read_known_open_type<bool>([&](FieldReader& f) {
        error(r.read_bit(),ErrorCode::invalid_state,0); REQUIRE(!dc.failed());
        error(BitReader::make(wire,dc),ErrorCode::invalid_state,0); REQUIRE(!dc.failed());
        return f.read_known_open_type<bool>([&](FieldReader& inner) {
            REQUIRE(dc.known_open_depth()==2 && dc.known_open_staging_octets()==3);
            return inner.read_bit();
        });
    });
    REQUIRE(result && result.value() && dc.wire_bits()==24);
    for(unsigned mode=0;mode<3;++mode) {
        Limits l; if(mode==0) l.max_known_open_depth=1; if(mode==1)l.max_known_open_staging_octets=2; if(mode==2)l.max_wire_bits=23;
        DecodeContext d(l); auto m=BitReader::make(wire,d); auto reader=std::move(m).value();
        auto value=reader.read_known_open_type<bool>([](FieldReader& f){return f.read_known_open_type<bool>([](FieldReader& inner){return inner.read_bit();});});
        REQUIRE(!value && value.error().code==ErrorCode::resource_limit);
        REQUIRE(reader.cursor_bit()==0 && d.wire_bits()==0 && d.known_open_depth()==0 && d.known_open_staging_octets()==0);
    }
    EncodeContext ec; BitWriter w(ec);
    REQUIRE(w.write_known_open_type([&](FieldWriter& f) {
        error(w.write_bit(false),ErrorCode::invalid_state,0); REQUIRE(!ec.failed());
        return f.write_known_open_type([](FieldWriter& inner){return inner.write_bit(true);});
    }));
    auto complete=w.finish(); REQUIRE(complete && complete.value().octets==wire && ec.wire_bits()==24);
    FieldWriter finished(w); error(finished.record_failure({ErrorCode::constraint_violation,0}),ErrorCode::invalid_state,24);
}
void provenance_and_shared_budgets() {
    const std::vector<std::byte> boolean{std::byte{1},std::byte{0x80}};
    Limits exact; exact.max_wire_bits=16; exact.max_retained_unknown_payload_octets=0; exact.max_retained_unknown_records=0;
    DecodeContext dc(exact); auto made=BitReader::make(boolean,dc); auto reader=std::move(made).value();
    REQUIRE(reader.read_known_open_type<bool>([](FieldReader& f){return f.read_bit();}));
    REQUIRE(dc.wire_bits()==16 && dc.retained_unknown_records()==0);
    for(auto n:{16384ul,16385ul,65536ul,65537ul}) {
        const auto bytes=frame(n);
        DecodeContext c; auto m=BitReader::make(bytes,c); auto r=std::move(m).value();
        auto result=r.read_known_open_type<bool>([&](FieldReader& f) {
            for(std::size_t i=0;i<n;++i) REQUIRE(f.read_constrained_uint(8));
            return f.read_bit();
        });
        // The final determinant's payload starts after its one-byte header;
        // exact fragmented multiples map EOF before terminal zero.
        const auto position=(bytes.size()-(n%16384==0?1:0))*8;
        error(result,ErrorCode::truncated_input,position);
        REQUIRE(c.wire_bits()==0 && r.cursor_bit()==0);
    }
    // Two independently fragmented framing layers: inner logical EOF is
    // before its terminal zero, after crossing the outer determinant gap.
    const auto inner_wire=frame(16384);
    std::vector<std::byte> outer{std::byte{0xc1}};
    outer.insert(outer.end(),inner_wire.begin(),inner_wire.begin()+16384);
    outer.push_back(std::byte{2});
    outer.insert(outer.end(),inner_wire.begin()+16384,inner_wire.end());
    DecodeContext nested_context; auto nested_made=BitReader::make(outer,nested_context);
    auto nested_reader=std::move(nested_made).value();
    auto nested_value=nested_reader.read_known_open_type<bool>([](FieldReader& f) {
        return f.read_known_open_type<bool>([](FieldReader& child) {
            for(std::size_t i=0;i<16384;++i) REQUIRE(child.read_constrained_uint(8));
            return child.read_bit();
        });
    });
    error(nested_value,ErrorCode::truncated_input,16387*8);
    REQUIRE(nested_reader.cursor_bit()==0 && nested_context.wire_bits()==0 && nested_context.known_open_depth()==0);
    // Child retains unknown open payload normally, but a later failure rolls
    // back both retention counters and the intervening bitmap semantic charge.
    const std::vector<std::byte> nested{std::byte{4},std::byte{1},std::byte{1},std::byte{0x5a},std::byte{0}};
    DecodeContext c; auto m=BitReader::make(nested,c); auto r=std::move(m).value();
    auto value=r.read_known_open_type<bool>([&](FieldReader& f) {
        auto bitmap=f.read_sequence_extension_bitmap(); REQUIRE(bitmap && bitmap.value().bit_count==1);
        auto unknown=f.read_open_type_owned(); REQUIRE(unknown && unknown.value().size()==1);
        REQUIRE(c.extension_bitmap_bits()==1 && c.retained_unknown_records()==1 && c.retained_unknown_payload_octets()==1);
        REQUIRE(f.record_failure({ErrorCode::constraint_violation,f.cursor_bit()} ).has_value()==false);
        return Result<bool>::success(true); // ignored helper error cannot publish
    });
    REQUIRE(!value && c.extension_bitmap_bits()==0 && c.retained_unknown_records()==0 && c.retained_unknown_payload_octets()==0 && c.wire_bits()==0);
    const std::vector<std::byte> empty{std::byte{1},std::byte{0}};
    DecodeContext e; auto em=BitReader::make(empty,e); auto er=std::move(em).value();
    REQUIRE(er.read_known_open_type<bool>([](FieldReader&){return Result<bool>::success(false);}));
    EncodeContext ec; BitWriter ew(ec); REQUIRE(ew.write_known_open_type([](FieldWriter&){return Result<void>::success();}));
    auto encoded=ew.finish(); REQUIRE(encoded && encoded.value().octets==empty);
}
void allocations() {
    const std::vector<std::byte> wire{std::byte{1},std::byte{0x80}};
    DecodeContext dc; auto made=BitReader::make(wire,dc); auto r=std::move(made).value();
    allocation_countdown=0;
    auto value=r.read_known_open_type<bool>([](FieldReader& f){return f.read_bit();});
    allocation_countdown=-1;
    error(value,ErrorCode::allocation_failure,0); REQUIRE(r.cursor_bit()==0 && dc.wire_bits()==0 && dc.known_open_staging_octets()==0);
    for(int position=0;position<2;++position) {
        EncodeContext ec; BitWriter w(ec); allocation_countdown=position;
        auto result=w.write_known_open_type([](FieldWriter& f){return f.write_bit(true);});
        allocation_countdown=-1; error(result,ErrorCode::allocation_failure,0);
        REQUIRE(w.cursor_bit()==0 && ec.wire_bits()==0 && ec.logical_output_octets()==0 && ec.known_open_staging_octets()==0 && ec.known_open_depth()==0);
    }
}
}
void* operator new(std::size_t n) {
    if(allocation_countdown==0) { allocation_countdown=-1; throw std::bad_alloc(); }
    if(allocation_countdown>0)--allocation_countdown;
    if(void* p=std::malloc(n?n:1))return p;
    throw std::bad_alloc();
}
void* operator new[](std::size_t n) { return ::operator new(n); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p,std::size_t) noexcept { std::free(p); }
void operator delete[](void* p,std::size_t) noexcept { std::free(p); }
int main() { vectors(); transactions(); nesting_aliases_limits(); provenance_and_shared_budgets(); allocations(); std::puts("known-open PASS"); }
