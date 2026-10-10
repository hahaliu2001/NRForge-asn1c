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
    char command; std::size_t lower,upper,prefix; unsigned unconstrained; std::string text;
    while(std::cin>>command>>lower>>upper>>unconstrained>>prefix>>text) {
        auto bytes=unhex(text);
        if(command=='E') {
            auto r=encode_complete(bytes,{},[&](FieldWriter& f) {
                for(std::size_t i=0;i<prefix;++i) {auto b=f.write_bit(i%2==0);if(!b)return b;}
                return f.write_octet_string(bytes,lower,upper,unconstrained!=0);
            });
            if(r) std::cout<<hex(r.value().octets)<<'\n';
            else std::cout<<"ERR:"<<static_cast<unsigned>(r.error().code)<<":"<<r.error().bit_offset<<'\n';
        } else {
            auto r=decode_complete<std::vector<std::byte>>(bytes,{},[&](FieldReader& f) {
                for(std::size_t i=0;i<prefix;++i) {
                    auto b=f.read_bit();if(!b)return Result<std::vector<std::byte>>::failure(b.error());
                    if(b.value()!=(i%2==0))return Result<std::vector<std::byte>>::failure({ErrorCode::invalid_argument,i});
                }
                return f.read_octet_string_owned(lower,upper,unconstrained!=0);
            });
            if(r)std::cout<<hex(r.value())<<'\n';
            else std::cout<<"ERR:"<<static_cast<unsigned>(r.error().code)<<":"<<r.error().bit_offset<<'\n';
        }
    }
}
