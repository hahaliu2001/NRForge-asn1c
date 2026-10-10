#!/usr/bin/env python3
"""Validate replayed capability evidence and all-message compile readiness.

This verifies recorded evidence, not complete NGAP-PDU interoperability.
Run component replays and the frozen-source scanner before this command.
"""
import argparse
import hashlib
import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
REPORTS = [
    'tools/shared-integer-qualification/summary.json',
    'tools/batch-size-qualification/qualification-summary.json',
    'tools/batch-fragment-bits-qualification/qualification-summary.json',
    'tools/shared-character-qualification/qualification-summary.json',
    'tools/fragmented-collection-qualification/results.json',
    'tools/shared-null-qualification/native-summary.json',
    'tools/private-key-qualification/results.json',
]


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def check_outcome(name, report):
    if 'status' in report and report['status'] != 'PASS':
        raise ValueError('Component failed: ' + name)
    expected = {
        REPORTS[0]: {'total_cases': 768, 'all_generated_encode_bytes_equal': True,
                     'all_generated_decode_values_equal': True, 'all_native_decode_fields_equal': True},
        REPORTS[1]: {'native_model_exact_cases': 256, 'runtime_encode_cases': 256,
                     'runtime_decode_cases': 256, 'native_decode_exact_wire_cases': 256},
        REPORTS[2]: {'vectors': 152, 'encode_decode_comparisons': 304, 'extension_surrogate': False},
        REPORTS[3]: {'vectors': 427, 'native_encode_decode_vectors': 415,
                     'known_multiplier_extension_model_only_vectors': 12,
                     'all_vectors_independent_cpp_bit_model': True},
        REPORTS[6]: {'total': 240, 'pass': 240, 'fail': 0, 'pycrate_global_oid_checks': 120},
    }
    for key, value in expected.get(name, {}).items():
        if report.get(key) != value:
            raise ValueError('Missing/failed component outcome: ' + name + ': ' + key)
    if name == REPORTS[4]:
        rows = report.get('rows', [])
        if len(rows) != 8 or any(row.get('encode') != 'PASS' or row.get('decode') != 'PASS' for row in rows):
            raise ValueError('Fragment collection outcome incomplete')
    if name == REPORTS[5]:
        rows = report.get('cases', [])
        if len(rows) != 11 or any(row.get('decode_checked') is not True for row in rows):
            raise ValueError('NULL reference outcome incomplete')
    if name == REPORTS[6]:
        rows = report.get('checks', [])
        if len(rows) != 240 or any(row.get('result') != 'PASS' for row in rows):
            raise ValueError('Private reference outcome incomplete')


def testsuite(path, expected):
    text = path.read_text()
    values = {key: int(value) for key, value in re.findall(r'^# (TOTAL|PASS|SKIP|XFAIL|FAIL|XPASS|ERROR):\s*(\d+)', text, re.M)}
    if values != dict(TOTAL=expected, PASS=expected, SKIP=0, XFAIL=0, FAIL=0, XPASS=0, ERROR=0):
        raise ValueError('Focused testsuite failed/incomplete: ' + str(path))
    return dict(counts=values, log_sha256=digest(path))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--readiness', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--typed-testsuite', type=Path, default=ROOT / 'libasn1typed/test-suite.log')
    parser.add_argument('--runtime-testsuite', type=Path, default=ROOT / 'libaper/test-suite.log')
    args = parser.parse_args()
    evidence = {}
    for name in REPORTS:
        path = ROOT / name
        report = json.loads(path.read_text())
        check_outcome(name, report)
        fingerprints = report.get('source_sha256', report.get('input_sha256', report.get('sources', {})))
        if not fingerprints:
            raise ValueError('Missing component source fingerprints: ' + name)
        for source, expected in fingerprints.items():
            if digest(ROOT / source) != expected:
                raise ValueError('Stale evidence: ' + name + ': ' + source)
        evidence[name] = digest(path)
    readiness = json.loads(args.readiness.read_text())
    baseline = json.loads((ROOT / 'tools/n17-shape-qualification/readiness.json').read_text())
    rows = {row['message']: row for row in readiness['messages']}
    preserved = []
    for old in baseline['messages']:
        if old['compile']['status'] != 'PASS':
            continue
        current = rows[old['message']]
        if current['compile']['status'] != 'PASS' or current['compile']['header_sha256'] != old['compile']['header_sha256']:
            raise ValueError('Previously accepted header changed: ' + old['message'])
        preserved.append(old['message'])
    if len(rows) != 131 or len(preserved) != 69:
        raise ValueError('Frozen message/baseline count mismatch')
    for row in rows.values():
        if row['physical_extraction_rc'] or any(item['rc'] for item in row['generation']) or row['compile']['status'] != 'PASS':
            raise ValueError('Readiness gate failed: ' + row['message'])
    for name, expected in readiness['input_sha256'].items():
        if name != 'probe_binary' and digest(ROOT / name) != expected:
            raise ValueError('Stale readiness source: ' + name)
    # Capture all implementation, fixtures and replay inputs, including delegated
    # naming/core/extraction dependencies missing from some component manifests.
    inputs = [ROOT / 'configure.ac', ROOT / 'Makefile.am']
    for directory in ['libaper', 'libasn1typed', 'tools']:
        inputs += [path for path in (ROOT / directory).rglob('*') if path.is_file()
                   and (path.suffix in {'.c', '.h', '.cpp', '.hpp', '.py', '.sh', '.asn1', '.modules'}
                        or path.name == 'Makefile.am')]
    source_hashes = {str(path.relative_to(ROOT)): digest(path) for path in sorted(set(inputs))}
    result = {
        'scope': 'Seven bounded shared capabilities plus N19 prerequisite; generation/compile readiness, not full PDU wire qualification',
        'status': 'PASS', 'messages': 131, 'physical_extraction_pass': 131,
        'body_three_family_generation_pass': 131, 'strict_compile_pass': 131,
        'previously_accepted_header_triples_unchanged': len(preserved),
        'preserved_messages': preserved,
        'component_report_sha256': evidence,
        'focused_tests': {'typed': testsuite(args.typed_testsuite, 40),
                          'runtime': testsuite(args.runtime_testsuite, 10)},
        'readiness_sha256': digest(args.readiness),
        'source_sha256': source_hashes,
        'limitations': [
            'Native oracle limitations and surrogate/model-only cases remain in individual reports.',
            'Bounded implementation/resource domains do not imply support for every legal ASN.1 value.',
            'No complete 131-message NGAP-PDU interoperability qualification or benchmark.',
            'No passing LSan result is claimed for this execution environment.',
        ],
    }
    args.output.write_text(json.dumps(result, indent=2) + '\n')
    print('PASS 131 physical/BODY/strict compile; 69 previous header triples unchanged; current component fingerprints')


if __name__ == '__main__':
    main()
