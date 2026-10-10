#!/usr/bin/env python3
"""Actual generated character codecs against asn1tools roots and UTF8 wire.
Known-multiplier size extensions are independently modeled in the C++ driver;
asn1tools 0.167.0 explicitly cannot encode these extensions.
"""
import hashlib,json,os,pathlib,subprocess,tempfile
import asn1tools
ROOT=pathlib.Path(__file__).resolve().parents[2]
def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def main():
 assert asn1tools.__version__=="0.167.0", "Pinned reference asn1tools 0.167.0 is required"
 with tempfile.TemporaryDirectory(prefix='character-qualification-') as td:
  work=pathlib.Path(td)
  subprocess.run([str(ROOT/'libasn1typed/check_asn1typed_character_render'),str(ROOT/'libasn1typed/fixtures/characters-generation-batch.asn1'),str(work)],check=True)
  subprocess.run([str(ROOT/'libasn1typed/check_asn1typed_ioc_render'),str(ROOT/'libasn1typed/fixtures/ioc-characters-generation-batch.asn1'),'IOCCharacterGeneration','Message','ioccharacters',str(work/'physical')],check=True)
  subprocess.run([os.environ.get('CXX','c++'),'-std=c++20','-Wall','-Wextra','-Werror','-pedantic-errors','-Wconversion','-Wsign-conversion','-DNDEBUG','-I'+str(ROOT/'libaper'),'-I'+str(work),str(ROOT/'libasn1typed/check_asn1typed_character_generated.cpp'),str(ROOT/'libaper/runtime.cpp'),'-o',str(work/'check')],check=True)
  lines=subprocess.check_output([str(work/'check'),'--dump'],text=True).splitlines();native=0;model_only=0;cache={}
  for line in lines:
   kind,lower,upper,ext,prefix,text,encoded=line.split();kind=int(kind);unconstrained=lower=="none";lower=0 if unconstrained else int(lower);upper=0 if unconstrained else int(upper);ext=bool(int(ext));prefix=bool(int(prefix));raw=bytes.fromhex(text) if text!='-' else b'';value=raw.decode('utf8' if kind==2 else 'ascii')
   if kind!=2 and ext and not lower<=len(value)<=upper:model_only+=1;continue
   key=(kind,lower,upper,ext,prefix,unconstrained)
   if key not in cache:
    name=('PrintableString','VisibleString','UTF8String')[kind]
    # Prefix bool belongs to a real SEQUENCE, matching callback's first bit.
    size='' if unconstrained else f' (SIZE({lower}..{upper}{",..." if ext else ""}))'
    source=f'M DEFINITIONS ::= BEGIN S ::= {name}{size} V ::= SEQUENCE {{ prefix BOOLEAN, value S }} END'
    cache[key]=asn1tools.compile_string(source,'per')
   codec=cache[key];typename='V' if prefix else 'S';data={'prefix':True,'value':value} if prefix else value
   expected=codec.encode(typename,data)
   assert expected.hex()==encoded,(key,text,expected.hex(),encoded)
   decoded=codec.decode(typename,bytes.fromhex(encoded));assert decoded==data
   native+=1
  sources=['libasn1typed/asn1typed.c','libasn1typed/asn1typed.h','libasn1typed/asn1typed_extract.c','libasn1typed/asn1typed_name.c','libasn1typed/asn1typed_render_cpp_inline_enum.c','libaper/runtime.hpp','libaper/runtime.cpp','libasn1typed/asn1typed_render_cpp_compound.c','libasn1typed/asn1typed_render_cpp.h','libasn1typed/check_asn1typed_character_generated.cpp','libasn1typed/check_asn1typed_character_render.c','libasn1typed/fixtures/characters-generation-batch.asn1','libasn1typed/fixtures/ioc-characters-generation-batch.asn1','tools/shared-character-qualification/qualify.py']
  summary={'status':'PASS','reference':'asn1tools '+asn1tools.__version__,'vectors':len(lines),'native_encode_decode_vectors':native,'known_multiplier_extension_model_only_vectors':model_only,'all_vectors_independent_cpp_bit_model':True,'limitations':['asn1tools 0.167.0 known-multiplier SIZE extensions unsupported; extension values checked against independent bit model','Unfragmented UTF8 wire and known-multiplier extension lengths <16384 octets/characters; resource refusal at fragmentation determinant','Not complete NGAP-PDU qualification'],'source_sha256':{p:sha(ROOT/p) for p in sources},'generated_header_sha256':{p:sha(work/p) for p in ('types.hpp','mapping.hpp','codec.hpp')}}
  destination=ROOT/'tools/shared-character-qualification/qualification-summary.json';destination.write_text(json.dumps(summary,indent=2)+'\n');print(json.dumps(summary,indent=2))
if __name__=='__main__':main()
