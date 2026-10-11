#!/usr/bin/env python3
"""All-message readiness scan. Successful compilation is not wire qualification."""
import argparse
from collections import Counter, defaultdict
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import shlex
import subprocess
import sys

sys.dont_write_bytecode = True


def inventory(root):
    # Bounded recognizer of the verified frozen declarations, not an ASN.1 compiler.
    descriptions = (root / 'e1/asn1/E1AP-PDU-Descriptions.asn').read_text()
    contents = (root / 'e1/asn1/E1AP-PDU-Contents.asn').read_text()
    constants = (root / 'e1/asn1/E1AP-Constants.asn').read_text()
    descriptions = re.sub(r'--[^\n]*', '', descriptions)
    codes = dict(re.findall(r'^\s*(id-[\w-]+)\s+ProcedureCode\s*::=\s*(\d+)\s*$', constants, re.M))
    rows, procedures = [], []
    pattern = r'^\s*([a-z][\w-]*)\s+E1AP-ELEMENTARY-PROCEDURE\s*::=\s*\{([^}]+)\}'
    for proc, body in re.findall(pattern, descriptions, re.M):
        selectors = re.findall(r'(INITIATING MESSAGE|SUCCESSFUL OUTCOME|UNSUCCESSFUL OUTCOME)\s+([\w-]+)', body)
        code = re.findall(r'PROCEDURE CODE\s+(id-[\w-]+)', body)
        crit = re.findall(r'CRITICALITY\s+(reject|ignore|notify)', body)
        if len(code) != 1 or code[0] not in codes or not selectors or len(crit) > 1:
            raise ValueError('unrecognized procedure declaration: ' + proc)
        roles = [role for role, _ in selectors]
        if roles.count('INITIATING MESSAGE') != 1 or len(set(roles)) != len(roles):
            raise ValueError('ambiguous procedure roles: ' + proc)
        procedures.append(proc)
        for role, message in selectors:
            declaration = re.findall(r'^\s*' + re.escape(message) + r'\s*::=\s*SEQUENCE\s*\{([^}]+)', contents, re.M)
            if len(declaration) != 1:
                raise ValueError('message declaration absent or ambiguous: ' + message)
            rows.append({'message': message, 'procedure': proc, 'procedure_code': int(codes[code[0]]),
                         'role': role, 'expected_criticality': crit[0] if crit else 'ignore',
                         'criticality_provenance': 'explicit' if crit else 'class-default',
                         'root_container': 'private' if 'PrivateIE-Container' in declaration[0] else 'protocol-ie'})
    members = []
    for name in ['E1AP-ELEMENTARY-PROCEDURES-CLASS-1', 'E1AP-ELEMENTARY-PROCEDURES-CLASS-2']:
        bodies = re.findall(r'^\s*' + name + r'\s+E1AP-ELEMENTARY-PROCEDURE\s*::=\s*\{([^}]+)\}', descriptions, re.M)
        if len(bodies) != 1:
            raise ValueError('procedure root set missing')
        members += re.findall(r'[a-z][\w-]*', bodies[0])
    if len(procedures) != 40 or len(set(procedures)) != 40 or Counter(members) != Counter(procedures):
        raise ValueError('procedure inventory/root-set reconciliation failed')
    if len(rows) != 72 or len({r['message'] for r in rows}) != 72:
        raise ValueError('frozen 72-message reconciliation failed')
    if Counter(r['role'] for r in rows) != {'INITIATING MESSAGE': 40, 'SUCCESSFUL OUTCOME': 20, 'UNSUCCESSFUL OUTCOME': 12}:
        raise ValueError('outcome count mismatch')
    return rows


def verify(repo, root):
    manifest = json.loads((repo / 'tools/e1ap-readiness/source-manifest.json').read_text())
    order = (repo / 'tools/e1ap-readiness/e1ap-rel18.modules').read_text().splitlines()
    if order != [m['path'] for m in manifest['modules']]:
        raise ValueError('module order mismatch')
    for m in manifest['modules']:
        data = (root / m['path']).read_bytes()
        if len(data) != m['size'] or hashlib.sha256(data).hexdigest() != m['sha256'] or hashlib.sha1(b'blob ' + str(len(data)).encode() + b'\0' + data).hexdigest() != m['git_blob']:
            raise ValueError('frozen source mismatch: ' + m['path'])
    return manifest


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--repo', type=Path, default=Path('.'))
    ap.add_argument('--asn1-root', type=Path, required=True)
    ap.add_argument('--probe', type=Path, required=True)
    ap.add_argument('--work', type=Path, required=True)
    ap.add_argument('--output', type=Path, required=True)
    ap.add_argument('--cxx', default='g++')
    ap.add_argument('--envelopes', action='store_true', help='also generate and strictly compile target envelopes')
    args = ap.parse_args()
    repo, root, probe = args.repo.resolve(), args.asn1_root.resolve(), args.probe.resolve()
    spec = importlib.util.spec_from_file_location('n12guard', repo / 'tools/n12-codec-coverage/run.py')
    guard = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(guard)
    authority = verify(repo, root)
    rows = inventory(root)
    work = args.work.resolve()
    work.mkdir(parents=True, exist_ok=False)
    headers = work / 'headers'
    headers.mkdir()
    messages = work / 'messages.txt'
    messages.write_text(''.join(r['message'] + '\n' for r in rows))
    inputs = [probe, repo / 'tools/asn1typed_codec_coverage.c', repo / 'tools/e1ap-readiness/scan.py',
              repo / 'tools/n12-codec-coverage/run.py', repo / 'tools/e1ap-readiness/source-manifest.json', repo / 'tools/e1ap-readiness/e1ap-rel18.modules', *sorted((repo / 'libaper').glob('*.hpp')),
              *sorted((repo / 'libasn1typed').glob('asn1typed*.c')), *sorted((repo / 'libasn1typed').glob('asn1typed*.h'))]
    fingerprints = {str(p.relative_to(repo)) if p.is_relative_to(repo) else 'probe_binary': hashlib.sha256(p.read_bytes()).hexdigest() for p in inputs}
    with (work / 'probe.log').open('w') as log:
        run = subprocess.run([str(probe), str(repo / 'tools/e1ap-readiness/e1ap-rel18.modules'), str(root), str(messages), str(headers), 'E1AP-PDU-Contents', 'E1AP-PDU-Descriptions', 'E1AP-PDU']
                             + (['--envelopes'] if args.envelopes else []),
                             check=True, stdout=subprocess.PIPE, stderr=log, text=True)
    raw = guard.validate(json.loads(run.stdout), messages)
    (work / 'raw-inventory.json').write_text(json.dumps(raw) + '\n')
    compiler = shlex.split(args.cxx)
    version = subprocess.check_output(compiler + ['--version'], text=True).splitlines()[0]
    for i, (row, observed) in enumerate(zip(rows, raw['messages'], strict=True)):
        if row['message'] != observed['message']:
            raise ValueError('row identity mismatch')
        row.update({k: v for k, v in observed.items() if k != 'inventory'})
        print('scan', i + 1, '/', len(rows), row['message'], flush=True)
        row['compile'] = {'status': 'NOT_RUN', 'reason': 'physical extraction or BODY generation failed'}
        if observed['physical_extraction_rc'] == 0 and all(g['rc'] == 0 for g in observed['generation']):
            tu = headers / (f'{i:03d}.cpp')
            includes = ''.join(f'#include "{i:03d}_{f}.hpp"\n' for f in ['types', 'mapping', 'codec'])
            tu.write_text('#include "runtime.hpp"\n#include "sequence_extensions.hpp"\n' + includes)
            cmd = compiler + ['-std=c++20', '-Wall', '-Wextra', '-Werror', '-pedantic-errors', '-Wconversion', '-Wsign-conversion',
                              '-DNDEBUG', '-I' + str(repo / 'libaper'), '-fsyntax-only', str(tu)]
            result = subprocess.run(cmd, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
            (work / f'{i:03d}-compile.log').write_text(result.stdout)
            row['compile'] = {'status': 'PASS' if result.returncode == 0 else 'FAIL', 'return_code': result.returncode,
                              'diagnostic': result.stdout, 'header_sha256': {f: hashlib.sha256((headers / f'{i:03d}_{f}.hpp').read_bytes()).hexdigest() for f in ['types', 'mapping', 'codec']}}
        row['wire_qualification'] = 'NOT_RUN'
        if args.envelopes:
            generations = observed.get('envelope_generation', [])
            eligible = observed['physical_extraction_rc'] == 0 and observed['envelope_extraction_rc'] == 0 and all(g['rc'] == 0 for g in observed['generation'])
            if [g['family'] for g in generations] != ([0, 1, 2] if eligible else []):
                raise ValueError('envelope generation family coverage mismatch')
            row['envelope_compile'] = {'status': 'NOT_RUN', 'reason': 'BODY or envelope evidence/generation failed'}
            if eligible and all(g['rc'] == 0 for g in generations):
                tu = headers / f'{i:03d}_envelope.cpp'
                families = ['types', 'mapping', 'codec', 'envelope_types', 'envelope_mapping', 'envelope_codec']
                tu.write_text('#include "runtime.hpp"\n#include "sequence_extensions.hpp"\n' + ''.join(f'#include "{i:03d}_{f}.hpp"\n' for f in families))
                cmd = compiler + ['-std=c++20', '-Wall', '-Wextra', '-Werror', '-pedantic-errors', '-Wconversion', '-Wsign-conversion', '-DNDEBUG', '-I' + str(repo / 'libaper'), '-fsyntax-only', str(tu)]
                result = subprocess.run(cmd, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
                (work / f'{i:03d}-envelope-compile.log').write_text(result.stdout)
                row['envelope_compile'] = {'status': 'PASS' if result.returncode == 0 else 'FAIL', 'return_code': result.returncode,
                                           'diagnostic': result.stdout, 'header_sha256': {f: hashlib.sha256((headers / f'{i:03d}_{f}.hpp').read_bytes()).hexdigest() for f in families[3:]}}
    verify(repo, root)
    for p in inputs:
        key = str(p.relative_to(repo)) if p.is_relative_to(repo) else 'probe_binary'
        if hashlib.sha256(p.read_bytes()).hexdigest() != fingerprints[key]:
            raise ValueError('scan input changed during execution: ' + key)
    clusters = defaultdict(list)
    for row in rows:
        if row['physical_extraction_rc'] != 0:
            clusters['physical-extraction: ' + row['extraction_diagnostic']].append(row['message'])
        elif any(g['rc'] for g in row['generation']):
            first = next(g for g in row['generation'] if g['rc'])
            clusters['generation: ' + first['diagnostic']].append(row['message'])
        elif row['compile']['status'] == 'FAIL':
            clusters['strict-compile failure'].append(row['message'])
    report = {'scope': '72-message physical extraction, BODY generation and strict compile readiness; not new wire qualification',
              'baseline_commit': subprocess.check_output(['git', '-C', str(repo), 'rev-parse', 'HEAD'], text=True).strip(),
              'source_authority': authority, 'input_sha256': fingerprints, 'compiler': version,
              'strict_compile_options': ['-std=c++20', '-Wall', '-Wextra', '-Werror', '-pedantic-errors', '-Wconversion', '-Wsign-conversion', '-DNDEBUG', '-fsyntax-only'],
              'parse': raw['parse'], 'fix': raw['fix'], 'parser_deleted_before_generation': raw['parser_deleted_before_generation'],
              'inventory_counts': {'procedures': 40, 'messages': len(rows), 'roles': dict(Counter(r['role'] for r in rows))},
              'summary': {'physical': dict(Counter('PASS' if r['physical_extraction_rc'] == 0 else 'FAIL' for r in rows)),
                          'body_generation': dict(Counter('NOT_RUN' if r['physical_extraction_rc'] else 'PASS' if all(g['rc'] == 0 for g in r['generation']) else 'FAIL' for r in rows)),
                          'strict_compile': dict(Counter(r['compile']['status'] for r in rows)),
                          'envelope_extraction': dict(Counter('PASS' if r['envelope_extraction_rc'] == 0 else 'FAIL' for r in rows))},
              'first_failure_clusters': dict(clusters),
              'limitations': ['Observed inventory covers ordinary declarations and use-sites, not bound-instance internals.',
                              'Extraction stops at its first failure; later dependencies are masked.',
                              'Shared diagnostic text can have different underlying semantic causes.',
                              'Cluster sizes are observed affected message counts, not predicted unlock counts.',
                              'Target descriptor extraction does not qualify complete PDU encoding or interoperability.',
                              'Strict syntax compilation does not establish runtime bytes, linkage, vendor interoperability or protocol policy.'],
              'messages': rows}
    if args.envelopes:
        report['scope'] = '72-message BODY and target-envelope extraction/generation/strict compilation readiness; not wire qualification'
        report['summary']['envelope_generation'] = dict(Counter('NOT_RUN' if not r['envelope_generation'] else 'PASS' if all(g['rc'] == 0 for g in r['envelope_generation']) else 'FAIL' for r in rows))
        report['summary']['envelope_strict_compile'] = dict(Counter(r['envelope_compile']['status'] for r in rows))
        for r in rows:
            if r['envelope_extraction_rc']:
                clusters['envelope-extraction: ' + r['envelope_diagnostic']].append(r['message'])
            elif any(g['rc'] for g in r['envelope_generation']):
                clusters['envelope-generation failure'].append(r['message'])
            elif r['envelope_compile']['status'] == 'FAIL':
                clusters['envelope-strict-compile failure'].append(r['message'])
        report['first_failure_clusters'] = dict(clusters)
    args.output.write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report['summary']))


if __name__ == '__main__':
    main()
