#!/usr/bin/env python3
"""Strict wheel-only recovery with fixed inputs and a fresh extension build."""
import argparse, hashlib, json, shutil, subprocess, sys, tarfile, zipfile
from pathlib import Path

VERSION = '0.3.0'
WHEEL_NAME = 'nrforge_ngap-0.3.0-cp312-cp312-linux_x86_64.whl'
BUILD_PREFIXES = ('python/', 'cmake/', 'tools/', 'libaper/', 'libngap/')
QUALIFICATION_FILES = ('python/tests/run_installed.py', 'tools/ngap-transfer-sdk/installed_check.py')
REGISTERED_SOURCE_PATCHES = {
    'tools/unified-sdk/build_rrc.py': {'before_sha256': '087776820f70c0676d78d27b0b2274539ec4e2abeede16c659faa52f2aceb35f', 'after_sha256': '5195b924ff30aecbe9ff817d35f14fff2107ea633ec47e62ca65c5cbd02a4f2f', 'reason': 'historical recovery-chain input; not executed by this recovery'},
    'tools/unified-sdk/produce.py': {'before_sha256': 'e80b86b4ccce53bed52038ef5ad4eee39e8b588feb6a52665c4a053952f921f7', 'after_sha256': '5ab11faa6d28c98e1ac335001d8748c2123ed14d4f91846cb84f3935195175a1', 'reason': 'historical producer input; explicitly not resumed by R1'},
    'tools/unified-sdk/release.py': {'before_sha256': '4e89cc0e03f3cac0be28ea0b4e1e3604b117bc2e5a977da811696fc6172a40c2', 'after_sha256': '11cc1d247335c52857dc14594270a12fd498aa01009f70d949e315f6e7fca59d', 'reason': 'historical packaging-chain input; publish remains disabled'},
    'tools/unified-sdk/test_component_identities.py': {'before_sha256': 'b2770718f24ee91b179ca6a63cfda38b71eb9b227fac65361006d4f0c4851a31', 'after_sha256': 'ba10cd2d778e7d005e3e4ee9c1d8b36af26a136fb1c26b19abed61b5193f9746', 'reason': 'historical recovery-chain test; not executed by this recovery'},
    'tools/unified-sdk/test_release_guards.py': {'before_sha256': '9aac3af64b7c1870c4aa4ed234f00f34f209c89debc48389a64970159c337f47', 'after_sha256': '82e3ddb894cdb931e4f9157547a9cd6fcf7349bf0da6438219eb4b8fddf18081', 'reason': 'historical packaging-chain test; not executed by this recovery'},
    'tools/unified-sdk/check_rrc_closure.py': {'before_sha256': None, 'after_sha256': '4e6a7c6df6bf7c3e02131cf01bd77afe18c240d51776a3762418692b3c867d56', 'reason': 'historical recovery-chain input absent from old snapshot; not executed'},
    'tools/unified-sdk/resume_rrc.py': {'before_sha256': None, 'after_sha256': '3fde4046db5b6b90ac894b93907a4ca860feaca82a9c6b41e083c3122a11826d', 'reason': 'historical recovery-chain input absent from old snapshot; not executed'},
    'tools/unified-sdk/split_rrc.py': {'before_sha256': None, 'after_sha256': 'bfb7c68814022dd85151d237c0a1bddfd27ab8614872b15cd5cd9716de6cd2c3', 'reason': 'historical recovery-chain input absent from old snapshot; not executed'},
    'tools/unified-sdk/test_resume_rrc.py': {'before_sha256': None, 'after_sha256': '19298df16421a334d37b452873f0c788abda944e7fba1bbae114eef6d7fadb49', 'reason': 'historical recovery-chain test absent from old snapshot; not executed'},
    'tools/unified-sdk/test_split_rrc.py': {'before_sha256': None, 'after_sha256': '9a3ef428c11289ba7748d10693523548b59b01857306433a3566b96708eca6a7', 'reason': 'historical recovery-chain test absent from old snapshot; not executed'},
    'tools/ngap-transfer-sdk/resource_guard.py': {
        'before_sha256': 'aa62588c74070575aa2600923b3ec462061e243b5c2011504d4e649b8e4c697f',
        'after_sha256': 'f21c21640c7ab56f3d74f5e032776db8a0e9f3654044f909de7e228b9cbfa973',
        'reason': 'record CPU quota and period in guard evidence; required by R1 resource contract'},
    'tools/ngap-transfer-sdk/installed_check.py': {
        'before_sha256': 'aa8d0efd84eeeaf16cc425a34c103e231791d53cba98593a1e4fff5836afbb60',
        'after_sha256': 'e56447bc42237aebd6e3d950de05c1c5b6230671f8aa00a96a74b8def48119cc',
        'reason': 'fixed MAX_TRANSFER_OCTETS spelling; behavior and executed vectors unchanged'},
}

def sha(path):
    h = hashlib.sha256()
    with Path(path).open('rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''): h.update(block)
    return h.hexdigest()

def git_blob(data): return hashlib.sha1(b'blob ' + str(len(data)).encode() + b'\0' + data).hexdigest()

def read(path):
    try: return json.loads(Path(path).read_text())
    except (OSError, ValueError) as exc: raise ValueError('Unreadable JSON evidence: ' + str(path)) from exc

def require(condition, message):
    if not condition: raise ValueError(message)

def inventory(prefix):
    root = Path(prefix).resolve(); require(root.is_dir(), 'Missing SDK prefix: ' + str(root))
    result = {}
    for path in root.rglob('*'):
        require(not path.is_symlink(), 'SDK symlink found: ' + str(path))
        if path.is_file(): result[str(path.relative_to(root))] = sha(path)
    require(result, 'Empty SDK prefix: ' + str(root)); return result

def check_wheel(path, expected_digest=None):
    wheel = Path(path); require(wheel.name == WHEEL_NAME and wheel.is_file(), 'Unexpected wheel identity')
    actual = sha(wheel)
    if expected_digest: require(actual == expected_digest, 'Wheel bytes do not match recorded identity')
    with zipfile.ZipFile(wheel) as archive:
        require('nrforge_ngap/__init__.py' in archive.namelist(), 'Wheel ZIP inventory is invalid')
    return actual

def _tracked(repo):
    try:
        return subprocess.check_output(['git', '-C', str(repo), 'ls-files'], text=True).splitlines()
    except subprocess.CalledProcessError:
        return None

def source_manifest(repo, source, baseline):
    """Compare exact bytes; only two named, hashed recovery patches are allowed."""
    rows, patches = {}, {}
    repo_names = _tracked(repo); require(repo_names is not None, 'Repository tracked input inventory unavailable')
    source_names = _tracked(source)
    for name in repo_names:
        if not name.startswith(BUILD_PREFIXES): continue
        repo_path, source_path = Path(repo) / name, Path(source) / name
        if not source_path.exists():
            registered = REGISTERED_SOURCE_PATCHES.get(name)
            require(registered is not None or name in ('tools/unified-sdk/recover_wheel.py',
                    'tools/unified-sdk/test_recover_wheel.py'), 'Qualification source missing tracked input: ' + name)
            if registered is not None:
                require(registered['before_sha256'] is None and sha(repo_path) == registered['after_sha256'],
                        'Registered absent qualification patch bytes do not match: ' + name)
                patches[name] = dict(registered)
            else:
                patches[name] = {'before_sha256': None, 'after_sha256': sha(repo_path),
                                 'reason': 'new recovery implementation/test; not executed by qualification'}
            continue
        old = source_path.read_bytes(); new = repo_path.read_bytes()
        if old != new:
            registered = REGISTERED_SOURCE_PATCHES.get(name)
            require(registered is not None or name in ('tools/unified-sdk/recover_wheel.py',
                    'tools/unified-sdk/test_recover_wheel.py'), 'Qualification source differs for unregistered input: ' + name)
            if registered is not None:
                require(sha(source_path) == registered['before_sha256'] and sha(repo_path) == registered['after_sha256'],
                        'Registered qualification patch bytes do not match: ' + name)
                patches[name] = dict(registered)
            else:
                patches[name] = {'before_sha256': sha(source_path), 'after_sha256': sha(repo_path),
                                 'reason': 'R1-R3 recovery implementation/test revision'}
        rows[name] = {'sha256': sha(source_path), 'content_sha1': git_blob(old)}
    require(rows, 'No tracked build inputs were authenticated')
    if source_names is not None:
        expected = {n for n in repo_names if n.startswith(BUILD_PREFIXES)}
        extra = sorted(n for n in source_names if n.startswith(BUILD_PREFIXES) and n not in expected)
        require(not extra, 'Qualification source contains unregistered build inputs: ' + ', '.join(extra))
    for name in QUALIFICATION_FILES:
        require(name in rows and (name not in patches or name in REGISTERED_SOURCE_PATCHES),
                'Executed qualification input is not exact: ' + name)
    return {'baseline_revision': baseline, 'inputs': rows, 'registered_patches': patches}

def tool_hashes(repo, source):
    result = {}
    for name in ('tools/unified-sdk/recover_wheel.py', 'tools/unified-sdk/release.py',
                 'tools/ngap-transfer-sdk/resource_guard.py', 'python/ngap_sdk/generate_bindings.py',
                 'tools/ngap-transfer-sdk/installed_check.py'):
        qualification = Path(source) / name
        result[name] = {'repo': sha(Path(repo) / name),
                        'qualification': sha(qualification) if qualification.is_file() else None}
    return result

def check_reports(work, outer, transfer, component_revisions):
    a, b = read(outer), read(transfer)
    require(a.get('status') == 'PASS', 'New wheel outer report did not pass')
    require(a.get('native_message_count') == 131 and a.get('independent_ng_setup_count') == 3,
            'New wheel outer counts are invalid')
    require(a.get('identity', {}).get('source_revision') == component_revisions['ngap'],
            'New wheel outer identity mismatch')
    require(b.get('status') == 'PASS' and b.get('cases') == 143, 'New wheel Transfer report did not pass')
    require(b.get('vectors_sha256') == sha(work / 'vectors.json'), 'New wheel Transfer vector hash mismatch')
    require(b.get('identity', {}).get('source_revision') == component_revisions['ngap'],
            'New wheel Transfer identity mismatch')
    return {'outer': a, 'transfer': b}

def check_historical_scope(work):
    for label, name, count in (('F1AP', 'f1ap-outer.json', 158), ('E1AP', 'e1ap-outer.json', 72)):
        data = read(work / name)
        require(data.get('message_count') == count and isinstance(data.get('cases'), list),
                label + ' finite report count is invalid')
        require(all(row.get('status') == 'PASS' for row in data['cases']), label + ' finite report failed')
    require(read(work / 'e1ap-containing-audit.json').get('full_protocol_coverage') is False,
            'E1AP scope incorrectly claims full coverage')
    coexist = next(work.glob('coexist-*/ctest.log'), None)
    require(coexist and '100% tests passed' in coexist.read_text() and '0 tests failed' in coexist.read_text(),
            'Coexistence evidence failed')

def run_command(argv, cwd, record, name):
    out, err = record.parent / (name + '.stdout'), record.parent / (name + '.stderr')
    with out.open('wb') as stdout, err.open('wb') as stderr:
        result = subprocess.run(argv, cwd=cwd, stdout=stdout, stderr=stderr)
    data = read(record) if record.exists() else {}
    data.setdefault('commands', []).append({'name': name, 'argv': [str(x) for x in argv], 'cwd': str(cwd),
        'returncode': result.returncode, 'stdout_sha256': sha(out), 'stderr_sha256': sha(err)})
    record.write_text(json.dumps(data, indent=2, sort_keys=True) + '\n'); return result.returncode

def validate_old_candidate(old_candidate, work):
    manifest = read(old_candidate / 'candidate.json')
    require(manifest.get('version') == VERSION and manifest.get('qualification') == 'PASS_LOCAL_QUALIFICATION',
            'Old candidate is not a qualified 0.3.0 candidate')
    producer_path = old_candidate / 'producer.json'
    require(producer_path.is_file(), 'Old candidate producer is missing')
    require(sha(producer_path) == manifest.get('assets', {}).get('producer.json'),
            'Old candidate producer identity mismatch')
    producer = read(producer_path)
    require(producer.get('status') == 'PASS_LOCAL_QUALIFICATION', 'Old candidate producer is not qualified')
    require(producer.get('publication') == 'NOT_RUN' and producer.get('owner_acceptance') == 'PENDING',
            'Recovery cannot run after publication or acceptance')
    receipt = Path(producer.get('original_failure_receipt', '')); require(receipt.is_file(), 'Original failure receipt is missing')
    original = read(receipt)
    require(original.get('status') == 'FAIL_OR_INTERRUPTED' and '13-wheel' in original.get('diagnostic', ''),
            'Original failure receipt is not the recorded wheel failure')
    require((work / '13-wheel/status').read_text().strip() == 'EXIT 1', 'Original wheel stage status changed')
    wheel = old_candidate / WHEEL_NAME
    require(wheel.is_file() and sha(wheel) == manifest.get('assets', {}).get(WHEEL_NAME),
            'Old candidate wheel identity is missing or changed')
    require(producer.get('wheel_sha256') == sha(wheel), 'Old producer and candidate wheel differ')
    return producer, receipt.read_bytes(), receipt

def validate_inputs(repo, source, work, old_candidate):
    producer, original_bytes, original_path = validate_old_candidate(old_candidate, work)
    sdk, relocated = inventory(work / 'sdk'), inventory(work / 'relocated-sdk')
    require(sdk == relocated, 'sdk and relocated-sdk inventories differ')
    require(sdk == producer.get('sdk_files'), 'SDK inventory differs from old verified sdk_files')
    bundle = read(work / 'sdk/nrforge-asn1-bundle.json')
    require(bundle.get('version') == VERSION, 'SDK bundle version mismatch')
    require(bundle.get('source_revision') == producer.get('producer_revision'), 'SDK bundle source identity mismatch')
    require(bundle.get('component_source_revisions') == producer.get('component_source_revisions'),
            'SDK bundle component identity mismatch')
    source_inputs = source_manifest(repo, source, producer['qualification_source_revision'])
    tools = tool_hashes(repo, source); check_historical_scope(work)
    require((work / 'vectors.json').is_file(), 'Missing Transfer vectors')
    return {'producer': producer, 'original_bytes': original_bytes, 'original_path': original_path, 'sdk_files': sdk,
            'bundle': bundle, 'source_inputs': source_inputs, 'tool_hashes': tools,
            'old_wheel_sha256': producer['wheel_sha256'], 'historical_wheel': str(old_candidate / WHEEL_NAME)}

def check_guard(evidence):
    status, limit = read(evidence / 'guard-status.json'), read(evidence / 'resource-limit.json')
    require(status.get('status') == 'EXIT' and status.get('exit_code') == 0, 'resource_guard did not finish successfully')
    require(limit.get('memory_max') == 6 * 1024**3 and limit.get('tasks_max') == 128 and limit.get('swap_max') == 0,
            'resource guard contract is incomplete')
    require(limit.get('cpu_quota') in ('150%', '150000') and limit.get('cpu_period') > 0,
            'resource guard CPU contract is incomplete')
    return {'status': status, 'limit': limit}

def fixed_build_argv(repo, work, wheel_output, extension_build):
    return [sys.executable, '-I', '-m', 'build', '--wheel', '--no-isolation', str(repo / 'python'),
            '--outdir', str(wheel_output), '-Cbuild-dir=' + str(extension_build),
            '-Ccmake.define.NRFORGE_PYTHON_SDK_PREFIX=' + str(work / 'relocated-sdk'),
            '-Ccmake.define.CMAKE_EXE_LINKER_FLAGS=-fuse-ld=lld -Wl,--threads=1']

def exact_new_wheel(wheel_output):
    paths = sorted(wheel_output.iterdir())
    require([p.name for p in paths] == [WHEEL_NAME],
            'Guard succeeded but wheel output does not contain exactly the expected new wheel')
    return check_wheel(paths[0])

def recover(args):
    repo, source, work, old_candidate, evidence = map(Path, (args.repo, args.qualification_source, args.work,
        args.old_candidate, args.evidence))
    require(repo.resolve() == Path(__file__).resolve().parents[2], 'Recovery must run from this repository')
    require(not evidence.exists(), 'Recovery evidence directory reuse is forbidden')
    checked = validate_inputs(repo, source, work, old_candidate); evidence.mkdir(parents=True)
    (evidence / 'original-producer.json').write_bytes(checked['original_bytes'])
    shutil.copyfile(work / '13-wheel/status', evidence / 'original-wheel-status')
    shutil.copyfile(work / '13-wheel/stderr.txt', evidence / 'original-wheel-stderr.txt')
    wheel_output, extension_build = evidence / 'wheel-output', evidence / 'extension-build'
    wheel_output.mkdir(); extension_build.mkdir()
    command = fixed_build_argv(repo, work, wheel_output, extension_build)
    command_record = evidence / 'build-command.json'
    argv = [str(x) for x in command]
    command_record.write_text(json.dumps({'argv': argv, 'argv_sha256': hashlib.sha256(
        json.dumps(argv, separators=(',', ':')).encode()).hexdigest(), 'cwd': str(repo),
        'source': 'fixed-recovery-parameters'}, indent=2, sort_keys=True) + '\n')
    try:
        result = subprocess.run([sys.executable, str(repo / 'tools/ngap-transfer-sdk/resource_guard.py'),
            '--evidence', str(evidence / 'resource'), '--', *command], cwd=repo)
    except OSError as exc: raise ValueError('resource_guard invocation failed: ' + str(exc)) from exc
    (evidence / 'wheel-rebuild-status.json').write_text(json.dumps({'returncode': result.returncode}, indent=2) + '\n')
    guard = check_guard(evidence / 'resource'); wheel = wheel_output / WHEEL_NAME
    wheel_sha = exact_new_wheel(wheel_output)
    (evidence / 'wheel-identity.json').write_text(json.dumps({'path': str(wheel), 'sha256': wheel_sha}, indent=2) + '\n')
    commands = evidence / 'commands.json'; commands.write_text('{}\n'); venv = evidence / 'wheel-consumer-venv'
    require(run_command([sys.executable, '-m', 'venv', str(venv)], evidence, commands, 'create-venv') == 0,
            'Fresh wheel consumer venv creation failed')
    cp = venv / 'bin/python'
    require(run_command([str(cp), '-I', '-m', 'pip', 'install', '--no-index', '--no-deps', str(wheel)],
                         evidence, commands, 'install-wheel') == 0, 'Fresh wheel installation failed')
    outer, transfer = evidence / 'outer-report.json', evidence / 'transfer-report.json'
    require(run_command([str(cp), '-I', str(source / 'python/tests/run_installed.py'), '--python', str(cp),
                         '--expected-version', VERSION, '--output', str(outer)], evidence, commands, 'outer') == 0,
            'Fresh wheel outer qualification failed')
    require(run_command([str(cp), '-I', str(source / 'tools/ngap-transfer-sdk/installed_check.py'),
                         str(work / 'vectors.json'), str(work / 'relocated-sdk/share/nrforge-ngap/sdk-provenance.json'),
                         str(transfer)], evidence, commands, 'transfer') == 0, 'Fresh wheel Transfer qualification failed')
    reports = check_reports(work, outer, transfer, checked['producer']['component_source_revisions'])
    require(sha(wheel) == wheel_sha, 'Wheel changed during fresh installation checks')
    commit = subprocess.check_output(['git', '-C', str(repo), 'rev-parse', 'HEAD'], text=True).strip()
    receipt = {'status': 'PASS_LOCAL_QUALIFICATION', 'version': VERSION, 'recovery': 'wheel-only',
        'source_revision': commit, 'recovery_commit': commit, 'original_source_revision': checked['producer']['source_revision'],
        'original_producer_revision': checked['producer']['producer_revision'],
        'qualification_source_revision': checked['producer']['qualification_source_revision'],
        'component_source_revisions': checked['producer']['component_source_revisions'],
        'sdk_bundle_source_revision': checked['bundle']['source_revision'], 'source_inputs': checked['source_inputs'],
        'tool_hashes': checked['tool_hashes'], 'sdk_files': checked['sdk_files'], 'wheel': str(wheel),
        'wheel_sha256': wheel_sha, 'previous_wheel_sha256': checked['old_wheel_sha256'], 'fresh_venv': str(venv),
        'build_command': str(command_record), 'commands': str(commands),
        'reports': {str(p.relative_to(work)): sha(p) for p in (outer, transfer)},
        'report_identities': {'outer': reports['outer']['identity'], 'transfer': reports['transfer']['identity']},
        'vectors_sha256': sha(work / 'vectors.json'), 'guard': guard,
        'original_failure_receipt': str(evidence / 'original-producer.json'),
        'publication': 'NOT_RUN', 'owner_acceptance': 'PENDING'}
    receipt_path = evidence / 'qualification-receipt.json'; receipt_path.write_text(json.dumps(receipt, indent=2, sort_keys=True) + '\n')
    selected = [evidence / n for n in ('original-producer.json', 'original-wheel-status', 'original-wheel-stderr.txt',
        'build-command.json', 'wheel-rebuild-status.json', 'wheel-identity.json', 'commands.json',
        'create-venv.stdout', 'create-venv.stderr', 'install-wheel.stdout', 'install-wheel.stderr',
        'outer.stdout', 'outer.stderr', 'transfer.stdout', 'transfer.stderr', 'outer-report.json',
        'transfer-report.json', 'qualification-receipt.json')]
    selected += [evidence / 'resource' / n for n in ('guard-status.json', 'resource-limit.json', 'scope.stdout', 'scope.stderr')]
    (evidence / 'evidence-manifest.json').write_text(json.dumps({'files': {
        str(p.relative_to(work)): sha(p) for p in selected}}, indent=2, sort_keys=True) + '\n')
    archive = evidence / 'recovery-evidence.tar.gz'
    with tarfile.open(archive, 'w:gz') as tar:
        for path in selected + [evidence / 'evidence-manifest.json']: tar.add(path, arcname=str(path.relative_to(work)))
    producer = dict(checked['producer']); producer.update({'status': receipt['status'], 'source_revision': commit,
        'recovery_commit': commit, 'recovery_receipt': str(receipt_path), 'evidence_archive': str(archive),
        'wheel': str(wheel), 'wheel_sha256': wheel_sha, 'reports': receipt['reports'], 'prefix': str(work / 'sdk'),
        'sdk_files': checked['sdk_files'], 'publication': 'NOT_RUN', 'owner_acceptance': 'PENDING'})
    (work / 'producer.json').write_text(json.dumps(producer, indent=2, sort_keys=True) + '\n')
    print(json.dumps({'status': receipt['status'], 'receipt': str(receipt_path), 'wheel_sha256': wheel_sha}, sort_keys=True))

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('repo', 'qualification-source', 'work', 'old-candidate', 'evidence'):
        parser.add_argument('--' + name, type=Path, required=True)
    recover(parser.parse_args())

if __name__ == '__main__': main()
