#!/usr/bin/env python3
"""Guarded readiness sampling only; no compile or wire qualification claim."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess


def verify(repo, root):
    manifest = json.loads((repo / 'tools/n11-envelope-qualification/source-manifest.json').read_text())
    ordered = [x.strip() for x in (repo / 'tools/qualification/ngap-rel18.modules').read_text().splitlines()
               if x.strip() and not x.lstrip().startswith('#')]
    if ordered != [x['path'] for x in manifest['modules']]:
        raise ValueError('frozen module order mismatch')
    for row in manifest['modules']:
        data = (root / row['path']).read_bytes()
        blob = hashlib.sha1(b'blob ' + str(len(data)).encode() + b'\0' + data).hexdigest()
        if blob != row['git_blob'] or hashlib.sha256(data).hexdigest() != row['sha256']:
            raise ValueError('frozen source identity mismatch: ' + row['path'])
    return manifest


def validate(report, messages):
    expected = [s.strip() for s in messages.read_text().splitlines()
                if s.strip() and not s.lstrip().startswith('#')]
    if report.get('parse') != 'PASS' or report.get('fix') != 'PASS':
        raise ValueError('parse/fix did not pass')
    if not report.get('parser_deleted_before_generation'):
        raise ValueError('owned-lifetime evidence absent')
    if [r['message'] for r in report['messages']] != expected or len(set(expected)) != len(expected):
        raise ValueError('message order/count/uniqueness mismatch')
    for row in report['messages']:
        expected_families = [0, 1, 2] if row['physical_extraction_rc'] == 0 else []
        if [g['family'] for g in row['generation']] != expected_families:
            raise ValueError('generation family coverage mismatch')
    return report


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--repo', type=Path, default=Path('.'))
    ap.add_argument('--asn1-root', type=Path, required=True)
    ap.add_argument('--probe', type=Path)
    ap.add_argument('--output', type=Path)
    ap.add_argument('--check-report', type=Path)
    args = ap.parse_args()
    repo = args.repo.resolve()
    messages = repo / 'tools/n12-codec-coverage/messages.txt'
    manifest = verify(repo, args.asn1_root)
    if args.check_report:
        validate(json.loads(args.check_report.read_text()), messages)
        print('PASS frozen provenance and report structure; not codec qualification')
        return
    if not args.probe or not args.output:
        ap.error('--probe and --output are required for a fresh batch run')
    result = subprocess.run([str(args.probe.resolve()), str(repo / 'tools/qualification/ngap-rel18.modules'),
                             str(args.asn1_root.resolve()), str(messages)], check=True,
                            stdout=subprocess.PIPE, text=True)
    report = validate(json.loads(result.stdout), messages)
    verify(repo, args.asn1_root)
    report['source_authority'] = manifest
    report['baseline_commit'] = subprocess.check_output(['git', '-C', str(repo), 'rev-parse', 'HEAD'], text=True).strip()
    report['evidence_scope'] = 'physical extraction and BODY text generation only; initiating-target descriptor sampling'
    report['inventory_scope'] = 'ordinary owned types and their inline field/alternative constraints; not bound bodies'
    report['probe_sha256'] = hashlib.sha256(args.probe.read_bytes()).hexdigest()
    args.output.write_text(json.dumps(report, indent=2) + '\n')
    print('PASS batch completed; inspect per-message results, not process status')


if __name__ == '__main__':
    main()
