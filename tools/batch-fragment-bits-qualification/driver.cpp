#include "runtime.hpp"
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
using namespace nrforge::aper;
static std::vector<std::byte> unhex(const std::string& s) {
    std::vector<std::byte> v;
    if(s != "-") for(std::size_t i=0;i<s.size();i+=2)
        v.push_back(static_cast<std::byte>(std::stoul(s.substr(i,2),nullptr,16)));
    return v;
}
static std::string hex(const std::vector<std::byte>& v) {
    std::ostringstream s;
    for(auto b:v) s<<std::hex<<std::setw(2)<<std::setfill('0')<<std::to_integer<unsigned>(b);
    return v.empty()?"-":s.str();
}
int main() {
    char command; std::size_t lower,upper,prefix; unsigned unconstrained; std::size_t count; std::string text;
    while(std::cin>>command>>lower>>upper>>unconstrained>>prefix>>count>>text) {
        auto bytes=unhex(text);
        BitString value{bytes,count};
        if(command=='E') {
            auto r=encode_complete(value,{},[&](FieldWriter& f) {
                for(std::size_t i=0;i<prefix;++i) {auto b=f.write_bit(i%2==0);if(!b)return b;}
                return f.write_bit_string_fragmented_size(value,lower,upper);
            });
            if(r) std::cout<<hex(r.value().octets)<<'\n';
            else std::cout<<"ERR:"<<static_cast<unsigned>(r.error().code)<<":"<<r.error().bit_offset<<'\n';
        } else {
            auto r=decode_complete<BitString>(bytes,{},[&](FieldReader& f) {
                for(std::size_t i=0;i<prefix;++i) {
                    auto b=f.read_bit();if(!b)return Result<BitString>::failure(b.error());
                    if(b.value()!=(i%2==0))return Result<BitString>::failure({ErrorCode::invalid_argument,i});
                }
                return f.read_bit_string_owned_fragmented_size(lower,upper);
            });
            if(r)std::cout<<std::to_string(r.value().bit_count)+":"+hex(r.value().octets)<<'\n';
            else std::cout<<"ERR:"<<static_cast<unsigned>(r.error().code)<<":"<<r.error().bit_offset<<'\n';
        }
    }
}
