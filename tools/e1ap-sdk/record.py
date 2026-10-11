#!/usr/bin/env python3
"""Reconcile actual successful build, installed consumers and negative evidence."""
import argparse
import base64
import gzip
from collections import Counter
import hashlib
import json
from pathlib import Path
import re
import subprocess


def sha(path):
    h = hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda: stream.read(1<<20),b''): h.update(block)
    return h.hexdigest()


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    for name in ('repo','work','prefix','isolation','seal-negatives','reproducibility','regression','output'):
        ap.add_argument('--'+name,type=Path,required=True)
    ap.add_argument('--functional-log', type=Path, action='append', required=True)
    args = ap.parse_args()
    gates = []
    for log in args.functional_log:
        content = log.read_text()
        totals = re.findall(r'# TOTAL:\s*(\d+)', content)
        passed = re.findall(r'# PASS:\s*(\d+)', content)
        failed = re.findall(r'# (?:FAIL|ERROR):\s*(\d+)', content)
        if not totals or not passed or int(totals[-1]) != int(passed[-1]) or any(int(x) for x in failed[-2:]):
            raise ValueError('functional gate not successful: '+str(log))
        gates.append({'log':log.name,'sha256':sha(log),'total':int(totals[-1]),'pass':int(passed[-1])})
    if sorted(g['total'] for g in gates) != [3,10,42]:
        raise ValueError('require exact libngap/libaper/libasn1typed functional logs')
    generated, build = args.work/'generated', args.work/'build'
    archive = build/'libnrforge_e1ap.a'
    provenance = json.loads((generated/'sdk-provenance.json').read_text())
    manifest = json.loads((generated/'manifest.json').read_text())
    isolation = json.loads((args.isolation/'isolation-summary.json').read_text())
    negatives = json.loads((args.seal_negatives/'summary.json').read_text())
    if any(x['status'] != 'PASS' for x in (isolation,negatives,isolation['consumer'])):
        raise ValueError('missing successful verification')
    if manifest['message_count'] != 72 or len(manifest['schema']['procedures']) != 40:
        raise ValueError('incomplete registry')
    if isolation['consumer']['all_installed_slots'] != 72 or isolation['consumer']['protocol_core_coexist_orders'] != ['e1ap-first','e1ap-last']:
        raise ValueError('incomplete consumer coverage')
    if isolation['consumer']['sdk_fingerprint'] != provenance['fingerprint'] or isolation['consumer']['archive_sha256'] != sha(archive):
        raise ValueError('consumer evidence belongs to another SDK/archive')
    if isolation['consumer'].get('wrong_protocol_core_archive_rejected') != ['ngap','f1ap'] or isolation['consumer'].get('consumer_outcomes') != 6:
        raise ValueError('missing wrong-protocol negatives or populated Setup outcomes')
    expected_cases = ['unmodified-snapshot','extra-generated-file','missing-generated-file',
                      'altered-generated-header','altered-production-source',
                      'missing-archive-receipt','altered-archive']
    if negatives.get('sdk_fingerprint') != provenance['fingerprint'] or negatives.get('archive_sha256') != sha(archive):
        raise ValueError('seal negatives belong to another SDK/archive')
    if [c['case'] for c in negatives['cases']] != expected_cases or any(
        c['status'] != 'PASS' or ((c['returncode'] == 0) != (index == 0))
        for index,c in enumerate(negatives['cases'])):
        raise ValueError('incomplete seal negative coverage')
    historical_transport = json.loads((args.repo/'tools/e1ap-wire-qualification/accepted-profile.json').read_text())
    historical_bytes = gzip.decompress(base64.b64decode(historical_transport['data'],validate=True))
    if hashlib.sha256(historical_bytes).hexdigest()!=historical_transport['profile_sha256']:
        raise ValueError('historical profile transport mismatch')
    historical = json.loads(historical_bytes)
    unchanged = {name:value for name,value in historical['generated_sha256'].items()
                 if name not in {'CMakeLists.txt','manifest.json'}}
    if {name for name in provenance['generated_inputs'] if name not in {'CMakeLists.txt','manifest.json'}} != set(unchanged):
        raise ValueError('qualified production output closure changed')
    for name,expected in unchanged.items():
        path = (generated/name).resolve()
        if not path.is_relative_to(generated.resolve()) or sha(path)!=expected:
            raise ValueError('qualified generated production output changed: '+name)
    receipt = (build/'sdk-build-receipt.cmake').read_text()
    if sha(archive) not in receipt or provenance['fingerprint'] not in receipt:
        raise ValueError('archive receipt mismatch')
    subprocess.run([str(build/'nrforge_e1ap_sdk_link_check')],check=True)
    members = subprocess.check_output(['ar','t',str(archive)],text=True).splitlines()
    if len(members)!=76 or len(set(members))!=76:
        raise ValueError('archive member closure mismatch')
    # Examine every real object member without extracting the large archive.
    elf_count = 0
    with archive.open('rb') as stream:
        if stream.read(8)!=b'!<arch>\n': raise ValueError('not a regular archive')
        while True:
            header = stream.read(60)
            if not header: break
            if len(header)!=60 or header[58:60]!=b'`\n': raise ValueError('bad archive member')
            size = int(header[48:58])
            start = stream.tell()
            if stream.read(min(size,4))==b'\x7fELF': elf_count+=1
            stream.seek(start+size+(size%2))
    if elf_count!=76: raise ValueError('incomplete ELF production archive')
    registrations = Counter()
    process = subprocess.Popen(['nm','-C','-g','--defined-only',str(archive)],stdout=subprocess.PIPE,text=True)
    for line in process.stdout:
        match = re.search(r'\bT nrforge::e1ap::generated::registration_(\d{3})\(\)',line)
        if match: registrations[int(match.group(1))]+=1
    if process.wait()!=0 or registrations != Counter({i:1 for i in range(72)}):
        raise ValueError('registration symbol closure mismatch')
    fixtures = json.loads((args.repo/'tools/e1ap-sdk-consumer/native-fixtures.json').read_text())
    if len(fixtures['cases']) != 6 or len({c['message'] for c in fixtures['cases']}) != 6:
        raise ValueError('both Setup direction fixture closure required')
    accepted_messages = {m['message']: m for m in historical['identities']}
    for case in fixtures['cases']:
        old = next(c for c in accepted_messages[case['message']]['cases'] if c['id'] == case['id'])
        if hashlib.sha256(bytes.fromhex(case['pdu_hex'])).hexdigest() != old['wire_sha256']:
            raise ValueError('fixture differs from accepted wire bytes')
    files = {str(p.relative_to(args.prefix)):sha(p) for p in sorted(args.prefix.rglob('*')) if p.is_file()}
    expected_installed = {str(Path(name).relative_to(args.prefix.resolve()))
                          for name in (build/'install_manifest.txt').read_text().splitlines()}
    if set(files) != expected_installed:
        raise ValueError('installed file set differs from successful install manifest')
    installed_archives = list(args.prefix.rglob('libnrforge_e1ap.a'))
    if len(installed_archives)!=1 or sha(installed_archives[0])!=sha(archive):
        raise ValueError('installed archive differs from qualified build')
    installed_provenance = json.loads((args.prefix/'share/nrforge-e1ap/sdk-provenance.json').read_text())
    if installed_provenance != provenance: raise ValueError('installed provenance differs')
    if any(p.endswith(('_codec.hpp','_mapping.hpp')) for p in files):
        raise ValueError('private codec/mapping header installed')
    logs = {}
    for folder in (args.work,args.isolation,args.seal_negatives):
        for p in sorted(folder.rglob('*.log')):
            logs[str(p.relative_to(folder))+'@'+folder.name]=sha(p)
    reproducibility = json.loads(args.reproducibility.read_text())
    regression = json.loads(args.regression.read_text())
    if reproducibility.get('status') != 'PASS' or reproducibility.get('sdk_fingerprint') != provenance['fingerprint']:
        raise ValueError('missing same-identity generation reproducibility')
    if {p: d['messages'] for p,d in regression['profiles'].items()} != {'ngap':131,'f1ap':158}:
        raise ValueError('missing complete historical generator regression')
    core_archives = {}
    for profile in ('ngap','f1ap'):
        path = args.isolation/'wrong-archives'/(profile+'.a')
        symbols = subprocess.check_output(['nm','-C','-g','--defined-only',str(path)],text=True)
        if 'nrforge::'+profile+'::Registry::create(' not in symbols or 'nrforge::e1ap::' in symbols:
            raise ValueError('wrong-protocol archive is not a distinct real production core')
        core_archives[profile] = {'archive_sha256':sha(path),'defined_symbols_sha256':hashlib.sha256(symbols.encode()).hexdigest(),
                                 'members':subprocess.check_output(['ar','t',str(path)],text=True).splitlines()}
    record = {'status':'PASS','baseline_commit':provenance['source_revision'],
        'sdk_identity':{key:provenance[key] for key in ('version','fingerprint','schema_sha256','runtime_sha256','source_revision')},
        'registry':{'messages':72,'procedures':40,'registration_definitions':72,'production_elf_members':elf_count},
        'archive_sha256':sha(archive),'archive_receipt_sha256':sha(build/'sdk-build-receipt.cmake'),
        'consumer_evidence':isolation,'seal_negative_evidence':negatives,
        'functional_gates':gates,
        'wrong_protocol_core_archives':core_archives,
        'reproducibility_sha256':sha(args.reproducibility),
        'regression_sha256':sha(args.regression),
        'generator_regression':{p:{k:v for k,v in d.items() if k!='files_sha256'} for p,d in regression['profiles'].items()},
        'reproducible_generated_file_count':reproducibility['byte_exact_files'],
        'native_outcomes':[{'message':c['message'],'octets':len(bytes.fromhex(c['pdu_hex'])),
                            'wire_sha256':hashlib.sha256(bytes.fromhex(c['pdu_hex'])).hexdigest()} for c in fixtures['cases']],
        'installed_files':files,'logs_sha256':logs,
        'source_sha256':provenance['source_hashes'],
        'qualified_production_output_count_unchanged':len(unchanged),
        'limits':['E1-P4 finite qualification preserved, not replayed','O0 strict C++20 source SDK; no binary ABI promise',
                  'No Python SDK, benchmark or live vendor qualification','NGAP/F1AP development-core coexistence only; no historical complete SDK rebuild']}
    args.output.write_text(json.dumps(record,indent=2,sort_keys=True)+'\n')
    print('PASS reconciled complete installed E1AP SDK evidence')


if __name__ == '__main__':
    main()
