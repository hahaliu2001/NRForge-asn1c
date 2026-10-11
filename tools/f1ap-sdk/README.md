# Build the installed F1AP C++ SDK

Use the external `src` schema root pinned in
`tools/f1ap-readiness/source-manifest.json`. Build the owned generator using the
NGAP SDK README dependency order, then run from the repository:

```sh
python3 tools/f1ap-sdk/build.py --repo "$PWD" --asn1-root /path/to/frozen/src \
  --work /tmp/new-f1ap-sdk --prefix /tmp/new-f1ap-install --jobs 1
```

Work must be new; prefix new or empty. Requires Python 3.9+, CMake 3.20+, a C++20
compiler and GNU gold. Validation uses strict O0 compilation and GNU/Linux;
optimized warning-clean builds and cross-toolchain binary ABI are not qualified.
Build/install is followed by the separate installed consumer checks below.

Applications use:

```cmake
find_package(NRForgeF1AP 0.1.0 EXACT CONFIG REQUIRED)
target_link_libraries(app PRIVATE NRForge::f1ap)
```

Include `<nrforge/f1ap/f1ap.hpp>` and the selected
`<nrforge/f1ap/messages/F1SetupRequest.hpp>` public header. BODY codecs and
mapping headers are private. Runtime `nrforge::f1ap::sdk_identity()` agrees with
`header_sdk_identity`; `NRFORGE_F1AP_EXPECTED_FINGERPRINT` optionally pins the
CMake package identity. Fingerprints check consistency, not authenticity or ABI.

The build verifies previously accepted production output hashes and preserves
F1-P4 qualification boundaries. It does not repeat its 4,728-case campaign.
See `docs/f1ap-cpp-sdk-contract.md` and the consumer README.

Run `verify_seal.py --repo "$PWD" --generated /tmp/new-f1ap-sdk/generated
--build /tmp/new-f1ap-sdk/build --work /tmp/new-f1ap-seal-check` to exercise the
actual shared verifier against copied missing/extra/altered files, production
source changes, missing archive receipt and altered archive. The focused
`.github/workflows/f1ap-sdk.yml` repeats build/install/consumer checks and requires
`NRFORGE_SCHEMA_READ_TOKEN` with read-only private NRForge-RAN access. A supplied
workflow is not evidence of a successful remote run.

After the stronger isolation check and seal negative check, reconcile the actual
archive members, registration symbols, archive receipt and installed file hashes:

```sh
python3 tools/f1ap-sdk/record.py --repo "$PWD" --work /tmp/new-f1ap-sdk \
  --prefix /tmp/new-f1ap-install --isolation /tmp/new-isolated-consumers \
  --seal-negatives /tmp/new-f1ap-seal-check \
  --functional-log /tmp/libaper-check.log --functional-log /tmp/libngap-check.log \
  --functional-log /tmp/libasn1typed-check.log --output /tmp/f1-sdk-evidence.json
```

This requires successful actual consumer summaries. Its PASS does not promote a
new wire profile or expand the historical qualification boundary.
