# F1-P5 installed F1AP C++ SDK contract

Package the accepted TS 38.473 V18.10.0 registry (158 messages, 94 procedures)
without changing F1-P4 BODY, PDU framing, codecs or finite qualification scope.
SDK 0.1.0 is a source integration contract, not a cross-toolchain binary ABI.

Reuse the NGAP seal, CMake source/generated consistency checks, complete-registry
link gate, archive receipt, install check and fingerprint-specific link guard.
`find_package(NRForgeF1AP 0.1.0 EXACT CONFIG REQUIRED)` exports `NRForge::f1ap`.
Headers live in `include/nrforge/f1ap`; the archive, package config, runtime identity,
registry and provenance are independent of NGAP. Installed internal includes are
protocol-qualified. NGAP legacy entry points remain available; dual-protocol
applications should use `nrforge/ngap/...` and `nrforge/f1ap/...`.

Generate from the hash-verified six external frozen modules. Before sealing,
compare every previously qualified generated production file with F1-P4;
only generated CMake packaging and location-bearing manifest may differ.
No frozen schema is vendored or edited. Configure/build/install reject changed
inputs; install requires a successfully linked registry and matching archive.
Installed consumers require neither generator, Python nor a schema checkout.

Acceptance uses a real relocated installation: public construction, complete
encode/decode and checked BODY access for all 158 slots; F1 Setup Request,
Response and Failure fixtures independently constructed from pinned pycrate
0.7.11 and matched against the accepted complete bytes and semantic hashes.
The all-slot empty-container profile remains functional evidence, not valid
application procedure messages. Test fingerprint configuration mismatch,
direct public-header link mismatch and a genuine wrong-protocol archive.
Install NGAP and F1AP into one prefix and test both include/link orders with
both complete registries and both Setup outcome families. Audit actual compiler
dependencies and exported paths for original-checkout dependencies.

Keep existing libaper, libngap and typed checks passing. Independent agent review
and necessary fixes precede commit/push on the current branch. Python SDK,
E1AP, exhaustive values, live-vendor interoperability, performance benchmarks,
merge, branch deletion and force push remain outside this milestone.
