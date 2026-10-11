#!/usr/bin/env python3
"""Audit four installed wheels outside checkout with build inputs unavailable."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import tempfile


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--python", required=True)
    ap.add_argument("--output", type=Path, required=True)
    ap.add_argument("--hide", type=Path, action="append", default=[])
    args = ap.parse_args()
    repo = Path(__file__).resolve().parents[2]
    result = {"fixtures": {}, "hidden_inputs": []}
    moved = []
    with tempfile.TemporaryDirectory(prefix="rrc-installed-") as tmp:
        work = Path(tmp)
        for profile, source, names in (
            ("rrc", repo / "python/rrc/tests", ("test_sdk.py", "golden.json")),
            (
                "e1ap",
                repo / "python/e1ap/tests",
                ("test_sdk.py", "golden.json", "e1-setup-native.json"),
            ),
            (
                "f1ap",
                repo / "python/f1ap/tests",
                ("test_sdk.py", "golden.json", "f1-setup-native.json"),
            ),
            (
                "ngap",
                repo / "python/tests",
                ("test_sdk.py", "golden-131.json", "ng-setup-native.json"),
            ),
        ):
            tests = work / profile / "tests"
            tests.mkdir(parents=True)
            for name in names:
                shutil.copyfile(source / name, tests / name)
                result["fixtures"][profile + "/" + name] = hashlib.sha256(
                    (tests / name).read_bytes()
                ).hexdigest()
            if profile == "ngap":
                (tests.parent / "examples").mkdir()
                shutil.copyfile(
                    repo / "python/examples/ng_setup.py",
                    tests.parent / "examples/ng_setup.py",
                )
        try:
            requested = [repo, *[p.resolve() for p in args.hide]]
            if len(set(requested)) != len(requested):
                raise ValueError("duplicate hidden input")
            if not all(p.is_dir() for p in requested):
                raise ValueError("missing input directory")
            paths = []
            for path in sorted(requested, key=lambda p: len(p.parts)):
                if not any(path.is_relative_to(parent) for parent in paths):
                    paths.append(path)
            for path in paths:
                hidden = path.with_name(path.name + ".rrc-python-hidden")
                if hidden.exists():
                    raise ValueError("occupied hidden path")
                path.rename(hidden)
                moved.append((path, hidden))
            result["hidden_inputs"] = [str(p) for p, _ in moved]
            result["unavailable_inputs"] = [str(p) for p in requested]
            if any(p.exists() for p in requested):
                raise RuntimeError("build input remains available")
            audit = subprocess.run(
                [
                    args.python,
                    "-I",
                    "-c",
                    """import json,sys
import nrforge_e1ap as e,nrforge_f1ap as f,nrforge_ngap as n,nrforge_rrc as r
print(json.dumps({**{k:{'package':p.__file__,'extension':__import__(p.__name__+'._native',fromlist=['_native']).__file__,'identity':p.identity()} for k,p in [('e1ap',e),('f1ap',f),('ngap',n),('rrc',r)]},'pycrate_loaded':any(k.startswith('pycrate') for k in sys.modules)}))""",
                ],
                cwd=work,
                capture_output=True,
                text=True,
                check=True,
            )
            result.update(json.loads(audit.stdout))
            if result["pycrate_loaded"]:
                raise RuntimeError("runtime oracle loaded")
            for profile in ("e1ap", "f1ap", "ngap", "rrc"):
                for key in ("package", "extension"):
                    if (
                        not Path(result[profile][key])
                        .resolve()
                        .is_relative_to(Path(args.python).absolute().parent.parent)
                    ):
                        raise RuntimeError("import outside installed environment")
                run = subprocess.run(
                    [
                        args.python,
                        "-I",
                        "-m",
                        "unittest",
                        "discover",
                        "-s",
                        str(work / profile / "tests"),
                        "-p",
                        "test_sdk.py",
                        "-v",
                    ],
                    cwd=work,
                    capture_output=True,
                    text=True,
                )
                (args.output.parent / (profile + "-installed.log")).write_text(
                    run.stdout + run.stderr
                )
                result[profile].update(
                    status="PASS" if run.returncode == 0 else "FAIL",
                    stdout=run.stdout,
                    stderr=run.stderr,
                )
            expected = {"rrc": 12, "ngap": 12, "f1ap": 9, "e1ap": 10}
            for profile, count in expected.items():
                if f"Ran {count} tests" not in result[profile]["stderr"]:
                    raise RuntimeError("test closure mismatch " + profile)
            for marker in (
                "RRC_REAL_CASES 143 ROOTS 64 COMPLETIONS 3",
                "RRC_DEFAULT_OMISSIONS 19",
                "RRC_MALFORMED_QUOTA_CHECKS 998",
                "RRC_FOUR_PROTOCOL_IMPORT_ORDERS 24",
            ):
                if marker not in result["rrc"]["stdout"]:
                    raise RuntimeError("missing RRC marker " + marker)
            result["test_counts"] = expected
            result["status"] = (
                "PASS"
                if all(
                    result[p]["status"] == "PASS"
                    for p in ("e1ap", "f1ap", "ngap", "rrc")
                )
                else "FAIL"
            )
            result["isolated_python"] = True
            result["outside_repository"] = True
        finally:
            for original, hidden in reversed(moved):
                hidden.rename(original)
    args.output.write_text(json.dumps(result, indent=2) + "\n")
    if result["status"] != "PASS":
        raise RuntimeError(json.dumps(result, indent=2))
    print("PASS isolated installed RRC/E1AP/F1AP/NGAP consumers")


if __name__ == "__main__":
    main()
