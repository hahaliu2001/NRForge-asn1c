# F1-P1 independent review and validation

2026-10-10. Verdict: **PASS — Ready to Commit** for the read-only readiness tooling and batch plan. This is not a codec interoperability verdict.

An independent review agent performed read-only source/tool/report checks and did not modify, stage, commit or push repository content:

- Verified the six source manifest identities and inventory reconciliation: 94 procedures, 158 messages, roles 94/36/28, body identities, procedure codes and source criticality/default evidence.
- Reconciled all 158 report rows with the raw probe results; exact first-failure groups account for all 60 extraction failures; 98 physical/body-generation/strict-compilation successes and 158 envelope failures are consistent.
- Verified baseline commit, current scan-input hashes and all 98 sets of generated header hashes.
- Independently recompiled F1SetupFailure (004) and CUDUMobilityInitiationRequest (157) with strict g++ syntax checks, both PASS.
- Verified source and extractor root shape: four non-extensible F1AP roots versus the existing three extensible-root envelope contract.
- Checked the batch plan's four shared groups (46/6/4/4), source examples, scope, limitations and absence of per-message implementation planning.

Root agent validation: full 158-message scan; strict compilation of all 98 generated bodies; source guard/deterministic inventory tests; byte-identical old/current-default NGAP probe outputs for NGSetupRequest/Response/Failure; `make -C tools check` 1/1 PASS; whitespace checks. Existing Automake subdir-objects warnings were observed during regeneration and are not introduced by this change.

No clang++, sanitizer sweep, runtime vectors, independent wire qualification or benchmark was run for this milestone. Review did not repeat the entire scan. Optional future guard hardening: explicit total procedure-union/code-uniqueness assertions. Frozen source hashes already constrain the scanned inputs; the current source inventory was independently reconciled.


Subsequent shared implementation is independently reviewed in [F1-P2 review](review-f1-p2.md). The historical F1-P1 verdict and report above remain unchanged.
