# N15 — owned BIT STRING and use-site SIZE

Owner selected 2026-10-09 (America/Los_Angeles); baseline `e46e85f68e04331ee30f0c5cfbf291afb99a66e6`. Frozen TS38.413 V18.10.0 six-module authority is unchanged.

## Contract

- Own `nrforge::aper::BitString`: MSB-first `vector<byte> octets` plus explicit `size_t bit_count`. Storage must be exactly ceil(bit_count/8), with zero unused low bits. Decode owns its storage and does not read following fields as storage padding.
- Non-extensible fixed/bounded SIZE within 0..65535 bits, or unfragmented unconstrained values up to 16383 bits. Fragment determinants and larger unconstrained values are resource-limit errors. Named-bit lists are rejected, rather than silently losing their canonical-shortening semantics.
- X.691 (02/2021) clause16: fixed0 contributes no field bits; fixed1..16 unaligned; fixed17..65535 aligns before payload; variable bounds encode bit length offset with constrained whole-number rules, then align payload, including zero length. Unconstrained length uses an aligned one/two-octet determinant in bit units. No per-value trailing padding; complete boundary owns final padding/substitution.
- Runtime availability, alignment, determinant and payload are one atomic primitive. Allocation and cumulative wire/output/known-open-staging budgets are checked before publishing cursor/storage/counters. First errors remain sticky; finished/moved-from views return invalid_state without state mutation. Unknown retention and collection-element budgets are not used by these primitives.
- SEQUENCE/CHOICE use-site SIZE is owned effective metadata, with named identity retained. Named use-site bounds must be a supported intersection and a subset of the named declaration; anonymous constraints must match their declared evidence. No weaker-bound substitution, constraint erasure or min/max reduction of union/addition shapes.
- New opt-in `asn1typed_render_cpp_owned_bit_{types,mapping,codec}` family supports named/anonymous BIT and BIT/OCTET use-site SIZE; existing N14 opt-in outputs and refusal boundaries remain. Physical IOC allows plain anonymous BIT refs and named BIT declarations; constrained anonymous IOC cells remain refused because their registry reference has no SIZE slot.
- Extensible SIZE (including preserved terminal root extension), known SIZE additions, unsupported residue, named-bit lists and fragmentation are explicitly refused for generation. SIZE(8,...,16) is not reduced to a root interval; it stays an extraction failure. No contained-protocol interpretation.

Primary source: [ITU-T X.691 (02/2021)](https://www.itu.int/rec/T-REC-X.691-202102-I/en), clauses11.9 and16.

## Validation and closeout

- `make -C libaper check`: 6/6 PASS, including new BIT atomicity/canonical-storage/budget/lifecycle and known-child tests.
- `make -C libasn1typed check`: 29/29 PASS; strengthened BIT generated fixture script additionally executed after fixture enrichment. Parser/Fixer trees are deleted before repeat rendering, outputs are deterministic, and allocation sweeps cover rejection/cleanup.
- `make -C tools check`: 1/1 PASS. Strict C11 renderer checks and strict C++20 generated compilation pass. C++ test links/calls all three public C renderer entry points under their existing extern-C guard.
- Focused ASan/UBSan (no-recover, leak detection disabled) passes runtime and generated-code checks. Independent review instruments owned core/naming/slice/enum/uint/compound/IOC rendering and allocation sweeps; extraction author instruments owned core/extraction for its focused tests. Legacy parser/fixer libraries are not comprehensively instrumented in these runs.
- LeakSanitizer was attempted on the focused runtime binary with `detect_leaks=1`; it fails in this environment with ptrace/task attachment errors. No usable N15 LSan result is claimed. The same binary passes ASan/UBSan with `detect_leaks=0`.
- Independent asn1tools0.167.0 aligned-PER oracle plus separately written Python bit model: 296 runtime encode cases and 296 runtime decode cases, covering all offsets0..7, fixed0/1/8/16/17/65535, bounded variable domains and unconstrained values through16383 bits. 295 exact native/model byte comparisons and native back-decodes pass. One standalone fixedSIZE0 case explicitly separates the approved runtime complete-boundary `00` substitution from native empty output.
- The new anonymous BIT IOC regression caught missing builtin mapping/helper emission. The usage-driven emission fix passes its strict manual-vector fixture and independent re-review; original output names/text remain unchanged.
- A defensive SIZE AST leaf-storage residue gap was tightened, with focused mutated-fixed-tree tests. The first readiness run was rejected by its source-hash freshness gate after late extraction edits; it is not accepted evidence. The final scan is rebuilt from frozen reviewed production sources, and every recorded input hash matches the final files.

Primitive/reference qualification and BODY strict compilation do not constitute additional complete NGAP message wire qualification. Independent source and evidence review must pass before commit; final verdict is recorded below.

## Full131 readiness result

| Check | N14 | N15 |
|---|---:|---:|
| Physical extraction PASS |122|122|
| BODY generation PASS |40|63|
| BODY strict compilation PASS |40|63|
| Physical extraction FAIL |9|9|
| Extracted BODY generation FAIL |82|59|

All131 message/procedure/role/root identities and frozen source authority are reconciled. All40 historical successful types/mapping/codec output SHA triples match N14 exactly. There are no new strict compile failures. The original accepted N11 UEContextReleaseCommand bounded complete-PDU qualification is retained; no new complete message wire qualification is claimed.

23 newly BODY-ready messages:
- BroadcastSessionModificationResponse
- BroadcastSessionModificationFailure
- BroadcastSessionReleaseRequest
- BroadcastSessionReleaseResponse
- BroadcastSessionReleaseRequired
- BroadcastSessionSetupResponse
- BroadcastSessionSetupFailure
- BroadcastSessionTransportRequest
- BroadcastSessionTransportResponse
- BroadcastSessionTransportFailure
- ErrorIndication
- HandoverRequestAcknowledge
- MulticastSessionActivationRequest
- MulticastSessionActivationResponse
- MulticastSessionActivationFailure
- MulticastSessionDeactivationRequest
- MulticastSessionDeactivationResponse
- PWSCancelRequest
- PWSCancelResponse
- RANCPRelocationIndication
- RerouteNASRequest
- RetrieveUEInformation
- WriteReplaceWarningResponse

Remaining first-failure clusters:20 compound shape/storage/metadata,17 unsupported INTEGER root ranges,16 unsupported physical references,4 unsupported INTEGER bit-width domains,2 unsupported use-site SIZE. The same9 physical failures remain:1 SIZE(8,...,16) additions case,7 NULL-related alternatives and1 private-IE key. These are observed first failures, not predicted unlock counts.

## Next milestone

Recommend N16: shared bounded INTEGER domains and use-site value constraints, guided by the21 current first-failure INTEGER cases. NULL, extensible SIZE/addition representations, remaining inline/reference shapes and private IE architecture remain separately scoped. No benchmark, full adversarial scan or NGAP interoperability qualification was performed. N16 has not started.

## Final review

PASS / Ready to Commit: YES. Independent agent reviewed runtime atomicity/storage/budgets, effective SIZE grammar and lifetime, shared generator names/helper emission, focused sanitizer evidence and the final matrix/native/source-hash reconciliation. Both late findings are closed with persistent regression tests. No unresolved blocking issue remains.
