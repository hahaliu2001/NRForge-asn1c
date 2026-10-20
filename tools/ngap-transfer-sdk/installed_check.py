#!/usr/bin/env python3
"""Executed by a fresh consumer Python -I with no oracle/source imports."""
import hashlib
import json
from pathlib import Path
import sys
import nrforge_ngap as sdk


def restore(value):
    if isinstance(value, dict):
        if set(value) == {'bytes_hex'}:
            return bytes.fromhex(value['bytes_hex'])
        return {k: restore(v) for k, v in value.items()}
    if isinstance(value, list):
        return [restore(v) for v in value]
    return value


def main():
    vectors_path, identity_path, result_path = map(Path, sys.argv[1:])
    identity = json.loads(identity_path.read_text())
    expected_identity = {k: identity[k] for k in ('version', 'fingerprint', 'schema_sha256', 'runtime_sha256', 'source_revision')}
    assert sdk.identity() == expected_identity
    assert len(sdk.messages()) == 131
    assert set(sdk.transfers()) == {row['message'] for row in identity['transfer_roots'].values()}
    vectors = restore(json.loads(vectors_path.read_text()))
    assert {row['root'] for row in vectors} == set(sdk.transfers())
    assert not any(name.startswith('pycrate') for name in sys.modules)
    assert Path(sdk.__file__).resolve().is_relative_to(Path(sys.prefix).resolve())
    checks = 0
    for row in vectors:
        root, wire, expected = row['root'], row['wire'], row['body']
        if len(wire) > 4096:
            try:
                sdk.decode_transfer(root, wire, limits={'max_input_octets': 8192})
            except sdk.CodecError as error:
                assert error.code == 'resource_limit'
            else:
                raise AssertionError('Oversized Transfer accepted')
            checks += 1
            continue
        actual = sdk.decode_transfer(root, wire)
        assert actual == expected, row['name']
        # A separately oracle-constructed object drives encoding: not just
        # encode(decode(wire)); compare the full typed value above as well.
        try:
            encoded = sdk.encode_transfer(root, expected)
        except sdk.CodecError as error:
            assert not row['encode'] and error.code == 'constraint_violation', row['name']
        else:
            assert row['encode'] and encoded == wire, row['name']
            try:
                sdk.encode_transfer(root, expected, limits={'max_output_octets': len(wire)-1})
            except sdk.CodecError as error:
                assert error.code == 'resource_limit'
            else:
                raise AssertionError('Output budget ignored')
        for limits in ({'max_nodes': 0}, {'max_bytes': 0}):
            try:
                sdk.decode_transfer(root, wire, conversion_limits=limits)
            except (ValueError, sdk.CodecError):
                pass
            else:
                raise AssertionError('Conversion budget ignored')
        checks += 1
    root = 'PDUSessionResourceReleaseResponseTransfer'
    extended = sdk.decode_transfer(root, bytes.fromhex('80400180'))
    assert extended['sequence_extensions'] == dict(received_bitmap_bit_count=1,
        unknown_additions=[dict(addition_index=0, payload_octets=b'\x80')])
    try:
        sdk.encode_transfer(root, extended)
    except sdk.CodecError as error:
        assert error.code == 'constraint_violation'
    else:
        raise AssertionError('Unknown SEQUENCE addition was re-emitted')
    for row in vectors:
        if len(row['wire']) > 4096:
            continue
        try:
            sdk.decode_transfer(row['root'], row['wire']+b'\0')
        except sdk.CodecError as error:
            if len(row['wire']) == 4096:
                assert (error.code, error.bit_offset) == ('resource_limit', 0), row['name']
            else:
                assert error.code == 'trailing_data', row['name']
        else:
            raise AssertionError('Trailing bytes accepted')
    try:
        sdk.decode_transfer('PDUSessionResourceReleaseResponseTransfer', b'\x01')
    except sdk.CodecError as error:
        assert error.code == 'nonzero_padding'
    else:
        raise AssertionError('Nonzero terminal padding accepted')
    assert not any(name.startswith('pycrate') for name in sys.modules)
    report = dict(status='PASS', cases=checks, identity=sdk.identity(),
                  package=sdk.__file__, vectors_sha256=hashlib.sha256(vectors_path.read_bytes()).hexdigest(),
                  scope='finite installed Transfer binding vectors; no production receive adapter',
                  pycrate_loaded=False)
    result_path.write_text(json.dumps(report, indent=2)+'\n')
    print('PASS installed selected-root Python vectors:', checks)


if __name__ == '__main__':
    main()
