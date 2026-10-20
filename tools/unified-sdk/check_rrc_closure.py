#!/usr/bin/env python3
"""Isolated Owner-terminal RRC generation gate; never alter existing SDK evidence."""
import argparse
import json
import os
from pathlib import Path
import subprocess
import sys

REPO = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO/'tools/ngap-transfer-sdk'))
sys.path.insert(0, str(REPO/'tools/m6003-transfer-qualification'))
from resource_guard import verify
from snapshot import extract_source
from support import command, write, sha


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--schema-root', type=Path, required=True)
    parser.add_argument('--work', type=Path, required=True)
    args = parser.parse_args()
    resources = verify()
    if sys.platform != 'linux' or sys.version_info[:2] != (3, 12):
        raise RuntimeError('Linux CPython 3.12 is required')
    git = lambda *argv: subprocess.check_output(['git', '-C', str(REPO), *argv])
    if git('status', '--porcelain', '--untracked-files=no').strip():
        raise ValueError('Commit source changes before execution')
    revision = git('rev-parse', 'HEAD').decode().strip()
    work = args.work.expanduser().resolve()
    work.mkdir(parents=True, exist_ok=False)
    report = dict(status='RUNNING', source_revision=revision, resources=resources,
                  scope='C ownership regression and selected 91-root RRC sealed generation only',
                  sdk_build='NOT_RUN', publication='NOT_RUN', owner_acceptance='PENDING')
    write(work/'report.json', report)
    try:
        source = work/'source'
        archive = git('archive', revision, '--', '.', ':(exclude)verification',
                      ':(exclude)_codeql_detected_source_root', ':(exclude).vscode')
        write(work/'source-links.json', extract_source(archive, source))
        os.environ.update(CC='gcc-13', CXX='g++-13', CFLAGS='-O0 -g0',
                          CXXFLAGS='-O0 -g0', MAKEFLAGS='-j1',
                          CMAKE_BUILD_PARALLEL_LEVEL='1', PYTHONDONTWRITEBYTECODE='1')
        command(work/'01-autoreconf', ['autoreconf', '-iv'], cwd=source)
        command(work/'02-configure', [source/'configure'], cwd=source)
        for library in ('libasn1common', 'libasn1parser', 'libasn1fix', 'libasn1typed'):
            command(work/('03-'+library), ['make', '-j1', '-C', source/library],
                    timeout=21600, live=True)
        command(work/'04-check-build', ['make', '-j1', '-C', source/'libasn1typed',
                'check_asn1typed_enum_elements', 'check_asn1typed_collection_elements'],
                timeout=3600, live=True)
        for name in ('enum_elements', 'collection_elements'):
            command(work/('05-'+name), [source/('libasn1typed/check_asn1typed_'+name)], timeout=3600, live=True)
        # This target supplies developer_tree.o and validates extraction linkage.
        command(work/'06-probe-support', ['make', '-j1', '-C', source/'tools',
                'asn1typed_ioc_probe'], timeout=3600, live=True)
        command(work/'07-selected-rrc-generation', [sys.executable,
                source/'tools/unified-sdk/build_rrc.py', '--asn1-root', args.schema_root.resolve(),
                '--work', work/'rrc-generation', '--prefix', work/'unused-prefix',
                '--jobs', '1', '--source-revision', revision, '--generate-only'],
                timeout=43200, live=True)
        matrix = json.loads((work/'rrc-generation/matrix.json').read_text())
        if len(matrix['roots']) != 91 or any(r['extract'] or r['render'] for r in matrix['roots']):
            raise ValueError('Incomplete selected RRC matrix')
        report.update(status='PASS_RRC_GENERATION_ONLY', selected_roots=91,
                      historical_85_headers='UNCHANGED', matrix_sha256=sha(work/'rrc-generation/matrix.json'),
                      qualification='SDK build, installed consumers and wire qualification still pending')
    except BaseException as error:
        report.update(status='FAIL_OR_INTERRUPTED', diagnostic=str(error))
        raise
    finally:
        write(work/'report.json', report)


if __name__ == '__main__':
    main()
