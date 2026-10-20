#!/usr/bin/env python3
"""Fast checks for root-only conversion generation and resource gate rejection."""
import importlib.util
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parents[1]/'python/ngap_sdk'))
import generate_bindings as generator
from transfer_bindings import generate_transfers

spec = importlib.util.spec_from_file_location('resource_guard', HERE/'resource_guard.py')
guard = importlib.util.module_from_spec(spec)
spec.loader.exec_module(guard)


class DeliveryChecks(unittest.TestCase):
    def test_four_independent_roots(self):
        texts, roots = {}, {}
        for stem, tail in [('setup_request', 'SetupRequestTransfer'), ('setup_response', 'SetupResponseTransfer'),
                           ('release_command', 'ReleaseCommandTransfer'), ('release_response', 'ReleaseResponseTransfer')]:
            namespace = 'nrforge::ngap::transfers::'+stem
            typ = namespace+'::NgapIEsPduSessionResource'+tail
            enc = 'encode_test_'+stem
            roots[stem] = dict(message='PDUSessionResource'+tail, namespace=namespace,
                               cpp_type=typ, encode=enc, decode='decode_test_'+stem)
            texts[f'transfers/{stem}/root_types.hpp'] = f'namespace {namespace} {{\nstruct NgapIEsPduSessionResource{tail} {{ bool present{{}}; }};\n}}\n'
            texts[f'transfers/{stem}/root_codec.hpp'] = f'inline auto {enc}(const {typ}& v, Limits limits);'
        rendered = {'sources.cmake': 'set(NRFORGE_PYTHON_SOURCES\n)\n'}
        metadata = generate_transfers(texts, {'transfer_roots': roots}, rendered, generator.parse_header, generator.render)
        self.assertEqual(len(metadata), 4)
        for stem in roots:
            cpp = rendered['transfer_'+stem+'.cpp']
            self.assertIn('transfer_encode_'+stem, cpp)
            self.assertNotIn('make_ngap_pdu', cpp)
            self.assertNotIn('::Body', cpp)
        self.assertIn('transfer_dispatch.cpp', rendered['sources.cmake'])
        roots['setup_request']['cpp_type'] = 'wrong::Type'
        with self.assertRaisesRegex(ValueError, 'type identity'):
            generate_transfers(texts, {'transfer_roots': roots}, {}, generator.parse_header, generator.render)

    def test_unbounded_resource_scope_refused(self):
        def read(path):
            return {'cgroup': '0::/mock', 'memory.max': 'max', 'pids.max': '128'}[path.name]
        with patch.object(Path, 'read_text', read), self.assertRaisesRegex(RuntimeError, 'memory/tasks'):
            guard.verify()

    def test_bounded_scope(self):
        def read(path):
            return {'cgroup': '0::/mock', 'memory.max': str(guard.MAX_MEMORY), 'pids.max': '128', 'memory.swap.max': '0', 'cpu.max': '150000 100000'}[path.name]
        with patch.object(Path, 'read_text', read):
            self.assertEqual(guard.verify()['memory_max'], guard.MAX_MEMORY)

    def test_owner_memory_limit_is_exactly_ten_gib(self):
        self.assertEqual(guard.MAX_MEMORY, 10737418240)

    def test_accounting_records_peak_and_events(self):
        with tempfile.TemporaryDirectory() as directory:
            group = Path(directory)
            (group/'memory.peak').write_text('987654321\n')
            (group/'memory.events').write_text('low 0\nhigh 1\nmax 2\noom 0\noom_kill 0\n')
            identity = {'_cgroup_path': str(group)}
            self.assertEqual(guard.record_accounting(identity)['memory_peak'], 987654321)
            self.assertEqual(identity['memory_events']['oom_kill'], 0)
            self.assertEqual(identity['memory_events_raw'].splitlines()[1], 'high 1')


if __name__ == '__main__':
    unittest.main()
