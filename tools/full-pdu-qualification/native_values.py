"""Independent frozen pycrate semantic constructor; no generated codec logic."""
import argparse
import copy
import hashlib
import importlib.util
import importlib.metadata
import json
from pathlib import Path
import sys

REPO = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO / 'tools/n11-envelope-qualification'))
from native_reference import verify_sources, compile_native

class ConstructionError(Exception):
    pass

def minimum(constraint, fallback=0):
    if constraint is None or not constraint.root:
        return fallback
    vals = [item.lb if hasattr(item, 'lb') else item for item in constraint.root]
    return min(value for value in vals if value is not None)

def label(obj):
    ref = getattr(obj, '_typeref', None)
    return ref.called[1] if ref and hasattr(ref, 'called') and isinstance(ref.called, tuple) else obj.TYPE

def rows(obj):
    table = getattr(obj, '_const_tab', None)
    if table is None:
        return []
    values = table.get_val()
    return list(values.root) + list(values.ext or [])

def table_open(obj):
    if obj.TYPE != 'SEQUENCE':
        return None
    for name, field in obj._cont.items():
        if field.TYPE == 'OPEN_TYPE' and getattr(field, '_const_tab', None) is not None:
            return name, field
    return None

def json_value(value):
    if isinstance(value, bytes):
        return {'kind': 'bytes', 'hex': value.hex()}
    if isinstance(value, tuple):
        return {'kind': 'tuple', 'items': [json_value(item) for item in value]}
    if isinstance(value, list):
        return [json_value(item) for item in value]
    if isinstance(value, dict):
        return {key: json_value(item) for key, item in value.items()}
    return value

def storage_value(obj, value):
    """Resolve CONTAINING to genuine native bytes for generated opaque OCTET storage."""
    if obj.TYPE == 'OCTET STRING' and isinstance(value, tuple):
        contained = obj._const_cont
        contained.set_val(value[1])
        return contained.to_aper()
    if obj.TYPE == 'SEQUENCE':
        return {key: storage_value(obj._cont[key], item) for key, item in value.items()}
    if obj.TYPE == 'SEQUENCE OF':
        return [storage_value(obj._cont, item) for item in value]
    if obj.TYPE == 'CHOICE':
        return value[0], storage_value(obj._cont[value[0]], value[1])
    if obj.TYPE == 'OPEN_TYPE' and not value[0].startswith('_unk_'):
        for row in rows(obj):
            selected = row[obj._const_tab_id]
            if label(selected) == value[0]:
                return value[0], storage_value(selected, value[1])
        raise ConstructionError('missing OPEN semantic type ' + value[0])
    return value

def native_case(native, obj, proc, root, value, trace, variant, criticality):
    case = {'message': label(obj), 'procedure': proc['procedureCode'], 'root': root,
            'criticality': criticality, 'variant': variant, 'semantic_value': repr(value),
            'declared_criticality': proc['criticality'],
            'policy_profile': 'declared' if criticality == proc['criticality'] else 'received-criticality-variation',
            'semantic_json': json_value(value), 'known_table_trace': trace}
    try:
        obj.set_val(value)
        wire = obj.to_aper()
        obj.from_aper(wire)
        if obj.get_val() != value:
            raise ConstructionError('native BODY semantic mismatch')
        pdu = native.NGAP_PDU_Descriptions.NGAP_PDU
        envelope = root, {'procedureCode': proc['procedureCode'], 'criticality': criticality, 'value': (label(obj), value)}
        pdu.set_val(envelope)
        encoded = pdu.to_aper()
        pdu.from_aper(encoded)
        if pdu.get_val() != envelope:
            raise ConstructionError('native complete PDU semantic mismatch')
        case.update(status='PASS', body_hex=wire.hex(), pdu_hex=encoded.hex(),
                    storage_semantic_json=json_value(storage_value(obj,value)))
    except Exception as error:
        case.update(status='FAIL', error=str(error))
    return case

class Builder:
    def __init__(self, mode='low'):
        self.trace = []
        self.mode = mode

    def value(self, obj, path='', depth=0, table_row=None):
        if depth > 80:
            raise ConstructionError('recursive graph limit at ' + path)
        typ = obj.TYPE
        if typ == 'SEQUENCE':
            opened = table_open(obj)
            if opened:
                name, field = opened
                choices = rows(field)
                if table_row is None:
                    preferred = [row for row in choices if row.get('presence') == 'mandatory']
                    table_row = (preferred or choices or [None])[0]
                if table_row is None:
                    idfield = (obj._cont['id'] if 'id' in obj._cont else None)
                    if idfield is not None and idfield.TYPE == 'CHOICE' and set(idfield._root) == {'local', 'global'}:
                        self.trace.append({'path': path, 'private_unknown': True, 'key': 'local:0', 'payload': '00', 'claim': 'unknown raw payload transparency only'})
                        return {'id': self.value(idfield, path+'.id', depth+1), 'criticality': self.value(obj._cont['criticality'],path+'.criticality',depth+1), name: ('_unk_004', b'\x00')}
                    raise ConstructionError('empty known object set at ' + path)
                self.trace.append({'path': path, 'row_id': table_row.get('id'), 'presence': table_row.get('presence'), 'table_field': name})
            result = {}
            members = list(obj._root_mand) + (list(obj._root_opt) if self.mode == 'contrast' else [])
            for name in members:
                field = obj._cont[name]
                try:
                    if table_row and name in table_row and not hasattr(table_row[name], 'TYPE'):
                        result[name] = table_row[name]
                    elif field.TYPE == 'OPEN_TYPE' and table_row:
                        selected = table_row[field._const_tab_id]
                        result[name] = (label(selected), self.value(selected, path + '.' + name, depth+1))
                    else:
                        result[name] = self.value(field, path + '.' + name, depth+1)
                except ConstructionError:
                    if name not in obj._root_opt:
                        raise
                    self.trace.append({'path':path+'.'+name,'optional_omitted_unconstructible':True})
            return result
        if typ == 'SEQUENCE OF':
            count = minimum(getattr(obj, '_const_sz', None))
            item = obj._cont
            opened = table_open(item)
            if opened:
                candidates = rows(opened[1])
                required = [row for row in candidates if row.get('presence') == 'mandatory']
                selected = list(required)
                for row in candidates:
                    if len(selected) >= count:
                        break
                    if row not in selected:
                        selected.append(row)
                if len(selected) < count:
                    if not candidates and 'id' in item._cont and item._cont['id'].TYPE == 'CHOICE':
                        return [self.value(item,f'{path}[{index}]',depth+1) for index in range(count)]
                    raise ConstructionError('cannot satisfy IOC container minimum without duplicate rows at ' + path)
                return [self.value(item, f'{path}[{index}]', depth+1, row) for index, row in enumerate(selected)]
            return [self.value(item, f'{path}[{index}]', depth+1) for index in range(count)]
        if typ == 'CHOICE':
            failures = []
            for name in (list(reversed(obj._root)) if self.mode == 'contrast' else obj._root):
                saved = len(self.trace)
                try:
                    return name, self.value(obj._cont[name], path + '.' + name, depth+1)
                except ConstructionError as error:
                    del self.trace[saved:]
                    failures.append(str(error))
            raise ConstructionError('no constructible root CHOICE at ' + path + ': ' + '; '.join(failures))
        if typ == 'INTEGER':
            constraint = getattr(obj, '_const_val', None)
            if self.mode == 'contrast':
                if constraint is None or not constraint.root:
                    return 1
                values = [item.ub if hasattr(item,'ub') else item for item in constraint.root]
                return max(value for value in values if value is not None)
            return minimum(constraint)
        if typ == 'ENUMERATED':
            return obj._root[-1] if self.mode == 'contrast' else obj._root[0]
        if typ == 'BOOLEAN':
            return self.mode == 'contrast'
        if typ == 'NULL':
            return 0
        if typ == 'OCTET STRING':
            contained = getattr(obj, '_const_cont', None)
            if contained is not None:
                return label(contained), self.value(contained,path+'.CONTAINING',depth+1)
            size = getattr(obj, '_const_sz', None)
            width = minimum(size)
            if self.mode == 'contrast' and not width and (size is None or size.ub is None or size.ub > 0):
                width = 1
            return bytes([165 if self.mode == 'contrast' else 0])*width
        if typ == 'BIT STRING':
            width = minimum(getattr(obj, '_const_sz', None))
            return ((1 << width)-1 if self.mode == 'contrast' else 0), width
        if typ in ('PrintableString', 'VisibleString', 'UTF8String', 'IA5String', 'NumericString'):
            width = minimum(getattr(obj,'_const_sz',None))
            return ('9' if typ == 'NumericString' and self.mode == 'contrast' else '0' if typ == 'NumericString' else 'Z' if self.mode == 'contrast' else 'A') * width
        if typ == 'OBJECT IDENTIFIER':
            return (2,999,3) if self.mode == 'contrast' else (0,0)
        raise ConstructionError('unsupported native descriptor ' + typ + ' at ' + path)

def build_cases(native):
    procedures = native.NGAP_PDU_Descriptions.NGAP_ELEMENTARY_PROCEDURES.get_val()
    cases = []
    for proc in procedures.root + (procedures.ext or []):
        for kind, root in [('InitiatingMessage', 'initiatingMessage'), ('SuccessfulOutcome', 'successfulOutcome'), ('UnsuccessfulOutcome', 'unsuccessfulOutcome')]:
            if kind not in proc:
                continue
            obj = proc[kind]
            for mode in ('low','contrast'):
                builder = Builder(mode)
                try:
                    value = builder.value(obj, label(obj))
                    variants = [(mode+'-mandatory-minimum',value,list(builder.trace))]
                    for container, descriptor in obj._cont.items():
                        if descriptor.TYPE != 'SEQUENCE OF' or table_open(descriptor._cont) is None:
                            continue
                        opened = table_open(descriptor._cont)[1]
                        if not rows(opened) and 'id' in descriptor._cont._cont and descriptor._cont._cont['id'].TYPE == 'CHOICE':
                            changed = copy.deepcopy(value)
                            changed[container][0]['id'] = ('global', (2,999,3))
                            variants.append((mode+'-private-global-unknown',changed,list(builder.trace)))
                        for row in rows(opened):
                            if any(entry.get('id') == row.get('id') for entry in value.get(container,[])):
                                continue
                            extra = Builder(mode)
                            item = extra.value(descriptor._cont, f'{label(obj)}.{container}[row={row.get("id")}]', table_row=row)
                            changed = copy.deepcopy(value)
                            changed.setdefault(container,[]).append(item)
                            variants.append((f'{mode}-known-row-{row.get("id")}', changed, builder.trace+extra.trace))
                    for variant, semantic, trace in variants:
                        for criticality in ('reject','ignore','notify'):
                            cases.append(native_case(native,obj,proc,root,semantic,trace,variant,criticality))
                except Exception as error:
                    cases.append({'message':label(obj),'variant':'construction','status':'FAIL','error':str(error),'known_table_trace':builder.trace})
    return cases


def build_profile(native):
    """Grouped in-process API with true Python native values and checked wire."""
    import ast
    grouped = {}
    for case in build_cases(native):
        key = case["message"]
        profile = grouped.setdefault(key, {"message": key, "role": case.get("root"), "code": case.get("procedure"), "cases": []})
        record = dict(case)
        record["id"] = case.get("variant", "construction")+"-"+case.get("criticality", "unknown")
        if "semantic_value" in case:
            record["value"] = ast.literal_eval(case["semantic_value"])
        profile["cases"].append(record)
    return list(grouped.values())


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--frozen', type=Path, required=True)
    parser.add_argument('--work', type=Path, default=Path(__file__).parent)
    args = parser.parse_args()
    if importlib.metadata.version('pycrate') != '0.7.11':
        raise ValueError('This prototype requires pinned pycrate 0.7.11')
    args.work.mkdir(parents=True, exist_ok=True)
    manifest, texts = verify_sources(REPO, args.frozen, REPO / 'tools/n11-envelope-qualification/source-manifest.json')
    native = compile_native(texts, args.work)
    procedures = native.NGAP_PDU_Descriptions.NGAP_ELEMENTARY_PROCEDURES.get_val()
    cases = build_cases(native)
    report = {'oracle_version':'pycrate 0.7.11', 'source_manifest': manifest, 'prototype_sha256':hashlib.sha256(Path(__file__).read_bytes()).hexdigest(), 'native_oracle_sha256':hashlib.sha256((args.work/'native_oracle.py').read_bytes()).hexdigest(), 'procedures': len(procedures.root + (procedures.ext or [])), 'messages': len(set(case['message'] for case in cases)), 'pass': sum(case['status']=='PASS' for case in cases), 'cases': cases, 'limits': ['ASN.1 schema + mandatory IE row validity only, not application procedure semantics', 'CONTAINING transfer descriptors recursively constructed and natively decoded; unrelated NAS OCTET bytes opaque', 'Conditional IE requirements require independent acceptance profiles', 'Received criticality variations are legal schema encodings but not a claim of source-declared policy conformance', 'Empty private object set uses explicit unknown local/global key and raw00 entry, transparency only']}
    (args.work / 'minimal-native-report.json').write_text(json.dumps(report, indent=2)+'\n')
    print('native schema cases', report['pass'], '/', len(cases), 'messages', report['messages'])
    for case in cases:
        if case['status'] != 'PASS':
            print(case['message'], case['error'])

if __name__ == '__main__':
    main()
