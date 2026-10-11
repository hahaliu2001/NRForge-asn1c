# F1AP Python SDK 0.1.0

`nrforge_f1ap` exports `identity`, `messages`, `schema`, `encode`, `decode` and
`CodecError`. It wraps the sealed F1-P5 C++ SDK; no Python wire implementation is
introduced. Exact built-in dict/list models, explicit CHOICE wrapper labels,
bytes, integer range checks, conversion budgets and C++ resource limits follow
the NGAP Python contract. Unknown receive data is retained. The fourth F1AP root
continues to be refused by the inherited C++ policy.

Stage the self-contained distribution, then build against an installed SDK:

```sh
python3 tools/f1ap-python/prepare.py --output /absolute/new/f1ap-python
python3 -m build --no-isolation /absolute/new/f1ap-python \
  -Ccmake.define.NRFORGE_PYTHON_SDK_PREFIX=/absolute/installed/f1ap-sdk
```

Pinned build dependencies: pybind11 3.0.1 and scikit-build-core 0.11.6.
The wheel statically includes C++ codec code and needs no SDK/schema/compiler
at runtime. Initial qualification targets CPython 3.12 Linux x86-64. No stable
binary ABI, free-threaded/subinterpreter, performance or full value-domain claim.
