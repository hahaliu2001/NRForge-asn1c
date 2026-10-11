# Build an installable NGAP C++ SDK

The source SDK requires Python 3.9+, CMake 3.20+, a C++20 compiler, and a configured
owned generator. Use the six exact modules listed in
`tools/n11-envelope-qualification/source-manifest.json`; they are pinned to
NRForge-RAN revision `d6e514a33ec3695925c24aa292900514b371b074` (TS38.413 V18.10.0).

From the repository:

```sh
autoreconf -iv
./configure
for library in libasn1common libasn1parser libasn1fix libasn1print libasn1typed; do
  make -C "$library" -j2
done
make -C tools asn1typed_ngap_dispatch
python3 tools/ngap-sdk/build.py --repo "$PWD" --asn1-root /path/to/frozen-schema \
  --work /tmp/new-sdk-build --prefix /tmp/new-sdk-install --jobs 1
```

Work must be a new directory; the prefix must be new or empty. The script verifies
schema SHA-256/Git identities, generates all 131 adapters, seals exact content,
and invokes real CMake configure/build/install. It records separate logs and
`build-summary.json`. Building is not consumer qualification by itself.
Large generated message graphs consume substantial compiler memory; start with
one compilation job and increase concurrency only with sufficient memory.
The focused build script selects strict, unoptimized C++20 (`-O0`). An attempted
GCC 13 `-O3 -Werror` build reported `maybe-uninitialized` diagnostics in nested
`std::variant`/`std::optional` paths. Optimized warning-clean builds are not an
acceptance claim of this milestone; no warning suppression is added.

Run the installed consumer check described in `tools/ngap-sdk-consumer/README.md`.
Applications use `find_package(NRForgeNGAP 0.1.0 EXACT CONFIG REQUIRED)` and
`target_link_libraries(app PRIVATE NRForge::ngap)`. Only the installed prefix is
needed for application compilation. Each message has its own public header and
`Body` alias. The library contains all registry/adapters/runtime definitions.

Keep compiler/standard library ABI consistent between SDK and application. SDK
0.1.0 is a source integration contract, not a promise of binary ABI compatibility
across compilers. A fingerprint link guard catches different SDK content; it is
not an authentication signature or an ABI verifier.

The focused workflow `.github/workflows/ngap-sdk.yml` builds and verifies a clean
SDK installation plus the three independently constructed NG Setup outcomes.
The frozen schema repository is private: configure the Actions secret
`NRFORGE_SCHEMA_READ_TOKEN` with `contents:read` access to
`hahaliu2001/NRForge-RAN`. Fork PRs without this secret cannot execute that gate.
This is a CI credential prerequisite; local builds accept the already available
hash-verified frozen schema directory and do not need GitHub credentials.
Wire qualification remains the finite accepted profile, not live vendor or RAN
procedure validation.

F1-P5 adds protocol-qualified installed internal includes and the public
`<nrforge/ngap/...>` entry points alongside the legacy direct includes. For a
consumer linking both `NRForge::ngap` and `NRForge::f1ap`, use qualified includes
to avoid ambiguous same-named top-level headers. Package names, protocol
namespaces, registries, archives, configuration fingerprints and install paths
remain independent. The shared packaging helper is parameterized; production
NGAP BODY/codec/PDU output remains unchanged.
