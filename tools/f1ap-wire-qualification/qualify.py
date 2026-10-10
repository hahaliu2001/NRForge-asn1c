#!/usr/bin/env python3
"""Finite independent complete-byte/semantic F1AP public registry campaign."""
import argparse
from collections import Counter
from concurrent.futures import ThreadPoolExecutor, as_completed
import hashlib
import importlib.metadata
import importlib.util
import json
from pathlib import Path
import re
import shlex
import subprocess
import sys

sys.dont_write_bytecode = True
from native_values import build_profile, coverage_evidence, shared
json_value = shared.json_value
from semantic_bridge import Bridge
import reference_framing as framing


def load(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def sha(data):
    return hashlib.sha256(data).hexdigest()


def file_sha(path):
    digest = hashlib.sha256()
    with Path(path).open('rb') as source:
        for chunk in iter(lambda: source.read(1 << 20), b''):
            digest.update(chunk)
    return digest.hexdigest()


def restore(value):
    if isinstance(value, dict) and value.get('kind') == 'bytes':
        return bytes.fromhex(value['hex'])
    if isinstance(value, dict) and value.get('kind') == 'tuple':
        return tuple(restore(item) for item in value['items'])
    if isinstance(value, dict):
        return {key: restore(item) for key, item in value.items()}
    if isinstance(value, list):
        return [restore(item) for item in value]
    return value


def verify_generation(native, inventory, generated):
    registry = framing.verify_registry(native, inventory)
    generation = json.loads((generated / 'manifest.json').read_text())
    if any(generation.get(key) != value for key, value in
           [('parse', 'PASS'), ('fix', 'PASS'), ('parser_deleted', True),
            ('deterministic', True), ('message_count', 158)]):
        raise ValueError('missing complete owned generation evidence')
    entries = generation['messages']
    actual = [(framing.ROLES[e['role']], e['code'], e['message'],
               framing.POLICIES[e['criticality']]) for e in entries]
    if len(entries) != 158 or sorted(actual) != list(registry):
        raise ValueError('generated/native/frozen identity closure differs')
    descriptions = native.F1AP_PDU_Descriptions
    pdu = descriptions.F1AP_PDU
    roots = list(pdu._root)
    tags = {name: tuple(pdu._cont[name]._tagc[0]) for name in roots}
    if len(set(tags.values())) != 4:
        raise ValueError('ambiguous native effective root tags')
    canonical = sorted(roots, key=lambda name: tags[name])
    role_per = [canonical.index(name) for name in framing.ROLES]
    table = descriptions.F1AP_ELEMENTARY_PROCEDURES.get_val()
    fields = ('InitiatingMessage', 'SuccessfulOutcome', 'UnsuccessfulOutcome')
    procedures = [{'code': row['procedureCode'],
                   'criticality': framing.POLICIES.index(row['criticality']),
                   'payload_present': [field in row for field in fields]}
                  for row in list(table.root) + list(table.ext or [])]
    schema = {'pdu_module': pdu._mod, 'pdu_type': pdu._name, 'profile': 'f1ap',
              'root_count': 4, 'choice_is_extensible': False,
              'unsupported_root_policy': 'reject_before_header',
              'procedure_class_module': descriptions.F1AP_ELEMENTARY_PROCEDURE._mod,
              'procedure_class': descriptions.F1AP_ELEMENTARY_PROCEDURE._name,
              'object_set_module': descriptions.F1AP_ELEMENTARY_PROCEDURES._mod,
              'object_set': descriptions.F1AP_ELEMENTARY_PROCEDURES._name,
              'procedures': procedures}
    if generation['schema'] != schema or generation['registry'] != {
            'role_to_per_index': role_per, 'object_set_extensible': table.ext is not None}:
        raise ValueError('generated framing/schema differs from native evidence')
    # Check actual compiled registry declarations, not only its manifest.
    text = (generated / 'registry.cpp').read_text()
    block = re.search(r'array<ProcedureInfo,(\d+)>\s+procedures\{\{(.*?)\}\};', text, re.S)
    pattern = r'\{UINT64_C\((\d+)\),static_cast<Criticality>\((\d+)\),\{\{(true|false),(true|false),(true|false)\}\}\},'
    if not block:
        raise ValueError('actual registry declarations absent')
    observed = [{'code': int(code), 'criticality': int(policy),
                 'payload_present': [a == 'true', b == 'true', c == 'true']}
                for code, policy, a, b, c in re.findall(pattern, block.group(2))]
    if (int(block.group(1)) != 94 or observed != procedures
            or re.sub(pattern, '', block.group(2)).strip()
            or 'return Registry::create({{0,1,2}},true,procedures,messages,{4,false});' not in text
            or role_per != [0, 1, 2] or table.ext is None):
        raise ValueError('actual compiled registry provenance differs')
    for entry in entries:
        if entry.get('deterministic') is not True:
            raise ValueError('message generation is not deterministic')
        for key in ('public_header', 'types_header', 'mapping_header', 'codec_header', 'adapter'):
            path = generated / entry[key]
            if not path.is_file() or not path.resolve().is_relative_to(generated):
                raise ValueError('missing/escaped generated path')
    return entries, registry


def physical(bridge, case, index):
    wire = bytes.fromhex(case['pdu_hex'])
    lines = ['static void physical_checks() {', 'using namespace ::nrforge::f1ap;',
             'using Error = ::nrforge::aper::ErrorCode;', '::std::size_t checks = 0;']
    for _, bad, code, offset in framing.root_error_cases(wire):
        lines += ['{', 'const auto bytes = ' + bridge.literal(bad) + ';',
                  'auto result = decode_f1ap_pdu(bytes);',
                  f'REQUIRE(!result && result.error().code == Error::{code} && result.error().bit_offset == {offset});',
                  '++checks;', '}']
    lines += ['const auto bytes = ' + bridge.literal(wire) + ';',
              'auto valid = decode_f1ap_pdu(bytes); REQUIRE(valid && valid.value().kind() == PduKind::typed);',
              '::nrforge::aper::Limits exact{};',
              f'exact.max_input_octets = {len(wire)}; exact.max_output_octets = {len(wire)}; exact.max_wire_bits = {len(wire)*8};',
              'REQUIRE(decode_f1ap_pdu(bytes,exact)); REQUIRE(encode_f1ap_pdu(valid.value(),exact)); checks += 2;',
              'for(unsigned budget = 0; budget < 3; ++budget) { auto limits = exact;',
              'if(budget == 0) { --limits.max_input_octets; }',
              'if(budget == 1) { --limits.max_output_octets; }',
              'if(budget == 2) { --limits.max_wire_bits; }',
              'if(budget != 1) { auto r = decode_f1ap_pdu(bytes,limits); REQUIRE(!r && r.error().code == Error::resource_limit); ++checks; }',
              'if(budget != 0) { auto r = encode_f1ap_pdu(valid.value(),limits); REQUIRE(!r && r.error().code == Error::resource_limit); ++checks; }', '}',
              'auto input = bytes; auto owned = decode_f1ap_pdu(input); REQUIRE(owned);',
              'auto copied = owned.value(); auto moved = ::std::move(owned).value(); REQUIRE(owned.value().kind() == PduKind::invalid);',
              '::std::fill(input.begin(),input.end(),::std::byte{0xff}); input.clear(); input.shrink_to_fit();',
              'auto cw = encode_f1ap_pdu(copied); auto mw = encode_f1ap_pdu(moved); REQUIRE(cw && mw && cw.value().octets == bytes && mw.value().octets == bytes); ++checks;',
              'const auto malformed = ' + bridge.literal(wire[:3] + b'\x01\xff') + ';',
              'REQUIRE(!decode_f1ap_pdu(malformed)); ++checks;',
              f'::std::cout << "PHYSICAL {index} " << checks << "\\n";', '}']
    return '\n'.join(lines)


def policy_checks(registry):
    unknown, absent = framing.opaque_slots(registry)
    lines = ['static void policy_checks() {', 'using namespace ::nrforge::f1ap;',
             '::std::size_t checks = 0;']
    for role, code in (*unknown, *absent):
        wire = framing.encode_root(role, code, 'notify', b'\x5a')
        literal = '::std::vector<::std::byte>{' + ','.join(f'::std::byte{{0x{b:02x}}}' for b in wire) + '}'
        lines += ['{', 'auto input = ' + literal + ';',
                  'auto decoded = decode_f1ap_pdu(input); REQUIRE(decoded && decoded.value().kind() == PduKind::opaque_root);',
                  'const auto* header = decoded.value().root_header();',
                  f'REQUIRE(header && static_cast<unsigned>(header->role) == {framing.ROLES.index(role)} && header->procedure_code == {code} && header->received_criticality == Criticality::notify);',
                  'REQUIRE(decoded.value().opaque_root() && decoded.value().opaque_root()->payload == ::std::vector<::std::byte>{::std::byte{0x5a}});',
                  'auto copied = decoded.value(); auto moved = ::std::move(decoded).value();',
                  'REQUIRE(decoded.value().kind() == PduKind::invalid); input.assign(input.size(),::std::byte{0xff});',
                  'REQUIRE(copied.opaque_root()->payload == moved.opaque_root()->payload && moved.opaque_root()->payload == ::std::vector<::std::byte>{::std::byte{0x5a}});',
                  'REQUIRE(!encode_f1ap_pdu(copied) && !encode_f1ap_pdu(moved));',
                  '::nrforge::aper::Limits zero{}; zero.max_retained_unknown_payload_octets = 0;',
                  'auto limited = decode_f1ap_pdu(' + literal + ',zero);',
                  'REQUIRE(!limited && limited.error().code == ::nrforge::aper::ErrorCode::resource_limit && limited.error().bit_offset == 18);',
                  '++checks;', '}']
    lines += ['REQUIRE(checks == 610); ::std::cout << "POLICY " << checks << "\\n";', '}']
    return '\n'.join(lines)


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--repo', type=Path, default=Path(__file__).resolve().parents[2])
    ap.add_argument('--asn1-root', type=Path, required=True)
    ap.add_argument('--generated-root', type=Path, required=True)
    ap.add_argument('--archive', type=Path, required=True)
    ap.add_argument('--archive-receipt', type=Path, required=True)
    ap.add_argument('--work', type=Path, required=True)
    ap.add_argument('--output', type=Path, required=True)
    ap.add_argument('--cxx', default='g++')
    ap.add_argument('--jobs', type=int, choices=range(1, 5), default=1)
    ap.add_argument('--accepted-profile', type=Path)
    args = ap.parse_args()
    args.output.unlink(missing_ok=True)
    if importlib.metadata.version('pycrate') != '0.7.11':
        raise ValueError('pycrate version must be exactly 0.7.11')
    repo, root, generated, archive = args.repo.resolve(), args.asn1_root.resolve(), args.generated_root.resolve(), args.archive.resolve()
    work = args.work.resolve(); work.mkdir(parents=True, exist_ok=False)
    here = Path(__file__).resolve().parent
    reference = load('f1p4_exact_reference', here / 'native_reference.py')
    scan = load('f1p4_frozen_inventory', repo / 'tools/f1ap-readiness/scan.py')
    authority, texts = reference.verify_sources(repo, root)
    inventory = scan.inventory(root)
    sources = [*sorted((repo/'libaper').glob('*.hpp')), *sorted((repo/'libaper').glob('*.cpp')),
               *sorted((repo/'libngap').glob('*.hpp')), *sorted((repo/'libngap').glob('*.cpp')), *sorted((repo/'libngap').glob('*.inc')),
               *sorted((repo/'libasn1typed').glob('asn1typed*.c')), *sorted((repo/'libasn1typed').glob('asn1typed*.h')),
               *sorted(here.glob('*.py')), here/'requirements.txt',
               repo/'tools/full-pdu-qualification/native_values.py', repo/'tools/unified-ngap-qualification/semantic_bridge.py',
               repo/'tools/n11-envelope-qualification/native_reference.py', repo/'tools/f1ap-readiness/scan.py',
               repo/'tools/f1ap-readiness/source-manifest.json', repo/'tools/f1ap-readiness/f1ap-rel18.modules',
               repo/'tools/ngap-dispatch/generate.c', repo/'tools/Makefile.am']
    fingerprints = {str(p.relative_to(repo)): file_sha(p) for p in sources}
    generated_files = sorted(p for p in generated.rglob('*') if p.is_file())
    if any(not p.resolve().is_relative_to(generated) for p in generated_files):
        raise ValueError('generated file escapes input root')
    generated_hashes = {str(p.relative_to(generated)): file_sha(p) for p in generated_files}
    archive_hash = file_sha(archive)
    receipt = json.loads(args.archive_receipt.read_text())
    receipt_hash = file_sha(args.archive_receipt)
    if receipt.get('message_count') != 158 or len(receipt.get('cases', [])) != 158 or any(c.get('status') != 'PASS' for c in receipt['cases']):
        raise ValueError('archive needs complete all-slot integration attestation')
    if archive_hash not in [v for k, v in receipt['input_sha256'].items() if k.endswith('/libnrforge_f1ap.a')]:
        raise ValueError('archive differs from actual linked all-slot integration receipt')
    # Historical absolute paths are normalized only for locating inputs, never
    # for altering byte/semantic comparisons. All relevant archived inputs match.
    archived_inputs = {relative: generated / relative for relative in generated_hashes}
    for directory, patterns in [('libaper', ('*.hpp', 'runtime.cpp')),
                                ('libngap', ('*.hpp', '*.inc', 'f1ap_pdu.cpp'))]:
        for pattern in patterns:
            for path in (repo / directory).glob(pattern):
                archived_inputs[str(path.relative_to(repo))] = path
    for relative, target in archived_inputs.items():
        matches = [digest for name, digest in receipt['input_sha256'].items()
                   if name.endswith('/' + relative)]
        if len(matches) != 1 or matches[0] != file_sha(target):
            raise ValueError('missing/changed archive attested input: ' + str(target))
    native = reference.compile_native(texts, work)
    entries, registry = verify_generation(native, inventory, generated)
    profiles = {p['message']: p for p in build_profile(native)}
    if set(profiles) != {e['message'] for e in entries}:
        raise ValueError('native value profile does not close all158 identities')
    for entry in entries:
        group = profiles[entry['message']]
        cases = group['cases']
        if (group['role'] != framing.ROLES[entry['role']] or group['code'] != entry['code']
                or not cases or len({c['id'] for c in cases}) != len(cases)
                or any(c['status'] != 'PASS' for c in cases)
                or {c['criticality'] for c in cases} != set(framing.POLICIES)):
            raise ValueError('failed/duplicate/incomplete native cases: ' + entry['message'])
    all_cases = [case for group in profiles.values() for case in group['cases']]
    coverage = coverage_evidence(native, all_cases)
    if (len(all_cases) != 4728 or coverage['declared_row_occurrences'] != 975
            or coverage['covered_row_occurrences'] != 975
            or coverage['missing_row_occurrences'] != 0
            or coverage['srbid_extension_values'] != [4, 5]):
        raise ValueError('finite native profile coverage closure differs')
    descriptors = {}
    table = native.F1AP_PDU_Descriptions.F1AP_ELEMENTARY_PROCEDURES.get_val()
    for row in list(table.root) + list(table.ext or []):
        for key in ('InitiatingMessage', 'SuccessfulOutcome', 'UnsuccessfulOutcome'):
            if key in row: descriptors[row[key]._typeref.called[1]] = row[key]
    options = ['-std=c++20', '-O0', '-g0', '-Wall', '-Wextra', '-Werror', '-pedantic-errors', '-Wconversion', '-Wsign-conversion', '-DNDEBUG']
    compiler = shlex.split(args.cxx)
    include = ['-I'+str(repo/'libaper'), '-I'+str(repo/'libngap'), '-I'+str(generated)]
    def compile_object(path, obj, log):
        result = subprocess.run(compiler+options+include+['-c',str(path),'-o',str(obj)], capture_output=True, text=True)
        log.write_text(result.stdout + result.stderr)
        if result.returncode:
            raise ValueError('strict compile failed: ' + str(log))
        with obj.open('rb') as compiled:
            if compiled.read(4) != b'\x7fELF':
                raise ValueError('compiler success without valid ELF object: ' + str(obj))
    def emit(index, entry):
        group = profiles[entry['message']]
        cases = [dict(c, value=restore(c['storage_semantic_json'])) for c in group['cases']]
        bridge = Bridge(generated/entry['types_header'], generated/entry['mapping_header'], generated/entry['codec_header'])
        text = bridge.emit_unified(descriptors[entry['message']], cases, entry['cpp_body_type'], entry['public_header'], index, entry['message'], entry['role'], entry['code'])
        declaration = f'void run_identity_{index:03d}() {{'
        if text.count(declaration) != 1:
            raise ValueError('semantic runner entry missing')
        text = text.replace(declaration, physical(bridge,cases[0],index)+'\n'+declaration+'\nphysical_checks();')
        path, obj = work/f'{index:03d}.cpp', work/f'{index:03d}.o'
        path.write_text(text)
        compile_object(path,obj,work/f'{index:03d}.compile.log')
        return index, obj, file_sha(path), file_sha(obj)
    details = []
    with ThreadPoolExecutor(max_workers=args.jobs) as pool:
        futures = [pool.submit(emit,i,e) for i,e in enumerate(entries)]
        try:
            for future in as_completed(futures):
                details.append(future.result()); print('compiled semantic identity',len(details),'/158',flush=True)
        except BaseException:
            for future in futures: future.cancel()
            raise
    mainlines = ['#include "f1ap.hpp"', '#include <cstdlib>', '#include <iostream>',
                 '#define REQUIRE(...) do { if(!(__VA_ARGS__)) { ::std::cerr << "policy failure " << __LINE__ << "\\n"; ::std::abort(); } } while(0)',
                 policy_checks(registry)]
    mainlines += [f'void run_identity_{i:03d}();' for i in range(158)]
    mainlines += ['int main() { const auto& registry = ::nrforge::f1ap::f1ap_registry_state(); REQUIRE(registry && registry.value().message_count() == 158);',
                  '::std::cout << "REGISTRY 158\\n"; policy_checks();']
    mainlines += [f'run_identity_{i:03d}();' for i in range(158)] + ['}']
    mainpath, mainobj = work/'main.cpp', work/'main.o'
    mainpath.write_text('\n'.join(mainlines)); compile_object(mainpath,mainobj,work/'main.compile.log')
    executable = work/'f1ap-wire-check'
    link_options = ['-fuse-ld=gold', '-Wl,--no-keep-files-mapped', '-Wl,--no-map-whole-files', '-Wl,--no-mmap-output-file']
    with (work/'link.log').open('w') as log:
        subprocess.run(compiler+link_options+[str(mainobj)]+[str(obj) for _,obj,_,_ in sorted(details)]+[str(archive),'-o',str(executable)],stdout=log,stderr=subprocess.STDOUT,check=True)
    run = subprocess.run([str(executable)],capture_output=True,text=True)
    (work/'actual.output').write_text(run.stdout); (work/'actual.stderr').write_text(run.stderr)
    if run.returncode:
        raise ValueError('actual qualification failed: see actual.stderr')
    actual, physical_counts = {}, {}
    registry_count = policy_count = 0
    for line in run.stdout.splitlines():
        fields = line.split()
        if fields == ['REGISTRY','158']: registry_count += 1
        elif fields == ['POLICY','610']: policy_count += 1
        elif len(fields) == 3 and fields[0] == 'PHYSICAL':
            index = int(fields[1])
            if index in physical_counts: raise ValueError('duplicate physical response')
            physical_counts[index] = int(fields[2])
        elif len(fields) == 4 and fields[0] == 'CASE':
            key = (int(fields[1]),fields[2])
            if key in actual: raise ValueError('duplicate semantic response')
            actual[key] = bytes.fromhex(fields[3])
        else: raise ValueError('unexpected actual response: ' + line)
    if registry_count != 1 or policy_count != 1 or set(physical_counts) != set(range(158)):
        raise ValueError('incomplete public registry/policy/physical execution')
    records = []
    pdu = native.F1AP_PDU_Descriptions.F1AP_PDU
    tests = {i:(source_hash,object_hash) for i,_,source_hash,object_hash in details}
    for index, entry in enumerate(entries):
        group = profiles[entry['message']]; proof = []
        for case in group['cases']:
            wire = actual.pop((index,case['id']))
            expected = (group['role'], {'procedureCode':group['code'],'criticality':case['criticality'],'value':(entry['message'],case['value'])})
            if wire.hex() != case['pdu_hex']: raise ValueError('complete native/generated bytes differ')
            frame = framing.parse_root(wire)
            if frame.payload.hex() != case['body_hex'] or framing.encode_root(group['role'],group['code'],case['criticality'],frame.payload) != wire:
                raise ValueError('independent complete framing differs')
            pdu.from_aper(wire)
            if pdu.get_val() != expected: raise ValueError('actual generated -> native semantic disagreement')
            proof.append({'id':case['id'],'status':'PASS','wire_hex':wire.hex(),'wire_sha256':sha(wire),
                          'body_sha256':sha(frame.payload),'semantic_sha256':sha(json.dumps(json_value(expected),sort_keys=True,separators=(',',':')).encode()),
                          'octet_count':len(wire),'criticality':case['criticality'],'policy_profile':case['policy_profile'],
                          'known_table_trace':case['known_table_trace']})
        records.append({'message':entry['message'],'role':entry['role'],'code':entry['code'],'expected_criticality':framing.POLICIES[entry['criticality']],
                        'status':'PASS','case_count':len(proof),'cases':proof,'physical_checks':physical_counts[index],
                        'translation_unit_sha256':tests[index][0],'object_sha256':tests[index][1]})
    if actual: raise ValueError('unrequested native/generated responses')
    reference.verify_sources(repo,root)
    if (fingerprints != {str(p.relative_to(repo)):file_sha(p) for p in sources}
            or generated_hashes != {str(p.relative_to(generated)):file_sha(p) for p in generated_files}
            or archive_hash != file_sha(archive) or receipt_hash != file_sha(args.archive_receipt)):
        raise ValueError('qualification inputs changed during execution')
    report = {'status':'MATCHED_F1AP_CANDIDATE_REQUIRES_INDEPENDENT_REVIEW', 'baseline_commit':'3c78d511561af8a846ca67e29b75dd88fbdbedde',
              'source_authority':authority,'source_sha256':fingerprints,'generated_sha256':generated_hashes,
              'archive_sha256':archive_hash,'archive_receipt_sha256':receipt_hash,
              'reference':{'pycrate_version':'0.7.11','native_oracle_sha256':file_sha(work/'native_oracle.py')},
              'executable_sha256':file_sha(executable),'main_translation_unit_sha256':file_sha(mainpath),
              'actual_output_sha256':sha(run.stdout.encode()),'actual_stderr_sha256':sha(run.stderr.encode()),
              'strict_options':options,'compile_jobs':args.jobs,'link_options':link_options,
              'compiler':subprocess.check_output(compiler+['--version'],text=True).splitlines()[0],
              'coverage':coverage,
              'identities':records,'counts':{'messages':158,'procedures':94,'roles':dict(Counter(r['role'] for r in records)),
                  'cases':sum(r['case_count'] for r in records),'physical_and_resource_checks':sum(physical_counts.values()),
                  'receive_only_policy_slots':610,'linked_executables':1},'unresolved_disagreements':0,
              'limitations':['Finite populated values and selected root/extension semantics; not exhaustive PER/ASN.1 or live vendor interoperability.',
                             'Conditional application rules and mandatory-IE procedure/state policy are outside the wire profile.',
                             'Nested optional omissions are explicit in traces; sizes, combinations and fragmentation not reached by these cases remain unqualified.',
                             'Opaque contained RRC/NAS and private vendor payload interpretation are outside scope.',
                             'Fourth-root payload semantics remain unsupported; immediate rejection is a policy check, not sender interoperability.']}
    if args.accepted_profile:
        accepted = json.loads(args.accepted_profile.read_text())
        for key in ('source_authority','source_sha256','generated_sha256','archive_sha256','reference','coverage','counts'):
            if accepted[key] != report[key]: raise ValueError('accepted profile replay differs: ' + key)
        projection = lambda identities: [{key: value for key, value in record.items()
                                         if key != 'object_sha256'} for record in identities]
        if projection(accepted['identities']) != projection(report['identities']):
            raise ValueError('accepted profile replay differs: identities')
        report['accepted_profile_replayed'] = True
    args.output.parent.mkdir(parents=True,exist_ok=True)
    args.output.write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(report['counts'],indent=2),flush=True)


if __name__ == '__main__':
    main()
