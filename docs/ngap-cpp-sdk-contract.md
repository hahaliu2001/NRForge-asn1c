# NGAP C++ SDK packaging and independent consumer contract

Owner accepted autonomous implementation, independent review/fix, verification,
commit and push on 2026-10-10 after unified dispatch commit
`3f8a915768b28e0129736618e2f4c28c3dcbeb25`.

The SDK contains the complete frozen TS38.413 V18.10.0 registry: 131 message
identities and 81 procedures. This milestone packages the existing wire behavior;
it changes neither schema nor BODY/runtime contracts. Version 0.1.0 describes an
initial source SDK, not a stable cross-toolchain binary ABI.

## Reproducible build and installation

A supplied six-module schema must match the accepted source manifest by SHA-256
and Git blob identity. Generation uses the existing complete owned-evidence
controller, then seals the generated files and production inputs before CMake
configuration. CMake checks the seal both at configuration and before compilation.
Source revision, exact content digests, schema identity and SDK version remain
inspectable. A source revision alone never stands in for dirty source contents.

The generated CMake project builds all production adapters, registry, PDU core and
APER runtime into one static library. Install exports `NRForge::ngap` through
`find_package(NRForgeNGAP CONFIG REQUIRED)`. Installed public headers contain
types and the lightweight PDU API; mapping/codec implementation headers remain
private. Exported target paths must be relocatable and must not require the source
checkout, schema checkout, Python, parser or generator at consumer build time.

An SDK fingerprint identifies exact production inputs and generated contents.
The public header/library agreement must be enforceable at link time, with a
runtime identity query for diagnosis. A deliberately mixed installed header and
library must fail rather than silently accepting an incompatible package.

## Consumer acceptance

Build and install the complete 131-message SDK using an actual CMake executable.
Copy/move the installation to another prefix, then configure, compile and run an
outside-repository consumer using only that prefix. Verify compilation commands
and exported configuration do not reference the original repository/generated
tree. Consumer code uses public `Body`, construction, encode, decode and checked
BODY access for NG Setup Request, Response and Failure.

For each outcome construct mandatory IE values independently, compare complete
wire length/bytes against the pinned native reference and historical accepted
profile, and check all selected decoded fields. Do not reduce acceptance to a
roundtrip. Verify SDK identity and a header/library mismatch negative. Preserve
existing typed/runtime test gates and the historical qualification profiles.

Add reproducible local tooling, a minimal consumer example and focused CI. Record
actual commands, artifact hashes, review result and limitations. A local CI-equivalent
run is evidence; a remote workflow must not be reported as passed without its run.

## Stop boundary

Independent agent review and fixes finish before commit/push. RAN CU-CP application
integration, Python bindings, schema changes, merge, branch deletion, force push,
benchmark, exhaustive value qualification and live vendor interoperability are
outside this milestone. Do not access `/mnt/wslg`.
