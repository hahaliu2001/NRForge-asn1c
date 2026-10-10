#!/usr/bin/env python3
"""Guard regressions; requires the pinned external ASN.1 source set."""
import argparse
import importlib.util
from pathlib import Path
import shutil
import sys
import tempfile

sys.dont_write_bytecode = True

spec = importlib.util.spec_from_file_location('scan', Path(__file__).with_name('scan.py'))
scan = importlib.util.module_from_spec(spec)
spec.loader.exec_module(scan)

def rejected(call):
    try:
        call()
    except ValueError:
        return
    raise RuntimeError('guard accepted corrupted input')

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--repo', type=Path, default=Path('.'))
    ap.add_argument('--asn1-root', type=Path, required=True)
    args = ap.parse_args()
    repo = args.repo.resolve()
    source = args.asn1_root.resolve()
    scan.verify(repo, source)
    rows = scan.inventory(source)
    if len(rows) != 158 or rows != scan.inventory(source):
        raise RuntimeError('inventory count/determinism failed')
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        shutil.copytree(source / 'f1', root / 'f1')
        file = root / 'f1/asn1/F1AP-PDU-Descriptions.asn'
        original = file.read_bytes()
        file.write_bytes(original + b'\n')
        rejected(lambda: scan.verify(repo, root))
        file.write_bytes(original)
        text = file.read_text()
        # A mutation must actually occur; use a bounded replacement independent of spacing.
        import re
        changed, n = re.subn(r'(INITIATING MESSAGE\s+)Reset\b', r'\1MissingBody', text, count=1)
        if n != 1:
            raise RuntimeError('mutation target absent')
        file.write_text(changed)
        rejected(lambda: scan.inventory(root))
        file.write_text(text)
        module_dir = root / 'f1/asn1'
        (module_dir / 'F1AP-Constants.asn').write_text('')
        rejected(lambda: scan.inventory(root))
    print('PASS pinned identity, inventory determinism, source tamper and missing declaration/code guards')

if __name__ == '__main__':
    main()
