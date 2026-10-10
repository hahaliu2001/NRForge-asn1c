"""Actual nonextensible wide BIT SIZE native aligned-PER differential."""
import argparse, hashlib, importlib.metadata, json, shlex, subprocess, tempfile
from pathlib import Path
import asn1tools
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--repo',type=Path,default=Path(__file__).resolve().parents[2]);p.add_argument('--output',type=Path,required=True);p.add_argument('--cxx',default='c++');a=p.parse_args();repo=a.repo.resolve();source=Path(__file__).resolve().parent
assert importlib.metadata.version('asn1tools')=='0.167.0'
schema='Wide DEFINITIONS ::= BEGIN\n'
for prefix in range(8):
 for lower in (0,1):
  schema+=f'P{prefix}L{lower} ::= SEQUENCE {{'+','.join([f'p{i} BOOLEAN' for i in range(prefix)]+[f'value BIT STRING(SIZE({lower}..131072))'])+'}\n'
schema+='END';oracle=asn1tools.compile_string(schema,'per');cases=[]
for prefix in range(8):
 for lower in (0,1):
  for n in (0,1,127,128,16383,16384,65535,65536,131071,131072):
   if n<lower:continue
   payload=bytearray((0xa5^(i%13)) for i in range((n+7)//8))
   if n%8:payload[-1]&=0xff<<(8-n%8)
   payload=bytes(payload);value={**{f'p{i}':i%2==0 for i in range(prefix)},'value':(payload,n)};typ=f'P{prefix}L{lower}';wire=oracle.encode(typ,value);bits=[i%2==0 for i in range(prefix)]
   while len(bits)%8:bits.append(False)
   def number(v,w):bits.extend(bool(v&(1<<i)) for i in reversed(range(w)))
   def content(start,count):bits.extend(bool(payload[i//8]&(0x80>>(i%8))) for i in range(start,start+count))
   done=0
   while n-done>=16384:
    chunks=min(4,(n-done)//16384);number(192+chunks,8);content(done,chunks*16384);done+=chunks*16384
   tail=n-done;number(tail|(0x8000 if tail>=128 else 0),8 if tail<128 else 16);content(done,tail)
   while len(bits)%8:bits.append(False)
   model=bytes(sum(int(bits[j+i])<<(7-i) for i in range(8)) for j in range(0,len(bits),8))
   assert wire==model,(prefix,lower,n,'native/model mismatch');assert oracle.decode(typ,wire)==value
   cases.append((prefix,lower,n,payload,wire))
with tempfile.TemporaryDirectory(prefix='widebit-native-') as tmp:
 exe=Path(tmp)/'driver';subprocess.run(shlex.split(a.cxx)+['-std=c++20','-O2','-Wall','-Wextra','-Werror','-pedantic-errors','-I'+str(repo/'libaper'),str(source/'driver.cpp'),str(repo/'libaper/runtime.cpp'),'-o',str(exe)],check=True);exe.chmod(0o755)
 requests=[];expected=[]
 for prefix,lower,n,payload,wire in cases:
  requests.extend([f'E {lower} 131072 0 {prefix} {n} {payload.hex() or "-"}',f'D {lower} 131072 0 {prefix} {n} {wire.hex()}']);expected.extend([wire.hex(),f'{n}:{payload.hex() or "-"}'])
 run=subprocess.run([str(exe)],input='\n'.join(requests)+'\n',text=True,capture_output=True,check=True);actual=run.stdout.splitlines();assert actual==expected,'runtime/native mismatch'
report={'status':'PASS','native':'asn1tools==0.167.0 aligned PER','actual_schema':'BIT STRING(SIZE(0..131072)) and BIT STRING(SIZE(1..131072)), wrapped by 0..7 BOOLEAN prefix fields','vectors':len(cases),'encode_decode_comparisons':len(expected),'lengths':[0,1,127,128,16383,16384,65535,65536,131071,131072],'fragment_length_units':'bits','extension_surrogate':False,'scope':'bounded primitive differential, not full NGAP PDU qualification','sources':{str(f.relative_to(repo)):hashlib.sha256(f.read_bytes()).hexdigest() for f in (repo/'libaper/runtime.hpp',repo/'libaper/runtime.cpp',source/'driver.cpp',source/'qualify.py')}}
a.output.parent.mkdir(parents=True,exist_ok=True);a.output.write_text(json.dumps(report,indent=2)+'\n');print(f'PASS {len(cases)} actual native fragmented BIT vectors / {len(expected)} comparisons')
