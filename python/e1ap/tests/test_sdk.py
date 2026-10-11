"""Installed E1AP SDK: historical finite vectors and Python API boundaries."""
from concurrent.futures import ThreadPoolExecutor
import hashlib
import itertools
import json
from pathlib import Path
import subprocess
import sys
import unittest
import nrforge_e1ap as sdk
HERE=Path(__file__).resolve().parent
DATA=json.loads((HERE/'golden.json').read_text())
GOLDEN=DATA['cases']
SETUP=json.loads((HERE/'e1-setup-native.json').read_text())['cases']
ROLES=('initiatingMessage','successfulOutcome','unsuccessfulOutcome')
NAME='GNB-CU-UP-E1SetupRequest'
def wire():return bytes.fromhex(next(c['pdu_hex'] for c in SETUP if c['message']==NAME))
def walk(value):
    yield value
    if type(value) is dict:
        for v in value.values():yield from walk(v)
    elif type(value) is list:
        for v in value:yield from walk(v)
class SDKTests(unittest.TestCase):
    def request(self):return sdk.decode(wire())['body']
    def test_identity_registry_schema(self):
        self.assertEqual(sdk.identity()['version'],'0.1.0')
        for k in ('fingerprint','schema_sha256','runtime_sha256'):self.assertRegex(sdk.identity()[k],r'^[0-9a-f]{64}$')
        rows=sdk.messages();self.assertEqual(len(rows),72)
        self.assertEqual(len({r['procedure_code'] for r in rows}),40)
        self.assertEqual({r['message'] for r in rows},{r['message'] for r in GOLDEN})
        for row in rows:self.assertIn('declarations',sdk.schema(row['message']))
    def test_historical_vectors_and_uint64(self):
        self.assertEqual(len(GOLDEN),1341)
        seen=set()
        for c in GOLDEN:
            with self.subTest(message=c['message'],case=c['id']):
                w=bytes.fromhex(c['wire_hex']);self.assertEqual(hashlib.sha256(w).hexdigest(),c['wire_sha256'])
                d=sdk.decode(w)
                self.assertEqual((d['kind'],d['message'],d['role'],d['procedure_code'],d['criticality']),('typed',c['message'],ROLES[c['role']],c['code'],c['criticality']))
                self.assertEqual(sdk.encode(c['message'],d['body'],criticality=c['criticality']),w)
                seen.update(v for v in walk(d['body']) if type(v) is int and v >= (1<<63))
        self.assertIn(1<<63,seen);self.assertIn((1<<64)-1,seen)
    def test_uint64_field_conversion_boundaries(self):
        def counter_paths(value,path=()):
            if type(value) is dict:
                for k,v in value.items():
                    if k in ('usage_count_ul','usage_count_dl') and type(v) is int and v==(1<<64)-1:yield path+(k,)
                    else:yield from counter_paths(v,path+(k,))
            elif type(value) is list:
                for k,v in enumerate(value):yield from counter_paths(v,path+(k,))
        candidates={}
        for c in GOLDEN:
            body=sdk.decode(bytes.fromhex(c['wire_hex']))['body']
            for path in counter_paths(body):candidates.setdefault((c['message'],path),c)
        self.assertEqual(len(candidates),6)
        for (message,path),candidate in candidates.items():
            def parent(body):
                for key in path[:-1]:body=body[key]
                return body
            for value in (0,1,(1<<63)-1,1<<63,(1<<64)-1,True,-1,1<<64):
                with self.subTest(message=message,path=path,value=value):
                    body=sdk.decode(bytes.fromhex(candidate['wire_hex']))['body'];parent(body)[path[-1]]=value
                    if type(value) is bool or value<0 or value>=(1<<64):
                        with self.assertRaises((TypeError,ValueError,OverflowError)):sdk.encode(message,body)
                    else:
                        decoded=sdk.decode(sdk.encode(message,body,criticality=candidate['criticality']))
                        self.assertEqual(parent(decoded['body'])[path[-1]],value)
    def test_six_setup_and_independent_construction(self):
        self.assertEqual(len(SETUP),6)
        for c in SETUP:
            d=sdk.decode(bytes.fromhex(c['pdu_hex']))
            self.assertEqual(d['message'],c['message'])
            entries=d['body']['protocol_i_es']['elements']
            self.assertEqual(entries[0]['id'],57)
            self.assertEqual(entries[0]['value']['value']['value'],0)
            self.assertEqual(sdk.encode(c['message'],d['body']),bytes.fromhex(c['pdu_hex']))
        # Explicit generated wrapper and body; independent construction of a
        # CU-CP failure with TransactionID and Cause.
        body={'protocol_i_es':{'elements':[{'id':57,'criticality':'reject','value':{
            'type':'E1ApContainersProtocolIeFieldE1ApPduContentsGnbCuCpE1SetupFailureIEs_transaction_id','value':{'value':0}}},
            {'id':0,'criticality':'ignore','value':{'type':'E1ApContainersProtocolIeFieldE1ApPduContentsGnbCuCpE1SetupFailureIEs_cause','value':{'value':{'type':'E1ApIEsCause_radio_network','value':{'value':'unspecified'}}}}}]},
            'sequence_extensions':{'received_bitmap_bit_count':0,'unknown_additions':[]}}
        target=next(c for c in SETUP if c['message']=='GNB-CU-CP-E1SetupFailure')
        self.assertEqual(sdk.encode(target['message'],body),bytes.fromhex(target['pdu_hex']))
    def test_shapes_and_ranges(self):
        for bad in (True,-1,1<<100):
            b=self.request();b['protocol_i_es']['elements'][0]['id']=bad
            with self.assertRaises((TypeError,ValueError,OverflowError)):sdk.encode(NAME,b)
        for change in (lambda b:b.update(extra=0),lambda b:b.pop('protocol_i_es'),lambda b:b['protocol_i_es']['elements'][0]['value'].update(type='wrong')):
            b=self.request();change(b)
            with self.assertRaises((TypeError,ValueError)):sdk.encode(NAME,b)
        class D(dict):pass
        class L(list):pass
        with self.assertRaises((TypeError,ValueError)):sdk.encode(NAME,D(self.request()))
        b=self.request();b['protocol_i_es']['elements']=L(b['protocol_i_es']['elements'])
        with self.assertRaises((TypeError,ValueError)):sdk.encode(NAME,b)
        for name in ('F1SetupRequest','NGSetupRequest','N'*257):
            with self.assertRaises((TypeError,ValueError)):sdk.encode(name,{})
    def test_unknown_outer_and_opaque(self):
        for c in DATA['outer_extension_policy']:
            d=sdk.decode(bytes.fromhex(c['wire_hex']))
            self.assertEqual(d,{'kind':'unknown_extension','extension_index':c['index'],'payload':bytes.fromhex(c['payload_hex'])})
            if c['payload_hex']:
                with self.assertRaises(sdk.CodecError):sdk.decode(bytes.fromhex(c['wire_hex']),limits={'max_retained_unknown_payload_octets':0})
        d=sdk.decode(bytes.fromhex('40fa4001a5'))
        self.assertEqual((d['kind'],d['procedure_code'],d['payload']),('opaque_root',250,b'\xa5'))
        with self.assertRaises((TypeError,ValueError)):sdk.encode('unknown_extension',d)
    def test_unknown_ie_and_optional(self):
        w=wire();w=w[:3]+bytes([w[3]+7])+w[4:6]+bytes([w[6]+1])+w[7:]+bytes.fromhex('ffff400300ff80')
        d=sdk.decode(w);entry=d['body']['protocol_i_es']['elements'][-1]
        self.assertEqual(entry['id'],65535);self.assertEqual(entry['value']['value']['payload'],b'\0\xff\x80')
        with self.assertRaises(sdk.CodecError):sdk.encode(NAME,d['body'])
        b=self.request()
        def drop(value):
            if type(value) is dict:
                for key in list(value):
                    if value[key] is None:del value[key]
                    else:drop(value[key])
            elif type(value) is list:
                for v in value:drop(v)
        drop(b);self.assertEqual(sdk.encode(NAME,b),wire())
        w=wire();w=w[:3]+bytes([w[3]+5])+b'\x80'+w[5:]+bytes.fromhex('04400200ff')
        d=sdk.decode(w)
        self.assertEqual(d['body']['sequence_extensions'],{'received_bitmap_bit_count':3,
            'unknown_additions':[{'addition_index':2,'payload_octets':b'\0\xff'}]})
        with self.assertRaises(sdk.CodecError):sdk.encode(NAME,d['body'])
    def test_errors_budgets(self):
        w=wire()
        for data,code,offset in ((b'','truncated_input',0),(w+b'\0','trailing_data',len(w)*8)):
            with self.assertRaises(sdk.CodecError) as e:sdk.decode(data)
            self.assertEqual((e.exception.code,e.exception.bit_offset),(code,offset))
        with self.assertRaises(sdk.CodecError):sdk.decode(w,limits={'max_input_octets':len(w)-1})
        for l in ({'max_output_octets':len(w)-1},{'max_wire_bits':len(w)*8-1}):
            with self.assertRaises(sdk.CodecError):sdk.encode(NAME,self.request(),limits=l)
        for k in ('max_depth','max_nodes','max_bytes','max_collection_elements'):
            with self.assertRaises((ValueError,sdk.CodecError)):sdk.decode(w,conversion_limits={k:0})
        for l in ({'max_input_octets':-1},{'max_wire_bits':True},{'invented':1}):
            with self.assertRaises((TypeError,ValueError,OverflowError)):sdk.decode(w,limits=l)
    def test_concurrency(self):
        def worker(c):
            w=bytes.fromhex(c['wire_hex'])
            for _ in range(4):
                d=sdk.decode(w);self.assertEqual(sdk.encode(c['message'],d['body'],criticality=c['criticality']),w)
                with self.assertRaises(sdk.CodecError):sdk.decode(b'')
        with ThreadPoolExecutor(max_workers=4) as p:list(p.map(worker,GOLDEN[::167]))
    def test_three_protocol_import_orders(self):
        for order in itertools.permutations(('nrforge_e1ap','nrforge_f1ap','nrforge_ngap')):
            script='import '+', '.join(order)+'''\ne,f,n=nrforge_e1ap,nrforge_f1ap,nrforge_ngap
assert [len(p.messages()) for p in (e,f,n)]==[72,158,131]
assert len({p.identity()['schema_sha256'] for p in (e,f,n)})==3
assert len({p.CodecError for p in (e,f,n)})==3
for p in (e,f,n):
 try:p.decode(b'')
 except p.CodecError:pass
 else:raise AssertionError('missing error')
'''+f"w=bytes.fromhex('{wire().hex()}');d=e.decode(w);assert e.encode(d['message'],d['body'])==w\n"
            subprocess.run([sys.executable,'-I','-c',script],check=True,cwd='/tmp')
if __name__=='__main__':unittest.main()
