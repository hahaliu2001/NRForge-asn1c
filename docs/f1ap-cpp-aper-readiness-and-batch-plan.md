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
