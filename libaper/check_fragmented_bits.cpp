#include "runtime.hpp"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <new>
using namespace nrforge::aper;
static bool fail_next=false;
void* operator new(std::size_t n){if(fail_next){fail_next=false;throw std::bad_alloc();}if(auto p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void operator delete(void* p) noexcept{std::free(p);}void operator delete(void* p,std::size_t) noexcept{std::free(p);}
#define R(x) do{if(!(x)){std::fprintf(stderr,"fragmentbits:%d %s\n",__LINE__,#x);std::abort();}}while(0)
struct Model{std::vector<std::byte>b;std::size_t n=0;void bit(bool x){if(n%8==0)b.push_back(std::byte{0});if(x)b.back()|=static_cast<std::byte>(0x80u>>(n%8));++n;}void number(std::size_t x,unsigned w){for(unsigned i=w;i;--i)bit(((x>>(i-1))&1)!=0);}void align(){while(n%8)bit(false);}};
BitString data(std::size_t n){BitString v{{},n};v.octets.resize(n/8+(n%8!=0));for(std::size_t i=0;i<n;++i)if(i%3)v.octets[i/8]|=static_cast<std::byte>(0x80u>>(i%8));return v;}
Model model(const BitString& v,unsigned prefix){Model m;for(unsigned i=0;i<prefix;++i)m.bit(true);m.align();std::size_t done=0;while(v.bit_count-done>=16384){auto units=(v.bit_count-done)/16384;if(units>4)units=4;m.number(192+units,8);for(std::size_t i=0;i<units*16384;++i)m.bit((std::to_integer<unsigned>(v.octets[(done+i)/8])&(0x80u>>((done+i)%8)))!=0);done+=units*16384;}auto tail=v.bit_count-done;m.number(tail|(tail>=128?0x8000u:0u),tail<128?8u:16u);for(std::size_t i=done;i<v.bit_count;++i)m.bit((std::to_integer<unsigned>(v.octets[i/8])&(0x80u>>(i%8)))!=0);return m;}
int main(){std::size_t checks=0;
for(unsigned prefix=0;prefix<8;++prefix)for(auto n:std::array<std::size_t,10>{0,1,127,128,16383,16384,65535,65536,131071,131072}){
 auto v=data(n);auto m=model(v,prefix);const auto end=m.n;m.bit(true);m.align();EncodeContext ec;BitWriter w(ec);for(unsigned i=0;i<prefix;++i)R(w.write_bit(true));R(w.write_bit_string_fragmented_size(v,0,131072));R(w.cursor_bit()==end&&ec.wire_bits()==end);R(w.write_bit(true));auto e=w.finish();R(e&&e.value().octets==m.b);
 DecodeContext dc;auto made=BitReader::make(m.b,dc);R(made);auto r=std::move(made).value();for(unsigned i=0;i<prefix;++i)R(r.read_bit().value());auto got=r.read_bit_string_owned_fragmented_size(0,131072);R(got&&got.value().bit_count==n&&got.value().octets==v.octets);R(r.cursor_bit()==end&&dc.wire_bits()==end);R(r.read_bit().value());R(r.validate_complete_value());
 for(auto cut:std::array<std::size_t,4>{0,8,end?end-1:0,end/2}){if(cut<prefix||cut>=end)continue;DecodeContext tc;auto mr=BitReader::make_bounded_for_test(m.b,cut,tc);R(mr);auto tr=std::move(mr).value();for(unsigned i=0;i<prefix;++i)R(tr.read_bit());auto bad=tr.read_bit_string_owned_fragmented_size(0,131072);R(!bad&&bad.error().code==ErrorCode::truncated_input&&bad.error().bit_offset==cut&&tr.cursor_bit()==prefix&&tc.wire_bits()==prefix);auto sticky=tr.read_bit();R(!sticky&&sticky.error().code==bad.error().code&&sticky.error().bit_offset==cut);}
 Limits l;l.max_wire_bits=end-1;EncodeContext bc(l);BitWriter bw(bc);for(unsigned i=0;i<prefix;++i)R(bw.write_bit(true));auto bad=bw.write_bit_string_fragmented_size(v,0,131072);R(!bad&&bad.error().code==ErrorCode::resource_limit&&bw.cursor_bit()==prefix&&bc.wire_bits()==prefix);
 Limits ol;ol.max_output_octets=(end+7)/8-1;EncodeContext oc(ol);BitWriter ow(oc);for(unsigned i=0;i<prefix;++i)R(ow.write_bit(true));bad=ow.write_bit_string_fragmented_size(v,0,131072);R(!bad&&bad.error().code==ErrorCode::resource_limit&&ow.cursor_bit()==prefix);
 Limits dl;dl.max_wire_bits=end-1;DecodeContext dbc(dl);auto dm=BitReader::make(m.b,dbc);R(dm);auto dr=std::move(dm).value();for(unsigned i=0;i<prefix;++i)R(dr.read_bit());auto dbad=dr.read_bit_string_owned_fragmented_size(0,131072);R(!dbad&&dbad.error().code==ErrorCode::resource_limit&&dr.cursor_bit()==prefix&&dbc.wire_bits()==prefix);
 Limits il;il.max_input_octets=m.b.size()-1;DecodeContext ic(il);auto im=BitReader::make(m.b,ic);R(!im&&im.error().code==ErrorCode::resource_limit);
 ++checks;
}
for(unsigned marker:{192u,197u,255u}){std::array<std::byte,1> b{static_cast<std::byte>(marker)};DecodeContext c;auto made=BitReader::make(b,c);R(made);auto r=std::move(made).value();auto bad=r.read_bit_string_owned_fragmented_size(0,131072);R(!bad&&bad.error().code==ErrorCode::constraint_violation&&r.cursor_bit()==0);}
{auto v=data(16384);auto m=model(v,0);m.b.pop_back();DecodeContext c;auto made=BitReader::make(m.b,c);auto r=std::move(made).value();auto bad=r.read_bit_string_owned_fragmented_size(0,131072);R(!bad&&bad.error().code==ErrorCode::truncated_input&&r.cursor_bit()==0);}
{std::array<std::byte,2>b{std::byte{128},std::byte{0}};DecodeContext c;auto made=BitReader::make(b,c);auto r=std::move(made).value();auto bad=r.read_bit_string_owned_fragmented_size(0,131072);R(!bad&&bad.error().code==ErrorCode::constraint_violation);}
{auto v=data(65536);EncodeContext c;BitWriter w(c);R(w.write_bit(true));fail_next=true;auto bad=w.write_bit_string_fragmented_size(v,1,131072);R(!bad&&bad.error().code==ErrorCode::allocation_failure&&w.cursor_bit()==1&&c.wire_bits()==1);auto m=model(v,0);DecodeContext dc;auto made=BitReader::make(m.b,dc);auto r=std::move(made).value();fail_next=true;auto rd=r.read_bit_string_owned_fragmented_size(1,131072);R(!rd&&rd.error().code==ErrorCode::allocation_failure&&r.cursor_bit()==0&&dc.wire_bits()==0);}
{EncodeContext c;BitWriter w(c);auto zero=data(0);auto bad=w.write_bit_string_fragmented_size(zero,1,131072);R(!bad&&bad.error().code==ErrorCode::constraint_violation);EncodeContext old;BitWriter ow(old);bad=ow.write_bit_string(data(1),1,131072);R(!bad&&bad.error().code==ErrorCode::invalid_argument);}
{auto v=data(32768);std::vector<std::byte> b{std::byte{193}};b.insert(b.end(),v.octets.begin(),v.octets.begin()+2048);b.push_back(std::byte{193});b.insert(b.end(),v.octets.begin()+2048,v.octets.end());b.push_back(std::byte{0});DecodeContext c;auto made=BitReader::make(b,c);auto r=std::move(made).value();auto bad=r.read_bit_string_owned_fragmented_size(0,131072);R(!bad&&bad.error().code==ErrorCode::constraint_violation&&r.cursor_bit()==0&&c.wire_bits()==0);}
{auto v=data(128);auto m=model(v,3);m.b[0]|=std::byte{0x08};DecodeContext c;auto made=BitReader::make(m.b,c);auto r=std::move(made).value();for(unsigned i=0;i<3;++i)R(r.read_bit());auto bad=r.read_bit_string_owned_fragmented_size(1,131072);R(!bad&&bad.error().code==ErrorCode::nonzero_padding&&bad.error().bit_offset==4&&r.cursor_bit()==3&&c.wire_bits()==3);}
std::printf("PASS %zu bounded fragmented BIT model vectors, atomic/budget/sticky/OOM rejection\n",checks);
}
