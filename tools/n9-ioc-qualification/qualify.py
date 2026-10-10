#!/usr/bin/env python3
"""Compare actual generated IOC codecs with independent, pinned native senders."""
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
p.add_argument('--generated', type=Path)
p.add_argument('--render-driver', type=Path)
p.add_argument('--work', type=Path)
p.add_argument('--cxx', default='g++')
p.add_argument('--output', type=Path, required=True)
a = p.parse_args()
source = Path(__file__).resolve().parent
repo = a.repo.resolve()
work = a.work.resolve() if a.work else Path(tempfile.mkdtemp(prefix='n9-ioc-'))
work.mkdir(parents=True, exist_ok=True)
if bool(a.generated) == bool(a.render_driver):
    raise SystemExit('Specify exactly one of --generated or --render-driver')
generated = a.generated.resolve() if a.generated else work / 'generated'
if a.render_driver:
    generated.mkdir(exist_ok=True)
    for group, root in (('main','DispatchMessage'),('empty','EmptyMessage'),('closed','ClosedMessage'),('ext','ExtensionMessage'),('empty_ext','EmptyExtensionMessage')):
        subprocess.run([str(a.render_driver.resolve()), str(repo / 'libasn1typed/fixtures/ioc-generation-n9.asn1'), 'IOCCppGeneration', root, 'foo::nrforge::' + group, str(generated / group)], check=True)
versions = {'pycrate': '0.7.11', 'asn1tools': '0.167.0'}
for name, version in versions.items():
    if importlib.metadata.version(name) != version:
        raise SystemExit(f'{name} must be pinned to {version}')
subprocess.run([a.cxx, '-std=c++20', '-Wall', '-Wextra', '-Werror', '-pedantic',
                '-DROOT_KIND=0', '-I' + str(repo / 'libaper'), '-I' + str(generated),
                str(source / 'driver.cpp'), str(repo / 'libaper/runtime.cpp'),
                '-o', str(work / 'driver')], check=True)
from pycrate_asn1c.asnproc import compile_text
from pycrate_asn1c.generator import PycrateGenerator
import asn1tools
compile_text((source / 'sender.asn1').read_text())
PycrateGenerator(str(work / 'oracle.py'))
spec = importlib.util.spec_from_file_location('n9_oracle', work / 'oracle.py')
native = importlib.util.module_from_spec(spec)
spec.loader.exec_module(native)
native = native.IOCNativeSender
raw = asn1tools.compile_string((source / 'raw-framing.asn1').read_text(), 'per')

def run(mode, value, kind="dispatch"):
    return subprocess.check_output([str(work / 'driver'), mode, kind, '-'], input=value + '\n', text=True).strip()

def row(ident, policy, value):
    typ = {91: 'Flag', 42: 'Flag', 7: 'Word', 50: 'Pair', 80: 'WordList', 300: 'Pair', 301: 'Blob'}[ident]
    return {'number': ident, 'policy': policy, 'content': (typ, value)}

def payload(r):
    obj = getattr(native, r['content'][0]); obj.set_val(r['content'][1]); return obj.to_aper()

def expected(rows):
    out = ''
    for r in rows:
        ident = r['number']; policy = ('reject', 'ignore', 'notify').index(r['policy'])
        if ident in (91, 42): content = 'flag:' + str(int(r['content'][1]))
        elif ident == 7: content = 'word:' + str(r['content'][1])
        elif ident == 50:
            v = r['content'][1]; content = 'pair:' + str(v['value']) + ',' + (str(int(v['marker'])) if 'marker' in v else 'absent')
        elif ident == 80: content = 'words:' + ''.join(str(x) + ',' for x in r['content'][1])
        else: content = 'unknown:' + payload(r).hex()
        out += f'{ident}:{policy}:{content};'
    return out

cases = [('empty', [])]
for ident, values in ((91, (False, True)), (42, (False, True)), (7, (0, 1, 127, 255))):
    for policy in ('reject', 'ignore', 'notify'):
        for value in values:
            cases.append((f'id{ident}-{policy}-{int(value)}', [row(ident, policy, value)]))
for policy in ('reject', 'ignore', 'notify'):
    rows = [row(42, policy, True), row(7, policy, 255), row(91, policy, False), row(42, policy, False)]
    cases.append((f'duplicate-reordered-{policy}', rows))
    cases.append((f'unknown-compound-{policy}', [row(300, policy, {'value': 17, 'marker': False})]))
for policy in ('reject', 'ignore', 'notify'):
    for marker in (None, False, True):
        value = {'value': 17}
        if marker is not None: value['marker'] = marker
        cases.append((f'known-pair-{policy}-{marker}', [row(50, policy, value)]))
    for values in ([], [0], [0, 17], [0, 17, 255]):
        cases.append((f'known-words-{policy}-{len(values)}', [row(80, policy, values)]))
for size in (0, 1, 127, 128, 16383, 16384, 16385, 32768, 65536):
    cases.append((f'unknown-blob-{size}', [row(301, 'ignore', bytes((i * 17) % 256 for i in range(size)))]))
profile = {'versions': versions, 'matches': [], 'mismatches': []}
for case, rows in cases:
    native.DispatchMessage.set_val({'entries': rows})
    primary = native.DispatchMessage.to_aper()
    secondary = raw.encode('DispatchMessage', {'entries': [dict(number=r['number'], policy=r['policy'], content=payload(r)) for r in rows]})
    for oracle, wire in (('pycrate', primary), ('asn1tools_raw_framing', secondary)):
        actual = run('decode', wire.hex())
        item = {'case': case, 'oracle': oracle, 'octet_count': len(wire), 'sha256': hashlib.sha256(wire).hexdigest()}
        if actual == expected(rows): profile['matches'].append(item)
        else: profile['mismatches'].append(dict(item, actual=actual))
    if all(r['number'] in (91, 42, 7, 50, 80) for r in rows):
        def token(r):
            v = r['content'][1]
            if r['number'] == 50: v = str(v['value']) + ',' + (str(int(v['marker'])) if 'marker' in v else 'absent')
            elif r['number'] == 80: v = ','.join(str(x) for x in v)
            else: v = str(int(v))
            return f"{r['number']}:{('reject','ignore','notify').index(r['policy'])}:{v}"
        tokens = ';'.join(token(r) for r in rows)
        wire = bytes.fromhex(run('encode', tokens))
        native.DispatchMessage.from_aper(wire)
        item = {'case': case, 'oracle': 'generated_encode_both_oracles', 'octet_count': len(wire), 'sha256': hashlib.sha256(wire).hexdigest()}
        if wire == primary == secondary: profile['matches'].append(item)
        else: profile['mismatches'].append(dict(item, actual=wire.hex()))
# Unknown ownership: the driver overwrites physical input before inspecting owned records.
# Unknown re-encoding is intentionally forbidden, rather than silently forwarding bytes.
for case, rows in cases:
    if any(r['number'] not in (91, 42, 7, 50, 80) for r in rows):
        native.DispatchMessage.set_val({'entries': rows}); wire = native.DispatchMessage.to_aper()
        actual = run('refuse', wire.hex())
        item = {'case': case, 'oracle': 'unknown_encode_refusal', 'octet_count': len(wire), 'sha256': hashlib.sha256(wire).hexdigest()}
        (profile['matches'] if actual == 'error:1:24' else profile['mismatches']).append(item if actual == 'error:1:24' else dict(item, actual=actual))
for kind, typename, member, ext in (('empty', 'EmptyMessage', 'items', False), ('extension', 'ExtensionMessage', 'additions', True), ('empty_extension', 'EmptyExtensionMessage', 'additions', True), ('closed', 'ClosedMessage', 'entries', False)):
    for ident in (13, 300, 65535):
        for policy in ('reject', 'ignore', 'notify'):
            is_known = kind == 'extension' and ident == 13
            content = ('Flag', True) if is_known else ('_unk_004', b'\x80\x00\xff')
            entry = {'tag' if ext else 'number': ident, 'receivedPolicy' if ext else 'policy': policy, 'extensionContent' if ext else 'content': content}
            obj = getattr(native, typename); obj.set_val({member: [entry]}); wire = obj.to_aper()
            raw_entry = dict(entry); raw_entry['extensionContent' if ext else 'content'] = b'\x80' if is_known else content[1]
            secondary = raw.encode(typename, {member: [raw_entry]})
            wanted = 'error:1:34' if kind == 'closed' else f"{ident}:{('reject','ignore','notify').index(policy)}:" + ('flag:1;' if is_known else 'unknown:8000ff;')
            for oracle, value in (('pycrate', wire), ('asn1tools_raw_framing', secondary)):
                actual = run('decode', value.hex(), kind)
                item = {'case': f'{kind}-id{ident}-{policy}', 'oracle': oracle, 'octet_count': len(value), 'sha256': hashlib.sha256(value).hexdigest()}
                (profile['matches'] if actual == wanted else profile['mismatches']).append(item if actual == wanted else dict(item, actual=actual))
# Strict inner complete validation: independent literal vectors, not native lax decoder policy.
for case, wire, wanted in (('known-boolean-nonzero-padding', '000001005b000101', 'error:3:63'), ('known-boolean-trailing', '000001005b00028000', 'error:4:64'), ('known-word-trailing', '000001000700020102', 'error:4:64')):
    actual = run('decode', wire); value = bytes.fromhex(wire)
    item = {'case': case, 'oracle': 'strict_literal_rejection', 'octet_count': len(value), 'sha256': hashlib.sha256(value).hexdigest()}
    (profile['matches'] if actual == wanted else profile['mismatches']).append(item if actual == wanted else dict(item, actual=actual))
if profile['mismatches']:
    (work / 'actual-profile.json').write_text(json.dumps(profile, indent=2) + '\n')
    raise SystemExit('Unexpected native or strict-rejection disagreement; no PASS can be published')
accepted = json.loads((source / 'accepted-profile.json').read_text())
if profile != accepted:
    (work / 'actual-profile.json').write_text(json.dumps(profile, indent=2) + '\n')
    raise SystemExit('Exact oracle profile changed; investigate actual-profile.json before acceptance')
summary = {'versions': versions, 'byte_normalization': False, 'profile_verified': True,
           'production_disagreements_unresolved': 0, 'matched_checks': len(profile['matches']),
           'matched_checks_by_kind': dict(Counter(x['oracle'] for x in profile['matches'])),
           'known_oracle_disagreements': len(profile['mismatches'])}
a.output.write_text(json.dumps(summary, indent=2) + '\n')
print(json.dumps(summary))
