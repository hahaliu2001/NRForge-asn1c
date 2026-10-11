#!/usr/bin/env python3
"""Compare two independently generated/sealed SDK inputs across locations."""
import argparse
import hashlib
import json
from pathlib import Path


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    for name in ('generated', 'repeat', 'output'):
        ap.add_argument('--' + name, type=Path, required=True)
    args = ap.parse_args()
    left, right = args.generated.resolve(), args.repeat.resolve()
    if left == right:
        raise ValueError('distinct generation directories required')
    a = json.loads((left/'sdk-provenance.json').read_text())
    b = json.loads((right/'sdk-provenance.json').read_text())
    if a['fingerprint'] != b['fingerprint'] or a['source_hashes'] != b['source_hashes']:
        raise ValueError('location-independent source SDK identity mismatch')
    files = []
    for root in (left, right):
        files.append({str(p.relative_to(root)): sha(p) for p in root.rglob('*')
                      if p.is_file() and p.name not in {'manifest.json', 'sdk-lock.cmake', 'sdk-provenance.json'}})
    if files[0] != files[1]:
        raise ValueError('fresh generated and sealed public files changed')
    manifests = [json.loads((root/'manifest.json').read_text()) for root in (left, right)]
    for manifest in manifests:
        manifest.pop('source_inputs')
    if manifests[0] != manifests[1]:
        raise ValueError('logical generation manifest changed')
    args.output.write_text(json.dumps({'status': 'PASS', 'sdk_fingerprint': a['fingerprint'],
        'byte_exact_files': len(files[0]), 'logical_manifest_equal': True,
        'scope': 'distinct fresh generation and seal; no duplicate archive build',
        'files_sha256': files[0]}, indent=2, sort_keys=True)+'\n')
    print('PASS reproducible source/generated SDK identity')


if __name__ == '__main__':
    main()
