#!/usr/bin/env python3
"""Bounded terminal build and installed-consumer qualification for NGAP 0.2.0."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parents[0]/'m6003-transfer-qualification'))
from snapshot import extract_source
from support import command, sha, write
from resource_guard import verify


def git(repo, *args):
    return subprocess.check_output(['git', '-C', str(repo), *args])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--ran-repo', type=Path, required=True)
    parser.add_argument('--work', type=Path, required=True)
    args = parser.parse_args()
    verify()
    repo = HERE.parents[1]
    if sys.version_info[:2] != (3, 12) or sys.platform != 'linux':
        raise RuntimeError('Delivery qualification requires native Linux CPython 3.12')
    if git(repo, 'diff', '--name-only', 'HEAD').strip():
        raise RuntimeError('Commit tracked changes before building')
    head = git(repo, 'rev-parse', 'HEAD').decode().strip()
    out = args.work.expanduser().resolve()
    out.mkdir(parents=True, exist_ok=False)
    summary = dict(status='RUNNING', source_head=head, sdk_version='0.2.0')
    write(out/'summary.json', summary)
    try:
        source = out/'source'
        archive = git(repo, 'archive', head, '--', '.', ':(exclude)verification',
                      ':(exclude)_codeql_detected_source_root', ':(exclude).vscode')
        write(out/'source-links.json', extract_source(archive, source))
        authority = json.loads((source/'tools/n11-envelope-qualification/source-manifest.json').read_text())
        schema = out/'asn1'
        schema.mkdir()
        for row in authority['modules']:
            data = git(args.ran_repo, 'show', authority['authority_commit']+':src/'+row['path'])
            if hashlib.sha256(data).hexdigest() != row['sha256'] or hashlib.sha1(b'blob '+str(len(data)).encode()+b'\0'+data).hexdigest() != row['git_blob']:
                raise RuntimeError('Frozen schema identity mismatch')
            target = schema/row['path']
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(data)
        os.environ.update(CC='gcc-13', CXX='g++-13', CMAKE_BUILD_PARALLEL_LEVEL='1', MAKEFLAGS='-j1',
                          PYTHONDONTWRITEBYTECODE='1', CFLAGS='-O0 -g0')
        version = command(out/'00-compiler', ['g++-13', '-dumpfullversion']).strip()
        if version.split('.')[0] != '13':
            raise RuntimeError('GCC 13 is required')
        command(out/'01-autoreconf', ['autoreconf', '-iv'], cwd=source)
        command(out/'02-configure', [source/'configure'], cwd=source)
        for library in ('libasn1common', 'libasn1parser', 'libasn1fix', 'libasn1typed'):
            command(out/('03-'+library), ['make', '-j1', '-C', source/library], cwd=source)
        command(out/'04-generators', ['make', '-j1', '-C', source/'tools', 'asn1typed_ioc_probe', 'asn1typed_ngap_dispatch'], cwd=source)
        prefix = out/'sdk-prefix'
        command(out/'05-sdk', [sys.executable, source/'tools/ngap-sdk/build.py', '--repo', source,
                '--asn1-root', schema, '--work', out/'sdk-build', '--prefix', prefix,
                '--version', '0.2.0', '--source-revision', head, '--jobs', '1'], timeout=21600, live=True)
        toolenv = out/'tool-venv'
        command(out/'06-tool-venv', [sys.executable, '-m', 'venv', toolenv])
        python = toolenv/'bin/python'
        command(out/'07-build-tools', [python, '-I', '-m', 'pip', 'install', 'build==1.3.0',
                'scikit-build-core==0.11.6', 'pybind11==3.0.1'])
        command(out/'08-oracle', [python, '-I', '-m', 'pip', 'install', '--no-deps', '--require-hashes',
                '-r', source/'tools/m6003-transfer-qualification/oracle-requirements.lock'])
        provenance_path = prefix/'share/nrforge-ngap/sdk-provenance.json'
        provenance = json.loads(provenance_path.read_text())
        command(out/'09-verify-sdk', [python, source/'python/ngap_sdk/generate_bindings.py',
                '--sdk-prefix', prefix, '--expected-fingerprint', provenance['fingerprint'], '--verify-only'])
        command(out/'10-vectors', [python, source/'tools/ngap-transfer-sdk/make_vectors.py',
                '--repo', source, '--asn1-root', schema, '--prefix', prefix, '--output', out/'vectors.json'], timeout=1800)
        # Source build caches and output directories are new for every run.
        dist = out/'dist'
        command(out/'11-wheel', [python, '-I', '-m', 'build', '--no-isolation', '--wheel', source/'python/ngap-transfer',
                '--outdir', dist, '-Ccmake.define.NRFORGE_PYTHON_SDK_PREFIX='+str(prefix),
                '-Ccmake.define.CMAKE_CXX_FLAGS=-O0 -g0 --param=ggc-min-expand=5 --param=ggc-min-heapsize=16384'], timeout=21600, live=True)
        wheels = list(dist.glob('*.whl'))
        if len(wheels) != 1 or wheels[0].name != 'nrforge_ngap-0.2.0-cp312-cp312-linux_x86_64.whl':
            raise RuntimeError('Unexpected wheel identity/platform')
        wheel = wheels[0]
        consumer = out/'consumer-venv'
        command(out/'12-consumer-venv', [sys.executable, '-m', 'venv', consumer])
        requirements = out/'wheel-requirements.txt'
        requirements.write_text(wheel.as_uri()+' --hash=sha256:'+sha(wheel)+'\n')
        consumer_python = consumer/'bin/python'
        command(out/'13-install', [consumer_python, '-I', '-m', 'pip', 'install', '--no-index', '--no-deps', '--require-hashes', '-r', requirements])
        command(out/'14-outer-regression', [sys.executable, source/'python/tests/run_installed.py',
                '--python', consumer_python, '--expected-version', '0.2.0', '--output', out/'outer-report.json'])
        external = out/'installed-consumer'
        external.mkdir()
        command(out/'15-transfer-regression', [consumer_python, '-I', source/'tools/ngap-transfer-sdk/installed_check.py',
                out/'vectors.json', provenance_path, out/'transfer-report.json'], cwd=external)
        summary.update(status='PASS_LOCAL_QUALIFICATION', compiler=version,
            prefix=str(prefix), wheel=str(wheel), wheel_sha256=sha(wheel), wheel_size_bytes=wheel.stat().st_size,
            identity={k: provenance[k] for k in ('version', 'fingerprint', 'schema_sha256', 'runtime_sha256', 'source_revision')},
            reports={name: sha(out/name) for name in ('outer-report.json', 'transfer-report.json', 'vectors.json')},
            publication='NOT_RUN', owner_acceptance='PENDING')
    except BaseException as error:
        summary.update(status='FAIL_OR_INTERRUPTED', diagnostic=str(error))
        raise
    finally:
        write(out/'summary.json', summary)
        print(json.dumps(summary, indent=2), flush=True)


if __name__ == '__main__':
    main()
