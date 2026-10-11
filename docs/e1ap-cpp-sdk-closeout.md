# E1-P5 — E1AP C++ SDK delivery closeout (2026-10-11)

Baseline: `96a4449b3dbd2d3c1911a9d5af5f087ad1c899a2` on
`feature/f1ap-cpp-aper`. E1AP C++ source SDK **0.1.0** packages the accepted
72-message / 40-procedure frozen TS 37.483 V18.6.0 Release 18 profile.

Consumers use `find_package(NRForgeE1AP 0.1.0 EXACT CONFIG REQUIRED)`,
`NRForge::e1ap` and `<nrforge/e1ap/...>` public headers. The separate
`libnrforge_e1ap.a`, public namespace/registry, package config, source/schema
provenance and fingerprint link guard reuse the existing protocol-aware SDK
machinery. SDK identity is a source integration consistency contract, not an
ABI or authenticity guarantee. No schema or codec semantics changed.

## Actual local evidence

`tools/e1ap-sdk/verification-summary.json` reconciles actual installed files,
archive receipt, ELF/symbol closure, isolated consumer receipts and log hashes.
The source SDK fingerprint is
`50ecb4d316c5910ca297cd2474a51f45ecfbaa0f14477877b3c3f5141ebad9e9`.

- The complete archive contains 76 production ELF objects and exactly one
  definition of each of the 72 registration functions. Its actual full registry
  link-check passes; the installed archive matches its successful build receipt.
- All 72 installed public typed slots construct, encode/decode, check BODY and
  metadata identity, exercise received criticality, and reject truncated/trailing
  input. Values are 71 empty IE containers and one local:0 opaque private entry;
  these are functional integration values, not mandatory-IE-valid applications.
- Six populated E1 Setup outcomes cover both CU-UP and CU-CP directions.
  Independent pinned pycrate 0.7.11 construction from the exact frozen modules
  matches accepted E1-P4 full bytes and semantic hashes. The installed public
  C++ example independently constructs BODY values, checks every selected leaf
  on receive and emits the complete native bytes (33/13/19 octets per direction,
  with request/response lengths swapped between directions).
- The original checkout, frozen source root, build workspace and install prefix
  are unavailable throughout the successful relocated consumer campaign.
  Actual dependency files and exported CMake paths exclude original inputs and
  private codec/mapping headers. Isolation restores renamed inputs on failure
  and refuses to overwrite recreated nonempty content or symlinks.
- Expected package fingerprint mismatch and mismatched full/PDU-only/message-only
  public headers reach the intended rejection gates. E1AP no-op consumers linked
  against genuine NGAP/F1AP development-core archives fail at the E1 identity
  guard. Three public core profiles compile/link/run in both E1-first and E1-last
  orders. Only E1 has the complete generated registry; NGAP/F1AP development
  cores use explicit rejected empty-registry fixtures. This is coexistence evidence, not a
  historical complete SDK rebuild/coinstallation or historical wire replay.
- The real shared seal verifier passes valid inputs and rejects extra/missing
  generated files, altered public header, altered production source, missing
  archive receipt and altered archive. Original deliverables are preserved.
- Independent fresh E1 generation/sealing preserves 516 public/production/identity
  files byte-exact and logical manifests; its SDK fingerprint is location-independent.
  All 362 E1-P4 qualified production outputs remain byte-identical, excluding
  CMake packaging and location-bearing manifest metadata.
- NGAP/F1AP baseline/current controller generation preserves 658/793 non-manifest
  files for all 131/158 messages, respectively, and logical manifests. Baseline
  controller source is compiled from the stated E1-P5 baseline with unchanged
  typed libraries. Shared functional suites pass APER 10/10, protocol registry
  3/3 and typed IR/render/extraction 42/42.

Initial coexistence-harness attempts exposed a missing generated registry
fixture and an out-of-prefix test-header dependency. Both failed campaigns were
retained and excluded from acceptance; the corrected complete isolation campaign
passes with explicit rejected fixtures and one relocated public include prefix.

The source build, committed examples, consumer/negative/reproducibility/regression
scripts, machine-readable receipts and focused CI workflow are delivered in the
repository. CI requires the read-only private-schema token; no remote CI PASS
is claimed. Local toolchain is GNU/Linux, GCC 13.3.0, strict C++20/O0, CMake 4.4.4
and GNU gold. No benchmark is performed. Native fixture preparation alone
requires pycrate; installed CMake/C++ consumers do not.

## Retained boundary and review

The E1-P4 finite 1,341-vector complete-PDU profile is retained, not replayed or
expanded by SDK packaging. Unreached typed sizes/fragmentation, nested combinations,
future typed extensions, vendor/contained semantics, application policy and live
interoperability remain unqualified. Historical NGAP/F1AP acceptance remains
unchanged. Independent acceptance is recorded in `tools/e1ap-sdk/review.md`.
Next scope: **E1-P6 — Python SDK delivery**.
