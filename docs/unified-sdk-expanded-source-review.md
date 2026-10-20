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

## B1 provenance identity correction

The independent read-only review of 9696461d..3f1cb904 found one deterministic
blocker: produce.py read every component version at the provenance top level,
whereas RRC's producer writes identity.version. Even successful component builds
would fail at the merge identity gate before installed/wheel qualification.

The correction reads the actual per-protocol contracts through
read_component_identities: flat identity for NGAP/F1AP/E1AP, nested identity for
RRC. Every one of the four components remains mandatory. Version must equal the
unified candidate version, source_revision must equal the committed snapshot
being built, and fingerprint/schema/runtime hashes must be 64 lowercase hex
characters. A missing RRC identity never falls back to unrelated top-level data.
The existing bundle component_versions field retains its original output shape.

Author execution: eight offline Python regression tests PASS (temporary JSON
fixtures only, no network/build/subprocess execution by the tests). They cover
the actual flat/nested contracts, mixed RRC and APER versions, source mismatch
for each component, missing RRC identity, missing component, malformed digest
and malformed nested identity. git diff --check passes. These results close the
author-side B1 regression only; they do not claim SDK generation, compilation,
installed qualification, wheel creation, publication or Owner acceptance.
Targeted independent read-only re-review concluded: B1 CLOSED, no new blocker.
The reviewer inspected the source and regression cases without running tests
or builds and without modifying files. This is source-review closure only;
full native SDK/wheel qualification and Owner acceptance remain pending.

## NGAP standalone header failure and bounded recovery (2026-10-19 UTC)

Owner's Ubuntu build at source `589781c1d2647da2cce5675612e9c6ab08909255`
failed compiling `sdk_link_check.cpp`, reporting absent standalone encoders. The
reported tail alone does not establish the earliest compiler diagnostic. Source
review identified a concrete self-containment defect: `root.hpp` included types,
mapping and codecs before either runtime header, although these generated headers
use runtime types. Link checking includes roots before the protocol umbrella.
Root entries now include `runtime.hpp` and `sequence_extensions.hpp` first.
A small GCC syntax test also reproduced skipped root namespaces when all root
forwarding headers have identical contents and `#pragma once`, even after adding
runtime includes. Entries now use a distinct include guard per catalog stem.
The regression includes all 26 entries together and independently verifies that
removing runtime includes or restoring identical pragma-once headers fails.
The initial proposal to add `::codec::` to public calls was withdrawn after
independent review: owned compound public APIs live in the root namespace.
Their existing API and manifest conventions are preserved.

`produce.py --resume-ngap` validates the original SDK lock and changed source git
preimages before mutation. It allows only the repair/recovery/test/review files;
archives the original producer report, command failure, compiler log, seal,
changed source files, and all 26 old root headers in `namespace-recovery/`.
It updates the reviewed source delta and entry headers, reseals with the new
revision, configures the same build directory, builds serially and installs.
Raw adapters and their object paths are retained for dependency-based reuse;
identity/check objects and affected dependencies may rebuild. It then continues
all remaining protocols, installed consumers and wheel gates. No failed run or
historical accepted campaign is relabeled PASS. Normal resource limits still
apply. Recovery cannot be rerun in the same evidence directory after failure.

No native Ubuntu qualification, publication or Owner acceptance is claimed here.

Repair validation: 22 lightweight checks passed, including a small GCC
`-fsyntax-only` compilation with all 26 entry fixtures against the real runtime
headers, two failing defect mutations, and mocked recovery evidence/object
preservation. This does not build the SDK or rerun the accepted native campaign.
The independent read-only reviewer passed the final source repair and recovery
flow with no new confirmed blocker. Actual Ubuntu recovery remains pending.

## Split standalone checks under the 4 GiB build limit (2026-10-19)

Owner's recovered build reused the 131 adapters and linked the NGAP archive, but
its combined 26-root check stayed in one GCC process for six hours. The supplied
process snapshot showed `D/mem_cgroup_handle_over_h`, 29 minutes CPU time, about
3.72 GiB cgroup memory versus a 3.5 GiB soft threshold and 4 GiB hard limit,
~80% full memory pressure, and zero OOM events. This is sustained memory-high
reclaim/throttling; no VM/platform change is assumed to resolve it.

The sealer now emits one Transfer check translation unit per root. A small main
calls every helper and retains the registry/identity check. CMake links all 26
helpers into the same executable, executes that executable, then writes the
build receipt as before. Generated helper sources and their CMake list are all
hashed in the SDK lock; they are neither omitted from the gate nor exempted from
integrity checks. Compilation remains serial. Splitting reduces the aggregate
per-file input but does not certify the peak usage of each real root.

`--resume-ngap-split` resumes only the stopped first recovery before installation.
It rejects active compiler/build processes using the work path, excluding its own
supervisor ancestry; obtains the current baseline from the sealed provenance,
and requires it to match the previous recovery migration. It verifies the old
seal, saves old evidence in `split-check-recovery/`, checks changed-source git
preimages, and reseals/configures/builds at the existing paths. It preserves the
first recovery directory, all raw root headers, adapter sources and objects.
All remaining unified gates still execute. The original recovery remains a
separate mode. A second interrupted recovery keeps its evidence and is not
silently overwritten or retried.

Split repair validation: all 26 lightweight checks passed. The 26 helper fixtures
were actually compiled serially, linked and called through a small CMake project
using the real runtime header; this is not an SDK/native protocol qualification.
Seven recovery checks include supervisor ancestry, stale compiler refusal,
current-seal baseline selection and preservation of previous recovery evidence.
Independent final read-only review closed the supervisor-blocking issue and
passed the final split implementation with no new confirmed blocker. Actual
Ubuntu SDK execution, peak memory, wheel and release qualification remain pending.

## Anonymous RRC enum collection elements (2026-10-20 UTC)

Owner's WSL run `asn1-unified-wsl-nxFcGZdf` completed the three APER SDK build
and install stages, then stopped before RRC installation. Two roots failed
extraction: CellGroupConfig through PDSCH-HARQ-ACK-CodebookList-r16, and
UE-NR-Capability through FeatureSetDownlink-v1830's
pdcch-RACH-AffectedBands-TargetBandList-r18. Both frozen ASN.1 declarations use
`SEQUENCE OF ENUMERATED`. The extractor handled direct constrained INTEGER/BIT
STRING list elements and inline constructed types, but not anonymous enum list
elements. The repair promotes these enum elements to owned `@element` paths
only in the ordinary RRC graph profile. Existing enum extraction continues to
validate all labels, extensions and constraints. APER profile behavior is not
expanded by this branch.

The new C regression checks five graphs (direct enum list, two independent
inline enum lists, extensible enum, nested list, SetupRelease specialization),
then deletes the Parser tree before inspecting owned enums and rendering UPER.
It also sweeps ownership extraction allocation failures. Native execution is an
Owner-terminal gate; source syntax checking is not recorded as a native PASS.

`check_rrc_closure.py` provides an isolated snapshot with serial C builds and
the new and existing collection-element regressions, followed by selected
91-root sealed generation. The existing builder must still reject incomplete
closure and require the historical 85 header hashes to remain unchanged. The
gate writes a distinct report and never alters the three already built SDKs,
the failed unified workspace, or historical qualification. Its success would
be generation evidence only, not an installed/wire-qualified SDK or a release.
A full resumed qualification is not claimed or implemented by this diagnostic
gate; the previous NGAP-specific recovery modes do not resume this RRC failure.

### Parser assertion in the new regression input

Owner's isolated check `asn1-rrc-closure-fZTwKNaO` built the C libraries/tests,
then aborted parsing the new fixture at `asn1p_y.y`'s nested collection constraint
assertion. The added direct list-of-list fixture has been replaced with a list
of records containing an enum list, matching the nested shape already used by
the accepted collection-element fixture. The test still checks the nested owned
element path (`$inline$Nested$@element$values`). The parser is not changed and
this change makes no claim to support the direct constrained list-of-list form.
The enum collection extraction repair remains unchanged.

`check_rrc_closure.py --resume-tests` only accepts the recorded enum parser
assertion before probe/generation stages. It requires git preimages for a narrow
allowlist of test/runner/review changes, archives the original report and source
preimages in `test-input-recovery/`, and uses separate command logs. It rebuilds
the changed test using existing C libraries, reruns both tests, then proceeds to
91-root generation. The original failure command directory is preserved. It
does not resume or modify the original unified SDK workspace.
