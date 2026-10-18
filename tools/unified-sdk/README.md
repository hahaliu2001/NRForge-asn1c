# One current ASN1 SDK

Current candidate version: 0.3.0. Source implementation only; native generation,
builds, oracle execution, installed tests, independent review, Owner acceptance
and publication have NOT_RUN/PENDING status until actual terminal receipts exist.
No new artifact exists merely because source code is pushed.

NRForge-asn1c owns the four-protocol C++ bundle and its NGAP Python wheel.
The bundle installs NGAP, F1AP, E1AP and RRC at the same version and exposes
`find_package(NRForgeASN1 0.3.0 EXACT CONFIG REQUIRED)` / `NRForge::asn1`, as well
as protocol-specific targets. Four archives inside one bundle are not separately
maintained SDK releases. RAN consumes the bundle; 5GC consumes the wheel. Their
pinned consumer locks are emitted from actual artifact bytes, never placeholders.

## Selected scope

- NGAP: all 26 explicit CONTAINING Transfer roots in the frozen NGAP-IEs module,
  public owned headers and Python converters. The existing outer 131-message API
  remains. Catalog: python/ngap_sdk/root_catalog.py; production extraction retains
  the existing shape, closure and root-specific extension-binding checks.
- RRC: the historical 85 roots plus CellGroupConfig, UE-NR-Capability,
  UE-MRDC-Capability, CG-Config, CG-ConfigInfo and HandoverPreparationInformation.
  All 91 must generate and link; no partial installation. A failed extraction or
  render writes blocked-roots.json and stops. New roots are not mislabeled with
  P16's historical finite wire qualification. Source APIs are generator-owned;
  generated headers are created by Owner's later terminal run.
- F1AP: the fourth nonextensible PDU root is a ProtocolIE-SingleContainer with
  a 16-bit id, criticality, and owned opaque OPEN TYPE payload. The frozen object
  set has no known rows. It is distinct from typed outcome bodies and unknown
  CHOICE additions. `Registry::make_choice_extension` constructs it; decode
  returns PduKind::choice_extension and `choice_extension()`. No outcome header
  is exposed for it. Raw payload bytes are preserved; no invented known type.
- E1AP: no new root admitted without evidence. The producer inventories explicit
  CONTAINING in all six authenticated modules and stops if new targets appear.
  The source read-only audit saw none in IEs/PDU-Descriptions; no full protocol
  coverage claim follows. Its outer 72-message profile remains finite.

Transfer bindings retain the existing 4096-octet/32768-bit finite profile.
Adding roots does not remove this limit or prove all standard values work.
RRC byte carriers remain bytes; RRCSetup/masterCellGroup composition has its own
oracle-derived installed-consumer case. Cross-protocol carrier decoding remains
an explicit caller operation, not an implicit F1AP or NGAP recursion.

## Owner terminal execution, after source review

Requires native Ubuntu/Linux x86-64, CPython 3.12, GCC 13, lld, Autotools,
CMake, make, Python venv/pip, and delegated cgroup v2 user systemd. No sudo,
WSLg, benchmarks or unbounded fallback. Do not run another large build alongside.

Only ASN1 producer code is executed. Frozen standard modules can be staged with
read-only git-show from the repository that historically supplied them; its
checkout is not modified and its producer/consumer tools are never invoked:

```bash
cd "$HOME/jie_github/NRForge-asn1c"
python3 tools/unified-sdk/stage_schema.py \
  --schema-repository "$HOME/jie_github/NRForge-RAN" \
  --output "$HOME/asn1-frozen-schema-03"
bash tools/unified-sdk/run.sh "$HOME/asn1-03-resources" \
  --schema-root "$HOME/asn1-frozen-schema-03" \
  --work "$HOME/asn1-03-candidate"
```

Every output directory must be new. Inputs are authenticated again before the
build; arbitrary current schema edits are refused. The snapshot identifies the
exact committed ASN1 source, excluding historical verification binaries and
local editor settings. The entire producer descendant tree must verify at most
6 GiB memory, no swap, 128 tasks and 150% CPU. All compiler/link work is serial;
lld uses one thread. Failures retain evidence and do not retry or publish.

The pipeline builds/seals/installs four components, compiles independent public
NGAP/RRC root consumers, audits dependencies against original input paths,
executes finite F1AP/E1AP outer-slot regressions, verifies F1AP fourth-root oracle
witnesses/native cases, builds one NGAP wheel and installs it in a fresh venv,
checks 131 historical outer vectors plus installed root bindings, and links the
four protocols together. The accepted native 250-check campaign remains at its
original source/hash; this runner does not invoke that campaign or relabel it.
New installed cases may reuse its finite fixtures for a different API boundary.

## Separate packaging, publication, migration and retirement

No build command calls a release/deletion API. After a PASS_LOCAL_QUALIFICATION:

```bash
python3 tools/unified-sdk/release.py package \
  --work "$HOME/asn1-03-candidate" --output "$HOME/asn1-03-release"
```

Packaging rechecks SDK files, wheel and qualification report digests and emits
candidate.json plus actual sdk-consumer-lock.json and wheel-consumer-lock.json.
The candidate SHA256 is the review/acceptance identity. `publish` is a separate
command requiring an acceptance JSON with `candidate_sha256`,
`independent_review: "PASS"`, and `owner_acceptance: "ACCEPTED"`. These fields
record decisions; they are not cryptographic signatures. No acceptance receipt
is synthesized by the producer. It creates a new ASN1 draft, refuses existing
release/tag collisions, uploads without clobber and hashes remote bytes before
making the release visible. Failed writes are not retried automatically.

RAN/5GC copy the respective exact consumer lock into their own dependencies and
use consume.py to download/hash-check assets. CMake integration and 5GC receive
policy remain consumer work; no producer sources or private application source
are embedded in the SDK. Source changes alone do not migrate a running consumer.

Owner has authorized deleting old SDK/r1 assets after replacement validation and
both consumers migrate. `retire` requires the publication receipt, both consumer
migration PASS records bound to candidate SHA/version, and an explicit inventory
of old asset repository/id/name/size/SHA256. It rechecks identities before DELETE,
refuses the current release's assets, and never deletes tags or Git history.
Old producer entry points are retired now; old deployed assets are not deleted
by this source-only batch. Historical acceptance records remain evidence.

## Review boundary

The current change is a source candidate, not a qualified release or protocol
completion. Native generation can expose previously unseen closure/renderer
limitations in the newly selected roots. The runner stops with named failures;
it does not silently omit those fields/roots or fall back to bytes for a root
promised as typed. Any such failures must be fixed before this batch is accepted.
Independent review should focus on root identity/closure, F1AP framing, receipt
integrity, complete wheel/bundle inventories, consumer-only dependencies and
publication/retirement state transitions.
