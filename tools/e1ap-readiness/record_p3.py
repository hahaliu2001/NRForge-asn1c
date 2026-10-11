#!/usr/bin/env python3
"""Verify E1-P3 generation closure, guards and unchanged E1-P2 scan inputs."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess

def digest(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--asn1-root',type=Path,required=True)
    ap.add_argument('--generated',type=Path,required=True)
    ap.add_argument('--messages',type=Path,required=True)
    ap.add_argument('--work',type=Path,required=True)
    ap.add_argument('--output',type=Path,required=True)
    args=ap.parse_args();repo=Path.cwd().resolve();work=args.work.resolve();work.mkdir(exist_ok=False)
    root=args.asn1_root.resolve();generated=args.generated.resolve()
    report=repo/'tools/e1ap-readiness/readiness-e1-p2.json';prior=json.loads(report.read_text())
    inputs={k:digest(repo/k) for k in prior['input_sha256']}
    if inputs!=prior['input_sha256']:raise ValueError('E1-P2 scan input changed')
    for module in prior['source_authority']['modules']:
        path=root/module['path']
        if digest(path)!=module['sha256']:raise ValueError('frozen E1AP source changed')
    manifest=json.loads((generated/'manifest.json').read_text())
    if (manifest['message_count']!=72 or len(manifest['messages'])!=72
        or manifest['schema']['profile']!='e1ap' or not manifest['parser_deleted']
        or not manifest['deterministic']):raise ValueError('invalid E1AP manifest')
    base=[str(repo/'tools/asn1typed_ngap_dispatch'),str(repo/'tools/e1ap-readiness/e1ap-rel18.modules'),str(root)]
    repeat=work/'repeat';repeat.mkdir()
    subprocess.run([*base,str(args.messages.resolve()),str(repeat),'--e1ap'],check=True)
    a={str(p.relative_to(generated)):digest(p) for p in generated.rglob('*') if p.is_file() and p.name!='manifest.json'}
    b={str(p.relative_to(repeat)):digest(p) for p in repeat.rglob('*') if p.is_file() and p.name!='manifest.json'}
    if a!=b:raise ValueError('repeat generation changed')
    repeated=json.loads((repeat/'manifest.json').read_text())
    x=dict(manifest);x.pop('source_inputs');repeated.pop('source_inputs')
    if x!=repeated:raise ValueError('repeat logical manifest changed')
    names=args.messages.read_text().splitlines();negative={}
    for name,selection,option in [('duplicate',names+[names[0]],'--e1ap'),('missing',names[:-1],'--e1ap'),
                                  ('extra',names+['UndeclaredMessage'],'--e1ap'),('wrong_profile',names,'--f1ap')]:
        target=work/name;target.mkdir();listing=work/(name+'.txt');listing.write_text('\n'.join(selection)+'\n')
        result=subprocess.run([*base,str(listing),str(target),option],capture_output=True,text=True)
        (work/(name+'.log')).write_text(result.stdout+result.stderr)
        if result.returncode==0 or (target/'manifest.json').exists():raise ValueError('negative guard accepted '+name)
        negative[name]={'rc':result.returncode,'success_manifest_absent':True}
    tests={}
    for program in ['check_ngap_pdu','check_f1ap_pdu','check_e1ap_pdu']:
        path=repo/'libngap'/program;result=subprocess.run([str(path)],capture_output=True,text=True,check=True)
        tests[program]={'status':'PASS','sha256':digest(path),'stdout':result.stdout}
    source_paths=['tools/ngap-dispatch/generate.c','tools/asn1typed_ngap_dispatch','libngap/e1ap_pdu.cpp','libngap/e1ap_pdu.hpp',
                  'libngap/pdu_declarations.inc','libngap/pdu_implementation.inc','libngap/check_e1ap_pdu.cpp',
                  'tools/e1ap-readiness/check_dispatch.py','tools/e1ap-readiness/check_generator_regression.py',
                  'tools/e1ap-readiness/record_p3.py']
    receipt={'scope':'owned complete dispatch integration; no independent full-PDU wire qualification',
        'baseline_commit':'b255db5768c90260b6a5ba1bf98fc2e931a43e56',
        'message_count':72,'procedure_count':40,'role_counts':[40,20,12],
        'generated_files_sha256':a,'repeat_generation_byte_exact':True,'negative_guards':negative,'pdu_tests':tests,
        'input_sha256':{k:digest(repo/k) for k in source_paths},
        'prior_readiness':{'report_sha256':digest(report),'input_sha256':inputs,'summary':prior['summary'],
                          'reused_unchanged_inputs':True,'new_scan_run':False}}
    for k,wanted in inputs.items():
        if digest(repo/k)!=wanted:raise ValueError('input changed '+k)
    args.output.write_text(json.dumps(receipt,indent=2)+'\n')
if __name__=='__main__':main()
