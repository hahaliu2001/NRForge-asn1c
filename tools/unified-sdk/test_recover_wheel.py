"""Targeted B1-B5 gates; all fixtures are temporary and no native build runs."""
import importlib.util
import json
import tempfile
import unittest
import zipfile
from pathlib import Path
from unittest.mock import patch

HERE = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location('recover_wheel', HERE / 'recover_wheel.py')
recover = importlib.util.module_from_spec(spec); spec.loader.exec_module(recover)


class RecoveryGates(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory(); self.root = Path(self.tmp.name)
        self.evidence = self.root / 'evidence'; self.evidence.mkdir()

    def tearDown(self): self.tmp.cleanup()

    def write_guard(self, exit_code=0, cpu=True):
        (self.evidence / 'resource').mkdir(exist_ok=True)
        (self.evidence / 'resource/guard-status.json').write_text(json.dumps({'status': 'EXIT', 'exit_code': exit_code}))
        limit = {'memory_max': 6 * 1024**3, 'tasks_max': 128, 'swap_max': 0}
        if cpu: limit.update(cpu_quota='150%', cpu_period=100000)
        (self.evidence / 'resource/resource-limit.json').write_text(json.dumps(limit))

    def test_guard_requires_final_success_and_complete_contract(self):
        self.write_guard(exit_code=1)
        with self.assertRaisesRegex(ValueError, 'did not finish'):
            recover.check_guard(self.evidence)
        self.write_guard(cpu=False)
        with self.assertRaisesRegex(ValueError, 'CPU contract'):
            recover.check_guard(self.evidence)

    def test_report_status_counts_and_identity_are_independently_checked(self):
        work = self.root / 'work'; work.mkdir(); (work / 'vectors.json').write_text('[]')
        outer, transfer = work / 'outer.json', work / 'transfer.json'; identity = {'source_revision': 'a' * 40}
        outer.write_text(json.dumps({'status': 'PASS', 'native_message_count': 131, 'independent_ng_setup_count': 3, 'identity': identity}))
        transfer.write_text(json.dumps({'status': 'PASS', 'cases': 143, 'identity': identity, 'vectors_sha256': recover.sha(work / 'vectors.json')}))
        recover.check_reports(work, outer, transfer, {'ngap': 'a' * 40})
        transfer.write_text(json.dumps({'status': 'FAIL', 'cases': 143, 'identity': identity, 'vectors_sha256': recover.sha(work / 'vectors.json')}))
        with self.assertRaisesRegex(ValueError, 'Transfer report'):
            recover.check_reports(work, outer, transfer, {'ngap': 'a' * 40})

    def test_historical_scope_and_coexistence_are_checked(self):
        work = self.root / 'work'; work.mkdir(); cases = [{'status': 'PASS'}]
        (work / 'f1ap-outer.json').write_text(json.dumps({'message_count': 158, 'cases': cases}))
        (work / 'e1ap-outer.json').write_text(json.dumps({'message_count': 72, 'cases': cases}))
        (work / 'e1ap-containing-audit.json').write_text(json.dumps({'full_protocol_coverage': False}))
        c = work / 'coexist-retry-20261020T144408Z'; c.mkdir(); (c / 'ctest.log').write_text('100% tests passed, 0 tests failed')
        recover.check_historical_scope(work)
        (work / 'e1ap-containing-audit.json').write_text(json.dumps({'full_protocol_coverage': True}))
        with self.assertRaisesRegex(ValueError, 'full coverage'):
            recover.check_historical_scope(work)

    def test_wheel_zip_and_digest_gate(self):
        wheel = self.root / recover.WHEEL_NAME
        with zipfile.ZipFile(wheel, 'w') as archive: archive.writestr('nrforge_ngap/__init__.py', '')
        with self.assertRaisesRegex(ValueError, 'Wheel bytes'):
            recover.check_wheel(wheel, '0' * 64)

    def test_evidence_reuse_is_always_refused(self):
        with self.assertRaisesRegex(ValueError, 'reuse is forbidden'):
            recover.require(self.evidence.exists() is False, 'Recovery evidence directory reuse is forbidden')

    def test_source_manifest_rejects_unregistered_build_input_patch(self):
        repo, source = self.root / 'repo', self.root / 'source'; repo.mkdir(); source.mkdir()
        (repo / 'python').mkdir(); (source / 'python').mkdir()
        (repo / 'python/ok.py').write_text('same'); (source / 'python/ok.py').write_text('same')
        (repo / 'python/tampered.py').write_text('new'); (source / 'python/tampered.py').write_text('old')
        with patch.object(recover.subprocess, 'check_output', return_value='python/ok.py\npython/tampered.py\n'):
            with self.assertRaisesRegex(ValueError, 'Unexpected qualification source patch'):
                recover.source_manifest(repo, source, 'b' * 40)

    def test_mocked_guard_failure_stops_before_pass_receipt(self):
        work = self.root / 'work'; work.mkdir(); old = self.root / 'old'; old.mkdir(); evidence = work / 'failed-evidence'
        (work / '13-wheel').mkdir(); (work / '13-wheel/command.json').write_text(json.dumps({'argv': ['python', '-m', 'build']})); (work / '13-wheel/status').write_text('EXIT 1'); (work / '13-wheel/stderr.txt').write_text('old')
        data = {'producer': {'wheel': str(work / recover.WHEEL_NAME)}, 'original_bytes': b'{}'}
        with patch.object(recover, 'validate_inputs', return_value=data), patch.object(recover.subprocess, 'run', side_effect=self._failed_guard):
            args = type('A', (), {'repo': HERE.parents[1], 'qualification_source': work, 'work': work, 'old_candidate': old, 'evidence': evidence})()
            with self.assertRaisesRegex(ValueError, 'did not finish'):
                recover.recover(args)
        self.assertFalse((evidence / 'qualification-receipt.json').exists())

    def _failed_guard(self, argv, cwd):
        evidence = Path(argv[argv.index('--evidence') + 1]); evidence.mkdir(parents=True, exist_ok=True)
        (evidence / 'guard-status.json').write_text(json.dumps({'status': 'EXIT', 'exit_code': 1}))
        (evidence / 'resource-limit.json').write_text(json.dumps({'memory_max': 6 * 1024**3, 'tasks_max': 128, 'swap_max': 0, 'cpu_quota': '150%', 'cpu_period': 100000}))
        (evidence / 'scope.stdout').write_text(''); (evidence / 'scope.stderr').write_text('OOM')
        return type('R', (), {'returncode': 1})()

    def test_mocked_recovery_success_binds_fresh_checks_and_tools(self):
        work = self.root / 'work'; work.mkdir(); old = self.root / 'old'; old.mkdir(); wheel = work / recover.WHEEL_NAME
        (work / 'vectors.json').write_text('[]')
        with zipfile.ZipFile(wheel, 'w') as archive: archive.writestr('nrforge_ngap/__init__.py', '')
        digest = recover.sha(wheel)
        data = {'producer': {'source_revision': 'old', 'producer_revision': 'old', 'qualification_source_revision': 'base', 'component_source_revisions': {'ngap': 'n'}, 'wheel': str(wheel), 'wheel_sha256': digest, 'status': 'PASS_LOCAL_QUALIFICATION'}, 'original_bytes': b'{}', 'bundle': {'source_revision': 'old'}, 'sdk_files': {'x': 'y'}, 'source_inputs': {'inputs': {}, 'registered_patches': {}}, 'tool_hashes': {'tool': {}}, 'old_wheel_sha256': digest}
        (old / 'candidate.json').write_text(json.dumps({'version': recover.VERSION, 'qualification': 'PASS_LOCAL_QUALIFICATION', 'assets': {'producer.json': 'x'}}))
        with patch.object(recover, 'validate_inputs', return_value=data), patch.object(recover, 'check_guard', return_value={'status': {}, 'limit': {}}), patch.object(recover.subprocess, 'run', side_effect=self._guard_result), patch.object(recover, 'run_command', side_effect=lambda argv, cwd, record, name: self._fresh_result(cwd, name)):
            (work / '13-wheel').mkdir(); (work / '13-wheel/command.json').write_text(json.dumps({'argv': ['python', '-m', 'build']})); (work / '13-wheel/status').write_text('EXIT 1'); (work / '13-wheel/stderr.txt').write_text('old')
            with patch.object(recover.subprocess, 'check_output', return_value='c' * 40 + '\n'):
                args = type('A', (), {'repo': HERE.parents[1], 'qualification_source': work, 'work': work, 'old_candidate': old, 'evidence': work / 'new-evidence'})()
                recover.recover(args)
        receipt = json.loads((work / 'new-evidence/qualification-receipt.json').read_text())
        self.assertEqual(receipt['status'], 'PASS_LOCAL_QUALIFICATION'); self.assertIn('commands', receipt)

    def _fresh_result(self, cwd, name):
        (cwd / (name + '.stdout')).write_text('')
        (cwd / (name + '.stderr')).write_text('')
        if name == 'outer':
            (cwd / 'outer-report.json').write_text(json.dumps({'status': 'PASS', 'native_message_count': 131, 'independent_ng_setup_count': 3, 'identity': {'source_revision': 'n'}}))
        if name == 'transfer':
            (cwd / 'transfer-report.json').write_text(json.dumps({'status': 'PASS', 'cases': 143, 'identity': {'source_revision': 'n'}, 'vectors_sha256': recover.sha(cwd.parent / 'vectors.json')}))
        return 0

    def _guard_result(self, argv, cwd):
        evidence = Path(argv[argv.index('--evidence') + 1]); evidence.mkdir(parents=True, exist_ok=True)
        (evidence / 'guard-status.json').write_text(json.dumps({'status': 'EXIT', 'exit_code': 0}))
        (evidence / 'resource-limit.json').write_text(json.dumps({'memory_max': 6 * 1024**3, 'tasks_max': 128, 'swap_max': 0, 'cpu_quota': '150%', 'cpu_period': 100000}))
        (evidence / 'scope.stdout').write_text(''); (evidence / 'scope.stderr').write_text('')
        return type('R', (), {'returncode': 0})()


if __name__ == '__main__': unittest.main()
