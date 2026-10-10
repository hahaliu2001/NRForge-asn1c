#!/usr/bin/env python3
"""One linked public registry, fresh typed values, exact frozen native semantics."""
import argparse
from concurrent.futures import ThreadPoolExecutor, as_completed
import hashlib,importlib.util,importlib.metadata,json,pathlib,re,shlex,subprocess,sys
sys.dont_write_bytecode=True
from semantic_bridge import Bridge

def load(name,path):
 spec=importlib.util.spec_from_file_location(name,path);m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m);return m

def sha(data):return hashlib.sha256(data).hexdigest()
def restore(v):
 if isinstance(v,dict) and v.get('kind')=='bytes':return bytes.fromhex(v['hex'])
 if isinstance(v,dict) and v.get('kind')=='tuple':return tuple(restore(x) for x in v['items'])
 if isinstance(v,dict):return {k:restore(x) for k,x in v.items()}
 if isinstance(v,list):return [restore(x) for x in v]
 return v

def verify_schema(native,generation,registry_source):
 descriptions=native.NGAP_PDU_Descriptions
 pdu=descriptions.NGAP_PDU;procedure_class=descriptions.NGAP_ELEMENTARY_PROCEDURE;object_set=descriptions.NGAP_ELEMENTARY_PROCEDURES
 table=object_set.get_val()
 if table.ext or len(table.root)!=81:raise ValueError('native procedure closure changed')
 role_names=('initiatingMessage','successfulOutcome','unsuccessfulOutcome');payload_names=('InitiatingMessage','SuccessfulOutcome','UnsuccessfulOutcome')
 if set(pdu._root)!=set(role_names) or len(pdu._root)!=3:raise ValueError('native root closure changed')
 tags={name:tuple(pdu._cont[name]._tagc[0]) for name in role_names}
 if len(set(tags.values()))!=3:raise ValueError('native root tags ambiguous')
 canonical=sorted(role_names,key=lambda name:tags[name]);role_per=[canonical.index(name) for name in role_names]
 rows=[{'code':row['procedureCode'],'criticality':('reject','ignore','notify').index(row['criticality']),'payload_present':[name in row for name in payload_names]} for row in list(table.root)+list(table.ext or [])]
 expected={'pdu_module':pdu._mod,'pdu_type':pdu._name,'procedure_class_module':procedure_class._mod,'procedure_class':procedure_class._name,'object_set_module':object_set._mod,'object_set':object_set._name,'procedures':rows}
 if generation['schema']!=expected:raise ValueError('manifest schema differs from independent frozen native descriptors')
 expected_registry={'role_to_per_index':role_per,'object_set_extensible':table.ext is not None}
 if generation['registry']!=expected_registry:raise ValueError('manifest role PER/object-set extension evidence differs from native')
 # Inspect the actual compiled registry TU too: metadata cannot merely describe
 # a different set of declarations than the linked public dispatcher uses.
 block=re.search(r'array<ProcedureInfo,(\d+)>\s+procedures\{\{(.*?)\}\};',registry_source,re.S)
 if not block:raise ValueError('actual registry procedure declaration missing')
 pattern=r'\{UINT64_C\((\d+)\),static_cast<Criticality>\((\d+)\),\{\{(true|false),(true|false),(true|false)\}\}\},'
 actual=[{'code':int(code),'criticality':int(policy),'payload_present':[a=='true',b=='true',c=='true']} for code,policy,a,b,c in re.findall(pattern,block.group(2))]
 if re.sub(pattern,'',block.group(2)).strip() or int(block.group(1))!=len(rows) or actual!=rows:raise ValueError('actual compiled registry procedure schema differs from native')
 creations=re.findall(r'Registry::create\(\{\{(\d+),(\d+),(\d+)\}\},(true|false),procedures,messages\)',registry_source)
 if len(creations)!=1 or [int(x) for x in creations[0][:3]]!=role_per or (creations[0][3]=='true')!=(table.ext is not None):raise ValueError('actual compiled registry role PER/extension evidence differs from native')
 return expected_registry

def physical(bridge,case,index,reference):
 wire=bytes.fromhex(case['pdu_hex'])
 lines=['static void physical_checks() {','using namespace ::nrforge::ngap;','using Error = ::nrforge::aper::ErrorCode;','::std::size_t checks = 0;']
 for name,bad,code,offset in reference.root_error_cases(wire):
  lines += ['{','const auto bytes = '+bridge.literal(bad)+';','auto result = decode_ngap_pdu(bytes);',f'REQUIRE(!result && result.error().code == Error::{code} && result.error().bit_offset == {offset});','++checks;','}']
 lines += ['const auto bytes = '+bridge.literal(wire)+';','auto valid = decode_ngap_pdu(bytes);','REQUIRE(valid && valid.value().kind() == PduKind::typed);',
  '::nrforge::aper::Limits exact{};',f'exact.max_input_octets = {len(wire)}; exact.max_output_octets = {len(wire)}; exact.max_wire_bits = {len(wire)*8};',
  'REQUIRE(decode_ngap_pdu(bytes,exact));','REQUIRE(encode_ngap_pdu(valid.value(),exact));','checks += 2;',
  'for(unsigned budget = 0; budget < 3; ++budget) {','auto limits = exact;',
  'if(budget == 0) --limits.max_input_octets;','if(budget == 1) --limits.max_output_octets;','if(budget == 2) --limits.max_wire_bits;',
  'if(budget != 1) { auto r = decode_ngap_pdu(bytes,limits); REQUIRE(!r && r.error().code == Error::resource_limit); ++checks; }',
  'if(budget != 0) { auto r = encode_ngap_pdu(valid.value(),limits); REQUIRE(!r && r.error().code == Error::resource_limit); ++checks; }','}',
  'auto input = bytes;','auto owned = decode_ngap_pdu(input); REQUIRE(owned);','auto copied = owned.value(); auto moved = ::std::move(owned).value();',
  'REQUIRE(owned.value().kind() == PduKind::invalid);',
  '::std::fill(input.begin(),input.end(),::std::byte{0xff}); input.clear(); input.shrink_to_fit();',
  'auto cw = encode_ngap_pdu(copied); auto mw = encode_ngap_pdu(moved); REQUIRE(cw && mw && cw.value().octets == bytes && mw.value().octets == bytes); ++checks;',
  'const auto malformed = '+bridge.literal(wire[:3]+b'\x01\xff')+';',
  'REQUIRE(!decode_ngap_pdu(malformed)); ++checks;',f'::std::cout << "PHYSICAL {index} " << checks << "\\n";','}']
 return '\n'.join(lines)

def main():
 parser=argparse.ArgumentParser()
 parser.add_argument('--repo',type=pathlib.Path,default=pathlib.Path(__file__).resolve().parents[2])
 parser.add_argument('--generated-root',type=pathlib.Path,required=True)
 parser.add_argument('--asn1-root',type=pathlib.Path,required=True)
 parser.add_argument('--work',type=pathlib.Path,required=True)
 parser.add_argument('--output',type=pathlib.Path,required=True)
 parser.add_argument('--cxx',default='g++');parser.add_argument('--jobs',type=int,default=1)
 parser.add_argument('--accepted-profile',type=pathlib.Path)
 args=parser.parse_args();args.output.unlink(missing_ok=True)
 if args.jobs<1:raise ValueError('jobs must be positive')
 if importlib.metadata.version('pycrate')!='0.7.11':raise ValueError('pycrate 0.7.11 is required')
 repo=args.repo.resolve();generated=args.generated_root.resolve();root=args.asn1_root.resolve();work=args.work.resolve();work.mkdir(parents=True,exist_ok=False)
 historical=repo/'tools/full-pdu-qualification';sys.path.insert(0,str(historical))
 native_values=load('unified_native_values',historical/'native_values.py')
 framing=load('unified_reference_framing',historical/'reference_framing.py')
 reference=load('unified_native_reference',repo/'tools/n11-envelope-qualification/native_reference.py')
 golden_path=args.accepted_profile or historical/'accepted-profile.json';golden=json.loads(golden_path.read_text())
 manifest_path=repo/'tools/n11-envelope-qualification/source-manifest.json';authority,texts=reference.verify_sources(repo,root,manifest_path)
 if authority!=golden['source_authority']:raise ValueError('frozen source authority differs from accepted golden')
 generation=json.loads((generated/'manifest.json').read_text())
 if generation.get('parse')!='PASS' or generation.get('fix')!='PASS' or generation.get('parser_deleted') is not True or generation.get('deterministic') is not True or generation.get('message_count')!=131:raise ValueError('generator lifecycle/determinism closure missing')
 registry_evidence=generation.get('registry',{})
 if sorted(registry_evidence.get('role_to_per_index',[]))!=[0,1,2] or not isinstance(registry_evidence.get('object_set_extensible'),bool):raise ValueError('registry wire evidence missing')
 if not generation.get('schema'):raise ValueError('generated schema provenance missing')
 entries=generation.get('messages',generation.get('identities'))
 if not isinstance(entries,list) or len(entries)!=131:raise ValueError('complete generated manifest requires 131 message identities')
 if any(e.get('deterministic') is not True for e in entries):raise ValueError('per-message deterministic evidence missing')
 for e in entries:
  for key in ('public_header','types_header','mapping_header','codec_header','adapter'):
   path=pathlib.Path(e[key])
   if path.is_absolute() or '..' in path.parts or not (generated/path).is_file() or not (generated/path).resolve().is_relative_to(generated):raise ValueError('generated manifest path escapes provenance: '+key)
 names=[e['message'] for e in entries]
 if len(set(names))!=131 or len({(e['role'],e['code']) for e in entries})!=131:raise ValueError('duplicate generated registry identity')
 # Snapshot every production input and every supplied generated file before execution.
 sources=[*sorted((repo/'libaper').glob('*.hpp')),*sorted((repo/'libaper').glob('*.cpp')),*sorted((repo/'libngap').glob('*.hpp')),*sorted((repo/'libngap').glob('*.cpp')),
  *sorted((repo/'libasn1typed').glob('asn1typed*.c')),*sorted((repo/'libasn1typed').glob('asn1typed*.h')),
  *sorted(pathlib.Path(__file__).resolve().parent.glob('*.py')),*sorted(historical.glob('*.py')),repo/'tools/n11-envelope-qualification/native_reference.py',manifest_path,golden_path,repo/'tools/ngap-dispatch/generate.c',repo/'tools/Makefile.am']
 fingerprints={str(p.relative_to(repo)):sha(p.read_bytes()) for p in sources}
 generated_files=sorted(p for p in generated.rglob('*') if p.is_file())
 if any(not p.resolve().is_relative_to(generated) for p in generated_files):raise ValueError('generated file escapes provenance')
 generated_hashes={str(p.relative_to(generated)):sha(p.read_bytes()) for p in generated_files}
 native=reference.compile_native(texts,work)
 verify_schema(native,generation,(generated/'registry.cpp').read_text())
 profiles={p['message']:p for p in native_values.build_profile(native)}
 gold={r['message']:r for r in golden['identities']}
 if set(names)!=set(profiles) or set(names)!=set(gold):raise ValueError('native/generated/golden message closure differs')
 descriptors={}
 table=native.NGAP_PDU_Descriptions.NGAP_ELEMENTARY_PROCEDURES.get_val()
 for row in list(table.root)+list(table.ext or []):
  for field in ('InitiatingMessage','SuccessfulOutcome','UnsuccessfulOutcome'):
   if field in row:descriptors[row[field]._typeref.called[1]]=row[field]
 options=['-std=c++20','-Wall','-Wextra','-Werror','-pedantic-errors','-Wconversion','-Wsign-conversion','-DNDEBUG']
 compiler=shlex.split(args.cxx);include=['-I'+str(repo/'libaper'),'-I'+str(repo/'libngap'),'-I'+str(generated)]
 def compile_object(source,destination,log):
  run=subprocess.run(compiler+options+include+['-c',str(source),'-o',str(destination)],stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True)
  log.write_text(run.stdout)
  if run.returncode:raise ValueError('strict compilation failed: '+str(log))
 runtime=work/'runtime.o';api=work/'pdu.o';registry=work/'registry.o'
 compile_object(repo/'libaper/runtime.cpp',runtime,work/'runtime.log');compile_object(repo/'libngap/pdu.cpp',api,work/'pdu.log');compile_object(generated/'registry.cpp',registry,work/'registry.log')
 def identity(value):return sha(json.dumps(native_values.json_value(value),sort_keys=True,separators=(',',':')).encode())
 def emit(index,entry):
  name=entry['message'];group=profiles[name];accepted=gold[name]
  if group['code']!=entry['code'] or framing.ROLES.index(group['role'])!=entry['role']:raise ValueError('native/generated role/code discrepancy')
  if framing.POLICIES.index(accepted['expected_criticality'])!=entry['criticality']:raise ValueError('declared criticality discrepancy')
  cases=group['cases'];old={c['id']:c for c in accepted['cases']}
  if len(cases)!=len(old) or {c['id'] for c in cases}!=set(old):raise ValueError('accepted case identity set changed')
  for c in cases:
   expected=(group['role'],{'procedureCode':group['code'],'criticality':c['criticality'],'value':(name,c['value'])})
   if c['status']!='PASS' or sha(bytes.fromhex(c['pdu_hex']))!=old[c['id']]['wire_sha256'] or identity(expected)!=old[c['id']]['semantic_sha256']:raise ValueError('accepted native golden bytes or semantics changed')
  cases=[dict(c,value=restore(c['storage_semantic_json'])) for c in cases]
  bridge=Bridge(generated/entry['types_header'],generated/entry['mapping_header'],generated/entry['codec_header'])
  tu=bridge.emit_unified(descriptors[name],cases,entry['cpp_body_type'],entry['public_header'],entry['adapter'],index,name,entry['role'],entry['code'])
  fn=f'void run_identity_{index:03d}() {{'
  tu=tu.replace(fn,physical(bridge,cases[0],index,framing)+'\n'+fn+'\nphysical_checks();')
  path=work/f'{index:03d}.cpp';path.write_text(tu);obj=work/f'{index:03d}.o';compile_object(path,obj,work/f'{index:03d}.compile.log')
  return index,obj,sha(path.read_bytes())
 details=[]
 with ThreadPoolExecutor(max_workers=args.jobs) as pool:
  futures=[pool.submit(emit,i,e) for i,e in enumerate(entries)]
  try:
   for future in as_completed(futures):details.append(future.result());print('compiled adapter/test',len(details),'/131',flush=True)
  except BaseException:
   for future in futures:future.cancel()
   raise
 mainlines=['#include "pdu.hpp"','#include <cstdlib>','#include <iostream>','#include <set>','#define REQUIRE(...) do { if(!(__VA_ARGS__)) ::std::abort(); } while(0)']
 mainlines += [f'void run_identity_{i:03d}();' for i in range(131)]+['int main() {','const auto& state = ::nrforge::ngap::ngap_registry_state();','REQUIRE(state && state.value().message_count() == 131);','::std::set<::std::pair<unsigned,::std::uint64_t>> keys;']
 for e in entries:
  mainlines += ['{ bool found = false;','for(::std::size_t i=0;i<state.value().message_count();++i) {','const auto* info = state.value().message_info(i);','REQUIRE(info);',f'if(info->message == "{e["message"]}") {{',f'REQUIRE(info->module == "NGAP-PDU-Contents" && static_cast<unsigned>(info->role) == {e["role"]} && info->procedure_code == {e["code"]} && static_cast<unsigned>(info->declared_criticality) == {e["criticality"]});','REQUIRE(keys.insert({static_cast<unsigned>(info->role),info->procedure_code}).second); found = true;','}','}','REQUIRE(found); }']
 mainlines += ['REQUIRE(keys.size() == 131);','::std::cout << "REGISTRY 131\\n";']+[f'run_identity_{i:03d}();' for i in range(131)]+['}']
 mainpath=work/'main.cpp';mainpath.write_text('\n'.join(mainlines));mainobj=work/'main.o';compile_object(mainpath,mainobj,work/'main.log')
 executable=work/'unified-check'
 subprocess.run(compiler+options+[str(mainobj),str(runtime),str(api),str(registry)]+[str(obj) for _,obj,_ in sorted(details)]+['-o',str(executable)],check=True)
 run=subprocess.run([str(executable)],stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True);(work/'actual.output').write_text(run.stdout);(work/'actual.stderr').write_text(run.stderr)
 if run.returncode:raise ValueError('unified execution failed; see actual.stderr')
 actual={};physical_counts={};registry_count=0
 for line in run.stdout.splitlines():
  fields=line.split()
  if fields==['REGISTRY','131']:registry_count+=1
  elif len(fields)==3 and fields[0]=='PHYSICAL':
   i=int(fields[1])
   if i in physical_counts:raise ValueError('duplicate physical response')
   physical_counts[i]=int(fields[2])
  elif len(fields)==4 and fields[0]=='CASE':
   key=(int(fields[1]),fields[2])
   if key in actual:raise ValueError('duplicate case response')
   actual[key]=bytes.fromhex(fields[3])
  else:raise ValueError('unexpected unified response')
 if registry_count!=1 or set(physical_counts)!=set(range(131)):raise ValueError('incomplete registry/physical execution')
 records=[];pdu=native.NGAP_PDU_Descriptions.NGAP_PDU;tu_hashes={i:h for i,_,h in details}
 for i,e in enumerate(entries):
  group=profiles[e['message']];proof=[]
  for case in group['cases']:
   wire=actual.pop((i,case['id']));expected=(group['role'],{'procedureCode':group['code'],'criticality':case['criticality'],'value':(e['message'],case['value'])})
   if wire.hex()!=case['pdu_hex']:raise ValueError('actual public generated bytes differ')
   frame=framing.parse_root(wire)
   if frame.payload.hex()!=case['body_hex'] or framing.encode_root(group['role'],group['code'],case['criticality'],frame.payload)!=wire:raise ValueError('independent framing differs')
   pdu.from_aper(wire)
   if pdu.get_val()!=expected:raise ValueError('public generated -> native semantics differ')
   proof.append({'id':case['id'],'wire_sha256':sha(wire),'semantic_sha256':identity(expected),'octet_count':len(wire),'status':'PASS'})
  records.append({'message':e['message'],'role':e['role'],'code':e['code'],'case_count':len(proof),'cases':proof,'physical_checks':physical_counts[i],'translation_unit_sha256':tu_hashes[i],'status':'PASS'})
 if actual:raise ValueError('unrequested case responses')
 reference.verify_sources(repo,root,manifest_path)
 if fingerprints!={str(p.relative_to(repo)):sha(p.read_bytes()) for p in sources} or generated_hashes!={str(p.relative_to(generated)):sha(p.read_bytes()) for p in sorted(generated.rglob('*')) if p.is_file()}:raise ValueError('source/generated inputs changed during qualification')
 report={'status':'MATCHED_UNIFIED_CANDIDATE_REQUIRES_INDEPENDENT_REVIEW','source_authority':authority,'source_sha256':fingerprints,'generated_sha256':generated_hashes,'historical_accepted_profile_sha256':sha(golden_path.read_bytes()),'reference':{'pycrate_version':'0.7.11','native_oracle_sha256':sha((work/'native_oracle.py').read_bytes())},'executable_sha256':sha(executable.read_bytes()),'main_translation_unit_sha256':sha(mainpath.read_bytes()),'actual_output_sha256':sha(run.stdout.encode()),'strict_options':options,'compiler':subprocess.check_output(compiler+['--version'],text=True).splitlines()[0],'identities':records,'counts':{'messages':len(records),'cases':sum(r['case_count'] for r in records),'linked_executables':1,'registry_entries':131,'physical_and_resource_checks':sum(physical_counts.values())},'historical_case_wire_and_semantic_hashes_preserved':True,'unresolved_disagreements':0,'limitations':['Finite accepted values, not exhaustive NGAP value space or live vendor interoperability.','Embedded NAS/RRC opaque contents and conditional application rules are outside scope.','Private empty object set retains explicit opaque vendor payload profile.','Physical matrix uses one baseline per message; independent API tests cover unknown-root policies.']}
 if report['counts']['cases']!=3432:raise ValueError('accepted 3432-case closure changed')
 args.output.parent.mkdir(parents=True,exist_ok=True);args.output.write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report['counts'],indent=2))
if __name__=='__main__':main()
