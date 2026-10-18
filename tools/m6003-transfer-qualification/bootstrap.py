#!/usr/bin/env python3
"""Export immutable inputs, build a probe, isolate oracle, then run qualification."""
import argparse
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import subprocess
import sys

from support import command, sha, write
from snapshot import extract_source


def git(repo, *args):
    return subprocess.check_output(['git', '-C', str(repo), *args])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--ran-repo', type=Path,
                        default=Path.home()/'jie_github/NRForge-RAN')
    parser.add_argument('--output', type=Path)
    parser.add_argument('--cxx', default='c++')
    parser.add_argument('--jobs', type=int, default=2, choices=(1, 2))
    args = parser.parse_args()
    repo = Path(__file__).resolve().parents[2]
    if sys.version_info < (3, 11):
        raise RuntimeError('Python >=3.11 required')
    # All qualification inputs must be committed; untracked unrelated evidence
    # is not consumed and is not silently added to the immutable export.
    if git(repo, 'diff', '--name-only', 'HEAD').strip():
        raise RuntimeError('Tracked worktree/index changes: commit or preserve them first')
    head = git(repo, 'rev-parse', 'HEAD').decode().strip()
    git(repo, 'merge-base', '--is-ancestor', '36df0f5ba547e985cc3db236f83c99273dc78e3b', head)
    out = args.output or Path.home()/('m6003-transfer-qualification-' +
             datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S.%fZ'))
    out = out.expanduser().absolute()
    out.mkdir(parents=True, exist_ok=False)
    print('Evidence:', out, flush=True)
    summary = {'status': 'RUNNING', 'source_head': head, 'qualification': 'NOT_RUN'}
    write(out/'summary.json', summary)
    try:
        # git archive contains only tracked files, not local caches/libraries.
        archive = git(repo, 'archive', '--format=tar', head, '--', '.', ':(exclude)verification',
                      ':(exclude)_codeql_detected_source_root')
        source = out/'source'
        links = extract_source(archive, source)
        write(out/'source-links.json', links)
        authority = json.loads((source/'tools/n11-envelope-qualification/source-manifest.json').read_text())
        schema = out/'asn1'; schema.mkdir()
        for item in authority['modules']:
            data = git(args.ran_repo, 'show', authority['authority_commit']+':src/'+item['path'])
            blob = hashlib.sha1(b'blob '+str(len(data)).encode()+b'\0'+data).hexdigest()
            if blob != item['git_blob'] or hashlib.sha256(data).hexdigest() != item['sha256']:
                raise RuntimeError('Schema identity mismatch: '+item['path'])
            path = schema/item['path']; path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(data)
        inventory = {str(p.relative_to(source)): sha(p) for p in source.rglob('*') if p.is_file()}
        write(out/'source-inventory.json', inventory)
        write(out/'identity.json', {'source_head': head, 'schema': authority,
                                    'bootstrap_python': sys.version, 'cxx': args.cxx})
        command(out/'00-compiler-version', [args.cxx, '--version'])
        command(out/'00-git-version', ['git', '--version'])
        command(out/'01-autoreconf', ['autoreconf', '-iv'], cwd=source)
        command(out/'02-configure', [source/'configure'], cwd=source)
        for i, library in enumerate(('libasn1common', 'libasn1parser', 'libasn1fix', 'libasn1typed')):
            command(out/f'03-{i}-{library}', ['make', f'-j{args.jobs}', '-C', source/library], cwd=source)
        command(out/'04-probe-build', ['make', f'-j{args.jobs}', '-C', source/'tools', 'asn1typed_ioc_probe'], cwd=source)
        venv = out/'oracle-venv'
        command(out/'05-venv', [sys.executable, '-m', 'venv', venv])
        python = venv/'bin/python'
        tool = source/'tools/m6003-transfer-qualification'
        wheels = out/'oracle-wheel'; wheels.mkdir()
        command(out/'06-oracle-download', [python, '-I', '-m', 'pip', 'download',
                '--disable-pip-version-check', '--no-deps', '--only-binary=:all:', '--require-hashes',
                '-r', tool/'oracle-requirements.lock', '-d', wheels])
        pin = json.loads((tool/'qualification-contract.json').read_text())['oracle_recommendation']
        files = list(wheels.iterdir())
        if len(files) != 1 or files[0].name != pin['install_artifact'] or sha(files[0]) != pin['install_artifact_sha256']:
            raise RuntimeError('Oracle wheel identity differs from approved pin')
        command(out/'07-oracle-install', [python, '-I', '-m', 'pip', 'install',
                '--disable-pip-version-check', '--no-index', '--no-deps', '--require-hashes',
                '--find-links', wheels, '-r', tool/'oracle-requirements.lock'])
        summary['qualification'] = 'RUNNING'
        write(out/'summary.json', summary)
        command(out/'08-qualification', [python, '-I', tool/'qualify.py', '--repo', source,
                '--asn1-root', schema, '--probe', source/'tools/asn1typed_ioc_probe',
                '--work', out/'campaign', '--cxx', args.cxx], timeout=7200, live=True)
        report = json.loads((out/'campaign/report.json').read_text())
        if report['status'] != 'PASS':
            raise RuntimeError('Campaign did not finish with PASS')
        summary.update(status='PASS', qualification='PASS', checks=report['checks'],
                       report_sha256=sha(out/'campaign/report.json'))
    except BaseException as error:
        summary.update(status='FAIL_OR_INTERRUPTED', error_type=type(error).__name__)
        raise
    finally:
        write(out/'summary.json', summary)
        (out/'SUMMARY.txt').write_text('\n'.join(f'{k}={v}' for k, v in summary.items())+'\n')
        print((out/'SUMMARY.txt').read_text(), flush=True)
        print('Evidence:', out, flush=True)


if __name__ == '__main__':
    main()
