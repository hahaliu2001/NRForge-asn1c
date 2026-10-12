# RRC-P17 expanded C++ SDK delivery

Baseline: 4132fc6c3a574b7a21a1d1aa3989d3a07a713272.
Target branch: feature/f1ap-cpp-aper. Actual checkout at record creation:
detached HEAD at the baseline; the target branch is not the active local branch.

The parallel SDK 0.2.0 packages the P16 finite 85-root profile with public owned
models, an actual archive containing 170 out-of-line encode/decode wrappers,
all 12 selected channels and exact-version relocatable NRForgeRRC / NRForge::rrc
CMake exports. Historical SDK 0.1.0 tools and receipts remain immutable.
Only packaging, acceptance tooling, documentation and EXTRA_DIST are added;
production schema/extractor/renderer/runtime algorithms remain unchanged.

Raw regenerated headers match every P16-qualified hash. Repeated generation
reproduces the complete 180-file output byte for byte. Locked inventories cover
producer source and actual linked generator artifacts. A real 85-root anchor
must resolve both public wrappers before the archive earns a build receipt.
Installation rechecks the archive, receipt and source/generated inventories.

Independent native pycrate0.7.11 descriptors prepare complete expected models
for 248 accepted real cases. Consumers call installed public wrappers and check
owned equality, exact octets/semantic bits, octets/contained_bits/exact completion,
input mutation after decode, truncation, trailing bytes, padding and direct
public depth/work/input quota failures. Remaining quotas retain P16 internal
qualification only. Supplied original repository/schema/build/prefix/preparation
paths are temporarily hidden during relocated compilation and execution;
compiler dependency inventories exclude extra unhidden source trees/oracles.

P17 acceptance is limited to the RRC installed SDK and its separate MIB
example, matching the authoritative remaining-blockers batch plan.
Four-protocol coexistence belongs to P18. Fingerprint and header/archive
mismatch rejection remain required, as do the seal mutation, missing
inventory/receipt and omitted encode/decode wrapper negatives.

Environment: GCC13.3, CMake3.28.3, lld18.1.3, 8GiB memory ceiling. One build job,
GCC GC expand5/heapsize16384 and lld threads1 are recorded resource controls.
Large workspace archive copies were observed to become incomplete. Completed archive bytes were pinned outside that surface and
verified against their build receipt before acceptance. The dedicated seal
archive copy is removed only after all checks and restored-hash verification,
with logs and the hash receipt retained. No benchmark or merge is performed.

Final acceptance status and exact evidence hashes are recorded in
../tools/rrc-sdk-expanded/verification.json; exact-snapshot independent approval
is recorded in ../tools/rrc-sdk-expanded/review.md before publication.
The selected profile is finite: full RRC, arbitrary inner-carrier codec
qualification, every optional/SIZE/cartesian value and stable cross-toolchain
binary ABI/authenticity are not claimed. P18 Python expansion is separate work.

## Acceptance evidence and review state

Technical acceptance: PASS; independent review: PENDING. P17 is not yet
marked complete; no commit or push has been performed for this delivery.

The resumed RRC-only execution passed all 85 root executables, 248 exact
public wire/model cases and 5,510 public negative checks. The separate
installed MIB example passed. Dependency audit covers 86 translation units
and 395 distinct allowed dependency paths. Wrong expected fingerprint and
mismatched header/archive identity both rejected as required. All five
original input paths were unavailable during this execution and restored
successfully afterward.

The 85 root consumers were reused from the completed build; root074 through
root084 were completed in the previous continuation. This acceptance did not
repeat their compilation. The separate MIB example was configured, compiled
and tested using CMake 3.28.3 discovered in the installed Python package.
The resumed run does not claim a fresh full configure/build/CTest run for all
root consumers. Direct execution validates each root's complete expected
CASE and NEG transcript.

Runtime transcripts, restoration journal and resume adapter are retained in
`tools/rrc-sdk-expanded/runtime-acceptance/`; their SHA-256 inventory is in
`evidence-sha256.json`. The adapter executes the existing isolation checks
while skipping already completed root configure/build steps. Final record
creation rechecks preparation/source identities, installed and relocated
closures, 170 archive wrappers, repeated generation, seal receipts and the
previous native UPER regression evidence without rerunning those tests.

The canonical source/verification snapshot is prepared for an independent
read-only review. Approval must bind that exact snapshot before commit/push.

## Independent review corrections

The first independent review requested changes for missing source-distribution
evidence and inaccurate checkout-state reporting. EXTRA_DIST now explicitly
includes all 94 runtime-acceptance files. The source inventory also includes
the nested runtime verification.json; only the top-level final verification
and review files are excluded to prevent self-referential hashes.
The final record separates target_branch from observed checkout_state.
Technical acceptance remains PASS; the revised snapshot awaits focused review.
