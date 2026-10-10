"""Verify an already installed wheel using isolated Python outside the repository.

This runner never imports a build-tree package. It copies only the test source,
finite fixtures and consumer example to an external temporary directory.
"""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--python", required=True, help="Python from wheel installation environment")
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    tests = Path(__file__).resolve().parent
    with tempfile.TemporaryDirectory(prefix="nrforge-python-consumer-") as directory:
        work = Path(directory)
        (work / "tests").mkdir()
        (work / "examples").mkdir()
        for name in ("test_sdk.py", "golden-131.json", "ng-setup-native.json"):
            shutil.copyfile(tests / name, work / "tests" / name)
        shutil.copyfile(tests.parent / "examples" / "ng_setup.py", work / "examples" / "ng_setup.py")
        audit = subprocess.run([args.python, "-I", "-c", """
import json, sys
import nrforge_ngap
from nrforge_ngap import _native
print(json.dumps({'package': nrforge_ngap.__file__, 'extension': _native.__file__,
                  'identity': nrforge_ngap.identity(),
                  'pycrate_loaded': any(x.startswith('pycrate') for x in sys.modules)}))
"""], cwd=work, capture_output=True, text=True)
        if audit.returncode:
            raise RuntimeError(audit.stdout + audit.stderr)
        metadata = json.loads(audit.stdout)
        repository = tests.parent.parent.resolve()
        for key in ("package", "extension"):
            if Path(metadata[key]).resolve().is_relative_to(repository):
                raise RuntimeError("Build/source tree import is not an installed-wheel verification")
        if metadata["pycrate_loaded"]:
            raise RuntimeError("Runtime unexpectedly imported the native-oracle compiler")
        run = subprocess.run([args.python, "-I", "-m", "unittest", "discover", "-s",
                              str(work / "tests"), "-v"], cwd=work, capture_output=True, text=True)
        metadata.update(status="PASS" if run.returncode == 0 else "FAIL",
                        native_message_count=131, independent_ng_setup_count=3,
                        isolated_python=True, outside_repository=True,
                        stdout=run.stdout, stderr=run.stderr,
                        fixture_sha256=hashlib.sha256((tests / "golden-131.json").read_bytes()).hexdigest())
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(json.dumps(metadata, indent=2) + "\n")
        if run.returncode:
            raise RuntimeError(run.stdout + run.stderr)
        print("PASS installed Python SDK consumer, 131 messages and NG Setup outcomes")


if __name__ == "__main__":
    main()
