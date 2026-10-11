"""E1AP protocol seals and fail-closed installed declaration parsing."""
import importlib.util
import json
import os
from pathlib import Path
import shutil
import tempfile
import unittest
DEFAULT = next((p/'ngap_sdk/generate_bindings.py' for p in Path(__file__).resolve().parents if (p/'ngap_sdk/generate_bindings.py').is_file()), None)
PATH=Path(os.environ.get('NRFORGE_BINDING_GENERATOR',DEFAULT))
spec=importlib.util.spec_from_file_location('generator',PATH)
g=importlib.util.module_from_spec(spec);spec.loader.exec_module(g)

class SealingTests(unittest.TestCase):
    def test_both_seals_and_tampering(self):
        for profile,var,count in (('e1ap','NRFORGE_TEST_E1AP_PREFIX',150),):
            source=Path(os.environ[var])
            with tempfile.TemporaryDirectory() as tmp:
                p=Path(tmp);shutil.copytree(source/'include',p/'include')
                shutil.copytree(source/'share'/f'nrforge-{profile}',p/'share'/f'nrforge-{profile}')
                fp=json.loads((p/'share'/f'nrforge-{profile}'/'sdk-provenance.json').read_text())['fingerprint']
                _,receipt=g.verify_installed_sdk(p,fp,profile)
                self.assertEqual(len(receipt['public_header_sha256']),count)
                for name in ('runtime.hpp','sdk_version.hpp','pdu_declarations.inc',
                    f'messages/{"GnbCuUpE1"}SetupRequest_types.hpp'):
                    path=p/'include/nrforge'/profile/name;original=path.read_bytes()
                    path.write_bytes(original+b'\n// tamper\n')
                    with self.subTest(profile=profile,header=name),self.assertRaises(ValueError):g.verify_installed_sdk(p,fp,profile)
                    path.write_bytes(original)
                # Reversing a namespaced include must not erase a tamper:
                # installed source hash reconstruction also checks its exact
                # forward canonical rewrite, preserving coinstall isolation.
                path=p/'include/nrforge'/profile/f'messages/{"GnbCuUpE1"}SetupRequest_types.hpp'
                original=path.read_bytes()
                qualified=f'#include <nrforge/{profile}/sequence_extensions.hpp>'.encode()
                self.assertIn(qualified,original)
                path.write_bytes(original.replace(qualified,b'#include <sequence_extensions.hpp>'))
                with self.assertRaises(ValueError):g.verify_installed_sdk(p,fp,profile)
                path.write_bytes(original)
                extra=p/'include/nrforge'/profile/'extra.hpp';extra.write_text('// extra\n')
                with self.assertRaises(ValueError):g.verify_installed_sdk(p,fp,profile)
                extra.unlink()
                with self.assertRaises(ValueError):g.verify_installed_sdk(p,'0'*64,profile)
                with self.assertRaises((ValueError,FileNotFoundError)):g.verify_installed_sdk(p,fp,'ngap' if profile=='f1ap' else 'f1ap')
    def test_unsupported_shapes(self):
        for profile in ('e1ap','f1ap','ngap'):
            for source in ('using A = double;','struct A { bool x{}; bool x{}; };','struct A { void method(); };',
                'union A { bool x; };','using A = ::std::optional<bool,bool>;','#define A bool\n'):
                with self.assertRaises(ValueError):g.parse_header(source,'nrforge::'+profile+'::messages::Test')

if __name__=='__main__':unittest.main()
