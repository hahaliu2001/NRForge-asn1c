#!/usr/bin/env python3
"""Verify recovered P18 sources against previously built and tested artifacts."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import tarfile
import zipfile

REPO = Path(__file__).resolve().parents[2]
BASE = '56a1d3dbb693d93779bd2e4a550a9f72aa76367d'

def digest(data):
    return hashlib.sha256(data).hexdigest()

def sha(path):
    h = hashlib.sha256()
    with Path(path).open('rb') as f:
        for block in iter(lambda: f.read(1048576), b''): h.update(block)
    return h.hexdigest()

def read(path):
    return json.loads(Path(path).read_text())

def main():
    ap = argparse.ArgumentParser(description=__doc__)
    for name in ('sdist','wheel','build','sdk','venv','output','snapshot'):
        ap.add_argument('--'+name, type=Path, required=True)
    a = ap.parse_args()
    package = REPO/'python/rrc-expanded'
    evidence = REPO/'tools/rrc-python-expanded/evidence'
    iso = read(evidence/'isolated.json')
    assert iso['status'] == 'PASS' and iso['original_paths_restored']
    assert iso['isolated_python'] and iso['outside_repository'] and not iso['pycrate_loaded']
    assert read(evidence/'isolated.restoration.json')['restored']
    counts = {'rrc':12,'ngap':12,'f1ap':9,'e1ap':10}
    assert iso['test_counts'] == counts
    site = a.venv/'lib/python3.12/site-packages'
    for profile,count in counts.items():
        row = iso[profile]
        assert row['status'] == 'PASS' and f'Ran {count} tests' in row['stderr']
        assert (evidence/(profile+'-installed.log')).read_text() == row['stdout']+row['stderr']
    for marker in ('RRC_REAL_CASES 248 ROOTS 85 COMPLETIONS 3','RRC_DEFAULT_OMISSIONS 19','RRC_MALFORMED_QUOTA_CHECKS 5510','RRC_FOUR_PROTOCOL_IMPORT_ORDERS 24'):
        assert marker in iso['rrc']['stdout']
    fixture = read(package/'tests/golden.json')
    assert len(fixture['cases']) == 248 and len({(c['module'],c['root']) for c in fixture['cases']}) == 85
    assert len(fixture['supplemental']) == 3
    assert fixture['qualification_sha256'] == sha(REPO/'tools/rrc-wire-expanded/qualification.json')
    paths = {'rrc':package/'tests','e1ap':REPO/'python/e1ap/tests','f1ap':REPO/'python/f1ap/tests','ngap':REPO/'python/tests'}
    for name,value in iso['fixtures'].items():
        profile,filename = name.split('/',1)
        assert sha(paths[profile]/filename) == value, name
    generated = a.build/'generated'
    receipt = read(generated/'bindings-receipt.json')
    assert len(receipt['generated_sha256']) == 89
    for name,value in receipt['generated_sha256'].items(): assert sha(generated/name) == value, name
    seal = receipt['seal']
    assert seal['identity'] == iso['rrc']['identity'] == fixture['sdk_identity']
    assert seal['identity']['version'] == '0.2.0'
    assert sha(a.sdk/'lib/libnrforge_rrc.a') == seal['archive_sha256']
    for name,value in seal['producer_sha256'].items(): assert sha(package/name) == value, name
    for name,value in seal['metadata_sha256'].items(): assert sha(a.sdk/'share/nrforge-rrc'/name) == value, name
    for name,value in seal['public_header_sha256'].items(): assert sha(a.sdk/'include/nrforge/rrc'/name) == value, name
    assert seal['public_header_sha256'] == fixture['public_header_sha256']
    with tarfile.open(a.sdist) as t:
        members = {m.name.split('/',1)[1]:t.extractfile(m).read() for m in t if m.isfile()}
    stage = json.loads(members['staging-receipt.json'])
    assert len(stage) == 12 and set(members) == set(stage)|{'staging-receipt.json','PKG-INFO'}
    producer = a.build.parent.parent
    for name,value in members.items(): assert (producer/name).read_bytes() == value, name
    for name,value in stage.items():
        assert sha(package/name) == digest(members[name]) == value, name
    with zipfile.ZipFile(a.wheel) as z:
        wheel_files = {name:digest(z.read(name)) for name in z.namelist() if not name.endswith('/')}
        for name,value in wheel_files.items():

            if not name.endswith(".dist-info/RECORD"): assert sha(site/name) == value, name
        assert z.read('nrforge_rrc/__init__.py') == (package/'src/nrforge_rrc/__init__.py').read_bytes()
        assert z.read('nrforge_rrc/conversion-schema.json') == (generated/'conversion-schema.json').read_bytes()
    historical = read(REPO/'tools/rrc-python/verification-summary.json')
    retained = {}
    for profile in ('e1ap','f1ap','ngap'):
        assert iso[profile]['identity'] == historical['isolated_consumers'][profile]['identity']
        accepted_path = {'e1ap':'tools/e1ap-python/verification-summary.json','f1ap':'tools/f1ap-python/verification-summary.json','ngap':'python/verification-summary.json'}[profile]
        accepted = read(REPO/accepted_path)
        wheel_name = f'nrforge_{profile}-0.1.0-cp312-cp312-linux_x86_64.whl'
        entry = accepted['artifact'] if 'artifact' in accepted else accepted['artifacts'][wheel_name]
        archived = REPO.parent/'recovered-aper'
        candidates = list(archived.rglob(wheel_name))
        assert len(candidates) == 1 and sha(candidates[0]) == entry['sha256']
        assert historical['retained_aper_wheels'][wheel_name]['sha256'] == entry['sha256']
        with zipfile.ZipFile(candidates[0]) as z:
            member_manifest = {n:digest(z.read(n)) for n in z.namelist() if not n.endswith('/')}
            if 'files' in entry: assert member_manifest == entry['files']
            verified = 0
            for name,value in member_manifest.items():
                # pip rewrites RECORD and adds INSTALLER/direct_url.json.
                if name.endswith('.dist-info/RECORD'): continue
                assert sha(site/name) == value, name
                verified += 1
        retained[profile] = {'accepted_receipt':accepted_path,'accepted_receipt_sha256':sha(REPO/accepted_path),'accepted_wheel_sha256':entry['sha256'],'installed_members_verified':verified,'native_member_matches_accepted_digest':True,'current_native_sha256':sha(next((site/('nrforge_'+profile)).glob('_native*.so'))),'archive_reverified_after_recovery':True,'wheel_member_sha256':member_manifest}
    extension = next((site/'nrforge_rrc').glob('_native*.so'))
    exports = subprocess.check_output(['nm','-D','-C','--defined-only',str(extension)],text=True)
    dynamic = subprocess.check_output(['readelf','-d',str(extension)],text=True)
    assert 'PyInit__native' in exports
    assert not any(re.match(r'^\S+\s+\S\s+nrforge::',line) for line in exports.splitlines())
    assert not any(name in dynamic for name in ('RPATH','RUNPATH','libnrforge'))
    (evidence/'elf-exports.log').write_text(exports)
    (evidence/'elf-dynamic.log').write_text(dynamic)
    for name,marker in [('generator.log','Ran 5 tests'),('native.log','PASS 248'),('wheel-initial.log','Successfully built'),('wheel-final.log','Successfully built'),('sdist.log','Successfully built')]:
        assert marker in (evidence/name).read_text(), name
    # Preserve every baseline file including symlink bytes.
    tracked = subprocess.check_output(['git','ls-tree','-r','--name-only',BASE],cwd=REPO,text=True).splitlines()
    for name in tracked:
        if name == 'Makefile.am': continue
        path = REPO/name
        content = os.readlink(path).encode() if path.is_symlink() else path.read_bytes()
        expected = subprocess.check_output(['git','rev-parse',BASE+':'+name],cwd=REPO,text=True).strip()
        actual = subprocess.check_output(['git','hash-object','--stdin'],cwd=REPO,input=content).decode().strip()
        assert actual == expected, name
    sources = [REPO/'Makefile.am',REPO/'docs/rrc-python-expanded-p18-closeout.md']
    for root in ('python/rrc-expanded','tools/rrc-python-expanded'):
        sources += [p for p in (REPO/root).rglob('*') if p.is_file() and '__pycache__' not in p.parts and p.name not in ('verification.json','review.md')]
    source_sha = {str(p.relative_to(REPO)):sha(p) for p in sorted(sources)}
    dist = (REPO/'Makefile.am').read_text().split()
    for name in [n for n in source_sha if n!='Makefile.am']+['tools/rrc-python-expanded/verification.json','tools/rrc-python-expanded/review.md']:
        assert dist.count(name) == 1, name
    result = dict(batch='RRC-P18',status='VERIFIED_PENDING_INDEPENDENT_ACCEPTANCE',baseline_commit=BASE,source_sha256=source_sha,bindings_receipt=receipt,staged_source_sha256=stage,isolated_consumers=iso,retained_aper=retained,preserved_baseline_blobs=len(tracked)-1,qualification={'real_cases':248,'supported_roots':85,'selected_roots':80,'channel_roots':12,'completion_modes':3,'malformed_quota_checks':5510,'import_orders':24,'default_omissions':19,'full_rrc_coverage':False},artifacts={a.wheel.name:{'sha256':sha(a.wheel),'bytes':a.wheel.stat().st_size,'files':wheel_files},a.sdist.name:{'sha256':sha(a.sdist),'bytes':a.sdist.stat().st_size,'files':{n:digest(v) for n,v in members.items()}}},recovery={'previous_snapshot_sha256':'1d085e3e2edf75f7c683aaae3536d58ef82e528c82028eac7462ec7b90a20dfa','previous_snapshot_available':False,'package_sources_restored_exactly_from_tested_sdist':True,'workflow_scripts_reconstructed_from_P9':True,'original_aper_archives_available':True,'original_restoration_journal_is_historical':True,'new_independent_review_required':True},benchmark_run=False,merge=False)
    a.output.write_text(json.dumps(result,indent=2,sort_keys=True)+'\n')
    snapshot = {'source_sha256':source_sha,'verification_sha256':sha(a.output)}
    a.snapshot.write_text(json.dumps(snapshot,sort_keys=True,separators=(',',':')))
    print('PASS recovered artifact chain; snapshot',sha(a.snapshot))

if __name__ == '__main__': main()
