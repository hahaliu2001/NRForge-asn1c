"""R1-R3 recovery tests.

The recovery tests call recover() for workflow failures; the mocked guard
creates the wheel during the simulated build, so a pre-existing wheel cannot
make a test pass.
"""
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
        self.evidence = self.root / 'evidence'

    def tearDown(self): self.tmp.cleanup()

    def guard_files(self, evidence, code=0, complete=True):
        resource = evidence; resource.mkdir(parents=True, exist_ok=True)
        (resource / 'guard-status.json').write_text(json.dumps({'status': 'EXIT', 'exit_code': code}))
        limit = {'memory_max': 10 * 1024**3, 'tasks_max': 128, 'swap_max': 0,
                 'memory_peak': 123456, 'memory_events': {'low': 0, 'high': 0, 'max': 0, 'oom': 0, 'oom_kill': 0}}
        if complete: limit.update(cpu_quota='150%', cpu_period=100000)
        (resource / 'resource-limit.json').write_text(json.dumps(limit))
        for name in ('scope.stdout', 'scope.stderr'): (resource / name).write_text('')

    def test_guard_rejects_nonzero_and_incomplete_contract(self):
        self.evidence.mkdir(); self.guard_files(self.evidence, code=1)
        with self.assertRaisesRegex(ValueError, 'did not finish'): recover.check_guard(self.evidence)
        self.guard_files(self.evidence, complete=False)
        with self.assertRaisesRegex(ValueError, 'CPU contract'): recover.check_guard(self.evidence)

    def test_guard_zero_without_new_wheel_fails(self):
        output = self.root / 'output'; output.mkdir()
        with self.assertRaisesRegex(ValueError, 'exactly the expected new wheel'):
            recover.exact_new_wheel(output)

    def test_guard_rejects_extra_output(self):
        output = self.root / 'output'; output.mkdir()
        (output / 'unexpected.whl').write_bytes(b'x')
        with self.assertRaisesRegex(ValueError, 'exactly the expected new wheel'):
            recover.exact_new_wheel(output)

    def test_fixed_argv_has_no_historical_command(self):
        argv = recover.fixed_build_argv(Path('/repo'), Path('/work'), Path('/new-wheel'), Path('/new-ext'))
        self.assertNotIn('command.json', ' '.join(map(str, argv)))
        self.assertIn('CMAKE_BUILD_PARALLEL_LEVEL=1', argv)
        self.assertIn('--no-isolation', argv); self.assertIn('--wheel', argv)
        self.assertIn('--outdir', argv); self.assertIn('/new-wheel', argv)

    def test_source_manifest_rejects_unregistered_difference(self):
        repo, source = self.root / 'repo', self.root / 'source'; repo.mkdir(); source.mkdir()
        (repo / 'python').mkdir(); (source / 'python').mkdir()
        (repo / 'python/a.py').write_text('new'); (source / 'python/a.py').write_text('old')
        with patch.object(recover, '_tracked', side_effect=[['python/a.py'], ['python/a.py'], ['python/a.py']]):
            with self.assertRaisesRegex(ValueError, 'unregistered input'):
                recover.source_manifest(repo, source, 'b' * 40)

    def test_source_manifest_reports_all_unregistered_differences(self):
        repo, source = self.root / 'repo', self.root / 'source'; repo.mkdir(); source.mkdir()
        for directory in ('python', 'tools'):
            (repo / directory).mkdir(); (source / directory).mkdir()
        (repo / 'python/a.py').write_text('new-a'); (source / 'python/a.py').write_text('old-a')
        (repo / 'tools/b.py').write_text('new-b'); (source / 'tools/b.py').write_text('old-b')
        with patch.object(recover, 'QUALIFICATION_FILES', ()), patch.object(
                recover, '_tracked', side_effect=[['python/a.py', 'tools/b.py'], None]):
            with self.assertRaisesRegex(ValueError, r'a\.py[\s\S]*b\.py') as failure:
                recover.source_manifest(repo, source, 'b' * 40)
        self.assertIn('source_sha256=', str(failure.exception))

    def test_snapshot_without_git_suppresses_expected_stderr(self):
        with patch.object(recover.subprocess, 'run', return_value=type(
                'R', (), {'returncode': 128, 'stdout': '',
                          'stderr': "fatal: not a git repository (or any of the parent directories): .git\n"})()), \
                patch.object(recover.Path, 'exists', return_value=False):
            self.assertIsNone(recover._tracked(self.root / 'snapshot'))

    def test_tracked_git_error_is_reported(self):
        with patch.object(recover.subprocess, 'run', return_value=type(
                'R', (), {'returncode': 128, 'stdout': '', 'stderr': 'fatal: bad object HEAD\n'})()), \
                patch.object(recover.Path, 'exists', return_value=True):
            with self.assertRaisesRegex(ValueError, 'Git input inventory failed.*bad object HEAD'):
                recover._tracked(self.root / 'repo')

    def test_source_manifest_rejects_extra_source_input(self):
        repo, source = self.root / 'repo', self.root / 'source'; repo.mkdir(); source.mkdir()
        (repo / 'python').mkdir(); (source / 'python').mkdir()
        (repo / 'python/a.py').write_text('same'); (source / 'python/a.py').write_text('same')
        (source / 'python/extra.py').write_text('extra')
        (repo / 'python/extra.py').write_text('extra')
        with patch.object(recover, 'QUALIFICATION_FILES', ()), patch.object(recover, '_tracked', side_effect=[['python/a.py'], ['python/a.py', 'python/extra.py']]):
            with self.assertRaisesRegex(ValueError, 'unregistered build inputs'):
                recover.source_manifest(repo, source, 'b' * 40)

    def test_reports_check_exact_reason(self):
        work = self.root / 'work'; work.mkdir(); (work / 'vectors.json').write_text('[]')
        identity = {'source_revision': 'a' * 40}
        (work / 'outer.json').write_text(json.dumps({'status': 'PASS', 'native_message_count': 131,
            'independent_ng_setup_count': 3, 'identity': identity}))
        (work / 'transfer.json').write_text(json.dumps({'status': 'FAIL', 'cases': 143, 'identity': identity,
            'vectors_sha256': recover.sha(work / 'vectors.json')}))
        with self.assertRaisesRegex(ValueError, 'Transfer report'):
            recover.check_reports(work, work / 'outer.json', work / 'transfer.json', {'ngap': 'a' * 40})

    def test_old_candidate_state_rejects_publication(self):
        old = self.root / 'old'; old.mkdir()
        (old / 'candidate.json').write_text(json.dumps({'version': recover.VERSION,
            'qualification': 'PASS_LOCAL_QUALIFICATION', 'assets': {}}))
        with self.assertRaisesRegex(ValueError, 'producer'):
            recover.validate_old_candidate(old, self.root)

    def test_recovery_refuses_reused_evidence_before_side_effects(self):
        self.evidence.mkdir()
        args = type('A', (), {'repo': HERE.parents[1], 'qualification_source': self.root,
            'work': self.root, 'old_candidate': self.root, 'evidence': self.evidence})()
        with self.assertRaisesRegex(ValueError, 'reuse is forbidden'):
            recover.recover(args)

    def _validation(self, work, wheel):
        return {'producer': {'source_revision': 'old', 'producer_revision': 'old',
            'qualification_source_revision': 'base', 'component_source_revisions': {'ngap': 'n'},
            'wheel': str(wheel), 'wheel_sha256': 'old-wheel', 'status': 'PASS_LOCAL_QUALIFICATION'},
            'original_bytes': b'{}', 'bundle': {'source_revision': 'old'}, 'sdk_files': {'x': 'y'},
            'source_inputs': {'inputs': {}, 'registered_patches': {}}, 'tool_hashes': {},
            'old_wheel_sha256': 'old-wheel'}

    def _guard_success_creates_wheel(self, argv, cwd):
        output = Path(argv[argv.index('--') + 1])  # python -m build ... --outdir OUTPUT
        output = Path(argv[argv.index('--outdir') + 1])
        with zipfile.ZipFile(output / recover.WHEEL_NAME, 'w') as archive:
            archive.writestr('nrforge_ngap/__init__.py', '')
        evidence = Path(argv[argv.index('--evidence') + 1]); self.guard_files(evidence)
        return type('R', (), {'returncode': 0})()

    def _guard_success_no_wheel(self, argv, cwd):
        evidence = Path(argv[argv.index('--evidence') + 1]); self.guard_files(evidence)
        return type('R', (), {'returncode': 0})()

    def _fake_command(self, argv, cwd, record, name):
        (record.parent / (name + '.stdout')).write_text('')
        (record.parent / (name + '.stderr')).write_text('')
        data = {'status': 'PASS', 'native_message_count': 131, 'independent_ng_setup_count': 3,
                'identity': {'source_revision': 'n'}}
        if name == 'outer': (record.parent / 'outer-report.json').write_text(json.dumps(data))
        if name == 'transfer':
            data = {'status': 'PASS', 'cases': 143, 'identity': {'source_revision': 'n'},
                    'vectors_sha256': recover.sha(self.root / 'work/vectors.json')}
            (record.parent / 'transfer-report.json').write_text(json.dumps(data))
        record.write_text(json.dumps({'commands': []})); return 0

    def _run_recovery(self, guard):
        work = self.root / 'work'; work.mkdir(); old = self.root / 'old'; old.mkdir()
        (work / 'vectors.json').write_text('[]'); (work / '13-wheel').mkdir()
        (work / '13-wheel/status').write_text('EXIT 1'); (work / '13-wheel/stderr.txt').write_text('old')
        wheel = work / recover.WHEEL_NAME; wheel.write_bytes(b'historical')
        data = self._validation(work, wheel)
        args = type('A', (), {'repo': HERE.parents[1], 'qualification_source': work,
            'work': work, 'old_candidate': old, 'evidence': work / 'new-evidence'})()
        with patch.object(recover, 'validate_inputs', return_value=data), \
             patch.object(recover.subprocess, 'run', side_effect=guard), \
             patch.object(recover, 'run_command', side_effect=self._fake_command), \
             patch.object(recover.subprocess, 'check_output', return_value='c' * 40 + '\n'):
            return recover.recover(args)

    def test_mock_success_creates_new_wheel_and_enters_recover(self):
        self._run_recovery(self._guard_success_creates_wheel)
        receipt = json.loads((self.root / 'work/new-evidence/qualification-receipt.json').read_text())
        self.assertEqual(receipt['status'], 'PASS_LOCAL_QUALIFICATION')
        self.assertTrue((self.root / 'work/new-evidence/wheel-output' / recover.WHEEL_NAME).is_file())
        self.assertIn('build_command', receipt)

    def test_guard_success_without_wheel_never_writes_pass(self):
        with self.assertRaisesRegex(ValueError, 'exactly the expected new wheel'):
            self._run_recovery(self._guard_success_no_wheel)
        self.assertFalse((self.root / 'work/new-evidence/qualification-receipt.json').exists())

    def test_guard_failure_never_writes_pass(self):
        def failed(argv, cwd):
            evidence = Path(argv[argv.index('--evidence') + 1]); self.guard_files(evidence, code=1)
            return type('R', (), {'returncode': 1})()
        with self.assertRaisesRegex(ValueError, 'did not finish'):
            self._run_recovery(failed)
        self.assertFalse((self.root / 'work/new-evidence/qualification-receipt.json').exists())

    def test_run_command_closes_logs(self):
        record = self.root / 'commands.json'; record.write_text('{}\n')
        self.assertEqual(recover.run_command(['python3', '-c', 'print(1)'], self.root, record, 'closed'), 0)
        self.assertTrue((self.root / 'closed.stdout').read_text().strip() == '1')

    def test_historical_scope_is_finite(self):
        work = self.root / 'work'; work.mkdir()
        for name, count in (('f1ap-outer.json', 158), ('e1ap-outer.json', 72)):
            (work / name).write_text(json.dumps({'message_count': count, 'cases': [{'status': 'PASS'}]}))
        (work / 'e1ap-containing-audit.json').write_text(json.dumps({'full_protocol_coverage': False}))
        coexist = work / 'coexist-test'; coexist.mkdir(); (coexist / 'ctest.log').write_text('100% tests passed 0 tests failed')
        recover.check_historical_scope(work)

    def test_wheel_digest_gate(self):
        wheel = self.root / recover.WHEEL_NAME
        with zipfile.ZipFile(wheel, 'w') as archive: archive.writestr('nrforge_ngap/__init__.py', '')
        with self.assertRaisesRegex(ValueError, 'Wheel bytes'):
            recover.check_wheel(wheel, '0' * 64)

if __name__ == '__main__': unittest.main()
