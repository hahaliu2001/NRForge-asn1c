# E1AP Python SDK 0.1.0

Distribution `nrforge-e1ap`, import `nrforge_e1ap`. All 72 messages and 40
procedures use the sealed E1-P5 C++ SDK. CPython 3.12 Linux x86-64 is the
initial qualified wheel target.

```python
import nrforge_e1ap as e1ap
print(e1ap.identity())
print(e1ap.messages())
# body follows schema(message): exact dict/list, bytes OCTET STRING,
# tagged {"type": wrapper_label, "value": value} variants.
message = "GNB-CU-UP-E1SetupRequest"
print(e1ap.schema(message))
# wire = e1ap.encode(message, body)
# decoded = e1ap.decode(wire)
```

Exports: `identity`, `messages`, `schema`, `encode`, `decode`, `CodecError`.
`CodecError.code` and `.bit_offset` retain C++ first-error information. Limits
are keyword-only: `limits` controls codec resources; `conversion_limits`
controls Python conversion depth, nodes, bytes and collection elements.
Integers retain unsigned 64-bit values and reject bool, negatives and overflow
before narrowing. Optional omissions/None stay distinct from false.

E1AP has three extensible outer CHOICE roots. Unknown outer extensions,
opaque procedures, unknown IE payloads and sequence additions retain owned
bytes on receive; this does not authorize encoding received unknown values.
The GIL is released only while owned C++ data enters encode/decode.

Build from a sealed installed C++ SDK; no runtime schema or oracle dependency:

```sh
python3 tools/e1ap-python/prepare.py --output /absolute/new/e1ap-python
CC=gcc CXX=g++ python3 -m build --no-isolation /absolute/new/e1ap-python \
  -Ccmake.define.NRFORGE_PYTHON_SDK_PREFIX=/absolute/installed/e1ap-sdk
python3 -m pip install /absolute/new/e1ap-python/dist/nrforge_e1ap-*.whl
```

Build requirements pin pybind11 3.0.1 and scikit-build-core 0.11.6. On constrained
Linux builds use serial compilation and lld 18 via
`-Cbuild.tool-args=-j1` and
`-Ccmake.define.CMAKE_MODULE_LINKER_FLAGS="-fuse-ld=lld -Wl,--threads=1"`.
Each extension statically links its own SDK with hidden symbols and archive-local
visibility. Distribution/profile mismatches and changed public headers fail
closed. Installed public inventory: 150 E1AP headers.

`python/e1ap/tests/prepare_vectors.py` projects the hash-validated frozen E1-P4
profile: 1,341 finite vectors, all 72 messages/40 procedures, and historical
outer-extension policy cases. Six independently generated E1Setup outcomes
come from E1-P5. This is Python integration coverage, not new wire qualification,
all-value coverage, live RAN or vendor interoperability.

`run_installed.py --python /absolute/venv/bin/python --output /absolute/report.json`
runs E1AP, F1AP and NGAP external tests under `python -I`; pass `--hide` for the
schema directory, installed SDK prefix and build/source staging directories.
The checkout is hidden automatically and all paths restored in `finally`.
No stable ABI, other-platform, free-threaded/subinterpreter or performance
qualification is claimed. No performance benchmark is part of E1-P6.
