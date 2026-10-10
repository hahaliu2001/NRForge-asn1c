#!/usr/bin/env python3
"""Frozen native reference preflight; generated qualification requires later integration."""
import argparse
from collections import Counter
import hashlib
import importlib.metadata
import json
from pathlib import Path
import tempfile
import subprocess

from body_cases import build_body_cases
from native_reference import verify_sources, compile_native, procedure_evidence, initiating_body, fragmented_ue_child, opaque_root_payload, extension_payload

POLICIES = ('reject', 'ignore', 'notify')
ROOTS = {'i': 'initiatingMessage', 's': 'successfulOutcome', 'u': 'unsuccessfulOutcome'}


def build_envelope_cases(native):
    bodies, to_native = build_body_cases(native.NGAP_IEs)
    cases = []
    for name, model, opaque in bodies:
        for criticality in range(3):
            cases.append({
                'id': name + '-outer-criticality-' + str(criticality),
                'model': 'i:41:' + str(criticality) + ':' + model,
                'value': ('initiatingMessage', {'procedureCode': 41, 'criticality': POLICIES[criticality],
                          'value': ('UEContextReleaseCommand', to_native(model))}),
                'body_model': model, 'opaque': opaque,
                'criticality': criticality, 'category': 'target_body',
            })
    for root, selector in ROOTS.items():
        for code in (0, 41, 255):
            if root == 'i' and code == 41: continue  # Known target must decode typed, even on failure.
            for criticality in range(3):
                for size in (1, 3, 127, 128, 16384, 65536):
                    payload = bytes((i * 17 + 128) % 256 for i in range(size))
                    cases.append({'id': f'opaque-{root}-{code}-{criticality}-{size}',
                        'model': f'o:{root}:{code}:{criticality}:{payload.hex()}', 'opaque': True,
                        'category': 'opaque_raw_known_other' if (root, code) in (('i',0),('s',0),('s',41),('u',0)) else 'opaque_raw_unregistered',
                        'value': (selector, {'procedureCode': code, 'criticality': POLICIES[criticality],
                                  'value': ('_unk_004', payload)})})
    for index in (0, 63, 64, 255, 256):
        for size in (1, 3, 127, 128, 16384, 65536):
            payload = bytes((i * 17 + 128) % 256 for i in range(size))
            cases.append({'id': f'outer-extension-{index}-{size}', 'model': f'x:{index}:{payload.hex()}',
                          'opaque': True, 'category': 'outer_choice_extension', 'value': ('_ext_' + str(index), payload)})
    # Native schema-decodable other procedures remain opaque in the bounded receiver.
    # Empty protocolIEs does not establish application-level mandatory IE policy.
    for root, code, typename in (('i', 0, 'AMFConfigurationUpdate'),
            ('s', 0, 'AMFConfigurationUpdateAcknowledge'),
            ('u', 0, 'AMFConfigurationUpdateFailure'),
            ('s', 41, 'UEContextReleaseComplete')):
        for criticality in range(3):
            child_obj = getattr(native.NGAP_PDU_Contents, typename)
            child_obj.set_val({'protocolIEs': []})
            payload = child_obj.to_aper()
            cases.append({'id': f'unsupported-native-valid-{root}-{code}-{criticality}',
                'model': f'o:{root}:{code}:{criticality}:{payload.hex()}', 'opaque': True,
                'category': 'unsupported_native_schema_decodable',
                'value': (ROOTS[root], {'procedureCode': code, 'criticality': POLICIES[criticality],
                          'value': (typename, {'protocolIEs': []})})})
    return cases


def preflight(native, cases):
    """Oracle evidence only. No generated receiver result or qualification PASS."""
    pdu = native.NGAP_PDU_Descriptions.NGAP_PDU
    body = native.NGAP_PDU_Contents.UEContextReleaseCommand
    results, limitations, framing_only = [], [], []
    for case in cases:
        pdu.set_val(case['value']); wire = pdu.to_aper()
        record = {'case': case['id'], 'category': case['category'], 'octet_count': len(wire), 'sha256': hashlib.sha256(wire).hexdigest()}
        if case['category'].startswith('opaque_raw_'):
            record['native_sender_constructor'] = 'unmodified_frozen_OPEN_raw_unk_004'
        if case['category'] == 'opaque_raw_known_other':
            record['classification'] = 'intentional_raw_payload_not_native_typed_validity'
        if 'body_model' in case:
            child, framing = initiating_body(wire, case['criticality'])
            body_value = case['value'][1]['value'][1]
            body.set_val(body_value)
            if child != body.to_aper(): raise ValueError('Envelope body differs from standalone native encoding')
            record['outer_framing'] = framing
        elif case['model'].startswith('o:'):
            _, root, code, criticality, payloadhex = case['model'].split(':', 4)
            payload, framing = opaque_root_payload(wire, root, int(code), int(criticality))
            if payload != bytes.fromhex(payloadhex): raise ValueError('Opaque root payload differs')
            record['outer_framing'] = framing
        else:
            _, index, payloadhex = case['model'].split(':', 2)
            payload, framing = extension_payload(wire, int(index))
            if payload != bytes.fromhex(payloadhex): raise ValueError('Outer extension payload differs')
            record['outer_framing'] = framing
        try:
            pdu.from_aper(wire)
            if pdu.get_val() != case['value']:
                # Unsupported known procedures may be selected by the native registry.
                record['native_selfdecode'] = 'different_semantics'
            else: record['native_selfdecode'] = 'exact'
        except Exception as error:
            record['native_selfdecode'] = 'exception'
            record['exception'] = type(error).__name__
            record['message'] = str(error)
            if 'body_model' in case and case['id'].startswith(('ue-choice-unknown-', 'pair-extension-unknown-')):
                record['standalone_child_proof'] = fragmented_ue_child(native, child, body_value)
            if case['category'] == 'target_body':
                if 'standalone_child_proof' not in record:
                    raise ValueError('Unexpected target native exception: ' + case['id'])
                limitations.append(record.copy())
            elif case['category'] == 'opaque_raw_known_other':
                pass  # Classified as deliberately framing-only regardless of native decoder outcome.
            else:
                raise ValueError('Unexpected native exception: ' + case['id'])
        if case['category'] != 'opaque_raw_known_other' and record['native_selfdecode'] == 'different_semantics':
            raise ValueError('Unexpected native semantic disagreement: ' + case['id'])
        if case['category'] == 'opaque_raw_known_other': framing_only.append(record.copy())
        results.append(record)
    return {'status': 'native_preflight_only_generated_receiver_not_run',
            'procedure_evidence': procedure_evidence(native), 'case_count': len(cases),
            'cases': results, 'native_selfdecode_limitations': limitations,
            'opaque_framing_only_inputs': framing_only,
            'category_counts': dict(Counter(c['category'] for c in cases)),
            'native_relation_counts': dict(Counter(c['category'] + ':' + c['native_selfdecode'] for c in results)),
            'sequence_suffix_native_observations': sequence_suffix_observations(native)}


def sequence_suffix_observations(native):
    """Record exact sender API suffix behavior without relabeling or repair."""
    pdu = native.NGAP_PDU_Descriptions.NGAP_PDU
    observations = []
    for key in ('_ext_0', '_ext_2'):
        value = ('initiatingMessage', {'procedureCode': 41, 'criticality': 'reject',
            'value': ('UEContextReleaseCommand', {'protocolIEs': [], key: b'\x80'})})
        pdu.set_val(value); wire = pdu.to_aper()
        item = {'input_key': key, 'input_payload_hex': '80', 'octet_count': len(wire),
                'sha256': hashlib.sha256(wire).hexdigest(), 'wire_hex': wire.hex()}
        try:
            pdu.from_aper(wire)
            got = pdu.get_val()[1]['value'][1]
            item['observed_keys'] = sorted(got)
            item['native_semantics_equal'] = got == value[1]['value'][1]
        except Exception as error:
            item['exception'] = type(error).__name__; item['message'] = str(error)
        observations.append(item)
    return observations


def receiver_literals():
    """Independent inputs, not presented as native full-PDU semantic agreement."""
    baseline = bytes.fromhex('002900100000020072000400010002000f400140')
    rows = []
    for size in range(len(baseline)):
        rows.append((f'target-short-prefix-{size}', baseline[:size].hex(),
                     f'error:truncated_input:{size * 8}'))
    rows.extend([
        ('outer-selector-alignment', '012900100000020072000400010002000f400140', 'error:nonzero_padding:7'),
        ('outer-known-open-alignment', '002901100000020072000400010002000f400140', 'error:nonzero_padding:23'),
        ('known-cause-inner-padding', '002900100000020072000400010002000f400141', 'error:nonzero_padding:159'),
        ('known-cause-inner-trailing', '002900110000020072000400010002000f40024000', 'error:trailing_data:160'),
        ('body-open-inner-alignment', '002900100000020072200400010002000f400140', 'error:nonzero_padding:74'),
        ('body-open-inner-trailing', '002900110000020072000400010002000f40014000', 'error:trailing_data:160'),
        ('pdu-trailing', baseline.hex() + '00', 'error:trailing_data:160'),
        ('native-ext-zero-invalid-suffix-literal', '00290003800000', 'error:truncated_input:56'),
    ])
    return rows


def integrate(a, source, work, manifest, native, cases, reference, qualification_hashes):
    repo = a.repo.resolve(); root = a.asn1_root.resolve()
    generated = work / 'generated'; generated.mkdir(exist_ok=True)
    subprocess.run([str(a.probe.resolve()), '--module-list', str(repo / 'tools/qualification/ngap-rel18.modules'),
        '--asn1-root', str(root), '--root-module', 'NGAP-PDU-Contents', '--message', 'UEContextReleaseCommand',
        '--namespace', 'n10::body', '--output-prefix', str(generated / 'body'),
        '--envelope-module', 'NGAP-PDU-Descriptions', '--envelope-type', 'NGAP-PDU',
        '--envelope-output-prefix', str(generated / 'envelope'), '--verify-determinism'], check=True)
    qualification_inputs = {str(p.relative_to(repo)): p for p in [
        source / 'qualify.py', source / 'native_reference.py', source / 'body_cases.py',
        source / 'requirements.txt', source / 'source-manifest.json']}
    if qualification_hashes != {name: hashlib.sha256(path.read_bytes()).hexdigest() for name, path in qualification_inputs.items()}:
        raise SystemExit('Qualification reference sources changed before generated integration')
    inputs = {str(p.relative_to(repo)): p for p in [
        source / 'driver.cpp', source / 'adapter.hpp',
        repo / 'tools/n10-body-qualification/driver.cpp', repo / 'tools/n10-body-qualification/body_adapter.hpp',
        repo / 'libaper/runtime.cpp', repo / 'libaper/runtime.hpp', repo / 'libaper/sequence_extensions.hpp']}
    hashes = {name: hashlib.sha256(path.read_bytes()).hexdigest() for name, path in inputs.items()}
    header_hashes = {name: hashlib.sha256((generated / name).read_bytes()).hexdigest()
        for name in [f'{prefix}_{kind}.hpp' for prefix in ('body', 'envelope') for kind in ('types', 'mapping', 'codec')]}
    driver = work / 'driver'
    subprocess.run([a.cxx, '-std=c++20', '-Wall', '-Wextra', '-Werror', '-pedantic-errors',
        '-Wconversion', '-Wsign-conversion', '-DNDEBUG', '-I' + str(repo / 'libaper'),
        '-I' + str(source), '-I' + str(generated), str(source / 'driver.cpp'),
        str(repo / 'libaper/runtime.cpp'), '-o', str(driver)], check=True)
    if hashes != {name: hashlib.sha256(path.read_bytes()).hexdigest() for name, path in inputs.items()}:
        raise SystemExit('Integration inputs changed during strict compilation')
    driver.chmod(0o755)  # Own freshly compiled scratch executable, never a supplied binary.
    subprocess.run([str(driver), 'self-test'], check=True)
    if header_hashes != {name: hashlib.sha256((generated / name).read_bytes()).hexdigest() for name in header_hashes}:
        raise SystemExit('Generated headers changed during strict compilation')
    n10_headers = json.loads((repo / 'tools/n10-body-qualification/accepted-profile.json').read_text())['generated_sha256']
    if {name: header_hashes[name] for name in n10_headers} != n10_headers:
        raise SystemExit('Actual BODY generation differs from the accepted N10 three-header baseline')
    profile = {'pycrate_version': '0.7.11', 'source_manifest': manifest, 'generated_header_sha256': header_hashes,
        'procedure_evidence': reference['procedure_evidence'], 'category_counts': reference['category_counts'],
        'native_relation_counts': reference['native_relation_counts'],
        'native_selfdecode_limitations': reference['native_selfdecode_limitations'],
        'opaque_framing_only_inputs': reference['opaque_framing_only_inputs'],
        'sequence_suffix_native_observations': reference['sequence_suffix_native_observations'],
        'matches': [], 'mismatches': []}
    def run(mode, value):
        return subprocess.check_output([str(driver), mode, '-'], input=value + '\n', text=True).strip()
    def check(item, actual, wanted):
        if actual != wanted:
            profile['mismatches'].append(dict(item, expected_sha256=hashlib.sha256(wanted.encode()).hexdigest(),
                actual_sha256=hashlib.sha256(actual.encode()).hexdigest(), actual_preview=actual[:160]))
        else:
            profile['matches'].append(dict(item, actual_sha256=hashlib.sha256(actual.encode()).hexdigest()))
    pdu = native.NGAP_PDU_Descriptions.NGAP_PDU
    for case in cases:
        pdu.set_val(case['value']); wire = pdu.to_aper()
        signature = {'case': case['id'], 'category': case['category'], 'octet_count': len(wire),
                     'sha256': hashlib.sha256(wire).hexdigest()}
        check(dict(signature, operation='generated_decode'), run('decode-pdu', wire.hex()), case['model'])
        if case['opaque']:
            actual = run('refuse-pdu', wire.hex())
            if not actual.startswith('error:constraint_violation:'):
                profile['mismatches'].append(dict(signature, operation='opaque_encode_refusal', actual=actual))
            else: profile['matches'].append(dict(signature, operation='opaque_encode_refusal', actual=actual))
        else:
            actual = run('encode-pdu', case['model'])
            check(dict(signature, operation='generated_encode'), actual, wire.hex())
            if actual == wire.hex():
                pdu.from_aper(bytes.fromhex(actual))
                if pdu.get_val() != case['value']: raise SystemExit('Native generated-byte semantic backdecode differs')
    for case, literal, wanted in receiver_literals():
        wire = bytes.fromhex(literal)
        check({'case': case, 'operation': 'receiver_literal_rejection', 'octet_count': len(wire),
               'sha256': hashlib.sha256(wire).hexdigest()}, run('decode-pdu', literal), wanted)
    literal = '0029000780000002800180'
    signature = {'case': 'body-sequence-suffix-literal', 'octet_count': len(bytes.fromhex(literal)),
                 'sha256': hashlib.sha256(bytes.fromhex(literal)).hexdigest()}
    check(dict(signature, operation='receiver_literal_decode'), run('decode-pdu', literal), 'i:41:0:2/1,80|')
    actual = run('refuse-pdu', literal)
    if not actual.startswith('error:constraint_violation:'):
        profile['mismatches'].append(dict(signature, operation='receiver_literal_refusal', actual=actual))
    else: profile['matches'].append(dict(signature, operation='receiver_literal_refusal', actual=actual))
    if qualification_hashes != {name: hashlib.sha256(path.read_bytes()).hexdigest() for name, path in qualification_inputs.items()}:
        raise SystemExit('Qualification reference sources changed during execution')
    candidate = work / 'actual-profile.json'
    candidate.write_text(json.dumps(profile, indent=2, sort_keys=True) + '\n')
    if profile['mismatches']: raise SystemExit('Unexpected generated/reference disagreement; no PASS')
    accepted = source / 'accepted-profile.json'
    if not accepted.exists() or profile != json.loads(accepted.read_text()):
        raise SystemExit('Exact accepted profile absent or changed; candidate requires independent investigation')
    # State is published only after the exact checked-in per-case profile guard.
    summary = {'pycrate_version': '0.7.11', 'frozen_modules_verified': 6,
        'integration_source_sha256': hashes, 'qualification_source_sha256': qualification_hashes, 'generated_header_sha256': header_hashes,
        'native_case_counts': reference['category_counts'], 'native_relation_counts': reference['native_relation_counts'],
        'matched_checks': len(profile['matches']), 'operations': dict(Counter(x['operation'] for x in profile['matches'])),
        'inherited_fragment_oracle_limitations': len(reference['native_selfdecode_limitations']),
        'intentional_opaque_framing_only_inputs': len(reference['opaque_framing_only_inputs']),
        'byte_normalization': False, 'profile_verified': True, 'production_disagreements_unresolved': 0}
    a.output.write_text(json.dumps(summary, indent=2, sort_keys=True) + '\n')
    print(json.dumps(summary))


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--repo', type=Path, required=True)
    p.add_argument('--asn1-root', type=Path, required=True)
    p.add_argument('--work', type=Path)
    p.add_argument('--output', type=Path, required=True)
    p.add_argument('--native-preflight-only', action='store_true')
    p.add_argument('--probe', type=Path)
    p.add_argument('--cxx', default='g++')
    a = p.parse_args()
    if importlib.metadata.version('pycrate') != '0.7.11': raise SystemExit('pycrate must be pinned to0.7.11')
    source = Path(__file__).resolve().parent
    manifest, texts = verify_sources(a.repo.resolve(), a.asn1_root.resolve(), source / 'source-manifest.json')
    work = a.work.resolve() if a.work else Path(tempfile.mkdtemp(prefix='n11-envelope-'))
    work.mkdir(parents=True, exist_ok=True)
    if not a.native_preflight_only and not a.probe: p.error('--probe is required for generated qualification')
    qualification_hashes = {str(path.relative_to(a.repo.resolve())): hashlib.sha256(path.read_bytes()).hexdigest()
        for path in [source / name for name in ('qualify.py', 'native_reference.py', 'body_cases.py', 'requirements.txt', 'source-manifest.json')]}
    native = compile_native(texts, work)
    cases = build_envelope_cases(native)
    report = preflight(native, cases)
    if not a.native_preflight_only:
        integrate(a, source, work, manifest, native, cases, report, qualification_hashes)
        return
    report.update({'oracle': {'pycrate': '0.7.11'}, 'source_manifest': manifest})
    a.output.write_text(json.dumps(report, indent=2, sort_keys=True) + '\n')
    print(report['status'], report['case_count'])


if __name__ == '__main__': main()
