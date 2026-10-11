#!/usr/bin/env python3
"""Regenerate every accepted NGAP/F1AP BODY and compare historical header bytes."""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import subprocess
import sys
sys.dont_write_bytecode = True

def digest(p):
    return hashlib.sha256(p.read_bytes()).hexdigest()

def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--asn1-root', type=Path, required=True)
    ap.add_argument('--work', type=Path, required=True)
    ap.add_argument('--output', type=Path, required=True)
    ap.add_argument('--probe', type=Path, default=Path('tools/asn1typed_codec_coverage'))
    ap.add_argument('--cxx', default='/usr/bin/g++')
    args = ap.parse_args()
    repo = Path.cwd().resolve(); root = args.asn1_root.resolve(); work = args.work.resolve()
    work.mkdir(parents=True, exist_ok=False)
    inputs = [args.probe.resolve(), Path(__file__).resolve(), repo/'tools/asn1typed_codec_coverage.c',
              *sorted((repo/'libasn1typed').glob('asn1typed*.c')), *sorted((repo/'libasn1typed').glob('asn1typed*.h')),
              *sorted((repo/'libaper').glob('*.hpp')), repo/'libaper/runtime.cpp']
    profiles = [('f1ap','F1AP','tools/f1ap-readiness/readiness-f1-p3.json','tools/f1ap-readiness/f1ap-rel18.modules'),
                ('ngap','NGAP','tools/f1ap-readiness/ngap-regression-f1-p3.json','tools/qualification/ngap-rel18.modules')]
    inputs += [repo/path for _,_,baseline,modules in profiles for path in [baseline,modules]]
    fingerprints = {str(p.relative_to(repo)):digest(p) for p in inputs}
    spec = importlib.util.spec_from_file_location('guard',repo/'tools/n12-codec-coverage/run.py')
    guard = importlib.util.module_from_spec(spec); spec.loader.exec_module(guard)
    results = {}
    for profile,prefix,baseline_path,modules in profiles:
        baseline = json.loads((repo/baseline_path).read_text())
        # Accepted module identities are checked independently of source checkout state.
        for module in baseline['source_authority']['modules']:
            data = (root/module['path']).read_bytes()
            assert hashlib.sha256(data).hexdigest() == module['sha256']
            assert hashlib.sha1(b'blob '+str(len(data)).encode()+b'\0'+data).hexdigest() == module['git_blob']
        directory = work/profile; directory.mkdir(); headers=directory/'headers'; headers.mkdir()
        messages=directory/'messages.txt'; messages.write_text(''.join(r['message']+'\n' for r in baseline['messages']))
        with (directory/'probe.log').open('w') as log:
            run=subprocess.run([str(args.probe.resolve()),str(repo/modules),str(root),str(messages),str(headers),prefix+'-PDU-Contents',prefix+'-PDU-Descriptions',prefix+'-PDU','--envelopes'],text=True,stdout=subprocess.PIPE,stderr=log,check=True)
        raw=guard.validate(json.loads(run.stdout),messages)
        assert raw['parse']=='PASS' and raw['fix']=='PASS' and raw['parser_deleted_before_generation'] is True
        (directory/'raw.json').write_text(json.dumps(raw)+'\n')
        compared=0; targeted=[]
        targets = {'F1SetupRequest','F1SetupResponse','F1SetupFailure','UEContextSetupRequest','PrivateMessage'} if profile=='f1ap' else {'NGSetupRequest','NGSetupResponse','NGSetupFailure','InitialContextSetupRequest','PrivateMessage'}
        for i,(before,after) in enumerate(zip(baseline['messages'],raw['messages'],strict=True)):
            assert before['message']==after['message'] and after['physical_extraction_rc']==0 and after['envelope_extraction_rc']==0
            assert [g['family'] for g in after['generation']]==[0,1,2] and all(g['rc']==0 for g in after['generation'])
            assert [g['family'] for g in after['envelope_generation']]==[0,1,2] and all(g['rc']==0 for g in after['envelope_generation'])
            families=['types','mapping','codec']
            for family,wanted in before['compile']['header_sha256'].items():
                assert digest(headers/f'{i:03d}_{family}.hpp')==wanted,(profile,before['message'],family)
                compared+=1
            if profile=='f1ap':
                for family,wanted in before['envelope_compile']['header_sha256'].items():
                    assert digest(headers/f'{i:03d}_{family}.hpp')==wanted,(profile,before['message'],family)
                    compared+=1
            if before['message'] in targets:
                families+=['envelope_types','envelope_mapping','envelope_codec']
                tu=headers/f'{i:03d}_check.cpp'; tu.write_text('#include "runtime.hpp"\n#include "sequence_extensions.hpp"\n'+''.join(f'#include "{i:03d}_{f}.hpp"\n' for f in families))
                result=subprocess.run([args.cxx,'-std=c++20','-Wall','-Wextra','-Werror','-pedantic-errors','-Wconversion','-Wsign-conversion','-DNDEBUG','-I'+str(repo/'libaper'),'-fsyntax-only',str(tu)],text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
                (directory/f'{i:03d}-compile.log').write_text(result.stdout)
                assert result.returncode==0,result.stdout
                targeted.append(before['message'])
        assert set(targeted)==targets
        # Unchanged accepted runtime plus identical generated bytes preserves the old compile inputs.
        for key in ['libaper/runtime.hpp','libaper/sequence_extensions.hpp']:
            assert digest(repo/key)==baseline['input_sha256'][key]
        for module in baseline['source_authority']['modules']:
            assert digest(root/module['path'])==module['sha256']
        results[profile]={'messages':len(raw['messages']),'physical_generation_and_descriptor':'PASS',
            'body_and_envelope_generation':'PASS','historical_headers_byte_exact':compared,
            'targeted_strict_compile':targeted,'historical_runtime_headers_unchanged':True}
        print(profile,results[profile],flush=True)
    for key,wanted in fingerprints.items(): assert digest(repo/key)==wanted,key
    args.output.write_text(json.dumps({'scope':'all-message regeneration and historical-header byte equality with targeted strict compilation; no new wire qualification',
        'baseline_commit':subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip(),
        'input_sha256':fingerprints,'compiler':subprocess.check_output([args.cxx,'--version'],text=True).splitlines()[0],
        'profiles':results,'limits':['NGAP historical baseline retains BODY hashes only; new target-envelope generation is checked but no historical envelope byte-equality claim.',
            'Full SDK rebuild, all-message runtime/native-wire corpus and interoperability are not rerun.']},indent=2)+'\n')
if __name__=='__main__': main()
