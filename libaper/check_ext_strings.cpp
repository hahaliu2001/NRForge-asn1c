#include "runtime.hpp"
#include <cstdio>
#include <cstdlib>
#include <new>
#include <array>
using namespace nrforge::aper;
static bool fail_next=false;
void* operator new(std::size_t n) { if(fail_next) {fail_next=false; throw std::bad_alloc();} if(auto p=std::malloc(n?n:1)) return p; throw std::bad_alloc(); }
void operator delete(void* p) noexcept {std::free(p);} void operator delete(void* p,std::size_t) noexcept {std::free(p);}
#define R(x) do {if(!(x)){std::fprintf(stderr,"extstrings:%d %s\n",__LINE__,#x);std::abort();}}while(0)
struct Model {
 std::vector<std::byte> b; std::size_t n=0;
 void bit(bool x){if(n%8==0)b.push_back(std::byte{0});if(x)b.back()|=static_cast<std::byte>(0x80u>>(n%8));++n;}
 void number(std::size_t x,unsigned w){for(unsigned i=w;i;--i)bit(((x>>(i-1))&1)!=0);}
 void align(){while(n%8)bit(false);}
};
BitString data(std::size_t n){BitString b{{},n};b.octets.resize(n/8+(n%8!=0));for(std::size_t i=0;i<n;++i)if(i%3)b.octets[i/8]|=static_cast<std::byte>(0x80u>>(i%8));return b;}
void vectors(){
 std::size_t checks=0;
 for(bool octets:{false,true}) for(auto range:std::array<std::array<std::size_t,2>,4>{{{1,160},{16,16},{8,8},{0,256}}}) for(unsigned prefix=0;prefix<8;++prefix) for(auto count:std::array<std::size_t,6>{0,range[0],range[1],range[1]+1,128,16383}) {
  const bool extension=count<range[0]||count>range[1];auto payload=data(octets?count*8:count);Model m;for(unsigned i=0;i<prefix;++i)m.bit(true);m.bit(extension);
  const bool variable=extension||range[0]!=range[1];const auto cardinality=range[1]-range[0]+1;
  if(extension||(!variable&&range[1]>(octets?2u:16u))||(variable&&cardinality>=256))m.align();
  unsigned width=0;if(extension)width=count<128?8:16;else if(variable){if(cardinality>=257)width=16;else for(auto x=cardinality-1;x;x>>=1)++width;}
  m.number(extension?(count|(width==16?0x8000u:0u)):count-range[0],width);if(variable)m.align();
  for(std::size_t i=0;i<payload.bit_count;++i)m.bit((std::to_integer<unsigned>(payload.octets[i/8])&(0x80u>>(i%8)))!=0);
  const auto end=m.n;m.bit(true);m.align();EncodeContext ec;BitWriter w(ec);for(unsigned i=0;i<prefix;++i)R(w.write_bit(true));
  R(octets?w.write_octet_string(payload.octets,range[0],range[1],false,true):w.write_bit_string(payload,range[0],range[1],false,true));R(w.cursor_bit()==end&&ec.wire_bits()==end);R(w.write_bit(true));auto encoded=w.finish();R(encoded&&encoded.value().octets==m.b);
  DecodeContext dc;auto made=BitReader::make(m.b,dc);R(made);auto r=std::move(made).value();for(unsigned i=0;i<prefix;++i)R(r.read_bit().value());
  if(octets){auto got=r.read_octet_string_owned(range[0],range[1],false,true);R(got&&got.value()==payload.octets);}else{auto got=r.read_bit_string_owned(range[0],range[1],false,true);R(got&&got.value().octets==payload.octets&&got.value().bit_count==count);}R(r.cursor_bit()==end&&dc.wire_bits()==end);R(r.read_bit().value());R(r.validate_complete_value());++checks;
 }
 std::printf("PASS %zu extensible BIT/OCTET model vectors\n",checks);
}
void failures(){
 auto value=data(17);EncodeContext c;BitWriter w(c);R(w.write_bit(true));fail_next=true;auto result=w.write_bit_string(value,16,16,false,true);R(!result&&result.error().code==ErrorCode::allocation_failure&&result.error().bit_offset==1);R(w.cursor_bit()==1&&c.wire_bits()==1);auto sticky=w.write_bit(false);R(!sticky&&sticky.error().code==result.error().code&&sticky.error().bit_offset==1);
 auto large=data(16384);EncodeContext cap;BitWriter cw(cap);auto capped=cw.write_bit_string(large,16,16,false,true);R(!capped&&capped.error().code==ErrorCode::resource_limit&&cw.cursor_bit()==0);
 auto wire=std::array<std::byte,4>{std::byte{0x80},std::byte{0x11},std::byte{0x55},std::byte{0x55}};DecodeContext dc;auto made=BitReader::make(wire,dc);R(made);auto reader=std::move(made).value();auto truncated=reader.read_bit_string_owned(16,16,false,true);R(!truncated&&truncated.error().code==ErrorCode::truncated_input&&truncated.error().bit_offset==32&&reader.cursor_bit()==0&&dc.wire_bits()==0);
 auto valid=std::array<std::byte,5>{std::byte{0x80},std::byte{0x11},std::byte{0x55},std::byte{0x55},std::byte{0x00}};DecodeContext alloc;auto rm=BitReader::make(valid,alloc);R(rm);auto ar=std::move(rm).value();fail_next=true;auto oom=ar.read_bit_string_owned(16,16,false,true);R(!oom&&oom.error().code==ErrorCode::allocation_failure&&ar.cursor_bit()==0&&alloc.wire_bits()==0);
 auto bad=valid;bad[0]=std::byte{0x81};DecodeContext padding;auto pm=BitReader::make(bad,padding);R(pm);auto pr=std::move(pm).value();auto nonzero=pr.read_bit_string_owned(16,16,false,true);R(!nonzero&&nonzero.error().code==ErrorCode::nonzero_padding&&nonzero.error().bit_offset==7&&pr.cursor_bit()==0);
 Limits limits;limits.max_wire_bits=32;EncodeContext budget(limits);BitWriter bw(budget);auto limited=bw.write_bit_string(value,16,16,false,true);R(!limited&&limited.error().code==ErrorCode::resource_limit&&bw.cursor_bit()==0);
}
int main(){vectors();failures();}
