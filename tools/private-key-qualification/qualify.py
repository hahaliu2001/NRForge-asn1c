#!/usr/bin/env python3
"""Compare generated physical-private BODY codecs to a native framing model.

The reference replaces an unresolved open type with OCTET STRING containing
its complete encoded bytes; this qualifies length/content framing, not the
schema or semantics of unknown vendor values, and not a full NGAP-PDU.
"""
import argparse, hashlib, json, pathlib, subprocess
import asn1tools
from pycrate_asn1rt.asnobj_basic import OID
SCHEMA = '''X DEFINITIONS AUTOMATIC TAGS ::= BEGIN
Key ::= CHOICE { local INTEGER (0..65535), global OBJECT IDENTIFIER }
Policy ::= ENUMERATED {reject, ignore, notify}
Entry ::= SEQUENCE {id Key, criticality Policy, value OCTET STRING}
Container ::= SEQUENCE (SIZE(1..65535)) OF Entry
Message ::= SEQUENCE {entries Container,...}
END'''
p=argparse.ArgumentParser();p.add_argument('generated_binary');p.add_argument('--output',required=True);a=p.parse_args()
codec=asn1tools.compile_string(SCHEMA,'per');lines=subprocess.check_output([a.generated_binary,'vectors'],text=True).splitlines()
keys=[('local',n) for n in (0,1,255,65535)]+[('global',s) for s in ('0.0','1.2.840.113549','2.999.3','2.184467440737095516160.1')]
checks=[]
for line in lines:
 k,c,n,h=line.split();k,c,n=int(k),int(c),int(n);payload=bytes(j%256 for j in range(n));v={'entries':[{'id':keys[k],'criticality':('reject','ignore','notify')[c],'value':payload}]}
 actual=bytes.fromhex(h);expected=codec.encode('Message',v)
 assert actual==expected,(k,c,n,actual.hex(),expected.hex())
 decoded=codec.decode('Message',actual)
 if k<4: assert decoded==v
 else:
  # asn1tools 0.167.0 decodes combined first subidentifier by unbounded
  # division by 40, so e.g. 2.999.3 becomes 26.39.3. Do not accept that
  # semantic result: use independent pycrate OID decode for the actual bytes.
  assert decoded['entries'][0]['criticality']==v['entries'][0]['criticality']
  assert decoded['entries'][0]['value']==payload
  oid=OID(name='O');oid.from_aper(actual[4:5+actual[4]])
  assert oid.get_val()==tuple(int(arc) for arc in keys[k][1].split('.'))
 checks.append({'key':k,'criticality':c,'payload_octets':n,'encoding_sha256':hashlib.sha256(actual).hexdigest(),'octets':len(actual),'result':'PASS'})
assert len(checks)==240
repo=pathlib.Path(__file__).resolve().parents[2]
source_paths=['libaper/runtime.hpp','libaper/runtime.cpp','libasn1typed/asn1typed.h','libasn1typed/asn1typed.c','libasn1typed/asn1typed_extract.c','libasn1typed/asn1typed_render_cpp_compound.c','libasn1typed/asn1typed_render_cpp_ioc.c','libasn1typed/check_asn1typed_private_render.c','libasn1typed/check_asn1typed_private_generated.cpp','libasn1typed/fixtures/private-generation-batch.asn1','tools/private-key-qualification/qualify.py']
source_hashes={p:hashlib.sha256((repo/p).read_bytes()).hexdigest() for p in source_paths}
pathlib.Path(a.output).write_text(json.dumps({'source_sha256':source_hashes,'schema':SCHEMA,'reference':'asn1tools 0.167.0 APER native surrogate framing + pycrate 0.7.11 OID decode','reference_limitation':'asn1tools incorrect global first-arc decoding for combined subidentifier >=120 is explicitly replaced by independent pycrate OID decoding; native encoding bytes agree','pycrate_global_oid_checks':120,'scope':'physical private BODY local/global selector, raw unknown encoded values; no full NGAP-PDU qualification','checks':checks,'total':240,'pass':240,'fail':0},indent=2)+'\n')
print('PASS 240 generated private BODY / native APER model encodings and native decoded field values')
