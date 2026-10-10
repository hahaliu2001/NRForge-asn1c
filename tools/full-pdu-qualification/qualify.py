#!/usr/bin/env python3
"""Finite complete-PDU native interoperability campaign; never a roundtrip-only gate."""
import argparse
from collections import Counter
from concurrent.futures import ThreadPoolExecutor, as_completed
import hashlib
import importlib.util
import importlib.metadata
import json
from pathlib import Path
import re
import shlex
import subprocess
import sys

sys.dont_write_bytecode = True
from native_values import build_profile, json_value
from semantic_bridge import Bridge
from reference_framing import ROLES, POLICIES, parse_root, encode_root, root_error_cases, verify_registry


def load(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def sha(data):
    return hashlib.sha256(data).hexdigest()


def identity(value):
    return sha(json.dumps(json_value(value), sort_keys=True, separators=(',', ':')).encode())


def restore(value):
    if isinstance(value, dict) and value.get('kind') == 'bytes': return bytes.fromhex(value['hex'])
    if isinstance(value, dict) and value.get('kind') == 'tuple': return tuple(restore(x) for x in value['items'])
    if isinstance(value, dict): return {key:restore(item) for key,item in value.items()}
    if isinstance(value, list): return [restore(item) for item in value]
    return value


def error_test(bridge, row, case, env_codec, mapping):
    """Independently specified physical mutations + observable resource boundaries."""
    wire = bytes.fromhex(case['pdu_hex'])
    decode = re.search(r'\b(decode_\w+)\(', env_codec).group(1)
    encode = 'encode_' + decode[7:]
    lines = ['static void physical_checks() {', 'using r = ::nrforge::aper::ErrorCode;',
             '::std::size_t checks = 0;']
    for name, bad, code, offset in root_error_cases(wire):
        lines += ['{', 'const auto bytes = ' + bridge.literal(bad) + ';',
                  f'auto result = ::fullpdu::{decode}(bytes);',
                  f'REQUIRE(!result && result.error().code == r::{code} && result.error().bit_offset == {offset});',
                  '++checks;', '}']
    lines += ['const auto bytes = ' + bridge.literal(wire) + ';',
              f'auto valid = ::fullpdu::{decode}(bytes);', 'REQUIRE(valid);',
              '::nrforge::aper::Limits exact{};',
              f'exact.max_input_octets = {len(wire)}; exact.max_output_octets = {len(wire)}; exact.max_wire_bits = {len(wire)*8};',
              f'REQUIRE(::fullpdu::{decode}(bytes, exact));',
              f'REQUIRE(::fullpdu::{encode}(valid.value(), exact));', 'checks += 2;',
              'for(unsigned budget = 0; budget < 3; ++budget) {',
              'auto limits = exact;',
              'if(budget == 0) --limits.max_input_octets;',
              'if(budget == 1) --limits.max_output_octets;',
              'if(budget == 2) --limits.max_wire_bits;',
              f'if(budget != 1) {{ auto result = ::fullpdu::{decode}(bytes, limits); REQUIRE(!result && result.error().code == r::resource_limit); ++checks; }}',
              f'if(budget != 0) {{ auto result = ::fullpdu::{encode}(valid.value(), limits); REQUIRE(!result && result.error().code == r::resource_limit); ++checks; }}',
              '}',
              'auto damaged_input = bytes;',
              f'auto owned = ::fullpdu::{decode}(damaged_input); REQUIRE(owned);',
              'auto copied = owned.value(); auto moved = ::std::move(owned).value();',
              '::std::fill(damaged_input.begin(),damaged_input.end(),::std::byte{0xff}); damaged_input.clear(); damaged_input.shrink_to_fit();',
              f'auto copied_wire = ::fullpdu::{encode}(copied); auto moved_wire = ::fullpdu::{encode}(moved);',
              'REQUIRE(copied_wire && moved_wire && copied_wire.value().octets == bytes && moved_wire.value().octets == bytes); ++checks;',
              # The target code must remain typed even when its BODY is malformed.
              'const auto malformed = ' + bridge.literal(wire[:3]+b'\x01\xff') + ';',
              f'REQUIRE(!::fullpdu::{decode}(malformed)); ++checks;',
              '::std::cout << "PHYSICAL " << checks << "\\n";', '}']
    return '\n'.join(lines)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--repo', type=Path, default=Path(__file__).resolve().parents[2])
    parser.add_argument('--asn1-root', type=Path, required=True)
    parser.add_argument('--probe', type=Path, required=True)
    parser.add_argument('--work', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--cxx', default='g++')
    parser.add_argument('--jobs', type=int, default=1)
    parser.add_argument('--accepted-profile', type=Path)
    args = parser.parse_args()
    # A failed rerun must not leave a previous PASS masquerading as its result.
    args.output.unlink(missing_ok=True)
    if importlib.metadata.version('pycrate') != '0.7.11': raise ValueError('pycrate version must be exactly 0.7.11')
    repo, root, source = args.repo.resolve(), args.asn1_root.resolve(), Path(__file__).resolve().parent
    work = args.work.resolve(); work.mkdir(parents=True, exist_ok=False)
    reference = load('fullpdu_native_guard', repo/'tools/n11-envelope-qualification/native_reference.py')
    scan = load('fullpdu_inventory', repo/'tools/n13-full-codec-readiness/scan.py')
    manifest_path = repo/'tools/n11-envelope-qualification/source-manifest.json'
    authority, texts = reference.verify_sources(repo, root, manifest_path)
    rows = scan.inventory(root)
    native = reference.compile_native(texts, work)
    verify_registry(native, rows)
    paths = [*sorted((repo/'libaper').glob('*.hpp')), *sorted((repo/'libaper').glob('*.cpp')),
             *sorted((repo/'libasn1typed').glob('asn1typed*.c')), *sorted((repo/'libasn1typed').glob('asn1typed*.h')),
             *sorted(source.glob('*.py')), source/'probe.c', repo/'tools/Makefile.am',
             repo/'tools/developer_tree.c', repo/'tools/developer_tree.h',
             repo/'tools/n11-envelope-qualification/native_reference.py', manifest_path,
             repo/'tools/n13-full-codec-readiness/scan.py', repo/'tools/qualification/ngap-rel18.modules']
    fingerprints = {str(p.relative_to(repo)): sha(p.read_bytes()) for p in paths}
    probe_hash = sha(args.probe.read_bytes())
    profiles = {p['message']: p for p in build_profile(native)}
    if set(profiles) != {r['message'] for r in rows}: raise ValueError('native profile identity mismatch')
    for row in rows:
        p = profiles[row['message']]
        if p['role'] != ROLES[['INITIATING MESSAGE','SUCCESSFUL OUTCOME','UNSUCCESSFUL OUTCOME'].index(row['role'])] or p['code'] != row['procedure_code']:
            raise ValueError('native procedure role/code identity mismatch')
        ids = [c['id'] for c in p['cases']]
        if not ids or len(ids) != len(set(ids)) or any(c['status'] != 'PASS' for c in p['cases']):
            raise ValueError('incomplete/failed/duplicate native profile: '+row['message'])
        if {c['criticality'] for c in p['cases']} != set(POLICIES): raise ValueError('criticality coverage missing')
    headers = work/'headers'; headers.mkdir()
    messages = work/'messages.txt'; messages.write_text(''.join(r['message']+'\n' for r in rows))
    with (work/'probe.log').open('w') as log:
        output = subprocess.check_output([str(args.probe.resolve()), str(repo/'tools/qualification/ngap-rel18.modules'),
                                         str(root), str(messages), str(headers)], stderr=log, text=True)
    observed = json.loads(output)
    if observed.get('parse') != 'PASS' or observed.get('fix') != 'PASS' or observed.get('parser_deleted') is not True:
        raise ValueError('missing Parser/Fixer/owned-lifetime evidence')
    if len(observed['messages']) != len(rows): raise ValueError('probe message count mismatch')
    for row, actual in zip(rows, observed['messages'], strict=True):
        if actual != {'message':row['message'], 'code':row['procedure_code'],
                      'role':ROLES.index(profiles[row['message']]['role']),
                      'criticality':POLICIES.index(row['expected_criticality']), 'deterministic':True}:
            raise ValueError('probe/native/text registry disagreement: '+row['message'])
    (work/'generation.json').write_text(output)
    compiler = shlex.split(args.cxx)
    options = ['-std=c++20','-Wall','-Wextra','-Werror','-pedantic-errors','-Wconversion','-Wsign-conversion','-DNDEBUG']
    include = ['-I'+str(repo/'libaper'), '-I'+str(headers)]
    runtime = work/'runtime.o'
    subprocess.run(compiler+options+include+['-c',str(repo/'libaper/runtime.cpp'),'-o',str(runtime)],check=True)
    descriptors = {}
    table = native.NGAP_PDU_Descriptions.NGAP_ELEMENTARY_PROCEDURES.get_val()
    for p in table.root:
        for k in ('InitiatingMessage','SuccessfulOutcome','UnsuccessfulOutcome'):
            if k in p: descriptors[p[k]._typeref.called[1]] = p[k]
    details = []
    def execute(index, row):
        stem = f'{index:03d}'
        group = profiles[row['message']]
        body_files = [headers/f'{stem}_body_{f}.hpp' for f in ('types','mapping','codec')]
        env_files = [headers/f'{stem}_envelope_{f}.hpp' for f in ('types','mapping','codec')]
        body_type = re.search(r'using body_type = ::fullpdu::(\w+);', env_files[1].read_text()).group(1)
        bridge = Bridge(*body_files)
        desc = descriptors[row['message']]
        # Native descriptor encoders are mutable/global; workers only read the
        # storage captured by the serial independent native stage.
        cases = [dict(c, value=restore(c['storage_semantic_json'])) for c in group['cases']]
        tu = bridge.emit_pdu(desc,cases,body_type,stem+'_body',*env_files,stem+'_envelope')
        physical = error_test(bridge,row,cases[0],env_files[2].read_text(),env_files[1].read_text())
        tu = tu.replace('int main() {', physical+'\nint main() {\nphysical_checks();')
        path = work/f'{stem}.cpp'; path.write_text(tu)
        binary = work/f'{stem}.driver'
        compile_run = subprocess.run(compiler+options+include+[str(path),str(runtime),'-o',str(binary)],stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True)
        (work/f'{stem}.compile.log').write_text(compile_run.stdout)
        if compile_run.returncode: raise ValueError(f'{row["message"]} compile failed; see {stem}.compile.log')
        run = subprocess.run([str(binary)],stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True)
        (work/f'{stem}.output').write_text(run.stdout)
        (work/f'{stem}.stderr').write_text(run.stderr)
        if run.returncode: raise ValueError(f'{row["message"]} execution failed; see {stem}.stderr')
        actual_wires = {}
        physical_count = 0
        for line in run.stdout.splitlines():
            fields = line.split()
            if len(fields)==2 and fields[0]=='PHYSICAL': physical_count += int(fields[1])
            elif len(fields)==3 and fields[0]=='CASE' and fields[1] not in actual_wires:
                actual_wires[fields[1]] = bytes.fromhex(fields[2])
            else: raise ValueError('unexpected/duplicate generated response')
        if set(actual_wires) != {c['id'] for c in cases} or not physical_count: raise ValueError('missing actual execution evidence')
        # Native oracle objects are shared; explicit backdecode happens serially below.
        return (index,row,group,actual_wires,physical_count,
                {p.name:sha(p.read_bytes()) for p in body_files+env_files},sha(path.read_bytes()),sha(run.stdout.encode()))
    with ThreadPoolExecutor(max_workers=args.jobs) as pool:
        futures = [pool.submit(execute,i,row) for i,row in enumerate(rows)]
        try:
            for future in as_completed(futures):
                details.append(future.result())
                print('compiled/executed',len(details),'/131',flush=True)
        except BaseException:
            for future in futures: future.cancel()
            raise
    records=[]
    pdu = native.NGAP_PDU_Descriptions.NGAP_PDU
    for index,row,group,wires,physical_count,hashes,tu_hash,response_hash in sorted(details):
        proof=[]
        for case in group['cases']:
            wire=wires[case['id']]
            if wire.hex()!=case['pdu_hex']: raise ValueError('complete generated/native byte disagreement')
            framed=parse_root(wire)
            if framed.payload.hex()!=case['body_hex'] or encode_root(group['role'],group['code'],case['criticality'],framed.payload)!=wire:
                raise ValueError('independent framing/BODY discrepancy')
            expected=(group['role'],{'procedureCode':group['code'],'criticality':case['criticality'],
                                    'value':(row['message'],case['value'])})
            pdu.from_aper(wire)
            if pdu.get_val()!=expected: raise ValueError('generated/native semantic disagreement')
            proof.append({'id':case['id'],'octet_count':len(wire),'wire_sha256':sha(wire),
                          'semantic_sha256':identity(expected),'variant':case['variant'],
                          'known_table_trace':case['known_table_trace'],
                          'declared_criticality':case['declared_criticality'],
                          'policy_profile':case['policy_profile'],'status':'PASS'})
        records.append(dict(row,case_count=len(proof),physical_checks=physical_count,
                            generated_sha256=hashes,translation_unit_sha256=tu_hash,response_sha256=response_hash,cases=proof,status='PASS'))
    reference.verify_sources(repo,root,manifest_path)
    if fingerprints!={str(p.relative_to(repo)):sha(p.read_bytes()) for p in paths} or probe_hash!=sha(args.probe.read_bytes()):
        raise ValueError('qualification inputs changed during execution')
    profile={'source_authority':authority,'source_sha256':fingerprints,
             'reference':{'pycrate_version':'0.7.11','generated_oracle_sha256':sha((work/'native_oracle.py').read_bytes())},
             'identities':records,
             'counts':{'messages':len(records),'procedures':len({r['procedure_code'] for r in records}),
                       'roles':dict(Counter(r['role'] for r in records)),
                       'complete_native_and_generated_semantic_cases':sum(r['case_count'] for r in records),
                       'policy_profiles':dict(Counter(c['policy_profile'] for r in records for c in r['cases'])),
                       'physical_and_resource_checks':sum(r['physical_checks'] for r in records)}}
    candidate=work/'candidate-profile.json'; candidate.write_text(json.dumps(profile,indent=2)+'\n')
    accepted=False
    if args.accepted_profile:
        if json.loads(args.accepted_profile.read_text())!=profile: raise ValueError('reviewed exact profile mismatch')
        accepted=True
    report={'status':'PASS_REVIEWED_PROFILE' if accepted else 'MATCHED_CANDIDATE_REQUIRES_INDEPENDENT_REVIEW',
            'profile_sha256':sha(candidate.read_bytes()),'probe_sha256':probe_hash,
            'compiler':subprocess.check_output(compiler+['--version'],text=True).splitlines()[0],
            'strict_options':options,'counts':profile['counts'],'unresolved_disagreements':0,
            'scope':'finite complete-PDU native reference acceptance profile, not exhaustive value-space or live vendor qualification',
            'limitations':['Conditional/application requirements and contained NAS/RRC interpretation are outside scope.',
                           'Private empty object set uses explicit vendor-opaque content, not a known typed vendor row.',
                           'Root physical/resource matrix exercises one mandatory-minimum case per message; component tests cover deeper primitive cases.',
                           'Only native reference operations actually executed are counted; no normalization or oracle exception suppression.']}
    temporary=args.output.with_name(args.output.name+'.tmp')
    temporary.write_text(json.dumps(report,indent=2)+'\n'); temporary.replace(args.output)
    print(json.dumps(report['counts']),flush=True)


if __name__=='__main__':
    main()
