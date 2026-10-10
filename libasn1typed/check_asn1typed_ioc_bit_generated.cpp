#include <runtime.hpp>
#include "main_types.hpp"
#include "main_mapping.hpp"
#include "main_codec.hpp"
#include "main_adapters.hpp"
#include <cstdio>
#include <cstdlib>
#define REQUIRE(x) do { if(!(x)) { std::fprintf(stderr,"%s:%d: %s\n",__FILE__,__LINE__,#x); std::abort(); } } while(0)
using Bytes=std::vector<std::byte>;
using Map=bit_ioc::EntryMapping;
using Bits=::nrforge::aper::BitString;
static Bytes bytes(std::initializer_list<unsigned>ns){Bytes out;for(auto n:ns)out.push_back(static_cast<std::byte>(n));return out;}
template<class R>static void wire(const R&r,const Bytes&b){REQUIRE(r&&r.value().octets==b&&r.value().complete_encoding_bits==b.size()*8);}
int main(){
 static_assert(Map::rows[0].id==7&&Map::rows[1].id==42);
 bit_ioc::Entry plain{},named{};
 plain.number=7;plain.policy.value=Map::criticality_type::Known::reject;plain.content=Map::wrapper_0{Bits{bytes({0xa0}),3}};
 named.number=42;named.policy.value=Map::criticality_type::Known::notify;named.content=Map::wrapper_1{Bits{bytes({0xa0}),3}};
 // Unconstrained BIT child: bit-length determinant3 + three bits + pad5.
 wire(bit_ioc::encode_entry(plain),bytes({0,7,0,2,3,0xa0}));
 // Fixed SIZE3 child: three bits + pad5, with no inner determinant.
 wire(bit_ioc::encode_entry(named),bytes({0,42,0x80,1,0xa0}));
 bit_ioc::Message m{};m.entries.elements={plain,named};
 auto b=bytes({0,0,2,0,7,0,2,3,0xa0,0,42,0x80,1,0xa0});wire(bit_ioc::encode_message(m),b);
 auto d=bit_ioc::decode_message(b);REQUIRE(d);b.assign(b.size(),std::byte{0});
 const auto&v=std::get<Map::wrapper_0>(d.value().entries.elements[0].content).value;
 REQUIRE(v.bit_count==3&&v.octets==bytes({0xa0}));
 auto copy=d.value();std::get<Map::wrapper_0>(copy.entries.elements[0].content).value.octets[0]=std::byte{0};REQUIRE(v.octets==bytes({0xa0}));
 auto bad=named;std::get<Map::wrapper_1>(bad.content).value.bit_count=2;REQUIRE(!bit_ioc::encode_entry(bad));
 auto pad=bytes({0,7,0,2,3,0xa1});REQUIRE(!bit_ioc::decode_entry(pad));
 ::nrforge::aper::Limits limits{};limits.max_known_open_staging_octets=1;REQUIRE(!bit_ioc::encode_entry(plain,limits));REQUIRE(!bit_ioc::decode_entry(bytes({0,7,0,2,3,0xa0}),limits));
 limits.max_known_open_staging_octets=2;REQUIRE(bit_ioc::encode_entry(plain,limits));REQUIRE(bit_ioc::decode_entry(bytes({0,7,0,2,3,0xa0}),limits));
 std::puts("IOC anonymous/named BIT vectors PASS");
}
