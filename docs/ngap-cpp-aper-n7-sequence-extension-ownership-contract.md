# N7 — SEQUENCE Extension Ownership Contract

2026-10-09. Accepted under continuous execution authority; independent document review **PASS**. Base: `9a93357a8ceff02824a7e6fb00025f19fc03f9cd`.

This task freezes ownership and implementation boundaries only. It changes no production API, IR, runtime or generated output. Detailed framing primitives and generator integration are separately reviewed follow-on tasks.

## Authority and initial schema domain

[D1-R2 and D2-04](ngap-cpp-aper-approved-decisions.md) require owned unknown extensions and separate codec preservation from protocol policy. Unknown-payload preservation does not authorize unchecked encoding. The complete first-message target remains unchanged.

Initial integration supports an extensible SEQUENCE with ordinary root fields and one trailing extension marker, with **no schema-declared additions or extension groups**. Root types/presence remain within N6. It does not support CHOICE extensions, IOC, collections, inline constructed fields or DEFAULT.

Current extraction rejects SEQUENCE components after an extension marker and repeated markers. Its is_extensible flag alone is not accepted as new wire evidence: a focused IR task must explicitly record and validate resolved root-only extension structure, including root field count and zero known additions. Missing/unsupported evidence must fail generation. Existing extraction acceptance and old renderer outputs must remain unchanged; do not infer this evidence from type names or manual zero initialization.

Real UE-NGAP-ID-pair and UEContextReleaseCommand contain a trailing SEQUENCE marker without declared additions. Their IOC/container dependencies remain separate blockers; preserving sequence additions does not qualify those entire types.

## Owned value model

Freeze the conceptual representation (final support-header API names are deferred):

```cpp
struct UnknownSequenceAddition {
    std::uint64_t addition_index;
    std::vector<std::byte> payload_octets;
};
struct SequenceExtensionData {
    std::size_t received_bitmap_bit_count;
    std::vector<UnknownSequenceAddition> unknown_additions;
};
```

Each generated extensible SEQUENCE owns a separate extension-data member alongside its root fields. Naming planning must reserve/check its final member name; never silently overwrite or reject a supported ordinary name merely to fit a hard-coded sidecar name. Non-extensible N6 types remain unchanged.

An addition index is the position in the extension-addition bitmap, not a root field ordinal, IE ID, enum assigned value or tag. Records retain increasing indexes, uniqueness and the original received bitmap width. Index/count conversion is checked before narrowing, allocating or indexing; sparse high indexes still consume the bitmap budget.

Payload is the exact concatenation of open-type payload octets after removing that open type's length/framing/alignment. It includes opaque inner complete-encoding padding and any unknown semantics. Do not inspect, normalize, truncate or zero opaque payload bytes. It excludes outer framing bytes; byte-exact payload preservation does not promise byte-exact reproduction of original length determinants or fragmentation boundaries. The later framing contract defines which legal frames can be accepted.

No spans or pointers borrow the input. Copies deeply copy records/bytes; moves transfer ownership; destruction releases everything. Default construction means no received extension section (bitmap width zero, no records) and valid default root fields. Declared known addition types/groups require a later value/IR contract and cannot be stored here as if semantically supported.

## Decode, encode and policy boundaries

Decode retains every successfully delimited unknown addition, preserving its index, payload and bitmap width. Unknown data is not dropped based on NGAP criticality or application support. A malformed frame or exceeded resource budget fails the whole decode; complete wrappers publish no partial value.

This milestone authorizes encoding supported root fields with the extension bit clear. Any non-default received extension data on encode is rejected through a context-sticky error path; it is never silently discarded and opaque payloads are not emitted. Explicit validated opaque re-encoding requires a later contract. Known enum unknown-index encoding remains governed by N4 and is a different mechanism.

Allocation failures become allocation_failure while preserving the first error. A bounded open-type read must include alignment, determinant parsing, payload availability, budget checks and owned allocation in one transactional operation; failure leaves that primitive's cursor/counters unchanged. A SEQUENCE helper may already have consumed prior root fields; this does not authorize partial complete publication.

Nested payload readers must share the per-call budget/state and cannot escape through fresh complete contexts to reset limits. Unsupported inner semantics are not decoded solely to establish an already known opaque boundary. Protocol ignore/reject/report and defaulting policy remain above the codec.

## Resource design decisions

Preserve existing input/output/wire budgets and their meaning. A separately reviewed runtime API task will add shared per-call budgets for total extension bitmap bits, retained unknown payload octets and retained unknown records, with proposed defaults **1024 bits / 1 MiB / 1024 records**. These are new cumulative limits across nested sequences, not per-field allowances; existing ordinary operations consume none of them. The final API must expose counters and checked reservation/rollback without breaking existing aggregate Limits initialization.

Bitmap widths and length determinants are validated before allocation. Readable but over-budget data reports resource_limit, rather than being treated as an ignorable unsupported extension. Exact error priority and bit offsets, legal determinant forms, fragmentation, empty open payloads, bitmap canonicality and normally-small bitmap-length encoding will be frozen in the framing task against X.691 before runtime implementation. This ownership contract does not invent or relax those wire rules.

## Separate follow-on tasks and acceptance

1. **N7-P1 — Owned IR evidence:** root-only SEQUENCE extension structure, validation/invalidation and ownership tests; no runtime or codec.
2. **N7-P2 — Bounded framing contract and runtime:** first freeze open-type/bitmap layouts, API, budget defaults and error matrix; implement reviewed primitives with independent vectors. Runtime changes must not silently alter existing primitives.
3. **N7-P3 — Generated root-only extensible SEQUENCE:** sidecar types, mapping/evidence checks and decode preservation; root-only encode and sticky rejection of retained data. Independent native APER vectors, input-destruction/deep-copy tests, nested cumulative budget/rollback and allocation-failure coverage precede review/commit.

Each task has one primary objective and independent review. Declared known additions, extension groups, opaque re-encoding, collection/object-set/open-type dispatch and complete NGAP message qualification are outside these tasks. No frozen schema pruning, benchmark or WSLg dependency.

Ownership decisions N7-01 root-only initial domain, N7-02 indexed owned opaque sidecar, N7-03 exact payload preservation/deep-copy, N7-04 no opaque re-encoding, N7-05 cumulative bounded retention, and N7-06 separate IR/framing/generation tasks are accepted under continuous authority after independent review PASS. Review confirmed D1-R2/D2-04 preservation/policy boundaries, bitmap/index ownership, scope and task separation without blockers. No code or tests were changed or executed in this document-only milestone; whitespace checks passed.
