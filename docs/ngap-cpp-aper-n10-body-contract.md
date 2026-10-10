# N10 — UEContextReleaseCommand body integration and qualification

2026-10-09 (Owner local date). Independently reviewed and accepted: **design PASS**,
at baseline `d44a36aeb4282b89a859c69932724d0593499d37`.
The continuous execution authority applies. This milestone qualifies the actual **message body only**, not its
InitiatingMessage open field or any complete NGAP-PDU envelope.

## Source and support boundary

Use the six untouched frozen TS 38.413 V18.10.0 modules, their ordered module
list and Git blob identities recorded in the first-message readiness study.
Verify all six identities before generation and native compilation. Neither
receiver nor oracle may prune declarations, rewrite constraints, filter modules,
replace empty object sets, or use an installed/precompiled NGAP schema as a
substitute. A native compiler failure or unresolved production disagreement is
a blocker, not permission to alter the authoritative source.

N9 already proved extraction and strict C++20 generation of the physical graph:
14 ordinary declarations, 6 materialized bound instances, 4 finalized registries.
The command retains one physical `protocolIEs` collection and its SEQUENCE
extension bit; its main registry has ID114 UE-NGAP-IDs/reject/mandatory and
ID15 Cause/ignore/mandatory. Two single-container registries and the pair's
optional extension-container registry are explicitly empty and extensible.
These counts/identities are target evidence, not generic renderer rules.

No new runtime primitive, renderer support family, whole-procedure registry,
protocol-policy enforcement, forwarding of opaque bytes, or schema semantics
are introduced. Existing N9 generator/runtime and all older support/output
promises remain unchanged. No benchmark or WSLg dependency is in scope.

## Reproducible body integration

Generate the actual owned body with the generic `tools/asn1typed_ioc_probe`,
the existing six-module list, root module NGAP-PDU-Contents, message
UEContextReleaseCommand, namespace `n10::body`, and an explicit scratch output
prefix. The probe deletes the Parser/Fixer tree before rendering. A narrow
optional `--verify-determinism` probe mode renders each family twice from the
same unchanged owned graph after tree destruction, compares the entire output,
and refuses publication on mismatch; it does not parse/fix the schema twice.
Its existing default behavior and failure categories remain unchanged.

Persistent target integration and native tooling live under
`tools/n10-body-qualification`; they consume these actual generated types,
mapping and codec headers, plus the real runtime. A small target adapter exposes
the body type, its mapping, encode/decode functions, physical container/entry
types and typed IE wrappers. It is a test/integration adapter, not a new generic
ASN.1 API or a production NGAP service API. Generated headers remain scratch
build products rather than checked-in authority.

The physical container and entry aliases are derived through the generated root
field/container mapping. Each known IE wrapper and payload mapping is selected
by looking up its authoritative **numeric ID** in `EntryMapping::rows`; source
row ordinal and `variant::index()` never stand for a wire ID. Static assertions
tie the chosen wrapper to its published ID, expected criticality, presence and
payload identity. CHOICE construction/inspection uses named generated wrappers
and generated storage/PER-index maps, never a fabricated selector permutation.
Named enum values and enum mapping entries similarly distinguish assigned
numbers, root indexes, known addition indexes and unknown extension indexes.

Only body complete wrappers are called at the outer boundary. Known IE payloads
continue using generated scoped field helpers and shared P3 contexts; the
adapter must not reset limits by decoding/encoding payloads through separate
complete wrappers. Runtime, types, mapping, codec include order is retained.
Strict C++20/NDEBUG tests use active REQUIRE checks and conversion/sign warnings.

## Owned model and policy separation

The body API is the generated ordered owned collection of entries: numeric ID,
received criticality, and either its schema-associated known wrapper or owned
unknown payload. It preserves order, duplicate IDs and missing mandatory rows;
an empty body collection is an encodable physical value, not application success.
Known entry encoding requires the wrapper's ID to match the entry ID, but does
not silently replace received criticality with expected registry criticality.

Qualification reports the observed ID/order/criticality/value model. If a
convenience policy report is supplied, it remains a separate inspection of
registry presence/expected criticality and the actual ordered entries; it does
not delete duplicates, fabricate missing entries, rewrite criticality, or
change codec success. No protocol conformance/application-success claim is
made for missing, duplicate, unexpected or mismatched entries.

Unknown **ENUMERATED extension indexes** are typed scalar values and remain
re-encodable through their existing uint64 index representation. This is
different from opaque unknown IOC open payloads and retained SEQUENCE extension
sidecars, which must fail encoding through sticky refusal. Root alternatives
`choice-Extensions` in UE-NGAP-IDs and Cause are ordinary known CHOICE roots
whose selected single-container payload has an empty extensible registry; they
are not unknown CHOICE extension markers. Their unknown records can be received
and owned but cannot be re-encoded. The same rule applies to the pair's optional
extension container and unknown body/pair SEQUENCE additions.

## Bounded positive/native profile

Fresh pinned pycrate compiles and operates on all six verified frozen source
modules. Native body bytes are decoded by the actual generated body codec, and
all supported known generated encodings are compared byte-for-byte against the
native body encodings and decoded back natively. Compare decoded values as well
as complete bytes; no normalization, padding trimming or criticality rewriting.
Freeze case IDs, oracle identity, byte counts and SHA-256 values in a reviewed
accepted profile. The runner cannot silently update the profile or publish a
success summary while disagreements remain. asn1tools' previous IOC limitation
does not justify a rewritten target schema; it is not required as a second
typed whole-target oracle in this milestone.

The finite case plan includes:

- All three UE-NGAP-IDs roots: pair, AMF-only, and receive-only opaque
  choice-Extensions; all six Cause roots, including its receive-only opaque
  choice-Extensions.
- Every declared value of all five Cause ENUMERATED families (the frozen graph
  currently owns 59/2/7/7/6 values: 81 total), including the root/addition boundary
  and all known additions in CauseRadioNetwork and CauseNas. Add unknown scalar
  extension indexes at the known-addition count and representative 63/64/255
  boundaries; distinguish them from assigned enum numbers and verify their
  allowed re-encoding separately from opaque-data refusals.
- AMF ID 0, 1, the values immediately below/at each 8/16/24/32-bit octet-width
  boundary, and exactly `2^40-1`; RAN ID corresponding 8/16/24-bit boundaries,
  0, 1 and exactly `2^32-1`. Pair cases vary both independently, with optional
  extension container absent and with receive-only unknown extension records.
- Both IE orders, representative duplicate known IDs, empty/missing-one
  collections, all received criticalities, and received-criticality mismatch.
  These remain lossless codec cases, not successful protocol-policy assertions.
- Unknown main IDs and empty-registry nested unknown entries, with representative
  opaque payload lengths 1/127/128/16384/65536 where native tooling permits raw
  unknown payload representation without changing the schema. Body/pair unknown
  SEQUENCE additions exercise owned sidecars. Destroy/overwrite input storage
  and verify retained bytes plus copy/move independence before encode refusal.

Native limitations are explicitly classified. If the native API cannot produce
one retained-unknown/SEQUENCE-addition shape, an independently constructed PER
literal can test the receiver contract, but is not relabeled native encode
agreement. A discrepancy in supported known-body bytes or typed values remains
a production blocker. Exact final case counts are frozen with the inspected
accepted profile, not invented before implementation.

## Negative and resource acceptance

Persistent actual-body checks include ID/wrapper mismatch, AMF/RAN out-of-domain
values, known IE payload truncation/nonzero complete padding/trailing octets,
reserved CHOICE/enum root selectors, malformed outer open framing and outer
body truncation/padding/trailing octets. A malformed known ID is a failure,
never a fallback to an unknown wrapper. Literal strict-padding/trailing rules
remain independent expectations if pycrate accepts them laxly.

Exercise exact/one-less body input/output/wire and collection budgets, known
open depth/staging limits, nested unknown-retention and SEQUENCE bitmap limits,
and actual allocation failures. Preserve sticky first failure, scoped rollback
and no publication of partial body values. This is focused target integration
coverage; N9 remains the authority for exhaustive primitive fragmentation,
position mapping and lifecycle checks, rather than duplicating those suites.

Acceptance requires deterministic owned generation, strict compiled body tests,
the frozen native/literal profile with zero unresolved production disagreements,
appropriate existing regressions, focused ASan/UBSan and independent final
implementation/evidence review. Leak-check limitations must remain honest.

## Explicit exclusion and closeout

Report success only as bounded **UEContextReleaseCommand body** byte/value
qualification against the untouched frozen schema. No `NGAP-PDU`, initiating
procedure selector/criticality, outer InitiatingMessage open framing,
all-procedure registry or whole-network interoperability is qualified. Those
remain a distinct later envelope milestone. Record exact executed evidence,
oracle versions, source identities, test results and exclusions before commit.
