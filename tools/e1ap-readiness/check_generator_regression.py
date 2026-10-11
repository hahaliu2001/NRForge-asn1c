#!/usr/bin/env python3
"""Compare full NGAP/F1AP controller outputs against the E1-P3 baseline."""
import argparse
from concurrent.futures import ThreadPoolExecutor
import hashlib
import json
from pathlib import Path
import subprocess

def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--asn1-root',type=Path,required=True)
    ap.add_argument('--baseline-generator',type=Path,required=True)
    ap.add_argument('--work',type=Path,required=True)
    ap.add_argument('--output',type=Path,required=True)
    args=ap.parse_args()
    repo=Path.cwd().resolve(); root=args.asn1_root.resolve(); work=args.work.resolve()
    work.mkdir(parents=True,exist_ok=False)
    inputs=[repo/'tools/ngap-dispatch/generate.c',repo/'tools/asn1typed_ngap_dispatch',
            args.baseline_generator.resolve(),Path(__file__).resolve()]
    profiles=[('ngap','tools/f1ap-readiness/ngap-regression-f1-p3.json','tools/qualification/ngap-rel18.modules',[]),
              ('f1ap','tools/f1ap-readiness/readiness-f1-p3.json','tools/f1ap-readiness/f1ap-rel18.modules',['--f1ap'])]
    tasks=[]; sources={}
    for profile,report,modules,options in profiles:
        prior=json.loads((repo/report).read_text());inputs.extend([repo/report,repo/modules])
        for m in prior['source_authority']['modules']:
            path=root/m['path']
            if digest(path)!=m['sha256']:raise ValueError('source hash mismatch: '+str(path))
            sources[str(path)]=m['sha256']
        names=work/(profile+'-messages.txt');names.write_text(''.join(m['message']+'\n' for m in prior['messages']))
        for label,generator in [('baseline',args.baseline_generator.resolve()),('current',repo/'tools/asn1typed_ngap_dispatch')]:
            output=work/(profile+'-'+label);output.mkdir()
            tasks.append((profile,label,[str(generator),str(repo/modules),str(root),str(names),str(output),*options]))
    hashes={str(p):digest(p) for p in inputs}
    def run(task):
        profile,label,command=task
        with (work/(profile+'-'+label+'.log')).open('w') as log:
            subprocess.run(command,stdout=log,stderr=subprocess.STDOUT,check=True)
        print(profile,label,'generated',flush=True)
    with ThreadPoolExecutor(max_workers=4) as pool:list(pool.map(run,tasks))
    results={}
    for profile,_,_,_ in profiles:
        before=work/(profile+'-baseline');after=work/(profile+'-current')
        a={str(p.relative_to(before)):digest(p) for p in before.rglob('*') if p.is_file() and p.name!='manifest.json'}
        b={str(p.relative_to(after)):digest(p) for p in after.rglob('*') if p.is_file() and p.name!='manifest.json'}
        if a!=b:raise ValueError(profile+' generated output changed')
        x=json.loads((before/'manifest.json').read_text());y=json.loads((after/'manifest.json').read_text())
        x.pop('source_inputs');y.pop('source_inputs')
        if x!=y:raise ValueError(profile+' logical manifest changed')
        results[profile]={'messages':x['message_count'],'byte_exact_files':len(a),'logical_manifest_equal':True,'files_sha256':a}
    for path,wanted in {**hashes,**sources}.items():
        if digest(Path(path))!=wanted:raise ValueError('input changed: '+path)
    args.output.write_text(json.dumps({'scope':'full controller generation nonregression; no new wire campaign',
        'baseline_commit':'b255db5768c90260b6a5ba1bf98fc2e931a43e56',
        'input_sha256':hashes,'frozen_source_sha256':sources,'profiles':results},indent=2)+'\n')

if __name__=='__main__':main()
