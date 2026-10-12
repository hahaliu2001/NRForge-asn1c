# RRC-P17 expanded installed C++ SDK contract

Baseline 4132fc6c3a574b7a21a1d1aa3989d3a07a713272; feature/f1ap-cpp-aper.
One objective: deliver the P16 finite 85-root profile as a sealed SDK 0.2.0.
Historical P8/P9 tools, receipts and SDK profile remain immutable. No schema,
extractor, renderer, runtime algorithm or Python SDK change. Regenerated headers
must exactly reproduce all 85 P16-qualified hashes. Namespace/include/linkage
adaptation preserves qualified bodies. Catalog covers 80 selected plus five
DEFAULT roots and all 12 channels; no blocked member in this selected ledger,
no full RRC claim. Public APIs and all three completion modes remain unchanged.

Install archive, 85 root headers, identity/catalog, provenance/manifest and
relocatable NRForgeRRC 0.2.0 / NRForge::rrc exports. Repeated generation must be
byte-identical. Shared immutable locks, source/generated inventories, real
85-root/170-wrapper link anchor, successful archive receipt and install rechecks
must hold. Source/header/archive/missing-file/receipt and omitted-wrapper negatives
remain required. Record GCC GC parameters and lld resources explicitly.

Prepare independent public models from unmodified authenticated pycrate0.7.11
and P16 semantic receipts. Installed consumers use public out-of-line wrappers,
not unchecked raw codec calls: 248 real cases, complete owned equality, exact
bytes/bits, octets/contained_bits/exact, malformed/truncated/padding and quota
checks (direct public depth, work-bit and input-octet quotas; remaining runtime
quotas retain P16 internal qualification only). Preserve canonical opaque-carrier and finite optional-path limits.
Relocate inputs, hide originals during build/run, audit compiler dependencies and
exports, test header/archive and expected-fingerprint mismatches. Acceptance
covers only the RRC installed SDK, as specified by the P17 batch plan.
Four-protocol coexistence belongs to P18. Native oracle is preparation
only, unavailable at installed execution. No benchmark, merge or P18 work.
Independent exact-snapshot review precedes commit and push.
