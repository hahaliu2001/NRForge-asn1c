#!/usr/bin/env python3
"""Untouched six-module native qualification of the generated command BODY."""
import argparse
from collections import Counter
import hashlib
import importlib.metadata
import importlib.util
import json
from pathlib import Path
import subprocess
import tempfile

p = argparse.ArgumentParser()
p.add_argument('--repo', type=Path, required=True)
p.add_argument('--asn1-root', type=Path, required=True)
p.add_argument('--probe', type=Path, required=True)
p.add_argument('--cxx', default='g++')
p.add_argument('--work', type=Path)
p.add_argument('--output', type=Path, required=True)
a = p.parse_args()
source = Path(__file__).resolve().parent
repo, root = a.repo.resolve(), a.asn1_root.resolve()
work = a.work.resolve() if a.work else Path(tempfile.mkdtemp(prefix='n10-body-'))
work.mkdir(parents=True, exist_ok=True)
version = importlib.metadata.version('pycrate')
if version != '0.7.11': raise SystemExit('pycrate must be pinned to 0.7.11')
manifest = json.loads((source / 'source-manifest.json').read_text())
ordered = [x.strip() for x in (repo / 'tools/qualification/ngap-rel18.modules').read_text().splitlines() if x.strip() and not x.startswith('#')]
if ordered != [x['path'] for x in manifest['modules']]: raise SystemExit('Ordered six-module provenance changed')
texts = []
for item in manifest['modules']:
    data = (root / item['path']).read_bytes()
    blob = hashlib.sha1(b'blob ' + str(len(data)).encode() + b'\0' + data).hexdigest()
    if blob != item['git_blob'] or hashlib.sha256(data).hexdigest() != item['sha256']:
        raise SystemExit('Frozen source identity mismatch: ' + item['path'])
    texts.append(data.decode())

generated = work / 'generated'; generated.mkdir(exist_ok=True)
subprocess.run([str(a.probe.resolve()), '--module-list', str(repo / 'tools/qualification/ngap-rel18.modules'), '--asn1-root', str(root), '--root-module', 'NGAP-PDU-Contents', '--message', 'UEContextReleaseCommand', '--namespace', 'n10::body', '--output-prefix', str(generated / 'body'), '--verify-determinism'], check=True)
driver = work / 'driver'
integration_inputs = {'driver.cpp': source / 'driver.cpp', 'body_adapter.hpp': source / 'body_adapter.hpp', 'libaper/runtime.cpp': repo / 'libaper/runtime.cpp', 'libaper/runtime.hpp': repo / 'libaper/runtime.hpp', 'libaper/sequence_extensions.hpp': repo / 'libaper/sequence_extensions.hpp'}
integration_hashes = {name: hashlib.sha256(path.read_bytes()).hexdigest() for name, path in integration_inputs.items()}
subprocess.run([a.cxx, '-std=c++20', '-Wall', '-Wextra', '-Werror', '-pedantic-errors', '-Wconversion', '-Wsign-conversion', '-DNDEBUG', '-I' + str(repo / 'libaper'), '-I' + str(source), '-I' + str(generated), str(source / 'driver.cpp'), str(repo / 'libaper/runtime.cpp'), '-o', str(driver)], check=True)
if integration_hashes != {name: hashlib.sha256(path.read_bytes()).hexdigest() for name, path in integration_inputs.items()}:
    raise SystemExit('Integration inputs changed during compilation; no qualification claim')
subprocess.run([str(driver), 'self-test'], check=True)
generated_hashes = {name: hashlib.sha256((generated / name).read_bytes()).hexdigest() for name in ('body_types.hpp','body_mapping.hpp','body_codec.hpp')}

from pycrate_asn1c.asnproc import compile_text
from pycrate_asn1c.generator import PycrateGenerator
compile_text(texts)  # Every exact authoritative declaration; no installed schema or filtering.
PycrateGenerator(str(work / 'native_oracle.py'))
spec = importlib.util.spec_from_file_location('n10_exact_native_oracle', work / 'native_oracle.py')
native = importlib.util.module_from_spec(spec); spec.loader.exec_module(native)
body = native.NGAP_PDU_Contents.UEContextReleaseCommand
ies = native.NGAP_IEs
branches = {'radio_network': ('radioNetwork', 'CauseRadioNetwork'), 'transport': ('transport', 'CauseTransport'), 'nas': ('nas', 'CauseNas'), 'protocol': ('protocol', 'CauseProtocol'), 'misc': ('misc', 'CauseMisc')}
policies = ('reject', 'ignore', 'notify')

def run(mode, value):
    return subprocess.check_output([str(driver), mode, '-'], input=value+'\n', text=True).strip()

def sequence(text):
    if text == '-': return {}
    width, rows = text.split('/', 1)
    # Native unknown SEQUENCE suffix handling is inconsistent. Do not normalize it.
    raise ValueError('Opaque sequence sidecars use separate literal rejection evidence')

def to_native(model):
    seq, entries = model.split('|', 1)
    value = {'protocolIEs': []}; value.update(sequence(seq))
    for record in entries.split(';'):
        if not record: continue
        fields = record.split(':'); ident, criticality = int(fields[0]), int(fields[1]); kind = fields[2]
        if kind == 'pair':
            pair = {'aMF-UE-NGAP-ID': int(fields[3]), 'rAN-UE-NGAP-ID': int(fields[4])}
            if fields[5] != '-':
                pair['iE-Extensions'] = [{'id': int(r.split(',')[0]), 'criticality': policies[int(r.split(',')[1])], 'extensionValue': ('_unk_004', bytes.fromhex(r.split(',')[2]))} for r in fields[5].split('+')]
            pair.update(sequence(fields[6])); payload = ('UE-NGAP-IDs', ('uE-NGAP-ID-pair', pair))
        elif kind == 'amf': payload = ('UE-NGAP-IDs', ('aMF-UE-NGAP-ID', int(fields[3])))
        elif kind in ('ue_ext', 'cause_ext'):
            extension = {'id': int(fields[3]), 'criticality': policies[int(fields[4])], 'value': ('_unk_004', bytes.fromhex(fields[5]))}
            payload = ('UE-NGAP-IDs' if kind == 'ue_ext' else 'Cause', ('choice-Extensions', extension))
        elif kind == 'cause':
            branch, typename = branches[fields[3]]; enum = getattr(ies, typename)
            label = '_ext_' + fields[5] if fields[4] == 'x' else next(k for k, n in enum._cont.items() if n == int(fields[5]))
            payload = ('Cause', (branch, label))
        elif kind == 'unknown': payload = ('_unk_004', bytes.fromhex(fields[3]))
        else: raise ValueError(kind)
        value['protocolIEs'].append({'id': ident, 'criticality': policies[criticality], 'value': payload})
    return value

cases = []
base_ids = '114:0:pair:1:2:-:-'
base_cause = '15:1:cause:radio_network:k:0'
def add(name, records, opaque=False): cases.append((name, '-|' + records, opaque))
for key, (_, typename) in branches.items():
    enum = getattr(ies, typename)
    for label, number in enum._cont.items(): add('known-'+typename+'-'+label, base_ids+f';15:1:cause:{key}:k:{number}')
    for index in sorted(set((len(enum._ext or []),63,64,255,256))): add('unknown-'+typename+'-'+str(index), base_ids+f';15:1:cause:{key}:x:{index}')
amfs = (0,1,255,256,65535,65536,16777215,16777216,4294967295,4294967296,1099511627775)
rans = (0,1,255,256,65535,65536,16777215,16777216,4294967295)
for amf in amfs:
    add('amf-'+str(amf),f'114:0:amf:{amf};'+base_cause)
    for ran in rans: add(f'pair-{amf}-{ran}',f'114:0:pair:{amf}:{ran}:-:-;'+base_cause)
for left in range(3):
    for right in range(3): add(f'received-criticality-{left}-{right}',f'114:{left}:pair:1:2:-:-;15:{right}:cause:nas:k:0')
for name, entries in [('empty',''),('missing-cause',base_ids),('missing-ids',base_cause),('reordered',base_cause+';'+base_ids),('duplicate-ids',base_ids+';'+base_ids+';'+base_cause),('duplicate-cause',base_ids+';'+base_cause+';'+base_cause)]: add(name,entries)
for ident in (0,65535):
    for crit in range(3):
        for size in (1,3,127,128,16384,65536):
            payload = bytes((i*17+128)%256 for i in range(size)).hex()
            add(f'main-unknown-{ident}-{crit}-{size}',base_ids+';'+base_cause+f';{ident}:{crit}:unknown:{payload}',True)
            add(f'ue-choice-unknown-{ident}-{crit}-{size}',f'114:0:ue_ext:{ident}:{crit}:{payload};'+base_cause,True)
            add(f'cause-choice-unknown-{ident}-{crit}-{size}',base_ids+f';15:1:cause_ext:{ident}:{crit}:{payload}',True)
            add(f'pair-extension-unknown-{ident}-{crit}-{size}',f'114:0:pair:1:2:{ident},{crit},{payload}:-;'+base_cause,True)
limitations = []
for member in ('_ext_0', '_ext_2'):
    supplied = {'protocolIEs': [], member: b'\x80'}
    try:
        body.set_val(supplied); probe_wire = body.to_aper(); body.from_aper(probe_wire)
        observed = [k for k in body.get_val() if k.startswith('_ext_')]
        limitations.append({'case': 'native-sequence-suffix-' + member, 'input_unknown_member': member, 'input_payload_hex': '80', 'wire_hex': probe_wire.hex(), 'sha256': hashlib.sha256(probe_wire).hexdigest(), 'observed_unknown_members': observed})
    except Exception as exc:
        limitations.append({'case': 'native-sequence-suffix-' + member, 'input_unknown_member': member, 'input_payload_hex': '80', 'exception_type': type(exc).__name__, 'exception_message': str(exc)})
profile = {'pycrate_version': version, 'source_manifest': manifest, 'generated_sha256': generated_hashes, 'oracle_limitations': limitations, 'matches': [], 'mismatches': []}
for case, model, opaque in cases:
    value = to_native(model); body.set_val(value); wire = body.to_aper()
    if opaque:
        try:
            body.from_aper(wire)
            if body.get_val() != value: raise ValueError('native body self-decode changed the supplied value')
        except Exception as exc:
            # Verify untouched native sender bytes by independent aligned frame
            # concatenation, then standalone native child comparison/decode.
            if wire[0] != 0 or int.from_bytes(wire[1:3], 'big') != len(value['protocolIEs']) or int.from_bytes(wire[3:5], 'big') != 114 or wire[5] != 0:
                raise ValueError('native fragment investigation header/count/first ID/criticality changed')
            position = 6; chunks = []; fragmented = False
            while True:
                header = wire[position]; position += 1
                if header < 128: length, final = header, True
                elif header < 192:
                    length = ((header & 63) << 8) | wire[position]; position += 1; final = True
                    if length < 128: raise ValueError('nonminimal native long determinant')
                else:
                    if not 1 <= (header & 63) <= 4: raise ValueError('noncanonical native fragment multiplier')
                    length, final = (header & 63) * 16384, False; fragmented = True
                if final and (length >= 16384 or (length == 0 and not fragmented)): raise ValueError('noncanonical native final determinant')
                part = wire[position:position+length]
                if len(part) != length: raise ValueError('native frame content truncated')
                chunks.append(part); position += length
                if final: break
            child = b''.join(chunks)
            if not child: raise ValueError('zero-octet known open payload')
            if not fragmented or len(child) < 16384: raise ValueError('unexpected nonfragmented native oracle failure')
            first = value['protocolIEs'][0]['value']
            if first[0] != 'UE-NGAP-IDs': raise ValueError('unexpected native oracle failure shape')
            ies.UE_NGAP_IDs.set_val(first[1]); standalone = ies.UE_NGAP_IDs.to_aper()
            if child != standalone: raise ValueError('native body sender frame differs from native standalone child')
            ies.UE_NGAP_IDs.from_aper(child)
            if ies.UE_NGAP_IDs.get_val() != first[1]: raise ValueError('native standalone child did not preserve value')
            limitations.append({'case': case, 'classification': 'fragmented-known-open-native-body-self-decode', 'wire_octet_count': len(wire), 'wire_sha256': hashlib.sha256(wire).hexdigest(), 'child_octet_count': len(child), 'child_sha256': hashlib.sha256(child).hexdigest(), 'standalone_child_byte_identical': True, 'standalone_child_native_decode_preserved': True, 'exception_type': type(exc).__name__, 'exception_message': str(exc)})
    actual = run('decode-body', wire.hex())
    signature = {'case': case, 'octet_count': len(wire), 'sha256': hashlib.sha256(wire).hexdigest()}
    if actual != model: profile['mismatches'].append(dict(signature, operation='native_decode', actual=actual)); continue
    profile['matches'].append(dict(signature, operation='native_decode'))
    if opaque:
        result = run('refuse-body', wire.hex())
        if not result.startswith('error:constraint_violation:'): profile['mismatches'].append(dict(signature, operation='opaque_encode_refusal', actual=result))
        else: profile['matches'].append(dict(signature, operation='opaque_encode_refusal', actual=result))
    else:
        encoded = run('encode-body', model)
        if encoded != wire.hex(): profile['mismatches'].append(dict(signature, operation='native_encode', actual=encoded)); continue
        body.from_aper(bytes.fromhex(encoded))
        if body.get_val() != value: profile['mismatches'].append(dict(signature, operation='native_roundtrip', actual=repr(body.get_val()))); continue
        profile['matches'].append(dict(signature, operation='native_encode'))
# Independent bit trace: ext bit + zero count, normally-small width 2,
# bitmap 01, zero alignment, one-octet open length and payload 80.
for case, literal, wanted in (('body-sequence-opaque-literal', '80000002800180', '2/1,80|'),):
    actual = run('decode-body', literal); wire = bytes.fromhex(literal)
    item = {'case': case, 'octet_count': len(wire), 'sha256': hashlib.sha256(wire).hexdigest(), 'operation': 'receiver_literal'}
    if actual != wanted: profile['mismatches'].append(dict(item, actual=actual))
    else:
        profile['matches'].append(item)
        refusal = run('refuse-body', literal)
        if not refusal.startswith('error:constraint_violation:'): profile['mismatches'].append(dict(item, operation='receiver_literal_refusal', actual=refusal))
        else: profile['matches'].append(dict(item, operation='receiver_literal_refusal', actual=refusal))
for case, literal, wanted in (
    ('known-cause-inner-padding', '0000020072000400010002000f400141', 'error:nonzero_padding:127'),
    ('known-cause-inner-trailing', '0000020072000400010002000f40024000', 'error:trailing_data:128'),
    ('known-open-alignment', '0000020072200400010002000f400140', 'error:nonzero_padding:42'),
    ('body-trailing', '0000020072000400010002000f40014000', 'error:trailing_data:128'),
    ('known-cause-truncated', '0000020072000400010002000f4001', 'error:truncated_input:120'),
):
    actual = run('decode-body', literal); wire = bytes.fromhex(literal)
    item = {'case': case, 'octet_count': len(wire), 'sha256': hashlib.sha256(wire).hexdigest(), 'operation': 'receiver_literal_rejection', 'expected': wanted}
    if actual != wanted: profile['mismatches'].append(dict(item, actual=actual))
    else: profile['matches'].append(item)
if profile['mismatches']:
    (work/'actual-profile.json').write_text(json.dumps(profile,indent=2)+'\n')
    raise SystemExit('Unexpected generated/native disagreement; no PASS')
accepted = json.loads((source/'accepted-profile.json').read_text())
if profile != accepted:
    (work/'actual-profile.json').write_text(json.dumps(profile,indent=2)+'\n')
    raise SystemExit('Exact accepted profile changed; investigate before acceptance')
summary = {'pycrate_version':version,'frozen_modules_verified':len(texts),'integration_source_sha256':integration_hashes,'native_body_cases':len(cases),'matched_checks':len(profile['matches']),'operations':dict(Counter(x['operation'] for x in profile['matches'])),'oracle_limitations':len(limitations),'byte_normalization':False,'profile_verified':True,'production_disagreements_unresolved':0}
a.output.write_text(json.dumps(summary,indent=2)+'\n');print(json.dumps(summary))
