#!/usr/bin/env python3
"""Compile standalone entries before any umbrella; catch missing runtime includes."""
import importlib.util
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

REPO = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location('transfer_prepare', REPO/'tools/ngap-transfer-sdk/prepare.py')
prepare = importlib.util.module_from_spec(spec); spec.loader.exec_module(prepare)
from root_catalog import ROOTS


class RootHeaderChecks(unittest.TestCase):
    def test_all_entries_are_self_contained_and_keep_root_api_namespace(self):
        compiler = shutil.which('g++')
        self.assertIsNotNone(compiler, 'g++ is required for this small syntax regression')
        with tempfile.TemporaryDirectory() as root:
            root = Path(root)
            headers, calls = [], []
            for stem in ROOTS:
                directory = root/stem; directory.mkdir()
                ns = 'nrforge::ngap::transfers::'+stem
                (directory/'root.hpp').write_text(prepare.root_header(stem))
                (directory/'root_types.hpp').write_text('namespace '+ns+' { struct Root { ::nrforge::aper::BitString bits; ::nrforge::aper::SequenceExtensionData extensions; }; }\n')
                (directory/'root_mapping.hpp').write_text('namespace '+ns+' { using Limits = ::nrforge::aper::Limits; }\n')
                (directory/'root_codec.hpp').write_text('namespace '+ns+' { inline bool encode_root(const Root&, const Limits&) { return true; } }\n')
                headers.append('#include "'+stem+'/root.hpp"')
                calls.append('(void)::'+ns+'::encode_root({}, {});')
            source = root/'check.cpp'
            source.write_text('\n'.join(headers)+'\nint main() {\n'+'\n'.join(calls)+'\n}\n')
            argv = [compiler, '-std=c++20', '-fsyntax-only', '-I'+str(REPO/'libaper'), str(source)]
            good = subprocess.run(argv, capture_output=True, text=True)
            self.assertEqual(good.returncode, 0, good.stderr)
            # The original entry order must fail even when later entries include runtime.
            first = root/next(iter(ROOTS))/'root.hpp'
            first.write_text('#pragma once\n#include "root_types.hpp"\n#include "root_mapping.hpp"\n#include "root_codec.hpp"\n')
            bad = subprocess.run(argv, capture_output=True, text=True)
            self.assertNotEqual(bad.returncode, 0, 'Original missing-includes defect escaped')
            # Even with runtime included, identical pragma-once forwarding headers
            # can be treated as the same file by GCC; each entry needs its own guard.
            identical = '#pragma once\n#include "runtime.hpp"\n#include "sequence_extensions.hpp"\n#include "root_types.hpp"\n#include "root_mapping.hpp"\n#include "root_codec.hpp"\n'
            for stem in ROOTS: (root/stem/'root.hpp').write_text(identical)
            bad = subprocess.run(argv, capture_output=True, text=True)
            self.assertNotEqual(bad.returncode, 0, 'Identical pragma-once headers escaped')



if __name__ == '__main__':
    unittest.main()
