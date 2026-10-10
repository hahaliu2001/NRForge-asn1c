#!/usr/bin/env python3
"""Study-only bit model/native comparison; no NRForge runtime/codec qualification."""
import argparse
import hashlib
import importlib.metadata
import json
from pathlib import Path
import asn1tools

PROFILES = [(5, 5), (0, 1), (0, 254), (1, 256), (-128, 128),
            (0, 65535), (0, 65536), (1, 40000000)]
MIN, MAX = -(1 << 63), (1 << 63) - 1


def model(lower, upper, value, residue):
    bits = [True] * residue
    def put(number, width):
        bits.extend(bool(number & (1 << bit)) for bit in range(width - 1, -1, -1))
    def align():
        bits.extend([False] * (-len(bits) % 8))
    extension = not lower <= value <= upper
    put(int(extension), 1)
    if extension:
        length = next(n for n in range(1, 9) if -(1 << (8*n-1)) <= value < (1 << (8*n-1)))
        payload = value.to_bytes(length, 'big', signed=True)
        align()
        put(length, 8)
        for octet in payload:
            put(octet, 8)
    else:
        cardinality, offset = upper - lower + 1, value - lower
        if cardinality == 1:
            pass
        elif cardinality <= 255:
            put(offset, (cardinality - 1).bit_length())
        elif cardinality == 256:
            align(); put(offset, 8)
        elif cardinality <= 65536:
            align(); put(offset, 16)
        else:
            max_octets = ((cardinality - 1).bit_length() + 7) // 8
            length = max(1, (offset.bit_length() + 7) // 8)
            put(length - 1, (max_octets - 1).bit_length())
            align(); put(offset, length * 8)
    put(1, 1)  # A following field exposes the INTEGER end cursor.
    field_end = len(bits)
    align()
    octets = bytes(sum(int(bit) << (7-i) for i, bit in enumerate(bits[j:j+8]))
                   for j in range(0, len(bits), 8))
    return octets, field_end, extension


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    version = importlib.metadata.version('asn1tools')
    if version != '0.167.0':
        raise RuntimeError('Use pinned asn1tools 0.167.0')
    counts, vectors, total, extension_count = [], [], 0, 0
    for lower, upper in PROFILES:
        values = sorted({v for v in [lower, upper, lower-1, upper+1, MIN, MAX,
                                    -129, -128, -1, 0, 127, 128, 255, 256]
                         if MIN <= v <= MAX})
        root_cases, extension_cases = 0, 0
        for residue in range(8):
            prefix = ', '.join(f'p{i} BOOLEAN' for i in range(residue))
            fields = (prefix + ', ' if prefix else '') + 'value I, after BOOLEAN'
            schema = f'M DEFINITIONS ::= BEGIN I ::= INTEGER ({lower}..{upper}, ...) T ::= SEQUENCE {{ {fields} }} END'
            oracle = asn1tools.compile_string(schema, 'per')
            for value in values:
                datum = {f'p{i}': True for i in range(residue)}
                datum.update(value=value, after=True)
                expected, end, extension = model(lower, upper, value, residue)
                actual = oracle.encode('T', datum)
                assert actual == expected, (lower, upper, value, residue, actual.hex(), expected.hex())
                assert oracle.decode('T', expected) == datum
                total += 1
                root_cases += not extension
                extension_cases += extension
                extension_count += extension
                if residue == 0 and value in [lower, upper, lower-1, upper+1, -129, -128, -1, 127, 128, MIN, MAX]:
                    vectors.append(dict(root=[lower, upper], value=value, extension=extension,
                                        initial_bits=residue, following_bit=True,
                                        field_end_bit=end, complete_hex=expected.hex().upper()))
        counts.append(dict(root=[lower, upper], root_cases=root_cases, extension_cases=extension_cases))
    report = dict(scope='Study-only native and independent bit model; not generated codec/runtime or NGAP qualification',
                  baseline_commit='0bda141dea90f9df8baba0d6ecc64598c2330f00',
                  native=dict(name='asn1tools', version=version, codec='per'),
                  total_cases=total, extension_cases=extension_count, root_cases=total-extension_count,
                  residue_bits=list(range(8)), all_encode_bytes_equal=True, all_native_decode_equal=True,
                  profiles=counts, selected_vectors=vectors,
                  input_sha256={Path(__file__).name: hashlib.sha256(Path(__file__).read_bytes()).hexdigest()},
                  limitations=['No production implementation exercised', 'Only single finite contiguous roots',
                               'Only signed int64 values; no malformed input acceptance or atomicity proof',
                               'No complete-message qualification or readiness improvement claimed'])
    args.output.write_text(json.dumps(report, indent=2) + '\n')
    print(f'PASS {total} study cases ({total-extension_count} root, {extension_count} extension)')

if __name__ == '__main__':
    main()
