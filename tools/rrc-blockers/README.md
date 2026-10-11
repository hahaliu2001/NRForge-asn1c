# RRC-P10 remaining blocker study

Read `../../docs/rrc-remaining-blockers-and-batch-plan.md` for results and repair
boundaries. `study.json` records observed first failures and targeted inspection;
static source shapes are conservative leads, never predicted unlock counts.

Run from a configured/built checkout with the existing parser/fixer/typed
archives and developer_tree.o present, using GCC 13.3.0 and the same authenticated
producer inputs recorded in P9:

```sh
PYTHONDONTWRITEBYTECODE=1 python3 tools/rrc-blockers/study.py \
  --asn1-root /absolute/frozen/src --work /absolute/new/p10-work \
  --output tools/rrc-blockers/study.json --check
```

The work directory must be new. Omit `--check` only to intentionally record a
new study; it does not refresh SDK/qualification reports. Raw JSON and stderr,
four fresh probe executables, generated headers and root lists remain in work.
The source authority is the six frozen modules in the original readiness
manifest. Source/archive drift fails before probes and is checked again after
probing. The matrix uses the existing P6 root order and authenticates all 64
accepted header hashes and all 21 first diagnostics.

Existing P1 AST, P4 owned graph and P6 render probes are reused. `inspect.c`
adds read-only combined/declaration constraint and generic/specialization tag
inspection in a private fixed tree. No extractor/renderer implementation is
modified. Empty actual `ref_expr` caches are recorded; lexical module/import
resolution locates their source identities without pretending that a cached
pointer exists. The full raw fixed JSON hash binds all 264 SetupRelease clones;
the report retains aggregate tag classes and the ten relevant instances.

`verification.json` binds report/tool/document hashes and preserved baseline
blobs. `review.md` binds canonical sorted compact JSON containing
`source_sha256` and `verification_sha256`, excluding itself. These P10 records do
not override accepted P7 wire qualification or P8/P9 SDK delivery evidence.
