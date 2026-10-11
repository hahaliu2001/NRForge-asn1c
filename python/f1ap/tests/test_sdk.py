"""Installed F1AP wheel: finite inherited bytes and Python boundary checks."""
import copy
from concurrent.futures import ThreadPoolExecutor
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import unittest
import nrforge_f1ap as f1ap

HERE = Path(__file__).resolve().parent
GOLDEN = json.loads((HERE/'golden.json').read_text())['cases']
SETUP = json.loads((HERE/'f1-setup-native.json').read_text())['cases']
ROLES = ('initiatingMessage','successfulOutcome','unsuccessfulOutcome')

def setup_wire(name='F1SetupRequest'):
    c=next(c for c in SETUP if c['message']==name)
    return bytes.fromhex(c['pdu_hex'])

class SDKTests(unittest.TestCase):
    def request(self): return f1ap.decode(setup_wire())['body']

    def test_identity_registry_schema(self):
        i=f1ap.identity()
        self.assertEqual(i['version'],'0.1.0')
        for key in ('fingerprint','schema_sha256','runtime_sha256'):
            self.assertRegex(i[key],r'^[0-9a-f]{64}$')
        rows=f1ap.messages()
        self.assertEqual(len(rows),158)
        self.assertEqual({r['message'] for r in rows},{c['message'] for c in GOLDEN})
        self.assertEqual(len({r['procedure_code'] for r in rows}),94)
        for r in rows: self.assertIn('declarations',f1ap.schema(r['message']))

    def test_all_historical_finite_vectors(self):
        self.assertEqual(len(GOLDEN),4728)
        for c in GOLDEN:
            with self.subTest(message=c['message'],case=c['id']):
                wire=bytes.fromhex(c['wire_hex'])
                self.assertEqual(hashlib.sha256(wire).hexdigest(),c['wire_sha256'])
                d=f1ap.decode(wire)
                self.assertEqual((d['kind'],d['message'],d['role'],d['procedure_code'],d['criticality']),
                    ('typed',c['message'],ROLES[c['role']],c['code'],c['criticality']))
                self.assertEqual(f1ap.encode(c['message'],d['body'],criticality=c['criticality']),wire)

    def test_independent_setup_fields_and_construction(self):
        for c in SETUP:
            d=f1ap.decode(bytes.fromhex(c['pdu_hex']))
            entries=d['body']['protocol_i_es']['elements']
            self.assertEqual(entries[0]['id'],78)
            self.assertEqual(entries[0]['value']['value']['value'],0)
            if c['message']=='F1SetupRequest':
                self.assertEqual([e['id'] for e in entries],[78,42,171])
                self.assertEqual(entries[1]['value']['value']['value'],0)
                version=entries[2]['value']['value']['value']
                self.assertEqual(version['latest_rrc_version'],{'octets':b'\0','bit_count':3})
                self.assertIsNone(version['i_e_extensions'])
            elif c['message']=='F1SetupResponse':
                self.assertEqual([e['id'] for e in entries],[78,170])
                self.assertEqual(entries[1]['value']['value']['value']['latest_rrc_version'],{'octets':b'\0','bit_count':3})
            else:
                self.assertEqual([e['id'] for e in entries],[78,0])
                cause=entries[1]['value']['value']['value']
                self.assertEqual(cause['type'],'F1ApIEsCause_radio_network')
                self.assertEqual(cause['value']['value'],'unspecified')
            self.assertEqual(f1ap.encode(c['message'],d['body'],criticality=c['criticality']),bytes.fromhex(c['pdu_hex']))
        # Construct independently of decode using generated public type labels.
        entries=[]
        for ident,label,value in ((78,'transaction_id',0),(42,'g_nb_du_id',0),
              (171,'gnb_du_rrc_version',{'latest_rrc_version':{'octets':b'\0','bit_count':3},'i_e_extensions':None})):
            entries.append({'id':ident,'criticality':'reject','value':{
                'type':'F1ApContainersProtocolIeFieldF1ApPduContentsF1SetupRequestIEs_'+label,'value':{'value':value}}})
        body={'protocol_i_es':{'elements':entries},'sequence_extensions':{'received_bitmap_bit_count':0,'unknown_additions':[]}}
        self.assertEqual(f1ap.encode('F1SetupRequest',body),setup_wire())

    def test_exact_shapes_and_ranges(self):
        for bad in (True,-1,1<<100):
            b=self.request();b['protocol_i_es']['elements'][0]['id']=bad
            with self.assertRaises((TypeError,ValueError,OverflowError)): f1ap.encode('F1SetupRequest',b)
        for modify in (lambda b:b.update(invented=1),lambda b:b.pop('protocol_i_es'),
                       lambda b:b['protocol_i_es']['elements'][0]['value'].update(type='wrong_wrapper')):
            b=self.request();modify(b)
            with self.assertRaises((TypeError,ValueError)):f1ap.encode('F1SetupRequest',b)
        b=self.request();b['protocol_i_es']['elements'][2]['value']['value']['value']['latest_rrc_version']['bit_count']=True
        with self.assertRaises((TypeError,ValueError)):f1ap.encode('F1SetupRequest',b)
        class CustomDict(dict): pass
        class CustomList(list): pass
        with self.assertRaises((TypeError,ValueError)):f1ap.encode('F1SetupRequest',CustomDict(self.request()))
        b=self.request();b['protocol_i_es']['elements']=CustomList(b['protocol_i_es']['elements'])
        with self.assertRaises((TypeError,ValueError)):f1ap.encode('F1SetupRequest',b)
        with self.assertRaises((TypeError,ValueError)):f1ap.encode('N'*257,{})
        with self.assertRaises((TypeError,ValueError)):f1ap.encode('NGSetupRequest',{})

    def test_unknown_receive_only_and_optional(self):
        # Independent F1AP IE framing: retain unknown id 65535/ignore/payload 00ff80.
        wire=setup_wire();wire=wire[:3]+bytes([wire[3]+7])+wire[4:6]+b'\4'+wire[7:]+bytes.fromhex('ffff400300ff80')
        d=f1ap.decode(wire); unknown=d['body']['protocol_i_es']['elements'][-1]
        self.assertEqual(unknown['id'],65535)
        self.assertEqual(unknown['criticality'],'ignore')
        self.assertEqual(unknown['value']['value']['payload'],b'\0\xff\x80')
        with self.assertRaises(f1ap.CodecError) as raised:f1ap.encode('F1SetupRequest',d['body'])
        self.assertEqual(raised.exception.code,'constraint_violation')
        b=self.request();del b['protocol_i_es']['elements'][2]['value']['value']['value']['i_e_extensions']
        self.assertEqual(f1ap.encode('F1SetupRequest',b),setup_wire())
        # Body extension bitmap length=3, bit 2 present, open payload 00ff.
        wire=setup_wire();wire=wire[:3]+bytes([wire[3]+5])+b'\x80'+wire[5:]+bytes.fromhex('04400200ff')
        d=f1ap.decode(wire)
        self.assertEqual(d['body']['sequence_extensions'],{'received_bitmap_bit_count':3,
            'unknown_additions':[{'addition_index':2,'payload_octets':b'\0\xff'}]})
        with self.assertRaises(f1ap.CodecError):f1ap.encode('F1SetupRequest',d['body'])

    def test_fourth_root_and_opaque(self):
        with self.assertRaises(f1ap.CodecError) as raised:f1ap.decode(b'\xc0')
        self.assertEqual((raised.exception.code,raised.exception.bit_offset),('constraint_violation',2))
        d=f1ap.decode(bytes.fromhex('80fa4001a5'))
        self.assertEqual((d['kind'],d['role'],d['procedure_code'],d['payload']),('opaque_root','unsuccessfulOutcome',250,b'\xa5'))

    def test_errors_and_limits(self):
        wire=setup_wire()
        for data,code,offset in ((b'','truncated_input',0),(wire+b'\0','trailing_data',len(wire)*8)):
            with self.assertRaises(f1ap.CodecError) as raised:f1ap.decode(data)
            self.assertEqual((raised.exception.code,raised.exception.bit_offset),(code,offset))
        with self.assertRaises(f1ap.CodecError):f1ap.decode(wire[:-1])
        with self.assertRaises(f1ap.CodecError):f1ap.decode(wire,limits={'max_input_octets':len(wire)-1})
        for limits in ({'max_output_octets':len(wire)-1},{'max_wire_bits':len(wire)*8-1}):
            with self.assertRaises(f1ap.CodecError):f1ap.encode('F1SetupRequest',self.request(),limits=limits)
        for key in ('max_depth','max_nodes','max_bytes','max_collection_elements'):
            with self.assertRaises((ValueError,f1ap.CodecError)):f1ap.decode(wire,conversion_limits={key:0})
        for bad in ({'max_input_octets':-1},{'max_wire_bits':True},{'invented':1}):
            with self.assertRaises((TypeError,ValueError,OverflowError)):f1ap.decode(wire,limits=bad)

    def test_concurrent_calls(self):
        def worker(c):
            w=bytes.fromhex(c['wire_hex'])
            for _ in range(8):
                d=f1ap.decode(w)
                self.assertEqual(f1ap.encode(c['message'],d['body'],criticality=c['criticality']),w)
                with self.assertRaises(f1ap.CodecError):f1ap.decode(b'')
            return d['message']
        selected=GOLDEN[::max(1,len(GOLDEN)//8)]
        with ThreadPoolExecutor(max_workers=4) as pool:
            self.assertEqual(list(pool.map(worker,selected)),[c['message'] for c in selected])

    def test_coinstalled_protocols_both_import_orders(self):
        for order in ('nrforge_f1ap, nrforge_ngap','nrforge_ngap, nrforge_f1ap'):
            script=f'''import {order}
f,n=nrforge_f1ap,nrforge_ngap
assert len(f.messages())==158 and len(n.messages())==131
assert f.identity()['schema_sha256']!=n.identity()['schema_sha256']
assert f.CodecError is not n.CodecError
for p in (f,n):
 try:p.decode(b'')
 except p.CodecError:pass
 else:raise AssertionError('missing package-specific error')
w=bytes.fromhex('{setup_wire().hex()}');d=f.decode(w);assert f.encode(d['message'],d['body'])==w
'''
            subprocess.run([sys.executable,'-I','-c',script],check=True,cwd='/tmp')

if __name__=='__main__':unittest.main()
