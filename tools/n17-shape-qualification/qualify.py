"""Independent qualification of actual generated inline ENUMERATED compound codecs."""
import argparse
import hashlib
import importlib.metadata
import json
from pathlib import Path
import shlex
import subprocess
import tempfile
import asn1tools

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--repo',type=Path,default=Path(__file__).resolve().parents[2])
p.add_argument('--build',type=Path)
p.add_argument('--generated',type=Path)
p.add_argument('--cxx',default='c++')
p.add_argument('--output',type=Path,required=True)
a=p.parse_args()
if importlib.metadata.version('asn1tools')!='0.167.0':raise RuntimeError('Require asn1tools==0.167.0')
repo=a.repo.resolve();build=(a.build or repo).resolve();source=Path(__file__).resolve().parent
fixture=repo/'libasn1typed/fixtures/shape-generation-n17.asn1'
paths=[source/'driver.cpp',source/'qualify.py',fixture]+[repo/f for f in [
 'libaper/runtime.hpp','libaper/runtime.cpp','libaper/sequence_extensions.hpp',
 'libasn1typed/asn1typed.h','libasn1typed/asn1typed.c','libasn1typed/asn1typed_extract.h','libasn1typed/asn1typed_extract.c',
 'libasn1typed/asn1typed_name.h','libasn1typed/asn1typed_name.c','libasn1typed/asn1typed_render_cpp.h',
 'libasn1typed/asn1typed_render_cpp_internal.h','libasn1typed/asn1typed_render_cpp_slice.c',
 'libasn1typed/asn1typed_render_cpp_compound.c','libasn1typed/asn1typed_render_cpp_enum.c',
 'libasn1typed/asn1typed_render_cpp_uint.c','libasn1typed/asn1typed_render_cpp_integer.c',
 'libasn1typed/asn1typed_render_cpp_inline_enum.c','libasn1typed/asn1typed_render_cpp_ioc_internal.h',
 'libasn1typed/asn1typed_render_cpp_ioc.c']]
def hashes():return {str(f.relative_to(repo)):hashlib.sha256(f.read_bytes()).hexdigest() for f in paths}
initial=hashes()
compiler=shlex.split(a.cxx);flags=['-std=c++20','-Wall','-Wextra','-Werror','-pedantic-errors','-Wconversion','-Wsign-conversion']
version=subprocess.run(compiler+['--version'],capture_output=True,text=True,check=True).stdout.splitlines()[0]
schema=fixture.read_text().split('END')[0]+'END\n'
oracle=asn1tools.compile_string(schema,'per')

def model(phase,status,copy,klass,amount,envelope):
    bits=[]
    def put(value,width):bits.extend(bool(value&(1<<i)) for i in range(width-1,-1,-1))
    def align():bits.extend([False]*((-len(bits))%8))
    def panel():
        put(int(status is not None),1);put(int(copy is not None),1)
        put(1,1)
        put({-2:0,5:1,9:2}[phase],2)
        if status is not None:
            if status in [0,3]:put(0,1);put({0:0,3:1}[status],1)
            else:put(1,1);put({1:0,10:1}[status],7)
        if copy is not None:put({-2:0,5:1,9:2}[copy],2)
        put(klass,1);align();put(amount,8)
    if envelope:put(1,1);put(0,1)
    panel()
    if envelope:put(1,2);panel()
    align()
    return bytes(sum(int(bit)<<(7-i) for i,bit in enumerate(bits[j:j+8])) for j in range(0,len(bits),8))

cases=[]
phase_names={-2:'low',5:'middle',9:'high'}
status_names={0:'start',3:'end',1:'between',10:'later'}
class_names={0:'default',1:'else'}
for phase in [-2,5,9]:
 for status in [None,0,3,1,10]:
  for copy in [None,-2,5,9]:
   for klass in [0,1]:
    amount=(phase+19*klass+(status or 0)*7+(copy or 0))%256
    panel={'marker':True,'phase':phase_names[phase],'class':class_names[klass],'amount':amount}
    if status is not None:panel['status']=status_names[status]
    if copy is not None:panel['copy']=phase_names[copy]
    for envelope in [False,True]:
     name='Envelope' if envelope else 'Panel'
     value={'lead':True,'branch':('panel',panel),'panels':[panel]} if envelope else panel
     native=oracle.encode(name,value)
     expected=model(phase,status,copy,klass,amount,envelope)
     if native!=expected:raise RuntimeError(f'Native/model mismatch {name} {(phase,status,copy,klass,amount)}: {native.hex()} {expected.hex()}')
     semantic=f'1:{phase}:{status if status is not None else "-"}:{copy if copy is not None else "-"}:{klass}:{amount}'
     cases.append((name,phase,status,copy,klass,amount,native,value,semantic+'|'+semantic if envelope else semantic))
with tempfile.TemporaryDirectory(prefix='n17-shape-') as temporary:
 work=Path(temporary);generated=a.generated.resolve() if a.generated else work/'generated'
 if not a.generated:
  generated.mkdir()
  subprocess.run([str(build/'libasn1typed/check_asn1typed_shape_render'),str(fixture),'ShapeGeneration','shaperef',str(generated)],check=True)
 header_hashes={name:hashlib.sha256((generated/name).read_bytes()).hexdigest() for name in ['types.hpp','mapping.hpp','codec.hpp']}
 subprocess.run(compiler+flags+['-I'+str(repo/'libaper'),'-I'+str(generated),str(source/'driver.cpp'),str(repo/'libaper/runtime.cpp'),'-o',str(work/'driver')],check=True)
 requests=[];expected=[]
 for name,phase,status,copy,klass,amount,wire,_,semantic in cases:
  args=f'{"E" if name=="Envelope" else "P"} {phase} {status if status is not None else -1} {copy if copy is not None else -99} {klass} {amount}'
  requests.append(f'E {args} -');expected.append(wire.hex())
  requests.append(f'D {args} {wire.hex()}');expected.append(semantic)
 output=subprocess.run([str(work/'driver')],input='\n'.join(requests)+'\n',capture_output=True,text=True,check=True).stdout.splitlines()
 if output!=expected:
  for index,(got,want) in enumerate(zip(output,expected)):
   if got!=want:raise RuntimeError(f'Generated/native mismatch request{index}: {got} vs {want}')
  raise RuntimeError('Wrong output count')
 for i,(name,_,_,_,_,_,_,value,_) in enumerate(cases):
  if oracle.decode(name,bytes.fromhex(output[2*i]))!=value:raise RuntimeError(f'Native decoded semantic mismatch {name}')
if hashes()!=initial:raise RuntimeError('Sources changed during qualification; refusing publication')
summary={'status':'PASS','oracle':'asn1tools','oracle_version':'0.167.0','native_model_cases':len(cases),'generated_encode_cases':len(cases),'generated_decode_cases':len(cases),'native_decode_cases':len(cases),'source_sha256':initial,'source_snapshot_verified_unchanged':True,'generated_header_sha256':header_hashes,'compiler':version,'compiler_command':compiler,'compiler_flags':flags,'fixture_module':'ShapeGeneration','generated_namespace':'shaperef','scope':'Actual generated inline ENUMERATED root/addition fields, optional presence, CHOICE/list nested compound. Known values only; no full NGAP qualification.'}
a.output.write_text(json.dumps(summary,indent=2)+'\n');print(json.dumps(summary,indent=2))
