#!/usr/bin/env python3
"""Attest unchanged production and retained prior protocol evidence for E1-P4."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess

def digest(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--output',type=Path,required=True)
    args=ap.parse_args();repo=Path.cwd()
    baseline='4ef7a5ebbd5ca4f902beddd1a967459a9e060659'
    changed=subprocess.check_output(['git','diff',baseline,'--name-only'],text=True).splitlines()
    if any(p.startswith(('libaper/','libasn1typed/','libngap/','tools/ngap-dispatch/')) for p in changed):
        raise ValueError('shared production changed; broader regression required')
    generation=Path('tools/e1ap-readiness/generator-regression-e1-p3.json');r=json.loads(generation.read_text())
    for name,wanted in {**r['input_sha256'],**r['frozen_source_sha256']}.items():
        if digest(Path(name))!=wanted:raise ValueError('prior generator input changed: '+name)
    retained=[generation,Path('tools/e1ap-readiness/verification-e1-p3.json'),
              Path('tools/f1ap-wire-qualification/accepted-profile.json'),Path('tools/f1ap-wire-qualification/qualification-summary.json'),
              Path('tools/unified-ngap-qualification/accepted-profile.json'),Path('tools/unified-ngap-qualification/qualification-summary.json')]
    hashes={}
    for p in retained:
        raw=subprocess.check_output(['git','show',baseline+':'+str(p)])
        if p.read_bytes()!=raw:raise ValueError('historical evidence changed '+str(p))
        hashes[str(p)]=digest(p)
    tests={}
    for name in ['check_ngap_pdu','check_f1ap_pdu','check_e1ap_pdu']:
        p=repo/'libngap'/name
        result=subprocess.run([str(p.resolve())],capture_output=True,text=True,check=True)
        tests[name]={'status':'PASS','sha256':digest(p),'stdout':result.stdout}
    args.output.write_text(json.dumps({'scope':'unchanged production and retained generator/wire evidence; no new NGAP/F1AP wire campaign',
       'baseline_commit':baseline,'production_changes':False,'retained_sha256':hashes,
       'prior_full_generator_regression':{'ngap':{'messages':131,'byte_exact_files':658},'f1ap':{'messages':158,'byte_exact_files':793},
           'all_retained_input_fingerprints_match':True},'pdu_tests':tests},indent=2)+'\n')
if __name__=='__main__':main()
