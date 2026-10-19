#!/usr/bin/env python3
"""Partition all catalog roots and link their small public-API fixtures serially."""
import importlib.util
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

REPO = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO/'python/ngap_sdk'))
from root_catalog import ROOTS, cpp_type
spec = importlib.util.spec_from_file_location('seal_sdk', REPO/'tools/ngap-dispatch/seal_sdk.py')
seal = importlib.util.module_from_spec(spec); spec.loader.exec_module(seal)


class SplitCheckTests(unittest.TestCase):
    def test_all_26_are_separate_and_called(self):
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            roots = {}
            for stem, name in ROOTS.items():
                ns = 'nrforge::ngap::transfers::'+stem
                header = directory/'transfers'/stem/'root.hpp'
                header.parent.mkdir(parents=True)
                header.write_text('#include <runtime.hpp>\nnamespace '+ns+' {\nstruct '+cpp_type(name)+' {};\ninline bool encode_probe(const '+cpp_type(name)+'&, const ::nrforge::aper::Limits&) {return true;}\n}\n')
                roots[stem] = dict(namespace=ns, cpp_type=ns+'::'+cpp_type(name),
                                   encode='encode_probe', headers=[str(header.relative_to(directory))])
            declarations, calls = seal.write_transfer_link_checks(directory, dict(roots=roots))
            paths = sorted((directory/'sdk-transfer-checks').glob('*.cpp'))
            self.assertEqual(len(paths), 26)
            cmake = (directory/'sdk-transfer-checks.cmake').read_text()
            for stem, row in roots.items():
                text = (directory/'sdk-transfer-checks'/(stem+'.cpp')).read_text()
                self.assertEqual(text.count('#include'), 1)
                self.assertIn('#include <'+row['headers'][0]+'>', text)
                self.assertIn('::'+row['namespace']+'::encode_probe(', text)
                self.assertIn('nrforge_sdk_transfer_check_'+stem+'();', declarations)
                self.assertIn('nrforge_sdk_transfer_check_'+stem+'();', calls)
                self.assertIn('/sdk-transfer-checks/'+stem+'.cpp', cmake)
            (directory/'main.cpp').write_text(declarations+'int main(){\n'+calls+'}\n')
            (directory/'CMakeLists.txt').write_text('cmake_minimum_required(VERSION 3.20)\nproject(CheckPartition LANGUAGES CXX)\ninclude(sdk-transfer-checks.cmake)\nadd_executable(check main.cpp ${NRFORGE_TRANSFER_CHECK_SOURCES})\ntarget_compile_features(check PRIVATE cxx_std_20)\ntarget_include_directories(check PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}" "'+str(REPO/'libaper')+'")\n')
            for argv in (['cmake', '-S', directory, '-B', directory/'build'],
                         ['cmake', '--build', directory/'build', '--parallel', '1'],
                         [directory/'build/check']):
                run = subprocess.run([str(x) for x in argv], capture_output=True, text=True)
                self.assertEqual(run.returncode, 0, run.stdout+run.stderr)

    def test_other_protocols_have_no_transfer_sources(self):
        with tempfile.TemporaryDirectory() as directory:
            self.assertEqual(seal.write_transfer_link_checks(Path(directory), None), ('', ''))
            self.assertEqual(list(Path(directory).iterdir()), [])


if __name__ == '__main__':
    unittest.main()
