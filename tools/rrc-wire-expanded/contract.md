# RRC-P16 finite expanded wire qualification contract

Baseline: fc12b37d8d7527c5f2591c16a9728ec367e02114.
One objective: qualify the finite expanded real UPER profile before SDK delivery.
Developer-only tools/evidence: production, schema and historical receipts stay
immutable. Reuse P7 native descriptor/value generation and public-model bridge.
Pycrate0.7.11 compiles all six original authenticated modules unmodified; no
oracle tag/Contents behavior patch. C++ expectations derive from native semantics,
not generated codec bodies or IR wire instructions.

Regenerate all current 85-root headers and authenticate against the latest
NULL compile repair matrix. Preserve the old 64-root/143-real-case and 59-compound
corpus by identity and full value/normalized/wire/bit/offset/policy data.
Each selected new value must pass native encode to C++ decode with full owned
model equality and exact cursor, plus C++ encode to native decode with exact
bytes/bits and normalized semantics. Native CHOICE ordering audit remains active.
Opaque Contents qualifies carrier bytes only, not the contained payload codec.
Selected optional-feature witnesses require native descriptor/path checks and
explicit records; no all-field/all-value, SIZE-endpoint or nested-cartesian claim.
Frozen MIB/SIB2/MTC and newly reachable SIB1 are independently checked unchanged.
Retain P7 reversed-explicit-tag reference boundary and compound regression.

Report generated/strict/finite-qualified roots and selected/default/channel roles
separately. SDK delivery stays 64 roots, with P17/P18 expansion deferred. Bind
sources, producer archives, oracle implementations, corpus and execution logs
with before/after drift guards, then independently review the exact snapshot.
No full APER regeneration is necessary for developer-only changes; all historical
and production Git blobs must remain unchanged. No benchmark or merge.
