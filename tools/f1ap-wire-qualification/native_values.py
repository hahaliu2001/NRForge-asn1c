"""Frozen F1AP semantic profiles; independent native oracle, never generated codecs."""
import argparse
import ast
import copy
import hashlib
import importlib.util
import json
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
# Reuse the schema-driven semantic constructor, not NGAP wire/framing logic.
spec = importlib.util.spec_from_file_location('shared_native_semantic_constructor', REPO / 'tools/full-pdu-qualification/native_values.py')
shared = importlib.util.module_from_spec(spec)
spec.loader.exec_module(shared)
ref_spec = importlib.util.spec_from_file_location('f1ap_native_reference', Path(__file__).with_name('native_reference.py'))
reference = importlib.util.module_from_spec(ref_spec)
ref_spec.loader.exec_module(reference)
label, rows, table_open = shared.label, shared.rows, shared.table_open


class Builder(shared.Builder):
    def value(self, obj, path='', depth=0, table_row=None):
        value = super().value(obj, path, depth, table_row)
        if obj.TYPE == 'CHOICE':
            self.trace.append({'path': path, 'type': label(obj), 'choice': value[0]})
        elif obj.TYPE in ('INTEGER', 'ENUMERATED', 'BOOLEAN', 'NULL'):
            self.trace.append({'path': path, 'type': label(obj), 'primitive': value})
        elif obj.TYPE in ('BIT STRING', 'OCTET STRING'):
            self.trace.append({'path': path, 'type': label(obj), 'octets': len(value) if isinstance(value, bytes) else None,
                               'bits': value[1] if obj.TYPE == 'BIT STRING' else None})
        return value


def srb_extension_value(obj, value, selected, path=''):
    """Replace only actual native SRBID descriptor instances; preserve all other values."""
    if label(obj) == 'SRBID' and obj.TYPE == 'INTEGER':
        return selected, [{'path': path, 'type': 'SRBID', 'primitive': selected, 'declared_extension_union': True}]
    traces = []
    if obj.TYPE == 'SEQUENCE':
        result = {}
        for key, item in value.items():
            result[key], extra = srb_extension_value(obj._cont[key], item, selected, path+'.'+key)
            traces.extend(extra)
        return result, traces
    if obj.TYPE == 'SEQUENCE OF':
        result = []
        for index, item in enumerate(value):
            changed, extra = srb_extension_value(obj._cont, item, selected, f'{path}[{index}]')
            result.append(changed)
            traces.extend(extra)
        return result, traces
    if obj.TYPE == 'CHOICE':
        changed, traces = srb_extension_value(obj._cont[value[0]], value[1], selected, path+'.'+value[0])
        return (value[0], changed), traces
    if obj.TYPE == 'OPEN_TYPE' and not value[0].startswith('_unk_'):
        candidates = [row[obj._const_tab_id] for row in rows(obj) if label(row[obj._const_tab_id]) == value[0]]
        if not candidates or any(getattr(candidate._typeref, 'called', None) != getattr(candidates[0]._typeref, 'called', None) for candidate in candidates):
            raise shared.ConstructionError('Cannot uniquely resolve native SRBID traversal OPEN type '+value[0])
        changed, traces = srb_extension_value(candidates[0], value[1], selected, path+'.'+value[0])
        return (value[0], changed), traces
    return value, []


def native_case(native, obj, proc, root, value, trace, variant, criticality):
    case = {'message': label(obj), 'procedure': proc['procedureCode'], 'root': root,
            'criticality': criticality, 'variant': variant, 'semantic_value': repr(value),
            'declared_criticality': proc['criticality'], 'semantic_json': shared.json_value(value),
            'policy_profile': 'declared' if criticality == proc['criticality'] else 'received-criticality-variation',
            'known_table_trace': trace}
    try:
        obj.set_val(value)
        body = obj.to_aper()
        obj.from_aper(body)
        if obj.get_val() != value:
            raise shared.ConstructionError('Native BODY semantic mismatch')
        pdu = native.F1AP_PDU_Descriptions.F1AP_PDU
        envelope = root, {'procedureCode': proc['procedureCode'], 'criticality': criticality, 'value': (label(obj), value)}
        pdu.set_val(envelope)
        wire = pdu.to_aper()
        expected_root = {'initiatingMessage': 0, 'successfulOutcome': 64, 'unsuccessfulOutcome': 128}[root]
        if len(wire) < 4 or wire[0] != expected_root or wire[1] != proc['procedureCode'] or wire[2] != ('reject', 'ignore', 'notify').index(criticality) << 6:
            raise shared.ConstructionError('Native frozen four-root envelope header mismatch')
        pdu.from_aper(wire)
        if pdu.get_val() != envelope:
            raise shared.ConstructionError('Native complete PDU semantic mismatch')
        case.update(status='PASS', body_hex=body.hex(), pdu_hex=wire.hex(),
                    storage_semantic_json=shared.json_value(shared.storage_value(obj, value)))
    except Exception as error:
        case.update(status='FAIL', error_type=type(error).__name__, error=str(error))
    return case


def build_cases(native):
    reference.procedure_evidence(native)
    table = native.F1AP_PDU_Descriptions.F1AP_ELEMENTARY_PROCEDURES.get_val()
    cases = []
    for proc in list(table.root) + list(table.ext or []):
        for kind, root in [('InitiatingMessage', 'initiatingMessage'), ('SuccessfulOutcome', 'successfulOutcome'), ('UnsuccessfulOutcome', 'unsuccessfulOutcome')]:
            if kind not in proc:
                continue
            obj = proc[kind]
            for mode in ('low', 'contrast'):
                builder = Builder(mode)
                try:
                    value = builder.value(obj, label(obj))
                except Exception as error:
                    cases.append({'message': label(obj), 'procedure': proc['procedureCode'], 'root': root,
                                  'variant': mode+'-mandatory-minimum', 'status': 'FAIL',
                                  'error_type': type(error).__name__, 'error': str(error), 'known_table_trace': builder.trace})
                    continue
                variants = [(mode+'-mandatory-minimum', value, list(builder.trace))]
                for container, descriptor in obj._cont.items():
                    if descriptor.TYPE != 'SEQUENCE OF' or table_open(descriptor._cont) is None:
                        continue
                    opened = table_open(descriptor._cont)[1]
                    if not rows(opened) and 'id' in descriptor._cont._cont and descriptor._cont._cont['id'].TYPE == 'CHOICE':
                        changed = copy.deepcopy(value)
                        changed[container][0]['id'] = ('global', (2, 999, 3))
                        variants.append((mode+'-private-global-unknown', changed, list(builder.trace)))
                    for row in rows(opened):
                        if any(entry.get('id') == row.get('id') for entry in value.get(container, [])):
                            continue
                        extra = Builder(mode)
                        variant = f'{mode}-known-row-{row.get("id")}'
                        try:
                            item = extra.value(descriptor._cont, f'{label(obj)}.{container}[row={row.get("id")}]', table_row=row)
                            changed = copy.deepcopy(value)
                            changed.setdefault(container, []).append(item)
                            variants.append((variant, changed, builder.trace+extra.trace))
                        except Exception as error:
                            cases.append({'message': label(obj), 'procedure': proc['procedureCode'], 'root': root,
                                          'variant': variant, 'status': 'FAIL', 'error_type': type(error).__name__,
                                          'error': str(error), 'known_table_trace': builder.trace+extra.trace})
                extended = list(variants)
                for variant, semantic, trace in variants:
                    for selected in (4, 5):
                        changed, extension_trace = srb_extension_value(obj, semantic, selected, label(obj))
                        if extension_trace:
                            extended.append((variant+f'-srbid-extension-{selected}', changed, trace+extension_trace))
                for variant, semantic, trace in extended:
                    for criticality in ('reject', 'ignore', 'notify'):
                        cases.append(native_case(native, obj, proc, root, semantic, trace, variant, criticality))
    return cases


def build_profile(native):
    grouped = {}
    for case in build_cases(native):
        profile = grouped.setdefault(case['message'], {'message': case['message'], 'role': case['root'], 'code': case['procedure'], 'cases': []})
        record = dict(case)
        record['id'] = case['variant']+'-'+case.get('criticality', 'unknown')
        if 'semantic_value' in case:
            record['value'] = ast.literal_eval(case['semantic_value'])
        profile['cases'].append(record)
    return list(grouped.values())


def coverage_evidence(native, cases):
    """Reconcile declared top-level IE rows against actual successfully checked semantics."""
    table = native.F1AP_PDU_Descriptions.F1AP_ELEMENTARY_PROCEDURES.get_val()
    result = []
    for proc in list(table.root) + list(table.ext or []):
        for kind in ('InitiatingMessage', 'SuccessfulOutcome', 'UnsuccessfulOutcome'):
            if kind not in proc:
                continue
            obj = proc[kind]
            passed = [case for case in cases if case['message'] == label(obj) and case['status'] == 'PASS']
            for container, descriptor in obj._cont.items():
                if descriptor.TYPE != 'SEQUENCE OF' or table_open(descriptor._cont) is None:
                    continue
                declared = rows(table_open(descriptor._cont)[1])
                expected = sorted(row['id'] for row in declared)
                seen = set()
                for case in passed:
                    value = ast.literal_eval(case['semantic_value'])
                    seen.update(item['id'] for item in value.get(container, []) if isinstance(item['id'], int))
                result.append({'message': label(obj), 'container': container, 'declared_rows': expected,
                               'presence': {str(row['id']): row.get('presence') for row in declared},
                               'covered_rows': sorted(seen), 'missing_rows': sorted(set(expected)-seen),
                               'empty_private_object_set': not declared and 'id' in descriptor._cont._cont and descriptor._cont._cont['id'].TYPE == 'CHOICE'})
    traces = [entry for case in cases if case['status'] == 'PASS' for entry in case['known_table_trace']]
    return {'top_level_ie_rows': result,
            'declared_row_occurrences': sum(len(item['declared_rows']) for item in result),
            'covered_row_occurrences': sum(len(item['covered_rows']) for item in result),
            'missing_row_occurrences': sum(len(item['missing_rows']) for item in result),
            'srbid_extension_values': sorted({entry['primitive'] for entry in traces if entry.get('declared_extension_union')}),
            'actual_choice_selections': sorted({entry['type']+':'+entry['choice'] for entry in traces if 'choice' in entry}),
            'nested_optional_omissions': [entry for entry in traces if entry.get('optional_omitted_unconstructible')]}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--frozen', type=Path, required=True)
    parser.add_argument('--work', type=Path, required=True)
    args = parser.parse_args()
    manifest, texts = reference.verify_sources(REPO, args.frozen)
    native = reference.compile_native(texts, args.work)
    evidence = reference.procedure_evidence(native)
    cases = build_cases(native)
    inputs = [Path(__file__), Path(reference.__file__), Path(shared.__file__), REPO / 'tools/n11-envelope-qualification/native_reference.py', args.work / 'native_oracle.py']
    report = {'oracle_version': 'pycrate 0.7.11', 'source_manifest': manifest, 'inventory': evidence,
              'input_sha256': {str(path): hashlib.sha256(path.read_bytes()).hexdigest() for path in inputs},
              'messages': len(set(case['message'] for case in cases)), 'pass': sum(case['status'] == 'PASS' for case in cases),
              'fail': sum(case['status'] != 'PASS' for case in cases), 'cases': cases,
              'coverage': coverage_evidence(native, cases),
              'limits': ['Finite ASN.1 schema profiles, not application procedure semantics',
                         'Mandatory/minimal profiles and one declared optional/conditional top-level IE at a time, low/contrast values',
                         'Nested optional construction omissions remain explicit in trace; not exhaustive nested/extension coverage',
                         'Received criticality variations do not assert source-declared criticality policy',
                         'Private empty object set: unknown local/global keys with raw00, no vendor-known payload qualification',
                         'Native oracle failures are retained verbatim; no sender-byte normalization']}
    output = args.work / 'native-profile-report.json'
    output.write_text(json.dumps(report, indent=2)+'\n')
    print('native schema cases', report['pass'], '/', len(cases), 'messages', report['messages'])
    for case in cases:
        if case['status'] != 'PASS':
            print(case['message'], case['variant'], case['error'])
    raise SystemExit(1 if report['fail'] else 0)


if __name__ == '__main__':
    main()
