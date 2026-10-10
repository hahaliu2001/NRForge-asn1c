#include <runtime.hpp>
#include "main_types.hpp"
#include "main_mapping.hpp"
#include "main_codec.hpp"
#include "main_adapters.hpp"
#include "ext_types.hpp"
#include "ext_mapping.hpp"
#include "ext_codec.hpp"
#include "ext_adapters.hpp"
#include <cstdio>
#include <cstdlib>
#define REQUIRE(x) do { if(!(x)) { std::fprintf(stderr,"%s:%d: %s\n",__FILE__,__LINE__,#x); std::abort(); } } while(0)
using Bytes = std::vector<std::byte>;
using Map = octets_main::EntryMapping;
using Policy = Map::criticality_type;
static Bytes bytes(std::initializer_list<unsigned> ns) {
    Bytes out; for(auto n : ns) out.push_back(static_cast<std::byte>(n)); return out;
}
template<class R> static void wire(const R& r, const Bytes& expected) {
    REQUIRE(r && r.value().octets == expected);
    REQUIRE(r.value().complete_encoding_bits == expected.size()*8);
}
template<class W> static octets_main::Entry entry(std::uint64_t id, typename Policy::Known crit, Bytes payload) {
    octets_main::Entry out{}; out.number=id; out.policy.value=crit; out.content=W{std::move(payload)}; return out;
}
int main() {
    static_assert(Map::rows[0].id==91 && Map::rows[1].id==7 && Map::rows[2].id==42);
    const auto contents=entry<Map::wrapper_0>(91,Policy::Known::reject,bytes({0x80,0xff}));
    const auto plain=entry<Map::wrapper_1>(7,Policy::Known::ignore,{});
    const auto named=entry<Map::wrapper_2>(42,Policy::Known::notify,bytes({1,2,3}));
    // Open length3 frames an independent inner OCTET length2 + two bytes.
    wire(octets_main::encode_entry(contents),bytes({0,91,0,3,2,0x80,0xff}));
    wire(octets_main::encode_entry(plain),bytes({0,7,0x40,1,0}));
    // Fixed SIZE3 has no inner length determinant.
    wire(octets_main::encode_entry(named),bytes({0,42,0x80,3,1,2,3}));
    octets_main::Message body{}; body.entries.elements={contents,plain,named};
    auto expected=bytes({0,0,3,0,91,0,3,2,0x80,0xff,0,7,0x40,1,0,0,42,0x80,3,1,2,3});
    wire(octets_main::encode_message(body),expected);
    auto decoded=octets_main::decode_message(expected); REQUIRE(decoded);
    expected.assign(expected.size(),std::byte{0});
    REQUIRE(std::get<Map::wrapper_0>(decoded.value().entries.elements[0].content).value==bytes({0x80,0xff}));
    REQUIRE(std::get<Map::wrapper_1>(decoded.value().entries.elements[1].content).value.empty());
    REQUIRE(std::get<Map::wrapper_2>(decoded.value().entries.elements[2].content).value==bytes({1,2,3}));
    auto bad=named; std::get<Map::wrapper_2>(bad.content).value.pop_back();
    REQUIRE(!octets_main::encode_entry(bad));
    auto large=plain; std::get<Map::wrapper_1>(large.content).value.resize(16384);
    REQUIRE(!octets_main::encode_entry(large));
    auto malformed=bytes({0,91,0,3,3,0x80,0xff});
    REQUIRE(!octets_main::decode_entry(malformed)); // known inner length cannot fall back to opaque
    ::nrforge::aper::Limits limits{}; limits.max_known_open_staging_octets=2;
    REQUIRE(!octets_main::encode_entry(contents,limits));
    REQUIRE(!octets_main::decode_entry(bytes({0,91,0,3,2,0x80,0xff}),limits));
    limits.max_known_open_staging_octets=3;
    wire(octets_main::encode_entry(contents,limits),bytes({0,91,0,3,2,0x80,0xff}));
    REQUIRE(octets_main::decode_entry(bytes({0,91,0,3,2,0x80,0xff}),limits));
    using EMap=octets_ext::EntryMapping;
    octets_ext::Entry ext{}; ext.tag=91; ext.policy.value=EMap::criticality_type::Known::ignore;
    ext.extension_content=EMap::wrapper_0{bytes({0x12})};
    wire(octets_ext::encode_entry(ext),bytes({0,91,0x40,2,1,0x12}));
    octets_ext::Message extbody{}; extbody.extensions.elements={ext};
    wire(octets_ext::encode_message(extbody),bytes({0,0,0,91,0x40,2,1,0x12}));
    REQUIRE(octets_ext::decode_entry(bytes({0,91,0x40,2,1,0x12})));
    std::puts("generated IOC OCTET vectors PASS");
}
