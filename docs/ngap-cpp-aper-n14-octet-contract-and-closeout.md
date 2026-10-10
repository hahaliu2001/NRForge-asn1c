# N14 — Shared OCTET STRING Capability

Owner-selected milestone: 2026-10-09 (America/Los_Angeles), based on N13 full-message readiness. Implementation baseline `d77537d3ac25f3b749e0f2ae9ae57468f765f1f5`. Frozen NGAP sources and ordered six-module authority remain unchanged.

## Bounded contract

Use owned `std::vector<std::byte>` storage. Named OCTET STRING declarations accept absent SIZE or one non-extensible interval `0 <= lower <= upper <= 65535`. Anonymous primitive references are unconstrained only. Additional unsupported metadata, anonymous/use-site SIZE, extensible SIZE, unsupported primitive references and unrepresented constraint shapes fail in generation/extraction; no field or semantic is silently dropped.

Wire rules follow [ITU-T X.691 (02/2021)](https://www.itu.int/rec/T-REC-X.691-202102-I/en), clauses 17.5–17.8 and 11.5/11.9. Fixed SIZE(0) contributes no field bits. Fixed lengths 1 and 2 have no length determinant or octet alignment; fixed lengths 3 through 65535 have no determinant and align the payload to an octet. Variable bounded lengths encode the constrained length offset then octet-align the payload, even when the payload is empty. Unconstrained values use an aligned one-octet or two-octet determinant and opaque payload. This implementation supports only unfragmented unconstrained values through 16383 octets; larger values or fragmented receive determinants fail with resource_limit. This is an implementation limit, not an ASN.1 schema restriction. No extension bit is synthesized for a non-extensible SIZE.

The runtime exposes atomic `write_octet_string(span, lower, upper, unconstrained)` and `read_octet_string_owned(lower, upper, unconstrained)` through cursor and Field views. Unconstrained mode requires bounds 0/0. Alignment, determinant, availability/budget checks, allocation and payload form one operation. Failure preserves cursor, wire/output counters and first-error sticky behavior; allocation succeeds before publishing output/decoded ownership. Known-open child views follow existing shared wire/staging conventions. Ordinary bytes do not consume unknown-extension retention or collection-element counters. Complete encode/decode still use the existing final-padding, trailing-data and empty-encoding substitution contract.

Three `asn1typed_render_cpp_owned_octet_*` APIs cover types/mapping/codec and reuse compound naming, reference plans and collision checks. They compose with existing ENUMERATED/unsigned INTEGER/CHOICE/SEQUENCE/collection/sequence-extension support. Existing earlier non-IOC compound/collection APIs retain their old OCTET rejection boundary. Physical IOC renderers now support the same named OCTET storage and unconstrained anonymous primitive references. New builtin helpers are emitted only for graphs actually using OCTET STRING, preserving previous no-OCTET generated header bytes.

## Selected CONTAINING ownership

Extend the already approved opaque ContentsConstraint compatibility rule to physical IOC selected Value/Extension cells only when both declared and present combined constraints satisfy the existing exact Contents-only recognizer, the selected value is a direct OCTET STRING type and has no parameter actuals. Retain the outer anonymous primitive bytes; do not traverse, generate or validate the contained type. Plain anonymous OCTET STRING is also accepted. Anonymous SIZE, Contents+SIZE and combined-only unknown residue continue to fail closed. Named payload references preserve their declaration-owned constraints through dependency closure.

The registry row owns copied symbolic ID metadata. The temporary symbolic ID is now freed on successful deep-copy and malformed-row exits, closing the scoped pre-existing temporary allocation leak in the edited path.

## Evidence and acceptance boundary

Focused runtime tests include all eight bit residues, fixed and variable alignment thresholds, determinant boundaries, ownership, complete finish boundaries, every selected padding bit, truncation, sticky/lifecycle behavior, exact/one-less budgets, actual allocation failure and known-open child framing.

Real Parser/Fixer fixtures verify named/anonymous primitive codec composition, source-owned SIZE, selected main/extension Contents-only payloads, combined constraint rejection and allocation failure cleanup. All success checks read Owned IR after Parser deletion. Generator tests compare repeated output and compile combined types/mapping/codec under strict C++20; byte vectors cover OPTIONAL, CHOICE, collections and IOC/open-type composition.

Independent qualification uses pinned asn1tools 0.167.0 aligned PER and a separate Python bit model, comparing 264 selected cases at offsets 0–7. Exactly 263 compare native bytes and native backdecode with no transformation. One standalone fixed SIZE(0) difference is explicit: asn1tools emits empty bytes, while the existing complete-runtime contract substitutes 00. That case is checked separately against the documented complete boundary and empty-field native semantics. This is bounded primitive framing evidence, not generated-code or complete-NGAP qualification. Compiler/version/flags and stable before/after source hashes are recorded in [qualification-summary.json](../tools/n14-octet-qualification/qualification-summary.json).

The full post-implementation matrix is [readiness.json](../tools/n14-octet-qualification/readiness.json), reproduced by the unchanged N13 scan runner. No predicted unlock count is used as acceptance.

## Full-message observed results

| Stage | N13 PASS | N14 PASS | N14 FAIL | N14 NOT_RUN |
|---|---:|---:|---:|---:|
| Physical extraction | 104 | 122 | 9 | 0 |
| Three BODY generators | 18 | 40 | 82 | 9 |
| Strict combined-header C++20 syntax compile | 18 | 40 | 0 | 91 |

All 18 selected-Contents first extraction failures from N13 now pass physical extraction. The nine remaining extraction failures are the same seven NULL cases, one known SIZE-addition case and one private-class/key case; they remain outside N14. Twenty-two additional BODY triples now generate and compile, with no compile failures among the 40 successful triples. New BODY compile-ready messages:

- AMFCPRelocationIndication
- DeactivateTrace
- DownlinkNonUEAssociatedNRPPaTransport
- DownlinkUEAssociatedNRPPaTransport
- HandoverCommand
- HandoverPreparationFailure
- HandoverFailure
- InitialContextSetupResponse
- InitialContextSetupFailure
- NASNonDeliveryIndication
- PathSwitchRequestFailure
- PDUSessionResourceModifyConfirm
- TimingSynchronisationStatusRequest
- TimingSynchronisationStatusResponse
- TimingSynchronisationStatusFailure
- TraceFailureIndication
- UERadioCapabilityCheckRequest
- UERadioCapabilityIDMappingRequest
- UERadioCapabilityIDMappingResponse
- UERadioCapabilityInfoIndication
- UplinkNonUEAssociatedNRPPaTransport
- UplinkUEAssociatedNRPPaTransport

All 18 historical generation-success triples retain exactly the N13 types/mapping/codec SHA256 values, including UEContextReleaseCommand. This provides generated-text compatibility evidence for those cases; historical N11 complete-PDU qualification is retained, not rerun or broadened. The first N14 scan exposed a stale linked probe library; it was discarded as final evidence. After refreshing the typed libraries and coverage probe, the fresh scan reproduced the stage counts and passed all 18 historical header-hash comparisons. Final API inspection also moved the new declarations inside the existing C-linkage guard; C++ callers link and invoke all three C renderer entries successfully. A final scan after that header fix supplies the accepted input snapshot. Each scan parses the six modules once, not once per message.

## Validation and independent review

- Runtime checks: 5/5 pass, including the new atomic OCTET test.
- Typed checks: all 24 pre-linkage tests pass; the additionally registered C++ API link-and-call regression also passes (25 test targets in the final manifest). Initial run passed 23/24; the new extraction executable had a local generated-file executable-permission problem. Restoring its executable permission and rerunning that one check passes 1/1. No production source change was needed for that issue.
- Tools checks: 1/1 pass. Strict C renderer warnings include conversion and sign-conversion; generated C++20 tests and all 40 BODY syntax compilations pass.
- Independent runtime qualification: 264 encode and 264 decode cases, 263 exact native/model/native-backdecode cases and the one separately documented empty-complete difference. Stable source snapshots and exact compiler evidence are in the summary.
- ASan/UBSan: focused runtime, instrumented owned core/renderers with allocation sweeps, selected extraction and generated-code paths pass with leak detection disabled. The parser/fixer libraries are not claimed comprehensively instrumented. A separate detect_leaks=1 attempt failed because LeakSanitizer cannot operate under the environment's ptrace; there is no usable N14 LSan result.
- Distribution manifests include new source/tests/fixtures and qualification artifacts. Whitespace checks pass.
- Independent review: runtime/renderer contract, selected opaque extraction/reference ownership, focused sanitizer evidence, final matrix/compatibility claims and the C++ linkage fix all pass before commit. The unconditional unused-helper emission finding was fixed; no unresolved blocking finding remains.

## Remaining shared work

Recommend N15 as BIT STRING and use-site SIZE evidence/lowering, informed by the actual remaining 82 generation failures and nine physical failures. Additional INTEGER domains/use-site constraints, inline ENUMERATED and bounded strings remain separate capability batches. Known SIZE additions must be represented rather than erased; NULL and private-IE keys remain explicitly scoped work. Fragmented/unconstrained-large and extensible OCTET support are not silently included. No additional complete NGAP message is wire-qualified by this milestone. N15 has not started.
