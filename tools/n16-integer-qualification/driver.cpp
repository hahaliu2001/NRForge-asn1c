#include "runtime.hpp"
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
using namespace nrforge::aper;
static std::vector<std::byte> unhex(const std::string&s){std::vector<std::byte>v;if(s!="-")for(std::size_t i=0;i<s.size();i+=2)v.push_back(static_cast<std::byte>(std::stoul(s.substr(i,2),nullptr,16)));return v;}
static std::string hex(const std::vector<std::byte>&v){std::ostringstream s;for(auto b:v)s<<std::hex<<std::setw(2)<<std::setfill('0')<<std::to_integer<unsigned>(b);return s.str();}
template<class T> static void run(char command,T lo,T hi,std::size_t prefix,const std::string&text){
 if(command=='E'){
  T value;
  if constexpr(std::is_signed_v<T>)value=std::stoll(text);else value=std::stoull(text);
  auto r=encode_complete(value,{},[&](FieldWriter&f){for(std::size_t i=0;i<prefix;++i){auto b=f.write_bit(i%2==0);if(!b)return b;}if constexpr(std::is_signed_v<T>)return f.write_bounded_int(value,lo,hi);else return f.write_bounded_uint(value,lo,hi);});
  if(r)std::cout<<hex(r.value().octets)<<'\n';else std::cout<<"ERR:"<<static_cast<unsigned>(r.error().code)<<":"<<r.error().bit_offset<<'\n';
 }else{
  auto wire=unhex(text);auto r=decode_complete<T>(wire,{},[&](FieldReader&f){for(std::size_t i=0;i<prefix;++i){auto b=f.read_bit();if(!b)return Result<T>::failure(b.error());if(b.value()!=(i%2==0))return Result<T>::failure({ErrorCode::invalid_argument,i});}if constexpr(std::is_signed_v<T>)return f.read_bounded_int(lo,hi);else return f.read_bounded_uint(lo,hi);});
  if(r)std::cout<<r.value()<<'\n';else std::cout<<"ERR:"<<static_cast<unsigned>(r.error().code)<<":"<<r.error().bit_offset<<'\n';
 }
}
int main(){char command,kind;std::string lo,hi,text;std::size_t prefix;while(std::cin>>command>>kind>>lo>>hi>>prefix>>text){if(kind=='S')run<std::int64_t>(command,std::stoll(lo),std::stoll(hi),prefix,text);else run<std::uint64_t>(command,std::stoull(lo),std::stoull(hi),prefix,text);}}
