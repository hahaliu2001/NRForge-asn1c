# NGAP Python SDK integration contract and closeout

## Scope and authority

The Owner requested Python SDK integration under the existing autonomous implementation, independent review, repair and publication authority. This milestone adds a Python consumer of the installed C++ SDK. It does not change the ASN.1 schema, C++ runtime, message codec generator or previously qualified wire behaviour.

Baseline: `6f2b1598fe9c9db41062edd24c3db18c0b0447a8` on `feature/ngap-cpp-aper-first-message`. The installed SDK identity is `cc912cb618961dad464e4d826b223ec324e541d395e4dec20db91a8e1facb7a4`; frozen schema SHA-256 is `2d03fffa622648d51c240d9f690b078ab6723b4d338b342264893dc2958f9339` (TS 38.413 V18.10.0). Provenance consistency checks are not a signed authenticity claim.

## Accepted API

Package `nrforge_ngap` exposes `identity`, `messages`, `schema`, `encode` and `decode`. Encoding accepts a registered ASN.1 message name and an owned typed body model. Decoding returns the received outer identity and either a typed body or retained opaque payload. `CodecError` carries the unchanged C++ error code and bit offset.

The data model uses final generated C++ spellings. Structs are exact built-in dictionaries; collections are exact built-in lists. Mandatory members are required and unknown keys are rejected. OPTIONAL values distinguish None from false. Integer conversion rejects bool, negatives for unsigned storage and overflow before narrowing. Variant selection uses an explicit wrapper type label; storage ordinal is never presumed to be PER index. Bytes, BIT STRING, enum extension indexes and sequence extension payloads remain explicit.

Unknown IE and SEQUENCE extension data are preserved on receive. Their re-encoding remains rejected where the inherited C++ contract is receive-only. Unknown enum extensions can roundtrip where the C++ codec supports it. Unknown outer PDUs are not accepted by typed-message encode. No Python wire codec or silent dropping of unknown information is introduced.

Each call has separate conversion budgets and C++ codec limits. Conversion budgets cover traversal, collection sizes and copied value/label/input/output bytes, not pre-existing caller memory, allocator overhead or schema reflection. Labels have an additional bounded length. Python conversion runs with the GIL; only pure owned C++ codec execution releases it.

## Build and installation boundary

The build reads all 267 installed public headers and the installed provenance receipt, verifies their inventory and expected hashes, and refuses unsupported public type shapes. It generates converters for all 131 messages (8,741 structs, 866 enums, 1,571 aliases and 587 constraints). Checks run at configure, build and installation. The C++ SDK is statically linked into a CPython extension; runtime imports require neither schema files, pycrate, compiler nor the C++ SDK prefix.

Build dependencies are pinned to pybind11 3.0.1 and scikit-build-core 0.11.6. Strict C++20 O0 compilation matches the initial installed SDK qualification. The first binary artifact targets CPython 3.12 on Linux x86-64. Free-threaded Python, subinterpreters, stable ABI, other platforms and optimized performance are outside this milestone.

## Evidence

Final installed-wheel results and artifact digest are recorded in `python/verification-summary.json`. Build-time header tamper/inventory/parser rejection tests, exact input shape tests and independently reviewed conversion-core sanitizer probes accompany the installed consumer tests. The consumer suite runs from an external directory with isolated Python and checks package/extension locations.

The suite uses 131 accepted minimal native vectors and three independently produced NG Setup outcome vectors, including decoded semantic fields and an explicit construction example. Additional tests cover unknown data preservation and receive-only rejection, exact error codes/offsets, conversion budgets and input/output/wire codec limits. Independent review distinguishes these finite integration checks from full value-domain qualification, live RAN procedure interoperability and external codec differential qualification.

The original 131-message PCAP and C++ qualification evidence are not changed by this milestone. Some minimal PCAP payloads contain intentionally empty nested containers; Python golden vectors do not convert that coverage into a claim of complete nested protocol validity.

Final gates passed: 12 installed consumer tests, 2 build-time rejection test methods, hidden-input rerun, native import/registry gate, distribution manifest, and independent installed-wheel review. Independent review additionally compared overflow offsets with the unchanged C++ baseline and checked 256 mixed concurrent contexts. Conversion-core ASan/UBSan passed; full native-extension sanitizer and LSan qualification were not run. Remote CI results are not claimed.
