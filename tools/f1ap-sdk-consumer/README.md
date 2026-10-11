# F1-P5 public installed consumer

First build/install F1AP and NGAP using their SDK build helpers. Prepare F1 Setup
fixtures from the independent frozen native oracle and accepted historical hashes:

```sh
python3 tools/f1ap-sdk-consumer/prepare.py --generated-root /tmp/new-f1ap-sdk/generated \
  --asn1-root /path/to/frozen/src --work /tmp/new-f1-native
python3 tools/f1ap-sdk-consumer/prepare_all.py --generated /tmp/new-f1ap-sdk/generated \
  --output /tmp/new-f1-slots
python3 tools/f1ap-sdk-consumer/verify.py --prefix /tmp/new-f1ap-install \
  --ngap-prefix /tmp/new-ngap-install --all-slots /tmp/new-f1-slots \
  --work /tmp/new-f1-consumer --output /tmp/f1-consumer.json --jobs 1
```

Only preparation uses schema/Python/private mapping text to identify public
wrapper types. Actual consumer compilation uses copied C++ and installed public
headers/libraries. Expected semantics and complete bytes come from pycrate,
with hashes matched to F1-P4; actual C++ encode bytes and decoded selected fields
must match. The standalone example is `f1_setup.cpp` plus `CMakeLists.txt`.

Verification copies installations, disables inherited compiler/include/CMake
search environment, audits dependency files, checks identity mismatches and
wrong-protocol library refusal. It tests 158 installed slots and NGAP/F1AP
coinstallation with both include/link orders and six populated Setup outcomes.
No new wire, mandatory-IE policy or live RAN procedure qualification is claimed.

For the stronger source-unavailable check, after both builds finish run from a
workspace outside all input directories (normal verification already copies and
audits installed dependencies):

```sh
python3 /path/to/repo/tools/f1ap-sdk-consumer/isolate.py --repo /path/to/repo \
  --f1-work /tmp/new-f1ap-sdk --ng-work /tmp/new-ngap-sdk --schema /path/to/frozen/src \
  --f1-prefix /tmp/new-f1ap-install --ng-prefix /tmp/new-ngap-install \
  --all-slots /tmp/new-f1-slots --work /tmp/new-isolated-consumers --jobs 1
```

This copies runners, examples, slot sources and installations, then temporarily
renames original source, schema, build and installation directories for the
actual configure/build/run. It restores every renamed path in `finally`,
including on consumer failure. Do not run it concurrently with those builds.

For resource-constrained full NGAP/F1AP static co-linking, install LLVM lld and
pass `--linker lld` to `verify.py` or `isolate.py`. The default is GNU gold.
This selector and stripping apply only to consumer executables; installed
archives and complete registries are retained. A Python-installed CMake wrapper
may depend on HOME; use its native CMake executable with `--cmake` when running
under the deliberately sanitized consumer environment.
