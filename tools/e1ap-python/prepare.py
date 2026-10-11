#!/usr/bin/env python3
"""Stage a self-contained E1AP Python source distribution from shared sources."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil

def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--output', type=Path, required=True)
    args = ap.parse_args()
    repo = Path(__file__).resolve().parents[2]
    source, target = repo/'python', args.output.resolve()
    target.mkdir(parents=True, exist_ok=False)
    for name in ('ngap_sdk',):
        shutil.copytree(source/name, target/name, ignore=shutil.ignore_patterns('__pycache__'))
    for name in ('CMakeLists.txt', 'LICENSE', 'PYBIND11-LICENSE'):
        shutil.copyfile(source/name, target/name)
    project = (source/'pyproject.toml').read_text().replace('nrforge-ngap','nrforge-e1ap').replace('nrforge_ngap','nrforge_e1ap').replace('Typed NGAP','Typed E1AP')
    project = project.replace('sdist.include = [', 'sdist.include = ["staging-receipt.json", ')
    project += '\n[tool.scikit-build.cmake.define]\nNRFORGE_PYTHON_PROFILE = "e1ap"\n'
    (target/'pyproject.toml').write_text(project)
    package = target/'src/nrforge_e1ap'
    package.mkdir(parents=True)
    (package/'__init__.py').write_text((source/'src/nrforge_ngap/__init__.py').read_text().replace('NGAP','E1AP'))
    shutil.copyfile(repo/'tools/e1ap-python/README.md', target/'README.md')
    shutil.copytree(source/'e1ap/tests', target/'tests',
                    ignore=shutil.ignore_patterns('prepare_vectors.py', '__pycache__'))
    receipt = {str(p.relative_to(target)):hashlib.sha256(p.read_bytes()).hexdigest()
               for p in sorted(target.rglob('*')) if p.is_file()}
    (target/'staging-receipt.json').write_text(json.dumps(receipt,indent=2)+'\n')
    print('PASS self-contained E1AP Python source staging')

if __name__ == '__main__': main()
