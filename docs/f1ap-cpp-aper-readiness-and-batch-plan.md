# F1-P1 — F1AP C++ APER readiness and batch plan

Date: 2026-10-10. Baseline: `2e6d3b4a6779d248dfc8849e0f70da2b0d512336`. Branch: `feature/f1ap-cpp-aper`.

## Frozen authority

Six unchanged F1AP modules from NRForge-RAN commit `d6e514a33ec3695925c24aa292900514b371b074`, TS 38.473 V18.10.0 (Release 18). The source manifest records module order, byte lengths, Git blob identities and SHA-256. All were checked before and after the scan. No production extraction, runtime, renderer or SDK semantics were changed in F1-P1.

Independent source inventory reconciles 94 procedures with their two root procedure sets and 158 distinct message declarations: 94 initiating messages, 36 successful outcomes and 28 unsuccessful outcomes. Every outcome is included in the same batch.

## Observed gates

| Gate | PASS | FAIL / not run |
|---|---:|---:|
| Combined Parser/Fixer source set | 1 | 0 |
| Physical message extraction | 98 | 60 FAIL |
| Owned body types + mapping + codec generation | 98 | 60 NOT_RUN |
| Strict C++20 body syntax compilation | 98 | 60 NOT_RUN |
| Target envelope descriptor extraction | 0 | 158 FAIL |
| Independent wire / full-PDU qualification | 0 | 158 NOT_RUN |

The Parser/Fixer tree was destroyed before body generation. Compiler: g++ 13.3.0, with C++20, Wall/Wextra/Werror, pedantic-errors, conversion/sign-conversion warnings and syntax-only. The per-message report contains exact diagnostics, all generated header hashes, scan input hashes and source identities. There were no body-generation or strict-compilation failures among the 98 physically extracted messages.

This establishes reuse of existing NGAP compiler/runtime machinery for 98 bodies; it does not deliver 98 qualified F1AP codecs or a full-PDU API.

## Shared first-failure groups

| Shared evidence gap | Observed affected messages | Examples / evidence |
|---|---:|---|
| Parameterized SEQUENCE OF element references | 46 | F1SetupRequest: GNB-DU-Served-Cells-List; reset connection lists; DRB/MRB/cell lists |
| Inline constructed CHOICE payloads | 6 | TransmissionComb.n2 (4); TransmissionCombPos.n2 (2), both inline SEQUENCE payloads |
| Inline constructed SEQUENCE fields | 4 | freqBandListNr (1), listofDL-PRSResourceSetARP (2), pRSTransmissionOffIndicationPerResourceList (1) |
| Extensible INTEGER extension union | 4 | SRBID ::= INTEGER (0..3, ..., 4 \| 5) |

These counts partition the 60 observed extraction failures. They are first failures, so additional dependencies may appear after each shared fix. They are not estimated unlock counts. `tools/f1ap-readiness/readiness.json` retains the original per-declaration diagnostic clusters and every affected message.

All 158 envelope extractions fail at `root physical class/selector evidence`. Source inspection explains the structural mismatch: F1AP-PDU has four root alternatives, including `choice-extension`, and no CHOICE extension marker. The existing envelope extractor requires exactly three roots plus an extension marker. F1AP framing must therefore be derived from its own evidence; the NGAP outer framing cannot be copied unchanged. No framing implementation was attempted here.

## Execution plan

1. **F1-P2 — shared body gaps:** support parameterized collection element binding, owned lifting of inline constructed fields/alternatives, and the SRBID extension union as shared IR/runtime/generator capabilities. Add focused semantic tests, rescan all 158, and repeat until new dependency failures are exhausted. Preserve NGAP regression behavior. Do not implement a separate codec for each message.
2. **F1-P3 — complete F1AP-PDU integration:** model the fourth root alternative and non-extensible outer CHOICE from owned evidence, expose all procedure outcomes and a registry dispatch API, with explicit unknown/unsupported behavior.
3. **F1-P4 — batch wire qualification:** independent reference comparisons across the full inventory, complete-byte vectors, populated IE/payload coverage, errors and budgets, with documented coverage limits. Compilation is not qualification.
4. **F1-P5 — C++ SDK delivery:** installed public headers/libraries, relocated consumer tests and NGAP/F1AP coexistence; parameterize NGAP packaging without mixing identities.
5. **F1-P6 — Python SDK:** reuse established native binding/conversion/install tests with an F1AP profile and full message registry.

Owner's autonomous execution preference applies: internal implementation/review/fix loops do not require routine approval. Scope-changing decisions, destructive actions and merges remain separate. E1AP follows F1AP; RRC follows E1AP and needs its own encoding/profile evidence.

## Validation and limitations

- Frozen identity and source inventory guards passed, including source tampering and missing body/code declarations; inventory generation was deterministic.
- The old and parameterized-default NGAP probe outputs for NGSetupRequest, NGSetupResponse and NGSetupFailure were byte-identical.
- All 158 F1AP identities were scanned; all 98 generated bodies compiled under strict C++20.
- Exact probe return codes distinguish physical extraction, generation and envelope evidence. No successful process status is used to claim all-message support.
- Ordinary raw inventories omit bound-instance internal bodies. Earlier errors mask later dependencies. Shared diagnostic text is not proof of identical semantics.
- No runtime byte tests, external differential, performance benchmark, SDK packaging qualification or sanitizer sweep was performed in F1-P1.

Reproduction commands and evidence interpretation: `tools/f1ap-readiness/README.md`. Independent review and final local verification are recorded in `tools/f1ap-readiness/review.md`.


## F1-P2 completion (2026-10-10)

F1-P2 shared body capability work is complete. The stable-source scan now passes
physical extraction, all BODY generation families and strict C++20 syntax
compilation for all 158 outcomes, with no first-failure clusters. Newly exposed
shared dependencies were resolved as constrained selected primitives,
root-only extensible CHOICE and large bounded collections in addition to the
four initial groups. No per-message codec implementation or frozen schema edit
was introduced.

NGAP retains 131/131 extraction/generation/strict compilation PASS, 14-message
baseline raw output and 42 generated headers are byte-identical, and 53/53
functional tests pass. Complete-PDU F1AP wire qualification remains NOT_RUN;
all 158 target envelope extractions still fail at the F1-P3 framing boundary.

Current evidence: [F1-P2 closeout](f1ap-cpp-aper-shared-body-closeout.md),
`tools/f1ap-readiness/readiness-f1-p2.json`, and the separate NGAP regression
reports. The historical F1-P1 report above remains unchanged. Next: **F1-P3**,
complete F1AP-PDU integration; then F1-P4 batch wire qualification, F1-P5 C++ SDK
and F1-P6 Python SDK. E1AP and RRC follow F1AP.

## F1-P3 completion (2026-10-10)

F1-P3 shared complete-PDU integration is complete. Owned evidence supports
F1AP's four-root non-extensible CHOICE; the parameterized fourth root is owned,
validated and explicitly rejected before any ordinary procedure header.
Profile-separated public APIs reuse the NGAP registry/ownership/transaction
implementation. The controller reconciles all 94 procedures and 158 outcomes,
rejects missing closure, and emits byte-identical complete outputs on repeat.

All 158 messages pass separate BODY and envelope extraction/generation/strict
compilation gates. All 474 BODY headers remain byte-identical to F1-P2. The
complete adapter library builds, links and passes a finite all-158 typed-slot
runtime check. Functional tests pass 55/55. NGAP retains 131/131 readiness,
baseline artifact equality and existing installed/relocated SDK behavior.
Independent agent review accepts the final evidence.

These are integration gates, not independent wire qualification. All 158 wire
statuses remain `NOT_RUN`; the finite values are 157 empty-container BODY values
and one vendor-opaque PrivateMessage entry. No mandatory-IE policy or populated
payload interoperability is implied. F1-P5 installed F1AP SDK is not delivered.

Evidence: [F1-P3 closeout](f1ap-cpp-aper-pdu-integration-closeout.md),
`tools/f1ap-readiness/readiness-f1-p3.json`,
`tools/f1ap-readiness/dispatch-integration-f1-p3.json`, and
`tools/f1ap-readiness/review-f1-p3.md`. Historical P1/P2 evidence is unchanged.
Next: **F1-P4**, independent batch wire qualification; then F1-P5/F1-P6.
E1AP and RRC remain after F1AP.

## F1-P4 completion (2026-10-10)

Independent finite complete-PDU wire qualification is accepted for all 158
message identities and 94 procedures. The actual full public registry campaign
passes 4,728 complete-byte and bidirectional semantic cases, all 975 declared
top-level IE row occurrences, 330 SRBID extension-union cases, 8,781 main
physical/ownership/resource checks and 610 receive-only unknown/absent slots.
Separate independent framing evidence contributes 2,619 checks, not additional
main semantic cases. Independent review approves the exact candidate hash.

No production fix or frozen-schema edit was required. P3 readiness evidence
remains unchanged and is not relabelled as a new scan. NGAP freshly passes 55/55
functional tests, and 14 raw inventories, 42 BODY headers and 659 dispatch
artifacts remain byte-exact. Historical NGAP wire reports are unchanged.

Acceptance is finite: main vectors are 13–330 octets; repeated optional-omission
traces, unreached typed fragmentation/sizes/combinations, application/mandatory-IE
policy, RRC/NAS/vendor interpretation and fourth-root payload semantics are not
qualified. Immediate fourth-root refusal remains a negative policy check.
No installed F1AP SDK or benchmark is included.

Evidence: [F1-P4 closeout](f1ap-cpp-aper-wire-qualification-closeout.md),
`tools/f1ap-wire-qualification/accepted-profile.json`,
`tools/f1ap-wire-qualification/qualification-summary.json`, and
`tools/f1ap-wire-qualification/review.md`.
Next: **F1-P5** C++ SDK delivery, then F1-P6 Python SDK; E1AP and RRC follow F1AP.

## F1-P5 completion (2026-10-11)

The independent F1AP C++ SDK 0.1.0 is accepted for all 158 messages and 94
procedures. It reuses the qualified NGAP packaging machinery with separate
`NRForgeF1AP` / `NRForge::f1ap` interfaces, provenance and header/library guards.
Both protocols coinstall in one prefix and coexist in either include/link order.

Actual relocated consumers pass with original source, schema, build and install
inputs unavailable: all 158 finite F1AP typed slots, three independently sourced
populated F1Setup complete-PDU outcomes, both complete registries (158/131), and
both protocols' Setup outcomes in each coexistence order. Wrong fingerprint,
wrong header/library identity and a genuine wrong-protocol archive are rejected.
Actual seal negative checks and shared functional gates pass. Independent review
accepts the reconciled actual archive, installed file and consumer evidence.

The 792 F1-P4 qualified production generated files remain unchanged; the 4,728
historical finite wire cases and their boundaries are retained rather than rerun
or expanded. No frozen schema, codec semantics or historical qualification
report was changed. NGAP retains qualified production output equality and its
complete installed registry/consumer checks. No benchmark or binary ABI promise
is included.

Evidence: [F1-P5 closeout](f1ap-cpp-sdk-closeout.md),
[SDK contract](f1ap-cpp-sdk-contract.md),
`tools/f1ap-sdk/verification-summary.json`, and `tools/f1ap-sdk/review.md`.
Next: **F1-P6**, the separate Python SDK task. E1AP remains a later independent
protocol task; no merge is part of this acceptance.
