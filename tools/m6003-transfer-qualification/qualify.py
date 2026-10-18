#!/usr/bin/env python3
"""Four standalone-root finite interoperability campaign, never self-roundtrip only."""
import argparse
from collections import Counter
from copy import deepcopy
import hashlib
import importlib.metadata
import importlib.util
import json
from pathlib import Path
import re
import sys

# -I deliberately removes the script directory. Only this immutable exported
# qualification directory is added, not checkout/site/user search paths.
sys.path.insert(0, str(Path(__file__).resolve().parent))
from cases import ROOTS, build_cases, ie, request, response
from policy import classify, native_error, ALLOWED
from support import command, sha, write, json_value


def load(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def require(test, message):
    if not test:
        raise RuntimeError(message)


def cpp_driver(bridge, desc, cases):
    """Independent object construction/assertions, not a PER implementation."""
    codec = bridge.codec
    entries = re.findall(r'inline .*?\b(encode_\w+)\(const ([\w:]+)& v', codec)
    roots = [(e, t) for e, t in entries if t.endswith('Transfer') and
             t.split('::')[-1] in bridge.structs]
    # Nested Transfers must not accidentally select another entry point.
    root = next((t for _, t in roots if t.split('::')[-1] == cases[0]['cpp_type']), None)
    require(root is not None, 'Generated root entry point not found')
    encode = next(e for e, t in roots if t == root)
    decode = 'decode_' + encode[7:]
    qenc = '::'+bridge.ns+'::'+encode
    qdec = '::'+bridge.ns+'::'+decode
    functions = []
    for index, case in enumerate(cases):
        bridge.reset()
        bridge.walk(desc, case['value'], root, 'expected', 'decoded.value()')
        wire = bridge.literal(case['wire'])
        body = [f'static void case_{index}() {{', root+' expected{};'] + bridge.lines + [
            'const auto bytes = '+wire+';', 'auto limits = profile_limits();',
            f'auto decoded = {qdec}(bytes, limits);']
        if len(case['wire']) > 4096:
            body += ['REQUIRE(!decoded && decoded.error().code == E::resource_limit);']
        else:
            body += ['REQUIRE(decoded);'] + bridge.checks
            # Check ownership after source bytes are destroyed using a fresh
            # mutable input, with every decoded field checked again.
            body += ['auto owned_bytes = bytes;', f'auto owned = {qdec}(owned_bytes, limits);',
                     'REQUIRE(owned);', 'owned_bytes.assign(owned_bytes.size(), std::byte{0xff});',
                     'owned_bytes.clear(); owned_bytes.shrink_to_fit();',
                     '{ auto decoded = std::move(owned);'] + bridge.checks + ['}']
        if case['encode']:
            body += [f'auto encoded = {qenc}(expected, limits);']
            if len(case['wire']) > 4096:
                body += ['REQUIRE(!encoded && encoded.error().code == E::resource_limit);']
            else:
                body += ['REQUIRE(encoded);', 'REQUIRE(encoded.value().octets == bytes);',
                         'auto tight = limits; tight.max_output_octets = bytes.size() - 1;',
                         f'auto no_output = {qenc}(expected, tight);',
                         'REQUIRE(!no_output && no_output.error().code == E::resource_limit);',
                         f'std::cout << "ENCODE {index} ";',
                         'print_hex(encoded.value().octets); std::cout << "\\n";']
        elif len(case['wire']) <= 4096:
            # Retained unknown values are decode-only in the existing codec.
            # Explicit refusal prevents this harness from laundering them.
            body += [f'auto encoded = {qenc}(expected, limits);',
                     'REQUIRE(!encoded && encoded.error().code == E::constraint_violation);']
        body += [f'std::cout << "CASE {index} PASS\\n";', '}']
        functions.extend(body)
    includes = r'''
#include "runtime.hpp"
#include "sequence_extensions.hpp"
#include "root_types.hpp"
#include "root_mapping.hpp"
#include "root_codec.hpp"
#include <cstdlib>
#include <iostream>
#include <string>
#include <algorithm>
#include <type_traits>
using E = ::nrforge::aper::ErrorCode;
#define REQUIRE(...) do { if(!(__VA_ARGS__)) { std::cerr << "assertion line " << __LINE__ << "\n"; std::exit(1); } } while(0)
static ::nrforge::aper::Limits profile_limits() {
    ::nrforge::aper::Limits x{};
    x.max_input_octets=4096; x.max_output_octets=4096;
    x.max_wire_bits=32768; x.max_collection_elements=65536;
    x.max_known_open_depth=16; x.max_known_open_staging_octets=1048576;
    x.max_retained_unknown_payload_octets=4096; x.max_retained_unknown_records=1024;
    x.max_extension_bitmap_bits=1024;
    return x;
}
static void print_hex(const std::vector<std::byte>& bytes) {
    for(auto b:bytes) { auto n=std::to_integer<unsigned>(b); std::cout << "0123456789abcdef"[n>>4] << "0123456789abcdef"[n&15]; }
}
static const char* error_name(E e) {
    switch(e) {
    case E::invalid_argument:return "invalid_argument";
    case E::constraint_violation:return "constraint_violation";
    case E::truncated_input:return "truncated_input";
    case E::nonzero_padding:return "nonzero_padding";
    case E::trailing_data:return "trailing_data";
    case E::resource_limit:return "resource_limit";
    case E::allocation_failure:return "allocation_failure";
    case E::invalid_state:return "invalid_state";
    }
    std::exit(2);
}
'''
    main = ['int main(int argc, char** argv) {', 'if(argc == 2) {',
        'std::string text; std::getline(std::cin, text);',
        'if(text.size()%2) return 2;', 'std::vector<std::byte> bytes;',
        'auto digit=[](char c)->unsigned { if(c>=\'0\' && c<=\'9\') return static_cast<unsigned>(c-\'0\'); if(c>=\'a\' && c<=\'f\') return static_cast<unsigned>(c-\'a\'+10); std::exit(2); };',
        'for(std::size_t i=0;i<text.size();i+=2) bytes.push_back(std::byte{static_cast<unsigned char>((digit(text[i])<<4)|digit(text[i+1]))});',
        'auto limits=profile_limits();',
        'if(std::string(argv[1])=="zero-collections") limits.max_collection_elements=0;',
        'else if(std::string(argv[1])=="zero-unknown") limits.max_retained_unknown_payload_octets=0;',
        'else if(std::string(argv[1])=="zero-records") limits.max_retained_unknown_records=0;',
        'else if(std::string(argv[1])=="zero-depth") limits.max_known_open_depth=0;',
        'else if(std::string(argv[1])=="zero-wire") limits.max_wire_bits=0;',
        'else if(std::string(argv[1])=="zero-bitmap") limits.max_extension_bitmap_bits=0;',
        'else if(std::string(argv[1])!="decode" && std::string(argv[1])!="inspect-additions" && std::string(argv[1])!="reencode") return 2;',
        f'auto result={qdec}(bytes, limits);',
        'if(result && std::string(argv[1])=="inspect-additions") {',
        'const auto& additions=result.value().sequence_extensions;',
        'std::cout << "additions:" << additions.received_bitmap_bit_count << ":" << additions.unknown_additions.size();',
        'for(const auto& addition:additions.unknown_additions) { std::cout << ":" << addition.addition_index << ":"; print_hex(addition.payload_octets); }',
        'std::cout << "\\n"; return 0; }',
        'if(result && std::string(argv[1])=="reencode") {',
        f'auto encoded={qenc}(result.value(), limits);',
        'if(encoded) { print_hex(encoded.value().octets); std::cout << "\\n"; }',
        'else std::cout << "error:" << error_name(encoded.error().code) << ":" << encoded.error().bit_offset << "\\n";',
        'return 0; }',
        'if(result) std::cout << "ok\\n";',
        'else std::cout << "error:" << error_name(result.error().code) << ":" << result.error().bit_offset << "\\n";',
        'return 0; }', 'if(argc != 1) return 2;']
    main += [f'case_{i}();' for i in range(len(cases))] + ['return 0;', '}']
    return '\n'.join([includes]+functions+main)+'\n'


def boundary_cases(native):
    """Find exact lengths with the oracle; do not hand-code length determinants."""
    result = []
    for root, typename in ROOTS.items():
        obj = getattr(native.NGAP_IEs, typename)
        for target in (4096, 4097):
            found = None
            for size in range(target-256, target+1):
                value = request() if root == 'setup_request' else (response() if root == 'setup_response'
                    else ({'cause': ('nas', 'normal-release')} if root == 'release_command' else {}))
                field = 'protocolIEs' if root == 'setup_request' else 'iE-Extensions'
                entry = {'id': 65535, 'criticality': 'ignore',
                         ('value' if root == 'setup_request' else 'extensionValue'): ('_unk_004', b'\x80'*size)}
                value.setdefault(field, []).append(entry)
                obj.set_val(value); wire = obj.to_aper()
                if len(wire) == target:
                    found = dict(root=root, name=f'input-boundary-{target}', value=value,
                                 policy='success' if target == 4096 else 'resource_limit',
                                 encode=False, category='input_boundary')
                    break
            require(found is not None, f'Cannot produce oracle input of {target} octets for {root}')
            result.append(found)
    obj = native.NGAP_IEs.PDUSessionResourceSetupRequestTransfer
    for target in (4096, 4097):
        found = None
        for size in range(target-256, target+1):
            value = request()
            value['protocolIEs'].append(ie(394, 'TLContainer', b'\x80'*size, 'ignore'))
            obj.set_val(value)
            if len(obj.to_aper()) == target:
                found = dict(root='setup_request', name=f'output-boundary-{target}', value=value,
                             policy='unsupported_profile' if target == 4096 else 'resource_limit',
                             encode=True, category='output_boundary')
                break
        require(found is not None, f'Cannot produce oracle output boundary {target}')
        result.append(found)
    return result


def schema_ledger(native):
    """Enumerate every reachable field, including non-R1 optionals and IOC rows."""
    visited = set(); result = []

    def visit(desc, path):
        # Same declaration may occur in many branches; references remain visible.
        row = {'path': path, 'kind': desc.TYPE, 'optional': bool(getattr(desc, '_opt', False))}
        if id(desc) in visited:
            row['shared_reference'] = True; result.append(row); return
        visited.add(id(desc)); result.append(row)
        if desc.TYPE in ('SEQUENCE', 'CHOICE'):
            for name, child in desc._cont.items():
                visit(child, path+'/'+name)
        elif desc.TYPE == 'SEQUENCE OF':
            visit(desc._cont, path+'/[]')
        elif desc.TYPE == 'OPEN_TYPE' and getattr(desc, '_const_tab', None) is not None:
            table = desc._const_tab._val
            row['registry'] = [{'id': r['id'], 'criticality': r['criticality']}
                               for r in list(table.root)+list(table.ext or [])]
            for r in list(table.root)+list(table.ext or []):
                visit(r[desc._const_tab_id], path+'/id='+str(r['id']))

    for root, name in ROOTS.items():
        visit(getattr(native.NGAP_IEs, name), root)
    return {'fields': result, 'allowed_r1_field_names': sorted(ALLOWED),
            'known_fields_outside_allowlist': 'unsupported_profile',
            'known_extension_values': 'unsupported_profile',
            'unknown_sequence_additions': 'retain index/payload; unsupported_profile (no safe-listed additions)',
            'unknown_ignore_ie_or_extension': 'structurally valid opaque bytes retained; ignored without semantic projection',
            'singletons': 'duplicates invalid_transfer; legal receive order retained',
            'events_emitted': False, 'implementation': 'qualification-only policy.py'}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--repo', type=Path, required=True)
    parser.add_argument('--asn1-root', type=Path, required=True)
    parser.add_argument('--probe', type=Path, required=True)
    parser.add_argument('--work', type=Path, required=True)
    parser.add_argument('--cxx', default='c++')
    args = parser.parse_args()
    source = Path(__file__).resolve().parent
    repo = args.repo.resolve(); work = args.work.resolve()
    work.mkdir(parents=True, exist_ok=False)
    report = {'status': 'RUNNING', 'checks': 0, 'roots': {}, 'limitations': [],
              'qualification_scope': 'finite native C++ standalone Transfer campaign; no Python binding or installed SDK qualification'}
    write(work/'report.json', report)
    try:
        pin = json.loads((source/'qualification-contract.json').read_text())
        require(pin['owner_policy_gates']['status'] == 'OWNER_APPROVED', 'Owner policy is not frozen')
        require(importlib.metadata.version('pycrate') == pin['oracle_recommendation']['version'], 'Oracle version mismatch')
        reference = load('m6003_reference', repo/'tools/n11-envelope-qualification/native_reference.py')
        bridge_module = load('m6003_bridge', repo/'tools/full-pdu-qualification/semantic_bridge.py')
        authority, texts = reference.verify_sources(repo, args.asn1_root.resolve(),
                                    repo/'tools/n11-envelope-qualification/source-manifest.json')
        inputs = {str(p.relative_to(repo)): sha(p) for p in list(source.glob('*')) if p.is_file()}
        for path in ('libaper/runtime.hpp', 'libaper/runtime.cpp', 'libaper/sequence_extensions.hpp',
                     'tools/full-pdu-qualification/semantic_bridge.py',
                     'tools/n11-envelope-qualification/native_reference.py'):
            inputs[path] = sha(repo/path)
        write(work/'inputs.json', {'sha256': inputs, 'schema': authority,
                                  'oracle': pin['oracle_recommendation'], 'limits': {
            'max_input_octets':4096, 'max_output_octets':4096, 'max_wire_bits':32768,
            'max_collection_elements':65536, 'max_known_open_depth':16,
            'max_known_open_staging_octets':1048576, 'max_retained_unknown_payload_octets':4096,
            'max_retained_unknown_records':1024, 'max_extension_bitmap_bits':1024}})
        print('Compiling oracle from six unchanged ASN.1 modules', flush=True)
        native = reference.compile_native(texts, work)
        write(work/'oracle-schema.json', {'sha256': sha(work/'native_oracle.py'),
                                         'modules': authority, 'bundled_ngap_schema_used': False})
        write(work/'receive-ledger.json', schema_ledger(native))
        cases = build_cases()+boundary_cases(native)
        # The oracle's values are never created by decoding native encoder output.
        for case in cases:
            desc = getattr(native.NGAP_IEs, ROOTS[case['root']])
            desc.set_val(deepcopy(case['value'])); case['wire'] = desc.to_aper()
            desc.from_aper(case['wire'])
            require(desc.get_val() == case['value'], 'Oracle selfdecode changes semantics: '+case['name'])
            actual = 'resource_limit' if len(case['wire']) > 4096 else classify(case['root'], desc, case['value'])
            require(actual == case['policy'], f'Policy fixture disagreement: {case["name"]}: {actual} vs {case["policy"]}')
        write(work/'vectors.json', cases)
        for root, typename in ROOTS.items():
            print('ROOT', root, flush=True)
            directory = work/root; directory.mkdir()
            generated = directory/'generated'; generated.mkdir()
            command(directory/'generate', [args.probe, '--module-list', repo/'tools/qualification/ngap-rel18.modules',
                    '--asn1-root', args.asn1_root, '--root-module', 'NGAP-IEs', '--message', typename,
                    '--output-prefix', generated/'root', '--namespace', 'transfer_'+root, '--verify-determinism'])
            genlog = (directory/'generate/stdout.txt').read_text()
            for family in ('types', 'mapping', 'codec'):
                require(f'GENERATION {family} PASS' in genlog and f'DETERMINISM {family} PASS' in genlog,
                        'Missing generation/determinism evidence')
            bridge = bridge_module.Bridge(*(generated/f'root_{kind}.hpp' for kind in ('types', 'mapping', 'codec')))
            group = [c for c in cases if c['root'] == root]
            for case in group:
                case['cpp_type'] = 'NgapIEsPduSessionResource'+typename[len('PDUSessionResource'):]
            desc = getattr(native.NGAP_IEs, typename)
            cpp = directory/'consumer.cpp'; cpp.write_text(cpp_driver(bridge, desc, group))
            executable = directory/'consumer'
            command(directory/'compile', [args.cxx, '-std=c++20', '-O0', '-g0', '-Wall', '-Wextra',
                    '-Werror', '-pedantic-errors', '-Wconversion', '-Wsign-conversion',
                    '-I'+str(repo/'libaper'), '-I'+str(generated), cpp, repo/'libaper/runtime.cpp', '-o', executable])
            output = command(directory/'positive', [executable])
            for index, case in enumerate(group):
                require(f'CASE {index} PASS' in output.splitlines(), 'Native case not executed')
                if case['encode'] and len(case['wire']) <= 4096:
                    line = next(l for l in output.splitlines() if l.startswith(f'ENCODE {index} '))
                    emitted = bytes.fromhex(line.split()[2])
                    require(emitted == case['wire'], 'Canonical wire differs')
                    desc.from_aper(emitted)
                    require(desc.get_val() == case['value'], 'Oracle rejects native encoder semantics')
                    report['checks'] += 1
                report['checks'] += 1
            seed = group[0]['wire']
            mutations = []
            for cut in range(len(seed)):
                mutations.append((f'truncate-{cut}', seed[:cut], 'truncated_input',
                                  {'operation':'truncate', 'offset_octet':cut, 'original_length':len(seed)}))
            mutations.append(('trailing', seed+b'\x00', 'trailing_data',
                              {'operation':'append', 'offset_octet':len(seed), 'new_hex':'00'}))
            # Empty ReleaseResponse has one extension bit and one optional bit.
            # Flip an actually-unused terminal bit in the independent oracle seed.
            if root == 'release_response':
                require(seed == b'\x00', 'Frozen empty SEQUENCE layout changed')
                mutations.append(('terminal-padding', bytes([seed[0] | 1]), 'nonzero_padding',
                                  {'operation':'bit_or', 'offset_octet':0, 'old_hex':'00', 'new_hex':'01'}))
                # Receiver mutation from the oracle's empty root. Preserve the
                # optional bit, set extension-present, insert normally-small
                # bitmap length (one), bitmap bit 1, zero alignment, and one
                # opaque open octet. This is a documented seed mutation, not a
                # hand-written golden Release Response or a second PER codec.
                bits = '1'+format(seed[0], '08b')[1:2]+'0000000'+'1'
                bits += '0'*((-len(bits)) % 8)
                suffix_wire = int(bits, 2).to_bytes(len(bits)//8, 'big')+b'\x01\x80'
                suffix_record = {'seed_hex':seed.hex(), 'wire_hex':suffix_wire.hex(),
                    'operation':'replace terminal padding with one unknown SEQUENCE addition',
                    'extension_index':0, 'payload_hex':'80', 'inserted_bitmap_bits':'00000001',
                    'provenance':'oracle-seeded documented receiver mutation, not oracle encoder output'}
                desc.from_aper(suffix_wire)
                oracle_value = desc.get_val()
                additions = {k:v for k,v in oracle_value.items() if k.startswith('_ext_')}
                require(len(additions)==1 and list(additions.values())==[b'\x80'],
                        'Oracle cannot independently retain the mutated unknown addition')
                require(classify(root, desc, oracle_value)=='unsupported_profile', 'Unknown SEQUENCE addition was silently ignored')
                suffix_record['oracle_value'] = oracle_value
                inspected = command(directory/'sequence-addition', [executable, 'inspect-additions'],
                                    data=suffix_wire.hex()+'\n').strip()
                require(inspected=='additions:1:1:0:80', 'Unknown SEQUENCE addition metadata differs: '+inspected)
                refused = command(directory/'sequence-addition-refusal', [executable, 'reencode'],
                                  data=suffix_wire.hex()+'\n').strip()
                require(refused.startswith('error:constraint_violation:'), 'Unknown addition must not be re-encoded')
                bitmap_refused = command(directory/'sequence-addition-bitmap-budget',
                    [executable, 'zero-bitmap'], data=suffix_wire.hex()+'\n').strip()
                require(bitmap_refused.startswith('error:resource_limit:'), 'Extension bitmap budget not enforced')
                suffix_record.update(native_metadata=inspected, native_encode=refused,
                                     category='unsupported_profile')
                write(directory/'sequence-addition.json', suffix_record)
                report['checks'] += 4
            records = []
            for index, (name, wire, code, mutation) in enumerate(mutations):
                result = command(directory/f'mutation-{index:03}', [executable, 'decode'], data=wire.hex()+'\n').strip()
                require(result.startswith('error:'+code+':'), f'Mutation {root}/{name}: {result}')
                records.append({'name':name, 'seed_hex':seed.hex(), 'wire_hex':wire.hex(),
                                'mutation':mutation, 'native_result':result, 'category':native_error(code)})
                report['checks'] += 1
            # Independent malformed corpus: select up to four single-bit
            # mutations rejected by the unchanged oracle. Native outcome may
            # be syntax refusal or a budget refusal (length determinants can
            # request resources first), but it must never be successful.
            oracle_bad = []
            for offset in range(len(seed)*8):
                bad = bytearray(seed); bad[offset//8] ^= 1 << (7-offset%8)
                try:
                    desc.from_aper(bytes(bad))
                except Exception as error:
                    oracle_bad.append((offset, bytes(bad), type(error).__name__))
                if len(oracle_bad)==4:
                    break
            require(oracle_bad, 'No oracle-rejected bit mutation found: '+root)
            for index,(offset, wire, oracle_error) in enumerate(oracle_bad):
                actual = command(directory/f'oracle-invalid-{index}', [executable, 'decode'],
                                 data=wire.hex()+'\n').strip()
                require(actual.startswith('error:'), 'Native accepted oracle-invalid mutation')
                code = actual.split(':')[1]
                require(native_error(code) in ('malformed_aper','resource_limit'),
                        'Unexpected local/internal error for malformed input')
                records.append({'name':f'oracle-invalid-{index}', 'seed_hex':seed.hex(),
                    'wire_hex':wire.hex(), 'mutation':{'operation':'flip_bit','offset_bit':offset,
                    'original_bit':(seed[offset//8]>>(7-offset%8))&1},
                    'oracle_exception':oracle_error, 'native_result':actual,
                    'category':native_error(code)})
                report['checks'] += 1
            # Collection and retained-byte budgets are independently exercised.
            for mode, case in [('zero-collections', next((c for c in group if c['value'].get('protocolIEs') or
                    c['value'].get('iE-Extensions') or 'dLQosFlowPerTNLInformation' in c['value']), None)),
                    ('zero-unknown', next(c for c in group if c['category']=='unknown_open')),
                    ('zero-records', next(c for c in group if c['category']=='unknown_open')),
                    ('zero-depth', group[0] if root=='setup_request' else next(c for c in group if c['name']=='known-extension')),
                    ('zero-wire', group[0])]:
                if case is None:
                    continue
                result = command(directory/mode, [executable, mode], data=case['wire'].hex()+'\n').strip()
                require(result.startswith('error:resource_limit:'), 'Budget not enforced: '+mode)
                report['checks'] += 1
            write(directory/'mutations.json', records)
            report['roots'][root] = {'cases':len(group), 'mutation_checks':len(records),
                'policy_counts':dict(Counter(c['policy'] for c in group)),
                'generated_sha256':{p.name:sha(p) for p in generated.glob('*.hpp')},
                'consumer_sha256':sha(cpp), 'executable_sha256':sha(executable)}
            write(work/'report.json', report)
        require(all(sha(repo/path)==value for path,value in inputs.items()), 'Qualification inputs changed')
        report['status'] = 'PASS'
        report['case_count'] = len(cases)
        report['acceptance'] = 'Independent review and Owner acceptance pending'
    except BaseException as error:
        report.update(status='FAIL_OR_INTERRUPTED', error_type=type(error).__name__,
                      diagnostic=str(error))
        raise
    finally:
        write(work/'report.json', report)
        (work/'SUMMARY.txt').write_text(json.dumps(report, indent=2)+'\n')
        print('Qualification', report['status'], 'checks', report['checks'], flush=True)


if __name__ == '__main__':
    main()
