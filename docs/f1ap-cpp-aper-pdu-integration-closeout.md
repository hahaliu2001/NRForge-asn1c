# F1-P3 — complete-PDU integration closeout

Baseline: `0ad019c340934391861e8c80ba5c3e7253adfef5`.
Branch: `feature/f1ap-cpp-aper`. Frozen TS 38.473 V18.10.0 authority and
all six schema identities are unchanged. No benchmark was run.

## Shared implementation

Owned envelope evidence now distinguishes F1AP's four-root, non-extensible
CHOICE from NGAP's three-root extensible CHOICE. The parameterized fourth
`choice-extension` root is owned and validated but explicitly unsupported;
decode rejects immediately after its two-bit selector, before any procedure
header. This root is not an outer CHOICE extension addition.

Shared registry/ownership/transaction machinery exposes distinct
`nrforge::f1ap` and `nrforge::ngap` APIs. The profile-aware controller generates
all 94 procedure identities and 158 outcome adapters (94 initiating, 36
successful, 28 unsuccessful), lightweight public BODY headers and one complete
registry. Missing closure is rejected without publishing outputs. Unknown
procedure codes and absent outcomes are receive-only owned opaque values for
extensible procedure sets; malformed known values never become opaque.

Independent review caught an empty unsupported-root reference comparison
that incorrectly rejected otherwise matching schemas. The fix only permits
matching empty optional scalar references at this bounded unsupported root;
required references remain strict. A direct comparator regression was added.

## Evidence

- Full stable-input scan: 158/158 PASS for physical extraction, BODY generation,
  strict BODY compilation, envelope extraction, envelope generation and strict
  envelope compilation. All 474 BODY headers remain byte-identical to F1-P2;
  474 additional envelope headers are compiled. No first-failure clusters.
- Separate repeated complete registry generation produces 794/794
  byte-identical files, including the reconciled manifest. Public-only
  integration TUs also compile under strict C++20 for all 158 slots.
- Functional suites: 41 typed, 10 APER runtime, two registry and two tooling
  tests, all 55 PASS. Focused four-root complete-byte/framing/error tests and
  ASan/UBSan registry tests also pass; no LeakSanitizer sweep is claimed.
- NGAP: 131/131 extraction/generation/strict compilation PASS; 14 baseline raw
  inventories, 42 BODY headers and 659 complete dispatch artifacts unchanged.
- Complete F1AP archive: all 158 adapters plus registry/profile/runtime objects
  build successfully. A fresh strict all-slot consumer links and runs all 158
  typed construction/encode/decode/identity/criticality/re-encode cases and
  truncation/trailing rejection checks. The accepted run uses two public-TU
  compile jobs and the system GNU gold linker, recorded in the report.
- Existing NGAP SDK seal, build, registry link check, install and relocated
  three-outcome consumer pass. The installed shared declarations include is a
  real consumer dependency. Direct header guards and a clean exclusive
  wrong-library negative fail with the expected undefined guard symbol and
  normal linker status, without OOM. The full public checker passes all 112
  absent outcomes, unknown codes and outer extension receive-only cases.

The first all-slot test link rejected a zero-byte object despite compiler
status zero; no PASS report was published. The runner now verifies ELF output
after every compilation. A single-TU replay and the fresh complete accepted run
pass. Diagnostic build interruptions, SDK resource failures and the old
resource-contaminated negative verifier receipt are excluded from acceptance.
The existing SDK verifier's receipt alone does not establish a clean negative;
the additional actual clean replay is recorded explicitly.

Machine-readable scan evidence is in
`tools/f1ap-readiness/readiness-f1-p3.json`,
`dispatch-integration-f1-p3.json`, `ngap-regression-f1-p3.json`, and
`ngap-artifact-regression-f1-p3.json`.
Reproduction: `tools/f1ap-readiness/README.md`. API semantics and exclusions:
[integration contract](f1ap-cpp-aper-pdu-integration-contract.md).
Independent acceptance: `tools/f1ap-readiness/review-f1-p3.md`.

Accepted F1AP integration report SHA-256:
`2c0a024ed2d63e62821b94af5d5ba7a1addcf90050bf6995c50626e7d711d8d5`.
Actual linked archive SHA-256:
`9adc4035e09161473ea504f5b59b649220401f5b9536b4a21b416bffd7f2a4f9`.

## Qualification boundary and next phase

This is integration readiness, not wire qualification. Finite all-slot tests
use 157 default empty-container BODY values and one vendor-opaque
PrivateMessage entry to satisfy its nonzero minimum SIZE. Same-implementation
roundtrips do not prove populated payload interoperability or required-IE
procedure policy. All 158 wire-qualification statuses remain `NOT_RUN`.

Next is **F1-P4**: independent reference comparisons, complete-byte vectors,
populated IE/payload batches, negative cases and budgets with explicit coverage
limits. F1-P5 installed C++ SDK and F1-P6 Python SDK remain separate. Project
order is F1AP → E1AP → RRC.
