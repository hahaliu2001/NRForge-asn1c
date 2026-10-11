"""Independent bounded E1AP-PDU framing model, not a BODY codec.

All byte math here is independent of generated/runtime implementations. A full
semantic qualification case must ALSO compare actual native body/PDU encoding
and native decoded field values; passing this module alone is framing evidence.
"""
from dataclasses import dataclass

ROLES = ('initiatingMessage', 'successfulOutcome', 'unsuccessfulOutcome')
POLICIES = ('reject', 'ignore', 'notify')

class FramingError(ValueError):
    def __init__(self, code, bit_offset):
        self.code, self.bit_offset = code, bit_offset
        super().__init__(f'{code}@{bit_offset}')

@dataclass(frozen=True)
class Segment:
    determinant_octet: int
    payload_octet: int
    payload_octets: int
    fragmented: bool

@dataclass(frozen=True)
class RootPDU:
    role: str
    procedure_code: int
    criticality: str
    payload: bytes
    segments: tuple[Segment, ...]
    end_octet: int
    open_primitive_start_bit: int = 18
    def payload_bit_to_wire(self, bit):
        if not 0 <= bit < len(self.payload)*8:
            raise ValueError('payload bit outside owned contents')
        for s in self.segments:
            if bit < s.payload_octets*8:
                return s.payload_octet*8+bit
            bit -= s.payload_octets*8
        raise AssertionError('unreachable mapping')

def encode_open(payload):
    if not payload: raise ValueError('complete open payload must be nonempty')
    out=bytearray(); offset=0
    while len(payload)-offset >= 16384:
        count=min(4,(len(payload)-offset)//16384)*16384
        out.append(192+count//16384);out.extend(payload[offset:offset+count]);offset+=count
    tail=len(payload)-offset
    if tail < 128: out.append(tail)
    else: out.extend((128+(tail>>8),tail&255))
    out.extend(payload[offset:]);return bytes(out)

def encode_root(role, code, policy, payload):
    if role not in ROLES or policy not in POLICIES or not 0 <= code <= 255:
        raise ValueError('invalid root metadata')
    return bytes((ROLES.index(role)<<5,code,POLICIES.index(policy)<<6))+encode_open(payload)

def parse_open(wire, offset=3, primitive_start_bit=18):
    segments=[];chunks=[];start=offset
    while True:
        determinant=offset
        if offset >= len(wire): raise FramingError('truncated_input',len(wire)*8)
        first=wire[offset];offset+=1
        if first<128:count=first;more=False
        elif first<192:
            if offset>=len(wire):raise FramingError('truncated_input',len(wire)*8)
            count=((first&63)<<8)|wire[offset];offset+=1;more=False
            if count<128:raise FramingError('constraint_violation',determinant*8)
        else:
            units=first&63
            if not 1<=units<=4:raise FramingError('constraint_violation',determinant*8)
            count=units*16384;more=True
        if count>len(wire)-offset:raise FramingError('truncated_input',len(wire)*8)
        segments.append(Segment(determinant,offset,count,more))
        chunks.append(wire[offset:offset+count]);offset+=count
        if not more:break
    payload=b''.join(chunks)
    if not payload:raise FramingError('constraint_violation',primitive_start_bit)
    remaining=len(payload)
    for s in segments:
        expected=min(4,remaining//16384)*16384 if remaining>=16384 else remaining
        if s.payload_octets != expected or s.fragmented != (remaining>=16384):
            raise FramingError('constraint_violation',s.determinant_octet*8)
        remaining-=s.payload_octets
    assert remaining==0 and start==segments[0].determinant_octet
    return payload,tuple(segments),offset

def parse_root(wire):
    if not wire:raise FramingError('truncated_input',0)
    if wire[0]&128:raise ValueError('outer extension belongs to separate opaque-extension campaign')
    selector=(wire[0]>>5)&3
    if selector==3:raise FramingError('constraint_violation',0)
    # A code read atomically checks availability before validating alignment.
    if len(wire)<2:raise FramingError('truncated_input',len(wire)*8)
    for bit in range(3,8):
        if wire[0]&(128>>bit):raise FramingError('nonzero_padding',bit)
    if len(wire)<3:raise FramingError('truncated_input',len(wire)*8)
    policy=wire[2]>>6
    if policy==3:raise FramingError('constraint_violation',16)
    # Full open availability precedes padding; mutations below use complete PDU.
    payload,segments,end=parse_open(wire)
    for bit in range(18,24):
        if wire[2]&(128>>(bit%8)):raise FramingError('nonzero_padding',bit)
    if end!=len(wire):raise FramingError('trailing_data',end*8)
    return RootPDU(ROLES[selector],wire[1],POLICIES[policy],payload,segments,end)

def root_error_cases(wire, all_prefixes=True):
    """Mutations with independently known offsets, for nonempty valid root PDU.

Body-inner padding/constraints require schema-aware field-bit locations and
are deliberately not guessed from the wire. Output excludes budget errors,
whose encoded child staging may legitimately identify an earlier anchor.
"""
    parsed=parse_root(wire);rows=[]
    cuts=range(len(wire)) if all_prefixes else sorted({0,1,2,3,len(wire)//2,len(wire)-1})
    for cut in cuts:rows.append((f'short-{cut}',wire[:cut],'truncated_input',cut*8))
    for bit in list(range(3,8))+list(range(18,24)):
        changed=bytearray(wire);changed[bit//8]|=128>>(bit%8)
        rows.append((f'padding-{bit}',bytes(changed),'nonzero_padding',bit))
    changed=bytearray(wire);changed[0]=96
    rows.append(('selector-spare',bytes(changed),'constraint_violation',0))
    changed=bytearray(wire);changed[2]=192
    rows.append(('criticality-spare',bytes(changed),'constraint_violation',16))
    for suffix in (b'\0',b'\xff',b'\0\xff'):
        rows.append((f'trailing-{suffix.hex()}',wire+suffix,'trailing_data',len(wire)*8))
    for marker in (192,197,255):
        rows.append((f'reserved-fragment-{marker}',wire[:3]+bytes((marker,)),'constraint_violation',24))
    rows.append(('zero-open',wire[:3]+b'\0','constraint_violation',18))
    if len(parsed.payload)<128:
        rows.append(('overlong-open',wire[:3]+bytes((128,len(parsed.payload)))+parsed.payload,'constraint_violation',24))
    return rows

def verify_registry(native, source_inventory):
    table=native.E1AP_PDU_Descriptions.E1AP_ELEMENTARY_PROCEDURES.get_val()
    if table.ext:raise ValueError('unexpected procedure addition rows')
    role_keys=('InitiatingMessage','SuccessfulOutcome','UnsuccessfulOutcome')
    result=[]
    for entry in table.root:
        for role,key in zip(ROLES,role_keys):
            if key in entry:
                module,name=entry[key]._typeref.called
                if module!='E1AP-PDU-Contents':raise ValueError('body module changed')
                result.append((role,entry['procedureCode'],name,entry['criticality']))
    spelling={'INITIATING MESSAGE':ROLES[0],'SUCCESSFUL OUTCOME':ROLES[1],'UNSUCCESSFUL OUTCOME':ROLES[2]}
    source=[(spelling[r['role']],r['procedure_code'],r['message'],r['expected_criticality']) for r in source_inventory]
    if len(result)!=72 or len(table.root)!=40 or sorted(source)!=sorted(result):
        raise ValueError('native and frozen-text procedure registry disagree')
    return tuple(sorted(result))

if __name__=='__main__':
    for role in ROLES:
      for policy in POLICIES:
       for n in (1,127,128,16383,16384,32768,65535,65536,65537,131072):
        body=bytes(i%251 for i in range(n));wire=encode_root(role,80,policy,body);p=parse_root(wire)
        assert p.payload==body and p.role==role and p.criticality==policy
        assert all(wire[p.payload_bit_to_wire(i)//8]==body[i//8] for i in (0,n*8-1))
        for _,bad,code,offset in root_error_cases(wire,False):
          try:parse_root(bad)
          except FramingError as e:assert (e.code,e.bit_offset)==(code,offset),(e,code,offset)
          else:raise AssertionError('accepted malformed envelope')
    print('PASS independent root framing and error model self-checks')


def opaque_slots(registry):
    known={(role,code) for role,code,*_ in registry};codes={code for _,code in known}
    if len(known)!=72 or len(codes)!=40:raise ValueError('incomplete E1AP registry')
    unknown=tuple((role,code) for role in ROLES for code in range(256) if code not in codes)
    absent=tuple((role,code) for role in ROLES for code in sorted(codes) if (role,code) not in known)
    if len(unknown)!=648 or len(absent)!=48:raise ValueError('opaque slot closure differs')
    return unknown,absent
