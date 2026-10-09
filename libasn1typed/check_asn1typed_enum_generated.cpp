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
#include <limits>
#include <type_traits>
#include <vector>
#define REQUIRE(x) do { if(!(x)) { std::fprintf(stderr,"%s:%d: %s\n",__FILE__,__LINE__,#x); std::abort(); } } while(0)
using namespace nrforge::aper;
#define ADAPTER(NAME,NS,T,BASE) struct NAME { using Type=NS::T; using Meta=NS::T##_aper; static auto encode(const Type& v,const Limits& l={}) {return NS::encode_##BASE(v,l);} static auto decode(::std::span<const ::std::byte> b,const Limits& l={}) {return NS::decode_##BASE(b,l);} static auto put(FieldWriter& f,const Type& v) {return NS::enum_codec::put_##T(f,v);} static auto get(FieldReader& f) {return NS::enum_codec::get_##T(f);} };
ADAPTER(Entry,enumtest,Entry,entry)
ADAPTER(Reordered,enumtest,Reordered,reordered)
ADAPTER(Implicit,enumtest,Implicit,implicit)
ADAPTER(Endpoints,enumtest,Endpoints,endpoints)
ADAPTER(Singleton,enumtest,Singleton,singleton)
ADAPTER(BelowRoot,enumtest,BelowRoot,below_root)
ADAPTER(NoAdditions,enumtest,NoAdditions,no_additions)
ADAPTER(Altitude,renamedtest,Altitude,altitude)
ADAPTER(Status,renamedtest,Status,status)
#undef ADAPTER
static_assert(std::is_same_v<std::remove_cv_t<decltype(enumtest::Entry_aper::entries[0].value)>,enumtest::Entry::Known>);
static_assert(!std::is_same_v<enumtest::Reordered::Known,renamedtest::Altitude::Known>);
static_assert(!std::is_same_v<enumtest::BelowRoot::UnknownExtension,renamedtest::Status::UnknownExtension>);
static_assert(static_cast<std::int64_t>(enumtest::Reordered::Known::high)==9);
static_assert(static_cast<std::int64_t>(enumtest::Reordered::Known::low)==-2);
static_assert(static_cast<std::int64_t>(enumtest::Endpoints::Known::low)==INT64_MIN);
static_assert(static_cast<std::int64_t>(enumtest::Endpoints::Known::high)==INT64_MAX);
template<class M> constexpr bool tables() {
 for(std::size_t i=0;i<M::entries.size();++i) {const auto& e=M::entries[i]; if(M::source_ordinal_to_per_index[i]!=e.per_index) return false;
  if(e.is_extension) {if(M::addition_index_to_source_ordinal[e.per_index]!=i)return false;}
  else if(M::root_index_to_source_ordinal[e.per_index]!=i)return false;
  if(static_cast<std::int64_t>(e.value)!=e.assigned_number)return false; }
 return true;
}
static_assert(tables<Entry::Meta>());
static_assert(tables<Reordered::Meta>()&&tables<Implicit::Meta>()&&tables<Endpoints::Meta>()&&tables<Singleton::Meta>()&&tables<BelowRoot::Meta>()&&tables<NoAdditions::Meta>()&&tables<Altitude::Meta>()&&tables<Status::Meta>());
static_assert(Reordered::Meta::entries[0].per_index==2&&Reordered::Meta::entries[1].per_index==0&&Reordered::Meta::entries[2].per_index==1);
static_assert(BelowRoot::Meta::entries[2].assigned_number==1&&BelowRoot::Meta::entries[2].is_extension);
static std::vector<std::byte> model(EnumeratedIndex v,unsigned roots,bool extensible,unsigned residue,std::size_t& end) {
 std::vector<bool> bits(residue,true);auto put=[&](std::uint64_t n,unsigned width){for(unsigned i=width;i>0;--i)bits.push_back(((n>>(i-1))&1u)!=0);};
 if(extensible)bits.push_back(v.is_extension);
 if(!v.is_extension) {unsigned width=0;for(auto n=roots-1;n;n/=2)++width;put(v.index,width);}
 else if(v.index<64){bits.push_back(false);put(v.index,6);}
 else {bits.push_back(true);while(bits.size()%8)bits.push_back(false);unsigned octets=1;for(auto n=v.index;n>255;n>>=8)++octets;put(octets,8);put(v.index,octets*8);}
 end=bits.size();if(bits.empty())bits.resize(8,false);while(bits.size()%8)bits.push_back(false);
 std::vector<std::byte> bytes(bits.size()/8,std::byte{0});for(std::size_t i=0;i<bits.size();++i)if(bits[i])bytes[i/8]|=static_cast<std::byte>(0x80u>>(i%8));return bytes;
}
template<class R> void error(const R& r,ErrorCode code,std::size_t offset){REQUIRE(!r&&r.error().code==code&&r.error().bit_offset==offset);}
template<class A> void equal(const typename A::Type& a,const typename A::Type& b){
 if constexpr(A::Meta::extensible){REQUIRE(a.value.index()==b.value.index());if(auto p=std::get_if<typename A::Type::Known>(&a.value)) REQUIRE(*p==std::get<typename A::Type::Known>(b.value));else REQUIRE(std::get<typename A::Type::UnknownExtension>(a.value).index==std::get<typename A::Type::UnknownExtension>(b.value).index);}
 else REQUIRE(a.value==b.value);
}
template<class A> void check(const typename A::Type& value,EnumeratedIndex wire){
 for(unsigned residue=0;residue<8;++residue){std::size_t end;auto expected=model(wire,static_cast<unsigned>(A::Meta::root_count),A::Meta::extensible,residue,end);
  auto encoded=encode_complete(value,Limits{},[&](FieldWriter& f){for(unsigned i=0;i<residue;++i){auto r=f.write_bit(true);if(!r)return r;}return A::put(f,value);});
  REQUIRE(encoded&&encoded.value().octets==expected);const auto& e=encoded.value();REQUIRE(e.last_field_end_bit==end);REQUIRE(e.complete_encoding_bits==expected.size()*8);REQUIRE(e.octet_count==expected.size());REQUIRE(e.final_padding_bits==(8-end%8)%8);REQUIRE(e.empty_encoding_substitution==(end==0));
  auto decoded=decode_complete<typename A::Type>(expected,Limits{},[&](FieldReader& f)->Result<typename A::Type>{for(unsigned i=0;i<residue;++i){auto r=f.read_bit();if(!r)return Result<typename A::Type>::failure(r.error());REQUIRE(r.value());}return A::get(f);});REQUIRE(decoded);equal<A>(value,decoded.value());
  if(residue==0){REQUIRE(A::encode(value).value().octets==expected);equal<A>(value,A::decode(expected).value());
   for(std::size_t n=0;n<expected.size();++n)error(A::decode(std::span<const std::byte>(expected.data(),n)),ErrorCode::truncated_input,n*8);
   auto trailing=expected;trailing.push_back(std::byte{0});error(A::decode(trailing),ErrorCode::trailing_data,expected.size()*8);
   if(end%8){auto bad=expected;bad.back()|=std::byte{1};error(A::decode(bad),ErrorCode::nonzero_padding,expected.size()*8-1);}
   for(bool short_budget:{false,true}){Limits l;l.max_wire_bits=expected.size()*8-(short_budget?1u:0u);auto enc=A::encode(value,l);auto dec=A::decode(expected,l);if(short_budget){REQUIRE(!enc&&!dec&&enc.error().code==ErrorCode::resource_limit&&dec.error().code==ErrorCode::resource_limit);}else REQUIRE(enc&&dec);
    l=Limits{};l.max_output_octets=expected.size()-(short_budget?1u:0u);auto out=A::encode(value,l);if(short_budget)REQUIRE(!out&&out.error().code==ErrorCode::resource_limit);else REQUIRE(out);
    l=Limits{};l.max_input_octets=expected.size()-(short_budget?1u:0u);auto admitted=A::decode(expected,l);if(short_budget)error(admitted,ErrorCode::resource_limit,0);else REQUIRE(admitted);
   }
  }
 }
}
template<class A> void run(){
 typename A::Type def{};const auto first=A::Meta::entries[A::Meta::root_index_to_source_ordinal[0]].value;
 if constexpr(A::Meta::extensible)REQUIRE(std::get<typename A::Type::Known>(def.value)==first);else REQUIRE(def.value==first);
 for(const auto& entry:A::Meta::entries){typename A::Type v{};v.value=entry.value;check<A>(v,{entry.is_extension,entry.per_index});}
 if constexpr(A::Meta::extensible){for(std::uint64_t index:std::array<std::uint64_t,7>{0,1,63,64,255,256,UINT64_MAX})if(index>=A::Meta::known_addition_count){typename A::Type v{};v.value=typename A::Type::UnknownExtension{index};check<A>(v,{true,index});}
  if(A::Meta::known_addition_count){typename A::Type v{};v.value=typename A::Type::UnknownExtension{0};error(A::encode(v),ErrorCode::constraint_violation,0);}
 }
 typename A::Type invalid{};invalid.value=static_cast<typename A::Type::Known>(42);error(A::encode(invalid),ErrorCode::constraint_violation,0);
 error(encode_complete(invalid,Limits{},[&](FieldWriter& f){(void)f.write_enumerated({false,3},3,false);return A::put(f,invalid);}),ErrorCode::constraint_violation,0);
 const std::array<std::byte,1> bad{std::byte{0xc0}};
 error(decode_complete<typename A::Type>(bad,Limits{},[&](FieldReader& f){(void)f.read_enumerated(3,false);return A::get(f);}),ErrorCode::constraint_violation,0);
}
int main(){run<Entry>();run<Reordered>();run<Implicit>();run<Endpoints>();run<Singleton>();run<BelowRoot>();run<NoAdditions>();run<Altitude>();run<Status>();
 for(const auto& bytes:std::vector<std::vector<std::byte>>{{std::byte{0xc0},std::byte{1},std::byte{63}},{std::byte{0xc0},std::byte{2},std::byte{0},std::byte{64}}})error(NoAdditions::decode(bytes),ErrorCode::constraint_violation,0);
 error(Reordered::decode(std::array<std::byte,1>{std::byte{0xc0}}),ErrorCode::constraint_violation,0);
 error(NoAdditions::decode(std::array<std::byte,2>{std::byte{0xc0},std::byte{0}}),ErrorCode::constraint_violation,0);
 error(NoAdditions::decode(std::array<std::byte,2>{std::byte{0xc0},std::byte{9}}),ErrorCode::resource_limit,0);
 error(NoAdditions::decode(std::array<std::byte,3>{std::byte{0xc1},std::byte{1},std::byte{64}}),ErrorCode::nonzero_padding,7);
}
