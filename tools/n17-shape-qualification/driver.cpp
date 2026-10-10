#include "runtime.hpp"
#include "types.hpp"
#include "mapping.hpp"
#include "codec.hpp"
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
using namespace shaperef;
static std::vector<std::byte> unhex(const std::string&s){std::vector<std::byte>v;for(std::size_t i=0;i<s.size();i+=2)v.push_back(static_cast<std::byte>(std::stoul(s.substr(i,2),nullptr,16)));return v;}
static std::string hex(const std::vector<std::byte>&v){std::ostringstream s;for(auto b:v)s<<std::hex<<std::setw(2)<<std::setfill('0')<<std::to_integer<unsigned>(b);return s.str();}
static Panel make(int phase,int status,int copy,int klass,unsigned amount){Panel p{};p.marker=true;p.phase.value=static_cast<decltype(p.phase)::Known>(phase);if(status>=0){p.status.emplace();p.status->value=static_cast<typename decltype(p.status)::value_type::Known>(status);}if(copy!=-99){p.copy.emplace();p.copy->value=static_cast<typename decltype(p.copy)::value_type::Known>(copy);}p.class_.value=static_cast<decltype(p.class_)::Known>(klass);p.amount=amount;return p;}
static std::string show(const Panel&p){using S=typename decltype(p.status)::value_type;return std::to_string(p.marker)+":"+std::to_string(static_cast<int>(p.phase.value))+":"+(p.status?std::to_string(static_cast<int>(std::get<typename S::Known>(p.status->value))):"-")+":"+(p.copy?std::to_string(static_cast<int>(p.copy->value)):"-")+":"+std::to_string(static_cast<int>(p.class_.value))+":"+std::to_string(p.amount);}
int main(){char cmd,kind;int phase,status,copy,klass;unsigned amount;std::string wire;while(std::cin>>cmd>>kind>>phase>>status>>copy>>klass>>amount>>wire){
 auto p=make(phase,status,copy,klass,amount);
 if(kind=='P'){
  if(cmd=='E'){auto r=encode_panel(p);if(r)std::cout<<hex(r.value().octets)<<'\n';else std::cout<<"ERR\n";}
  else{auto r=decode_panel(unhex(wire));if(r)std::cout<<show(r.value())<<'\n';else std::cout<<"ERR\n";}
 }else{
  Envelope e{};e.lead=true;e.branch=Branch_panel{p};e.panels.elements.push_back(p);
  if(cmd=='E'){auto r=encode_envelope(e);if(r)std::cout<<hex(r.value().octets)<<'\n';else std::cout<<"ERR\n";}
  else{auto r=decode_envelope(unhex(wire));if(r&&r.value().lead&&r.value().branch.index()==0&&r.value().panels.elements.size()==1)std::cout<<show(std::get<Branch_panel>(r.value().branch).value)+"|"+show(r.value().panels.elements[0])<<'\n';else std::cout<<"ERR\n";}
 }
}}
