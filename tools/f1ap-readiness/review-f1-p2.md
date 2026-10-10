# F1-P2 independent review and final validation

2026-10-10. Verdict: **PASS — Ready to Commit**. Baseline `8b741dad60645916beab0545ce6ca1ce5d1768a9`, branch `feature/f1ap-cpp-aper`.

The independent review agent made no repository modifications, staged no files, and performed no commit or push. Its test harnesses used scratch files. This verdict covers shared body capabilities and finite functional/readiness evidence; it is not a full F1AP wire interoperability verdict.

## Independent review evidence

- Reviewed parameterized collection binding, anonymous constructed owned identities and recursive closure, synthetic generated-name collision checks, constrained selected primitive lifting, declared/effective constraint preflight, INTEGER known-addition ownership/copy/cleanup and fail-closed metadata guards.
- Reviewed root-only extensible CHOICE marker evidence, preserved canonical root tag order and explicit unknown-extension rejection; reviewed large collection segmentation, maximum 65536-element chunks, final zero determinant and budget charging before reserve.
- Re-ran generated INTEGER domain, current inline/IOC/CHOICE and large collection scripts successfully. Independently checked fixed bytes, unsupported inputs and preallocation budget refusal.
- A scratch ASan/UBSan rejection harness passed with `detect_leaks=0`. LeakSanitizer could not run in the process environment; no leak-sweep claim is made.
- Independently reconciled all 158 final F1AP report rows: extraction rc=0, all three generation-family rc=0, strict compilation PASS/rc=0; all 158 compile logs empty; all 474 generated-header hashes match the exact report. No full duplicate scan was required for review.
- Independently matched all current input fingerprints and the six frozen module sizes, Git blob SHA-1 and SHA-256 identities; reconciled 94 procedures, 158 messages and roles 94/36/28. First-failure clusters are empty. All 158 envelope failures and wire NOT_RUN statuses are correctly scoped to future milestones.
- Independently verified final NGAP 131-message success and current input fingerprints; compared baseline 14 raw JSON exactly and all 42 generated headers byte-for-byte. The 393-header intermediate comparison is explicitly not a full baseline comparison.
- Reviewed closeout, capability matrix, execution-plan completion and README reproduction instructions. `git diff --check` passed. No remaining blocking finding.

Issues found during implementation review—INTEGER root/addition emission order, complete metadata cleanup, constructed-constraint rejection and scalar envelope evidence that cannot own additions—were resolved before final acceptance.

## Root validation and qualification boundary

Final functional tests: **53/53 PASS**, zero failures/skips (libasn1typed 41, libaper 10, libngap 1, tools 1). Frozen-source/inventory guard tests passed. Stable scans: **F1AP 158/158** extraction, generation and strict compilation; **NGAP 131/131** for the same gates. Parser/Fixer destruction before generation and final source/tool stability guards passed. No APER runtime source/header or frozen schema changes.

Exact reports are `readiness-f1-p2.json`, `ngap-regression-f1-p2.json` and `ngap-artifact-regression-f1-p2.json`. Historical F1-P1 evidence and accepted NGAP wire profiles are preserved. No benchmark, full 3432-case native replay, complete F1AP-PDU wire qualification, SDK packaging or vendor interoperability campaign was performed.

Next milestone: F1-P3, F1AP-PDU integration with four non-extensible roots. F1-P4 remains the independent batch wire qualification milestone.
