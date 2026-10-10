#include <runtime.hpp>
#include "private_types.hpp"
#include "private_mapping.hpp"
#include "private_codec.hpp"
#include "private_adapters.hpp"
#include <cstdio>
#include <cstdlib>
#include <string_view>
#define CHECK(x) do { if(!(x)) { std::fprintf(stderr,"check failed line %d: %s\n",__LINE__,#x); std::abort(); } } while(0)
static std::vector<std::byte> hex(std::string_view s) {
 std::vector<std::byte> v;
 auto digit=[](char c)->unsigned { return c <= '9' ? static_cast<unsigned>(c-'0') : static_cast<unsigned>(c-'a'+10); };
 for(std::size_t i=0;i<s.size();i+=2) v.push_back(static_cast<std::byte>((digit(s[i])<<4)|digit(s[i+1])));
 return v;
}
int main(int argc, char**) {
 using namespace privatekeys;
 using P = PrivateKeysPolicy::Known;
 if(argc > 1) {
  const unsigned locals[]={0,1,255,65535};
  const char* global[]={"00","2a864886f70d","883703","9480808080808080805001"};
  const std::size_t lengths[]={1,2,3,7,16,127,128,255,1024,16383};
  for(unsigned key=0;key<8;++key) for(unsigned policy=0;policy<3;++policy) for(auto length:lengths) {
   Message m{};Entry e{};
   if(key<4)e.id=PrivateKeysSelector_local{locals[key]};else e.id=PrivateKeysSelector_global{hex(global[key-4])};
   e.criticality.value=static_cast<P>(policy);
   for(std::size_t j=0;j<length;++j)e.value.push_back(static_cast<std::byte>(j%256));
   m.entries.elements.push_back(e);auto wire=encode_message(m);CHECK(wire);
   auto decoded=decode_message(wire.value().octets);CHECK(decoded);const auto& d=decoded.value().entries.elements[0];
   CHECK(d.value==e.value && d.criticality.value==e.criticality.value && d.id.index()==e.id.index());
   if(key<4)CHECK(std::get<PrivateKeysSelector_local>(d.id).value==locals[key]);else CHECK(std::get<PrivateKeysSelector_global>(d.id).value==hex(global[key-4]));
   std::printf("%u %u %zu ",key,policy,length);
   for(auto byte:wire.value().octets)std::printf("%02x",std::to_integer<unsigned>(byte));
   std::puts("");
  }
  return 0;
 }
 const auto raw = hex("00ff80");
 for(unsigned n=0;n<4;++n) {
  Message m{}; Entry e{}; e.criticality.value=P::ignore; e.value=raw;
  if(n<2) e.id=PrivateKeysSelector_local{n ? 65535u : 0u};
  else e.id=PrivateKeysSelector_global{hex(n==2 ? "2a864886f70d" : "883703")};
  m.entries.elements.push_back(e);
  const char* vectors[]={"000000000000400300ff80","00000000ffff400300ff80","00000080062a864886f70d400300ff80","0000008003883703400300ff80"};
  const auto expected=hex(vectors[n]); auto wire=encode_message(m); CHECK(wire); CHECK(wire.value().octets==expected);
  auto decoded=decode_message(expected); CHECK(decoded); CHECK(decoded.value().entries.elements.size()==1);
  const auto& d=decoded.value().entries.elements[0]; CHECK(d.criticality.value==P::ignore); CHECK(d.value==raw); CHECK(d.id.index()==e.id.index());
  if(n<2) CHECK(std::get<PrivateKeysSelector_local>(d.id).value==std::get<PrivateKeysSelector_local>(e.id).value);
  else CHECK(std::get<PrivateKeysSelector_global>(d.id).value==std::get<PrivateKeysSelector_global>(e.id).value);
  for(std::size_t cut=0;cut<expected.size();++cut) CHECK(!decode_message(std::span(expected).first(cut)));
  auto trail=expected;trail.push_back(std::byte{});CHECK(!decode_message(trail));
 }
 for(auto bad:{"", "80", "802a", "2a81"}) {
  Entry e{};e.id=PrivateKeysSelector_global{hex(bad)};e.value=raw;auto wire=encode_entry(e);CHECK(!wire);CHECK(wire.error().code==nrforge::aper::ErrorCode::constraint_violation);
 }
 { Entry e{};e.id=PrivateKeysSelector_local{65536};CHECK(!encode_entry(e)); }
 { Message m{};CHECK(!encode_message(m)); }
 { Entry e{};auto content=hex("81ffffffffffffffffffff7f00");e.id=PrivateKeysSelector_global{content};e.value=raw;auto wire=encode_entry(e);CHECK(wire);auto d=decode_entry(wire.value().octets);CHECK(d);CHECK(std::get<PrivateKeysSelector_global>(d.value().id).value==content); }
 {
  Entry e{};e.id=PrivateKeysSelector_local{0};e.value=raw;auto wire=encode_entry(e);CHECK(wire);
  nrforge::aper::Limits limits{};limits.max_retained_unknown_payload_octets=2;
  auto fail=decode_entry(wire.value().octets,limits);CHECK(!fail);CHECK(fail.error().code==nrforge::aper::ErrorCode::resource_limit);
  limits.max_retained_unknown_payload_octets=3;limits.max_retained_unknown_records=0;
  CHECK(!decode_entry(wire.value().octets,limits));
  limits.max_retained_unknown_records=1;CHECK(decode_entry(wire.value().octets,limits));
 }
 for(auto bad:{"0180", "028000", "0181", "00"}) {
  const auto bytes=hex(bad);
  auto d=nrforge::aper::decode_complete<std::vector<std::byte>>(bytes,{},[](nrforge::aper::FieldReader& f){return nrforge::aper::read_object_identifier(f);});
  CHECK(!d);
 }
 for(auto malformed:{"00", "0180", "028000", "0181"}) {
  auto framed=hex(malformed);framed.insert(framed.begin(),std::byte{0x80});
  nrforge::aper::DecodeContext ctx{};auto made=nrforge::aper::BitReader::make(framed,ctx);CHECK(made);auto reader=std::move(made).value();CHECK(reader.read_bit());
  const auto cursor=reader.cursor_bit(),wire=ctx.wire_bits();auto invalid=reader.read_object_identifier_owned();CHECK(!invalid);CHECK(invalid.error().code==nrforge::aper::ErrorCode::constraint_violation);CHECK(invalid.error().bit_offset==1);CHECK(reader.cursor_bit()==cursor && ctx.wire_bits()==wire);
  auto again=reader.read_bit();CHECK(!again && again.error().code==invalid.error().code && again.error().bit_offset==1);CHECK(reader.cursor_bit()==cursor && ctx.wire_bits()==wire);
 }
 { Entry e{};e.id=PrivateKeysSelector_local{0};CHECK(!encode_entry(e)); }
 std::puts("PASS lossless local/global private selector, raw open payload, canonical OID and independent native byte vectors");
}
