# N2 — ENUMERATED API and Wire Contract Freeze

2026-10-09. **Owner Approved / Accepted — N2-01 through N2-07, 2026-10-09.**
Base: `75b2ea193bd33029dc9e304f28fa051560c909b7`.
Parent: approved UEContextReleaseCommand complete-PDU readiness study.
One objective: freeze the next runtime primitive family. No production code,
Owned IR changes, generated enum types/mapping, dispatch or message codec here.

## Frozen schema evidence

The untouched TS 38.413 V18.10.0 source at NRForge-RAN commit
`d6e514a33ec3695925c24aa292900514b371b074` was inspected directly.
Local source snapshots match Git blobs `acaba01d1fad8d656ae8f568bf951de3b67278ac`
(CommonDataTypes) and `a8974dd27e5c7f37bf439990fb39da397ec97044` (IEs).

| Type | Root count | Known addition count | Extensible |
| --- | ---: | ---: | --- |
| Criticality | 3 | 0 | no |
| CauseRadioNetwork | 45 | 14 | yes |
| CauseTransport | 2 | 0 | yes |
| CauseNas | 4 | 3 | yes |
| CauseProtocol | 7 | 0 | yes |
| CauseMisc | 6 | 0 | yes |

Zero known additions does not mean that extension values are prohibited.
`choice-Extensions` in Cause is a separate CHOICE mechanism.

## Runtime API proposal

Add a scalar wire-index value in `nrforge::aper`:

```cpp
struct EnumeratedIndex {
    bool is_extension;
    std::uint64_t index;
};
Result<void> write_enumerated(EnumeratedIndex value,
                              unsigned root_count, bool extensible);
Result<EnumeratedIndex> read_enumerated(unsigned root_count, bool extensible);
```

Expose operations on BitWriter/BitReader and forward through Field views.
`index` is independently zero-based in root and extension namespaces. It is not
an ASN.1 assigned number, a combined root-plus-addition ordinal, or a C++ enum
underlying value. Runtime does not know names or the number of known additions.
The later generated representation must preserve each schema type's identity
and distinguish known root, known addition and unknown extension index.

Supported root_count is **1..255**, including singletons. Other counts are
invalid API arguments; this deliberately excludes octet-aligned large roots.
Extension indexes cover the entire uint64 domain, independently of root_count.
An extension value is invalid when extensible is false. Root indexes must be
less than root_count. No new Limits, ErrorCode, context or finish API.

## Wire layout

Primary source: [ITU-T X.691 (02/2021)](https://www.itu.int/rec/dologin_pub.asp?id=T-REC-X.691-202102-I!!PDF-E&lang=e&type=items),
clauses 14, 11.5.7.1, 11.6, 11.7 and 11.9.3.6.
Root indexes follow ascending ASN.1 enumeration values; addition indexes restart
at zero in the ascending addition-value order required by clause 14.1. Later
mapping must validate resolved values and must not assume array order. Extensible fields start with a root/extension bit. Root encoding uses
ceil(log2(root_count)) unaligned bits; a singleton uses none. Extension indexes
0..63 use a zero indicator plus six unaligned bits. Larger indexes use a one
indicator, zero alignment, a one-octet payload length, and minimal unsigned
big-endian payload. Length is 1..8 for uint64. No pre-alignment precedes the
extension or size indicator. Complete final padding remains outermost only.

For an extensible root, its extension bit is zero; for an addition it is one.
A non-extensible root has no extension bit. N1 write_constrained_uint is not a
substitute for the unaligned root bit-field.

## Strict decode and supported unknown values

This runtime can decode and encode any unknown numeric extension index within
uint64. This explicit numeric authorization does not authorize encoding opaque
unknown IE/open-type payloads. Protocol handling policy remains outside runtime.
Values exceeding uint64 are rejected with resource_limit; no truncation,
replacement with an unspecified enumerator, or silently losing extension state.
This bounded preservation scope must be documented by later generated APIs.

Reject unused root bit patterns as constraint_violation. Reject long-form values
below 64, leading-zero multi-octet payloads, zero length, and two-octet length
forms claiming 1..8 as constraint_violation. Lengths greater than 8 and fragmented
length forms are resource_limit: they cannot represent an in-scope minimal index.
All malformed cases are explicit strict-decoder decisions. No general length or
normally-small-number API is added by N2.

## State, priority, offsets and atomicity

One primitive includes the extension flag, normally-small prefix, alignment,
length and payload, as applicable. It must not be implemented by publishing a
sequence of individually successful bit operations and then failing halfway.

1. Check live state before all arguments: failed replays its first Error;
   finished/moved-from returns invalid_state without changing context.
2. Healthy invalid root_count fails as invalid_argument at field start. Invalid
   encode index/domain fails as constraint_violation there before any publication.
3. Decode peeks enough metadata to identify the layout without changing cursor
   or counters. Missing prefix/length reports truncated_input at logical input
   end. For long form, peek the length after alignment; do not check padding yet.
4. Once length metadata is available, apply the malformed/oversize length rules
   above before checking payload availability or wire budget. A two-octet length
   header must be complete before classifying its value; fragmented length forms
   are classified from their first length octet.
5. For an otherwise valid layout: preflight whole-field input availability,
   then whole-field wire budget, then check every zero alignment bit, then decode
   and check root validity or payload canonicality. Report the first nonzero
   alignment bit. This priority holds when multiple conditions are invalid.
6. Failure leaves cursor, counters and output contents unchanged; a healthy
   context records one sticky Error. Truncation uses logical input end; padding
   uses its offending bit; argument/constraint/resource/allocation use field start.
   Reserve writer storage only after complete preflight and before publication.
7. All prefix, alignment, length and payload bits count against shared wire
   limits; projected logical output octets use existing output limits. Reader
   input admission and complete-wrapper publication remain unchanged. Singletons
   still check lifecycle/arguments although a non-extensible singleton uses zero
   bits. No reader allocation or per-value heap representation is needed.

## Owned IR boundary for subsequent work

Current enum items retain name and is_extension_addition, but no assigned ASN.1
numeric value or independently validated PER index. Extraction currently accepts
members without preserving their numeric assignments. This is insufficient
proof for general enum mapping, including explicit, sparse or reordered numbers.
A separately scoped Owned IR evidence task is required before generated enum
mapping: preserve resolved numbers and canonical root/addition indexes, validate
uniqueness and completeness, and fail closed if evidence is missing. Runtime
N2 does not modify or infer that evidence from source/array order.

## Independent study vectors and external limitations

These are complete field encodings at cursor zero (outermost zero padding
included), independently calculated. `E` below is an extensible enum with three
root items; extension indexes are independent of its known addition count.

| Field value | Bytes |
| --- | --- |
| Criticality root 0 / 1 / 2 | `00` / `40` / `80` |
| E root 0 / 2 | `00` / `40` |
| E extension 0 / 1 / 63 | `80` / `81` / `bf` |
| E extension 64 / 255 | `c00140` / `c001ff` |
| E extension 256 | `c0020100` |
| E extension UINT64_MAX | `c008ffffffffffffffff` (arithmetic model only) |

Study evidence: pycrate 0.7.11 native schema encodings, with explicit Boolean
prefix fields, matched an independent bit model for six extension indexes
0,1,63,64,255,256 at all eight starting residues (48 comparisons). Boolean prefix
values were false for this study. For index 64, results by residue 0..7 are
`c00140`, `600140`, `300140`, `180140`, `0c0140`, `060140`, `030140`, `01800140`.
These include the prefix bits and complete final padding.

asn1tools 0.167.0 agrees on short extensions and tested root encodings, but its
long normally-small path omits alignment before the length. At residue zero it
produces `c05000` for index 64 instead of `c00140`; it agrees when the two flag
bits happen to end on an octet boundary. This is an external oracle limitation,
not a reason to alter the normative contract. It also emits an empty byte string
for a standalone non-extensible singleton; S3 complete encoding requires `00`.
No external full-schema qualification or runtime implementation is claimed.

## Acceptance after approval

- Independently derive vectors for root counts 1,2,3,4,6,7,45,255 and root endpoints;
  extension transitions 0,63,64,255,256 and every payload-octet transition through
  UINT64_MAX, at cursor residues 0..7.
- Cross-check complete bytes and lengths with an external APER implementation;
  separately test unknown additions where external tools cannot emit them.
- Every truncation boundary, each alignment bit, root spare patterns, strict
  non-minimal forms, malformed/oversize lengths, and combined-error priorities.
- Exact/one-less budgets, actual allocation failure, atomic snapshots, sticky
  callbacks, singleton zero-bit state, and finished/moved-from priority.
- N1/S3 and existing synthetic regressions; independent review before commit.
- No benchmark or real-message qualification.

## Owner decisions

| ID | Proposed freeze | Status |
| --- | --- | --- |
| N2-01 | Scalar root/extension index API; root_count 1..255 | Approved / Accepted |
| N2-02 | Root bit-field and both normally-small extension forms | Approved / Accepted |
| N2-03 | uint64 unknown numeric extension preservation and encoding | Approved / Accepted |
| N2-04 | Strict malformed/non-minimal rejection and bounded overflow policy | Approved / Accepted |
| N2-05 | Whole-primitive atomicity, sticky state and error priorities | Approved / Accepted |
| N2-06 | Later separate IR evidence/type mapping; no source-order inference | Approved / Accepted |
| N2-07 | Focused acceptance above; no message implementation in N2 | Approved / Accepted |

Independent read-only agent review on 2026-10-09: **PASS as an Owner approval
candidate**, including source counts, addition ordering, state/error contract,
study vectors and explicit external-tool limitations. This is not Owner approval
or implementation qualification. Repository checks: document whitespace and
`git diff --check` pass; only this new document is changed. No production tests
or benchmarks were run in this documentation-only task.

Owner approved N2-01 through N2-07 in the project conversation on 2026-10-09.
Implementation proceeds as a separate change; its commit awaits independent
review and a concrete delivery decision.
