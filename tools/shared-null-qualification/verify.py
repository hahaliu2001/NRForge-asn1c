#!/usr/bin/env python3
"""Focused NULL native reference checks; no whole-NGAP qualification claim."""
import argparse
import hashlib
import json
from pathlib import Path
import asn1tools

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    repo = Path(__file__).resolve().parents[2]
    # asn1tools 0.167.0 ignores reordered outer CHOICE tags for PER ordering.
    # Use a canonically source-ordered equivalent solely for the native oracle;
    # actual generator fixture retains reverse storage/canonical tag evidence.
    schema = '''NativeNull DEFINITIONS AUTOMATIC TAGS ::= BEGIN
    Nothing ::= NULL
    Packet ::= SEQUENCE { first NULL, present NULL OPTIONAL, marker BOOLEAN }
    Select ::= CHOICE { empty [0] NULL, truth [1] BOOLEAN }
    Envelope ::= SEQUENCE { item NULL, signal BOOLEAN }
    Trigger ::= CHOICE { empty [0] NULL, coded [1] ENUMERATED { high(9), low(-2), ..., future(20) } }
    END'''
    codec = asn1tools.compile_string(schema, codec='per')
    cases = [('Nothing', None, '00')]
    for presence in (False, True):
        for marker in (False, True):
            value = {'first': None, 'marker': marker}
            if presence: value['present'] = None
            cases.append(('Packet', value, bytes([(128 if presence else 0) | (64 if marker else 0)]).hex()))
    cases += [('Select', ('empty', None), '00'), ('Select', ('truth', True), 'c0'), ('Envelope', {'item': None, 'signal': True}, '80')]
    cases += [("Trigger", ("coded", "high"), "a0"), ("Trigger", ("coded", "low"), "80"), ("Trigger", ("coded", "future"), "c000")]
    rows = []
    for type_name, value, expected in cases:
        encoded = codec.encode(type_name, value)
        # Native NULL emits zero octets; complete outer encoding needs substitution.
        normalized = encoded or b'\0'
        assert normalized.hex() == expected
        decoded = codec.decode(type_name, encoded)
        assert decoded == value
        rows.append({'type': type_name, 'value': value, 'native_hex': encoded.hex(), 'complete_hex': normalized.hex(), 'decode_checked': True})
    inputs = ['libasn1typed/asn1typed_render_cpp_inline_enum.c', 'libasn1typed/asn1typed.h', 'libasn1typed/asn1typed.c', 'libasn1typed/asn1typed_extract.c', 'libasn1typed/asn1typed_render_cpp_compound.c', 'libasn1typed/asn1typed_render_cpp_ioc.c', 'libasn1typed/check_asn1typed_null_render.c', 'libasn1typed/check_asn1typed_null_generated.cpp', 'libasn1typed/fixtures/null-generation-batch.asn1', 'libasn1typed/fixtures/ioc-null-batch.asn1']
    report = {'scope': 'Focused NULL native/model support, not NGAP qualification', 'native_version': asn1tools.__version__, 'canonical_native_schema': schema, 'limitations': ['Native CHOICE oracle is canonical source-order equivalent, not reordered-tag parser evidence.', 'Native empty encoding substitution is normalized to the frozen complete boundary.', 'Physical IOC vector is independent hand-model evidence in the C++ check, not asn1tools IOC qualification.'], 'cases': rows, 'input_sha256': {p: hashlib.sha256((repo/p).read_bytes()).hexdigest() for p in inputs}}
    args.output.write_text(json.dumps(report, indent=2)+'\n')
    print(f'PASS {len(rows)} NULL native encode/decode cases')
if __name__ == '__main__': main()
