# RRC-P11: closed singleton INTEGER

Baseline: `99784032a83ab55c3009800bdb31606eb6031000` on
`feature/f1ap-cpp-aper`. This batch implements the P11 scope in
`docs/rrc-remaining-blockers-and-batch-plan.md`.

After configuring/building the native libraries and
`libasn1typed/check_asn1typed_extract`, run from the repository root:

```sh
python tools/rrc-singleton/check.py --asn1-root /path/to/frozen/NRForge-RAN \
  --work /path/to/new/p11-check --output /path/to/readiness.json
```

The work directory must not exist. The driver captures current producer
sources and archives before rebuilding its probes; it authenticates the six
frozen RRC modules, checks inputs again after execution, generates eleven
synthetic roots and rejects four unsupported roots, compiles/runs generated
code with strict C++20 flags, rescans all 85 selected RRC identities, compares
all 64 previously accepted headers, and syntax-compiles the newly supported
`SystemInformation` header. It also rebuilds the APER coverage probe and runs
the existing historical comparison helper for all 361 APER messages.

`readiness.json` records the current P11 invocation, including the historical
APER helper's original batch label. `verification.json` binds the source
snapshot and native regression logs. `review.md` records independent review.
There is no benchmark or SDK/package rebuild in this batch.

The new matrix is **65 render PASS / 15 extraction blocked / 5 render blocked**.
The delivered P8/P9 SDK remains **64 roots**. New generated availability alone
does not qualify `SystemInformation` wire behavior or expand SDK delivery.
