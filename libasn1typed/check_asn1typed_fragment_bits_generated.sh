#!/bin/sh
set -eu
src=${srcdir:-.}
work=$(mktemp -d "${TMPDIR:-/tmp}/asn1typed-fragbits.XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM
./check_asn1typed_fragment_bits_render "$src/fixtures/fragment-bits-generation-batch.asn1" "$work"
cat >"$work/main.cpp" <<'CPP'
#include <runtime.hpp>
#include "types.hpp"
#include "mapping.hpp"
#include "codec.hpp"
#include <cstdio>
#include <cstdlib>
#include <array>
#define R(x) do{if(!(x)){std::fprintf(stderr,"generatedfragbits:%d %s\n",__LINE__,#x);std::abort();}}while(0)
using namespace sizetest;
int main(){
 static_assert(Wide_aper::fragmented_supported&&Wide_aper::length_units_are_bits);
 static_assert(Envelope_member_1_size_aper::fragmented_supported&&Envelope_member_2_size_aper::fragmented_supported);
 for(auto n:std::array<std::size_t,9>{1,127,128,16383,16384,65535,65536,131071,131072}){
 Wide v{{},n};v.octets.resize(n/8+(n%8!=0),std::byte{0xA5});if(n%8)v.octets.back()&=static_cast<std::byte>(0xffu<<(8-n%8));
 auto e=encode_wide(v);R(e);std::vector<std::byte> expected;std::size_t consumed=0;while(n-consumed>=16384){auto chunks=(n-consumed)/16384;if(chunks>4)chunks=4;expected.push_back(static_cast<std::byte>(192+chunks));expected.insert(expected.end(),v.octets.begin()+static_cast<std::ptrdiff_t>(consumed/8),v.octets.begin()+static_cast<std::ptrdiff_t>((consumed+chunks*16384)/8));consumed+=chunks*16384;}auto tail=n-consumed;if(tail>=128)expected.push_back(static_cast<std::byte>(128+(tail>>8)));expected.push_back(static_cast<std::byte>(tail&255));expected.insert(expected.end(),v.octets.begin()+static_cast<std::ptrdiff_t>(consumed/8),v.octets.end());R(e.value().octets==expected);
 auto d=decode_wide(expected);R(d&&d.value().bit_count==n&&d.value().octets==v.octets);
 Envelope env;env.named=v;env.inline_bits=v;env.use_site=v;auto all=encode_envelope(env);R(all);auto back=decode_envelope(all.value().octets);R(back&&back.value().named.octets==v.octets&&back.value().inline_bits.octets==v.octets&&back.value().use_site.octets==v.octets);
 }
 Wide zero{{},0};auto bad=encode_wide(zero);R(!bad&&bad.error().code==nrforge::aper::ErrorCode::constraint_violation);
 std::puts("PASS generated named/inline/named-use-site fragmented BIT wire values");
}
CPP
${CXX:-c++} -std=c++20 -Wall -Wextra -Werror -pedantic-errors -Wconversion -Wsign-conversion -DNDEBUG -I"$src/../libaper" -I"$work" "$work/main.cpp" "$src/../libaper/runtime.cpp" -o "$work/check"
chmod +x "$work/check"
"$work/check"
