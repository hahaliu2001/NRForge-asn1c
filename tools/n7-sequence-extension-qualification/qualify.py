import argparse, hashlib, importlib.metadata, importlib.util, json, shlex, subprocess, tempfile
from pathlib import Path
parser=argparse.ArgumentParser(description="Native APER qualification with explicit oracle discrepancy accounting.")
parser.add_argument("--repo",type=Path,default=Path(__file__).resolve().parents[2])
parser.add_argument("--build",type=Path,help="Configured/built source or VPATH build directory; defaults to repo.")
parser.add_argument("--generated",type=Path,help="Optional existing extensiontest types/mapping/codec header directory.")
parser.add_argument("--cxx",default="c++")
parser.add_argument("--output",type=Path,required=True,help="JSON result path; no large raw vector files are persisted.")
args=parser.parse_args()
base=args.repo.resolve();build=(args.build or base).resolve()
for package,version in (("pycrate","0.7.11"),("asn1tools","0.167.0")):
 if importlib.metadata.version(package)!=version:raise RuntimeError(f"{package} requires version {version}; reassess oracle discrepancies before upgrading")
temp=tempfile.TemporaryDirectory(prefix="n7-native-");here=Path(temp.name)
generated=args.generated.resolve() if args.generated else here/"generated"
if not args.generated:
 generated.mkdir()
 subprocess.run([str(build/"libasn1typed/check_asn1typed_sequence_extension_render"),str(base/"libasn1typed/fixtures/sequence-extension-generation-n7.asn1"),"SequenceExtensionGeneration","extensiontest",str(generated)],check=True)
subprocess.run(shlex.split(args.cxx)+["-std=c++20","-Wall","-Wextra","-Werror","-pedantic-errors","-Wconversion","-Wsign-conversion","-I"+str(base/"libaper"),"-I"+str(generated),str(Path(__file__).with_name("driver.cpp")),str(base/"libaper/runtime.cpp"),"-o",str(here/"driver")],check=True)
from pycrate_asn1c.asnproc import compile_text
from pycrate_asn1c.generator import PycrateGenerator
import asn1tools
sender=base/"libasn1typed/fixtures/sequence-extension-sender-n7.asn1"
compile_text(sender.read_text());PycrateGenerator(str(here/"oracle.py"))
spec=importlib.util.spec_from_file_location("n7_native_oracle",here/"oracle.py");m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m);S=m.SequenceExtensionSender
secondary=asn1tools.compile_files(str(sender),"per")

def bitflag(v,k):return str(int(v[k])) if k in v else '-1'
def extension(obj,v):
 present=[(i,k) for i,k in enumerate(obj._ext_nest) if k in v]
 unknown=sorted((int(k[5:])-1,k) for k in v if k.startswith('_ext_'))
 if not present and not unknown:return '0'
 width=max([len(obj._ext_nest)]+[i+1 for i,k in unknown]);s=str(width)
 for i,k in present:
  c=obj._cont[k];c.set_val(v[k]);s+=','+str(i)+'='+c.to_aper().hex()
 for i,k in unknown:s+=','+str(i)+'='+v[k].hex()
 return s

def show(t,v):
 if t=='Inner':return 'I:'+str(v['value'])+':'+bitflag(v,'flag')+':'+extension(S.Inner,v)
 if t=='Outer':return show('Inner',v['inner'])+'|O:'+bitflag(v,'flag')+':'+extension(S.Outer,v)
 if t=='Empty':return 'X:'+extension(S.Empty,v)
 if t=='Collision':return 'C:'+str(int(v['sequence-extensions']))+':'+bitflag(v,'sequence-extensions-1')+':'+extension(S.Collision,v)
 if t=='Envelope':return ('P:'+str(int(v['pick'][1])) if v['pick'][0]=='flag' else show('Inner',v['pick'][1]))+('|' + show('Outer',v['outer']) if 'outer'in v else '|-')
 raise AssertionError(t)

def addflag(v,f,k='flag'):
 if f>=0:v[k]=bool(f)
 return v
values=[0,1,255,256,65535,65536,4294967295]
patterns=[{}, {'extraFlag':False},{'extraFlag':True},{'extraWord':0},{'extraWord':4294967295},{'extraBytes':b''},{'extraBytes':b'\x00\xff\x80'},{'extraFlag':True,'extraWord':258,'extraBytes':b'\x01\x02'}]
cases=[]
for value in values:
 for f in (-1,0,1):
  for p in patterns:cases.append(('Inner',dict(addflag({'value':value},f),**p)))
for n in range(96):
 inner=dict(addflag({'value':values[n%7]},(-1,0,1)[n%3]),**patterns[n%8]);outer=dict(addflag({'inner':inner},(-1,0,1)[(n//3)%3]),**patterns[(n//8)%8]);cases.append(('Outer',outer))
for n in range(48):
 inner=dict(addflag({'value':values[n%7]},(-1,0,1)[n%3]),**patterns[n%8]);v={'pick':('inner',inner) if n%2 else ('flag',bool(n%3))}
 if n%3:v['outer']=dict(addflag({'inner':inner},n%2),**patterns[(n//8)%8])
 cases.append(('Envelope',v))
for p in ({},{'extraFlag':False},{'extraFlag':True}):cases.append(('Empty',p))
for f in (-1,0,1):
 for p in ({},{'extraFlag':True}):cases.append(('Collision',dict(addflag({'sequence-extensions':True},f,'sequence-extensions-1'),**p)))
for n in (127,128,16384,32768):cases.append(('Inner',{'value':258,'extraBytes':bytes(i%256 for i in range(n))}))
for i in (64,65,255):cases.append(('Inner',{'value':1,'_ext_'+str(i):b'\xaa\x00'}))
def native_decode_checks(oracle, subset):
 requests=[];expects=[];wires=[]
 for t,v in subset:
  if oracle=="pycrate":
   o=getattr(S,t);o.set_val(v);wire=o.to_aper()
  else:wire=secondary.encode(t,v)
  requests.append("D "+t+" "+wire.hex()+"\n");expects.append(show(t,v));wires.append(wire)
 p=subprocess.run([str(here/"driver")],input="".join(requests),text=True,capture_output=True,check=True)
 lines=p.stdout.splitlines()
 if len(lines)!=len(expects):raise RuntimeError("Driver response count mismatch")
 matches=[];mismatches=[]
 for i,(line,expected) in enumerate(zip(lines,expects)):
  if line==expected:matches.append(i)
  else:mismatches.append({"case":i,"type":subset[i][0],"octet_count":len(wires[i]),"sha256":hashlib.sha256(wires[i]).hexdigest(),"actual":line})
 return matches,mismatches
py_matches,py_mismatches=native_decode_checks("pycrate",cases)
declared=[(t,v) for t,v in cases if not any(k.startswith("_ext_") for k in v)]
asn_matches,asn_mismatches=native_decode_checks("asn1tools",declared)
# Encode root-only values, checking exact bytes and native decode fields in both engines.
enc=[];vals=[]
for t in ("Inner","Outer","Empty","Collision"):
 for value in values:
  for a in (-1,0,1):
   for z in (-1,0,1):
    if t=="Inner":v=addflag({"value":value},a)
    elif t=="Outer":v=addflag({"inner":addflag({"value":value},a)},z)
    elif t=="Empty":v={}
    else:v=addflag({"sequence-extensions":a!=0},z,"sequence-extensions-1")
    enc.append(f"E {t} {value} {a} {z}\n");vals.append((t,v))
p=subprocess.run([str(here/"driver")],input="".join(enc),text=True,capture_output=True,check=True)
lines=p.stdout.splitlines()
if len(lines)!=len(vals):raise RuntimeError("Encode response count mismatch")
for wire,(t,v) in zip(lines,vals):
 o=getattr(S,t);o.set_val(v)
 if o.to_aper().hex()!=wire:raise RuntimeError(("pycrate encode mismatch",t,v,wire))
 o.from_aper(bytes.fromhex(wire))
 if o.get_val()!=v:raise RuntimeError(("pycrate decode mismatch",t,v))
 if secondary.encode(t,v).hex()!=wire or secondary.decode(t,bytes.fromhex(wire))!=v:raise RuntimeError(("asn1tools root mismatch",t,v))
covered=set(py_matches)|set(asn_matches)
expected=(len(cases),len(py_matches),len(py_mismatches),len(declared),len(asn_matches),len(asn_mismatches),len(covered),len(vals))
if expected!=(328,116,212,325,323,2,326,252):raise RuntimeError(("Oracle discrepancy profile changed; investigate rather than normalize",expected))
profile={"pycrate":{"matched_case_ids":py_matches,"mismatches":py_mismatches},"asn1tools":{"matched_case_ids":asn_matches,"mismatches":asn_mismatches}}
accepted=json.loads(Path(__file__).with_name("accepted-profile.json").read_text())
unresolved=int(profile!=accepted)
if unresolved:
 args.output.parent.mkdir(parents=True,exist_ok=True)
 args.output.write_text(json.dumps({"production_disagreements_unresolved":unresolved,"observed_profile":profile},indent=2)+"\n")
 raise RuntimeError("Exact oracle case/signature profile changed; investigate the written observations before accepting")
result={"native_receiver_distinct_cases":328,"qualified_native_receiver_union":326,"native_large_bitmap_unqualified_cases":[326,327],"root_encode_cases_each_oracle":252,"pycrate":{"version":"0.7.11","attempted":328,"matched":116,"mismatched":212},"asn1tools":{"version":"0.167.0","attempted":325,"matched":323,"mismatched":2},"byte_normalization":False,"production_disagreements_unresolved":unresolved,"limitations":["Not an all-case zero-difference claim; see README for reproduced oracle defects.","Widths65/255 have normative/manual evidence, not native oracle qualification.","Not whole NGAP qualification; no opaque extension encoding."],"pycrate_mismatches":py_mismatches,"asn1tools_mismatches":asn_mismatches}
args.output.parent.mkdir(parents=True,exist_ok=True)
args.output.write_text(json.dumps(result,indent=2)+"\n")
print(json.dumps({k:v for k,v in result.items() if not k.endswith("mismatches")}))
