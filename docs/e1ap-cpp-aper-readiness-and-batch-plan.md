# E1-P1 — E1AP C++ APER readiness and batch plan

Status: **Complete / Accepted (2026-10-11)**; no E1AP production codec expansion.
Baseline: `3df8651d3d4a8e835a04ede4bde9d609fd6e0ced`, after accepted F1-P6.
Work branch: `feature/f1ap-cpp-aper`; no merge or force push.

## Frozen authority and complete inventory

Consume six unchanged E1AP modules from NRForge-RAN commit
`d6e514a33ec3695925c24aa292900514b371b074`, rooted at `src`.
The protocol-local `src/e1/asn1/README.md` identifies **TS 37.483 V18.6.0
Release 18**, APER, with accepted source extraction frozen. Stale DOCX core
properties mentioning TS 38.463 Release 16 are explicitly not authority.
This study verifies the accepted module bytes, not the original official
archive acquisition/extraction. Ordered module sizes, Git blobs and SHA-256
are recorded in `tools/e1ap-readiness/source-manifest.json`.

Combined input: 249,289 bytes, direct concatenation SHA-256
`5cfd3832772f7898360ba1a6d3a09c5ca9c78c3d9a97e6b5e009880bfcc685b9`.
The two elementary procedure root sets reconcile with **40 procedures**,
unique procedure codes 0–39 and **72 declared messages**: 40 initiating,
20 successful, 12 unsuccessful. Includes PrivateMessage and both CU-CP/CU-UP
E1 Setup directions. Selection is the complete frozen inventory, not a sample.

## Measured gates

| Gate | PASS | FAIL | NOT_RUN |
|---|---:|---:|---:|
| Physical extraction | 70 | 2 | 0 |
| BODY generation (all three families) | 70 | 0 | 2 |
| BODY strict C++20 syntax compile | 70 | 0 | 2 |
| Target-envelope descriptor extraction | 72 | 0 | 0 |
| Target-envelope generation (all three families) | 70 | 0 | 2 |
| BODY + target-envelope strict syntax compile | 70 | 0 | 2 |

Compiler: GCC 13.3.0, C++20, `-Wall -Wextra -Werror -pedantic-errors
-Wconversion -Wsign-conversion -DNDEBUG -fsyntax-only`. All 140 eligible
translation-unit invocations pass without diagnostics. The two masked messages
have no BODY/envelope generation or compile invocation; their descriptors alone
pass. No message has new wire qualification.
Parse/Fixer pass for the six-module tree. The parser tree is destroyed before
owned physical evidence is rendered. Each eligible BODY and target envelope
has type, mapping and codec generation families separately recorded.
No production extraction, renderer, runtime, dispatch or SDK source is changed.

## First shared gap and masked dependencies

`DataUsageReport` and `MRDC-DataUsageReport` both stop first at
`E1AP-IEs.usageCountUL: unsupported inline INTEGER constraint`.
The source declarations `DRB-Usage-Report-Item` and
`MRDC-Data-Usage-Report-Item` contain `usageCountUL` and `usageCountDL`, each
`INTEGER (0..18446744073709551615)` (full unsigned 64-bit range).
The observed failure count is two messages; it is not a prediction of two
messages becoming fully qualified after a fix. Downstream dependencies,
including `usageCountDL`, are masked by the first failure.

Source inspection identifies a shared representation boundary: Parser has a
wider integer representation on this build, but extractor `constraint_bound`
rejects values above `INTMAX_MAX`; owned INTEGER ranges use signed `intmax_t`.
The integer renderer also restricts upper bounds to `INT64_MAX`. Existing
`uint64_t` generated types and bounded-uint runtime APIs alone therefore do
not prove end-to-end full uint64 constraint support. E1-P2 must establish that
whole semantic path, preserving signed intervals and extension behavior.
It must inspect full-cardinality arithmetic, APER large-range length/payload,
minimal representation, malformed inputs, transactionality and budgets rather
than narrowing or rewriting the frozen schema. The runtime already implements
bounded uint64 domains, including full-domain tests in `libaper/check_integer.cpp`;
this finding is not evidence that an unsigned wire primitive is missing.

## PDU integration is a separate profile gate

The frozen `E1AP-PDU` has three root alternatives (initiating, successful,
unsuccessful) followed by `...`. All outcome headers contain procedureCode,
criticality and a role-specific IOC value selected by procedureCode.
Thus the outer choice is extensible, unlike F1AP's nonextensible fourth root.
E1AP nested choices may still contain choice-extension containers; inspect
owned descriptors for each site rather than assigning one outer policy globally.

Target-envelope evidence is local to one message and does not qualify complete
PDU dispatch. `tools/ngap-dispatch/generate.c` currently supports only NGAP and
`--f1ap`; there is no delivered E1AP registry/runtime/package profile. Reuse the
accepted APER/IOC machinery with a distinct E1 identity and explicit profile.
Verify role dispatch, both Setup directions, unsupported procedures/outcomes,
criticality policy, outer extension rejection/preservation contract, PrivateIE
local/global identification, decode rollback, budgets and consumed bits.
Do not silently route E1AP through NGAP or F1AP namespaces/schema locks.

## Ordered execution batches

| Batch | Scope | Exit evidence |
|---|---|---|
| E1-P1 | This frozen-source readiness study | Complete inventory, separate gates, shared gaps and independent review |
| E1-P2 | Shared full unsigned64 INTEGER constraints across owned IR, renderer and runtime as needed | Boundary/malformed/budget tests; rescan all 72; preserve signed semantics; NGAP/F1AP targeted regressions |
| E1-P3 | E1AP outer PDU profile, all procedure/outcome dispatch and build integration | All 72 generated and compiled; linked registry; profile identity; supported/rejected outer behavior and transaction tests |
| E1-P4 | Bounded full-PDU wire qualification | Explicit finite native-reference corpus for all 72, boundaries/extensions/negatives; separately state unqualified branches |
| E1-P5 | C++ SDK delivery | Frozen schema/package locks, install/export and external consumer, reproducible source delivery |
| E1-P6 | Python SDK delivery | Distinct package/module, bindings/model conversion/errors/budgets, clean wheel install and external consumer |

Each batch uses internal implementation/review/fix loops within the owner's
existing autonomous authorization. Discover further shared gaps after E1-P2
and revise batch scope from measured evidence; do not implement per-message
workarounds or advertise unconditional 72-message unlocks. RRC follows E1AP
with its own source and encoding/profile study.

## Limits and retained evidence

`tools/e1ap-readiness/readiness.json` records every message, compiler version,
strict options, hashes and diagnostics. `README.md` gives reproduction steps;
`review.md` records independent verification. Compilation is syntax-only, not
linkage or runtime qualification. Ordinary inventory/use sites do not exhaust
bound-instance internals. No new APER bytes, native-reference comparison,
interoperability, benchmark, ABI matrix or SDK release is claimed. Historical
F1AP/NGAP reports and accepted wire qualification remain unchanged.


## E1-P2 shared unsigned64 completion

All 72 frozen E1AP messages now pass physical extraction, all three BODY
and target-envelope generation families, and both strict compilation gates
(144 syntax-only invocations). In particular, DataUsageReport and
MRDC-DataUsageReport retain exact 0..UINT64_MAX bounds for both usageCountUL
and usageCountDL. No further extraction/generation/compile blocker is observed
in this scan. This does not qualify complete PDU dispatch or runtime bytes.

E1-P2 appends explicit finite unsigned endpoint evidence, validates ownership
and residue, preserves named refinements and inline SEQUENCE/CHOICE mappings,
and reuses the existing bounded_uint runtime. Signed intervals/sets/extensions
retain their domain. High unsigned sets/extensions and mixed negative-to-wide
ranges remain unsupported and are rejected. See
[E1-P2 contract and closeout](e1ap-cpp-aper-unsigned64-contract-and-closeout.md)
and `tools/e1ap-readiness/readiness-e1-p2.json`.

Typed IR checks pass 42/42 and APER checks pass 10/10. Regression regenerates
all 158 F1AP and 131 NGAP messages, with 948 F1AP headers and 393 NGAP BODY
headers byte-exact against historical acceptance, plus ten representative
strict BODY/envelope compilations. See `ngap-f1ap-regression-e1-p2.json` and
`review-e1-p2.md` for the regression and independent acceptance receipts.
Next implementation scope: E1-P3, explicit E1AP outer profile and dispatch.
