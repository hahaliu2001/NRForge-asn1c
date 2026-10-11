#!/usr/bin/env python3
"""Record checked artifacts and actual isolated installed-consumer results."""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import subprocess
import tarfile
import zipfile

def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
    ap=argparse.ArgumentParser(description=__doc__)
    for arg in ('report','stage','sdk','output'):ap.add_argument('--'+arg,type=Path,required=True)
    ap.add_argument('--build-log',type=Path,required=True)
    ap.add_argument('--seal-log',type=Path,action='append',required=True)
    ap.add_argument('--regression-wheel',type=Path,action='append',required=True)
    ap.add_argument('--mismatch-log',type=Path,required=True)
    args=ap.parse_args();repo=Path(__file__).resolve().parents[2]
    report=json.loads(args.report.read_text());assert report['status']=='PASS'
    spec=importlib.util.spec_from_file_location('generator',repo/'python/ngap_sdk/generate_bindings.py')
    g=importlib.util.module_from_spec(spec);spec.loader.exec_module(g)
    _,receipt=g.verify_installed_sdk(args.sdk.resolve(),report['e1ap']['identity']['fingerprint'],'e1ap')
    build_log=args.build_log.read_text()
    assert 'PASS native extension import and complete registry' in build_log
    assert 'Successfully built nrforge_e1ap-0.1.0-cp312-cp312-linux_x86_64.whl' in build_log
    ninja=args.stage/'build/cp312-cp312-linux_x86_64/build.ninja'
    flags=ninja.read_text()
    for option in ('-O0','-Wall','-Wextra','-Werror','-pedantic-errors','-Wconversion','-Wsign-conversion','-std=c++20','-fuse-ld=lld','--threads=1','--exclude-libs,ALL'):assert option in flags
    cpp_receipt=json.loads((repo/'tools/e1ap-sdk/verification-summary.json').read_text())
    archive=args.sdk/'lib/libnrforge_e1ap.a'
    assert sha(archive)==cpp_receipt['archive_sha256']
    artifacts={}
    assert len(list((args.stage/'dist').glob('*.whl')))==1
    assert len(list((args.stage/'dist').glob('*.tar.gz')))==1
    for name in ('CMakeLists.txt','ngap_sdk/conversion.hpp','ngap_sdk/module.cpp','ngap_sdk/generate_bindings.py'):
        assert sha(args.stage/name)==sha(repo/'python'/name)
    installed_package=Path(report['e1ap']['package']).parent
    for p in sorted((args.stage/'dist').glob('*')):
        row={'sha256':sha(p),'size_bytes':p.stat().st_size}
        if p.suffix=='.whl':
            with zipfile.ZipFile(p) as z:
                row['files']={n:hashlib.sha256(z.read(n)).hexdigest() for n in z.namelist()}
                for name,digest in row['files'].items():
                    if name.startswith('nrforge_e1ap/'):
                        assert sha(installed_package/name.split('/',1)[1])==digest
                conversion_schema=json.loads(z.read('nrforge_e1ap/conversion-schema.json'))
        else:
            with tarfile.open(p) as t:
                members={n.name.split('/',1)[1]:t.extractfile(n).read() for n in t.getmembers() if n.isfile() and '/' in n.name}
                staging=json.loads(members['staging-receipt.json'])
                assert set(members)==set(staging)|{'staging-receipt.json','PKG-INFO'}
                for name,digest in staging.items():assert hashlib.sha256(members[name]).hexdigest()==digest
                row['staging_inventory_verified']=True
        artifacts[p.name]=row
    seal_logs={}
    assert len(args.seal_log)==3
    for log in args.seal_log:
        data=log.read_text();match=re.search(r'Ran (\d+) tests',data)
        assert match and int(match[1])==2 and re.search(r'^OK$',data,re.M)
        seal_logs[log.name]={'sha256':sha(log),'methods':int(match[1]),'status':'PASS'}
    assert 'Python distribution and SDK profile mismatch' in args.mismatch_log.read_text()
    elf=subprocess.check_output(['readelf','-d',report['e1ap']['extension']],text=True)
    assert not re.search(r'\((?:RPATH|RUNPATH)\)',elf)
    assert not re.search(r'NEEDED.*(?:nrforge|e1ap|f1ap|ngap)',elf,re.I)
    retained={}
    assert len(args.regression_wheel)==2
    for p in args.regression_wheel:
        profile='f1ap' if p.name.startswith('nrforge_f1ap-') else 'ngap'
        source=repo/('tools/f1ap-python/verification-summary.json' if profile=='f1ap' else 'python/verification-summary.json')
        historical=json.loads(source.read_text())
        expected=historical['artifacts'][p.name]['sha256'] if profile=='f1ap' else historical['artifact']['sha256']
        assert sha(p)==expected
        with zipfile.ZipFile(p) as z:
            package=Path(report[profile]['package']).parent
            for n in z.namelist():
                if n.startswith('nrforge_'+profile+'/'):
                    assert sha(package/n.split('/',1)[1])==hashlib.sha256(z.read(n)).hexdigest()
        retained[profile]={'filename':p.name,'sha256':sha(p),'source_receipt':str(source.relative_to(repo)),'source_receipt_sha256':sha(source),'installed_member_hashes':'PASS'}
    assert set(retained)=={'f1ap','ngap'}
    methods={p:int(re.search(r'Ran (\d+) tests',report[p]['stderr'])[1]) for p in ('e1ap','f1ap','ngap')}
    implementation=[repo/'python/CMakeLists.txt',*(repo/'python/ngap_sdk').glob('*.*'),
        *(repo/'python/e1ap/tests').glob('*.py'),*(repo/'python/e1ap/tests').glob('*.json'),
        *(repo/'tools/e1ap-python').glob('*.py'),repo/'tools/e1ap-python/README.md']
    result={'status':'VERIFIED_PENDING_INDEPENDENT_ACCEPTANCE',
        'baseline_commit':subprocess.check_output(['git','rev-parse','HEAD'],cwd=repo,text=True).strip(),
        'scope':'E1-P6 Python delivery; inherited finite E1-P4 wire profile, no new wire qualification',
        'build_evidence':{'log_sha256':sha(args.build_log),'ninja_sha256':sha(ninja),'unchanged_cpp_archive_sha256':sha(archive),'compiler':'GCC 13.3.0','linker':'lld 18.1.3 --threads=1','compile_jobs':2,'strict_options':['-O0','-Wall','-Wextra','-Werror','-pedantic-errors','-Wconversion','-Wsign-conversion']},
        'target':'CPython 3.12 Linux x86-64','messages':72,'procedures':40,
        'conversion_counts':conversion_schema['counts'],
        'sdk_receipt':receipt,'retained_regression_wheels':retained,'artifacts':artifacts,'installed_consumers':report,
        'implementation_sha256':{str(p.relative_to(repo)):sha(p) for p in sorted(implementation)},
        'validation':{'installed_test_methods':methods,'historical_e1_vectors':1341,'setup_outcomes':6,
            'three_protocol_import_orders':6,'seal_logs':seal_logs,
            'distribution_profile_mismatch':{'status':'PASS','log_sha256':sha(args.mismatch_log)},
            'installed_wheel_member_hashes':'PASS','elf_dynamic_section':elf},
        'limitations':['Finite integration coverage, not exhaustive values or live RAN/vendor interoperability',
            'Final sdist inventory verified; no successful full clean build from the extracted final sdist was run',
            'No benchmark, full-extension sanitizer, stable ABI, alternate-platform, free-threaded or subinterpreter qualification']}
    args.output.write_text(json.dumps(result,indent=2)+'\n')
    print('PASS actual E1AP Python delivery evidence recorded')
if __name__=='__main__':main()
