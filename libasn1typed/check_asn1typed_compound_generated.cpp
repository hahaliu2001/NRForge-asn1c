#include <runtime.hpp>
#include "main/types.hpp"
#include "main/mapping.hpp"
#include "main/codec.hpp"
#include "reversed/types.hpp"
#include "reversed/mapping.hpp"
#include "reversed/codec.hpp"
#include <cstdio>
#include <cstdlib>
#include <type_traits>
#include <vector>
#define REQUIRE(x) do{if(!(x)){std::fprintf(stderr,"%s:%d: %s\n",__FILE__,__LINE__,#x);std::abort();}}while(0)
using namespace nrforge::aper;
namespace c=compoundtest;namespace r=reversedcompound;
static_assert(std::variant_size_v<c::Tri> ==3);
static_assert(std::variant_size_v<c::Six> ==6);
static_assert(std::is_same_v<c::Tri_aper::wrapper_0,c::Tri_flag>);
static_assert(std::is_same_v<c::Tri_aper::payload_type_2,c::Word>);
static_assert(std::is_same_v<c::Tri_aper::payload_mapping_2,c::Word_aper>);
static_assert(c::Tri_aper::selector_bits==2&&c::Six_aper::selector_bits==3);
static_assert(r::Pick_aper::storage_ordinal_to_per_index[0]==2&&r::Pick_aper::storage_ordinal_to_per_index[2]==0);
static_assert(r::Pick_aper::per_index_to_storage_ordinal[0]==2);
template<class M>constexpr bool inverse(){for(std::size_t i=0;i<M::root_count;++i)if(M::per_index_to_storage_ordinal[M::storage_ordinal_to_per_index[i]]!=i)return false;return true;}
static_assert(inverse<c::Tri_aper>()&&inverse<c::Six_aper>()&&inverse<r::Pick_aper>()&&inverse<c::Solo_aper>());
static_assert(c::Octet_aper::root_bits==8&&c::Pair_aper::root_bits==16&&c::Word_aper::root_bits==32&&c::Identity_aper::root_bits==40);
static_assert(c::Inner_aper::optional_bitmap_bit_count==2);
static_assert(c::Inner_aper::field_0_declaration_ordinal==0&&c::Inner_aper::field_0_mandatory);
static_assert(c::Inner_aper::field_0_optional_bitmap_ordinal==static_cast<std::size_t>(-1));
static_assert(c::Inner_aper::field_1_optional_bitmap_ordinal==0&&c::Inner_aper::field_2_optional_bitmap_ordinal==1);
static_assert(std::is_same_v<c::Inner_aper::field_1_type,c::State>);
static_assert(std::is_same_v<c::Inner_aper::field_1_payload_mapping,c::State_aper>);
template<class T>bool equivalent(const T&a,const T&b){
 if constexpr(std::is_arithmetic_v<T>||std::is_enum_v<T>)return a==b;
 else if constexpr(requires{a.index();}){if(a.index()!=b.index())return false;return std::visit([](const auto& x,const auto& y){if constexpr(std::is_same_v<std::decay_t<decltype(x)>,std::decay_t<decltype(y)>>)return equivalent(x,y);else return false;},a,b);}
 else if constexpr(requires{a.has_value();})return a.has_value()==b.has_value()&&(!a||equivalent(*a,*b));
 else if constexpr(requires{a.value;})return equivalent(a.value,b.value);
 else if constexpr(requires{a.index;})return a.index==b.index;
 else if constexpr(requires{a.identity;})return a.identity==b.identity&&equivalent(a.state,b.state)&&equivalent(a.flag,b.flag);
 else if constexpr(requires{a.selection;})return equivalent(a.selection,b.selection)&&equivalent(a.inner,b.inner)&&equivalent(a.flag,b.flag);
 else if constexpr(requires{a.pick;})return equivalent(a.pick,b.pick)&&equivalent(a.mode,b.mode)&&equivalent(a.signal,b.signal);
 else return true; // empty SEQUENCE
}
struct Bits{std::vector<bool> bits;std::vector<std::size_t> padding;void put(std::uint64_t v,unsigned n){for(unsigned i=n;i>0;--i)bits.push_back(((v>>(i-1))&1u)!=0);}void align(){while(bits.size()%8){padding.push_back(bits.size());bits.push_back(false);}}void integer(std::uint64_t v,unsigned w){unsigned n=w<=16?w/8:1;if(w>16){for(auto rest=v;rest>255;rest>>=8)++n;put(n-1,w==32?2u:3u);}align();put(v,n*8);}void enumeration(bool ext,std::uint64_t index){put(ext?1u:0u,1);if(!ext){put(index,1);return;}if(index<64){put(0,1);put(index,6);}else{put(1,1);align();unsigned n=1;for(auto rest=index;rest>255;rest>>=8)++n;put(n,8);put(index,n*8);}}std::vector<std::byte> finish(){if(bits.empty())bits.resize(8,false);align();std::vector<std::byte> out(bits.size()/8,std::byte{0});for(std::size_t i=0;i<bits.size();++i)if(bits[i])out[i/8]|=static_cast<std::byte>(0x80u>>(i%8));return out;}};
static void model(Bits&b,const c::State&v){if(auto known=std::get_if<c::State::Known>(&v.value)){if(*known==c::State::Known::low)b.enumeration(false,0);else if(*known==c::State::Known::high)b.enumeration(false,1);else b.enumeration(true,0);}else b.enumeration(true,std::get<c::State::UnknownExtension>(v.value).index);}
static void model(Bits&b,const r::Mode&v){if(auto known=std::get_if<r::Mode::Known>(&v.value)){if(*known==r::Mode::Known::low)b.enumeration(false,0);else if(*known==r::Mode::Known::high)b.enumeration(false,1);else b.enumeration(true,0);}else b.enumeration(true,std::get<r::Mode::UnknownExtension>(v.value).index);}
static void model(Bits&b,const c::Tri&v){b.put(v.index(),2);if(auto p=std::get_if<c::Tri_flag>(&v))b.put(p->value,1);else if(auto p=std::get_if<c::Tri_state>(&v))model(b,p->value);else b.integer(std::get<c::Tri_word>(v).value,32);}
static void model(Bits&b,const c::Inner&v){b.put(v.state.has_value(),1);b.put(v.flag.has_value(),1);b.integer(v.identity,40);if(v.state)model(b,*v.state);if(v.flag)b.put(*v.flag,1);}
static void model(Bits&b,const c::Six&v){b.put(v.index(),3);if(auto p=std::get_if<c::Six_flag>(&v))b.put(p->value,1);else if(auto p=std::get_if<c::Six_state>(&v))model(b,p->value);else if(auto p=std::get_if<c::Six_word>(&v))b.integer(p->value,32);else if(auto p=std::get_if<c::Six_identity>(&v))b.integer(p->value,40);else if(auto p=std::get_if<c::Six_inner>(&v))model(b,p->value);else model(b,std::get<c::Six_tri>(v).value);}
static void model(Bits&b,const c::Outer&v){b.put(v.inner.has_value(),1);b.put(v.flag.has_value(),1);model(b,v.selection);if(v.inner)model(b,*v.inner);if(v.flag)b.put(*v.flag,1);}
static void model(Bits&b,const r::Pick&v){unsigned index=v.index()==0?2u:v.index()==1?1u:0u;b.put(index,2);if(auto p=std::get_if<r::Pick_counter>(&v))b.integer(p->value,32);else if(auto p=std::get_if<r::Pick_mode>(&v))model(b,p->value);else b.put(std::get<r::Pick_signal>(v).value,1);}
static void model(Bits&b,const r::Envelope&v){b.put(v.mode.has_value(),1);b.put(v.signal.has_value(),1);model(b,v.pick);if(v.mode)model(b,*v.mode);if(v.signal)b.put(*v.signal,1);}
static void model(Bits&b,const c::Solo&v){b.put(std::get<c::Solo_only>(v).value,1);}static void model(Bits&,const c::Empty&){}
#define ADAPTER(A,NS,T,BASE)struct A{using Type=NS::T;static auto put(FieldWriter&f,const Type&v){return NS::compound_codec::put_##T(f,v);}static auto get(FieldReader&f){return NS::compound_codec::get_##T(f);}static auto encode(const Type&v,const Limits&l={}){return NS::encode_##BASE(v,l);}static auto decode(std::span<const std::byte>w,const Limits&l={}){return NS::decode_##BASE(w,l);}};
ADAPTER(Tri,c,Tri,tri)ADAPTER(Inner,c,Inner,inner)ADAPTER(Six,c,Six,six)ADAPTER(Outer,c,Outer,outer)ADAPTER(Pick,r,Pick,pick)ADAPTER(Envelope,r,Envelope,envelope)ADAPTER(Solo,c,Solo,solo)ADAPTER(Empty,c,Empty,empty)
#undef ADAPTER
template<class R>void error(const R&v,ErrorCode code,std::size_t bit){REQUIRE(!v&&v.error().code==code&&v.error().bit_offset==bit);}
template<class A>void check(const typename A::Type&v){for(unsigned residue=0;residue<8;++residue){Bits b;for(unsigned i=0;i<residue;++i)b.put(1,1);model(b,v);const auto fieldend=b.bits.size();auto wire=b.finish();auto encoded=encode_complete(v,Limits{},[&](FieldWriter&f){for(unsigned i=0;i<residue;++i){auto r=f.write_bit(true);if(!r)return r;}return A::put(f,v);});REQUIRE(encoded&&encoded.value().octets==wire);const auto&e=encoded.value();REQUIRE(e.last_field_end_bit==fieldend&&e.final_padding_bits==(8-fieldend%8)%8&&e.empty_encoding_substitution==(fieldend==0)&&e.complete_encoding_bits==wire.size()*8&&e.octet_count==wire.size());auto decoded=decode_complete<typename A::Type>(wire,Limits{},[&](FieldReader&f)->Result<typename A::Type>{for(unsigned i=0;i<residue;++i){auto bit=f.read_bit();if(!bit)return Result<typename A::Type>::failure(bit.error());REQUIRE(bit.value());}return A::get(f);});REQUIRE(decoded&&equivalent(v,decoded.value()));
 if(residue==0){REQUIRE(A::encode(v).value().octets==wire);REQUIRE(equivalent(v,A::decode(wire).value()));for(std::size_t n=0;n<wire.size();++n)error(A::decode(std::span<const std::byte>(wire.data(),n)),ErrorCode::truncated_input,n*8);auto trailing=wire;trailing.push_back(std::byte{0});error(A::decode(trailing),ErrorCode::trailing_data,wire.size()*8);for(auto position:b.padding){auto bad=wire;bad[position/8]|=static_cast<std::byte>(0x80u>>(position%8));error(A::decode(bad),ErrorCode::nonzero_padding,position);}
  for(bool less:{false,true}){Limits l;l.max_input_octets=wire.size()-(less?1u:0u);auto in=A::decode(wire,l);if(less)error(in,ErrorCode::resource_limit,0);else REQUIRE(in);l=Limits{};l.max_output_octets=wire.size()-(less?1u:0u);auto out=A::encode(v,l);if(less)REQUIRE(!out&&out.error().code==ErrorCode::resource_limit);else REQUIRE(out);l=Limits{};l.max_wire_bits=wire.size()*8-(less?1u:0u);in=A::decode(wire,l);out=A::encode(v,l);if(less)REQUIRE(!in&&!out&&in.error().code==ErrorCode::resource_limit&&out.error().code==ErrorCode::resource_limit);else REQUIRE(in&&out);}
 }
}}
static c::State state(unsigned n){c::State s{};if(n==0)s.value=c::State::Known::low;else if(n==1)s.value=c::State::Known::high;else if(n==2)s.value=c::State::Known::addition;else s.value=c::State::UnknownExtension{n==3?64u:UINT64_MAX};return s;}
int main(){REQUIRE(std::get<c::State::Known>(c::State{}.value)==c::State::Known::low);check<Tri>({});check<Six>({});check<Outer>({});check<Pick>({});REQUIRE(c::encode_octet(255).value().octets==std::vector<std::byte>{std::byte{0xff}});REQUIRE(c::decode_octet(std::array<std::byte,1>{std::byte{0xff}}).value()==255);REQUIRE(c::encode_pair(65535).value().octets==std::vector<std::byte>({std::byte{0xff},std::byte{0xff}}));REQUIRE(c::decode_pair(std::array<std::byte,2>{std::byte{0xff},std::byte{0xff}}).value()==65535);for(unsigned n=0;n<5;++n){auto st=state(n);check<Tri>(c::Tri_state{st});check<Six>(c::Six_state{st});for(unsigned presence=0;presence<9;++presence){c::Inner in{};in.identity=UINT64_C(1099511627775);if(presence%3)in.state=st;if(presence/3)in.flag=presence/3==2;check<Inner>(in);check<Six>(c::Six_inner{in});c::Outer out{};out.selection=c::Six_tri{c::Tri_state{st}};if(presence%3)out.inner=in;if(presence/3)out.flag=presence/3==2;check<Outer>(out);}}
 for(std::uint64_t n:{UINT64_C(0),UINT64_C(255),UINT64_C(256),UINT64_C(65536),UINT64_C(4294967295)}){check<Tri>(c::Tri_word{n});check<Six>(c::Six_word{n});check<Pick>(r::Pick_counter{n});}
 check<Tri>(c::Tri_flag{false});check<Tri>(c::Tri_flag{true});check<Six>(c::Six_flag{true});check<Six>(c::Six_identity{UINT64_C(4294967296)});check<Six>(c::Six_identity{UINT64_C(1099511627775)});check<Solo>(c::Solo_only{true});check<Empty>({});
 for(unsigned n=0;n<5;++n){r::Mode s{};if(n<3)s.value=n==0?r::Mode::Known::low:n==1?r::Mode::Known::high:r::Mode::Known::addition;else s.value=r::Mode::UnknownExtension{n==3?64u:UINT64_MAX};check<Pick>(r::Pick_mode{s});r::Envelope e{};e.pick=r::Pick_signal{true};e.mode=s;e.signal=false;check<Envelope>(e);}
 error(Tri::decode(std::array<std::byte,1>{std::byte{0xc0}}),ErrorCode::constraint_violation,0);error(Six::decode(std::array<std::byte,1>{std::byte{0xc0}}),ErrorCode::constraint_violation,0);
 error(Tri::encode(c::Tri_word{UINT64_C(4294967296)}),ErrorCode::constraint_violation,2);error(Six::encode(c::Six_identity{UINT64_MAX}),ErrorCode::constraint_violation,3);
 c::State invalid{};invalid.value=static_cast<c::State::Known>(42);error(Tri::encode(c::Tri_state{invalid}),ErrorCode::constraint_violation,2);invalid.value=c::State::UnknownExtension{0};error(Six::encode(c::Six_state{invalid}),ErrorCode::constraint_violation,3);
 error(encode_complete(c::Empty{},Limits{},[](FieldWriter&f){(void)f.write_enumerated({false,3},3,false);return c::compound_codec::put_Empty(f,{});}),ErrorCode::constraint_violation,0);
 error(encode_complete(c::Tri_flag{true},Limits{},[](FieldWriter&f){(void)f.write_enumerated({false,3},3,false);return c::compound_codec::put_Tri(f,c::Tri_flag{true});}),ErrorCode::constraint_violation,0);
 const std::array<std::byte,1> bad{std::byte{0xc0}};error(decode_complete<c::Empty>(bad,Limits{},[](FieldReader&f){(void)f.read_enumerated(3,false);return c::compound_codec::get_Empty(f);}),ErrorCode::constraint_violation,0);
}
