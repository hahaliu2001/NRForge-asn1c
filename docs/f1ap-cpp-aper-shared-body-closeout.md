# F1-P2 — shared F1AP body capability closeout

Date: 2026-10-10. Branch: `feature/f1ap-cpp-aper`. Implementation baseline: `8b741dad60645916beab0545ce6ca1ce5d1768a9`.

## Scope and frozen authority

This milestone extends the shared owned IR, extractor and C++ BODY generation machinery used by NGAP. It does not create individual message codecs. The six F1AP modules remain byte-identical to the TS 38.473 V18.10.0 source manifest at NRForge-RAN `d6e514a33ec3695925c24aa292900514b371b074`; all 94 procedures and their 158 outcomes are scanned in one combined Parser/Fixer pass. The Parser/Fixer tree is destroyed before generation. Historical F1-P1 evidence remains unchanged in `tools/f1ap-readiness/readiness.json`.

## Shared changes

1. Parameterized SEQUENCE OF elements retain complete object-set actual identities and reuse existing bound-instance and IOC registry materialization.
2. Inline SEQUENCE/SEQUENCE OF/CHOICE fields, alternatives and elements lift into owned module-qualified internal identities with recursive closure. Reserved path keys cannot alias ASN.1 source identifiers; generated-name collision checks remain active. Original CHOICE tags, SIZE, presence and source provenance survive Parser/Fixer destruction. Unsupported constructed constraints still fail closed.
3. Bounded explicit INTEGER extension ranges/unions retain canonical known addition intervals separately from the root. SRBID, MaxDataBurstVolume and inline prachConfigIndex reuse existing signed extensible INTEGER encoding. Known additions do not close future extension membership. Sparse root gaps remain rejected. Copy, cleanup and preflight validate new metadata; envelope scalar evidence rejects additions that its descriptor cannot own.
4. Anonymous constrained primitive IOC Value/Extension rows lift into complete owned primitive types. Declared/effective constraints are validated before publication. The SIZE(3) OCTET payload in RRC-Version-ExtIEs uses existing primitive generation; opaque Contents-only behavior is preserved.
5. A root-only extensible CHOICE retains explicit terminal-marker ownership and writes the root extension bit. Incoming unknown extension selections return sticky constraint_violation before root lookup; schemas with known CHOICE additions remain explicitly unsupported. EUTRA-Coex-Mode-Info has only root alternatives and a terminal marker.
6. Large bounded collections retain schema bounds and reuse existing segmented APER length primitives. Segments are capped at 65536 elements, with a final zero determinant after exact fragments. The existing collection budget is checked before reserve/allocation. MappingInformationtoRemove retains its 1..67108864 bound; no maximum-sized allocation is needed for the finite tests.

No APER runtime source or header changes are required for these capabilities. Existing <=65536 collection emission and INTEGER mapping output without explicit additions preserve historical generated output.

## Final validation

The final stable-source scan exited zero with no first-failure clusters. All source/tool fingerprints and the six frozen modules were unchanged through execution. Results are recorded in `tools/f1ap-readiness/readiness-f1-p2.json`.

| Gate | F1-P1 PASS | F1-P2 result |
|---|---:|---|
| Physical body extraction | 98 | 158/158 PASS |
| All three BODY generation families | 98 | 158/158 PASS |
| Strict C++20 body syntax compilation | 98 | 158/158 PASS |
| Target envelope descriptor extraction | 0 | 158 FAIL; F1-P3 scope |
| Independent complete-PDU wire qualification | 0 | 158 NOT_RUN; F1-P4 scope |

The inventory remains 94 procedures and 158 messages (94 initiating, 36 successful and 28 unsuccessful outcomes). Strict compilation uses g++ 13.3.0 with C++20, Wall/Wextra/Werror, pedantic-errors, conversion/sign-conversion warnings and NDEBUG. Per-message results include generated-header hashes. The Parser/Fixer tree is destroyed before generation. The final report input hashes were independently compared with current source files.

NGAP final regression passed physical extraction, all three BODY generation families and strict C++20 syntax compilation for all 131 messages. The unchanged baseline executable was run before rebuilding the shared implementation: its 14-message raw JSON and all 42 generated headers are byte-identical to final output. The 393-header intermediate-to-final comparison has only 20 whitespace differences and no token differences; that intermediate comparison is not a full baseline qualification.

Functional suites passed 53/53 with no skips or failures: libasn1typed 41, libaper 10, libngap 1 and tools 1. Frozen F1AP manifest/inventory tampering guards passed. Exact per-message NGAP results and input fingerprints are in `tools/f1ap-readiness/ngap-regression-f1-p2.json`; baseline comparison and test-log fingerprints are in `ngap-artifact-regression-f1-p2.json`. Historical accepted wire profiles remain unchanged. The full 3432-case native NGAP campaign was not replayed; no new NGAP wire qualification is claimed.

Focused tests include parser-independent nested inline ownership, parameterized collection object-set binding, constrained payload ownership and invalid metadata/constraint rejection. Independent expected bytes include `a6` for the nested inline graph, `000100020006000001000180` for a nested bound collection IOC graph, `000100030003112233` for the SIZE(3) selected payload, and `20`/`60` for extensible CHOICE roots. Unknown CHOICE extension `80` and empty input are rejected. INTEGER byte tests use an independent bit model for root, known and future extension values; large collection tests cover the existing length boundaries plus 65537 and 131072, repeated fragments and budget refusal with allocation failure armed.

## Independent review

Independent agent verdict: **PASS — Ready to Commit**, no blocking findings. The reviewer verified the full report/header/source hashes, frozen identities, NGAP regression and focused tests without modifying, staging, committing or pushing repository files. Details and finite validation limits are in `tools/f1ap-readiness/review-f1-p2.md`.

## Qualification boundary and next milestone

Successful physical extraction, BODY generation and strict C++20 syntax compilation are separate readiness gates. They do not establish full F1AP-PDU wire interoperability, complete value-space correctness, vendor compatibility, procedure policy or SDK delivery. The unknown extensible CHOICE branch policy above is deliberately explicit and finite.

The envelope extractor remains outside F1-P2: F1AP-PDU has four non-extensible root alternatives, including choice-extension. F1-P3 must derive this framing from F1AP owned evidence rather than copying NGAP's three-root extensible profile. Independent batch wire qualification remains F1-P4, followed by C++ SDK delivery and Python integration. Project order remains F1AP → E1AP → RRC.

No merge, branch deletion, force push, frozen schema modification or benchmark is part of this milestone.
