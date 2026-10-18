# Unified expanded SDK source candidate

Baseline: 9696461d81ed1fea13f679f8c36cf3e11b5637b0 on
feature/ngap-transfer-owned-roots. This is one source implementation batch for
the Owner's expanded scope, not native qualification or acceptance.

## Change

NGAP's physical ordinary-root selection expands from three to 24 explicitly
named roots. SetupRequest and ModifyRequest use the established single IE
container path. All 26 roots are catalogued, generated as independent public
entries, sealed in SDK provenance and covered by Python conversion generation.
Token-based root type naming replaces the PDUSessionResource-only assumption.
Frozen explicit CONTAINING targets must match the catalog exactly.

The unified RRC producer requests the historical 85 roots plus six named roots,
requires owned extraction/rendering for all 91, preserves historical raw-header
hash checks, seals the resulting wrapper archive and installs an exact-version
package. New roots carry NOT_RUN wire-qualification metadata; no P16 acceptance
is attributed to them. No generated files are claimed to exist yet.

F1AP's fourth PDU alternative now owns a distinct single-container representation
(id/criticality/opaque payload), with construction, copy/move and encode/decode.
It cannot acquire a regular outcome header or typed BODY, and foreign-registry
encoding is refused. Its id is 16 bits and criticality is constrained; unknown
retention and wire/output budgets use the existing APER runtime. Native tests
cover full values, reordered root indices, truncation, trailing data, invalid
criticality and budgets. Independent frozen-schema oracle witnesses are supplied
for terminal execution. The empty frozen object set has no known typed payloads.
The target-specific generated envelope API still selects only its own outcome;
the shared registry API is the full outer PDU surface.

One maintained 0.3.0 SDK bundle contains four protocol libraries and one umbrella
CMake target. The NGAP wheel is built from python/, not a parallel four-root-only
project. Existing profile staging excludes NGAP-only exports for F1AP/E1AP.
Old standalone RRC and old transfer full-delivery entries stop immediately.

ASN1 owns bounded snapshot builds, installed consumers, wheel construction,
packaging, immutable release upload/readback and eventual old-asset retirement.
Build/qualification cannot call publication or deletion. Candidate locks contain
actual bytes' hashes. Release requires an exact-candidate independent-review and
Owner-acceptance record. Retirement additionally requires both consumers' PASS
migration records and a reviewed old-asset identity inventory. No current asset,
Git tag, branch or history is deleted by that operation.

## Author source checks performed

- Python AST parsing for 27 modified/new Python source files.
- Bash syntax parsing for tools/unified-sdk/run.sh.
- Source consistency: exactly 26 catalog roots; exactly 24 ordinary extractor
  names plus two established IE-container roots; six RRC additions.
- git diff --check.
- Remote baselines rechecked. 5GC independently moved to e2288fd5, removing its
  old three-repository orchestration; that concurrent change is preserved.

No build, generation, oracle execution, C++ syntax compilation, unit tests,
installed tests, wheel creation, benchmark, publication, old-asset deletion,
mainline merge or independent agent review has run in this batch. Deferred
native/offline regression source exists; it is not reported as executed.

## Required next evidence

Consolidated independent read-only source review, then Owner's bounded Ubuntu
execution. Newly selected root dependencies may reveal unsupported graph or
renderer features only during generation; such failures are blockers for this
batch, not permission to omit fields, install a partial SDK, relabel bytes as a
typed root, or claim all gaps closed. Runtime semantics, memory behavior and
end-to-end delivery remain unverified until that evidence exists.

The accepted four-root 250-check campaign remains exactly at source 460c790 and
its recorded report digest. Its full campaign is not rerun. Installed consumers
reuse finite independent fixtures at a different boundary, plus new descriptor
cases. No full-standard or exhaustive-values coverage claim is made.
