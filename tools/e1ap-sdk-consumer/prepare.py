#!/usr/bin/env python3
"""Regenerate public SDK example from independent native descriptors and goldens.
Only this preparation phase reads renderer mapping files. The resulting C++
consumer includes public installed message headers and never mapping/codecs.
"""
import argparse
import base64
import gzip
import hashlib
import importlib.metadata
import importlib.util
import json
from pathlib import Path
import sys

sys.dont_write_bytecode = True


def load(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--generated-root', type=Path, required=True)
    parser.add_argument('--asn1-root', type=Path, required=True)
    parser.add_argument('--work', type=Path, required=True)
    args = parser.parse_args()
    if importlib.metadata.version('pycrate') != '0.7.11':
        raise ValueError('pinned pycrate 0.7.11 required')
    repo = Path(__file__).resolve().parents[2]
    folder = Path(__file__).resolve().parent
    args.work.mkdir(parents=True, exist_ok=False)
    sys.path.insert(0, str(repo / 'tools/n11-envelope-qualification'))
    nv = load('sdk_native_values', repo / 'tools/e1ap-wire-qualification/native_values.py')
    reference = load('sdk_native_reference', repo / 'tools/e1ap-wire-qualification/native_reference.py')
    bridge_module = load('sdk_semantic_bridge', repo / 'tools/e1ap-wire-qualification/semantic_bridge.py')
    authority, texts = reference.verify_sources(repo, args.asn1_root,
        repo / 'tools/e1ap-readiness/source-manifest.json')
    native = reference.compile_native(texts, args.work)
    transport = json.loads((repo / 'tools/e1ap-wire-qualification/accepted-profile.json').read_text())
    expanded = gzip.decompress(base64.b64decode(transport['data'], validate=True))
    if hashlib.sha256(expanded).hexdigest() != transport['profile_sha256']:
        raise ValueError('accepted profile transport hash mismatch')
    accepted = json.loads(expanded)
    if authority != accepted['source_authority']:
        raise ValueError('source authority differs from accepted profile')
    golden = {e['message']: e for e in accepted['identities']}
    generation = json.loads((args.generated_root / 'manifest.json').read_text())
    entries = {e['message']: e for e in generation['messages']}
    rows = native.E1AP_PDU_Descriptions.E1AP_ELEMENTARY_PROCEDURES.get_val().root
    procedures = [row for row in rows if 'E1SetupRequest' in nv.label(row['InitiatingMessage'])]
    if len(procedures) != 2: raise ValueError('both setup directions required')
    roles = ('initiatingMessage', 'successfulOutcome', 'unsuccessfulOutcome')
    fields = ('InitiatingMessage', 'SuccessfulOutcome', 'UnsuccessfulOutcome')
    sources = []
    fixtures = []
    includes = ['#include <nrforge/e1ap/e1ap.hpp>']
    for index, (procedure, role, field) in enumerate((proc, role, field) for proc in procedures for role, field in zip(roles, fields)):
        code = procedure["procedureCode"]
        role_index = roles.index(role)
        desc = procedure[field]
        message = nv.label(desc)
        builder = nv.Builder('low')
        value = builder.value(desc, message)
        case = nv.native_case(native, desc, procedure, role, value, builder.trace,
                              'low-mandatory-minimum', procedure['criticality'])
        case['id'] = 'low-mandatory-minimum-' + procedure['criticality']
        old = next(c for c in golden[message]['cases'] if c['id'] == case['id'])
        envelope = (role, {'procedureCode': code, 'criticality': case['criticality'],
                          'value': (message, value)})
        semantic = json.dumps(nv.shared.json_value(envelope), sort_keys=True, separators=(',', ':')).encode()
        if case['status'] != 'PASS' or hashlib.sha256(bytes.fromhex(case['pdu_hex'])).hexdigest() != old['wire_sha256'] or hashlib.sha256(semantic).hexdigest() != old['semantic_sha256']:
            raise ValueError('native fixtures differ from accepted independent golden')
        entry = entries[message]
        bridge = bridge_module.Bridge(*(args.generated_root / entry[key] for key in ('types_header', 'mapping_header', 'codec_header')))
        body = nv.shared.storage_value(desc, value)
        one = dict(case, value=body)
        source = bridge.emit_unified(desc, [one], entry['cpp_body_type'], entry['public_header'], index, message, role_index, code)
        source = source[source.index('static void case_0()'):]
        public = Path(entry['public_header']).stem
        source = source.replace('case_0', 'check_' + public)
        source = source.replace('run_identity_%03d' % index, 'run_' + public)
        qtype = bridge.q(entry['cpp_body_type'])
        public = Path(entry['public_header']).stem
        source = source.replace(qtype, '::nrforge::e1ap::messages::' + public + '::Body')
        source = source.replace('"CASE %d %s "' % (index, case['id']), '"PASS %s "' % message)
        sources.append(source)
        fixtures.append(case)
        includes.append('#include <nrforge/e1ap/' + entry['public_header'] + '>')
    preamble = ['// Generated fixture example; regenerate with prepare.py. No test adapters or private codecs.',
                *includes, '#include <cstdlib>', '#include <iostream>', '#include <type_traits>',
                '#define REQUIRE(...) do { if(!(__VA_ARGS__)) { ::std::cerr << "consumer failure " << __LINE__ << "\\n"; ::std::abort(); } } while(0)']
    main = ['int main() {',
            'const auto& library = ::nrforge::e1ap::sdk_identity();',
            'const auto& headers = ::nrforge::e1ap::header_sdk_identity;',
            'REQUIRE(library.version == headers.version && library.fingerprint == headers.fingerprint);',
            'REQUIRE(library.schema_sha256 == headers.schema_sha256 && library.runtime_sha256 == headers.runtime_sha256);',
            'const auto& registry = ::nrforge::e1ap::e1ap_registry_state();',
            'REQUIRE(registry && registry.value().message_count() == 72);',
            ' '.join('run_' + Path(entries[fixture['message']]['public_header']).stem + '();' for fixture in fixtures), '}']
    (folder / 'e1_setup.cpp').write_text('\n'.join(preamble + sources + main) + '\n')
    (folder / 'native-fixtures.json').write_text(json.dumps({
        'oracle': 'pycrate 0.7.11', 'source_authority': authority,
        'claim': 'Frozen ASN.1 mandatory IE rows, not application procedure qualification',
        'cases': fixtures}, indent=2) + '\n')
    print('PASS regenerated six native-mandatory public consumer fixtures')


if __name__ == '__main__':
    main()
