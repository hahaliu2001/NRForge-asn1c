#!/usr/bin/env python3
"""Independent frozen oracle witnesses for the F1AP fourth-root native tests."""
import argparse
import importlib.util
import json
from pathlib import Path
import sys

REPO=Path(__file__).resolve().parents[2]
spec=importlib.util.spec_from_file_location('f1_reference',REPO/'tools/f1ap-wire-qualification/native_reference.py')
reference=importlib.util.module_from_spec(spec);spec.loader.exec_module(reference)


def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--schema-root',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
    a.output.mkdir(parents=True,exist_ok=False)
    _,texts=reference.verify_sources(REPO,a.schema_root)
    native=reference.compile_native(texts,a.output)
    obj=native.F1AP_PDU_Descriptions.F1AP_PDU
    if list(obj._root)!=['initiatingMessage','successfulOutcome','unsuccessfulOutcome','choice-extension'] or obj._ext is not None:raise ValueError('F1AP root order changed')
    records=[]
    for identifier,criticality,payload,expected in [(0,'reject',b'\x00','c00000000100'),(65535,'ignore',b'\x55','c0ffff400155'),(7,'notify',b'\x80','c00007800180')]:
        value=('choice-extension',dict(id=identifier,criticality=criticality,value=('_unk_004',payload)))
        obj.set_val(value);wire=obj.to_aper()
        if wire.hex()!=expected:raise ValueError('Independent F1AP fourth-root wire witness mismatch')
        obj.from_aper(wire)
        got=obj.get_val()
        if got[0]!='choice-extension' or got[1]['id']!=identifier or got[1]['criticality']!=criticality or got[1]['value'][1]!=payload:raise ValueError('Independent F1AP semantics mismatch')
        records.append(dict(id=identifier,criticality=criticality,payload=payload.hex(),wire=wire.hex()))
    (a.output/'report.json').write_text(json.dumps(dict(status='PASS_FINITE_ORACLE_WITNESSES',cases=records),indent=2)+'\n')


if __name__=='__main__':main()
