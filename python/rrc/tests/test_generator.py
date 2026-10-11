"""Actual installed public seal, generated closure and closed model grammar."""

import importlib.util
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

SOURCE = Path(__file__).resolve().parents[1]
GENERATOR = SOURCE / "bindings/generate_bindings.py"
spec = importlib.util.spec_from_file_location("rrc_generator", GENERATOR)
g = importlib.util.module_from_spec(spec)
spec.loader.exec_module(g)


class GeneratorTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.original = Path(os.environ["NRFORGE_RRC_TEST_SDK"]).resolve()
        cls.tmp = tempfile.TemporaryDirectory(prefix="rrc-python-seal-")
        cls.work = Path(cls.tmp.name)
        cls.prefix = cls.work / "sdk"

        def copy(src, dst):
            if Path(src).suffix == ".a":
                os.link(src, dst)
                return dst
            return shutil.copy2(src, dst)

        shutil.copytree(cls.original, cls.prefix, copy_function=copy)
        cls.producer = cls.work / "producer"
        cls.producer.mkdir()
        shutil.copytree(
            SOURCE / "bindings",
            cls.producer / "bindings",
            ignore=shutil.ignore_patterns("__pycache__"),
        )
        shutil.copyfile(SOURCE / "CMakeLists.txt", cls.producer / "CMakeLists.txt")
        cls.script = cls.producer / "bindings/generate_bindings.py"
        cls.generated = cls.work / "generated"
        cls.command("--output", str(cls.generated))

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    @classmethod
    def command(cls, *args, fail=False):
        r = subprocess.run(
            [sys.executable, str(cls.script), "--sdk-prefix", str(cls.prefix), *args],
            capture_output=True,
            text=True,
        )
        if (r.returncode == 0) == fail:
            raise AssertionError(r.stdout + r.stderr)
        return r

    def test_pristine_and_repeated_generation(self):
        self.command("--output", str(self.generated), "--verify-only")
        repeated = self.work / "repeat"
        self.command("--output", str(repeated))
        first = {p.name: g.sha(p) for p in self.generated.iterdir()}
        second = {p.name: g.sha(p) for p in repeated.iterdir()}
        self.assertEqual(first, second)
        self.assertEqual(len(first), 69)

    def test_actual_sdk_header_missing_extra_and_version_drift(self):
        base = self.prefix / "include/nrforge/rrc"
        header = next((base / "roots").rglob("*.hpp"))
        raw = header.read_bytes()
        try:
            header.write_bytes(raw + b"\n")
            self.assertIn(
                "header hash mismatch", self.command("--verify-only", fail=True).stderr
            )
            header.unlink()
            self.assertIn(
                "inventory mismatch", self.command("--verify-only", fail=True).stderr
            )
        finally:
            header.write_bytes(raw)
        extra = base / "foreign.hpp"
        try:
            extra.write_text("// extra")
            self.assertIn(
                "inventory mismatch", self.command("--verify-only", fail=True).stderr
            )
        finally:
            extra.unlink()
        version = base / "sdk_version.hpp"
        raw = version.read_bytes()
        try:
            version.write_bytes(raw + b"\n")
            self.assertIn(
                "version header mismatch",
                self.command("--verify-only", fail=True).stderr,
            )
        finally:
            version.write_bytes(raw)
        self.command("--verify-only")

    def test_metadata_duplicate_manifest_and_fingerprint_drift(self):
        metadata = self.prefix / "share/nrforge-rrc"
        manifest = metadata / "manifest.json"
        raw = manifest.read_bytes()
        try:
            manifest.write_bytes(raw + b"\n")
            self.assertIn(
                "manifest hash mismatch",
                self.command("--verify-only", fail=True).stderr,
            )
        finally:
            manifest.write_bytes(raw)
        provenance = metadata / "sdk-provenance.json"
        raw = provenance.read_text()
        try:
            provenance.write_text(raw.replace("{", '{"identity":{},', 1))
            self.assertIn(
                "duplicate JSON key", self.command("--verify-only", fail=True).stderr
            )
        finally:
            provenance.write_text(raw)
        self.assertIn(
            "fingerprint differs",
            self.command(
                "--expected-fingerprint", "0" * 64, "--verify-only", fail=True
            ).stderr,
        )

    def test_generated_cpp_schema_missing_extra_and_producer_drift(self):
        for name in ("root021.cpp", "conversion-schema.json", "sources.cmake"):
            file = self.generated / name
            raw = file.read_bytes()
            try:
                file.write_bytes(raw + b"\n")
                self.assertIn(
                    "binding closure changed",
                    self.command(
                        "--output", str(self.generated), "--verify-only", fail=True
                    ).stderr,
                )
                file.unlink()
                self.assertIn(
                    "binding closure changed",
                    self.command(
                        "--output", str(self.generated), "--verify-only", fail=True
                    ).stderr,
                )
            finally:
                file.write_bytes(raw)
        extra = self.generated / "foreign.cpp"
        try:
            extra.write_text("// extra")
            self.assertIn(
                "binding closure changed",
                self.command(
                    "--output", str(self.generated), "--verify-only", fail=True
                ).stderr,
            )
        finally:
            extra.unlink()
        source = self.producer / "bindings/module.cpp"
        raw = source.read_bytes()
        try:
            source.write_bytes(raw + b"\n")
            self.assertIn(
                "producer/SDK seal changed",
                self.command(
                    "--output", str(self.generated), "--verify-only", fail=True
                ).stderr,
            )
        finally:
            source.write_bytes(raw)
        self.command("--output", str(self.generated), "--verify-only")

    def test_closed_public_model_shapes(self):
        prefix = self.original / "include/nrforge/rrc"
        manifest = g.read_json(self.original / "share/nrforge-rrc/manifest.json")
        models = []
        for r in manifest["roots"]:
            if r["status"] == "PASS":
                text = (self.original / "include" / r["header"]).read_text()
                models.append(g.models(text))
                with self.assertRaises(ValueError):
                    g.models(
                        text.replace("struct T0 {", "struct T0 {\nfloat foreign;", 1)
                    )
        self.assertEqual(len(models), 64)
        header = next(prefix.rglob("MIB.hpp")).read_text()
        with self.assertRaises(ValueError):
            g.models(header.replace("using Root=T0;", "using Root=T1;"))


if __name__ == "__main__":
    unittest.main()
