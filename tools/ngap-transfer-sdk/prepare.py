#!/usr/bin/env python3
"""Generate every selected standalone NGAP Transfer root before sealing.

Run after the unchanged 131-message dispatch generator, before seal_sdk.py.
This generates source only; it never declares compilation/qualification PASS.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess

import sys
sys.path.insert(0, str(Path(__file__).resolve().parents[2]/'python/ngap_sdk'))
from root_catalog import ROOTS, VERSION, cpp_type



def prepare(repo, root, generated):
    authority = json.loads((repo/'tools/n11-envelope-qualification/source-manifest.json').read_text())
    for row in authority['modules']:
        data = (root/row['path']).read_bytes()
        blob = hashlib.sha1(b'blob '+str(len(data)).encode()+b'\0'+data).hexdigest()
        if hashlib.sha256(data).hexdigest() != row['sha256'] or blob != row['git_blob']:
            raise ValueError('Transfer schema authority mismatch: '+row['path'])
    source = (root/'ng/asn1/NGAP-IEs.asn').read_text()
    explicit = set(re.findall(r'CONTAINING\s+([\w-]+)', re.sub(r'--[^\n]*', '', source)))
    if explicit != set(ROOTS.values()):
        raise ValueError('Frozen explicit CONTAINING inventory differs from current root catalog')
    target = generated/'transfers'
    target.mkdir(exist_ok=False)
    rows = {}
    for stem, name in ROOTS.items():
        directory = target/stem
        directory.mkdir()
        namespace = 'nrforge::ngap::transfers::'+stem
        result = subprocess.run([str(repo/'tools/asn1typed_ioc_probe'),
            '--module-list', str(repo/'tools/qualification/ngap-rel18.modules'),
            '--asn1-root', str(root), '--root-module', 'NGAP-IEs', '--message', name,
            '--output-prefix', str(directory/'root'), '--namespace', namespace,
            '--verify-determinism'], capture_output=True, text=True, check=True)
        (directory/'generation.stdout').write_text("\n".join(line for line in result.stdout.splitlines() if not line.startswith("OUTPUT "))+"\n")
        (directory/'generation.stderr').write_text(result.stderr)
        for family in ('types', 'mapping', 'codec'):
            for stage in ('GENERATION', 'DETERMINISM'):
                if f'{stage} {family} PASS' not in result.stdout:
                    raise ValueError('Missing Transfer generation evidence: '+stem)
        codec = (directory/'root_codec.hpp').read_text()
        expected_type = cpp_type(name)
        matches = re.findall(r'inline .*?\b(encode_\w+)\(const ([\w:]+)& v', codec)
        matches = [(e, t) for e, t in matches if t.split('::')[-1] == expected_type]
        if len(matches) != 1:
            raise ValueError('Ambiguous generated Transfer API: '+stem)
        encode, qualified = matches[0]
        decode = 'decode_'+encode[7:]
        # Four headers form one independently consumable public root entry.
        (directory/'root.hpp').write_text('#pragma once\n#include "root_types.hpp"\n'
            '#include "root_mapping.hpp"\n#include "root_codec.hpp"\n')
        rows[stem] = dict(message=name, namespace=namespace, cpp_type=qualified,
                          encode=encode, decode=decode,
                          headers=[f'transfers/{stem}/{n}' for n in
                                   ('root.hpp', 'root_types.hpp', 'root_mapping.hpp', 'root_codec.hpp')])
    manifest = dict(version=VERSION, roots=rows, source_authority=authority,
                    scope='generated independent roots; installed qualification pending')
    (generated/'transfer-manifest.json').write_text(json.dumps(manifest, indent=2, sort_keys=True)+'\n')
    return manifest


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('repo', 'asn1-root', 'generated'):
        parser.add_argument('--'+name, type=Path, required=True)
    args = parser.parse_args()
    prepare(args.repo.resolve(), args.asn1_root.resolve(), args.generated.resolve())
