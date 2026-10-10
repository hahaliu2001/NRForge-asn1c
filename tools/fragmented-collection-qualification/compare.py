#!/usr/bin/env python3
"""Compare actual generated codec vectors with independent asn1tools APER."""
import argparse
import hashlib
import json
from pathlib import Path
import asn1tools
p=argparse.ArgumentParser()
p.add_argument('generated_vectors',type=Path)
p.add_argument('--output',type=Path,required=True)
a=p.parse_args()
m=asn1tools.compile_string('X DEFINITIONS ::= BEGIN Large ::= SEQUENCE (SIZE (1..65536)) OF BOOLEAN END','per')
rows=[]
for count in (1,127,128,16383,16384,32768,65535,65536):
    actual=(a.generated_vectors/f'{count}.bin').read_bytes()
    reference=m.encode('Large',[False]*count)
    assert actual==reference, count
    assert m.decode('Large',actual)==[False]*count
    rows.append(dict(count=count,octets=len(actual),sha256=hashlib.sha256(actual).hexdigest(),encode='PASS',decode='PASS'))
root=Path(__file__).resolve().parents[2]
sources={path:hashlib.sha256((root/path).read_bytes()).hexdigest() for path in ('libaper/runtime.hpp','libaper/runtime.cpp','libasn1typed/asn1typed_render_cpp_compound.c','libasn1typed/check_asn1typed_collection_generated.cpp','libasn1typed/fixtures/collection-generation-n8.asn1')}
a.output.write_text(json.dumps(dict(source_sha256=sources,reference='asn1tools',version=asn1tools.__version__,codec='per',rows=rows),indent=2)+'\n')
