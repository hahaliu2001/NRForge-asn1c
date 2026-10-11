#!/usr/bin/env python3
"""Stage the self-contained finite RRC Python source distribution."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--output", type=Path, required=True)
    a = p.parse_args()
    repo = Path(__file__).resolve().parents[2]
    source = repo / "python/rrc"
    shutil.copytree(
        source, a.output, ignore=shutil.ignore_patterns("__pycache__", "build", "dist")
    )
    receipt = {
        str(p.relative_to(a.output)): hashlib.sha256(p.read_bytes()).hexdigest()
        for p in sorted(a.output.rglob("*"))
        if p.is_file()
    }
    (a.output / "staging-receipt.json").write_text(json.dumps(receipt, indent=2) + "\n")
    print("PASS self-contained RRC Python staging", len(receipt), "files")


if __name__ == "__main__":
    main()
