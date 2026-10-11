# RRC-P8 installed C++ SDK closeout

Baseline: `0bc5b860ffc11378f88dbf5a0bef07b1d0c8f908` on
`feature/f1ap-cpp-aper`. SDK 0.1.0 exports `NRForgeRRC` / `NRForge::rrc`.
P8 packages the accepted finite P6/P7 profile without changing runtime or renderer
algorithms, schema authorities or historical evidence.

| Delivery gate | Evidence |
|---|---|
| Requested ledger | 85 identities: 64 supported, 21 blocked |
| Supported profile | 59 selected plus five DEFAULT seeds; seven channel roots |
| Header qualification preservation | All 64 raw P6/P7 hashes unchanged |
| Installed archive API | 128 encode/decode wrappers, 64 root objects, runtime and SDK identity |
| Repeat generation | All 138 generated files byte-identical in a new workspace |
| Source/build consistency | 126 producer inputs sampled before generation, checked after generation/install; shared CMake immutable lock |
| Install guard | All wrappers referenced by volatile pointers in a real linked executable; archive receipt rechecked on install |
| Negative gates | Eight shared seal checks plus two omitted-wrapper link failures; installed expected-fingerprint and header/archive rejection |
| Relocated public consumers | 64 executables, 143 real cases, 2,498 inherited negatives, three completion modes; 143 logical-bit truncation and 143 zero-depth failures; independent MIB example; 66 compiler dependency TUs audited |
| Four installed protocols | Shared-TU installed RRC/NGAP/F1AP/E1AP compile/link/run PASS; 131/158/72 registry closure and three accepted populated APER setup wires plus MIB |
| Regression | UPER 3, APER 10, PDU 3, typed 46, parser 1; all PASS |

Public headers are under `nrforge/rrc`; module/root names retain ASN.1 identity
with hyphens replaced by underscores. Models preserve the P6 Tn owned structures.
Explicit per-root headers supply public out-of-line encode/decode from
`libnrforge_rrc.a`; the algorithms remain the accepted generated inline bodies.
`rrc.hpp` exposes runtime/identity/catalog without requiring every root in one TU.
There is no invented RRC procedure/PDU dispatcher. The installed manifest includes
all supported and blocked identities; blocked roots receive no codec/header.

APIs retain `Limits`, optional logical bits and `Completion::octets`,
`contained_bits`, `exact`. Results include semantic/encoded bits, padding and
empty-value substitution. Input logical bits and zero-depth quota failure are
checked for every accepted real case. The consumer uses complete independently
constructed public model equality, exact P7 bytes and bits, three completion modes
and inherited bounded truncation/padding/budget/rollback negatives.

The original six authenticated modules are compiled unmodified for consumer value
preparation with pycrate 0.7.11; preparation verifies the stored native P7 semantic
receipts and qualifies the same 143 real cases. Prepared source hashes and the
installed qualification identity are checked before running. Native tooling is
not needed by the actual relocated C++ build.

The isolation harness copies SDKs and ordinary consumer sources, temporarily
renames the repository, schema, all four original SDK build/install paths and
preparation inputs, then restores them in `finally`. In a clean environment with
package registries disabled, CMake discovers only installed packages. Compiler
non-system dependencies must belong to the relocated installs or copied consumer
sources. CMake exports are checked for original-path leaks. A small committed MIB
example also configures/builds/runs using just the RRC install.

The mixed executable includes NGAP/F1AP/E1AP/RRC public APIs in one translation
unit, links actual installed archives, checks 131/158/72 APER registry closure,
one populated accepted setup wire/typed message identity per APER protocol and
23-bit MIB. NGAP/F1AP SDKs are freshly generated/built/installed from their accepted
profiles; E1AP reuses the accepted installed E1-P5 SDK. F1AP's large
UEContextModificationRequest compilation uses a single build job and GCC garbage
collection flags solely for compiler memory use. No codec/header/schema workaround
is made. The mixed consumer uses LLVM lld 18.1.3 with one linker thread after GNU
linkers exceeded the local 8 GiB memory limit. Prior resource-limit build/link
attempts are not advertised as passing checks.

Eight copied-input seal cases exercise accepted/restored baselines, source and
public generated changes, missing/extra generated files, archive changes and
missing receipt using the actual shared CMake verifier. Two copied MIB objects
omit encode or decode separately and demonstrate that the volatile anchor's real
link dependencies reject each omission. Installed identity negatives reject a
wrong expected fingerprint at configure and a deliberately mismatched header
identity against the actual archive at link.

The machine-readable `tools/rrc-sdk/verification-summary.json` binds installed
closure, exact archive hashes, public wrapper symbols, generation/reproduction,
consumer and seal receipts, current source hashes and regression logs.
`tools/rrc-sdk/review.md` records approval of the canonical final snapshot before
commit/push. Consistency fingerprints are not signatures or stable cross-toolchain
binary ABI guarantees. Producer binaries are bound along with their source inputs.

P7's finite semantic/reference limitations remain: 21 selected roots blocked,
SIB1 not delivered, seven of 12 channels supported, no all-field/all-value or all
2,630-declaration coverage. P7's synthetic compound fixture is not an installed
real RRC root. Existing production/runtime/APERSDK files remain unchanged except
`Makefile.am` distribution entries. There is no new 361-message APER wire campaign,
Python delivery, benchmark or merge. Next batch: RRC-P9 installed Python SDK.
