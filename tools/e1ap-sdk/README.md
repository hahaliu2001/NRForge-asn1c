# E1AP C++ source SDK 0.1.0

This source delivery uses the frozen external six-module source authority in
`tools/e1ap-readiness/source-manifest.json`; no schema is vendored. Build the
compiler first using `tools/e1ap-readiness/README.md` (dependency order matters),
then build `make -C tools asn1typed_ngap_dispatch`.

```sh
python3 tools/e1ap-sdk/build.py --repo "$PWD" --asn1-root /path/NRForge-RAN/src \
  --work /new/e1-sdk-work --prefix /new/e1-install --jobs 1 --cmake cmake
python3 tools/e1ap-sdk-consumer/prepare.py --generated-root /new/e1-sdk-work/generated \
  --asn1-root /path/NRForge-RAN/src --work /new/e1-native
python3 tools/e1ap-sdk-consumer/prepare_all.py --generated /new/e1-sdk-work/generated \
  --output /new/e1-all
python3 tools/e1ap-sdk-consumer/isolate.py --repo "$PWD" \
  --sdk-work /new/e1-sdk-work --schema /path/NRForge-RAN/src \
  --prefix /new/e1-install --all-slots /new/e1-all --work /new/e1-isolated
python3 tools/e1ap-sdk/verify_seal.py --repo "$PWD" --generated /new/e1-sdk-work/generated \
  --build /new/e1-sdk-work/build --work /new/e1-seal-negatives
```

Use pycrate 0.7.11 for native fixture preparation only. Each work/prefix must
be new or empty as specified by the script. The ordinary installed consumer
needs no Python, generator or frozen sources. Select a C++20 compiler and
supported gold linker. These scripts use strict O0 functional compilation,
not a performance benchmark. The source and generated content hashes identify
exact build inputs independently of checkout/work paths; the baseline revision
in provenance does not claim it identifies uncommitted content by itself.

Packaging installs public types/messages/PDU/runtime headers, `sdk_version.hpp`,
`libnrforge_e1ap.a`, relocatable CMake exports and source provenance. Private
codec/mapping headers are excluded. This is a source integration contract;
it does not promise binary ABI portability or all-value wire qualification.

To check reproducibility without rebuilding the archive, generate into a second
empty directory with `tools/asn1typed_ngap_dispatch ... --e1ap`, seal it using
`tools/ngap-dispatch/seal_sdk.py --profile e1ap`, then run
`check_reproducibility.py --generated FIRST --repeat SECOND --output receipt.json`.
This compares logical manifests, exact public/production outputs and SDK identity.
`check_regression.py` compares all NGAP/F1AP generator files against a controller
compiled from the E1-P5 baseline with the current unchanged typed libraries.
The focused `.github/workflows/e1ap-sdk.yml` contains the full baseline-controller
build, generation, isolation, negative and evidence commands; it needs the
read-only private schema secret `NRFORGE_SCHEMA_READ_TOKEN`. No remote CI PASS
is implied by its presence.

`record.py` reconciles the actual archive/ELF/registration closure, install
manifest and hashes, isolation receipt, seven seal cases, functional logs,
reproducibility receipt and full NGAP/F1AP generation receipt. Pass
`--reproducibility` and `--regression` with the matching JSON receipts. The
committed `verification-summary.json` records the accepted local campaign.
