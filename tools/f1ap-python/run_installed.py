#!/usr/bin/env python3
"""Run F1AP and NGAP consumers outside repository under isolated Python."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import tempfile

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--python',required=True)
    ap.add_argument('--output',type=Path,required=True)
    args=ap.parse_args();repo=Path(__file__).resolve().parents[2]
    with tempfile.TemporaryDirectory(prefix='f1ap-python-installed-') as tmp:
        work=Path(tmp)
        for profile in ('f1ap','ngap'):
            tests=work/profile/'tests';tests.mkdir(parents=True)
            if profile=='f1ap':
                source=repo/'python/f1ap/tests'; names=('test_sdk.py','golden.json','f1-setup-native.json')
            else:
                source=repo/'python/tests';names=('test_sdk.py','golden-131.json','ng-setup-native.json')
                (tests.parent/'examples').mkdir()
                shutil.copyfile(repo/'python/examples/ng_setup.py',tests.parent/'examples/ng_setup.py')
            for name in names:shutil.copyfile(source/name,tests/name)
        audit=subprocess.run([args.python,'-I','-c', '''import json,sys
import nrforge_f1ap as f, nrforge_ngap as n
from nrforge_f1ap import _native as fn
from nrforge_ngap import _native as nn
print(json.dumps({'f1ap':{'package':f.__file__,'extension':fn.__file__,'identity':f.identity()},
'ngap':{'package':n.__file__,'extension':nn.__file__,'identity':n.identity()},
'pycrate_loaded':any(s.startswith('pycrate') for s in sys.modules)}))'''],cwd=work,capture_output=True,text=True,check=True)
        result=json.loads(audit.stdout)
        if result['pycrate_loaded']:raise RuntimeError('Runtime oracle import')
        for profile in ('f1ap','ngap'):
            for key in ('package','extension'):
                if Path(result[profile][key]).resolve().is_relative_to(repo):raise RuntimeError('Source-tree import')
            run=subprocess.run([args.python,'-I','-m','unittest','discover','-s',str(work/profile/'tests'),'-p','test_sdk.py','-v'],cwd=work,capture_output=True,text=True)
            result[profile].update(status='PASS' if run.returncode==0 else 'FAIL',stdout=run.stdout,stderr=run.stderr)
        result['status']='PASS' if all(result[p]['status']=='PASS' for p in ('f1ap','ngap')) else 'FAIL'
        result['isolated_python']=True;result['outside_repository']=True
        result['fixture_sha256']={name:hashlib.sha256((repo/'python/f1ap/tests'/name).read_bytes()).hexdigest() for name in ('golden.json','f1-setup-native.json')}
        args.output.write_text(json.dumps(result,indent=2)+'\n')
        if result['status']!='PASS':raise RuntimeError(json.dumps(result,indent=2))
        print('PASS installed F1AP/NGAP Python consumers')
if __name__=='__main__':main()
