#!/usr/bin/env python3
"""Finite all-slot integration checks; not independent wire qualification."""
import argparse
from concurrent.futures import ThreadPoolExecutor
import hashlib
import json
from pathlib import Path
import subprocess


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--repo', type=Path, default=Path('.'))
    ap.add_argument('--generated', type=Path, required=True)
    ap.add_argument('--build', type=Path, required=True)
    ap.add_argument('--work', type=Path, required=True)
    ap.add_argument('--output', type=Path, required=True)
    ap.add_argument('--jobs', type=int, choices=range(1, 5), default=1)
    ap.add_argument('--linker', choices=['bfd', 'gold'], default='bfd')
    args = ap.parse_args()
    repo, generated, build = args.repo.resolve(), args.generated.resolve(), args.build.resolve()
    manifest = json.loads((generated / 'manifest.json').read_text())
    messages = manifest['messages']
    expected = {(role, p['code']) for p in manifest['schema']['procedures']
                for role, present in enumerate(p['payload_present']) if present}
    if (manifest['message_count'] != 158 or len(messages) != 158 or len(expected) != 158
            or len(manifest['schema']['procedures']) != 94 or {(m['role'], m['code']) for m in messages} != expected
            or [sum(m['role'] == role for m in messages) for role in range(3)] != [94, 36, 28]):
        raise ValueError('full frozen registry closure absent')
    if (manifest['schema']['pdu_type'] != 'F1AP-PDU' or not manifest['parser_deleted']
            or manifest['schema'].get('root_count') != 4 or manifest['schema'].get('choice_is_extensible') is not False
            or manifest['schema'].get('unsupported_root_policy') != 'reject_before_header'):
        raise ValueError('wrong profile or borrowed parser state')
    work = args.work.resolve()
    work.mkdir(parents=True, exist_ok=False)
    inputs = [*sorted(p for p in generated.rglob('*') if p.is_file()),
              *sorted((repo / 'libngap').glob('*.hpp')), *sorted((repo / 'libngap').glob('*.inc')),
              repo / 'libngap/f1ap_pdu.cpp', repo / 'libaper/runtime.cpp',
              *sorted((repo / 'libaper').glob('*.hpp')), build / 'libnrforge_f1ap.a', Path(__file__).resolve()]
    hashes = {str(p): hashlib.sha256(p.read_bytes()).hexdigest() for p in inputs}
    cases = []
    commands = []
    for i, m in enumerate(messages):
        source = work / f'{i:03d}.cpp'
        # The private container has SIZE(1..65535), unlike ordinary IE
        # containers. Its empty extensible set permits vendor-opaque raw data;
        # this tests transparency only, not a known vendor payload.
        initialization = 'Body body{};'
        if m['message'] == 'PrivateMessage':
            initialization += ' body.private_i_es.elements.emplace_back(); body.private_i_es.elements.back().value.push_back(std::byte{0});'
        source.write_text(f'''#include "f1ap.hpp"
#include "{m['public_header']}"
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
        obj = work / f'{i:03d}.o'
        command = ['g++', '-std=c++20', '-Wall', '-Wextra', '-Werror', '-pedantic-errors', '-Wconversion', '-Wsign-conversion', '-DNDEBUG',
                   '-I' + str(generated), '-I' + str(repo / 'libngap'), '-I' + str(repo / 'libaper'), '-c', str(source), '-o', str(obj)]
        commands.append((i, m['message'], command))
        cases.append({'message': m['message'], 'role': m['role'], 'code': m['code'], 'status': 'PASS',
                      'value_profile': 'one vendor-opaque local:0 raw00 entry' if m['message'] == 'PrivateMessage' else 'default empty-container BODY'})
    def compile_case(item):
        i, name, command = item
        with (work / f'{i:03d}-compile.log').open('w') as log:
            subprocess.run(command, stdout=log, stderr=subprocess.STDOUT, check=True)
        with (work / f'{i:03d}.o').open('rb') as compiled:
            if compiled.read(4) != b'\x7fELF':
                raise ValueError('missing ELF object after compiler success: ' + name)
        print('compile integration', i + 1, '/158', name, flush=True)
    with ThreadPoolExecutor(max_workers=args.jobs) as pool:
        list(pool.map(compile_case, commands))
    main_cpp = work / 'main.cpp'
    main_cpp.write_text('#include "f1ap.hpp"\n#include <cstdio>\n' +
                        ''.join(f'int check_{i}();\n' for i in range(158)) +
                        'int main() { const auto& registry = nrforge::f1ap::f1ap_registry_state(); if(!registry || registry.value().message_count() != 158) return 100;\n' +
                        ''.join(f'if(int result = check_{i}()) {{ std::fprintf(stderr,"case {i}: %d\\n",result); return result; }}\n' for i in range(158)) +
                        'std::puts("PASS all158 typed slots: 157 empty-container BODY + one private opaque entry, criticality, truncation/trailing rejection"); return 0; }\n')
    executable = work / 'check_all'
    link_options = ['-fuse-ld=' + args.linker, '-Wl,--no-keep-memory']
    subprocess.run(['g++', '-std=c++20', *link_options, '-I' + str(generated), '-I' + str(repo / 'libngap'), '-I' + str(repo / 'libaper'),
                    str(main_cpp), *[str(work / f'{i:03d}.o') for i in range(158)], str(build / 'libnrforge_f1ap.a'), '-o', str(executable)], check=True)
    result = subprocess.run([str(executable)], capture_output=True, text=True, check=True)
    print(result.stdout, end='')
    for p in inputs:
        if hashlib.sha256(p.read_bytes()).hexdigest() != hashes[str(p)]:
            raise ValueError('integration input changed: ' + str(p))
    args.output.write_text(json.dumps({'scope': 'finite generated all-slot integration; NOT wire qualification',
                                      'message_count': 158, 'cases': cases, 'input_sha256': hashes,
                                      'compiler': subprocess.check_output(['g++', '--version'], text=True).splitlines()[0],
                                      'compile_jobs': args.jobs,
                                      'link_options': link_options,
                                      'strict_compile_options': ['-std=c++20', '-Wall', '-Wextra', '-Werror', '-pedantic-errors', '-Wconversion', '-Wsign-conversion', '-DNDEBUG'],
                                      'executable_sha256': hashlib.sha256(executable.read_bytes()).hexdigest(),
                                      'runtime_result': result.stdout,
                                      'limitations': ['157 default empty-container BODY values plus one vendor-opaque local:0 raw00 PrivateMessage entry; no required-IE procedure policy assertion.',
                                                      'Same implementation roundtrip; no independent reference, populated payload coverage or interoperability claim.']}, indent=2) + '\n')


if __name__ == '__main__':
    main()
