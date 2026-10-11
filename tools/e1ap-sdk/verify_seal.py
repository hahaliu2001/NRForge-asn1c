#!/usr/bin/env python3
"""Exercise the shared consistency verifier using disposable sealed snapshots."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess


def digest(path):
    h = hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda: stream.read(1 << 20), b''): h.update(block)
    return h.hexdigest()


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--repo', type=Path, required=True)
    ap.add_argument('--generated', type=Path, required=True)
    ap.add_argument('--build', type=Path, required=True)
    ap.add_argument('--work', type=Path, required=True)
    ap.add_argument('--cmake', default='cmake')
    args = ap.parse_args()
    work = args.work.resolve()
    work.mkdir(parents=True, exist_ok=False)
    generated, source = work/'generated', work/'source'
    shutil.copytree(args.generated, generated)
    provenance = json.loads((generated/'sdk-provenance.json').read_text())
    for name in provenance['source_hashes']:
        destination = source/name
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(args.repo/name, destination)
    archive = work/'sdk.a'
    try:
        os.link(args.build/'libnrforge_e1ap.a', archive)
    except OSError:
        shutil.copyfile(args.build/'libnrforge_e1ap.a', archive)
    archive_sha256 = digest(archive)
    receipt = work/'receipt.cmake'
    shutil.copyfile(args.build/'sdk-build-receipt.cmake', receipt)
    command = [args.cmake, '-DNRFORGE_SOURCE_ROOT='+str(source),
               '-DNRFORGE_GENERATED_ROOT='+str(generated), '-DNRFORGE_SDK_ARCHIVE='+str(archive),
               '-DNRFORGE_SDK_RECEIPT='+str(receipt), '-P', str(source/'cmake/VerifyNgapSdk.cmake')]
    cases = []
    def check(label, succeeds, diagnostic=''):
        result = subprocess.run(command, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        (work/(label+'.log')).write_text(result.stdout)
        if (result.returncode == 0) != succeeds or diagnostic not in result.stdout:
            raise RuntimeError('unexpected consistency gate result: '+label)
        cases.append({'case': label, 'status': 'PASS', 'returncode': result.returncode,
                      'log_sha256': hashlib.sha256(result.stdout.encode()).hexdigest()})
    check('unmodified-snapshot', True)
    extra = generated/'unexpected.hpp'
    extra.write_text('// unexpected input\n')
    check('extra-generated-file', False, 'file set differs')
    extra.unlink()
    header = generated/'sdk_version.hpp'
    original = header.read_bytes()
    header.unlink()
    check('missing-generated-file', False, 'file set differs')
    header.write_bytes(original+b'\n')
    check('altered-generated-header', False, 'fingerprint mismatch')
    header.write_bytes(original)
    core = source/'libngap/e1ap_pdu.cpp'
    original = core.read_bytes()
    core.write_bytes(original+b'\n')
    check('altered-production-source', False, 'fingerprint mismatch')
    core.write_bytes(original)
    original = receipt.read_bytes()
    receipt.unlink()
    check('missing-archive-receipt', False, 'no successful build receipt')
    receipt.write_bytes(original)
    archive.unlink()
    archive.write_bytes(b'changed archive')
    check('altered-archive', False, 'differs from successful build receipt')
    (work/'summary.json').write_text(json.dumps({'status':'PASS', 'cases':cases,
        'sdk_fingerprint':provenance['fingerprint'],'archive_sha256':archive_sha256,
        'scope':'actual shared verifier on copied inputs; no original input mutation'},indent=2)+'\n')
    print('PASS seal consistency and archive negatives')


if __name__ == '__main__':
    main()
