"""Exact frozen-source oracle construction and independent aligned framing checks."""
import hashlib
import importlib.util
import json
from pathlib import Path


def verify_sources(repo, root, manifest_path):
    manifest = json.loads(Path(manifest_path).read_text())
    ordered = [s.strip() for s in (Path(repo) / 'tools/qualification/ngap-rel18.modules').read_text().splitlines() if s.strip() and not s.startswith('#')]
    if ordered != [m['path'] for m in manifest['modules']]:
        raise ValueError('Ordered six-module provenance changed')
    texts = []
    for item in manifest['modules']:
        data = (Path(root) / item['path']).read_bytes()
        blob = hashlib.sha1(b'blob ' + str(len(data)).encode() + b'\0' + data).hexdigest()
        if blob != item['git_blob'] or hashlib.sha256(data).hexdigest() != item['sha256']:
            raise ValueError('Frozen source identity mismatch: ' + item['path'])
        texts.append(data.decode())
    return manifest, texts


def compile_native(texts, work):
    from pycrate_asn1c.asnproc import compile_text
    from pycrate_asn1c.generator import PycrateGenerator
    compile_text(texts)  # All six untouched modules, with no installed NGAP schema.
    path = Path(work) / 'native_oracle.py'
    PycrateGenerator(str(path))
    spec = importlib.util.spec_from_file_location('n11_exact_native_oracle', path)
    native = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(native)
    return native


def procedure_evidence(native):
    descriptions = native.NGAP_PDU_Descriptions
    table = descriptions.NGAP_ELEMENTARY_PROCEDURES.get_val()
    rows = table.root + (table.ext or [])
    codes = [r['procedureCode'] for r in rows]
    target = descriptions.uEContextRelease.get_val()
    return {
        'root_rows': len(table.root), 'addition_rows': len(table.ext or []),
        'unique_codes': len(set(codes)), 'codes': sorted(codes),
        'class_default_criticality': descriptions.NGAP_ELEMENTARY_PROCEDURE._cont['criticality']._def,
        'target_code': target['procedureCode'], 'target_criticality': target['criticality'],
        'target_initiating': str(target['InitiatingMessage']._typeref),
        'target_successful': str(target['SuccessfulOutcome']._typeref),
        'target_has_unsuccessful': 'UnsuccessfulOutcome' in target,
    }


def aligned_open(wire, offset):
    """Parse canonical APER octet determinants; never modify the sender bytes."""
    start = offset
    chunks = []
    fragmented = False
    lengths = []
    while True:
        if offset >= len(wire):
            raise ValueError('Missing open-type determinant')
        lead = wire[offset]; offset += 1
        if lead < 128:
            length, more = lead, False
            if length == 0 and not fragmented:
                raise ValueError('Zero total open type is not a supported value')
        elif lead < 192:
            if offset >= len(wire): raise ValueError('Truncated long determinant')
            length = ((lead & 63) << 8) | wire[offset]; offset += 1
            if length < 128: raise ValueError('Nonminimal long determinant')
            more = False
        else:
            multiplier = lead & 63
            if not 1 <= multiplier <= 4: raise ValueError('Reserved fragment multiplier')
            length, more = multiplier * 16384, True
            fragmented = True
        if length > len(wire) - offset: raise ValueError('Truncated open payload')
        chunks.append(wire[offset:offset+length]); lengths.append(length); offset += length
        if not more: break
    return b''.join(chunks), offset, {
        'determinant_start': start, 'fragmented': fragmented, 'chunk_octets': lengths,
    }


def initiating_body(wire, criticality):
    """Independent frozen target header proof: CHOICE3, code41, received policy."""
    if len(wire) < 4 or wire[0] != 0 or wire[1] != 41 or wire[2] != criticality << 6:
        raise ValueError('Target envelope selector/code/criticality/alignment differs')
    body, end, framing = aligned_open(wire, 3)
    if end != len(wire): raise ValueError('Trailing octets outside envelope open type')
    return body, framing


def fragmented_ue_child(native, body_wire, body_value):
    """Prove known UE-ID child fragmentation separately from native BODY decode."""
    entries = body_value['protocolIEs']
    if len(body_wire) < 7 or body_wire[0] != 0 or int.from_bytes(body_wire[1:3], 'big') != len(entries):
        raise ValueError('Unexpected frozen body root/count header')
    if int.from_bytes(body_wire[3:5], 'big') != 114 or body_wire[5] != 0:
        raise ValueError('Unexpected first IE ID/criticality/alignment')
    child, _, framing = aligned_open(body_wire, 6)
    if not framing['fragmented']: raise ValueError('Expected independently framed child fragment')
    obj = native.NGAP_IEs.UE_NGAP_IDs
    expected = entries[0]['value'][1]
    obj.set_val(expected)
    if child != obj.to_aper(): raise ValueError('Reassembled child differs from standalone native encoding')
    obj.from_aper(child)
    if obj.get_val() != expected: raise ValueError('Standalone native child semantics differ')
    return dict(framing, child_octets=len(child), child_sha256=hashlib.sha256(child).hexdigest(), standalone_exact=True)


def opaque_root_payload(wire, root, code, criticality):
    selector = {'i': 0, 's': 32, 'u': 64}[root]
    if len(wire) < 4 or wire[0] != selector or wire[1] != code or wire[2] != criticality << 6:
        raise ValueError('Opaque root selector/code/criticality/alignment differs')
    payload, end, framing = aligned_open(wire, 3)
    if end != len(wire): raise ValueError('Opaque root trailing octets')
    return payload, framing


def extension_payload(wire, expected_index):
    if not wire or not wire[0] & 128: raise ValueError('Missing outer CHOICE extension bit')
    if not wire[0] & 64:
        index, offset = wire[0] & 63, 1
    else:
        if wire[0] != 192: raise ValueError('Nonzero large-index alignment padding')
        integer, offset, _ = aligned_open(wire, 1)
        if len(integer) > 1 and integer[0] == 0: raise ValueError('Nonminimal normally-small large integer')
        index = int.from_bytes(integer, 'big')
        if index < 64: raise ValueError('Nonminimal normally-small large form')
    if index != expected_index: raise ValueError('Outer extension index differs')
    payload, end, framing = aligned_open(wire, offset)
    if end != len(wire): raise ValueError('Outer extension trailing octets')
    return payload, framing
