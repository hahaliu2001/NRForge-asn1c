"""Independent bounded F1AP-PDU framing model; deliberately not a BODY codec.

Frozen F1AP has four non-extensible CHOICE roots: a two-bit selector, not
NGAP's extension bit plus selector. The fourth root is a SingleContainer,
not a procedure header. Its unsupported policy rejects at bit 2. Root message
headers align the constrained 0..255 procedure code to an octet; criticality
uses two bits and the open type starts at physical bit 18 before alignment.
Native semantic encoding/decoding must separately establish BODY evidence.
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
        if not 0 <= bit < len(self.payload) * 8:
            raise ValueError('payload bit outside owned contents')
        for segment in self.segments:
            if bit < segment.payload_octets * 8:
                return segment.payload_octet * 8 + bit
            bit -= segment.payload_octets * 8
        raise AssertionError('unreachable payload location')


def encode_open(payload):
    if not payload:
        raise ValueError('complete open payload must be nonempty')
    out = bytearray()
    offset = 0
    while len(payload) - offset >= 16384:
        count = min(4, (len(payload) - offset) // 16384) * 16384
        out.append(192 + count // 16384)
        out.extend(payload[offset:offset + count])
        offset += count
    tail = len(payload) - offset
    if tail < 128:
        out.append(tail)
    else:
        out.extend((128 + (tail >> 8), tail & 255))
    out.extend(payload[offset:])
    return bytes(out)


def encode_root(role, code, policy, payload):
    if role not in ROLES or policy not in POLICIES or not 0 <= code <= 255:
        raise ValueError('invalid root metadata')
    return bytes((ROLES.index(role) << 6, code, POLICIES.index(policy) << 6)) + encode_open(payload)


def parse_open(wire, offset=3, primitive_start_bit=18):
    segments, chunks = [], []
    while True:
        determinant = offset
        if offset >= len(wire):
            raise FramingError('truncated_input', len(wire) * 8)
        first = wire[offset]
        offset += 1
        if first < 128:
            count, more = first, False
        elif first < 192:
            if offset >= len(wire):
                raise FramingError('truncated_input', len(wire) * 8)
            count = ((first & 63) << 8) | wire[offset]
            offset += 1
            more = False
            if count < 128:
                raise FramingError('constraint_violation', determinant * 8)
        else:
            units = first & 63
            if not 1 <= units <= 4:
                raise FramingError('constraint_violation', determinant * 8)
            count, more = units * 16384, True
        if count > len(wire) - offset:
            raise FramingError('truncated_input', len(wire) * 8)
        segments.append(Segment(determinant, offset, count, more))
        chunks.append(wire[offset:offset + count])
        offset += count
        if not more:
            break
    payload = b''.join(chunks)
    if not payload:
        raise FramingError('constraint_violation', primitive_start_bit)
    remaining = len(payload)
    for segment in segments:
        expected = min(4, remaining // 16384) * 16384 if remaining >= 16384 else remaining
        if segment.payload_octets != expected or segment.fragmented != (remaining >= 16384):
            raise FramingError('constraint_violation', segment.determinant_octet * 8)
        remaining -= segment.payload_octets
    assert remaining == 0
    return payload, tuple(segments), offset


def parse_root(wire):
    if not wire:
        raise FramingError('truncated_input', 0)
    selector = wire[0] >> 6
    if selector == 3:
        raise FramingError('constraint_violation', 2)
    # Procedure-code availability is checked before alignment padding.
    if len(wire) < 2:
        raise FramingError('truncated_input', len(wire) * 8)
    for bit in range(2, 8):
        if wire[0] & (128 >> bit):
            raise FramingError('nonzero_padding', bit)
    if len(wire) < 3:
        raise FramingError('truncated_input', len(wire) * 8)
    policy = wire[2] >> 6
    if policy == 3:
        raise FramingError('constraint_violation', 16)
    # Complete open availability precedes its alignment validation.
    payload, segments, end = parse_open(wire)
    for bit in range(18, 24):
        if wire[2] & (128 >> (bit % 8)):
            raise FramingError('nonzero_padding', bit)
    if end != len(wire):
        raise FramingError('trailing_data', end * 8)
    return RootPDU(ROLES[selector], wire[1], POLICIES[policy], payload, segments, end)


def root_error_cases(wire, all_prefixes=True):
    """Physical mutations with exact offsets; not guessed BODY constraints.

    Budget failures are separate cases because child staging can use an
    earlier physical anchor. The fourth root is unsupported, not a spare
    selector or an outer extension addition.
    """
    parsed = parse_root(wire)
    cuts = range(len(wire)) if all_prefixes else sorted({0, 1, 2, 3, len(wire) // 2, len(wire) - 1})
    rows = [(f'short-{cut}', wire[:cut], 'truncated_input', cut * 8) for cut in cuts]
    for bit in (*range(2, 8), *range(18, 24)):
        changed = bytearray(wire)
        changed[bit // 8] |= 128 >> (bit % 8)
        rows.append((f'padding-{bit}', bytes(changed), 'nonzero_padding', bit))
    rows.append(('selector-fourth', b'\xc0' + wire[1:], 'constraint_violation', 2))
    rows.append(('selector-fourth-no-header', b'\xc0', 'constraint_violation', 2))
    changed = bytearray(wire)
    changed[2] = 192
    rows.append(('criticality-spare', bytes(changed), 'constraint_violation', 16))
    for suffix in (b'\0', b'\xff', b'\0\xff'):
        rows.append((f'trailing-{suffix.hex()}', wire + suffix, 'trailing_data', len(wire) * 8))
    for marker in (192, 197, 255):
        rows.append((f'reserved-fragment-{marker}', wire[:3] + bytes((marker,)), 'constraint_violation', 24))
    rows.append(('zero-open', wire[:3] + b'\0', 'constraint_violation', 18))
    if len(parsed.payload) < 128:
        rows.append(('overlong-open', wire[:3] + bytes((128, len(parsed.payload))) + parsed.payload, 'constraint_violation', 24))
    return rows


def verify_registry(native, source_inventory):
    pdu = native.F1AP_PDU_Descriptions.F1AP_PDU
    if tuple(pdu._root) != (*ROLES, 'choice-extension') or pdu._ext is not None:
        raise ValueError('native four-root non-extensible CHOICE disagrees')
    table = native.F1AP_PDU_Descriptions.F1AP_ELEMENTARY_PROCEDURES.get_val()
    if table.ext:
        raise ValueError('unexpected procedure addition rows')
    keys = ('InitiatingMessage', 'SuccessfulOutcome', 'UnsuccessfulOutcome')
    result = []
    for entry in table.root:
        for role, key in zip(ROLES, keys):
            if key in entry:
                module, name = entry[key]._typeref.called
                if module != 'F1AP-PDU-Contents':
                    raise ValueError('body module changed')
                result.append((role, entry['procedureCode'], name, entry['criticality']))
    spelling = dict(zip(('INITIATING MESSAGE', 'SUCCESSFUL OUTCOME', 'UNSUCCESSFUL OUTCOME'), ROLES))
    source = [(spelling[row['role']], row['procedure_code'], row['message'], row['expected_criticality']) for row in source_inventory]
    if len(result) != 158 or len(table.root) != 94 or len(set(result)) != 158 or sorted(source) != sorted(result):
        raise ValueError('native and frozen-text procedure registry disagree')
    if tuple(sum(row[0] == role for row in result) for role in ROLES) != (94, 36, 28):
        raise ValueError('native outcome counts disagree')
    return tuple(sorted(result))


def opaque_slots(registry):
    """Exhaustive actual unknown codes and absent known outcomes (receive only)."""
    known = {(role, code) for role, code, *_ in registry}
    codes = {code for _, code in known}
    if len(known) != 158 or len(codes) != 94:
        raise ValueError('incomplete F1AP registry')
    unknown = tuple((role, code) for role in ROLES for code in range(256) if code not in codes)
    absent = tuple((role, code) for role in ROLES for code in sorted(codes) if (role, code) not in known)
    assert len(unknown) == 486 and len(absent) == 124
    return unknown, absent


if __name__ == '__main__':
    count = 0
    for role in ROLES:
        for policy in POLICIES:
            for size in (1, 127, 128, 16383, 16384, 32768, 65535, 65536, 65537, 131072):
                body = bytes(i % 251 for i in range(size))
                wire = encode_root(role, 80, policy, body)
                parsed = parse_root(wire)
                assert (parsed.payload, parsed.role, parsed.criticality) == (body, role, policy)
                assert all(wire[parsed.payload_bit_to_wire(i) // 8] == body[i // 8] for i in (0, size * 8 - 1))
                for _, bad, code, offset in root_error_cases(wire, False):
                    try:
                        parse_root(bad)
                    except FramingError as error:
                        assert (error.code, error.bit_offset) == (code, offset), (error, code, offset)
                    else:
                        raise AssertionError('accepted malformed envelope')
                    count += 1
    print(f'PASS independent F1AP root framing: 90 boundary roots, {count} error mutations')
