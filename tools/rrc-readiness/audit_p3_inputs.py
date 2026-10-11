#!/usr/bin/env python3
"""Audit named actual/default shapes in the unchanged P1 fixed source AST.

This is source identity/shape evidence, not 338 successful instance closures.
Use a freshly linked P1 probe and scanner --check before passing its --ast file.
"""
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
from scan import HERE, authenticate, walk


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--asn1-root', type=Path, required=True)
    p.add_argument('--ast', type=Path, required=True)
    p.add_argument('--check', action='store_true')
    a = p.parse_args()
    authenticate(a.asn1_root, json.loads((HERE / 'source-manifest.json').read_text()))
    raw = a.ast.read_bytes()
    digest = hashlib.sha256(raw).hexdigest()
    if digest != '41f3524fdc7bdad93083f3a2892b6060a2492ca36418e2134ea32332061ba2f7':
        raise ValueError('P1 AST drift; run fresh P1 scanner --check')
    ast = json.loads(raw)
    modules = {m['name']: m for m in ast['modules']}
    types = {(m['name'], d['name']): d for m in ast['modules']
             for d in m['declarations'] if d['meta'] in (1, 2)}

    def resolve(module, name):
        if '.' in name:
            key = tuple(name.split('.'))
            if len(key) == 2 and key in types:
                return key
            raise ValueError('unresolved qualified type')
        if (module, name) in types:
            return module, name
        imported = [x['module'] for x in modules[module]['imports'] if x['name'] == name]
        if len(imported) != 1 or (imported[0], name) not in types:
            raise ValueError('unresolved named type')
        return imported[0], name

    sites, defaults = [], []
    for (module, owner), declaration in types.items():
        for node in walk(declaration):
            if node['actuals']:
                args = node['actuals']['members']
                if (node['reference'] != 'SetupRelease' or len(args) != 1 or
                        args[0]['kind'] != 'REFERENCE' or args[0]['constraint'] or args[0]['actuals']):
                    raise ValueError('frozen actual form outside P3 named-type contract')
                generic = resolve(module, node['reference'])
                actual = resolve(module, args[0]['reference'])
                sites.append({'module': module, 'owner': owner, 'member': node['name'],
                              'line': node['line'], 'template': list(generic), 'actual': list(actual),
                              'instance_key': '$type$' + generic[1] + '$' + actual[0] + '$' + actual[1]})
            if node['default']:
                body, body_module = node, module
                for _ in range(128):
                    if body['kind'] != 'REFERENCE':
                        break
                    body_module, name = resolve(body_module, body['reference'])
                    body = types[body_module, name]
                if body['kind'] not in ('INTEGER', 'ENUMERATED'):
                    raise ValueError('frozen DEFAULT type outside P3 scalar contract')
                defaults.append({'module': module, 'owner': owner, 'member': node['name'],
                                 'line': node['line'], 'scalar_kind': body['kind']})
    if len(sites) != 338 or len(defaults) != 20:
        raise ValueError('P1 source site inventory mismatch')
    report = {'batch': 'RRC-P3', 'baseline': 'e7b5b95ee70200cf110923c9262cd27cc1c01d08',
              'fixed_source_ast_sha256': digest, 'named_actual_sites': len(sites),
              'distinct_named_instance_keys': len({(s['template'][0], s['instance_key']) for s in sites}),
              'default_sites': len(defaults), 'default_scalar_kinds': dict(Counter(d['scalar_kind'] for d in defaults)),
              'actual_sites': sites, 'default_fields': defaults,
              'scope': 'All frozen source site shapes and explicit namespace identities; no per-site closure, codec or wire qualification'}
    output = HERE / 'input-shapes-rrc-p3.json'
    content = json.dumps(report, indent=2) + '\n'
    if a.check:
        if output.read_text() != content:
            raise ValueError('P3 input-shape audit drift')
    else:
        output.write_text(content)
    print(json.dumps({k: report[k] for k in ('named_actual_sites', 'distinct_named_instance_keys', 'default_sites', 'default_scalar_kinds')}, sort_keys=True))


if __name__ == '__main__':
    main()
