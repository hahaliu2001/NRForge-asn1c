# N3 — Owned ENUMERATED Numeric / PER Index Evidence

2026-10-09. **Owner Approved / Accepted — N3-01 through N3-07 approved on 2026-10-09.**
Base: `3c96b0e23dee97652219ece8131caf1b5d66d2e7`.
Parent authority: approved N2 contract, especially N2-06.
One objective: retain and validate owned schema evidence before generated enum
mapping. No runtime, C++/Python rendering, message integration or wire codec.

## Source and standards evidence

Current extraction (`asn1typed_extract.c`, populate_enumerated_items) uses
one path for named and inline ENUMERATED. It owns names and extension membership
but does not copy member->value. The Fixer (`asn1fix_enum.c`) assigns unnumbered
items and leaves resolved integers in member->value->value.v_integer. Parser
value kind ATV_INTEGER proves a resolved number; a NULL/non-integer value is not
proof of zero. asn1c_integer_t can be __int128, wider than proposed intmax_t.

Primary authorities:
[ITU-T X.680 (02/2021)](https://www.itu.int/rec/dologin_pub.asp?id=T-REC-X.680-202102-I!!PDF-E&lang=e&type=items), 20.2–20.6;
[ITU-T X.691 (02/2021)](https://www.itu.int/rec/dologin_pub.asp?id=T-REC-X.691-202102-I!!PDF-E&lang=e&type=items), 14.1.
Root PER ranks follow ascending assigned values. Addition ranks restart at zero;
addition values must increase relative to previous additions, not the root
maximum. Numeric values are globally unique. Implicit root numbering excludes
explicit root values, including declarations later in the root. N3 reads Fixer
results and does not independently implement these numbering rules.

## Proposed owned metadata and API

Add scalar fields to each asn1typed_enum_item_t:

```c
asn1typed_wire_evidence_e numeric_evidence; /* existing UNAVAILABLE/RESOLVED/UNSUPPORTED */
intmax_t assigned_number;
int has_per_enumeration_index;
size_t per_enumeration_index; /* independently zero-based within root/addition */
```

Add `int has_valid_per_enumeration_mapping` to the enclosing ENUMERATED type.
Reuse the existing evidence and finalize-result enums; no borrowed pointer or
new numeric string allocation. Names, locations and is_extension_addition remain
owned and source-ordered. This proposal is source-compatible with existing
constructors; binary ABI compatibility for separately built old clients is not
promised. All clients must rebuild after the header change.

Add ENUMERATED-scoped operations, names fixed by this contract:

```c
int asn1typed_enum_item_set_numeric_evidence(asn1typed_type_t *, size_t, intmax_t);
int asn1typed_enum_item_set_numeric_unavailable(asn1typed_type_t *, size_t);
int asn1typed_enum_item_set_numeric_unsupported(asn1typed_type_t *, size_t);
asn1typed_wire_finalize_result_e asn1typed_enumerated_evidence_finalize(
    asn1typed_type_t *, char *, size_t);
int asn1typed_enumerated_evidence_validate(const asn1typed_type_t *, char *, size_t);
```

Setters reject NULL/wrong-kind/out-of-range arguments with -1 and no mutation;
on success return 0. Existing add_enum_item/add_enum_item_ex signatures remain.
Constructed items begin UNAVAILABLE, with no index. intmax_t supports negative,
sparse and reordered assigned values, including INTMAX_MIN/MAX. Outside that
range is UNSUPPORTED, checked in the wider Parser domain before narrowing.
Unknown runtime extension indexes are not synthetic schema enum items; N2's
uint64 representation is separate from this signed schema-number domain.

## Validation and atomic publication

Full validation requires at least one root item, valid kind/storage/count and
capacity consistency, nonempty unique source names, boolean extension flags,
all roots before additions, and additions only for an extensible type. Numeric
evidence must be RESOLVED for every item. Numeric values must be globally unique.
Additions must be strictly increasing in retained declaration order; do not sort
an invalid addition list to make it pass. No greater-than-root-maximum check.
All comparisons use ordering operators, never subtract signed endpoints.

Compute each root rank among roots and each addition rank among additions.
Do not reorder enum_items[]. Validate per-part index uniqueness, continuity and
agreement with assigned-number ordering. A type's full mapping is valid only
when both parts are complete. An extensible type with zero known additions can
have a valid mapping. There is no N2 root_count<=255 restriction in schema IR;
a later runtime mapping adapter must enforce its own domain limit.

Finalize first checks API kind and traversable storage without mutation; NULL,
wrong kind or malformed storage returns ERROR without walking invalid storage.
For a well-formed ENUMERATED container it invalidates the type and clears all
item index flags and values before any semantic/evidence checks.
It then derives indexes privately and publishes all indexes and the valid flag
only after every check succeeds. Failure retains numeric evidence but publishes
no partial indexes, including index zero. Diagnostic text must explain the
actual missing/unsupported/duplicate/order cause; success clears the error buffer.

- OK: complete validated mapping published.
- UNAVAILABLE: missing/unsupported evidence, duplicate number/name, or invalid
  semantic root/addition ordering; no mapping published.
- ERROR: invalid API arguments or malformed storage, checked-size/allocation
  failure, or an internal invariant failure. These must not masquerade as normal
  evidence unavailability.

Validate returns 0 only after recomputing all invariants, verifying indexes and
checking the finalized flag; otherwise -1 with a concrete diagnostic. Direct
public-struct edits require validate before use. A stale valid flag alone proves
nothing. Every successful enum item insertion or numeric setter invalidates the
entire mapping and clears every published index. Setters clearing evidence also
clear assigned_number; a RESOLVED zero remains distinguished by evidence state.
Existing direct edits of membership/extensibility are covered by revalidation,
not by attempting to intercept public field writes.

## Extraction and compatibility boundary

From a successfully fixed tree, copy ATV_INTEGER values representable by intmax_t
into owned evidence. Missing/non-integer values become UNAVAILABLE; out-of-range
integers become UNSUPPORTED. No source-name lookup, index-as-number fallback,
implicit zero, or numbering heuristic. Finalize after the entire body is filled.

As in S2-P1, OK and UNAVAILABLE permit extraction success; UNAVAILABLE clears
its temporary diagnostic and leaves mapping invalid. ERROR propagates a specific
failure and normal extraction cleanup. This adds evidence without widening or
narrowing existing schema acceptance. Existing parse/fix rejections are retained;
N3 does not fix Parser/Fixer semantics or add named-number reference resolution.
Previously accepted but unsupported evidence remains extractable and unusable
for future enum mapping. Existing legacy renderer output and S1/S2/S4 behavior
must remain unchanged; existing legacy ENUMERATED acceptance is retained, and
the restricted S1/S2/S4 entry points do not start accepting ENUMERATED here.

## Ownership and copies

Audit both current deep-copy paths: asn1typed_field_copy and
asn1typed_type_add_inline_enumerated_field. Preserve owned names, locations,
extension flags and numeric evidence through these copies. For a source claiming
a finalized mapping, validate it first; a stale/invalid claim makes the copy fail.
Copy a valid complete mapping atomically, without exposing intermediate state.
For a source without a valid mapping, copy numeric evidence but leave every index
unpublished and the target invalid; do not opportunistically finalize it.
Copies must not drop metadata or create borrowed Parser/Fixer references.

Mutating/clearing/destroying the original body or deleting Parser/Fixer trees
must leave the copied body intact. Construction/copy allocation failure uses
existing cleanup guarantees and does not publish a partial destination field.
Type clear resets all new flags/scalars along with existing owned storage.
No generic type-copy API is introduced unless a private helper is needed to
share these existing enum-body copies.

## Study probes and existing limitations

Independent scratch Parser/Fixer probes were actually run. Reordered roots
9,-2,5 retain source order; implicit roots a(5),b,c(0),d resolve to 5,1,0,2.
Both a,b(3),...,c(1),d and a,z(25),...,d,e resolve to roots 0,3 / 0,25 and
additions 1,2. Descending additions 5,2 are rejected. Integers outside intmax_t
were accepted by the wider Parser/Fixer integer representation.

A negative first addition is rejected by the current Fixer, whose previous-
addition sentinel is -1. A named numeric reference probe crashes in the existing
Fixer ATV_REFERENCED branch. These are recorded existing limitations; no general
claim that referenced-number handling is supported or gracefully rejected. N3
must not repair these paths as a side effect. Run such probes in isolated scratch
processes, not in the persistent passing test process. Only successfully fixed
trees enter N3 extraction. This does not qualify the full frozen NGAP schema.

## Required focused implementation evidence after approval

Real Parser -> Fixer -> Owned IR fixtures, then delete Parser/Fixer trees before
asserting owned values, source order and mapping:

| Input body | Assigned numbers in source order | PER indexes in source order |
| --- | --- | --- |
| a, b, c | 0,1,2 | root 0,1,2 |
| high(9), low(2), mid(5) | 9,2,5 | root 2,0,1 |
| a(5), b, c(0) | 5,1,0 | root 2,1,0 |
| a(-3), b(4), c(0) | -3,4,0 | root 0,2,1 |
| a, z(25), ..., d, e(30) | 0,25,1,30 | root 0,1; addition 0,1 |
| a(0), b(3), ..., c(1) | 0,3,1 | root 0,1; addition 0 |

The table is a normative expectation, not a claim that fresh probes or the new
implementation already passed. Also require INTMAX endpoints and an outside-
INTMAX case when the configured Parser integer domain can represent it. Document
existing referenced-number failure rather than adding support. Cover both named and inline bodies, singleton and marker-only additions.

Hand-built IR tests: missing/unsupported/mixed evidence, duplicate numbers/names,
illegal membership/order/storage, duplicate/missing/stale indexes and modified
valid flag. Finalize failures and allocation injection must leave every index
unpublished; repeated finalize and recovery must work. Invalid setter arguments
must preserve the prior valid state; successful setters/additions invalidate it.
Copy complete and incomplete evidence through both paths, destroy originals,
and inject allocation failure to verify cleanup. Update tests/distribution only
as needed for this evidence task; run relevant typed/runtime regressions and
focused sanitizers. Independent review precedes commit. No benchmark, full NGAP
qualification or generated ENUMERATED codec in N3.

## Owner decisions

| ID | Proposed freeze | Status |
| --- | --- | --- |
| N3-01 | Owned signed numeric evidence and separate per-part index | Owner Approved / Accepted |
| N3-02 | ENUMERATED-scoped setters, atomic finalize and revalidate | Owner Approved / Accepted |
| N3-03 | Canonical ranks without array reordering or root-max rule | Owner Approved / Accepted |
| N3-04 | Fixed-tree extraction; preserve acceptance; no fallback numbering | Owner Approved / Accepted |
| N3-05 | intmax_t bound, explicit unavailable/unsupported evidence | Owner Approved / Accepted |
| N3-06 | Metadata-preserving deep copies and invalidation lifecycle | Owner Approved / Accepted |
| N3-07 | Focused acceptance above; generation/message work separate | Owner Approved / Accepted |

Independent read-only agent review on 2026-10-09: **PASS as an Owner approval
candidate**, with no blocking contradictions. The review checked source/API,
copy ownership, compatibility, extension ordering and publication boundaries;
it did not run a build or qualification. Independent study probes above were
run separately. Document whitespace and `git diff --check` pass. Only this new
document is changed; no stage, commit, push or production implementation.

Owner approval on 2026-10-09 freezes N3-01 through N3-07 and authorizes the focused implementation and independent review described above.
