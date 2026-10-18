#!/usr/bin/env python3
"""Download one immutable producer asset using an explicit consumer lock."""
import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath
import subprocess
import tarfile

REPOSITORY='hahaliu2001/NRForge-asn1c'


def sha(path):
    h=hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda:stream.read(1024*1024),b''):h.update(block)
    return h.hexdigest()


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--lock',type=Path,required=True);p.add_argument('--output',type=Path,required=True)
    a=p.parse_args();lock=json.loads(a.lock.read_text())
    if lock['repository']!=REPOSITORY or lock['version']!='0.3.0' or lock['tag']!='asn1-sdk-v0.3.0':raise ValueError('Unsupported producer lock')
    name=lock['asset'];digest=lock['sha256']
    if PurePosixPath(name).name!=name or len(digest)!=64 or any(c not in '0123456789abcdef' for c in digest):raise ValueError('Invalid immutable asset lock')
    a.output.mkdir(parents=True,exist_ok=False)
    subprocess.run(['gh','release','download',lock['tag'],'--repo','github.com/'+REPOSITORY,'--pattern',name,'--dir',str(a.output)],check=True)
    asset=a.output/name
    if sha(asset)!=digest:raise ValueError('Pinned download digest mismatch')
    if name.endswith('.tar.gz'):
        root='nrforge-asn1-sdk-0.3.0'
        with tarfile.open(asset,'r:gz') as archive:
            members=archive.getmembers();seen=set()
            for member in members:
                path=PurePosixPath(member.name)
                if path.is_absolute() or '..' in path.parts or not path.parts or path.parts[0]!=root or member.name in seen or not (member.isfile() or member.isdir()):raise ValueError('Unsafe SDK archive member')
                seen.add(member.name)
            archive.extractall(a.output,filter='data')
        print(a.output/root)
    elif not name.endswith('.whl'):raise ValueError('Unsupported consumer artifact')
    else:print(asset)


if __name__=='__main__':main()
