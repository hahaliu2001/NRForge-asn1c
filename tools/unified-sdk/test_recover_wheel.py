"""Offline gates for the wheel-only recovery entry point."""
import importlib.util
import json
import tempfile
import unittest
from pathlib import Path

HERE = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location('recover_wheel', HERE / 'recover_wheel.py')
recover = importlib.util.module_from_spec(spec)
spec.loader.exec_module(recover)


class RecoveryGates(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.root = Path(self.tmp.name)
        self.repo = self.root / 'repo'; self.source = self.root / 'source'; self.work = self.root / 'work'
        for path in (self.repo, self.source, self.work): path.mkdir()
        (self.repo / 'python/ngap_sdk').mkdir(parents=True)
        (self.repo / 'tools/ngap-transfer-sdk').mkdir(parents=True)
        (self.source / 'python/ngap_sdk').mkdir(parents=True)
        (self.source / 'tools/ngap-transfer-sdk').mkdir(parents=True)
        for relative in recover.FIXED_FILES:
            (self.repo / relative).write_bytes(b'current')
            (self.source / relative).write_bytes(b'current')
        (self.work / 'producer.json').write_text(json.dumps({
            'status': 'FAIL_OR_INTERRUPTED', 'version': recover.VERSION,
            'diagnostic': 'Command failed; see 13-wheel', 'source_revision': 'a' * 40,
            'producer_revision': 'b' * 40, 'qualification_source_revision': 'c' * 40,
            'component_source_revisions': {'ngap': 'd' * 40, 'f1ap': 'e' * 40, 'e1ap': 'f' * 40, 'rrc': 'c' * 40},
            'publication': 'NOT_RUN', 'owner_acceptance': 'PENDING'}))
        (self.work / '13-wheel').mkdir(); (self.work / '13-wheel/status').write_text('EXIT 1\n')
        (self.work / '13-wheel/stderr.txt').write_text('failure'); (self.work / '13-wheel/stdout.txt').write_text('')

    def tearDown(self): self.tmp.cleanup()

    def test_wrong_failure_stage_refused(self):
        (self.work / '13-wheel/status').write_text('EXIT 0\n')
        with self.assertRaisesRegex(ValueError, 'Wrong failed stage'):
            recover.validate_inputs(self.repo, self.source, self.work)

    def test_missing_evidence_refused(self):
        (self.work / '13-wheel/stderr.txt').unlink()
        with self.assertRaisesRegex(ValueError, 'incomplete'):
            recover.validate_inputs(self.repo, self.source, self.work)

    def test_sdk_tamper_refused(self):
        (self.work / 'sdk').mkdir(); (self.work / 'relocated-sdk').mkdir()
        (self.work / 'sdk/file').write_text('a'); (self.work / 'relocated-sdk/file').write_text('b')
        with self.assertRaisesRegex(ValueError, 'inventories differ'):
            recover.validate_inputs(self.repo, self.source, self.work)

    def test_wheel_tamper_refused(self):
        with self.assertRaisesRegex(ValueError, 'SDK prefixes'):
            recover.validate_inputs(self.repo, self.source, self.work)

    def test_wheel_digest_tamper_refused(self):
        wheel = self.root / 'wrong.whl'; wheel.write_bytes(b'not a wheel')
        with self.assertRaisesRegex(ValueError, 'Unexpected wheel identity'):
            recover.check_wheel(wheel, '0' * 64)


if __name__ == '__main__': unittest.main()
