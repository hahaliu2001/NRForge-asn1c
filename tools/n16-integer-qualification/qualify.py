"""Bounded aligned-PER INTEGER qualification against native ASN.1 tools."""
import argparse
import hashlib
import importlib.metadata
import json
from pathlib import Path
import shlex
import subprocess
import tempfile
import asn1tools

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--repo', type=Path, default=Path(__file__).resolve().parents[2])
parser.add_argument('--cxx', default='c++')
parser.add_argument('--output', type=Path, required=True)
args = parser.parse_args()
if importlib.metadata.version('asn1tools') != '0.167.0':
    raise RuntimeError('Require asn1tools==0.167.0; assess oracle profile before upgrading')
repo = args.repo.resolve()
source = Path(__file__).resolve().parent
provenance_paths = [repo/'libaper/runtime.hpp', repo/'libaper/runtime.cpp',
                    source/'driver.cpp', source/'qualify.py']
def source_hashes():
    return {str(path.relative_to(repo)): hashlib.sha256(path.read_bytes()).hexdigest()
            for path in provenance_paths}
initial_hashes = source_hashes()
compiler_command = shlex.split(args.cxx)
compiler_version = subprocess.run(compiler_command+['--version'], capture_output=True,
                                text=True, check=True).stdout.splitlines()[0]
compiler_flags = ['-std=c++20','-Wall','-Wextra','-Werror','-pedantic-errors',
                  '-Wconversion','-Wsign-conversion']
minimum64=-(1<<63)
maximum64=(1<<63)-1
profiles={
 'ConstantNegative':('S',-7,-7), 'ConstantUnsigned':('U',42,42),
 'Two':('S',-1,0),'Three':('S',-2,0),'Card255':('S',-100,154),
 'Card256':('U',900,1155),'Card257':('S',-200,56),
 'Card65536':('S',-32768,32767),'Card65537':('S',-32768,32768),
 'Signed32':('S',-(1<<31),(1<<31)-1),'Positive40':('U',7,7+(1<<40)-1),
 'SignedLowEdge':('S',minimum64,minimum64+256),
 'SignedHighEdge':('S',maximum64-256,maximum64),
 'FullSigned':('S',minimum64,maximum64),'FullUnsigned':('U',0,(1<<64)-1),
 'UnsignedHighEdge':('U',(1<<64)-257,(1<<64)-1),
}
schema='IntegerOracle DEFINITIONS ::= BEGIN\n'
for name,(_,lo,hi) in profiles.items():
    schema+=f'{name} ::= INTEGER ({lo}..{hi})\n'
    for prefix in range(1,8):
        fields=[f'p{i} BOOLEAN' for i in range(prefix)]+[f'value {name}']
        schema+=f'P{prefix}{name} ::= SEQUENCE {{'+','.join(fields)+'}\n'
schema+='END\n'
oracle=asn1tools.compile_string(schema,'per')
def model(value,lo,hi,prefix):
    bits=[i%2==0 for i in range(prefix)]
    def align(): bits.extend([False]*((-len(bits))%8))
    def append(v,width): bits.extend(bool(v&(1<<i)) for i in range(width-1,-1,-1))
    cardinality=hi-lo+1
    offset=value-lo
    if cardinality<=255: append(offset,(cardinality-1).bit_length())
    elif cardinality==256: align();append(offset,8)
    elif cardinality<=65536: align();append(offset,16)
    else:
        max_octets=((cardinality-1).bit_length()+7)//8
        octets=max(1,(offset.bit_length()+7)//8)
        append(octets-1,(max_octets-1).bit_length())
        align();append(offset,octets*8)
    if not bits: bits=[False]*8
    align()
    return bytes(sum(int(b)<<(7-i) for i,b in enumerate(bits[j:j+8])) for j in range(0,len(bits),8))
cases=[]
oracle_empty_substitution_difference=[]
for name,(kind,lo,hi) in profiles.items():
    offsets={0,hi-lo,(hi-lo)//2}
    for edge in [1,127,128,255,256,65535,65536,(1<<24)-1,1<<24,(1<<32)-1,1<<32,(1<<56)-1,1<<56]:
        if edge<=hi-lo:offsets.add(edge)
    for prefix in range(8):
        for offset in sorted(offsets):
            number=lo+offset
            typename=name if prefix==0 else f'P{prefix}{name}'
            value=number if prefix==0 else {**{f'p{i}':i%2==0 for i in range(prefix)},'value':number}
            native=oracle.encode(typename,value)
            expected=model(number,lo,hi,prefix)
            if not native and lo==hi and prefix==0:
                oracle_empty_substitution_difference.append({'profile':name,'prefix':prefix,'native_hex':'','complete_hex':'00'})
                native=expected
            if native!=expected:raise RuntimeError(f'Native/model mismatch {name} prefix{prefix} offset{offset}: {native.hex()} vs {expected.hex()}')
            cases.append((name,kind,lo,hi,prefix,number,native,typename,value))
with tempfile.TemporaryDirectory(prefix='n16-octet-') as temporary:
    work=Path(temporary)
    subprocess.run(compiler_command+compiler_flags+['-I'+str(repo/'libaper'),str(source/'driver.cpp'),
        str(repo/'libaper/runtime.cpp'),'-o',str(work/'driver')],check=True)
    requests=[];expected=[]
    for _,kind,lo,hi,prefix,number,wire,_,_ in cases:
        requests.append(f'E {kind} {lo} {hi} {prefix} {number}');expected.append(wire.hex())
        requests.append(f'D {kind} {lo} {hi} {prefix} {wire.hex()}');expected.append(str(number))
    output=subprocess.run([str(work/'driver')],input='\n'.join(requests)+'\n',text=True,capture_output=True,check=True).stdout.splitlines()
    if output!=expected:
        for index,(got,want) in enumerate(zip(output,expected)):
            if got!=want:raise RuntimeError(f'Runtime/native mismatch request{index}: {got} vs {want}')
        raise RuntimeError('Wrong response count')
    for i,case in enumerate(cases):
        _,_,lo,hi,prefix,_,_,typename,value=case
        native_input=b'' if lo==hi and prefix==0 else bytes.fromhex(output[2*i])
        if oracle.decode(typename,native_input)!=value:raise RuntimeError(f'Native decode mismatch {typename}')
if source_hashes()!=initial_hashes:raise RuntimeError('Qualification sources changed during execution; refusing publication')
summary={'status':'PASS','source_sha256':initial_hashes,
 'source_snapshot_verified_unchanged':True,'compiler':compiler_version,
 'compiler_command':compiler_command,'compiler_flags':compiler_flags,
 'compile_inputs':['libaper/runtime.cpp','tools/n16-integer-qualification/driver.cpp'],
 'include_directories':['libaper'],'oracle':'asn1tools','oracle_version':'0.167.0',
 'native_model_exact_cases':len(cases)-len(oracle_empty_substitution_difference),'runtime_encode_cases':len(cases),
 'runtime_decode_cases':len(cases),'native_decode_exact_wire_cases':len(cases)-len(oracle_empty_substitution_difference),
 'native_decode_constant_field_semantics_cases':len(oracle_empty_substitution_difference),
 'oracle_empty_substitution_difference':oracle_empty_substitution_difference,
 'offsets':list(range(8)),'profiles':list(profiles),
 'schema_sha256':hashlib.sha256(schema.encode()).hexdigest(),
 'runtime_sha256':hashlib.sha256((repo/'libaper/runtime.cpp').read_bytes()).hexdigest(),
 'scope':'Nonextensible bounded signed/unsigned 64-bit INTEGER primitive runtime framing. No generated codec or NGAP/PDU qualification.'}
args.output.write_text(json.dumps(summary,indent=2)+'\n')
print(json.dumps(summary,indent=2))
