#!/usr/bin/env python3
"""Independent finite oracle cases for all selected installed RRC APIs."""
import argparse
import importlib.util
import json
from pathlib import Path
import sys

REPO = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO/'tools/rrc-wire-expanded'))
import qualify
from native_values import pack, restore, build


def main():
    p=argparse.ArgumentParser(description=__doc__)
    for key in ('schema-root', 'prefix', 'output'):
        p.add_argument('--'+key, type=Path, required=True)
    a=p.parse_args()
    a.output.mkdir(parents=True, exist_ok=False)
    authority=json.loads((REPO/'tools/rrc-readiness/source-manifest.json').read_text())
    qualify.authenticate(a.schema_root, authority)
    native=qualify.native_compile([(a.schema_root/m['path']).read_text() for m in authority['modules']], a.output/'oracle.py')
    manifest=json.loads((a.prefix/'share/nrforge-rrc/manifest.json').read_text())
    rows=manifest['roots']
    expected=json.loads((REPO/'tools/rrc-sdk-expanded/verification.json').read_text())['manifest']['roots']
    additions=json.loads((REPO/'tools/unified-sdk/scope.json').read_text())['rrc_additional_roots']
    if len(rows)!=91 or {(r['module'],r['root']) for r in rows}!={(r['module'],r['root']) for r in expected+additions}:
        raise ValueError('Incomplete selected RRC installed manifest')
    spec=importlib.util.spec_from_file_location('public_consumer_emitter', REPO/'tools/rrc-sdk-expanded-consumer/prepare.py')
    emitter=importlib.util.module_from_spec(spec);spec.loader.exec_module(emitter)
    records,audits=qualify.corpus_real(native, rows)
    if len(records)!=91:
        raise ValueError('Partial RRC finite oracle corpus')
    # Exercise an explicit CellGroupConfig -> RRCSetup.masterCellGroup composition.
    cg=qualify.root(native,'NR-RRC-Definitions','CellGroupConfig')
    inner_wire,inner_bits,inner_value=qualify.oracle(cg, build(cg))
    setup=qualify.root(native,'NR-RRC-Definitions','RRCSetup')
    outer_value=build(setup)
    def inject(obj, value):
        if obj.TYPE=='SEQUENCE':
            for name in value:
                if name=='masterCellGroup':
                    value[name]=inner_wire
                    return True
                if inject(obj._cont[name],value[name]): return True
        if obj.TYPE=='CHOICE': return inject(obj._cont[value[0]],value[1])
        return False
    if not inject(setup,outer_value): raise ValueError('RRCSetup masterCellGroup path not found')
    outer_wire,outer_bits,normalized=qualify.oracle(setup,outer_value)
    setup_record=next(r for r in records if r['root']=='RRCSetup')
    setup_record['cases'].append(dict(id='CellGroupConfig-carrier-composition',value=pack(normalized),normalized=pack(normalized),wire_hex=outer_wire.hex(),semantic_bits=outer_bits,offset=0))
    source=a.output/'consumers';source.mkdir()
    cmake=['cmake_minimum_required(VERSION 3.20)', 'project(UnifiedRRCConsumers LANGUAGES CXX)',
           'find_package(NRForgeRRC 0.3.0 EXACT CONFIG REQUIRED)', 'enable_testing()']
    for record,entry in zip(records,rows):
        if (record['module'],record['root'])!=(entry['module'],entry['root']): raise ValueError('RRC order drift')
        obj=qualify.root(native,entry['module'],entry['root'])
        qualify.root_for_emit[entry['module'],entry['root']]=obj
        name='root'+str(entry['index']).zfill(3)
        header=a.prefix/'include'/entry['header']
        (source/(name+'.cpp')).write_text(emitter.public_emit(record, header, entry))
        cmake+=['add_executable('+name+' '+name+'.cpp)', 'target_link_libraries('+name+' PRIVATE NRForge::rrc)', 'add_test(NAME '+name+' COMMAND '+name+')']
    (source/'CMakeLists.txt').write_text('\n'.join(cmake)+'\n')
    (a.output/'cases.json').write_text(json.dumps(dict(status='PREPARED_NOT_EXECUTED',roots=records,audits=audits,composition=dict(inner_hex=inner_wire.hex(),inner_bits=inner_bits,outer_hex=outer_wire.hex(),outer_bits=outer_bits)),indent=2)+'\n')


if __name__=='__main__':main()
