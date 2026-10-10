"""Build-time sealing tests. Set NRFORGE_TEST_SDK_PREFIX to an installed SDK."""
import importlib.util
import json
import os
from pathlib import Path
import shutil
import tempfile
import unittest

SDK = os.environ.get('NRFORGE_TEST_SDK_PREFIX')
PATH = Path(__file__).resolve().parents[1] / 'ngap_sdk/generate_bindings.py'
spec = importlib.util.spec_from_file_location('nrforge_binding_generator', PATH)
generator = importlib.util.module_from_spec(spec)
spec.loader.exec_module(generator)

@unittest.skipUnless(SDK, 'build-time test requires NRFORGE_TEST_SDK_PREFIX')
class SealingTests(unittest.TestCase):
    def test_public_header_seal(self):
        source = Path(SDK)
        with tempfile.TemporaryDirectory() as tmp:
            prefix = Path(tmp)
            shutil.copytree(source/'include', prefix/'include')
            shutil.copytree(source/'share/nrforge-ngap', prefix/'share/nrforge-ngap')
            fp=json.loads((prefix/'share/nrforge-ngap/sdk-provenance.json').read_text())['fingerprint']
            _, receipt = generator.verify_installed_sdk(prefix,fp)
            self.assertEqual(receipt['fingerprint'],fp)
            for name in ['messages/NgSetupRequest_types.hpp','runtime.hpp','sdk_version.hpp']:
                path=prefix/'include/nrforge/ngap'/name
                original=path.read_bytes()
                path.write_bytes(original+b'\n// changed after install\n')
                with self.subTest(name=name), self.assertRaises(ValueError):
                    generator.verify_installed_sdk(prefix,fp)
                path.write_bytes(original)
            extra=prefix/'include/nrforge/ngap/extra.hpp'
            extra.write_text('// unexpected header\n')
            with self.assertRaises(ValueError):generator.verify_installed_sdk(prefix,fp)
            extra.unlink()
            with self.assertRaises(ValueError):generator.verify_installed_sdk(prefix,'0'*64)

    def test_unknown_shape_fails_closed(self):
        ns='nrforge::ngap::messages::Test'
        for source in ['using A = double;', 'struct A { bool x{}; bool x{}; };',
                       'struct A { void method(); };', 'union A { bool x; };',
                       'using A = ::std::optional<bool,bool>;', '#define A bool\n']:
            with self.subTest(source=source), self.assertRaises(ValueError):
                generator.parse_header(source,ns)

if __name__=='__main__':unittest.main()
