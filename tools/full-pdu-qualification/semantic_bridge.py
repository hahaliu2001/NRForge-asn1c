"""Qualification-only native semantic value -> C++ typed object + independent assertions.
No wire parsing and no production renderer logic is used here.
"""
import re,json,pathlib

def blocks(text):
 result={}
 for match in re.finditer(r'\bstruct\s+(\w+)\s*\{',text):
  start=match.end(); depth=1; pos=start
  while depth and pos<len(text):
   if text[pos]=='{':depth+=1
   elif text[pos]=='}':depth-=1
   pos+=1
  result[match.group(1)]=text[start:pos-1]
 return result

def split_args(text):
 out=[];level=0;start=0
 for i,c in enumerate(text):
  if c=='<':level+=1
  elif c=='>':level-=1
  elif c==',' and level==0:out.append(text[start:i].strip());start=i+1
 out.append(text[start:].strip());return out

class Bridge:
 def __init__(self,types,mapping,codec):
  self.text=pathlib.Path(types).read_text();self.structs=blocks(self.text)
  self.maps=blocks(pathlib.Path(mapping).read_text());self.codec=pathlib.Path(codec).read_text()
  self.aliases=dict(re.findall(r'^using (\w+) = (.+);$',self.text,re.M))
  self.ns=re.search(r'namespace ([\w:]+) \{',self.text).group(1)
  self.lines=[];self.checks=[];self.serial=0
 def q(self,t):return t if t.startswith('::') else '::'+self.ns+'::'+t
 def bare(self,t):return t.split('::')[-1]
 def fields(self,t):
  body=self.structs[self.bare(t)]
  return [(a.strip(),b) for a,b in re.findall(r'(?:^|;)\s*([^;{}]+?)\s+(\w+)\{[^{}]*\};',body,re.M) if b!='sequence_extensions']
 def literal(self,value):
  if isinstance(value,bool):return 'true' if value else 'false'
  if isinstance(value,int):return ('UINT64_C(%d)'%value) if value>=0 else ('(-INT64_C(9223372036854775807) - INT64_C(1))' if value==-(1<<63) else ('(-INT64_C(%d))'%-value))
  if isinstance(value,str):
   raw=value.encode();return '::std::string('+''.join('"\\x%02x"'%b for b in raw)+', '+str(len(raw))+')' if raw else '::std::string{}'
  if isinstance(value,bytes):return '::std::vector<::std::byte>{'+','.join('::std::byte{0x%02x}'%b for b in value)+'}'
  raise ValueError(type(value))
 def assign_scalar(self,p,c,v):
  lit=self.literal(v);self.lines.append(f'{p} = {lit};');self.checks.append(f'REQUIRE({c} == {lit});')
 def walk(self,desc,value,typ,p,c):
  kind=desc.TYPE;bare=self.bare(typ)
  if kind in ('INTEGER','BOOLEAN','PrintableString','VisibleString','UTF8String','OCTET STRING'):self.assign_scalar(p,c,value);return
  if kind=='OBJECT IDENTIFIER':
   arcs=list(value);encoded=[]
   for component in [arcs[0]*40+arcs[1]]+arcs[2:]:
    groups=[component&127];component>>=7
    while component:groups.append(component&127);component>>=7
    encoded.extend(x|(128 if i else 0) for i,x in reversed(list(enumerate(groups))))
   self.assign_scalar(p,c,bytes(encoded));return
  if kind=='NULL':self.checks.append(f'REQUIRE((::std::is_same_v<::std::remove_cvref_t<decltype({c})>, ::std::monostate>));');return
  if kind=='BIT STRING':
   n,width=value;raw=(n<<(8-(width%8) if width%8 else 0)).to_bytes((width+7)//8,'big');self.lines.extend([f'{p}.octets = {self.literal(raw)};',f'{p}.bit_count = {width};']);self.checks.extend([f'REQUIRE({c}.octets == {self.literal(raw)});',f'REQUIRE({c}.bit_count == {width});']);return
  if kind=='ENUMERATED':
   number=desc._cont[value];body=self.structs[bare]
   enumerators=re.findall(r'(\w+)\s*=\s*(?:INT64_C\((-?\d+)\)|\(-INT64_C\((\d+)\)\))',body)
   label=next(a for a,b,d in enumerators if int(b or '-'+d)==number);lit=self.q(typ)+'::Known::'+label
   self.lines.append(f'{p}.value = {lit};')
   expr=f'::std::get<{self.q(typ)}::Known>({c}.value)' if 'variant<Known' in body else c+'.value';self.checks.append(f'REQUIRE({expr} == {lit});');return
  if kind=='SEQUENCE OF':
   fields=self.fields(typ);vt,member=next((a,b) for a,b in fields if b=='elements');elem=split_args(vt[vt.index('<')+1:-1])[0]
   self.lines.append(f'{p}.{member}.resize({len(value)});');self.checks.append(f'REQUIRE({c}.{member}.size() == {len(value)});')
   for i,v in enumerate(value):self.walk(desc._cont,v,elem,f'{p}.{member}[{i}]',f'{c}.{member}[{i}]')
   return
  if kind=='CHOICE':
   label,payload=value;ordinal=list(desc._cont).index(label);alt=split_args(self.aliases[bare][self.aliases[bare].index('<')+1:-1])[ordinal]
   self.serial+=1;local='alternative_'+str(self.serial);self.lines.append(f'{alt} {local}{{}};');self.checks.append(f'REQUIRE(::std::holds_alternative<{alt}>({c}));');payloadtype=self.fields(alt)[0][0]
   self.walk(desc._cont[label],payload,payloadtype,local+'.value',f'::std::get<{alt}>({c}).value');self.lines.append(f'{p} = ::std::move({local});');return
  if kind=='SEQUENCE':
   fields=self.fields(typ);keys=list(desc._cont)
   if set(value)-set(keys):raise ValueError(('unrepresented native sequence additions',bare,set(value)-set(keys)))
   if 'sequence_extensions' in self.structs[bare]:
    self.checks.extend([f'REQUIRE({c}.sequence_extensions.received_bitmap_bit_count == 0);',f'REQUIRE({c}.sequence_extensions.unknown_additions.empty());'])
   if len(fields)!=len(keys):raise ValueError(('field shape',bare,fields,keys))
   for name,(ft,member) in zip(keys,fields):
    fp=p+'.'+member;fc=c+'.'+member;optional=ft.startswith('::std::optional<')
    if optional:
     self.checks.append(f'REQUIRE({fc}.has_value() == {"true" if name in value else "false"});')
     if name not in value:continue
     ft=ft[len('::std::optional<'):-1];self.lines.append(f'{fp}.emplace();');fp='(*'+fp+')';fc='(*'+fc+')'
    if name not in value:raise ValueError(('mandatory missing',name))
    if desc._cont[name].TYPE=='OPEN_TYPE':self.open(desc._cont[name],value[name],value,typ,ft,fp,fc)
    else:self.walk(desc._cont[name],value[name],ft,fp,fc)
   return
  raise ValueError(('unsupported bridge kind',kind,typ))
 def open(self,desc,value,parent,typ,ft,p,c):
  # Table row order is explicitly exported in mapping; choose by identifier,
  # never by native payload spelling or generated variant index.
  if ft=='::std::vector<::std::byte>':self.assign_scalar(p,c,value[1]);return
  body=self.maps[self.bare(typ)+'_aper'];rowtext=re.search(r'rows\{\{(.*?)\}\};',body,re.S).group(1)
  ids=[int(x) for x in re.findall(r'\{(\d+),',rowtext)]
  if isinstance(value[0],str) and value[0].startswith('_unk_'):
   unknown=re.search(r'using unknown_type = (.*?);',body).group(1);self.lines.append(f'{p} = {unknown}{{{self.literal(value[1])}}};');self.checks.extend([f'REQUIRE(::std::holds_alternative<{unknown}>({c}));',f'REQUIRE(::std::get<{unknown}>({c}).payload == {self.literal(value[1])});']);return
  index=ids.index(parent['id'])
  wrapper=re.search(r'using wrapper_'+str(index)+r' = (.*?);',body).group(1)
  payloadtype=re.search(r'using payload_type_'+str(index)+r' = (.*?);',body).group(1)
  rows=desc._const_tab._val.root;row=next(x for x in rows if x['id']==parent['id']);native=row[desc._const_tab_id]
  self.serial+=1;local='open_'+str(self.serial);self.lines.append(f'{wrapper} {local}{{}};');self.checks.append(f'REQUIRE(::std::holds_alternative<{wrapper}>({c}));')
  self.walk(native,value[1],payloadtype,local+'.value',f'::std::get<{wrapper}>({c}).value');self.lines.append(f'{p} = ::std::move({local});')
 def emit(self,desc,value,typ,wire,include_prefix):
  self.walk(desc,value,typ,'expected','decoded.value()')
  q=self.q(typ)
  pattern=r'inline .*?\b(encode_\w+)\(const '+re.escape(q)+r'& v'
  encode=re.search(pattern,self.codec).group(1);decode='decode_'+encode[len('encode_'):]
  return '\n'.join(['#include "runtime.hpp"','#include "sequence_extensions.hpp"',f'#include "{include_prefix}_types.hpp"',f'#include "{include_prefix}_mapping.hpp"',f'#include "{include_prefix}_codec.hpp"','#include <cstdlib>','#include <iostream>','#define REQUIRE(...) do { if(!(__VA_ARGS__)) { ::std::cerr << "semantic failure " << __LINE__ << "\\n"; ::std::abort(); } } while(0)','int main() {',q+' expected{};']+self.lines+['const auto native_wire = '+self.literal(bytes.fromhex(wire))+';',f'auto decoded = ::{self.ns}::{decode}(native_wire);','REQUIRE(decoded);']+self.checks+[f'auto encoded = ::{self.ns}::{encode}(expected);','REQUIRE(encoded);','REQUIRE(encoded.value().octets == native_wire);','::std::cout << "PASS independent native semantic bridge\\n";','}'])

 def reset(self):
  self.lines=[];self.checks=[];self.serial=0

 def emit_pdu(self,desc,cases,cpp_body_type,body_prefix,envelope_types,envelope_mapping,envelope_codec,envelope_prefix):
  """cases: id/value/pdu_hex/procedure/root/criticality. No native wire mutation."""
  mappingblocks=blocks(pathlib.Path(envelope_mapping).read_text())
  target=[(name,body) for name,body in mappingblocks.items() if re.search(r'using body_type = '+re.escape(self.q(cpp_body_type))+r';',body)]
  if len(target)!=1:raise ValueError(('unique envelope target mapping',target))
  mapname,mapbody=target[0];pdu_type=re.search(r'using value_type = (.*?);',mapbody).group(1)
  envcodec=pathlib.Path(envelope_codec).read_text()
  encode=re.search(r'inline .*?\b(encode_\w+)\(const '+re.escape(pdu_type)+r'& v',envcodec).group(1);decode='decode_'+encode[7:]
  ordinal=int(re.search(r'target_root_ordinal = (\d+)',mapbody).group(1))
  functions=[]
  for n,case in enumerate(cases):
   self.reset();self.walk(desc,case['value'],cpp_body_type,'body','actual_body')
   statements=list(self.lines);checks=list(self.checks)
   policy=case.get('criticality',case.get('actualcriticality'))
   independent_role={'initiatingMessage':0,'successfulOutcome':1,'unsuccessfulOutcome':2}[case['root']]
   wire=self.literal(bytes.fromhex(case['pdu_hex']));caseid=str(case.get('id',n))
   functions += [f'static void case_{n}() {{',f'using Mapping = {self.q(mapname)};',f'using Root = Mapping::wrapper_{ordinal};',f'{self.q(cpp_body_type)} body{{}};']+statements+[
    f'static_assert(Mapping::root_roles[{ordinal}] == {independent_role});',
    f'static_assert(Mapping::target_code == {case["procedure"]});',
    'Root root{};',f'root.*Mapping::root_{ordinal}_procedure_member = {case["procedure"]};',
    f'(root.*Mapping::root_{ordinal}_criticality_member).value = Mapping::criticality_type::Known::{policy};',
    f'root.*Mapping::root_{ordinal}_value_member = Mapping::target_wrapper_type{{::std::move(body)}};',
    'Mapping::value_type expected{::std::move(root)};',f'const auto native_wire = {wire};',
    f'auto decoded = ::{self.ns}::{decode}(native_wire);','REQUIRE(decoded);',
    'REQUIRE(::std::holds_alternative<Root>(decoded.value().value));',
    'const auto& actual_root = ::std::get<Root>(decoded.value().value);',
    f'REQUIRE(actual_root.*Mapping::root_{ordinal}_procedure_member == {case["procedure"]});',
    f'REQUIRE((actual_root.*Mapping::root_{ordinal}_criticality_member).value == Mapping::criticality_type::Known::{policy});',
    f'const auto& actual_payload = actual_root.*Mapping::root_{ordinal}_value_member;',
    'REQUIRE(::std::holds_alternative<Mapping::target_wrapper_type>(actual_payload));',
    'const auto& actual_body = ::std::get<Mapping::target_wrapper_type>(actual_payload).value;']+checks+[
    f'auto encoded = ::{self.ns}::{encode}(expected);','REQUIRE(encoded);','REQUIRE(encoded.value().octets.size() == native_wire.size());','REQUIRE(encoded.value().octets == native_wire);',
    f'::std::cout << "CASE {caseid} ";', 'for(auto octet:encoded.value().octets) { const auto v=::std::to_integer<unsigned>(octet); ::std::cout << "0123456789abcdef"[v>>4] << "0123456789abcdef"[v&15]; }','::std::cout << "\\n";','}']
  includes=['#include "runtime.hpp"','#include "sequence_extensions.hpp"']+[f'#include "{prefix}_{family}.hpp"' for prefix in (body_prefix,envelope_prefix) for family in ('types','mapping','codec')]+['#include <cstdlib>','#include <iostream>','#include <type_traits>','#define REQUIRE(...) do { if(!(__VA_ARGS__)) { ::std::cerr << "semantic failure " << __LINE__ << "\\n"; ::std::abort(); } } while(0)']
  return '\n'.join(includes+functions+['int main() {']+[f'case_{n}();' for n in range(len(cases))]+['}'])
