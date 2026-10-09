#include "runtime.hpp"
#include "types.hpp"
#include "mapping.hpp"
#include "codec.hpp"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <string>
#include <vector>
using namespace extensiontest;
static std::string hex(const std::vector<std::byte>& b){std::ostringstream s; for(auto x:b)s<<std::hex<<std::setw(2)<<std::setfill('0')<<std::to_integer<unsigned>(x);return s.str();}
template<class T,class M> static std::string ext(const T& v){auto &e=v.*M::extension_data_member;std::ostringstream s;s<<e.received_bitmap_bit_count;for(auto &r:e.unknown_additions)s<<","<<r.addition_index<<"="<<hex(r.payload_octets);return s.str();}
static int flag(const std::optional<bool>& v){return v?(*v?1:0):-1;}
static std::string show(const Inner& v){return "I:"+std::to_string(v.value)+":"+std::to_string(flag(v.flag))+":"+ext<Inner,Inner_aper>(v);}
static std::string show(const Outer& v){return show(v.inner)+"|O:"+std::to_string(flag(v.flag))+":"+ext<Outer,Outer_aper>(v);}
static std::string show(const Empty& v){return "X:"+ext<Empty,Empty_aper>(v);}
static std::string show(const Collision& v){return "C:"+std::to_string(v.sequence_extensions)+":"+std::to_string(flag(v.sequence_extensions_1))+":"+ext<Collision,Collision_aper>(v);}
static std::string show(const Envelope& v){std::string s;if(v.pick.index()==0)s="P:"+std::to_string(std::get<Pick_flag>(v.pick).value);else s=show(std::get<Pick_inner>(v.pick).value);return s+(v.outer?"|"+show(*v.outer):"|-");}
template<class F> static void dec(const std::vector<std::byte>& b,F f){auto r=f(b);if(!r){std::cout<<"ERR:"<<static_cast<unsigned>(r.error().code)<<":"<<r.error().bit_offset<<"\n";return;}std::cout<<show(r.value())<<"\n";}
template<class T,class F> static void enc(const T& v,F f){auto r=f(v);if(!r){std::cout<<"ERR\n";return;}std::cout<<hex(r.value().octets)<<"\n";}
int main(){std::string command,type,data;while(std::cin>>command>>type){if(command=="D"){std::cin>>data;std::vector<std::byte>b;for(size_t i=0;i<data.size();i+=2)b.push_back(static_cast<std::byte>(std::stoul(data.substr(i,2),nullptr,16)));if(type=="Inner")dec(b,[](auto x){return decode_inner(x);});else if(type=="Outer")dec(b,[](auto x){return decode_outer(x);});else if(type=="Empty")dec(b,[](auto x){return decode_empty(x);});else if(type=="Collision")dec(b,[](auto x){return decode_collision(x);});else if(type=="Envelope")dec(b,[](auto x){return decode_envelope(x);});else return 2;}else{std::uint64_t value;int a,z;std::cin>>value>>a>>z;Inner i{};i.value=value;if(a>=0)i.flag=a!=0;if(type=="Inner")enc(i,[](const auto& v){return encode_inner(v);});else if(type=="Outer"){Outer v{};v.inner=i;if(z>=0)v.flag=z!=0;enc(v,[](const auto& x){return encode_outer(x);});}else if(type=="Empty"){Empty v{};enc(v,[](const auto& x){return encode_empty(x);});}else if(type=="Collision"){Collision v{};v.sequence_extensions=a!=0;if(z>=0)v.sequence_extensions_1=z!=0;enc(v,[](const auto& x){return encode_collision(x);});}else return 3;}}
}
