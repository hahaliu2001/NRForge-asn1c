#include <runtime.hpp>
#include "main/types.hpp"
#include "main/mapping.hpp"
#include "main/codec.hpp"
#include "renamed/types.hpp"
#include "renamed/mapping.hpp"
#include "renamed/codec.hpp"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <type_traits>
#include <vector>
#define REQUIRE(x) do {if(!(x)){std::fprintf(stderr,"%s:%d: %s\n",__FILE__,__LINE__,#x);std::abort();}}while(0)
using namespace nrforge::aper;
#define ADAPTER(NAME,NS,T,BASE,W) struct NAME {using Type=NS::T;using Meta=NS::T##_aper;static constexpr unsigned width=W;static auto encode(Type v,const Limits& l={}){return NS::encode_##BASE(v,l);}static auto decode(std::span<const std::byte> b,const Limits& l={}){return NS::decode_##BASE(b,l);}static auto put(FieldWriter& f,Type v){return NS::uint_codec::put_##T(f,v);}static auto get(FieldReader& f){return NS::uint_codec::get_##T(f);} }; static_assert(std::is_same_v<NS::T,std::uint64_t>);static_assert(NS::T##_constraint::lower_bound==0&&NS::T##_constraint::upper_bound==((UINT64_C(1)<<W)-1));static_assert(NS::T##_aper::root_bits==W);static_assert(std::is_same_v<typename NS::T##_aper::value_type,NS::T>);
ADAPTER(Small,uinttest,Small,small,8)
ADAPTER(Medium,uinttest,Medium,medium,16)
ADAPTER(Large,uinttest,Large,large,32)
ADAPTER(Huge,uinttest,Huge,huge,40)
ADAPTER(Octet,renameduint,Octet,octet,8)
ADAPTER(Pair,renameduint,Pair,pair,16)
ADAPTER(Word,renameduint,Word,word,32)
ADAPTER(Identity,renameduint,Identity,identity,40)
#undef ADAPTER
static std::vector<std::byte> model(unsigned width,std::uint64_t value,unsigned residue){std::vector<bool> bits(residue,true);auto put=[&](std::uint64_t v,unsigned n){for(unsigned i=n;i>0;--i)bits.push_back(((v>>(i-1))&1u)!=0);};unsigned octets=width<=16?width/8:1; if(width>16){for(auto n=value;n>255;n>>=8)++octets;put(octets-1,width==32?2u:3u);}while(bits.size()%8)bits.push_back(false);put(value,octets*8);std::vector<std::byte> wire(bits.size()/8,std::byte{0});for(std::size_t i=0;i<bits.size();++i)if(bits[i])wire[i/8]|=static_cast<std::byte>(0x80u>>(i%8));return wire;}
template<class R>void error(const R& r,ErrorCode c,std::size_t offset){REQUIRE(!r&&r.error().code==c&&r.error().bit_offset==offset);}
template<class A>void run(){static_assert(A::Meta::lower_bound==0&&!A::Meta::extensible&&A::Meta::align_before_payload_to_octet&&A::Meta::most_significant_octet_first);static_assert(A::Meta::length_prefix_bits==(A::width==32?2u:A::width==40?3u:0u));static_assert(A::Meta::minimum_payload_octets==(A::width==16?2u:1u));static_assert(A::Meta::maximum_payload_octets==A::width/8);static_assert(A::Meta::minimal_payload==(A::width>16));typename A::Type zero{};REQUIRE(zero==0);
 const auto maximum=(UINT64_C(1)<<A::width)-1;std::vector<std::uint64_t> values{0,1,254,255,maximum};for(unsigned n=1;n<5;++n){auto v=UINT64_C(1)<<(n*8);if(v<=maximum){values.push_back(v-1);values.push_back(v);}}
 for(auto value:values)for(unsigned residue=0;residue<8;++residue){auto wire=model(A::width,value,residue);const auto end=wire.size()*8;
  auto encoded=encode_complete(value,Limits{},[&](FieldWriter& f){for(unsigned i=0;i<residue;++i){auto r=f.write_bit(true);if(!r)return r;}return A::put(f,value);});REQUIRE(encoded&&encoded.value().octets==wire);const auto& e=encoded.value();REQUIRE(e.last_field_end_bit==end&&e.final_padding_bits==0&&!e.empty_encoding_substitution&&e.complete_encoding_bits==end&&e.octet_count==wire.size());
  auto decoded=decode_complete<typename A::Type>(wire,Limits{},[&](FieldReader& f)->Result<typename A::Type>{for(unsigned i=0;i<residue;++i){auto r=f.read_bit();if(!r)return Result<typename A::Type>::failure(r.error());REQUIRE(r.value());}return A::get(f);});REQUIRE(decoded&&decoded.value()==value);
  for(std::size_t limit=residue;limit<end;++limit){DecodeContext c;auto made=BitReader::make_bounded_for_test(wire,limit,c);REQUIRE(made);auto r=std::move(made).value();for(unsigned i=0;i<residue;++i)REQUIRE(r.read_bit());FieldReader f(r);error(A::get(f),ErrorCode::truncated_input,limit);REQUIRE(r.cursor_bit()==residue&&c.wire_bits()==residue);}
  unsigned prefix=A::width==32?2u:A::width==40?3u:0u;std::size_t p=residue+prefix,aligned=(p+7)/8*8;for(auto bit=p;bit<aligned;++bit){auto bad=wire;bad[bit/8]|=static_cast<std::byte>(0x80u>>(bit%8));DecodeContext c;auto made=BitReader::make(bad,c);REQUIRE(made);auto r=std::move(made).value();for(unsigned i=0;i<residue;++i)REQUIRE(r.read_bit());FieldReader f(r);error(A::get(f),ErrorCode::nonzero_padding,bit);REQUIRE(r.cursor_bit()==residue&&c.wire_bits()==residue);}
  for(bool short_budget:{false,true}) {Limits l;l.max_wire_bits=end-(short_budget?1u:0u);EncodeContext ec(l);BitWriter w(ec);for(unsigned i=0;i<residue;++i)REQUIRE(w.write_bit(true));FieldWriter wf(w);auto out=A::put(wf,value);DecodeContext dc(l);auto made=BitReader::make(wire,dc);REQUIRE(made);auto r=std::move(made).value();for(unsigned i=0;i<residue;++i)REQUIRE(r.read_bit());FieldReader rf(r);auto in=A::get(rf);
   if(short_budget){error(out,ErrorCode::resource_limit,residue);error(in,ErrorCode::resource_limit,residue);REQUIRE(w.cursor_bit()==residue&&r.cursor_bit()==residue&&ec.wire_bits()==residue&&dc.wire_bits()==residue);}else REQUIRE(out&&in&&in.value()==value);
  }
  if(residue==0){REQUIRE(A::encode(value).value().octets==wire);REQUIRE(A::decode(wire).value()==value);for(std::size_t n=0;n<wire.size();++n)error(A::decode(std::span<const std::byte>(wire.data(),n)),ErrorCode::truncated_input,n*8);auto trailing=wire;trailing.push_back(std::byte{0});error(A::decode(trailing),ErrorCode::trailing_data,end);
   for(bool short_budget:{false,true}){Limits l;l.max_wire_bits=end-(short_budget?1u:0u);auto enc=A::encode(value,l);auto dec=A::decode(wire,l);if(short_budget){error(enc,ErrorCode::resource_limit,0);error(dec,ErrorCode::resource_limit,0);}else REQUIRE(enc&&dec);
    l=Limits{};l.max_output_octets=wire.size()-(short_budget?1u:0u);auto out=A::encode(value,l);if(short_budget)error(out,ErrorCode::resource_limit,0);else REQUIRE(out);
    l=Limits{};l.max_input_octets=wire.size()-(short_budget?1u:0u);auto in=A::decode(wire,l);if(short_budget)error(in,ErrorCode::resource_limit,0);else REQUIRE(in);
   }
  }
 }
 for(auto bad:{maximum+1,UINT64_MAX}){error(A::encode(bad),ErrorCode::constraint_violation,0);error(encode_complete(bad,Limits{},[&](FieldWriter& f){(void)A::put(f,bad);return A::put(f,0);}),ErrorCode::constraint_violation,0);}
 const std::array<std::byte,1> spare{std::byte{0xc0}};error(decode_complete<typename A::Type>(spare,Limits{},[&](FieldReader& f){(void)f.read_enumerated(3,false);return A::get(f);}),ErrorCode::constraint_violation,0);
}
int main(){run<Small>();run<Medium>();run<Large>();run<Huge>();run<Octet>();run<Pair>();run<Word>();run<Identity>();
 for(auto wire:std::vector<std::vector<std::byte>>{{std::byte{0x40},std::byte{0},std::byte{1}},{std::byte{0x20},std::byte{0},std::byte{1}}}){if(wire[0]==std::byte{0x40})error(Large::decode(wire),ErrorCode::constraint_violation,0);else error(Huge::decode(wire),ErrorCode::constraint_violation,0);}
 for(unsigned prefix:{0xa0u,0xc0u,0xe0u})error(Huge::decode(std::array<std::byte,1>{static_cast<std::byte>(prefix)}),ErrorCode::constraint_violation,0);
}
