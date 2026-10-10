"""Bounded aligned-PER BIT STRING qualification against native ASN.1 tools."""
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
profiles = {
 'Fixed0': (0,0,False,[0]), 'Fixed1': (1,1,False,[1]),
 'Fixed8': (8,8,False,[8]), 'Fixed16': (16,16,False,[16]),
 'Fixed17': (17,17,False,[17]),
 'FixedWide': (65535,65535,False,[65535]),
 'Small': (0,3,False,[0,1,2,3]),
 'Medium': (1,256,False,[1,2,3,127,128,255,256]),
 'Wide': (0,65535,False,[0,1,8,16,17,127,128,255,256,65535]),
 'Unbounded': (0,0,True,[0,1,8,16,17,127,128,255,256,16383]),
}
schema = 'BitOracle DEFINITIONS ::= BEGIN\n'
for name,(lower,upper,unconstrained,_) in profiles.items():
    constraint = '' if unconstrained else f' (SIZE ({lower}..{upper}))'
    schema += f'{name} ::= BIT STRING{constraint}\n'
    for prefix in range(1,8):
        fields = [f'p{i} BOOLEAN' for i in range(prefix)] + [f'value {name}']
        schema += f'P{prefix}{name} ::= SEQUENCE {{'+','.join(fields)+'}\n'
schema += 'END\n'
oracle = asn1tools.compile_string(schema, 'per')

def model(payload,count,lower,upper,unconstrained,prefix):
    bits = [i%2==0 for i in range(prefix)]
    def align():
        bits.extend([False]*((-len(bits))%8))
    def append(value,width):
        bits.extend(bool(value & (1<<i)) for i in range(width-1,-1,-1))
    variable = unconstrained or lower != upper
    cardinality = upper-lower+1
    if unconstrained:
        align()
        append(count if count<128 else count|0x8000,
               8 if count<128 else 16)
    elif variable:
        width = (cardinality-1).bit_length()
        if cardinality>=256:
            align(); width=8 if cardinality==256 else 16
        append(count-lower,width)
    elif upper>16:
        align()
    if variable: align()
    for i in range(count):
        bits.append(bool(payload[i//8] & (0x80 >> (i%8))))
    if not bits: bits=[False]*8
    align()
    return bytes(sum(int(b)<< (7-i) for i,b in enumerate(bits[j:j+8])) for j in range(0,len(bits),8))

cases=[]
oracle_empty_substitution_difference=[]
for name,(lower,upper,unconstrained,lengths) in profiles.items():
    for prefix in range(8):
        for length in lengths:
            payload=bytearray((i*37+length)%256 for i in range((length+7)//8))
            if length%8: payload[-1] &= 0xff << (8-length%8)
            payload=bytes(payload)
            typename=name if prefix==0 else f'P{prefix}{name}'
            value=(payload,length) if prefix==0 else {**{f'p{i}':i%2==0 for i in range(prefix)},'value':(payload,length)}
            native=oracle.encode(typename,value)
            expected=model(payload,length,lower,upper,unconstrained,prefix)
            if not native and name=='Fixed0' and prefix==0:
                oracle_empty_substitution_difference.append({'profile':name,'prefix':prefix,'native_hex':'','complete_hex':'00'})
                native=expected
            if native!=expected:
                raise RuntimeError(f'Native/model mismatch {name} prefix{prefix} length{length}: {native[:20].hex()} vs {expected[:20].hex()}')
            cases.append((name,lower,upper,unconstrained,prefix,length,payload,native,typename,value))
with tempfile.TemporaryDirectory(prefix='n15-octet-') as temporary:
    work=Path(temporary)
    subprocess.run(compiler_command+compiler_flags+['-I'+str(repo/'libaper'),str(source/'driver.cpp'),
        str(repo/'libaper/runtime.cpp'),'-o',str(work/'driver')],check=True)
    requests=[]; expected=[]
    for _,lower,upper,u,p,count,payload,wire,_,_ in cases:
        requests.append(f'E {lower} {upper} {int(u)} {p} {count} {payload.hex() or "-"}')
        expected.append(wire.hex())
        requests.append(f'D {lower} {upper} {int(u)} {p} {count} {wire.hex()}')
        expected.append(str(count)+':'+(payload.hex() or '-'))
    output=subprocess.run([str(work/'driver')],input='\n'.join(requests)+'\n',text=True,
                          capture_output=True,check=True).stdout.splitlines()
    if output!=expected:
        for index,(got,want) in enumerate(zip(output,expected)):
            if got!=want: raise RuntimeError(f'Runtime/native mismatch request{index}: {got[:100]} vs {want[:100]}')
        raise RuntimeError('Wrong response count')
    # Decode runtime-produced bytes through the independent native implementation.
    for i,case in enumerate(cases):
        _,_,_,_,_,_,_,_,typename,value=case
        native_input = b'' if typename=='Fixed0' else bytes.fromhex(output[2*i])
        if oracle.decode(typename,native_input) != value:
            raise RuntimeError(f'Native decode mismatch {typename}')
if source_hashes() != initial_hashes:
    raise RuntimeError('Qualification sources changed during execution; refusing to publish evidence')
summary={'status':'PASS','source_sha256':initial_hashes,
 'source_snapshot_verified_unchanged':True,'compiler':compiler_version,
 'compiler_command':compiler_command,'compiler_flags':compiler_flags,
 'compile_inputs':['libaper/runtime.cpp','tools/n15-bit-qualification/driver.cpp'],
 'include_directories':['libaper'],'oracle':'asn1tools','oracle_version':'0.167.0',
 'native_model_exact_cases':len(cases)-len(oracle_empty_substitution_difference),'runtime_encode_cases':len(cases),
 'runtime_decode_cases':len(cases),'native_decode_exact_wire_cases':len(cases)-len(oracle_empty_substitution_difference),
 'native_decode_empty_field_semantics_cases':len(oracle_empty_substitution_difference),
 'oracle_empty_substitution_difference':oracle_empty_substitution_difference,
 'offsets':list(range(8)),'profiles':list(profiles),
 'schema_sha256':hashlib.sha256(schema.encode()).hexdigest(),
 'runtime_sha256':hashlib.sha256((repo/'libaper/runtime.cpp').read_bytes()).hexdigest(),
 'scope':'Unnamed-bit BIT STRING primitive runtime framing; unfragmented unconstrained <=16383 bits. No generated codec or NGAP/PDU qualification.'}
args.output.write_text(json.dumps(summary,indent=2)+'\n')
print(json.dumps(summary,indent=2))
