#!/usr/bin/env python3
"""Separate candidate packaging, immutable publication and explicit retirement."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import tarfile

REPO='hahaliu2001/NRForge-asn1c'
VERSION='0.3.0'


def sha(path):
    h=hashlib.sha256()
    with Path(path).open('rb') as stream:
        for block in iter(lambda:stream.read(1024*1024),b''):h.update(block)
    return h.hexdigest()


def read(path):return json.loads(Path(path).read_text())


def gh(endpoint, method='GET', body=None):
    args=['gh','api','--hostname','github.com','--method',method,endpoint]
    if body is not None:args+=['--input','-']
    result=subprocess.run(args,input=None if body is None else json.dumps(body).encode(),stdout=subprocess.PIPE,check=True)
    return json.loads(result.stdout) if result.stdout else None


def asset_digest(repo, asset_id):
    # Downloads are streamed to a hash, not stored in memory or logged.
    process=subprocess.Popen(['gh','api','--hostname','github.com',f'repos/{repo}/releases/assets/{asset_id}','-H','Accept: application/octet-stream'],stdout=subprocess.PIPE)
    h=hashlib.sha256()
    try:
        for block in iter(lambda:process.stdout.read(1024*1024),b''):h.update(block)
    finally:
        process.stdout.close()
    if process.wait()!=0:raise RuntimeError('Cannot verify release asset bytes')
    return h.hexdigest()


def load_candidate(directory):
    root=directory.resolve();manifest=read(root/'candidate.json')
    if manifest['version']!=VERSION or manifest['repository']!=REPO:raise ValueError('Wrong producer/version')
    for name,digest in manifest['assets'].items():
        path=root/name
        if path.is_symlink() or path.name!=name or not path.is_file() or sha(path)!=digest:raise ValueError('Candidate asset identity mismatch')
    return manifest


def approval(path,digest):
    receipt=read(path)
    if receipt.get('candidate_sha256')!=digest or receipt.get('owner_acceptance')!='ACCEPTED' or receipt.get('independent_review')!='PASS':
        raise ValueError('Exact candidate independent review and Owner acceptance required')


def package(a):
    report_path=a.work/'producer.json';r=read(report_path)
    if r['status']!='PASS_LOCAL_QUALIFICATION' or r['version']!=VERSION:raise ValueError('Unified qualification incomplete')
    prefix=Path(r['prefix']).resolve()
    actual={str(p.relative_to(prefix)):sha(p) for p in prefix.rglob('*') if p.is_file()}
    if any(p.is_symlink() for p in prefix.rglob('*')) or actual!=r['sdk_files']:raise ValueError('Installed SDK changed after qualification')
    wheel=Path(r['wheel'])
    if sha(wheel)!=r['wheel_sha256']:raise ValueError('Wheel changed after qualification')
    for relative,digest in r['reports'].items():
        path=(a.work/relative).resolve()
        if not path.is_relative_to(a.work.resolve()) or sha(path)!=digest:raise ValueError('Qualification evidence changed')
    a.output.mkdir(parents=True,exist_ok=False)
    archive=a.output/f'nrforge-asn1-sdk-{VERSION}-linux-x86_64.tar.gz'
    with tarfile.open(archive,'w:gz') as tar:tar.add(prefix,arcname='nrforge-asn1-sdk-'+VERSION,recursive=True)
    shutil.copyfile(wheel,a.output/wheel.name)
    shutil.copyfile(report_path,a.output/'producer.json')
    for kind,path in [('sdk',archive),('wheel',a.output/wheel.name)]:
        lock=dict(repository=REPO,version=VERSION,tag='asn1-sdk-v'+VERSION,asset=path.name,sha256=sha(path),source_revision=r['source_revision'])
        (a.output/(kind+'-consumer-lock.json')).write_text(json.dumps(lock,indent=2,sort_keys=True)+'\n')
    assets={p.name:sha(p) for p in a.output.iterdir() if p.is_file()}
    m=dict(version=VERSION,repository=REPO,source_revision=r['source_revision'],producer_sha256=sha(report_path),assets=assets,
        qualification='PASS_LOCAL_QUALIFICATION',publication='NOT_RUN',owner_acceptance='PENDING')
    (a.output/'candidate.json').write_text(json.dumps(m,indent=2,sort_keys=True)+'\n')
    print('Candidate:',a.output,'candidate SHA256:',sha(a.output/'candidate.json'))


def publish(a):
    m=load_candidate(a.candidate);approval(a.acceptance,sha(a.candidate/'candidate.json'))
    tag='asn1-sdk-v'+VERSION
    # List all pages: errors are failures, never interpreted as "absent".
    def pages(endpoint):
        result=[];page=1
        while True:
            rows=gh(endpoint+f'?per_page=100&page={page}')
            result+=rows
            if len(rows)<100:return result
            page+=1
    if any(r['tag_name']==tag for r in pages(f'repos/{REPO}/releases')):raise ValueError('Release already exists; no overwrite/resume')
    refs=gh(f'repos/{REPO}/git/matching-refs/tags/{tag}')
    if any(r['ref']=='refs/tags/'+tag for r in refs):raise ValueError('Tag already exists; no mutation')
    release=gh(f'repos/{REPO}/releases','POST',dict(tag_name=tag,target_commitish=m['source_revision'],name='NRForge ASN1 SDK '+VERSION,draft=True,body='Unified C++ SDK and NGAP wheel. Exact candidate and qualification receipts included.'))
    (a.candidate/'publication-attempt.json').write_text(json.dumps(dict(release_id=release['id'],tag=tag,status='DRAFT_CREATED'),indent=2)+'\n')
    names=[*m['assets'],'candidate.json']
    subprocess.run(['gh','release','upload',tag,*[str(a.candidate/name) for name in names],'--repo','github.com/'+REPO],check=True)
    actual=gh(f'repos/{REPO}/releases/{release["id"]}/assets?per_page=100')
    if len(actual)!=len(names) or {r['name'] for r in actual}!=set(names):raise ValueError('Unexpected draft release asset inventory; leave draft for inspection')
    for row in actual:
        if asset_digest(REPO,row['id'])!=sha(a.candidate/row['name']):raise ValueError('Uploaded bytes differ; leave draft for inspection')
    current=gh(f'repos/{REPO}/releases/{release["id"]}')
    if current['tag_name']!=tag or not current['draft'] or current['target_commitish']!=m['source_revision']:
        raise ValueError('Draft identity changed; leave it for inspection')
    identity=lambda rows:sorted((r['id'],r['name'],r['size'],r.get('digest'),r.get('state')) for r in rows)
    latest=gh(f'repos/{REPO}/releases/{release["id"]}/assets?per_page=100')
    if identity(latest)!=identity(actual):raise ValueError('Draft assets changed before publication')
    published=gh(f'repos/{REPO}/releases/{release["id"]}','PATCH',dict(draft=False,make_latest='true'))
    reference=gh(f'repos/{REPO}/git/ref/tags/{tag}')['object']
    seen=set()
    while reference['type']=='tag':
        if reference['sha'] in seen:raise ValueError('Published tag cycle')
        seen.add(reference['sha']);reference=gh(f'repos/{REPO}/git/tags/{reference["sha"]}')['object']
    if reference['type']!='commit' or reference['sha']!=m['source_revision']:raise ValueError('Published tag source differs; do not move it')

    (a.candidate/'publication.json').write_text(json.dumps(dict(repository=REPO,tag=tag,release_id=release['id'],url=published['html_url'],candidate_sha256=sha(a.candidate/'candidate.json'),assets=actual),indent=2)+'\n')
    print(published['html_url'])


def retire(a):
    m=load_candidate(a.candidate);digest=sha(a.candidate/'candidate.json');approval(a.acceptance,digest)
    published=read(a.candidate/'publication.json')
    if published['candidate_sha256']!=digest:raise ValueError('Wrong publication identity')
    release=gh(f'repos/{REPO}/releases/{published["release_id"]}')
    if release['draft'] or release['tag_name']!='asn1-sdk-v'+VERSION:raise ValueError('Replacement release not published')
    migrations=read(a.migrations)
    for consumer in ('RAN','5GC'):
        row=migrations[consumer]
        if row.get('status')!='PASS' or row.get('version')!=VERSION or row.get('candidate_sha256')!=digest:
            raise ValueError('Both consumers must finish migration before retiring assets')
    plan=read(a.plan)
    allowed={REPO,'hahaliu2001/NRForge-RAN','hahaliu2001/NRForge-5GC'}
    identities=set();checked=[]
    for row in plan['assets']:
        repo=row['repository'];key=(repo,row['asset_id'])
        if repo not in allowed or key in identities:raise ValueError('Invalid/duplicate retirement target')
        identities.add(key)
        asset=gh(f'repos/{repo}/releases/assets/{row["asset_id"]}')
        if asset['name']!=row['name'] or asset['size']!=row['size'] or row['sha256'] in m['assets'].values():raise ValueError('Retirement identity mismatch or current asset selected')
        if repo==REPO and any(asset['id']==r['id'] for r in published['assets']):raise ValueError('Current release asset cannot be retired')
        if asset_digest(repo,asset['id'])!=row['sha256']:raise ValueError('Old asset digest differs from reviewed retirement plan')
        checked.append((repo,asset))
    # All identities are verified before any deletion. Recheck immediately before
    # each DELETE; no glob, release deletion, tag deletion or history rewrite.
    for repo,asset in checked:
        current=gh(f'repos/{repo}/releases/assets/{asset["id"]}')
        if any(current[k]!=asset[k] for k in ('id','name','size','updated_at')):raise ValueError('Old asset changed during retirement')
        gh(f'repos/{repo}/releases/assets/{asset["id"]}','DELETE')
    (a.candidate/'retirement.json').write_text(json.dumps(dict(candidate_sha256=digest,deleted=[dict(repository=r,asset_id=x['id'],name=x['name']) for r,x in checked]),indent=2)+'\n')


def main():
    p=argparse.ArgumentParser(description=__doc__);sub=p.add_subparsers(dest='command',required=True)
    s=sub.add_parser('package');s.add_argument('--work',type=Path,required=True);s.add_argument('--output',type=Path,required=True)
    for name in ('publish','retire'):
        s=sub.add_parser(name);s.add_argument('--candidate',type=Path,required=True);s.add_argument('--acceptance',type=Path,required=True)
        if name=='retire':s.add_argument('--migrations',type=Path,required=True);s.add_argument('--plan',type=Path,required=True)
    a=p.parse_args();globals()[a.command](a)


if __name__=='__main__':main()
