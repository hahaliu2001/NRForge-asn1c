# E1-P5 — installed E1AP C++ SDK contract

Baseline: `96a4449b3dbd2d3c1911a9d5af5f087ad1c899a2`, work branch
`feature/f1ap-cpp-aper`. Package the accepted frozen TS 37.483 V18.6.0
Release 18 registry: 72 messages, 40 procedures. No schema or codec change.

SDK 0.1.0 exports `find_package(NRForgeE1AP 0.1.0 EXACT CONFIG REQUIRED)`
and `NRForge::e1ap`; public headers are under `nrforge/e1ap`, with a separate
archive, namespace, runtime identity, package provenance and fingerprint guard.
The existing shared seal and CMake consistency machinery accepts an explicit
E1AP profile. Source/schema/generated locks, full-registry link gate and
archive receipt must pass before installation. Installed consumers require
only the installed files, C++20 toolchain and CMake, without Python/generator
or a schema checkout. Fingerprints establish consistency, not authenticity
or a stable cross-toolchain binary ABI.

Acceptance verifies unchanged E1-P4 production generation (except CMake and
location-bearing manifest), all 72 public installed slots, and six populated
native-reference E1 Setup fixtures covering both CU-CP and CU-UP directions.
Fixture bytes and semantics must match the accepted E1-P4 profile. All-slot
empty-container values are functional integration evidence, not valid
mandatory-IE application messages. Run consumers from a relocated prefix
with original source/schema/build/prefix paths unavailable. Audit compiler
dependencies and exported CMake paths. Exercise expected-fingerprint and
public-header/archive mismatch rejection and the actual shared seal's
source/generated/archive mutation negatives. Preserve NGAP/F1AP controller
output and run shared APER, protocol registry and typed functional suites.

Deliver reproducible source build and consumer instructions, committed
examples and machine-readable receipts. Independent read-only review and
necessary fixes precede commit/push; no merge. Retain the finite E1-P4
qualification limits: no exhaustive values/fragmentation/typed extensions,
application policy or live interoperability. Python delivery is E1-P6;
no benchmark or historical full SDK rebuild is required or claimed here.
