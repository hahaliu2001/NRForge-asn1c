# F1-P3 independent read-only review

Baseline: `0ad019c340934391861e8c80ba5c3e7253adfef5`.
Branch: `feature/f1ap-cpp-aper`. Review performed by a separate agent.

## Final verdict: PASS — Ready to Commit

Production implementation, stable readiness evidence, the complete linked
F1AP registry/runtime integration and NGAP SDK preservation pass review.
No unresolved blocking findings remain. This establishes the finite F1-P3
integration gates, not independent F1AP wire qualification.

## Independently verified evidence

- Final F1AP report SHA-256:
  `c244994cb60b84ab01f55e71d5a3448c0742b019ae59676c6a41dfa58562369c`.
  All 158 unique identities pass physical BODY extraction, three BODY generation
  families, strict BODY compilation, envelope extraction, three envelope
  generation families and strict envelope compilation. Parser/Fixer destruction
  precedes generation. The 316 compilation logs are empty; all 948 header hashes
  and every current input fingerprint match. Failure clusters are empty and all
  158 wire-qualification statuses remain `NOT_RUN`.
- All 474 F1AP BODY header hashes match the committed F1-P2 report exactly.
  All six frozen module sizes, Git blob identities and SHA-256 values match the
  unchanged source manifest.
- Full generated F1AP manifest SHA-256:
  `95ee7a02f278d4dedb4fcad7a611d33cce3bc4f2c12c7665f699cb2fc7023337`.
  Its 94 distinct procedure codes and 158 unique outcome slots reconcile with
  the independent frozen inventory, including every role/code/criticality.
  Roles total 94 initiating, 36 successful and 28 unsuccessful. All 794 expected
  files exist; a separate complete generation is byte-identical for all 794.
  The incomplete inventory rejects missing declared payload closure.
- NGAP final readiness has 131 physical/generation/strict-compilation passes.
  All 393 generated header hashes and current input fingerprints match.
  Actual baseline/final files were independently compared: 14-message raw output
  and 42 BODY headers are byte-identical; 659 dispatch artifacts are unchanged
  except allowed manifest source-path provenance normalization.
- Fresh strict-warning F1AP registry tests built and ran with ASan/UBSan,
  `detect_leaks=0`. Three roles, four-root selector remapping, immediate
  unsupported-root refusal, binding isolation, registry lifetime, copy/move,
  absent/unknown receive-only slots, malformed known BODY, and input/retention
  limits pass. NGAP registry tests and the actual controller schema-comparison
  regression were also independently executed successfully.
- Actual final suite logs show 55/55 PASS with zero skips/failures/errors:
  41 Typed IR, 10 APER runtime, two registry and two tooling tests. The Typed IR
  suite log SHA-256 is
  `12bd27a3c897dac710edc7af1d1e6c778ee2c60af0188897f448bf6f4b1bf6d2`.
  Its focused generated-envelope log explicitly records three/four-root framing
  success and exit zero, after the new test source modifications. This evidence
  comes from completed suite records, not incomplete shell-output capture.
- Existing NGAP SDK source fingerprints include both shared `.inc` files and
  match current sources. Installed `pdu_declarations.inc` matches its sealed
  public copy. The installed-only relocated three-outcome NG Setup consumer was
  independently rerun successfully; source/binary/output hashes and dependency
  paths match, without repository header fallback.
- The clean exclusive SDK wrong-library replay rejects the exact expected
  `sdk_require_ffffffff...` symbol with `ld returned 1 exit status`, without a
  kill, signal interruption or fatal linker error. Its log SHA-256 is
  `cc8c91818b5c3daa7cc150daa338eb088676cf434324e7a1610fcc09d7e1f362`.
  Both direct public-header mismatch negatives also fail cleanly. The installed
  and relocated archive hashes match the build receipt; the deliberately changed
  identity header was restored byte-exact. The full NGAP public registry test
  was independently rerun: unknown codes, all 112 absent outcome slots and
  unknown outer extensions retain their receive-only policy.
- Complete linked F1AP integration report SHA-256:
  `2c0a024ed2d63e62821b94af5d5ba7a1addcf90050bf6995c50626e7d711d8d5`.
  The fresh guarded run contains all 158 typed slots, 157 empty-container BODY
  values and one local:0/raw00 private opaque entry. Received criticality,
  same-implementation roundtrip and truncated/trailing input rejection pass.
  All 158 strict compilation logs are empty and all consumer objects have ELF
  magic. Every current input/archive fingerprint matches; the executable hash
  is `f3b312e9a5dad5b76d18aa2b5f9ba095545410a3c1b8ce3524867116146f1654`.
  The reviewer independently reran that exact executable successfully. The
  report records two compile workers, gold and `--no-keep-memory` link options.

## Findings resolved during review

The initial F1AP controller still passed the NGAP BODY module to envelope
extraction; it now selects the F1AP module. Fourth-root parameterized reference
validation now checks kind, nonempty base/actual identities, unsupported scalar
absence and physical field uniqueness before comparison. Role-to-PER mappings
are derived across all owned roots rather than assuming the unsupported root
is source ordinal three. Finally, intentionally empty unsupported-root scalar
references compare only as structurally empty values; required references
remain strict and a direct controller regression covers the distinction.

Public protocol identities are separate, framing profiles cannot be mixed,
and the fourth F1AP root is rejected after its two-bit selector before reading
any ordinary procedure header. Known failures do not fall back to opaque data.
Shared runtime ownership, model-token checks and known-open transactions retain
the NGAP semantics. Packaging copies, fingerprints and installs the newly
shared declarations include.

## Completed gates and finite limits

- Complete F1AP archive linking and all-158 runtime integration: PASS.
  The archive contains 161 distinct object members (158 adapters plus registry
  and shared runtime/PDU objects). An initial integration attempt encountered
  a zero-byte public consumer object and published no report; it is not
  acceptance evidence. The checker now verifies ELF object magic after compiler
  success. Only the subsequent fresh guarded full run is accepted.
- NGAP SDK preservation: PASS, using the clean exclusive negative above. The
  original verifier receipt alone is insufficient because its wrong-library log
  contained both the expected missing guard symbol and a signal-9 linker kill.
  The subsequent uninterrupted replay supplies the negative acceptance evidence;
  resource-interrupted negatives are not accepted.
- No F1AP independent wire comparison, populated mandatory-IE coverage, live
  vendor interoperability, installed F1AP SDK, full NGAP native campaign,
  LeakSanitizer sweep or benchmark is claimed. A separate actual-schema reviewer
  diagnostic was stopped before completion and is not counted as test evidence.

The reviewer changed no production/test implementation, staged nothing and
committed/pushed nothing. This review record was created only at the root
agent's request. Scratch probes were used for independent checks.
