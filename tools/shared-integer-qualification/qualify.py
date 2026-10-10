#!/usr/bin/env python3
"""Generated signed extensible INTEGER codecs vs independent model/native PER."""
import argparse
import hashlib
import importlib.metadata
import importlib.util
import json
from pathlib import Path
import os
import subprocess
import tempfile
import sys
sys.dont_write_bytecode = True
import asn1tools

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location('integer_study_reference', ROOT / 'tools/n18-integer-study/reference.py')
reference = importlib.util.module_from_spec(spec)
spec.loader.exec_module(reference)


def main():
    p = argparse.ArgumentParser()
    p.add_argument('--output', type=Path, required=True)
    args = p.parse_args()
    if importlib.metadata.version('asn1tools') != '0.167.0':
        raise RuntimeError('Use pinned asn1tools 0.167.0')
    subprocess.run(['make', '-C', str(ROOT / 'libasn1typed'), 'check_asn1typed_domain_render'], check=True, stdout=subprocess.DEVNULL)
    rows = []
    for profile, (lower, upper) in enumerate(reference.PROFILES):
        values = sorted({v for v in [lower, upper, lower-1, upper+1, reference.MIN, reference.MAX,
                                    -129, -128, -1, 0, 127, 128, 255, 256]
                         if reference.MIN <= v <= reference.MAX})
        for residue in range(8):
            prefix = ', '.join(f'p{i} BOOLEAN' for i in range(residue))
            fields = (prefix + ', ' if prefix else '') + 'value I, after BOOLEAN'
            schema = f'M DEFINITIONS ::= BEGIN I ::= INTEGER ({lower}..{upper}, ...) T ::= SEQUENCE {{ {fields} }} END'
            native = asn1tools.compile_string(schema, 'per')
            for value in values:
                data = {f'p{i}': True for i in range(residue)}
                data.update(value=value, after=True)
                model, end, extension = reference.model(lower, upper, value, residue)
                assert native.encode('T', data) == model
                assert native.decode('T', model) == data
                rows.append((profile, residue, value, model.hex().upper(), end, extension))
    with tempfile.TemporaryDirectory(prefix='nrforge-integer-qualification-') as work:
        work = Path(work)
        subprocess.run([str(ROOT / 'libasn1typed/check_asn1typed_domain_render'), str(ROOT / 'libasn1typed/fixtures/extensible-integers-batch.asn1'), 'IntegerDomains', 'domains', str(work)], check=True)
        compiler = os.environ.get('CXX', 'c++').split()
        subprocess.run(compiler + ['-std=c++20', '-Wall', '-Wextra', '-Werror', '-pedantic-errors', '-Wconversion', '-Wsign-conversion', '-DNDEBUG', '-I' + str(ROOT / 'libaper'), '-I' + str(work), str(ROOT / 'libasn1typed/check_asn1typed_domain_generated.cpp'), str(ROOT / 'libaper/runtime.cpp'), '-o', str(work / 'driver')], check=True)
        subprocess.run([str(work / 'driver')], check=True)
        output = subprocess.check_output([str(work / 'driver'), 'reference'], input=''.join(f'{profile} {residue} {value}\n' for profile,residue,value,*_ in rows), text=True).splitlines()
        assert len(output) == len(rows)
        for row, actual in zip(rows, output):
            assert actual == f'{row[3]} {row[4]}', (row, actual)
        header_hashes = {name: hashlib.sha256((work / name).read_bytes()).hexdigest() for name in ['types.hpp','mapping.hpp','codec.hpp']}
    files = ['libaper/runtime.hpp','libaper/runtime.cpp','libasn1typed/asn1typed.h','libasn1typed/asn1typed.c','libasn1typed/asn1typed_extract.c','libasn1typed/asn1typed_render_cpp.h','libasn1typed/asn1typed_render_cpp_integer.c','libasn1typed/asn1typed_render_cpp_compound.c','libasn1typed/asn1typed_render_cpp_ioc.c','libasn1typed/asn1typed_render_cpp_inline_enum.c','libasn1typed/asn1typed_render_cpp_enum.c','libasn1typed/asn1typed_render_cpp_slice.c','libasn1typed/asn1typed_name.c','libasn1typed/check_asn1typed_domain_render.c','libasn1typed/check_asn1typed_domain_generated.cpp','libasn1typed/fixtures/extensible-integers-batch.asn1','tools/n18-integer-study/reference.py','tools/shared-integer-qualification/qualify.py']
    report = dict(scope='Actual generated signed extensible INTEGER codecs; not complete NGAP qualification',
                  native=dict(name='asn1tools',version='0.167.0',codec='per'), total_cases=len(rows),
                  root_cases=sum(not row[5] for row in rows), extension_cases=sum(row[5] for row in rows),
                  residues=list(range(8)), all_generated_encode_bytes_equal=True, all_generated_decode_values_equal=True,
                  all_native_decode_fields_equal=True, profiles=reference.PROFILES, generated_header_sha256=header_hashes,
                  source_sha256={name:hashlib.sha256((ROOT / name).read_bytes()).hexdigest() for name in files},
                  limitations=['Contiguous domains only in native comparison; pinned native parser drops union tails',
                               'Discontinuous permitted roots use retained-set / hull-offset independent model and legacy constraint evidence',
                               'Hull-gap values excluded from bounded supported domain even with extensibility',
                               'Malformed input, atomicity, lifecycle and allocation coverage belongs to focused runtime tests',
                               'No complete-message/PDU interoperability or readiness claim'])
    args.output.write_text(json.dumps(report,indent=2)+'\n')
    print(f'PASS {len(rows)} actual generated/native/model encode/decode cases')

if __name__ == '__main__':
    main()
