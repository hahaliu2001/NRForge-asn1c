#!/usr/bin/env python3
"""Recovery must refuse unrelated revisions and locally changed source inputs."""
from pathlib import Path
import tempfile
import json
import unittest
from unittest.mock import patch
from resume_ngap import source_delta, recover, SEALED


class ResumeChecks(unittest.TestCase):
    def delta(self, names, existing=b'old', baseline=b'old'):
        def git(argv):
            if 'diff' in argv: return ('\n'.join(names)+'\n').encode()
            if 'ls-tree' in argv: return b'tools/ngap-transfer-sdk/prepare.py\n'
            return baseline if argv[-1].startswith('old:') else b'new'
        with tempfile.TemporaryDirectory() as root:
            source = Path(root)
            path = source/'tools/ngap-transfer-sdk/prepare.py'
            path.parent.mkdir(parents=True); path.write_bytes(existing)
            with patch('resume_ngap.subprocess.check_output', side_effect=git):
                return source_delta(source, source, 'old', 'new')

    def test_expected_patch(self):
        changes = self.delta(['tools/ngap-transfer-sdk/prepare.py'])
        self.assertEqual(changes['tools/ngap-transfer-sdk/prepare.py'], (b'old', b'new'))

    def test_modified_snapshot_refused(self):
        with self.assertRaisesRegex(ValueError, 'preimage'):
            self.delta(['tools/ngap-transfer-sdk/prepare.py'], existing=b'local edit')

    def test_unrelated_production_change_refused(self):
        with self.assertRaisesRegex(ValueError, 'reviewed'):
            self.delta(['libaper/runtime.cpp'])

    def test_recovery_preserves_failure_and_objects_before_reseal(self):
        with tempfile.TemporaryDirectory() as root:
            out = Path(root)
            source = out/'source'; source.mkdir()
            build = out/'ngap-build/build'; build.mkdir(parents=True)
            (build/'CMakeCache.txt').write_text('configured')
            obj = build/'adapter.o'; obj.write_bytes(b'keep object')
            generated = out/'ngap-build/generated'; generated.mkdir()
            for name in SEALED:
                if name == 'sdk-public':
                    (generated/name).mkdir(); (generated/name/'root.hpp').write_text('old public')
                else: (generated/name).write_text('old '+name)
            roots = {}
            old_header = '#pragma once\n#include "root_types.hpp"\n#include "root_mapping.hpp"\n#include "root_codec.hpp"\n'
            for index in range(26):
                header = generated/f'transfers/root{index}/root.hpp'
                header.parent.mkdir(parents=True); header.write_text(old_header)
                roots[str(index)] = dict(headers=[str(header.relative_to(generated))])
            (generated/'transfer-manifest.json').write_text(json.dumps(dict(roots=roots)))
            stage = out/'05-ngap'; stage.mkdir(); (stage/'status').write_text('EXIT 1\n')
            (out/'ngap-build/build.log').write_text('original compiler failure')
            previous = dict(status='FAIL_OR_INTERRUPTED', source_revision='old')
            (out/'producer.json').write_text(json.dumps(previous))
            patch_path = source/'tools/ngap-transfer-sdk/prepare.py'
            patch_path.parent.mkdir(parents=True); patch_path.write_bytes(b'old')
            calls = []
            def command(directory, argv, **kwargs):
                calls.append(directory.name)
                if len(calls) == 1:
                    self.assertEqual(patch_path.read_bytes(), b'old')
                    self.assertTrue((generated/'sdk-lock.cmake').exists())
                if directory.name == '02-reseal':
                    self.assertEqual(patch_path.read_bytes(), b'def root_header(stem): return \"fixed\"\n')
                    self.assertEqual((generated/'transfers/root0/root.hpp').read_text(), 'fixed')
                    self.assertFalse((generated/'sdk-lock.cmake').exists())
                    self.assertTrue((out/'namespace-recovery/original-seal/sdk-lock.cmake').exists())
                if directory.name == '04-build':
                    self.assertEqual(argv[-2:], ['--parallel', '1'])
            def write(path, value): path.write_text(json.dumps(value))
            with patch('resume_ngap.source_delta', return_value={
                    'tools/ngap-transfer-sdk/prepare.py': (b'old', b'def root_header(stem): return "fixed"\n')}):
                result = recover(source, out, 'new', command, write)
            self.assertEqual(calls, ['01-original-lock', '02-reseal', '03-configure', '04-build', '05-install'])
            self.assertEqual(obj.read_bytes(), b'keep object')
            self.assertEqual((stage/'status').read_text(), 'EXIT 1\n')
            self.assertEqual(json.loads((out/'namespace-recovery/original-producer.json').read_text()), previous)
            self.assertEqual(result['baseline'], 'old')

    def test_same_revision_refused(self):
        with self.assertRaisesRegex(ValueError, 'reviewed'):
            self.delta([])


if __name__ == '__main__':
    unittest.main()
