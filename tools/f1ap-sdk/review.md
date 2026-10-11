# F1-P5 independent review

Verdict: **ACCEPTED — no blocking findings** (2026-10-11).

Reviewed the shared protocol-aware seal/CMake packaging, public installed include
rewriting, F1AP build and native fixture preparation, all-slot consumer,
coinstallation and identity negatives, strong source-path isolation/restoration,
archive consistency negatives, evidence reconciliation, CI and closeout scope.
This reviewer did not stage, commit, push or rerun the qualification campaigns.

## Audited actual evidence

`verification-summary.json` SHA256:
`3cea90da8a7814cc4761396ffe3e5621586c47cfe3655e12ed01b34c50e8e3c2`.

- Summary reports PASS for 158 messages/94 procedures, 162 production ELF
  archive members and exactly 158 registration definitions. Build receipt,
  installed archive identity and consumer/negative evidence bind the same SDK.
- Independently recomputed all 52 recorded run-log hashes, all 39 sealed source
  fingerprints, all 328 installed file hashes and exact installation file closure.
- Independently compared 792 F1AP historical qualified production generated files
  and 656 NGAP qualified production generated files with the actual outputs.
- Read actual successful logs: all 158 installed F1AP slots; both include/link
  orders with three populated F1Setup and three NGSetup outcomes each; standalone
  NGAP consumer with three outcomes. F1Setup reference sizes are 24/18/18 octets.
- Recomputed hashes of final functional logs: libaper 10/10, libngap 2/2 and
  libasn1typed 41/41. Seven seal cases are complete and identity-bound: valid
  snapshot exits zero; six deliberate inconsistencies exit nonzero.
- Verified original checkout, schema, both build workspaces and install prefixes
  were restored. Installed dependency/configuration audits and guard negatives
  are supported by retained logs and reports.

Earlier review suggestions were addressed: protocol-qualified public includes,
full dependency private-header rejection, consumer and seal evidence identity
binding, explicit symlink refusal during empty-scaffold recovery, and gold/lld
undefined diagnostic compatibility with exact guard symbols still required.
Immutable archive hardlinks are read-only; negative archive writes unlink first.
Only test executables are stripped. Resource-recovery failures are retained and
are not counted as successful acceptance.

## Limits

The 158 all-slot values establish finite public SDK integration, including empty
IE containers; they do not establish mandatory-IE application validity. Three
populated F1Setup fixtures compare accepted independent reference bytes and
selected receive fields. F1-P4's 4,728-case finite wire qualification is retained,
not replayed or expanded. No exhaustive wire, live vendor, fourth-root payload
semantic, binary ABI, performance, Python SDK or E1AP claim is accepted here.
The supplied GitHub workflow has not been represented as a remote CI PASS.
