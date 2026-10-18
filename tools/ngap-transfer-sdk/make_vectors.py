#!/usr/bin/env python3
"""Generate independent expected typed values and wire inputs for a new SDK."""
import argparse
import importlib.util
import json
from pathlib import Path
import sys


def load(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    sys.modules[name] = module
    spec.loader.exec_module(module)
    return module


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for key in ('repo', 'asn1-root', 'prefix', 'output'):
        parser.add_argument('--'+key, type=Path, required=True)
    args = parser.parse_args()
    repo, prefix = args.repo.resolve(), args.prefix.resolve()
    sys.path.insert(0, str(repo/'tools/m6003-transfer-qualification'))
    sys.path.insert(0, str(Path(__file__).resolve().parent))
    from cases import build_cases
    sys.path.insert(0, str(repo/"python/ngap_sdk"))
    from root_catalog import ROOTS, cpp_type
    from qualify import boundary_cases
    from support import write
    from python_vectors import project
    reference = load('reference', repo/'tools/n11-envelope-qualification/native_reference.py')
    bridge_module = load('semantic_bridge', repo/'tools/full-pdu-qualification/semantic_bridge.py')
    _, texts = reference.verify_sources(repo, args.asn1_root.resolve(), repo/'tools/n11-envelope-qualification/source-manifest.json')
    work = args.output.parent/'oracle'
    work.mkdir(exist_ok=False)
    native = reference.compile_native(texts, work)
    cases = build_cases()+boundary_cases(native)
    constructor = load('standalone_native_constructor', repo/'tools/full-pdu-qualification/native_values.py')
    for stem, name in ROOTS.items():
        desc = getattr(native.NGAP_IEs, name.replace('-', '_'))
        for mode in ('low', 'contrast'):
            value = constructor.Builder(mode).value(desc, name)
            cases.append(dict(root=stem, name=name+'-'+mode, value=value, encode=True))
    if {case['root'] for case in cases} != set(ROOTS):
        raise ValueError('Missing selected standalone-root oracle cases')
    provenance = json.loads((prefix/'share/nrforge-ngap/sdk-provenance.json').read_text())
    bridges = {}
    drivers = args.output.parent/'ngap-consumers'
    drivers.mkdir(exist_ok=False)
    campaign = load('standalone_campaign', repo/'tools/m6003-transfer-qualification/qualify.py')
    grouped = {}
    result = []
    for case in cases:
        stem = case['root']
        row = provenance['transfer_roots'][stem]
        if stem not in bridges:
            directory = prefix/'include/nrforge/ngap/transfers'/stem
            bridges[stem] = bridge_module.Bridge(*(directory/f'root_{family}.hpp' for family in ('types', 'mapping', 'codec')))
        desc = getattr(native.NGAP_IEs, ROOTS[stem])
        desc.set_val(case['value'])
        wire = desc.to_aper()
        grouped.setdefault(stem, []).append(dict(case, wire=wire, cpp_type=cpp_type(row['message'])))
        result.append(dict(root=row['message'], name=case['name'], wire=wire,
                           body=project(bridges[stem], desc, case['value'], row['cpp_type']), encode=case['encode']))
    cmake = ['cmake_minimum_required(VERSION 3.20)', 'project(StandaloneTransferConsumers LANGUAGES CXX)',
             'find_package(NRForgeNGAP 0.3.0 EXACT CONFIG REQUIRED)', 'enable_testing()']
    for stem, root_cases in grouped.items():
        desc = getattr(native.NGAP_IEs, ROOTS[stem].replace('-', '_'))
        bridge = bridges[stem]
        text = campaign.cpp_driver(bridge, desc, root_cases)
        for header in ('runtime.hpp', 'sequence_extensions.hpp'):
            text = text.replace('#include "'+header+'"', '#include <nrforge/ngap/'+header+'>')
        for family in ('types', 'mapping', 'codec'):
            text = text.replace('#include "root_'+family+'.hpp"', '#include <nrforge/ngap/transfers/'+stem+'/root_'+family+'.hpp>')
        (drivers/(stem+'.cpp')).write_text(text)
        cmake += ['add_executable('+stem+' '+stem+'.cpp)', 'target_link_libraries('+stem+' PRIVATE NRForge::ngap)',
                  'add_test(NAME '+stem+' COMMAND '+stem+')']
    (drivers/'CMakeLists.txt').write_text('\n'.join(cmake)+'\n')
    write(args.output, result)
    print('Generated independent installed binding vectors:', len(result))


if __name__ == '__main__':
    main()
