#!/usr/bin/env python3
"""Compare shared NGAP/F1AP binding generation against the accepted baseline."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import tempfile
BASELINE='4d7e80b9e13c73867463d428632e6eed7fe0da25'
def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--ngap-sdk',type=Path,required=True)
    ap.add_argument('--f1ap-sdk',type=Path,required=True)
    ap.add_argument('--output',type=Path,required=True)
    args=ap.parse_args();repo=Path(__file__).resolve().parents[2];result={}
    with tempfile.TemporaryDirectory(prefix='e1-python-regression-') as tmp:
        work=Path(tmp);baseline=work/'baseline.py'
        baseline.write_bytes(subprocess.check_output(['git','show',BASELINE+':python/ngap_sdk/generate_bindings.py'],cwd=repo))
        for profile,prefix in (('ngap',args.ngap_sdk),('f1ap',args.f1ap_sdk)):
            snapshots=[]
            for label,generator in (('baseline',baseline),('current',repo/'python/ngap_sdk/generate_bindings.py')):
                output=work/(profile+'-'+label)
                subprocess.run([sys.executable,str(generator),'--profile',profile,'--sdk-prefix',str(prefix.resolve()),'--output',str(output)],check=True,capture_output=True,text=True)
                snapshots.append({str(p.relative_to(output)):hashlib.sha256(p.read_bytes()).hexdigest() for p in output.rglob('*') if p.is_file()})
            if snapshots[0]!=snapshots[1]:raise RuntimeError(profile+' binding regression')
            result[profile]={'status':'PASS','unchanged_files':len(snapshots[0]),'sha256':snapshots[0]}
    args.output.write_text(json.dumps(result,indent=2)+'\n');print('PASS unchanged NGAP/F1AP generated bindings')
if __name__=='__main__':main()
