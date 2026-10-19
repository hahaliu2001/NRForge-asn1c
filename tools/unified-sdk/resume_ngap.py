"""Narrow recovery of the pre-install NGAP failure; keeps its evidence and objects."""
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys

ALLOWED = {
    'tools/ngap-transfer-sdk/prepare.py',
    'tools/ngap-transfer-sdk/test_root_header.py',
    'tools/unified-sdk/produce.py', 'tools/unified-sdk/resume_ngap.py',
    'tools/unified-sdk/test_resume_ngap.py', 'Makefile.am',
    'docs/unified-sdk-expanded-source-review.md',
}
SEALED = ('sdk-lock.cmake', 'sdk-provenance.json', 'sdk_version.hpp',
          'sdk_identity.cpp', 'sdk_link_check.cpp', 'sdk-public')


def source_delta(repo, source, old, new):
    def git(*args):
        return subprocess.check_output(['git', '-C', str(repo), *args])
    names = git('diff', '--name-only', old, new).decode().splitlines()
    if not names or set(names)-ALLOWED:
        raise ValueError('Recovery requires only the reviewed namespace/recovery changes')
    old_names = set(git('ls-tree', '-r', '--name-only', old).decode().splitlines())
    updates = {}
    for name in names:
        path = source/name
        before = git('show', old+':'+name) if name in old_names else None
        if (path.read_bytes() if path.exists() else None) != before:
            raise ValueError('Recovery source preimage mismatch: '+name)
        updates[name] = (before, git('show', new+':'+name))
    return updates


def recover(repo, out, head, command, write):
    previous = json.loads((out/'producer.json').read_text())
    old = previous['source_revision']
    source, stage = out/'source', out/'ngap-build'
    generated, build = stage/'generated', stage/'build'
    prefix = out/'ngap-prefix'
    if previous['status'] != 'FAIL_OR_INTERRUPTED' or (out/'05-ngap/status').read_text().strip() != 'EXIT 1':
        raise ValueError('Only a failed NGAP component build may be resumed')
    if any((out/(p+'-build')).exists() for p in ('f1ap', 'e1ap', 'rrc')) or (out/'sdk').exists():
        raise ValueError('Later stages already exist; recovery refused')
    if prefix.exists() and any(prefix.iterdir()):
        raise ValueError('NGAP installation must not have started')
    if not (build/'CMakeCache.txt').is_file():
        raise ValueError('No configured NGAP object cache to recover')
    updates = source_delta(repo, source, old, head)
    # Verify every old entry before touching any generated input.
    transfer_manifest = json.loads((generated/'transfer-manifest.json').read_text())
    expected_old = '#pragma once\n#include "root_types.hpp"\n#include "root_mapping.hpp"\n#include "root_codec.hpp"\n'
    roots = {}
    for row in transfer_manifest['roots'].values():
        path = generated/row['headers'][0]
        if not path.resolve().is_relative_to(generated.resolve()) or path.read_text() != expected_old:
            raise ValueError('Unexpected Transfer root header preimage')
        roots[path.relative_to(generated)] = path.read_bytes()
    if len(roots) != 26:
        raise ValueError('Recovery requires all 26 standalone roots')
    evidence = out/'namespace-recovery'
    evidence.mkdir(exist_ok=False)
    # Validate the original sealed source/generated inputs BEFORE changing them.
    command(evidence/'01-original-lock', ['cmake', '-DNRFORGE_SOURCE_ROOT='+str(source),
        '-DNRFORGE_GENERATED_ROOT='+str(generated), '-P', source/'cmake/VerifyNgapSdk.cmake'])
    write(evidence/'original-producer.json', previous)
    shutil.copytree(out/'05-ngap', evidence/'original-05-ngap')
    shutil.copyfile(stage/'build.log', evidence/'original-build.log')
    archive = evidence/'original-seal'; archive.mkdir()
    for name in SEALED:
        shutil.copytree(generated/name, archive/name) if (generated/name).is_dir() else shutil.copyfile(generated/name, archive/name)
    for relative, before in roots.items():
        saved = evidence/'original-root-headers'/relative
        saved.parent.mkdir(parents=True, exist_ok=True); saved.write_bytes(before)
    changes = {}
    for name, (before, after) in updates.items():
        saved = evidence/'source-before'/name
        if before is not None:
            saved.parent.mkdir(parents=True, exist_ok=True); saved.write_bytes(before)
        changes[name] = {'before': hashlib.sha256(before).hexdigest() if before is not None else None,
                         'after': hashlib.sha256(after).hexdigest()}
    write(evidence/'source-migration.json', dict(baseline=old, source_revision=head, files=changes))
    # Only reviewed tracked files change; raw generated adapters and .o files stay.
    for name, (_, after) in updates.items():
        path = source/name; path.parent.mkdir(parents=True, exist_ok=True); path.write_bytes(after)
    import importlib.util
    spec = importlib.util.spec_from_file_location('recovered_transfer_prepare', source/'tools/ngap-transfer-sdk/prepare.py')
    prepare = importlib.util.module_from_spec(spec); spec.loader.exec_module(prepare)
    for relative in roots:
        (generated/relative).write_text(prepare.root_header(relative.parent.name))
    write(evidence/'root-header-migration.json', {str(relative): {
        'before': hashlib.sha256(before).hexdigest(),
        'after': hashlib.sha256((generated/relative).read_bytes()).hexdigest()} for relative, before in roots.items()})
    for name in SEALED:
        path = generated/name
        shutil.rmtree(path) if path.is_dir() else path.unlink()
    command(evidence/'02-reseal', [sys.executable, source/'tools/ngap-dispatch/seal_sdk.py',
        '--repo', source, '--generated', generated, '--version', '0.3.0', '--source-revision', head])
    command(evidence/'03-configure', ['cmake', '-S', generated, '-B', build,
        '-DNRFORGE_SOURCE_ROOT='+str(source), '-DCMAKE_INSTALL_PREFIX='+str(prefix),
        '-DCMAKE_BUILD_TYPE=', '-DCMAKE_EXE_LINKER_FLAGS=-fuse-ld=lld -Wl,--threads=1',
        '-DCMAKE_EXPORT_COMPILE_COMMANDS=ON',
        '-DCMAKE_CXX_FLAGS=-O0 -g0 --param=ggc-min-expand=5 --param=ggc-min-heapsize=16384 -Wall -Wextra -Werror -pedantic-errors -Wconversion -Wsign-conversion'])
    command(evidence/'04-build', ['cmake', '--build', build, '--parallel', '1'], timeout=43200, live=True)
    command(evidence/'05-install', ['cmake', '--install', build], timeout=3600)
    write(evidence/'result.json', dict(status='NGAP_RECOVERED_UNIFIED_QUALIFICATION_PENDING',
        baseline=old, source_revision=head, original_failure_preserved=True))
    return dict(baseline=old, evidence=str(evidence))
