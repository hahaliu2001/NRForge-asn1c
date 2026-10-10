# N17 — Shared compound/reference shape lowering

## Authority and baseline

Owner selected N17 under the continuous autonomous design, implementation, independent review/fix, commit/push workflow. Baseline `50ce618688a3a0d5358e2348bc937dfec8f2dd53`, branch `feature/ngap-cpp-aper-first-message`. Use the unchanged TS 38.413 V18.10.0 schema at RAN commit `d6e514a33ec3695925c24aa292900514b371b074`: 131 messages and 81 procedures. Preserve all68 N16 successful generated header triples.

## Concrete first-failure study

The N16 generic diagnostics do not represent one semantic gap. Read-only inspection of the unchanged owned graphs identifies the23 compound-shape first failures as15 TransportLayerAddress (extensible BIT STRING SIZE),3 AMFName and2 RANNodeName (extensible character strings),2 UE-associatedLogicalNG-connectionList (SIZE 1..65536), and1 NRencryptionAlgorithms (extensible BIT STRING SIZE). These remain unsupported; they are not generic shape defects to bypass.

Of11 physical-reference first failures,10 encounter an already-owned inline ENUMERATED SEQUENCE field. The other, RANPagingRequest, reaches PagingPolicyDifferentiationItem.dl-DataSize, an extensible INTEGER 0..96000, and remains outside N17. Subsequent unsupported dependencies may be masked by these first failures. The count10 is not a promise of10 additional BODY codecs.

Evidence: `tools/n17-shape-qualification/baseline-shapes.json`, linked to the accepted N16 readiness scan. No schema pruning or constraint erasure was used.

## Accepted contract

Lower already-owned inline ENUMERATED SEQUENCE fields into generation-local synthetic named declarations and reuse the approved N4 enum renderer/runtime. Source owned IR remains immutable, including field declaration order, OPTIONAL presence, enum assigned numeric identities, root/addition boundaries and finalized PER indexes. Synthetic identities are local emission details, not claims that the ASN.1 source contains a named declaration.

Require complete finalized enum evidence and revalidate it. Missing, stale, duplicate or unsupported evidence must not be reconstructed from source order. Reject inconsistent enum/body/field metadata, residual constraints, DEFAULT/CONDITIONAL presence, class/IOC selector metadata on the inline field and unsupported identities. Preflight synthetic source-key and final C++ symbol collisions before output. On failure return -1, a concrete diagnostic and null output; allocation failure releases only lowering-owned allocations. Borrowed source arrays and enum strings must never be freed by the temporary view.

Synthetic enum declarations precede original declarations so ordinary codecs reference earlier emitted helpers. Original declaration ordering and physical field/component ordering remain intact. Compound CHOICE storage-to-PER translation and SEQUENCE OPTIONAL bitmap semantics remain the existing contracts. Ordinary opt-in `asn1typed_render_cpp_owned_shape_*` and existing physical IOC generation share the same lowering implementation. Older ordinary value/compound entry points preserve their accepted support boundaries.

No new runtime wire semantic is introduced. Root indexes use existing constrained enum indexes; known and unknown extension indexes retain N4 behavior. The three generated outputs must come from unchanged IR and identical namespace, included after runtime.hpp in types/mapping/codec order.

## Explicit exclusions

Character-string codecs, extensible INTEGER and string SIZE, discontinuous ranges, NULL, private-IE keys, unrepresented constraints, contained-protocol interpretation, recursive graphs and fragmented SEQUENCE OF remain separately scoped. The SIZE 65536 collection boundary requires its own fragmentation contract; N17 does not merely increase a numeric limit.

## Verification and review

Typed `make check`: 34/34; runtime: 7/7; tools: 1/1. New tests use actual Parser/Fixer extraction followed by Parser deletion, repeated generation, source-IR invariants, allocation-failure sweeps and refusal/evidence/collision checks. Generated ordinary, renamed and physical IOC code passes strict C++20 with conversion warnings. The in-tree C++ harness compares96 independent complete bit-model vectors in both encode/decode directions (192 checks) with OPTIONAL root/addition/unknown enum states and nested CHOICE/collections; physical IOC bytes and typed payload decoding are separately checked.

`tools/n17-shape-qualification/qualification-summary.json` records240 actual generated codec cases with exact native asn1tools 0.167.0 and independent model bytes, generated decode of all fields/presence and native decode. Source fingerprints and generated-header hashes are recorded; the accepted report was rerun after final defensive guards. Known enum values only are included in this native comparison. Unknown extension values are covered by the focused generated tests and historical N4 behavior.

Independent implementation review PASS: borrowed ownership, transactional temporary-view cleanup, enum evidence/storage preflight, bound actual-key storage, naming/collision checks and source immutability. Strict C11 and focused C generation/allocation-sweep and generated C++ ASan/UBSan checks passed. Legacy parser/fixer archives were not comprehensively sanitizer-instrumented. LSan was actually attempted and failed with a ptrace/proc restriction, so no usable LSan result is claimed. Final source/evidence reconciliation PASS; no unresolved blocking finding.

## Result and next milestone

Accepted final full131 scan: physical extraction 122 PASS / 9 FAIL; BODY generation 69 PASS / 53 FAIL / 9 NOT_RUN; strict compilation 69 PASS / 0 FAIL / 62 NOT_RUN. Source authority and inventory are unchanged. All28 scan fingerprints match the accepted final source, and all68 previous PASS messages retain byte-identical types, mapping and codec headers.

New strict BODY PASS: **MTCommunicationHandlingRequest**. The original11 reference-first messages now have7 changed outcomes: this1 PASS,3 INTEGER constraint failures,1 extensible BIT STRING failure and2 use-site SIZE failures. Four messages still stop at another physical reference: ConnectionEstablishmentIndication and UEInformationTransfer have extensible inline time intervals; RANPagingRequest has extensible dl-DataSize; TimingSynchronisationStatusReport reaches extensible ClockAccuracy alternatives. None of these domains was erased or treated as an enum-lowering success.

Remaining generation first-failure counts:24 genuine compound shapes (17 extensible BIT STRING,5 extensible character strings,2 SIZE 65536 collections),20 unsupported INTEGER domains,4 physical references and5 use-site SIZE cases. The same9 physical extraction failures remain. These are observed first failures, not predicted unlock counts.

Recommend N18: an explicitly scoped extensible INTEGER root/extension domain contract study, guided by the20 INTEGER and4 inline INTEGER reference first failures. Existing discontinuous permitted sets and tails require separate verified treatment; do not assume every extensible domain is one contiguous interval. Extensible strings, fragmentation, NULL and private-IE architecture remain separate. N18 has not started. Syntax readiness is distinct from complete-message APER wire qualification. No benchmark, complete adversarial scan or new NGAP interoperability qualification is included. Stop after N17.

## Final review

Independent implementation and evidence review PASS. Typed34/34, runtime7/7 and tools1/1 gates passed. Final native/model/generated reports and full131 compatibility records are current. No new complete-PDU wire qualification is claimed; the bounded historical N11 UEContextReleaseCommand qualification remains unchanged.
