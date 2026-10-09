# N1 — Zero-Based Constrained INTEGER Runtime Contract

2026-10-09. **Owner Approved / Accepted: N1-01 through N1-05.** Parent scope is the approved
UEContextReleaseCommand complete-PDU study, commit
`5fc0fc70ac3262a40d04e5fb97a5691c1596845c`.
This task freezes one runtime primitive family; no ENUMERATED, generator,
collections, dispatch or real-message implementation is included.

## Supported domains and API proposal

Only non-extensible, zero-based complete root domains `(0..2^root_bits-1)`
with root_bits **8, 16, 32 or 40** are supported. Domain width describes the
constraint, not a fixed payload width. Signed/nonzero-lower-bound, other widths,
unbounded and extensible INTEGERs remain outside this primitive's contract.

Add these operations to BitWriter/BitReader and forward them through Field views:

```cpp
Result<void> write_constrained_uint(std::uint64_t value, unsigned root_bits);
Result<std::uint64_t> read_constrained_uint(unsigned root_bits);
```

No caller-provided payload buffer or new context/finish API is needed. Existing
write/read_aligned_u16_be retain their signature and behavior. The 16-bit domain
must have identical wire and error semantics; delegation is permitted. Caller
schema-to-domain selection belongs to a later mapping task and must reject any
constraint not exactly represented by these approved domains.

## Wire contract

Normative source: [ITU-T X.691 (02/2021)](https://www.itu.int/rec/dologin_pub.asp?id=T-REC-X.691-202102-I!!PDF-E&lang=e&type=items),
11.3, 11.5.7.2–11.5.7.4 and 13.2.6(a).

| Domain | Field layout |
| --- | --- |
| 8 | zero padding to octet boundary, one big-endian payload octet; no length |
| 16 | zero padding to octet boundary, two big-endian payload octets; no length |
| 32 | two unaligned bits for `payload_octets - 1` (1..4), then zero octet alignment, then minimal unsigned big-endian payload |
| 40 | three unaligned bits for `payload_octets - 1` (1..5), then zero octet alignment, then minimal unsigned big-endian payload |

Zero uses one payload octet in the 32/40 domains. Leading zero octets are not
emitted. The 40-bit length selectors representing 6..8 octets are invalid.
The length prefix starts at the current cursor; do not align before it. Padding
depends on that cursor plus prefix width. Complete final padding remains the
existing outermost wrapper's responsibility.

## State, errors and atomicity

One call includes its length prefix, alignment and payload as a single atomic
primitive. Reuse S3's input/output/wire budgets and lifecycle rules. All bits,
including length and alignment, count toward wire budget; output uses projected
logical octet length. No extra allocation or element-count budget is introduced.

1. Check live state first: failed replays the first Error; finished/moved-from
   returns invalid_state without modifying state, including for invalid arguments.
2. Healthy objects reject unsupported root_bits as invalid_argument at field
   start and record the sticky error. Validate width before shifts/arithmetic.
3. Encoding rejects an out-of-domain uint64 value as constraint_violation at
   field start, before narrowing or publishing any bit. Preflight all bits and
   octets, then reserve output before publication.
4. Decoding peeks a variable-length prefix without advancing state. Missing
   prefix reports truncated_input at logical input end. An invalid length
   selector reports constraint_violation at field start. For a valid length,
   preflight whole-field availability, then whole-field wire budget, then check
   alignment, then decode and check the payload.
5. A non-minimal 32/40-bit payload (multiple octets with a zero leading octet)
   is rejected as constraint_violation at field start. This is an explicit strict
   decoder decision; external permissiveness does not override it.
6. On failure, cursor, wire counters and output contents remain unchanged; only
   healthy context failure/first-error state changes. Truncation offset is the
   logical input end; nonzero alignment offset is its first nonzero bit; budget
   and allocation errors use field start. Wrappers preserve the sticky error.

Bad length/domain and non-minimal-payload rejection use existing ErrorCode values;
no Result, ErrorCode, Limits or complete-wrapper change is proposed.

## Independent study vectors

At initial cursor zero, complete bytes independently derived from the layout
above and checked against asn1tools 0.167.0 aligned PER in an isolated synthetic
study schema (not a modification of the frozen NGAP source):

| Value | 32-bit domain | 40-bit domain |
| ---: | --- | --- |
| 0 | `0000` | `0000` |
| 255 | `00ff` | `00ff` |
| 256 | `400100` | `200100` |
| 65535 | `40ffff` | `20ffff` |
| 65536 | `80010000` | `40010000` |
| 16777216 | `c001000000` | `6001000000` |
| 4294967295 | `c0ffffffff` | `60ffffffff` |
| 4294967296 | out of domain | `800100000000` |
| 1099511627775 | out of domain | `80ffffffffff` |

8-bit endpoints are `00` / `ff`; 16-bit endpoints are `0000` / `ffff`.
These are study evidence, not implementation or real NGAP qualification.

## Focused implementation acceptance after approval

- Test all length transitions, zero/max/max+1/UINT64_MAX and invalid widths.
- At starting cursor residues 0..7, compare bytes with independent bit vectors;
  test every truncation boundary and each alignment bit independently.
- Test malformed length selectors and non-minimal payloads, both alone and with
  constrained budgets, following the specified error priority.
- Verify exact/one-less budgets, atomic state snapshots, actual output allocation
  failure, sticky replays and finished/moved-from state priority.
- Preserve existing u16/S3 and synthetic integration regression results.
- Independent review precedes commit; no benchmark or message qualification.

## Owner decisions

| ID | Decision | State |
| --- | --- | --- |
| N1-01 | Restrict primitive to four zero-based non-extensible domains | Owner Approved / Accepted |
| N1-02 | uint64 value + explicit root_bits API, forwarded through Field views | Owner Approved / Accepted |
| N1-03 | Wire layout and one-call atomicity above | Owner Approved / Accepted |
| N1-04 | Error priorities and strict minimal-payload decode policy above | Owner Approved / Accepted |
| N1-05 | Runtime-only implementation and focused acceptance scope | Owner Approved / Accepted |

The Owner explicitly approved these five decisions in the project conversation
on 2026-10-09. Approval authorizes implementation, not a claim of qualification.

Study evidence: the primary agent directly checked the cited ITU PDF and
compared an independent arithmetic layout with external encode/decode for
18 values. Independent read-only contract review: **PASS as an Owner approval
candidate**, checking the API, wire formula, vectors and S3 compatibility.
The reviewer did not separately retrieve the normative PDF. No runtime code,
generator code or tests were modified in this study.
