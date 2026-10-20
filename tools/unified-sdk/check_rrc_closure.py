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


def repair_test_snapshot(work, revision, git):
    """Resume only the reviewed parser-assert fixture failure, before generation."""
    previous = json.loads((work/'report.json').read_text())
    failure = work/'05-enum_elements'
    if (previous['status'] != 'FAIL_OR_INTERRUPTED' or
            (failure/'status').read_text().strip() != 'EXIT -6' or
            '!TQ_FIRST' not in (failure/'stderr.txt').read_text()):
        raise ValueError('Not the supported enum test parser-assert failure')
    if (work/'06-probe-support').exists() or (work/'rrc-generation').exists():
        raise ValueError('Generation already started; test-only recovery refused')
    old = previous['source_revision']
    paths = git('diff', '--name-only', old, revision).decode().splitlines()
    allowed = {'libasn1typed/check_asn1typed_enum_elements.c',
               'tools/unified-sdk/check_rrc_closure.py',
               'docs/unified-sdk-expanded-source-review.md'}
    if not paths or set(paths)-allowed:
        raise ValueError('Recovery contains changes beyond the reviewed test input repair')
    updates = {}
    for name in paths:
        before = git('show', old+':'+name)
        if (work/'source'/name).read_bytes() != before:
            raise ValueError('Recovery source preimage differs: '+name)
        updates[name] = (before, git('show', revision+':'+name))
    evidence = work/'test-input-recovery'; evidence.mkdir()
    write(evidence/'original-report.json', previous)
    changes = {}
    for name, (before, after) in updates.items():
        saved = evidence/'source-before'/name
        saved.parent.mkdir(parents=True, exist_ok=True); saved.write_bytes(before)
        path = work/'source'/name; path.write_bytes(after)
        changes[name] = {'before': sha(saved), 'after': sha(path)}
    write(evidence/'source-migration.json', dict(baseline=old, source_revision=revision, files=changes))
    return evidence


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--schema-root', type=Path, required=True)
    parser.add_argument('--work', type=Path, required=True)
    parser.add_argument('--resume-tests', action='store_true', help='Resume the reviewed parser assertion in the test input without rebuilding C libraries')
    args = parser.parse_args()
    resources = verify()
    if sys.platform != 'linux' or sys.version_info[:2] != (3, 12):
        raise RuntimeError('Linux CPython 3.12 is required')
    git = lambda *argv: subprocess.check_output(['git', '-C', str(REPO), *argv])
    if git('status', '--porcelain', '--untracked-files=no').strip():
        raise ValueError('Commit source changes before execution')
    revision = git('rev-parse', 'HEAD').decode().strip()
    work = args.work.expanduser().resolve()
    work.mkdir(parents=True, exist_ok=args.resume_tests)
    logs = repair_test_snapshot(work, revision, git) if args.resume_tests else work
    report = dict(status='RUNNING', source_revision=revision, resources=resources,
                  scope='C ownership regression and selected 91-root RRC sealed generation only',
                  sdk_build='NOT_RUN', publication='NOT_RUN', owner_acceptance='PENDING')
    write(work/'report.json', report)
    try:
        source = work/'source'
        if not args.resume_tests:
            archive = git('archive', revision, '--', '.', ':(exclude)verification',
                          ':(exclude)_codeql_detected_source_root', ':(exclude).vscode')
            write(work/'source-links.json', extract_source(archive, source))
        os.environ.update(CC='gcc-13', CXX='g++-13', CFLAGS='-O0 -g0',
                          CXXFLAGS='-O0 -g0', MAKEFLAGS='-j1',
                          CMAKE_BUILD_PARALLEL_LEVEL='1', PYTHONDONTWRITEBYTECODE='1')
        if not args.resume_tests:
            command(work/'01-autoreconf', ['autoreconf', '-iv'], cwd=source)
            command(work/'02-configure', [source/'configure'], cwd=source)
            for library in ('libasn1common', 'libasn1parser', 'libasn1fix', 'libasn1typed'):
                command(work/('03-'+library), ['make', '-j1', '-C', source/library],
                        timeout=21600, live=True)
        command(logs/'04-check-build', ['make', '-j1', '-C', source/'libasn1typed',
                'check_asn1typed_enum_elements', 'check_asn1typed_collection_elements'],
                timeout=3600, live=True)
        for name in ('enum_elements', 'collection_elements'):
            command(logs/('05-'+name), [source/('libasn1typed/check_asn1typed_'+name)], timeout=3600, live=True)
        # This target supplies developer_tree.o and validates extraction linkage.
        command(logs/'06-probe-support', ['make', '-j1', '-C', source/'tools',
                'asn1typed_ioc_probe'], timeout=3600, live=True)
        command(logs/'07-selected-rrc-generation', [sys.executable,
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
