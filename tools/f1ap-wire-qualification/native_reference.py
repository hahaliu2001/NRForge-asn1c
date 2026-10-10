"""Independent pycrate oracle compiled from exactly the six frozen F1AP modules."""
import hashlib
import importlib.metadata
import importlib.util
import json
from pathlib import Path


def verify_sources(repo, root, manifest_path=None):
    repo, root = Path(repo), Path(root)
    manifest_path = manifest_path or repo / 'tools/f1ap-readiness/source-manifest.json'
    manifest = json.loads(Path(manifest_path).read_text())
    ordered = [line.strip() for line in (repo / 'tools/f1ap-readiness/f1ap-rel18.modules').read_text().splitlines()
               if line.strip() and not line.startswith('#')]
    if ordered != [item['path'] for item in manifest['modules']] or len(ordered) != 6:
        raise ValueError('Ordered six-module frozen F1AP authority changed')
    texts = []
    for item in manifest['modules']:
        data = (root / item['path']).read_bytes()
        blob = hashlib.sha1(b'blob ' + str(len(data)).encode() + b'\0' + data).hexdigest()
        if len(data) != item['size'] or blob != item['git_blob'] or hashlib.sha256(data).hexdigest() != item['sha256']:
            raise ValueError('Frozen F1AP source identity mismatch: ' + item['path'])
        texts.append(data.decode())
    return manifest, texts


def compile_native(texts, work):
    if importlib.metadata.version('pycrate') != '0.7.11':
        raise ValueError('F1-P4 requires pinned pycrate 0.7.11')
    from pycrate_asn1c.asnproc import compile_text
    from pycrate_asn1c.generator import PycrateGenerator
    work = Path(work)
    work.mkdir(parents=True, exist_ok=True)
    compile_text(texts)
    path = work / 'native_oracle.py'
    PycrateGenerator(str(path))
    spec = importlib.util.spec_from_file_location('f1ap_exact_native_oracle', path)
    native = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(native)
    return native


def procedure_evidence(native):
    table = native.F1AP_PDU_Descriptions.F1AP_ELEMENTARY_PROCEDURES.get_val()
    rows = list(table.root) + list(table.ext or [])
    roles = ('InitiatingMessage', 'SuccessfulOutcome', 'UnsuccessfulOutcome')
    codes = [row['procedureCode'] for row in rows]
    counts = {role: sum(role in row for row in rows) for role in roles}
    if len(rows) != 94 or len(set(codes)) != 94 or list(counts.values()) != [94, 36, 28]:
        raise ValueError('Frozen F1AP procedure/message inventory mismatch')
    pdu = native.F1AP_PDU_Descriptions.F1AP_PDU
    if list(pdu._root) != ['initiatingMessage', 'successfulOutcome', 'unsuccessfulOutcome', 'choice-extension'] or pdu._ext is not None:
        raise ValueError('Frozen F1AP four-root non-extensible PDU differs')
    return {'procedures': len(rows), 'messages': sum(counts.values()), 'root_rows': len(table.root),
            'addition_rows': len(table.ext or []), 'unique_codes': len(set(codes)),
            'codes': sorted(codes), 'roles': counts, 'pdu_roots': list(pdu._root), 'pdu_extensible': False}
