# RRC-P8 installed C++ SDK contract

Baseline `0bc5b860ffc11378f88dbf5a0bef07b1d0c8f908`, branch
`feature/f1ap-cpp-aper`. SDK 0.1.0 packages the authenticated P6/P7 supported
finite profile: 59 selected roots plus five DEFAULT seeds, seven channel roots,
three bare seeds. The manifest carries every requested identity and all 21
blocked selected roots. Blocked roots have no public header or codec entry.

Export `find_package(NRForgeRRC 0.1.0 EXACT CONFIG REQUIRED)` and `NRForge::rrc`,
`libnrforge_rrc.a`, namespaced headers under `nrforge/rrc`, relocatable exports,
source/schema/qualification provenance and a fingerprint link guard on every
public entry header. Public root namespaces use the original module/root with
hyphens replaced by underscores. Root models remain P6 Tn owned models; each
root header supplies out-of-line `encode` / `decode` resolved from the archive.
The existing `nrforge::uper` bit-aware runtime is unchanged; RRC models live in
`nrforge::rrc`. `rrc.hpp` exposes identity/catalog/runtime, not all 64 root headers
in one translation unit. No ordinary RRC generic PDU/procedure registry is invented.

Retain exact P7 semantics and bit counts, optional logical-bit limits and
`Completion::{octets,contained_bits,exact}`. Catalog discovery does not imply
runtime dynamic schema dispatch or application support for every value.
Source/schema/generated locks, a real executable resolving all 64 archive root
objects and a successful archive receipt gate install. Installed consumers need
only C++20/CMake and installed files. Verify all 143 accepted real cases using
public encode/decode, exact owned values/bytes/bits and contained framing; retain
frozen seeds. Qualifying new values or removing semantic gaps is outside P8.

Copy consumer sources and relocate prefixes; make original repo, schema, SDK
build/generation and install paths unavailable during configure/build/run. Audit
non-system compiler dependencies and exported paths. Exercise expected fingerprint,
header/archive pairing, source/generated mutation, archive mutation and missing
receipt rejection. Install NGAP/F1AP/E1AP/RRC together and link/test all four public
APIs in one executable; no source-tree or raw core fallback. Independent review
precedes commit/push, no merge or benchmark. Python remains P9.
