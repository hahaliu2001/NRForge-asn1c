# E1-P1 independent review — 2026-10-11

Reviewer: independent agent `/root/f1p6_review`.
Final verdict: **ACCEPT — Complete / Accepted, study scope**.
No blocking findings.

Independent checks PASS:

- Pinned per-module lengths/Git blob/SHA-256, deterministic inventory and
  source-tamper/missing declaration/code guard tests.
- Direct concatenation: 249,289 bytes, SHA-256
  `5cfd3832772f7898360ba1a6d3a09c5ca9c78c3d9a97e6b5e009880bfcc685b9`.
- Complete 40-procedure/72-message reconciliation, unique codes 0–39,
  roles 40 initiating/20 successful/12 unsuccessful.
- Separate extraction, generation and compile gates; masked gates are not PASS.
- Frozen E1AP outer PDU has three root alternatives and an extension marker.
  No F1AP fourth-root assumption is inherited.
- Actual parser configuration preserves wider integers; the observed unsigned64
  blocker is the signed endpoint boundary in extraction/owned IR and rendering,
  not demonstrated absence of an APER runtime primitive.
- Plan and reproduction instructions correctly distinguish target-envelope
  readiness from complete PDU dispatch, finite wire qualification and SDKs.

Nonblocking hardening suggestion: validate declared combined manifest fields
inside verify(). Deferred for this study: all module identity gates pass and
independent concatenation verification establishes both combined fields.
Scanner inputs were kept unchanged during the scan to preserve its fingerprints.

Final evidence reconciliation PASS: all 72 report rows match fresh frozen-source
inventory and raw probe rows. Every retained input hash and generated BODY /
envelope header hash matches. All 140 eligible compile logs are empty with
return code zero: 70 BODY and 70 BODY+envelope compilations. The two extraction
failures have generation/compile NOT_RUN, while all 72 envelope descriptors
extract. Independent guard-test rerun and strict Reset BODY+envelope translation
unit compilation PASS. Compiler, gate counts and batch plan are accurate.

Staged scope contains study documentation/tools and EXTRA_DIST registration
only. No production codec, frozen schema or historical qualification change.
No new wire, interoperability or SDK claim is made.
