#!/usr/bin/env python3
"""Export hash-locked standard modules as inputs, never invoke consumer code."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess

REPO=Path(__file__).resolve().parents[2]


def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--schema-repository',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
    a.output.mkdir(parents=True,exist_ok=False)
    rows=[]
    for profile,where in [('ngap','n11-envelope-qualification'),('f1ap','f1ap-readiness'),('e1ap','e1ap-readiness'),('rrc','rrc-readiness')]:
        manifest=json.loads((REPO/'tools'/where/'source-manifest.json').read_text())
        revision=manifest.get('authority_commit',manifest.get('commit'))
        for row in manifest['modules']:
            data=subprocess.check_output(['git','-C',str(a.schema_repository),'show',revision+':'+row.get('repository_path','src/'+row['path'])])
            blob=hashlib.sha1(b'blob '+str(len(data)).encode()+b'\0'+data).hexdigest()
            if hashlib.sha256(data).hexdigest()!=row['sha256'] or blob!=row['git_blob']:raise ValueError('Frozen module export mismatch: '+row['path'])
            dest=a.output/row['path'];dest.parent.mkdir(parents=True,exist_ok=True);dest.write_bytes(data)
            rows.append(dict(protocol=profile,path=row['path'],source_commit=revision,sha256=row['sha256']))
    (a.output/'schema-inputs.json').write_text(json.dumps(rows,indent=2)+'\n')


if __name__=='__main__':main()
