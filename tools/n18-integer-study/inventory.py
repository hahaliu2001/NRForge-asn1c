#!/usr/bin/env python3
"""Reproduce the N17 20+4 first-failure INTEGER evidence, not a new scan."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import tempfile


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--asn1-root', type=Path, required=True)
    ap.add_argument('--probe', type=Path, required=True)
    ap.add_argument('--output', type=Path, required=True)
    args = ap.parse_args()
    repo = Path(__file__).resolve().parents[2]
    baseline = repo / 'tools/n17-shape-qualification/readiness.json'
    readiness = json.loads(baseline.read_text())
    targets = []
    for message in readiness['messages']:
        generation = message['generation']
        diagnostic = generation[0]['diagnostic'] if generation else ''
        if 'unsupported integer constraint' in diagnostic:
            targets.append((message, 'integer-generation'))
        elif 'missing or unsupported physical graph reference' in diagnostic:
            targets.append((message, 'physical-reference-generation'))
    assert len(targets) == 24
    modules = []
    for entry in readiness['source_authority']['modules']:
        path = args.asn1_root / entry['path']
        actual = digest(path)
        if actual != entry['sha256']:
            raise SystemExit(f'frozen source hash mismatch: {path}')
        modules.append(dict(entry, hash_verified=True))
    with tempfile.TemporaryDirectory(prefix='n18-inventory-') as work:
        names = Path(work) / 'messages.txt'
        names.write_text(''.join(message['message']+'\n' for message, _ in targets))
        command = [str(args.probe.resolve()),
                   str(repo/'tools/qualification/ngap-rel18.modules'),
                   str(args.asn1_root.resolve()), str(names)]
        result = subprocess.run(command, capture_output=True, text=True, check=True)
        raw = json.loads(result.stdout)
    assert raw['parser_deleted_before_generation']
    schema = args.asn1_root / 'ng/asn1/NGAP-IEs.asn'
    lines = schema.read_text().splitlines()
    records = []
    unique = {}
    for (message, category), owned in zip(targets, raw['messages']):
        assert message['message'] == owned['message']
        assert owned['physical_extraction_rc'] == 0
        assert owned['generation'][0]['diagnostic'] == message['generation'][0]['diagnostic']
        domains = []
        for ordinal, typ in enumerate(owned['inventory']):
            nodes = [('named-type', typ['name'], typ['range'], None)]
            nodes += [('sequence-field', field['name'], field['range'], field)
                      for field in typ['fields']]
            nodes += [('choice-alternative', alt['name'], alt['range'], alt)
                      for alt in typ['alternatives']]
            for location, name, value_range, node in nodes:
                if not value_range['present'] or not (value_range['extensible'] or value_range['tail_count']):
                    continue
                identity = typ['module']+'.'+typ['name']
                if location != 'named-type':
                    identity += '.'+name
                intervals = [[value_range['lower'], value_range['upper']]] + value_range['tail']
                assert len(intervals) == value_range['tail_count'] + 1
                root_values = sum(high-low+1 for low, high in intervals)
                pattern = (rf'^\s*{re.escape(name)}\s*::=\s*INTEGER' if location == 'named-type'
                           else rf'^\s*{re.escape(name)}\s+INTEGER')
                declarations = [dict(line=i+1, text=text.strip()) for i, text in enumerate(lines)
                                if re.search(pattern, text)]
                assert declarations, identity
                data = dict(identity=identity, location=location, owned_type_ordinal=ordinal,
                            root_intervals=intervals, is_extensible=bool(value_range['extensible']),
                            interval_tail_count=value_range['tail_count'],
                            shape='finite-contiguous' if len(intervals)==1 else 'finite-discontinuous',
                            root_permitted_value_count=root_values,
                            source_declarations=declarations,
                            owned_evidence='present-after-parser-deletion',
                            separately_represented_extension_additions=False)
                if node is not None and location == 'sequence-field':
                    data['presence'] = node['presence']
                    data['type_semantics'] = node['semantics']
                domains.append(data)
                unique[identity] = {k:v for k,v in data.items() if k!='owned_type_ordinal'}
        assert domains
        records.append(dict(message=message['message'], role=message['role'],
                            procedure_code=message['procedure_code'], first_failure_cluster=category,
                            first_failure_diagnostic=message['generation'][0]['diagnostic'],
                            candidate_domains_in_owned_order=domains,
                            first_extensible_named_domain_in_owned_order=next(
                                (d['identity'] for d in domains if d['location']=='named-type'), None),
                            unlock_prediction=None))
    counts = {}
    for domain in unique.values():
        key=domain['location']+'/'+domain['shape']
        counts[key]=counts.get(key,0)+1
    report = dict(scope='N18 contract study only; exact N17 INTEGER and reference first-failure inventory',
                  baseline_commit='0bda141dea90f9df8baba0d6ecc64598c2330f00',
                  source_authority=dict(readiness['source_authority'], modules=modules),
                  input_sha256={str(path.relative_to(repo)):digest(path) for path in [
                      baseline, Path(__file__).resolve(), repo/'tools/n18-integer-study/probe.c',
                      repo/'tools/qualification/ngap-rel18.modules',
                      repo/'libasn1typed/asn1typed.h',repo/'libasn1typed/asn1typed.c',
                      repo/'libasn1typed/asn1typed_extract.c',
                      repo/'libasn1typed/asn1typed_render_cpp_integer.c']},
                  extraction=dict(parse=raw['parse'],fix=raw['fix'],parser_deleted_before_inventory=True),
                  counts=dict(integer_first_failures=20,reference_first_failures=4,
                              unique_domains=len(unique),by_location_and_shape=counts),
                  unique_domains=[unique[key] for key in sorted(unique)],messages=records,
                  limitations=[
                    'This is the selected 24 N17 first-failure clusters, not every INTEGER in all 131 messages.',
                    'Candidate order is Owned IR inventory order, not a proof of the internal lowering order.',
                    'Other later schema blockers remain masked; no message unlock count is predicted.',
                    'tail intervals are additional ROOT permitted intervals, not extension additions.',
                    'is_extensible records a marker; IR has no separately owned extension-addition INTEGER domain.',
                    'Explicit INTEGER extension additions are rejected: extractor accepts only root plus empty marker, not a separately specified addition.',
                    'Named serial use-site INTEGER intersections accept only non-extensible single finite interval constituents; extensible or tail refinements refuse.',
                    'No runtime or generator implementation, new strict compilation readiness, or wire qualification.'])
    args.output.parent.mkdir(parents=True,exist_ok=True)
    args.output.write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(report['counts'],sort_keys=True))


if __name__ == '__main__':
    main()
