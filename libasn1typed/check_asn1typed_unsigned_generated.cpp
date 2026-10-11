#include <runtime.hpp>
#include <sequence_extensions.hpp>
#include "types.hpp"
#include "mapping.hpp"
#include "codec.hpp"
#include <bit>
#include <cstdio>
#include <cstdlib>
#include <type_traits>
#define REQUIRE(x) do { if(!(x)) { std::fprintf(stderr,"%d: %s\n",__LINE__,#x); std::abort(); } } while(0)
namespace c = unsignedtests;
namespace r = nrforge::aper;
static_assert(std::is_same_v<c::Full,std::uint64_t>);
static_assert(c::Full_aper::root_bits == 64 && c::Full_aper::cardinality_is_full);
static_assert(c::High_aper::lower_bound == (UINT64_C(1)<<63));
static_assert(c::Constant_aper::root_bits == 0);
// Independent APER bit model retained from the accepted integer qualification.
struct Model {
    std::vector<std::byte> octets;
    std::size_t bits = 0;
    void bit(bool value) {
        if(bits % 8 == 0) octets.push_back(std::byte{0});
        if(value) octets.back() |= std::byte(0x80u >> (bits % 8));
        ++bits;
    }
    void number(std::uint64_t value, unsigned width) {
        for(unsigned i = width; i != 0; --i) bit(((value >> (i - 1)) & 1u) != 0);
    }
    void align() { while(bits % 8) bit(false); }
    static unsigned width(std::uint64_t value) { unsigned n = 0; while(value) { ++n; value >>= 1; } return n; }
    void integer_offset(std::uint64_t offset, std::uint64_t last) {
        if(last == 0) return;
        if(last < 255) number(offset, width(last));
        else if(last == 255) { align(); number(offset, 8); }
        else if(last <= 65535) { align(); number(offset, 16); }
        else {
            const unsigned maximum = (width(last) + 7) / 8;
            unsigned actual = (width(offset) + 7) / 8; if(actual == 0) actual = 1;
            number(actual - 1, width(maximum - 1)); align(); number(offset, actual * 8);
        }
    }
    void uint(std::uint64_t value, std::uint64_t lo, std::uint64_t hi) { integer_offset(value - lo, hi - lo); }
    static std::uint64_t rank(std::int64_t value) { return std::bit_cast<std::uint64_t>(value) ^ (UINT64_C(1) << 63); }
    void sint(std::int64_t value, std::int64_t lo, std::int64_t hi) { integer_offset(rank(value) - rank(lo), rank(hi) - rank(lo)); }
    std::vector<std::byte> complete() { if(bits == 0) bit(false); align(); return octets; }
};

int main() {
    constexpr auto high = UINT64_C(1)<<63;
    constexpr std::uint64_t values[] = {0,1,255,256,65535,65536,INT64_MAX,high,UINT64_MAX};
    for(auto v:values) {
        Model expected; expected.uint(v,0,UINT64_MAX);
        const auto bits = expected.bits; const auto bytes = expected.complete();
        auto encoded = c::encode_full(v); REQUIRE(encoded && encoded.value().octets == bytes);
        REQUIRE(encoded.value().last_field_end_bit == bits);
        auto decoded = c::decode_full(bytes); REQUIRE(decoded && decoded.value() == v);
        for(std::size_t n=0;n<bytes.size();++n) REQUIRE(!c::decode_full(std::span<const std::byte>(bytes.data(),n)));
        auto trailing=bytes;trailing.push_back(std::byte{0});REQUIRE(!c::decode_full(trailing));
        r::Limits limits; limits.max_output_octets=bytes.size()-1; REQUIRE(!c::encode_full(v,limits));
        limits={}; limits.max_input_octets=bytes.size()-1; REQUIRE(!c::decode_full(bytes,limits));
        limits={}; limits.max_wire_bits=bits-1; REQUIRE(!c::encode_full(v,limits)); REQUIRE(!c::decode_full(bytes,limits));
        for(unsigned residue=0;residue<8;++residue) {
            Model model; for(unsigned n=0;n<residue;++n) model.bit(true);
            model.uint(v,0,UINT64_MAX); model.bit(true); auto wanted=model.complete();
            r::EncodeContext ec; r::BitWriter writer(ec); r::FieldWriter field(writer);
            for(unsigned n=0;n<residue;++n) REQUIRE(field.write_bit(true));
            REQUIRE(c::integer_codec::put_Full(field,v)); REQUIRE(field.write_bit(true));
            auto wire=writer.finish(); REQUIRE(wire && wire.value().octets==wanted);
            r::DecodeContext dc; auto made=r::BitReader::make(wanted,dc); REQUIRE(made);
            auto reader=std::move(made).value(); r::FieldReader rf(reader);
            for(unsigned n=0;n<residue;++n) REQUIRE(rf.read_bit().value());
            auto got=c::integer_codec::get_Full(rf);REQUIRE(got && got.value()==v);
            REQUIRE(rf.read_bit().value() && reader.validate_complete_value());
        }
    }
    for(auto v:{high,high+1,UINT64_MAX}) {
        Model m;m.uint(v,high,UINT64_MAX);auto bytes=m.complete();
        REQUIRE(c::encode_high(v).value().octets==bytes && c::decode_high(bytes).value()==v);
    }
    REQUIRE(!c::encode_high(high-1));
    auto constant=c::encode_constant(UINT64_MAX);REQUIRE(constant && constant.value().empty_encoding_substitution);
    REQUIRE(c::decode_constant(constant.value().octets).value()==UINT64_MAX && !c::encode_constant(UINT64_MAX-1));
    for(bool present:{false,true}) for(bool named:{false,true}) {
        c::Packet packet;packet.lead=true;packet.inline_=UINT64_MAX;packet.narrow=high+1;packet.small=7;
        if(present) packet.optional=UINT64_MAX;
        if(named) packet.pick=c::Pick_named{UINT64_MAX};else packet.pick=c::Pick_inline_{high};
        packet.tail=true;
        auto encoded=c::encode_packet(packet);REQUIRE(encoded);
        auto decoded=c::decode_packet(encoded.value().octets);REQUIRE(decoded);
        const auto& q=decoded.value();REQUIRE(q.lead && q.tail && q.inline_==packet.inline_ && q.narrow==packet.narrow && q.small==7 && q.optional==packet.optional);
        REQUIRE(q.pick.index()==packet.pick.index());
        packet.small=8;REQUIRE(!c::encode_packet(packet));
        packet.small=7;packet.narrow=high-1;REQUIRE(!c::encode_packet(packet));
    }
    REQUIRE(!c::decode_full(std::vector<std::byte>{std::byte{0x20},std::byte{0},std::byte{1}}));
    REQUIRE(!c::decode_full(std::vector<std::byte>{std::byte{1},std::byte{0}}));
    REQUIRE(!c::decode_full(std::vector<std::byte>{std::byte{0xe0},std::byte{0xff}}));
    for(auto v:{INT64_MIN,INT64_C(0),INT64_MAX}) {
        auto wire=c::encode_signed_(v);REQUIRE(wire && c::decode_signed_(wire.value().octets).value()==v);
    }
    std::puts("PASS unsigned64 owned generation, independent finite bytes, residues, compounds, malformed input and budgets");
}
