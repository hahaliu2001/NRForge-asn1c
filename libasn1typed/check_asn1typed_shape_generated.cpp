#include <runtime.hpp>
#include "types.hpp"
#include "mapping.hpp"
#include "codec.hpp"
#include "renamed_types.hpp"
#include "renamed_mapping.hpp"
#include "renamed_codec.hpp"
#include "physical_types.hpp"
#include "physical_mapping.hpp"
#include "physical_codec.hpp"
#include "physical_adapters.hpp"
#include <cstdio>
#include <cstdlib>
#include <type_traits>
#include <vector>
#define REQUIRE(x) do { if(!(x)) { std::fprintf(stderr,"%s:%d: %s\n",__FILE__,__LINE__,#x); std::abort(); } } while(0)
using Phase=shapetest::Panel_aper::field_1_type;
using Status=shapetest::Panel_aper::field_2_type;
using Copy=shapetest::Panel_aper::field_3_type;
using Class=shapetest::Panel_aper::field_4_type;
static_assert(!std::is_same_v<Phase,Copy>);
static_assert(static_cast<std::int64_t>(Phase::Known::high)==9);
static_assert(shapetest::Panel_aper::field_1_payload_mapping::source_ordinal_to_per_index[0]==2);
struct Bits {
 std::vector<bool> bits;
 void put(std::uint64_t n,unsigned width){for(unsigned i=width;i>0;--i)bits.push_back(((n>>(i-1))&1u)!=0);}
 void align(){while(bits.size()%8)bits.push_back(false);}
 void enumeration(bool ext,std::uint64_t index){put(ext?1u:0u,1);if(!ext)put(index,1);else if(index<64){put(0,1);put(index,6);}else{put(1,1);align();unsigned octets=1;for(auto n=index;n>255;n>>=8)++octets;put(octets,8);put(index,octets*8);}}
 std::vector<std::byte> finish(){if(bits.empty())bits.resize(8,false);align();std::vector<std::byte> b(bits.size()/8);for(std::size_t i=0;i<bits.size();++i)if(bits[i])b[i/8]|=static_cast<std::byte>(0x80u>>(i%8));return b;}
};
static unsigned phase_index(Phase::Known k){return k==Phase::Known::low?0u:k==Phase::Known::middle?1u:2u;}
static void model(Bits& b,const shapetest::Panel& p){
 b.put(p.status.has_value()?1u:0u,1);b.put(p.copy.has_value()?1u:0u,1);b.put(p.marker?1u:0u,1);b.put(phase_index(p.phase.value),2);
 if(p.status){if(auto k=std::get_if<Status::Known>(&p.status->value)){if(*k==Status::Known::start||*k==Status::Known::end)b.enumeration(false,*k==Status::Known::end?1u:0u);else b.enumeration(true,*k==Status::Known::between?0u:1u);}else b.enumeration(true,std::get<Status::UnknownExtension>(p.status->value).index);}
 if(p.copy){const auto k=p.copy->value;b.put(k==Copy::Known::low?0u:k==Copy::Known::middle?1u:2u,2);}
 b.put(p.class_.value==Class::Known::else_?1u:0u,1);b.align();b.put(p.amount,8);
}
static void equal(const shapetest::Panel& a,const shapetest::Panel& b){
 REQUIRE(a.marker==b.marker&&a.phase.value==b.phase.value&&a.amount==b.amount&&a.class_.value==b.class_.value);
 REQUIRE(a.status.has_value()==b.status.has_value()&&a.copy.has_value()==b.copy.has_value());
 if(a.copy)REQUIRE(a.copy->value==b.copy->value);
 if(a.status){REQUIRE(a.status->value.index()==b.status->value.index());if(auto k=std::get_if<Status::Known>(&a.status->value))REQUIRE(*k==std::get<Status::Known>(b.status->value));else REQUIRE(std::get<Status::UnknownExtension>(a.status->value).index==std::get<Status::UnknownExtension>(b.status->value).index);}
}
int main(){
 for(auto k:{Phase::Known::low,Phase::Known::middle,Phase::Known::high})for(unsigned status=0;status<8;++status)for(bool copy:{false,true}){
  shapetest::Panel p{};p.marker=true;p.phase.value=k;p.amount=255;p.class_.value=Class::Known::else_;
  if(copy)p.copy=Copy{Copy::Known::middle};
  if(status){Status s{};if(status<=4)s.value=static_cast<Status::Known>(status==1?0:status==2?3:status==3?1:10);else s.value=Status::UnknownExtension{status==5?2u:status==6?63u:64u};p.status=s;}
  Bits ref;model(ref,p);const auto end=ref.bits.size();const auto expected=ref.finish();auto encoded=shapetest::encode_panel(p);REQUIRE(encoded&&encoded.value().octets==expected&&encoded.value().last_field_end_bit==end);auto decoded=shapetest::decode_panel(expected);REQUIRE(decoded);equal(p,decoded.value());
  auto trailing=expected;trailing.push_back(std::byte{0});auto bad=shapetest::decode_panel(trailing);REQUIRE(!bad&&bad.error().code==nrforge::aper::ErrorCode::trailing_data&&bad.error().bit_offset==expected.size()*8);
  shapetest::Envelope e{};e.lead=true;e.branch=shapetest::Branch_aper::wrapper_0{p};e.panels.elements={p,p};Bits outer;outer.put(1,1);outer.put(0,1);model(outer,p);outer.put(2,2);model(outer,p);model(outer,p);const auto bytes=outer.finish();auto out=shapetest::encode_envelope(e);REQUIRE(out&&out.value().octets==bytes);auto back=shapetest::decode_envelope(bytes);REQUIRE(back&&back.value().lead&&back.value().panels.elements.size()==2);equal(p,std::get<shapetest::Branch_aper::wrapper_0>(back.value().branch).value);equal(p,back.value().panels.elements[0]);equal(p,back.value().panels.elements[1]);
 }
 {
 shapetest::Panel invalid{};invalid.phase.value=static_cast<Phase::Known>(42);
 auto bad=shapetest::encode_panel(invalid);REQUIRE(!bad&&bad.error().code==nrforge::aper::ErrorCode::constraint_violation&&bad.error().bit_offset==3);
 const std::vector<std::byte> reserved{std::byte{0x18}};auto rejected=shapetest::decode_panel(reserved);REQUIRE(!rejected&&rejected.error().code==nrforge::aper::ErrorCode::constraint_violation&&rejected.error().bit_offset==3);
 auto sticky=nrforge::aper::encode_complete(invalid,nrforge::aper::Limits{},[&](nrforge::aper::FieldWriter& f){auto first=shapetest::compound_codec::put_Panel(f,invalid);REQUIRE(!first&&first.error().bit_offset==3);auto again=shapetest::compound_codec::put_Panel(f,shapetest::Panel{});REQUIRE(!again&&again.error().code==first.error().code&&again.error().bit_offset==3);return nrforge::aper::Result<void>::success();});REQUIRE(!sticky&&sticky.error().bit_offset==3);
 }
 renamedshapes::Display d{};using Shade=decltype(d.shade);using Mode=renamedshapes::Display_aper::field_2_type;using Keyword=decltype(d.class_);d.marker=true;d.shade.value=Shade::Known::upper;d.mode=Mode{Mode::Known::between};d.class_.value=Keyword::Known::else_;d.amount=17;
 shapetest::Panel p{};p.marker=true;p.phase.value=Phase::Known::high;p.status=Status{Status::Known::between};p.class_.value=Class::Known::else_;p.amount=17;Bits r;model(r,p);auto encoded=renamedshapes::encode_display(d);REQUIRE(encoded&&encoded.value().octets==r.finish());auto decoded=renamedshapes::decode_display(encoded.value().octets);REQUIRE(decoded&&decoded.value().shade.value==d.shade.value&&decoded.value().amount==17&&decoded.value().mode&&std::get<Mode::Known>(decoded.value().mode->value)==Mode::Known::between);
 {
 using W=iocshapes::EntryMapping::wrapper_0;using Policy=iocshapes::EntryMapping::criticality_type;
 W w{};using E=decltype(w.value.phase);w.value.marker=true;w.value.phase.value=E::Known::high;
 iocshapes::Entry entry{};entry.number=91;entry.policy.value=Policy::Known::reject;entry.content=w;
 iocshapes::Message msg{};msg.entries.elements.push_back(entry);
 const std::vector<std::byte> bytes{std::byte{0},std::byte{1},std::byte{0},std::byte{91},std::byte{0},std::byte{1},std::byte{0xc0}};
 auto wire=iocshapes::encode_message(msg);REQUIRE(wire&&wire.value().octets==bytes);auto back=iocshapes::decode_message(bytes);REQUIRE(back&&back.value().entries.elements.size()==1);
 const auto& value=std::get<W>(back.value().entries.elements[0].content).value;REQUIRE(value.marker&&value.phase.value==E::Known::high);
 }
 std::puts("PASS inline enums: independent complete vectors, optional root/addition/unknown values, nested choices/collections and renamed schemas");
}
