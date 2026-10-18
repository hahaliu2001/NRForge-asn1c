"""Safe git-archive export, including this repository's historical CI links."""
import hashlib
import io
import os
from pathlib import Path, PurePosixPath
import tarfile

CI_SKELETON_PREFIX = '/home/runner/work/asn1c/asn1c/skeletons/'


def extract_source(archive, source):
    source = Path(source)
    with tarfile.open(fileobj=io.BytesIO(archive)) as tar:
        members = tar.getmembers()
        by_name = {}
        for member in members:
            path = PurePosixPath(member.name)
            if path.is_absolute() or '..' in path.parts or str(path) in ('.', ''):
                raise RuntimeError('Unsafe source archive path: '+member.name)
            if member.name in by_name or not (member.isfile() or member.isdir() or member.issym()):
                raise RuntimeError('Duplicate or unsupported source entry: '+member.name)
            by_name[member.name] = member
        links = []
        for member in members:
            if not member.issym():
                continue
            path = PurePosixPath(member.name)
            raw = member.linkname
            rewritten = False
            if raw.startswith('/'):
                # Exact, independently verified legacy layout only. Never
                # dereference the original absolute path or arbitrary suffixes.
                if len(path.parts) != 2 or path.parts[0] != 'tests' or raw != CI_SKELETON_PREFIX+path.name:
                    raise RuntimeError('External source symlink: '+member.name)
                target = PurePosixPath('skeletons')/path.name
                rewritten = True
            else:
                target = PurePosixPath(os.path.normpath(str(path.parent/raw)))
                if target.is_absolute() or '..' in target.parts:
                    raise RuntimeError('Source symlink escapes snapshot: '+member.name)
            target_entry = by_name.get(str(target))
            if target_entry is None or not target_entry.isfile():
                raise RuntimeError('Source link must name an archived regular file: '+member.name)
            links.append({'path':member.name, 'archived_target':raw,
                          'snapshot_target':os.path.relpath(str(target), str(path.parent)),
                          'target_path':str(target), 'rebased_legacy_ci_link':rewritten})
        # Links are file aliases, never directory ancestors of other entries.
        names = set(by_name)
        for item in links:
            if any(n.startswith(item['path']+'/') for n in names):
                raise RuntimeError('Symlink is an archive directory ancestor: '+item['path'])
        source.mkdir(parents=True, exist_ok=False)
        for member in members:
            target = source/member.name
            if member.isdir():
                target.mkdir(parents=True, exist_ok=True)
            elif member.isfile():
                target.parent.mkdir(parents=True, exist_ok=True)
                target.write_bytes(tar.extractfile(member).read())
                target.chmod(member.mode & 0o777)
        for item in links:
            target = source/item['path']
            target.parent.mkdir(parents=True, exist_ok=True)
            target.symlink_to(item['snapshot_target'])
            item['target_sha256'] = hashlib.sha256((source/item['target_path']).read_bytes()).hexdigest()
        return links
