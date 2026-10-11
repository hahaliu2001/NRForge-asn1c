#!/usr/bin/env python3
"""Prepare public-only all-slot functional consumer; no new wire claim."""
import argparse
import json
from pathlib import Path

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--generated', type=Path, required=True)
    ap.add_argument('--output', type=Path, required=True)
    args = ap.parse_args()
    messages = json.loads((args.generated / 'manifest.json').read_text())['messages']
    work = args.output
    work.mkdir(parents=True, exist_ok=False)
    for i, m in enumerate(messages):
        source = work / f'{i:03d}.cpp'
        # The private container has SIZE(1..65535), unlike ordinary IE
        # containers. Its empty extensible set permits vendor-opaque raw data;
        # this tests transparency only, not a known vendor payload.
        initialization = 'Body body{};'
        if m['message'] == 'PrivateMessage':
            initialization += ' body.private_i_es.elements.emplace_back(); body.private_i_es.elements.back().value.push_back(std::byte{0});'
        source.write_text(f'''#include <nrforge/f1ap/f1ap.hpp>
#include <nrforge/f1ap/{m['public_header']}>
#include <cstdio>
int check_{i}() {{
using namespace nrforge::f1ap;
using Body = {m['namespace']}::Body;
{initialization}
auto made = make_f1ap_pdu(std::move(body));
if(!made || !made.value().body_if<Body>()) return 1;
auto encoded = encode_f1ap_pdu(made.value());
if(!encoded) return 2;
auto decoded = decode_f1ap_pdu(encoded.value().octets);
if(!decoded || !decoded.value().body_if<Body>()) return 3;
const auto* h = decoded.value().root_header();
const auto* info = decoded.value().message_info();
if(!h || !info || static_cast<unsigned>(h->role) != {m['role']} || h->procedure_code != {m['code']} || static_cast<unsigned>(h->received_criticality) != {m['criticality']}) return 4;
if(info->module != "{m['module']}" || info->message != "{m['message']}") return 5;
auto again = encode_f1ap_pdu(decoded.value());
if(!again || again.value().octets != encoded.value().octets) return 6;
auto changed = decoded.value().set_criticality(Criticality::notify);
if(!changed) return 7;
auto altered = encode_f1ap_pdu(decoded.value());
if(!altered) return 8;
auto received = decode_f1ap_pdu(altered.value().octets);
if(!received || received.value().root_header()->received_criticality != Criticality::notify) return 9;
auto short_input = encoded.value().octets; short_input.pop_back();
if(decode_f1ap_pdu(short_input)) return 10;
auto trailing = encoded.value().octets; trailing.push_back(std::byte{{0}});
if(decode_f1ap_pdu(trailing)) return 11;
return 0;
}}
''')
    declarations = ''.join(f'int check_{i}();\n' for i in range(len(messages)))
    calls = ''.join(f'if(check_{i}()) return {i+1};\n' for i in range(len(messages)))
    (work/'main.cpp').write_text('#include <cstdio>\n' + declarations + 'int main() {\n' + calls + 'puts("PASS all 158 installed slots");\n}\n')
    (work/'CMakeLists.txt').write_text('cmake_minimum_required(VERSION 3.20)\nproject(F1APAllSlots LANGUAGES CXX)\nfind_package(NRForgeF1AP 0.1.0 EXACT CONFIG REQUIRED)\nfile(GLOB sources "*.cpp")\nadd_executable(all_slots ${sources})\ntarget_link_libraries(all_slots PRIVATE NRForge::f1ap)\ntarget_compile_options(all_slots PRIVATE -Wall -Wextra -Werror -pedantic-errors -Wconversion -Wsign-conversion)\nset(NRFORGE_CONSUMER_LINKER gold CACHE STRING "Consumer linker")\ntarget_link_options(all_slots PRIVATE -fuse-ld=${NRFORGE_CONSUMER_LINKER} -Wl,-s)\n')

if __name__ == '__main__':
    main()
