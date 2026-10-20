#!/usr/bin/env python3
"""Recover only the interrupted 0.3.0 wheel stage from sealed evidence.

This entry point never runs produce.py or resume_ngap.py.  It validates the
old receipt and all immutable inputs, archives the old receipt byte-for-byte,
then reruns the cached wheel build under resource_guard before closing the
qualification receipt.
"""
import argparse
import hashlib
import json
import shutil
import subprocess
import sys
import zipfile
from pathlib import Path

VERSION = '0.3.0'
WHEEL_NAME = 'nrforge_ngap-0.3.0-cp312-cp312-linux_x86_64.whl'
RECORDED_WHEEL_SHA256 = '6f0b521ea7a2b9f59387c132ddf2f0f4cf2a3ebe8dffc84771b719c1e6837c0a'
FIXED_FILES = (
    'python/ngap_sdk/generate_bindings.py',
    'tools/ngap-transfer-sdk/installed_check.py',
)
REPORTS = (
    'outer-report.json', 'transfer-report.json', 'vectors.json',
    'f1ap-outer.json', 'e1ap-outer.json', 'f1-oracle/report.json',
    'e1ap-containing-audit.json', 'coexist-retry-20261020T144408Z/ctest.log',
)


def sha(path):
    digest = hashlib.sha256()
    with Path(path).open('rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            digest.update(block)
    return digest.hexdigest()


def read(path):
    try:
        return json.loads(Path(path).read_text())
    except (OSError, ValueError) as exc:
        raise ValueError('Unreadable JSON evidence: ' + str(path)) from exc


def files(prefix):
    root = Path(prefix).resolve()
    if not root.is_dir():
        raise ValueError('Missing installed SDK prefix: ' + str(root))
    result = {}
    for path in root.rglob('*'):
        if path.is_symlink():
            raise ValueError('SDK contains a symlink: ' + str(path))
        if path.is_file():
            result[str(path.relative_to(root))] = sha(path)
    if not result:
        raise ValueError('Installed SDK is empty: ' + str(root))
    return result


def require(condition, message):
    if not condition:
        raise ValueError(message)


def check_report(path, expected_status=None):
    data = read(path)
    if expected_status is not None:
        require(data.get('status') == expected_status,
                'Report did not pass: ' + str(path))
    return data


def check_case_statuses(data, label):
    cases = data.get('cases')
    require(isinstance(cases, list) and cases, label + ' has no cases')
    require(all(isinstance(row, dict) and row.get('status') == 'PASS' for row in cases),
            label + ' contains a non-PASS case')


def source_identity(repo, source, original_revision, producer_revision):
    result = {'producer_revision': producer_revision, 'files': {}}
    for relative in FIXED_FILES:
        repo_path, source_path = Path(repo) / relative, Path(source) / relative
        require(repo_path.is_file() and source_path.is_file(), 'Missing source identity file: ' + relative)
        current = repo_path.read_bytes()
        qualification = source_path.read_bytes()
        require(current == qualification, 'Repository/qualification source mismatch: ' + relative)
        git_blob = subprocess.check_output(['git', '-C', str(repo), 'hash-object', relative], text=True).strip()
        original = subprocess.check_output(['git', '-C', str(repo), 'show', original_revision + ':' + relative])
        result['files'][relative] = {
            'original_revision': original_revision,
            'original_git_blob_sha1': hashlib.sha1(original).hexdigest(),
            'original_sha256': hashlib.sha256(original).hexdigest(),
            'current_sha256': sha(repo_path),
            'qualification_sha256': sha(source_path),
            'current_git_blob_sha1': git_blob,
        }
    return result


def check_wheel(path, expected_digest):
    wheel = Path(path)
    require(wheel.name == WHEEL_NAME and wheel.is_file(), 'Unexpected wheel identity')
    if expected_digest:
        require(sha(wheel) == expected_digest, 'Wheel bytes do not match recorded identity')
    with zipfile.ZipFile(wheel) as archive:
        names = archive.namelist()
        require(len(names) == len(set(names)) and 'nrforge_ngap/__init__.py' in names,
                'Wheel ZIP inventory is invalid')
    return sha(wheel)


def validate_inputs(repo, source, work, expected_wheel_digest=RECORDED_WHEEL_SHA256):
    producer_path = work / 'producer.json'
    original_bytes = producer_path.read_bytes()
    producer = read(producer_path)
    resumable = producer.get('status') == 'FAIL_OR_INTERRUPTED'
    closed_retry = producer.get('status') == 'PASS_LOCAL_QUALIFICATION' and producer.get('recovery') == 'wheel-only'
    require(resumable or closed_retry, 'Recovery requires FAIL_OR_INTERRUPTED producer')
    if resumable:
        require('13-wheel' in producer.get('diagnostic', ''), 'Recovery requires the recorded 13-wheel failure')
    require((work / '13-wheel/status').read_text().strip() == 'EXIT 1', 'Wrong failed stage')
    require((work / '13-wheel/stderr.txt').is_file() and (work / '13-wheel/stdout.txt').is_file(),
            'Original wheel failure evidence is incomplete')
    require(producer.get('version') == VERSION, 'Wrong producer version')
    require(producer.get('publication') == 'NOT_RUN' and producer.get('owner_acceptance') == 'PENDING',
            'Recovery cannot run after publication or acceptance')
    require(producer.get('component_source_revisions', {}).get('rrc') == producer.get('qualification_source_revision'),
            'RRC component and qualification source revisions diverge')
    require((work / 'sdk').is_dir() and (work / 'relocated-sdk').is_dir(), 'SDK prefixes are missing')
    sdk = files(work / 'sdk')
    relocated = files(work / 'relocated-sdk')
    require(sdk == relocated, 'sdk and relocated-sdk inventories differ')
    bundle = read(work / 'sdk/nrforge-asn1-bundle.json')
    require(bundle.get('version') == VERSION, 'SDK bundle version mismatch')
    require(bundle.get('source_revision') == producer.get('producer_revision'), 'SDK producer identity mismatch')
    require(bundle.get('qualification_source_revision') == producer.get('qualification_source_revision'),
            'SDK qualification source identity mismatch')
    require(bundle.get('component_source_revisions') == producer.get('component_source_revisions'),
            'SDK component source identities mismatch')
    source_record = source_identity(repo, source, producer['producer_revision'], producer['producer_revision'])
    wheel = work / 'dist' / WHEEL_NAME
    wheel_digest = check_wheel(wheel, expected_wheel_digest)
    outer = check_report(work / 'outer-report.json', 'PASS')
    transfer = check_report(work / 'transfer-report.json', 'PASS')
    require(outer.get('native_message_count') == 131 and outer.get('independent_ng_setup_count') == 3,
            'Outer report counts are not the qualified result')
    require(outer.get('identity', {}).get('source_revision') == producer['component_source_revisions']['ngap'],
            'Outer report identity mismatch')
    require(transfer.get('cases') == 143 and transfer.get('vectors_sha256') == sha(work / 'vectors.json'),
            'Transfer report vector evidence mismatch')
    require(transfer.get('identity', {}).get('source_revision') == producer['component_source_revisions']['ngap'],
            'Transfer report identity mismatch')
    f1, e1 = check_report(work / 'f1ap-outer.json'), check_report(work / 'e1ap-outer.json')
    check_case_statuses(f1, 'F1AP finite outer report')
    check_case_statuses(e1, 'E1AP finite outer report')
    require(f1.get('message_count') == 158 and e1.get('message_count') == 72,
            'F1AP/E1AP finite result counts changed')
    oracle = check_report(work / 'f1-oracle/report.json', 'PASS_FINITE_ORACLE_WITNESSES')
    check_report(work / 'e1ap-containing-audit.json')
    require(read(work / 'e1ap-containing-audit.json').get('full_protocol_coverage') is False,
            'E1AP scope must remain finite, not full protocol coverage')
    coexist_log = (work / 'coexist-retry-20261020T144408Z/ctest.log').read_text()
    require('100% tests passed' in coexist_log and '0 tests failed' in coexist_log,
            'Four-protocol coexistence evidence is not PASS')
    return {
        'original_producer_bytes': original_bytes,
        'producer': producer,
        'sdk_files': sdk,
        'wheel': str(wheel),
        'wheel_sha256': wheel_digest,
        'source_identity': source_record,
        'reports': {name: sha(work / name) for name in REPORTS if (work / name).is_file()},
        'report_identities': {'outer': outer['identity'], 'transfer': transfer['identity']},
        'vectors_sha256': sha(work / 'vectors.json'),
        'scope': bundle.get('scope'),
        'oracle_status': oracle['status'],
    }


def run_guard(repo, work, evidence, source):
    command = read(work / '13-wheel/command.json')['argv']
    require(all('produce.py' not in part and 'resume_ngap.py' not in part for part in command),
            'Wheel command unexpectedly invokes a producer recovery entry point')
    guard = Path(repo) / 'tools/ngap-transfer-sdk/resource_guard.py'
    result = subprocess.run([sys.executable, str(guard), '--evidence', str(evidence / 'resource'), '--', *command],
                            cwd=repo)
    require(result.returncode == 0, 'Guarded wheel rebuild failed')
    require((evidence / 'resource/resource-limit.json').is_file(), 'Missing guarded resource receipt')


def recover(args):
    repo, source, work, evidence = map(Path, (args.repo, args.qualification_source, args.work, args.evidence))
    require(repo.resolve() == Path(__file__).resolve().parents[2], 'Recovery must run from this repository')
    guarded_already = (evidence / 'resource/resource-limit.json').is_file()
    require(not evidence.exists() or guarded_already, 'Recovery evidence directory must be new')
    checked = validate_inputs(repo, source, work, None if guarded_already else RECORDED_WHEEL_SHA256)
    if not guarded_already:
        evidence.mkdir(parents=True)
        (evidence / 'original-producer.json').write_bytes(checked.pop('original_producer_bytes'))
        shutil.copyfile(work / '13-wheel/status', evidence / 'original-wheel-status')
        shutil.copyfile(work / '13-wheel/stderr.txt', evidence / 'original-wheel-stderr.txt')
        run_guard(repo, work, evidence, source)
    checked = validate_inputs(repo, source, work, None)
    receipt = {
        'status': 'PASS_LOCAL_QUALIFICATION', 'version': VERSION,
        'recovery': 'wheel-only', 'original_failure_preserved': True,
        'original_failure_receipt': str(evidence / 'original-producer.json'),
        'resource_receipt': str(evidence / 'resource/resource-limit.json'),
        'producer_revision': checked['producer']['producer_revision'],
        'source_revision': checked['producer']['source_revision'],
        'qualification_source_revision': checked['producer']['qualification_source_revision'],
        'component_source_revisions': checked['producer']['component_source_revisions'],
        'prefix': str(work / 'sdk'),
        'source_identity': checked['source_identity'], 'wheel': checked['wheel'],
        'wheel_sha256': checked['wheel_sha256'], 'sdk_files': checked['sdk_files'],
        'recorded_prior_wheel_sha256': RECORDED_WHEEL_SHA256,
        'reports': checked['reports'], 'report_identities': checked['report_identities'],
        'vectors_sha256': checked['vectors_sha256'], 'scope': checked['scope'],
        'publication': 'NOT_RUN', 'owner_acceptance': 'PENDING',
    }
    (evidence / 'qualification-receipt.json').write_text(json.dumps(receipt, indent=2, sort_keys=True) + '\n')
    producer = checked['producer']
    producer.update({k: receipt[k] for k in ('status', 'wheel', 'wheel_sha256', 'sdk_files', 'reports', 'prefix')})
    producer.update({'recovery_receipt': str(evidence / 'qualification-receipt.json'),
                     'original_failure_receipt': str(evidence / 'original-producer.json'),
                     'recovery': 'wheel-only', 'diagnostic': None})
    (work / 'producer.json').write_text(json.dumps(producer, indent=2, sort_keys=True) + '\n')
    print(json.dumps({'status': receipt['status'], 'wheel_sha256': receipt['wheel_sha256'],
                      'receipt': str(evidence / 'qualification-receipt.json')}, sort_keys=True))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--repo', type=Path, required=True)
    parser.add_argument('--qualification-source', type=Path, required=True)
    parser.add_argument('--work', type=Path, required=True)
    parser.add_argument('--evidence', type=Path, required=True)
    recover(parser.parse_args())


if __name__ == '__main__':
    main()
