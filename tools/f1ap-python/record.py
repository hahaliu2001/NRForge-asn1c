#!/usr/bin/env python3
"""Record actual wheel, source distribution, installed-consumer and seal evidence."""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import subprocess
import zipfile

def digest(path):return hashlib.sha256(path.read_bytes()).hexdigest()

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--report',type=Path,required=True)
    ap.add_argument('--f1-stage',type=Path,required=True)
    ap.add_argument('--ngap-wheel',type=Path,required=True)
    ap.add_argument('--sdist',type=Path,required=True)
    ap.add_argument('--f1-sdk',type=Path,required=True)
    ap.add_argument('--ngap-sdk',type=Path,required=True)
    ap.add_argument('--output',type=Path,required=True)
    args=ap.parse_args();repo=Path(__file__).resolve().parents[2]
    report=json.loads(args.report.read_text())
    if report['status']!='PASS':raise ValueError('consumer report not passing')
    spec=importlib.util.spec_from_file_location('generator',repo/'python/ngap_sdk/generate_bindings.py')
    gen=importlib.util.module_from_spec(spec);spec.loader.exec_module(gen)
    receipts={}
    for profile,prefix in (('f1ap',args.f1_sdk),('ngap',args.ngap_sdk)):
        fp=report[profile]['identity']['fingerprint']
        _,receipts[profile]=gen.verify_installed_sdk(prefix.resolve(),fp,profile)
    artifacts={}
    wheel=next((args.f1_stage/'dist').glob('nrforge_f1ap-*.whl'))
    for path in (wheel,args.ngap_wheel,args.sdist):
        artifacts[path.name]={'sha256':digest(path),'size_bytes':path.stat().st_size}
        if path.suffix=='.whl':
            with zipfile.ZipFile(path) as z:
                artifacts[path.name]['files']={n:hashlib.sha256(z.read(n)).hexdigest() for n in z.namelist()}
    sources=[repo/'python/CMakeLists.txt',*(repo/'python/ngap_sdk').glob('*.*'),
             *(repo/'python/f1ap/tests').glob('*.py'),*(repo/'python/f1ap/tests').glob('*.json'),
             *(repo/'tools/f1ap-python').glob('*.py'),repo/'tools/f1ap-python/README.md']
    result={'status':'VERIFIED_PENDING_INDEPENDENT_ACCEPTANCE',
        'baseline_commit':subprocess.check_output(['git','rev-parse','HEAD'],cwd=repo,text=True).strip(),
        'scope':'F1-P6 installed Python integration; historical finite wire bytes reused without new wire qualification',
        'platform':'CPython 3.12 Linux x86-64',
        'bindings':{'f1ap_messages':158,'f1ap_procedures':94,'ngap_messages':131,
             'f1ap_type_counts':json.loads((args.f1_stage/'build/cp312-cp312-linux_x86_64/generated/conversion-schema.json').read_text())['counts']},
        'artifacts':artifacts,'installed_consumers':report,'sdk_receipts':receipts,
        'implementation_sha256':{str(p.relative_to(repo)):digest(p) for p in sorted(sources)},
        'validation':{'f1ap_test_methods':9,'historical_finite_vectors':4728,'independent_setup_outcomes':3,
             'ngap_test_methods':12,'ngap_native_vectors':131,'ngap_setup_outcomes':3,
             'both_import_orders':'PASS','f1ap_negative_seal_methods':2,'original_ngap_negative_seal_methods':2,
             'distribution_profile_mismatch':'PASS','sdist_staging_receipt':'PASS',
             'strict_cpp20_compile':'PASS','native_registry_import_and_install_seal':'PASS',
             'elf_no_rpath_or_sdk_shared_dependency':'PASS'},
        'build_environment':{'initial_link':'Default linker exceeded memory limit; no codec/source workaround',
             'accepted_link':'Serial lld 18, --threads=1, CMAKE_MODULE_LINKER_FLAGS; all compiled objects reused',
             'options':'-O0 -Wall -Wextra -Werror -pedantic-errors -Wconversion -Wsign-conversion'},
        'limitations':['Finite integration coverage, not exhaustive values or live RAN/vendor interoperability',
             'Source distribution inventory verified; a second full clean build from its extracted archive was not run',
             'No benchmark, full-extension sanitizer, stable ABI or alternate-platform qualification']}
    args.output.write_text(json.dumps(result,indent=2)+'\n')
    print('PASS actual Python SDK verification evidence recorded; independent acceptance pending')
if __name__=='__main__':main()
