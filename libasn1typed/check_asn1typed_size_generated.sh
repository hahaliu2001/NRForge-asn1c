#!/bin/sh
set -eu
src=${srcdir:-.}
work=$(mktemp -d "${TMPDIR:-/tmp}/asn1typed-size.XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM
./check_asn1typed_size_render "$src/fixtures/size-generation-batch.asn1" "$work"
cat >"$work/main.cpp" <<'CPP'
#include <runtime.hpp>
#include "types.hpp"
#include "mapping.hpp"
#include "codec.hpp"
#include <cstdio>
#include <cstdlib>
#define R(x) do {if(!(x)){std::fprintf(stderr,"sizes:%d %s\n",__LINE__,#x);std::abort();}}while(0)
using namespace sizetest;
int main(){
 static_assert(Envelope_member_2_size_aper::has_extension_addition);
 static_assert(Envelope_member_2_size_aper::extension_lower_bound==16);
 static_assert(Envelope_member_2_size_aper::extension_upper_bound==16);
 Address address{{std::byte{0xAA},std::byte{0x00}},9};
 auto e=encode_address(address);R(e);R(e.value().octets==std::vector<std::byte>({std::byte{0x04},std::byte{0x00},std::byte{0xAA},std::byte{0x00}}));
 auto d=decode_address(e.value().octets);R(d&&d.value().bit_count==9&&d.value().octets==address.octets);
 Algorithms ext{{std::byte{0x55},std::byte{0x55},std::byte{0}},17};auto ee=encode_algorithms(ext);R(ee);R(ee.value().octets==std::vector<std::byte>({std::byte{0x80},std::byte{0x11},std::byte{0x55},std::byte{0x55},std::byte{0}}));auto dd=decode_algorithms(ee.value().octets);R(dd&&dd.value().bit_count==17&&dd.value().octets==ext.octets);
 Envelope envelope;envelope.address=address;envelope.algorithms=Algorithms{{std::byte{0xAB},std::byte{0xCD}},16};envelope.primary={{std::byte{0x34},std::byte{0x12}},16};envelope.fixed={{std::byte{0xDE},std::byte{0xAD},std::byte{0xBE},std::byte{0xEF}},32};envelope.bytes={std::byte{1},std::byte{2}};
 auto all=encode_envelope(envelope);R(all);auto back=decode_envelope(all.value().octets);R(back&&back.value().address.octets==envelope.address.octets&&back.value().primary.bit_count==16&&back.value().primary.octets==envelope.primary.octets&&back.value().algorithms.octets==envelope.algorithms.octets&&back.value().fixed.octets==envelope.fixed.octets&&back.value().bytes==envelope.bytes);
 Endpoint endpoint=Endpoint_t_ngf_id{{{std::byte{0x12},std::byte{0x34}},16}};
 auto choice=encode_endpoint(endpoint);R(choice);auto readchoice=decode_endpoint(choice.value().octets);R(readchoice&&readchoice.value().index()==0&&std::get<Endpoint_t_ngf_id>(readchoice.value()).value.bit_count==16);
 std::puts("PASS generated effective/root/extension SIZE values");
}
CPP
${CXX:-c++} -std=c++20 -Wall -Wextra -Werror -pedantic-errors -Wconversion -Wsign-conversion -DNDEBUG -I"$src/../libaper" -I"$work" "$work/main.cpp" "$src/../libaper/runtime.cpp" -o "$work/check"
"$work/check"
