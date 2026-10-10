#include "runtime.hpp"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <new>

#ifndef NDEBUG
#error "Checks must remain active under NDEBUG."
#endif
namespace {
using namespace nrforge::aper;
bool fail_next_allocation = false;
#define REQUIRE(x) do { if(!(x)) { std::fprintf(stderr,"check_collections:%d: %s\n",__LINE__,#x); std::abort(); } } while(0)
template<class R> void error(const R& value, ErrorCode code, std::size_t offset) {
    REQUIRE(!value);
    if(value.error().code != code || value.error().bit_offset != offset)
        std::fprintf(stderr,"expected(%d,%zu), got(%d,%zu)\n",static_cast<int>(code),offset,
                     static_cast<int>(value.error().code),value.error().bit_offset);
    REQUIRE(value.error().code == code && value.error().bit_offset == offset);
}
struct Case { std::size_t lower, upper; unsigned width; bool aligned; };
constexpr std::array<Case, 14> cases{{
    {0,0,0,false}, {3,3,0,false}, {65535,65535,0,false},
    {0,1,1,false}, {2,4,2,false}, {0,127,7,false}, {0,254,8,false},
    {2,256,8,false}, {0,255,8,true}, {2,257,8,true}, {0,256,16,true},
    {2,258,16,true}, {0,65535,16,true}, {65530,65535,3,false}
}};
// Layouts above are fixed normative facts, not computed with production helpers.
struct Bits {
    std::vector<std::byte> bytes;
    std::size_t size = 0;
    void bit(bool value) {
        if(size % 8 == 0) bytes.push_back(std::byte{0});
        if(value) bytes.back() |= static_cast<std::byte>(0x80u >> (size % 8));
        ++size;
    }
    void prefix(unsigned residue) { for(unsigned i=0;i<residue;++i) bit(true); }
    void number(std::uint64_t value, unsigned width) {
        for(unsigned i=width;i>0;--i) bit(((value>>(i-1))&1u)!=0);
    }
    void align() { while(size % 8) bit(false); }
    void length(const Case& test, std::size_t count) {
        if(test.aligned) align();
        number(count-test.lower,test.width);
    }
    void set(std::size_t position) { bytes[position/8] |= static_cast<std::byte>(0x80u>>(position%8)); }
    std::vector<std::byte> complete() {
        if(size==0) { bytes.push_back(std::byte{0}); size=8; }
        align();
        return bytes;
    }
};
void prefix(BitReader& reader,unsigned residue) {
    for(unsigned i=0;i<residue;++i) REQUIRE(reader.read_bit().value());
}
void prefix(BitWriter& writer,unsigned residue) {
    for(unsigned i=0;i<residue;++i) REQUIRE(writer.write_bit(true));
}
void vectors_and_budgets() {
    for(const auto& test : cases) for(unsigned residue=0;residue<8;++residue) {
        for(auto count : {test.lower,(test.lower+test.upper)/2,test.upper}) {
            Bits expected;
            expected.prefix(residue);
            expected.length(test,count);
            const auto end=expected.size;
            const auto wire=expected.complete();
            EncodeContext context;
            BitWriter writer(context);
            prefix(writer,residue);
            FieldWriter fields(writer);
            REQUIRE(fields.write_bounded_collection_length(count,test.lower,test.upper));
            REQUIRE(writer.cursor_bit()==end && context.wire_bits()==end && context.collection_elements()==count);
            REQUIRE(context.logical_output_octets()==(end+7)/8);
            const auto complete=writer.finish();
            REQUIRE(complete && complete.value().octets==wire);
            REQUIRE(complete.value().last_field_end_bit==end);
            DecodeContext decoded_context;
            auto made=BitReader::make(wire,decoded_context);
            REQUIRE(made);
            auto reader=std::move(made).value();
            prefix(reader,residue);
            FieldReader input(reader);
            auto value=input.read_bounded_collection_length(test.lower,test.upper);
            REQUIRE(value && value.value()==count && reader.cursor_bit()==end);
            REQUIRE(decoded_context.wire_bits()==end && decoded_context.collection_elements()==count);
            REQUIRE(reader.validate_complete_value());
            for(unsigned budget=0;budget<3;++budget) for(bool less:{false,true}) {
                if((budget==0 && count==0) || (budget==1 && end==residue) ||
                   (budget==2 && end==0)) continue;
                Limits limits;
                if(budget==0) limits.max_collection_elements=count-static_cast<std::size_t>(less);
                if(budget==1) limits.max_wire_bits=end-static_cast<std::size_t>(less);
                if(budget==2) limits.max_output_octets=(end+7)/8-static_cast<std::size_t>(less);
                // A prefix that already exceeds the output budget fails earlier;
                // determinant atomicity is checked only where the prefix is admitted.
                if(budget==2 && residue && limits.max_output_octets==0) continue;
                EncodeContext output(limits);
                BitWriter attempt(output);
                prefix(attempt,residue);
                const auto before_octets=output.logical_output_octets();
                auto wrote=attempt.write_bounded_collection_length(count,test.lower,test.upper);
                if(less) {
                    error(wrote,ErrorCode::resource_limit,residue);
                    REQUIRE(attempt.cursor_bit()==residue && output.wire_bits()==residue &&
                            output.collection_elements()==0 && output.logical_output_octets()==before_octets);
                } else REQUIRE(wrote);
                if(budget==2) continue;
                DecodeContext input_context(limits);
                auto owner=BitReader::make(wire,input_context);
                REQUIRE(owner);
                auto attempt_reader=std::move(owner).value();
                prefix(attempt_reader,residue);
                auto read=attempt_reader.read_bounded_collection_length(test.lower,test.upper);
                if(less) {
                    error(read,ErrorCode::resource_limit,residue);
                    REQUIRE(attempt_reader.cursor_bit()==residue && input_context.wire_bits()==residue &&
                            input_context.collection_elements()==0);
                } else REQUIRE(read && read.value()==count);
            }
            for(std::size_t cut=residue;cut<end;++cut) {
                Limits limits;
                limits.max_wire_bits=residue;
                limits.max_collection_elements=0;
                DecodeContext truncated(limits);
                auto owner=BitReader::make_bounded_for_test(wire,cut,truncated);
                REQUIRE(owner);
                auto attempt=std::move(owner).value();
                prefix(attempt,residue);
                error(attempt.read_bounded_collection_length(test.lower,test.upper),ErrorCode::truncated_input,cut);
                REQUIRE(attempt.cursor_bit()==residue && truncated.collection_elements()==0);
            }
        }
    }
    Bits narrow;narrow.length({0,127,7,false},127);
    REQUIRE(narrow.complete()==std::vector<std::byte>{std::byte{0xfe}});
    Bits wide;wide.length({0,256,16,true},256);
    REQUIRE(wide.complete()==(std::vector<std::byte>{std::byte{1},std::byte{0}}));
}
void invalid_and_priority() {
    for(unsigned residue=0;residue<8;++residue) {
        for(const auto& test : {Case{2,4,2,false},Case{0,254,8,false},Case{2,258,16,true}}) {
            Bits bits;bits.prefix(residue);
            if(test.aligned) bits.align();
            bits.number(test.upper-test.lower+1,test.width); // First spare offset.
            const auto end=bits.size;
            auto wire=bits.complete();
            for(unsigned mode=0;mode<3;++mode) {
                auto bad=bits;
                auto limits=Limits{};
                limits.max_collection_elements=0; // Spare beats the element budget.
                if(mode==1) limits.max_wire_bits=residue;
                if(mode==2 && test.aligned && residue) bad.set(residue);
                DecodeContext context(limits);
                auto made=BitReader::make_bounded_for_test(bad.bytes,end,context);
                REQUIRE(made);
                auto reader=std::move(made).value();
                prefix(reader,residue);
                const auto code=mode==1?ErrorCode::resource_limit:
                    mode==2 && test.aligned && residue?ErrorCode::nonzero_padding:ErrorCode::constraint_violation;
                error(reader.read_bounded_collection_length(test.lower,test.upper),code,residue);
                REQUIRE(reader.cursor_bit()==residue && context.wire_bits()==residue && context.collection_elements()==0);
                error(reader.read_bounded_collection_length(9,1),code,residue);
            }
        }
        for(auto bounds : {std::pair<std::size_t,std::size_t>{4,3},{0,65536},
                           {0,std::numeric_limits<std::size_t>::max()}}) {
            Bits bits;bits.prefix(residue);const auto wire=bits.complete();
            DecodeContext decoded;
            auto made=BitReader::make(wire,decoded);REQUIRE(made);
            auto reader=std::move(made).value();prefix(reader,residue);
            error(reader.read_bounded_collection_length(bounds.first,bounds.second),ErrorCode::invalid_argument,residue);
            REQUIRE(decoded.collection_elements()==0 && reader.cursor_bit()==residue);
            EncodeContext encoded;BitWriter writer(encoded);prefix(writer,residue);
            error(writer.write_bounded_collection_length(UINT64_MAX,bounds.first,bounds.second),ErrorCode::invalid_argument,residue);
            REQUIRE(encoded.collection_elements()==0 && writer.cursor_bit()==residue);
        }
        for(auto count : {UINT64_C(1),UINT64_C(5),UINT64_MAX}) {
            Limits limits;limits.max_collection_elements=0;limits.max_wire_bits=residue;
            EncodeContext context(limits);BitWriter writer(context);prefix(writer,residue);
            error(writer.write_bounded_collection_length(count,2,4),ErrorCode::constraint_violation,residue);
            REQUIRE(context.collection_elements()==0 && writer.cursor_bit()==residue);
        }
    }
}
void cumulative_fixed_and_allocation() {
    constexpr Limits original_six{1,2,3,4,5,6};
    static_assert(original_six.max_retained_unknown_records==6 && original_six.max_collection_elements==65536);
    for(bool less:{false,true}) {
        Limits limits;limits.max_collection_elements=6-static_cast<std::size_t>(less);
        limits.max_input_octets=0;limits.max_output_octets=0;limits.max_wire_bits=0;
        DecodeContext context(limits);auto made=BitReader::make({},context);REQUIRE(made);
        auto reader=std::move(made).value();
        EncodeContext output(limits);BitWriter writer(output);
        REQUIRE(reader.read_bounded_collection_length(0,0).value()==0);
        REQUIRE(writer.write_bounded_collection_length(0,0,0));
        REQUIRE(reader.read_bounded_collection_length(3,3).value()==3);
        REQUIRE(writer.write_bounded_collection_length(3,3,3));
        if(less) {
            error(reader.read_bounded_collection_length(3,3),ErrorCode::resource_limit,0);
            error(writer.write_bounded_collection_length(3,3,3),ErrorCode::resource_limit,0);
            REQUIRE(context.collection_elements()==3 && output.collection_elements()==3);
        } else {
            REQUIRE(reader.read_bounded_collection_length(3,3).value()==3);
            REQUIRE(writer.write_bounded_collection_length(3,3,3));
            REQUIRE(context.collection_elements()==6 && output.collection_elements()==6);
        }
        REQUIRE(reader.cursor_bit()==0 && context.wire_bits()==0 && writer.cursor_bit()==0 && output.wire_bits()==0);
    }
    for(unsigned residue : {0u,1u,7u}) {
        EncodeContext context;BitWriter writer(context);prefix(writer,residue);
        const auto octets=context.logical_output_octets();
        fail_next_allocation=true;
        error(writer.write_bounded_collection_length(65535,0,65535),ErrorCode::allocation_failure,residue);
        REQUIRE(!fail_next_allocation && writer.cursor_bit()==residue && context.wire_bits()==residue &&
                context.collection_elements()==0 && context.logical_output_octets()==octets);
        error(writer.write_bounded_collection_length(0,0,0),ErrorCode::allocation_failure,residue);
    }
    EncodeContext no_allocate;BitWriter fixed(no_allocate);
    fail_next_allocation=true;
    REQUIRE(fixed.write_bounded_collection_length(3,3,3));
    REQUIRE(fail_next_allocation && no_allocate.collection_elements()==3);
    fail_next_allocation=false;
    error(fixed.write_bounded_collection_length(UINT64_MAX,0,65535),ErrorCode::constraint_violation,0);
    REQUIRE(no_allocate.collection_elements()==3);
    // Previously charged counts remain committed when a later element fails.
    const std::array<std::byte,1> input{std::byte{0xc0}};
    DecodeContext context;auto made=BitReader::make(input,context);REQUIRE(made);
    auto reader=std::move(made).value();
    REQUIRE(reader.read_bounded_collection_length(3,3).value()==3);
    error(reader.read_enumerated(3,false),ErrorCode::constraint_violation,0);
    REQUIRE(context.collection_elements()==3);
}
void lifecycle_and_wrappers() {
    const std::array<std::byte,1> input{std::byte{0}};
    DecodeContext context;auto made=BitReader::make(input,context);REQUIRE(made);
    auto reader=std::move(made).value();auto transferred=std::move(reader);
    error(reader.read_bounded_collection_length(9,1),ErrorCode::invalid_state,0);
    REQUIRE(!context.failed());
    REQUIRE(transferred.read_bounded_collection_length(3,3).value()==3);
    REQUIRE(transferred.validate_complete_value());
    error(transferred.read_bounded_collection_length(9,1),ErrorCode::invalid_state,8);
    REQUIRE(!context.failed() && context.collection_elements()==3);
    EncodeContext encoded;BitWriter writer(encoded);BitWriter moved(std::move(writer));
    error(writer.write_bounded_collection_length(UINT64_MAX,9,1),ErrorCode::invalid_state,0);
    REQUIRE(!encoded.failed());
    REQUIRE(moved.write_bounded_collection_length(3,3,3));REQUIRE(moved.finish());
    error(moved.write_bounded_collection_length(UINT64_MAX,9,1),ErrorCode::invalid_state,8);
    REQUIRE(!encoded.failed() && encoded.collection_elements()==3);
    auto ignored=decode_complete<int>(input,Limits{},[](FieldReader& fields){
        (void)fields.read_bounded_collection_length(9,1);return Result<int>::success(1);
    });error(ignored,ErrorCode::invalid_argument,0);
    auto thrown=decode_complete<int>(input,Limits{},[](FieldReader& fields)->Result<int>{
        (void)fields.read_bounded_collection_length(9,1);throw std::bad_alloc();
    });error(thrown,ErrorCode::invalid_argument,0);
    auto encoded_ignored=encode_complete(0,Limits{},[](FieldWriter& fields){
        (void)fields.write_bounded_collection_length(UINT64_MAX,0,65535);return Result<void>::success();
    });error(encoded_ignored,ErrorCode::constraint_violation,0);
}
}
void* operator new(std::size_t size) {
    if(fail_next_allocation) { fail_next_allocation=false;throw std::bad_alloc(); }
    if(void* memory=std::malloc(size?size:1)) return memory;
    throw std::bad_alloc();
}
void* operator new[](std::size_t size) { return ::operator new(size); }
void operator delete(void* pointer) noexcept { std::free(pointer); }
void operator delete[](void* pointer) noexcept { std::free(pointer); }
void operator delete(void* pointer,std::size_t) noexcept { std::free(pointer); }
void operator delete[](void* pointer,std::size_t) noexcept { std::free(pointer); }
int main() {
    vectors_and_budgets();invalid_and_priority();cumulative_fixed_and_allocation();lifecycle_and_wrappers();
}
