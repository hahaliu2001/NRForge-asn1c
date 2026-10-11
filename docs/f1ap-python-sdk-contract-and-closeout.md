# F1-P6 — F1AP Python SDK contract

## Scope

Baseline: `62345628` on `feature/f1ap-cpp-aper`, accepted F1-P5 C++ SDK.
The Owner authorizes implementation, independent agent review, repair, commit
and push on this branch. F1-P6 adds a Python consumer of installed public C++
headers and libraries. Frozen ASN.1, C++ codec semantics and F1-P4 qualification
reports are unchanged. E1AP and RRC remain subsequent protocol tasks.

## API and ownership

Distribution `nrforge-f1ap`, import `nrforge_f1ap`, version 0.1.0. Exports:
`identity`, `messages`, `schema`, `encode`, `decode`, `CodecError`.
Registry closure is 158 messages and 94 procedures. Python models use final
public C++ spellings, exact built-in dict/list containers, explicit wrapper type
labels for variants, bytes for OCTET STRING, and explicit BIT STRING lengths.
Required fields, unknown keys, unsigned negatives, bool-as-integer and overflow
are checked before narrowing. None is distinct from an optional false value.
Conversion budgets are separate from C++ codec resource limits. The GIL is
released only around owned C++ encode/decode calls.

Known messages use the installed C++ dispatch. Unknown received IE/SEQUENCE
payloads remain explicit and receive-only under the existing C++ policy. Unknown
outer procedures retain opaque payloads. The F1AP fourth root remains refused;
Python adds no NGAP extension-bit assumptions or wire codec.

## Packaging and integrity

The NGAP binding generator, conversion core and native module are reused with
an explicit closed protocol profile. NGAP remains the default distribution.
CMake refuses distribution/profile mismatch. F1AP staging creates a separate
self-contained source tree and package; both wheels may be installed together.
Each extension statically links its corresponding sealed SDK and imports only
its own package's CodecError. Hidden native visibility and archive-local linking
prevent public symbol interposition between the two protocol modules.

Installed F1AP header inventory is 322 files. Installed qualified NGAP inventory
is 268 files; the original 267-file legacy NGAP layout remains recognized.
Configure/build/install checks verify public header inventory, source hashes
through the deterministic installed-include rewrite, expected fingerprint and
SDK version/link guard. Parsing refuses unknown public declaration/type shapes.
These checks establish provenance consistency, not signed authenticity.

Pinned build dependencies: pybind11 3.0.1, scikit-build-core 0.11.6.
Initial wheel target is CPython 3.12 Linux x86-64. Runtime needs no compiler,
schema, oracle or C++ SDK prefix. No stable ABI, alternate platform,
free-threaded/subinterpreter support or performance qualification is claimed.

## Acceptance — Complete / Accepted (2026-10-11)

Actual wheel build, installation and external isolated consumers pass. F1AP:
9 test methods cover all 4,728 inherited finite vectors, all 158 registered
messages, 94 procedures, three independently sourced populated F1 Setup
outcomes and explicit construction, unknown receive retention, optional fields,
fourth-root refusal, opaque procedures, conversion/error/resource boundaries,
concurrent calls and both same-process protocol import orders. NGAP: all 12
original installed-consumer tests pass, including 131 native vectors and three
Setup outcomes. Two protocol seal negative methods and two original NGAP seal
methods pass. Wrong distribution/profile selection is refused before compiling.

Source-distribution inventory is checked against its complete staging receipt;
distributed generator tests resolve their local shared source. No second full
clean compile of an extracted source distribution was run. ELF inspection finds
no SDK shared dependency or RPATH. Initial default-linker attempts exceeded the
8 GiB environment memory limit; all units had compiled, and serial lld 18 using
CMAKE_MODULE_LINKER_FLAGS completed both modules without source/codec changes.

Evidence: tools/f1ap-python/verification-summary.json and review.md. Independent
agent /root/f1p6_review accepts the implementation and actual artifacts, verifies
their source/file fingerprints and independently reruns all nine F1AP installed
consumer methods from /tmp under isolated Python: 9/9 PASS. Historical finite bytes are reused for Python integration; this
neither reruns nor broadens F1-P4 wire qualification. No benchmark or full native
extension sanitizer qualification was run.
