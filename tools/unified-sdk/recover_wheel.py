#!/usr/bin/env python3
"""Strict, wheel-only recovery from the preserved 0.3.0 failure receipt."""
import argparse
import hashlib
import json
import shutil
import subprocess
import sys
import tarfile
import zipfile
from pathlib import Path

VERSION = '0.3.0'
WHEEL_NAME = 'nrforge_ngap-0.3.0-cp312-cp312-linux_x86_64.whl'
BUILD_PREFIXES = ('python/', 'cmake/', 'tools/', 'libaper/', 'libngap/')
RECOVERY_FILES = {
    'tools/unified-sdk/recover_wheel.py', 'tools/unified-sdk/test_recover_wheel.py',
    'tools/unified-sdk/release.py', 'tools/ngap-transfer-sdk/resource_guard.py',
    'tools/ngap-transfer-sdk/installed_check.py', 'tools/unified-sdk/build_rrc.py',
    'tools/unified-sdk/split_rrc.py', 'tools/unified-sdk/produce.py',
    'tools/unified-sdk/resume_rrc.py', 'tools/unified-sdk/test_component_identities.py',
    'tools/unified-sdk/test_resume_rrc.py', 'tools/unified-sdk/test_split_rrc.py',
}


def sha(path):
    digest = hashlib.sha256()
    with Path(path).open('rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            digest.update(block)
    return digest.hexdigest()


def git_blob(data):
    return hashlib.sha1(b'blob ' + str(len(data)).encode() + b'\0' + data).hexdigest()


def read(path):
    try:
        return json.loads(Path(path).read_text())
    except (OSError, ValueError) as exc:
        raise ValueError('Unreadable JSON evidence: ' + str(path)) from exc


def require(condition, message):
    if not condition:
        raise ValueError(message)


def inventory(prefix):
    root = Path(prefix).resolve()
    require(root.is_dir(), 'Missing SDK prefix: ' + str(root))
    result = {}
    for path in root.rglob('*'):
        require(not path.is_symlink(), 'SDK symlink found: ' + str(path))
        if path.is_file():
            result[str(path.relative_to(root))] = sha(path)
    require(result, 'Empty SDK prefix: ' + str(root))
    return result


def check_wheel(path, expected_digest=None):
    wheel = Path(path)
    require(wheel.name == WHEEL_NAME and wheel.is_file(), 'Unexpected wheel identity')
    actual = sha(wheel)
    if expected_digest:
        require(actual == expected_digest, 'Wheel bytes do not match recorded identity')
    with zipfile.ZipFile(wheel) as archive:
        require('nrforge_ngap/__init__.py' in archive.namelist(), 'Wheel ZIP inventory is invalid')
    return actual


def source_manifest(repo, source, baseline):
    names = subprocess.check_output(['git', '-C', str(repo), 'ls-files'], text=True).splitlines()
    rows, patches = {}, {}
    for name in names:
        if not name.startswith(BUILD_PREFIXES):
            continue
        repo_path, source_path = Path(repo) / name, Path(source) / name
        repo_data = repo_path.read_bytes()
        if not source_path.exists():
            require(name in RECOVERY_FILES, 'Qualification source missing tracked input: ' + name)
            patches[name] = {'kind': 'new-recovery-input', 'repo_sha256': sha(repo_path)}
            continue
        source_data = source_path.read_bytes()
        if repo_data != source_data:
            require(name in RECOVERY_FILES, 'Unexpected qualification source patch: ' + name)
            patches[name] = {'kind': 'registered-recovery-patch',
                             'baseline_sha256': hashlib.sha256(source_data).hexdigest(),
                             'repo_sha256': sha(repo_path)}
        rows[name] = {'sha256': sha(source_path), 'content_sha1': git_blob(source_data)}
    require(rows, 'No tracked build inputs were authenticated')
    return {'baseline_revision': baseline, 'inputs': rows, 'registered_patches': patches}


def tool_hashes(repo, source):
    names = ('tools/unified-sdk/recover_wheel.py', 'tools/unified-sdk/release.py',
             'tools/ngap-transfer-sdk/resource_guard.py',
             'python/ngap_sdk/generate_bindings.py', 'tools/ngap-transfer-sdk/installed_check.py')
    result = {}
    for name in names:
        for label, root in (('repo', repo), ('qualification', source)):
            path = Path(root) / name
            if not path.is_file() and label == 'qualification' and name in RECOVERY_FILES:
                result.setdefault(name, {})[label] = None
            else:
                require(path.is_file(), 'Missing executing tool: ' + str(path))
                result.setdefault(name, {})[label] = sha(path)
    return result


def check_reports(work, outer, transfer, component_revisions):
    outer_data, transfer_data = read(outer), read(transfer)
    require(outer_data.get('status') == 'PASS', 'New wheel outer report did not pass')
    require(outer_data.get('native_message_count') == 131 and outer_data.get('independent_ng_setup_count') == 3,
            'New wheel outer counts are invalid')
    require(outer_data.get('identity', {}).get('source_revision') == component_revisions['ngap'],
            'New wheel outer identity mismatch')
    require(transfer_data.get('status') == 'PASS' and transfer_data.get('cases') == 143,
            'New wheel Transfer report did not pass')
    require(transfer_data.get('vectors_sha256') == sha(work / 'vectors.json'),
            'New wheel Transfer vector hash mismatch')
    require(transfer_data.get('identity', {}).get('source_revision') == component_revisions['ngap'],
            'New wheel Transfer identity mismatch')
    return {'outer': outer_data, 'transfer': transfer_data}


def check_historical_scope(work):
    f1, e1 = read(work / 'f1ap-outer.json'), read(work / 'e1ap-outer.json')
    for label, data, count in (('F1AP', f1, 158), ('E1AP', e1, 72)):
        require(data.get('message_count') == count and isinstance(data.get('cases'), list),
                label + ' finite report count is invalid')
        require(all(row.get('status') == 'PASS' for row in data['cases']), label + ' finite report failed')
    audit = read(work / 'e1ap-containing-audit.json')
    require(audit.get('full_protocol_coverage') is False, 'E1AP scope incorrectly claims full coverage')
    coexist = (work / 'coexist-retry-20261020T144408Z/ctest.log').read_text()
    require('100% tests passed' in coexist and '0 tests failed' in coexist, 'Coexistence evidence failed')


def run_command(argv, cwd, record, name):
    out, err = record.parent / (name + '.stdout'), record.parent / (name + '.stderr')
    result = subprocess.run(argv, cwd=cwd, stdout=out.open('wb'), stderr=err.open('wb'))
    data = read(record) if record.exists() else {}
    data.setdefault('commands', []).append({'name': name, 'argv': [str(x) for x in argv], 'cwd': str(cwd),
        'returncode': result.returncode, 'stdout_sha256': sha(out), 'stderr_sha256': sha(err)})
    record.write_text(json.dumps(data, indent=2, sort_keys=True) + '\n')
    return result.returncode


def validate_old_candidate(old_candidate, work):
    manifest = read(old_candidate / 'candidate.json')
    require(manifest.get('version') == VERSION and manifest.get('qualification') == 'PASS_LOCAL_QUALIFICATION',
            'Old candidate is not a qualified 0.3.0 candidate')
    producer_path = old_candidate / 'producer.json'
    require(sha(producer_path) == manifest.get('assets', {}).get('producer.json'),
            'Old candidate producer identity mismatch')
    producer = read(producer_path)
    require(producer.get('status') == 'PASS_LOCAL_QUALIFICATION', 'Old candidate producer is not qualified')
    original_path = Path(producer.get('original_failure_receipt', ''))
    require(original_path.is_file(), 'Original failure receipt is missing')
    original_bytes = original_path.read_bytes()
    original = json.loads(original_bytes)
    require(original.get('status') == 'FAIL_OR_INTERRUPTED' and '13-wheel' in original.get('diagnostic', ''),
            'Original failure receipt is not the recorded wheel failure')
    require((work / '13-wheel/status').read_text().strip() == 'EXIT 1', 'Original wheel stage status changed')
    return producer, original_bytes, original_path


def validate_inputs(repo, source, work, old_candidate):
    producer, original_bytes, original_path = validate_old_candidate(old_candidate, work)
    require(producer.get('publication') == 'NOT_RUN' and producer.get('owner_acceptance') == 'PENDING',
            'Recovery cannot run after publication or acceptance')
    require(producer.get('component_source_revisions', {}).get('rrc') == producer.get('qualification_source_revision'),
            'Component and qualification source revisions diverge')
    sdk, relocated = inventory(work / 'sdk'), inventory(work / 'relocated-sdk')
    require(sdk == relocated, 'sdk and relocated-sdk inventories differ')
    bundle = read(work / 'sdk/nrforge-asn1-bundle.json')
    require(bundle.get('version') == VERSION, 'SDK bundle version mismatch')
    require(bundle.get('source_revision') == producer.get('producer_revision'), 'SDK bundle source identity mismatch')
    require(bundle.get('component_source_revisions') == producer.get('component_source_revisions'),
            'SDK component source identity mismatch')
    source_inputs = source_manifest(repo, source, producer['qualification_source_revision'])
    tools = tool_hashes(repo, source)
    check_historical_scope(work)
    old_wheel = Path(producer['wheel'])
    require(old_wheel.is_file() and sha(old_wheel) == producer['wheel_sha256'],
            'Pre-recovery wheel does not match old qualification receipt')
    require((work / 'vectors.json').is_file(), 'Missing Transfer vectors')
    return {'producer': producer, 'original_bytes': original_bytes, 'original_path': original_path,
            'sdk_files': sdk, 'bundle': bundle, 'source_inputs': source_inputs, 'tool_hashes': tools,
            'old_wheel_sha256': producer['wheel_sha256']}


def check_guard(evidence):
    status = read(evidence / 'resource/guard-status.json')
    limit = read(evidence / 'resource/resource-limit.json')
    require(status.get('status') == 'EXIT' and status.get('exit_code') == 0,
            'resource_guard did not finish successfully')
    require(limit.get('memory_max') == 6 * 1024**3 and limit.get('tasks_max') == 128 and limit.get('swap_max') == 0,
            'resource guard contract is incomplete')
    require(limit.get('cpu_quota') == '150%' and limit.get('cpu_period') > 0,
            'resource guard CPU contract is incomplete')
    return {'status': status, 'limit': limit}


def recover(args):
    repo, source, work, old_candidate, evidence = map(Path, (args.repo, args.qualification_source, args.work, args.old_candidate, args.evidence))
    require(repo.resolve() == Path(__file__).resolve().parents[2], 'Recovery must run from this repository')
    require(not evidence.exists(), 'Recovery evidence directory reuse is forbidden')
    checked = validate_inputs(repo, source, work, old_candidate)
    evidence.mkdir(parents=True)
    (evidence / 'original-producer.json').write_bytes(checked['original_bytes'])
    shutil.copyfile(work / '13-wheel/status', evidence / 'original-wheel-status')
    shutil.copyfile(work / '13-wheel/stderr.txt', evidence / 'original-wheel-stderr.txt')
    command = read(work / '13-wheel/command.json')['argv']
    require(all('produce.py' not in part and 'resume_ngap.py' not in part for part in command),
            'Wheel command invokes a forbidden producer entry point')
    guard = repo / 'tools/ngap-transfer-sdk/resource_guard.py'
    guard_result = subprocess.run([sys.executable, str(guard), '--evidence', str(evidence / 'resource'), '--', *command], cwd=repo)
    (evidence / 'wheel-rebuild-status.json').write_text(json.dumps({'returncode': guard_result.returncode}, indent=2) + '\n')
    guard_data = check_guard(evidence)
    wheel = Path(checked['producer']['wheel'])
    require(wheel.is_file() and wheel.name == WHEEL_NAME, 'Guarded rebuild did not produce the expected wheel')
    wheel_sha = sha(wheel)
    check_wheel(wheel)
    (evidence / 'wheel-identity.json').write_text(json.dumps({'path': str(wheel), 'sha256': wheel_sha}, indent=2) + '\n')
    commands = evidence / 'commands.json'; commands.write_text('{}\n')
    venv = evidence / 'wheel-consumer-venv'
    require(run_command([sys.executable, '-m', 'venv', str(venv)], evidence, commands, 'create-venv') == 0,
            'Fresh wheel consumer venv creation failed')
    cp = venv / 'bin/python'
    require(run_command([str(cp), '-I', '-m', 'pip', 'install', '--no-index', '--no-deps', str(wheel)], evidence, commands, 'install-wheel') == 0,
            'Fresh wheel installation failed')
    outer, transfer = evidence / 'outer-report.json', evidence / 'transfer-report.json'
    require(run_command([str(cp), '-I', str(source / 'python/tests/run_installed.py'), '--python', str(cp),
                         '--expected-version', VERSION, '--output', str(outer)], evidence, commands, 'outer') == 0,
            'Fresh wheel outer qualification failed')
    require(run_command([str(cp), '-I', str(source / 'tools/ngap-transfer-sdk/installed_check.py'),
                         str(work / 'vectors.json'), str(work / 'relocated-sdk/share/nrforge-ngap/sdk-provenance.json'), str(transfer)],
                        evidence, commands, 'transfer') == 0, 'Fresh wheel Transfer qualification failed')
    reports = check_reports(work, outer, transfer, checked['producer']['component_source_revisions'])
    require(sha(wheel) == wheel_sha, 'Wheel changed during fresh installation checks')
    recovery_commit = subprocess.check_output(['git', '-C', str(repo), 'rev-parse', 'HEAD'], text=True).strip()
    receipt = {'status': 'PASS_LOCAL_QUALIFICATION', 'version': VERSION, 'recovery': 'wheel-only',
        'source_revision': recovery_commit, 'recovery_commit': recovery_commit,
        'original_source_revision': checked['producer']['source_revision'],
        'original_producer_revision': checked['producer']['producer_revision'],
        'qualification_source_revision': checked['producer']['qualification_source_revision'],
        'component_source_revisions': checked['producer']['component_source_revisions'],
        'sdk_bundle_source_revision': checked['bundle']['source_revision'],
        'source_inputs': checked['source_inputs'], 'tool_hashes': checked['tool_hashes'],
        'sdk_files': checked['sdk_files'], 'wheel': str(wheel), 'wheel_sha256': wheel_sha,
        'previous_wheel_sha256': checked['old_wheel_sha256'], 'fresh_venv': str(venv),
        'commands': str(commands), 'reports': {str(p.relative_to(work)): sha(p) for p in (outer, transfer)},
        'report_identities': {'outer': reports['outer']['identity'], 'transfer': reports['transfer']['identity']},
        'vectors_sha256': sha(work / 'vectors.json'), 'guard': guard_data,
        'original_failure_receipt': str(evidence / 'original-producer.json'),
        'publication': 'NOT_RUN', 'owner_acceptance': 'PENDING'}
    receipt_path = evidence / 'qualification-receipt.json'
    receipt_path.write_text(json.dumps(receipt, indent=2, sort_keys=True) + '\n')
    selected = [evidence / name for name in ('original-producer.json', 'original-wheel-status', 'original-wheel-stderr.txt',
        'wheel-rebuild-status.json', 'wheel-identity.json', 'commands.json', 'create-venv.stdout',
        'create-venv.stderr', 'install-wheel.stdout', 'install-wheel.stderr',
        'outer.stdout', 'outer.stderr', 'transfer.stdout', 'transfer.stderr',
        'outer-report.json', 'transfer-report.json', 'qualification-receipt.json')]
    selected += [evidence / 'resource/guard-status.json', evidence / 'resource/resource-limit.json',
                 evidence / 'resource/scope.stdout', evidence / 'resource/scope.stderr']
    manifest = {'files': {str(p.relative_to(work)): sha(p) for p in selected}}
    (evidence / 'evidence-manifest.json').write_text(json.dumps(manifest, indent=2, sort_keys=True) + '\n')
    archive = evidence / 'recovery-evidence.tar.gz'
    with tarfile.open(archive, 'w:gz') as tar:
        for path in selected + [evidence / 'evidence-manifest.json']:
            tar.add(path, arcname=str(path.relative_to(work)))
    producer = dict(checked['producer'])
    producer.update({'status': receipt['status'], 'source_revision': recovery_commit, 'recovery_commit': recovery_commit,
        'recovery_receipt': str(receipt_path), 'evidence_archive': str(archive), 'wheel': str(wheel),
        'wheel_sha256': wheel_sha, 'reports': receipt['reports'], 'prefix': str(work / 'sdk'),
        'sdk_files': checked['sdk_files'], 'publication': 'NOT_RUN', 'owner_acceptance': 'PENDING'})
    (work / 'producer.json').write_text(json.dumps(producer, indent=2, sort_keys=True) + '\n')
    print(json.dumps({'status': receipt['status'], 'receipt': str(receipt_path), 'wheel_sha256': wheel_sha}, sort_keys=True))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('repo', 'qualification-source', 'work', 'old-candidate', 'evidence'):
        parser.add_argument('--' + name, type=Path, required=True)
    recover(parser.parse_args())


if __name__ == '__main__':
    main()
