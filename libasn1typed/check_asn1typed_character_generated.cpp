#include "runtime.hpp"
#include "types.hpp"
#include "mapping.hpp"
#include "codec.hpp"
#include "physical_types.hpp"
#include "physical_mapping.hpp"
#include "physical_codec.hpp"
#include "physical_adapters.hpp"
#include <cstdlib>
#include <iostream>
#include <iomanip>
#include <type_traits>
#define REQUIRE(x) do { if(!(x)) { std::cerr << __LINE__ << ": " << #x << '\n'; std::abort(); } } while(false)
static long allocation_countdown=-1;
static bool allocation_triggered=false;
void* operator new(std::size_t size) {
 if(allocation_countdown>=0) { if(!allocation_countdown) {allocation_countdown=-1;allocation_triggered=true;throw std::bad_alloc();} --allocation_countdown; }
 if(void* memory=std::malloc(size?size:1)) {return memory;} throw std::bad_alloc();
}
void* operator new[](std::size_t size) {return ::operator new(size);}
void operator delete(void* memory) noexcept {std::free(memory);}
void operator delete[](void* memory) noexcept {std::free(memory);}
void operator delete(void* memory,std::size_t) noexcept {std::free(memory);}
void operator delete[](void* memory,std::size_t) noexcept {std::free(memory);}
using namespace nrforge::aper;
static std::string hex(std::string_view value) { std::string out; constexpr char digits[]="0123456789abcdef"; for(char ch:value) { const auto byte=static_cast<unsigned char>(ch); out+=digits[byte>>4];out+=digits[byte&15u]; } return out; }
static std::string hex(std::span<const std::byte> value) { std::string out; constexpr char digits[]="0123456789abcdef"; for(auto byte:value) { const auto n=std::to_integer<unsigned>(byte);out+=digits[n>>4];out+=digits[n&15u]; } return out; }
struct Model {
 std::vector<bool> bits;
 void number(std::uint64_t value,unsigned width) { for(unsigned i=0;i<width;++i) bits.push_back(((value>>(width-i-1))&1u)!=0); }
 void align() { while(bits.size()%8) bits.push_back(false); }
 std::vector<std::byte> finish() { align();std::vector<std::byte> out(bits.size()/8,std::byte{0});for(std::size_t i=0;i<bits.size();++i) if(bits[i]) out[i/8]|=static_cast<std::byte>(0x80u>>(i%8));return out; }
 void string(std::string_view v,std::size_t lower,std::size_t upper,bool extensible,CharacterStringKind kind) {
  if(kind==CharacterStringKind::utf8) { align(); number(v.size()|(v.size()>=128?0x8000u:0u),v.size()<128?8u:16u);for(char c:v) number(static_cast<unsigned char>(c),8);return; }
  const bool unconstrained=lower==0&&upper==0;
  const bool extension=unconstrained||v.size()<lower||v.size()>upper;
  if(extensible) number(extension?1u:0u,1);
  if(extension) {align();number(v.size()|(v.size()>=128?0x8000u:0u),v.size()<128?8u:16u);}
  else if(lower!=upper) { const auto range=upper-lower+1;if(range>=256)align();unsigned width=0;for(auto n=range-1;n;n>>=1)++width;if(range>=256)width=(width+7u)/8u*8u;number(v.size()-lower,width); }
  if(!v.empty() && (extension || (lower!=upper?upper>=2:upper>2))) align();
  for(char c:v)number(static_cast<unsigned char>(c),8);
 }
};
static Result<void> generated_put(FieldWriter& f,const std::string& value,CharacterStringKind kind,std::size_t lower,std::size_t upper,bool extensible) {
 using namespace characters::compound_codec;
 if(lower==0 && upper==0) {if(kind==CharacterStringKind::visible)return put_UriAddress(f,value);if(kind==CharacterStringKind::printable)return put_UnboundedPrintable(f,value);return put_UnboundedUnicode(f,value);}
 if(lower==1 && upper==150 && extensible) { if(kind==CharacterStringKind::printable)return put_Name(f,value);if(kind==CharacterStringKind::visible)return put_Visible(f,value);return put_Unicode(f,value); }
 if(kind==CharacterStringKind::utf8 && lower==1 && upper==1)return put_ScalarOne(f,value);
 if(lower==2 && upper==2)return put_FixedTwo(f,value);
 if(lower==1 && upper==2)return put_VariableTwo(f,value);
 if(lower==0 && upper==256) {if(kind==CharacterStringKind::printable)return put_Wide(f,value);return put_WideVisible(f,value);}
 REQUIRE(false);return Result<void>::failure({ErrorCode::invalid_argument,0});
}
static Result<std::string> generated_get(FieldReader& f,CharacterStringKind kind,std::size_t lower,std::size_t upper,bool extensible) {
 using namespace characters::compound_codec;
 if(lower==0 && upper==0) {if(kind==CharacterStringKind::visible)return get_UriAddress(f);if(kind==CharacterStringKind::printable)return get_UnboundedPrintable(f);return get_UnboundedUnicode(f);}
 if(lower==1 && upper==150 && extensible) { if(kind==CharacterStringKind::printable)return get_Name(f);if(kind==CharacterStringKind::visible)return get_Visible(f);return get_Unicode(f); }
 if(kind==CharacterStringKind::utf8 && lower==1 && upper==1)return get_ScalarOne(f);
 if(lower==2 && upper==2)return get_FixedTwo(f);
 if(lower==1 && upper==2)return get_VariableTwo(f);
 if(lower==0 && upper==256) {if(kind==CharacterStringKind::printable)return get_Wide(f);return get_WideVisible(f);}
 REQUIRE(false);return Result<std::string>::failure({ErrorCode::invalid_argument,0});
}
static std::size_t cases=0;
static void check(const std::string& value,CharacterStringKind kind,std::size_t lower,std::size_t upper,bool extensible,bool prefix,bool dump) {
 auto encoded=encode_complete(value,{},[&](FieldWriter& f) { if(prefix) { auto b=f.write_bit(true);if(!b)return b; }return generated_put(f,value,kind,lower,upper,extensible); }); REQUIRE(encoded);
 Model model;if(prefix) model.number(1,1);model.string(value,lower,upper,extensible,kind);auto expected=model.finish(); if(encoded.value().octets!=expected)std::cerr<<"Mismatch kind "<<static_cast<unsigned>(kind)<<" size "<<value.size()<<" bounds "<<lower<<"/"<<upper<<" prefix "<<prefix<<" actual "<<hex(encoded.value().octets)<<" expected "<<hex(expected)<<"\n"; REQUIRE(encoded.value().octets==expected);
 auto decoded=decode_complete<std::string>(encoded.value().octets,{},[&](FieldReader& f) {if(prefix){auto b=f.read_bit();REQUIRE(b&&b.value());}return generated_get(f,kind,lower,upper,extensible);}); REQUIRE(decoded&&decoded.value()==value);
 ++cases;
 if(dump)std::cout<<static_cast<unsigned>(kind)<<' '<<(lower==0&&upper==0?std::string("none"):std::to_string(lower))<<' '<<(lower==0&&upper==0?std::string("none"):std::to_string(upper))<<' '<<extensible<<' '<<prefix<<' '<<(value.empty()?"-":hex(value))<<' '<<hex(encoded.value().octets)<<'\n';
}
int main(int argc,char**) {
 const bool dump=argc>1;
 static_assert(std::is_same_v<characters::Name,std::string>);
 static_assert(!characters::Unicode_aper::size_constraint_per_visible);
 static_assert(characters::Name_aper::bits_per_character==8);
 constexpr std::string_view printable=" '()+,-./0123456789:=?ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
 for(char c:printable) for(bool prefix:{false,true})check(std::string(1,c),CharacterStringKind::printable,1,150,true,prefix,dump);
 for(unsigned c=32;c<=126;++c) for(bool prefix:{false,true})check(std::string(1,static_cast<char>(c)),CharacterStringKind::visible,1,150,true,prefix,dump);
 for(const auto count:{0u,1u,2u,127u,128u,150u,151u,16383u})for(auto kind:{CharacterStringKind::printable,CharacterStringKind::visible})for(bool prefix:{false,true})check(std::string(count,'Z'),kind,1,150,true,prefix,dump);
 for(const std::string& value:{std::string{},std::string("A"),std::string("\xc3\xa9"),std::string("\xe4\xb8\xad"),std::string("\xf0\x9f\x98\x80"),std::string("A\xc3\xa9\xe4\xb8\xad\xf0\x9f\x98\x80")})for(bool prefix:{false,true})check(value,CharacterStringKind::utf8,1,150,true,prefix,dump);
 for(bool prefix:{false,true}) { check("\xc3\xa9",CharacterStringKind::utf8,1,1,false,prefix,dump);check("AB",CharacterStringKind::printable,2,2,false,prefix,dump);check("AB",CharacterStringKind::printable,1,2,false,prefix,dump);check("A",CharacterStringKind::printable,1,2,false,prefix,dump); }
 for(auto kind:{CharacterStringKind::printable,CharacterStringKind::visible})for(bool prefix:{false,true})check("ABC",kind,0,256,false,prefix,dump);
 check(std::string(150*4,'A'),CharacterStringKind::utf8,1,150,true,false,dump);

 for(auto kind:{CharacterStringKind::printable,CharacterStringKind::visible,CharacterStringKind::utf8}) for(unsigned count:{0u,1u,127u,128u,16383u}) for(bool prefix:{false,true}) check(std::string(count,'A'),kind,0,0,false,prefix,dump);
 check("https://trace.example/NGAP?cell=1",CharacterStringKind::visible,0,0,false,true,dump);
 check("\xc3\xa9",CharacterStringKind::utf8,0,0,false,false,dump);
 static_assert(characters::UriAddress_aper::unconstrained);
 REQUIRE(!characters::encode_uri_address(std::string(16384,'A')));
 for(auto kind:{CharacterStringKind::printable,CharacterStringKind::visible,CharacterStringKind::utf8}) {
   auto fragmented=decode_complete<std::string>(std::vector<std::byte>{std::byte{0xc1}}, {}, [kind](FieldReader& f){return f.read_character_string_owned(0,0,false,kind,true);});
   REQUIRE(!fragmented&&fragmented.error().code==ErrorCode::resource_limit&&fragmented.error().bit_offset==0);
 }

 {EncodeContext c;BitWriter w(c);allocation_countdown=0;allocation_triggered=false;auto r=w.write_character_string(std::string_view("https://trace.example/NGAP?cell=1"),0,0,false,CharacterStringKind::visible,true);allocation_countdown=-1;REQUIRE(allocation_triggered&&!r&&r.error().code==ErrorCode::allocation_failure&&w.cursor_bit()==0&&c.wire_bits()==0);}
 auto uri=characters::encode_uri_address("@");REQUIRE(uri&&hex(uri.value().octets)=="0140");
 auto illegal_uri=characters::encode_uri_address("\n");REQUIRE(!illegal_uri&&illegal_uri.error().code==ErrorCode::constraint_violation);
 {EncodeContext c;BitWriter w(c);REQUIRE(w.write_bit(true));auto r=w.write_character_string(std::string(16384,'A'),0,0,false,CharacterStringKind::visible,true);REQUIRE(!r&&w.cursor_bit()==1&&c.wire_bits()==1);}
 auto name=characters::encode_name("A");REQUIRE(name&&hex(name.value().octets)=="000041");REQUIRE(characters::decode_name(name.value().octets).value()=="A");
 auto visible=characters::encode_visible("@");REQUIRE(visible&&hex(visible.value().octets)=="000040");
 auto unicode=characters::encode_unicode("\xc3\xa9");REQUIRE(unicode&&hex(unicode.value().octets)=="02c3a9");
 characters::Message message{};message.mark=true;message.name="AMF";message.visible="RAN@";message.unicode="\xe4\xb8\xad";
 auto compound=characters::encode_message(message);REQUIRE(compound);auto result=characters::decode_message(compound.value().octets);REQUIRE(result&&result.value().mark&&result.value().name==message.name&&result.value().visible==message.visible&&result.value().unicode==message.unicode);
 for(const auto& invalid:{std::string("\xc0\xaf"),std::string("\xed\xa0\x80"),std::string("\xf4\x90\x80\x80"),std::string("\x80"),std::string("\xe2\x82")}) { auto e=characters::encode_unicode(invalid);REQUIRE(!e&&e.error().code==ErrorCode::constraint_violation&&e.error().bit_offset==0); }
 auto scalar_limit=encode_complete(std::string_view("AB"),{},[](FieldWriter& f){return f.write_character_string("AB",1,1,false,CharacterStringKind::utf8);});REQUIRE(!scalar_limit&&scalar_limit.error().code==ErrorCode::constraint_violation);
 REQUIRE(!characters::encode_name("@")); REQUIRE(!characters::encode_visible(std::string(1,'\n')));
 REQUIRE(!characters::encode_name(std::string(16384,'A')));
 for(std::size_t n=0;n<name.value().octets.size();++n) {auto d=characters::decode_name(std::span<const std::byte>(name.value().octets).first(n));REQUIRE(!d&&d.error().code==ErrorCode::truncated_input);}
 auto badpadding=name.value().octets;badpadding[1]|=std::byte{1};auto pad=characters::decode_name(badpadding);REQUIRE(!pad&&pad.error().code==ErrorCode::nonzero_padding&&pad.error().bit_offset==15);
 auto badchar=name.value().octets;badchar[2]=std::byte{0x40};auto bad=characters::decode_name(badchar);REQUIRE(!bad&&bad.error().code==ErrorCode::constraint_violation&&bad.error().bit_offset==16);
 DecodeContext context;std::vector<std::byte> malformed{std::byte{2},std::byte{0xc0},std::byte{0xaf}};auto reader=BitReader::make(malformed,context);REQUIRE(reader);auto utf=reader.value().read_character_string_owned(1,150,true,CharacterStringKind::utf8);REQUIRE(!utf&&reader.value().cursor_bit()==0&&context.wire_bits()==0);auto again=reader.value().read_bit();REQUIRE(!again&&again.error().code==utf.error().code&&again.error().bit_offset==utf.error().bit_offset);
 for(unsigned which=0;which<3;++which) {Limits limits; if(which==0)limits.max_output_octets=2;if(which==1)limits.max_wire_bits=23;if(which==2)limits.max_input_octets=2; if(which<2){auto limited=characters::encode_name("A",limits);REQUIRE(!limited&&limited.error().code==ErrorCode::resource_limit);}else{auto limited=characters::decode_name(name.value().octets,limits);REQUIRE(!limited&&limited.error().code==ErrorCode::resource_limit);} }
 EncodeContext ec;BitWriter writer(ec);FieldWriter fields(writer);auto fail=fields.write_character_string("@",1,150,true);REQUIRE(!fail&&writer.cursor_bit()==0&&ec.wire_bits()==0);auto sticky=fields.write_character_string("A",1,150,true);REQUIRE(!sticky&&sticky.error().bit_offset==fail.error().bit_offset&&sticky.error().code==fail.error().code);
 const std::string long_value(40,'A');
 { EncodeContext c;BitWriter w(c);allocation_countdown=0;allocation_triggered=false;auto result=w.write_character_string(long_value,1,150,true);allocation_countdown=-1;REQUIRE(allocation_triggered&&!result&&result.error().code==ErrorCode::allocation_failure&&w.cursor_bit()==0&&c.wire_bits()==0&&c.logical_output_octets()==0); }
 auto long_utf=characters::encode_unicode(long_value);REQUIRE(long_utf);
 for(long point:{0L,1L}) {DecodeContext c;auto r=BitReader::make(long_utf.value().octets,c);REQUIRE(r);allocation_countdown=point;allocation_triggered=false;auto result=r.value().read_character_string_owned(1,150,true,CharacterStringKind::utf8);allocation_countdown=-1;REQUIRE(allocation_triggered&&!result&&result.error().code==ErrorCode::allocation_failure&&r.value().cursor_bit()==0&&c.wire_bits()==0);}
 {EncodeContext c;BitWriter w(c);REQUIRE(w.write_character_string("A",1,150,true));REQUIRE(w.finish());const auto cursor=w.cursor_bit();const auto bits=c.wire_bits();auto result=w.write_character_string("@",1,150,true);REQUIRE(!result&&result.error().code==ErrorCode::invalid_state&&w.cursor_bit()==cursor&&c.wire_bits()==bits&&!c.failed());}
 {EncodeContext c;BitWriter original(c);BitWriter moved(std::move(original));auto result=original.write_character_string("@",1,150,true);REQUIRE(!result&&result.error().code==ErrorCode::invalid_state&&!c.failed());REQUIRE(moved.write_character_string("A",1,150,true));}
 {DecodeContext c;auto r=BitReader::make(name.value().octets,c);REQUIRE(r);REQUIRE(r.value().read_character_string_owned(1,150,true));REQUIRE(r.value().validate_complete_value());auto result=r.value().read_character_string_owned(1,150,true);REQUIRE(!result&&result.error().code==ErrorCode::invalid_state&&!c.failed());}
 {DecodeContext c;auto r=BitReader::make(name.value().octets,c);REQUIRE(r);BitReader moved(std::move(r).value());auto result=r.value().read_character_string_owned(1,150,true);REQUIRE(!result&&result.error().code==ErrorCode::invalid_state&&!c.failed());REQUIRE(moved.read_character_string_owned(1,150,true));}
 auto open=encode_complete(long_value,{},[&](FieldWriter& f){return f.write_known_open_type([&](FieldWriter& child){return child.write_character_string(long_value,1,150,true,CharacterStringKind::utf8);});});REQUIRE(open);
 auto open_back=decode_complete<std::string>(open.value().octets,{},[](FieldReader& f){return f.read_known_open_type<std::string>([](FieldReader& child){return child.read_character_string_owned(1,150,true,CharacterStringKind::utf8);});});REQUIRE(open_back&&open_back.value()==long_value);
 auto malformed_open=open.value().octets;malformed_open[2]=std::byte{0xC0};
 {DecodeContext c;auto r=BitReader::make(malformed_open,c);REQUIRE(r);auto result=r.value().read_known_open_type<std::string>([](FieldReader& child){return child.read_character_string_owned(1,150,true,CharacterStringKind::utf8);});REQUIRE(!result&&result.error().code==ErrorCode::constraint_violation&&r.value().cursor_bit()==0&&c.wire_bits()==0);}
 using W=ioccharacters::EntryMapping::wrapper_0; using Policy=ioccharacters::EntryMapping::criticality_type;
 W wrapper{};wrapper.value.marker=true;wrapper.value.name="A";wrapper.value.visible="@";wrapper.value.unicode="\xc3\xa9";
 ioccharacters::Entry entry{};entry.number=91;entry.policy.value=Policy::Known::reject;entry.content=wrapper;
 ioccharacters::Message physical{};physical.entries.elements.push_back(entry);
 auto physical_wire=ioccharacters::encode_message(physical);REQUIRE(physical_wire&&hex(physical_wire.value().octets)=="0001005b0009e0004100004002c3a9");
 auto physical_back=ioccharacters::decode_message(physical_wire.value().octets);REQUIRE(physical_back&&physical_back.value().entries.elements.size()==1);
 const auto& payload=std::get<W>(physical_back.value().entries.elements[0].content).value;REQUIRE(payload.marker&&payload.name=="A"&&payload.visible=="@"&&payload.unicode=="\xc3\xa9");
 if(!dump)std::cout<<"PASS character generated/runtime "<<cases<<" independent-model vectors\n";
}
