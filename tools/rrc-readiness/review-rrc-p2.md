# RRC-P2 independent review receipt

Result: **PASS**, 2026-10-11. Reviewer: independent agent
`/root/e1_sdk_review`. Baseline:
`21eac8aaba515dce8e03aecb78de3799035dff3c`.
No remaining blocking findings. The reviewer did not modify files, commit,
push, merge or run a benchmark.

The reviewer found and reproduced an alias stale-cache acceptance bug during
review. It verified the final fix compares explicit ordinary namespace targets
with cached targets for every reference, and independently ran alias-token and
alias-cache negative cases. Cross-module constructed-alias field provenance
was also corrected and tested. Both findings are closed.

Independent acceptance on the final sources:

* Strict GCC focused test/root probe builds and execution passed; all 140
  allocation-failure positions rolled back with clearable output.
* `record_p2 --check` reproduced 44 PASS / 36 FAIL across 80 selected roots,
  including owned closure serialized after fixed-tree destruction.
* Freshly linked P1 probe/scanner `--check` preserved the historical report.
* ASan/UBSan focused test and all 80 roots passed with `detect_leaks=0`.
  This does not claim a passing LeakSanitizer scan or instrumented parser/fixer.
* The reviewer independently checked every generated historical APER header
  against its accepted hash: F1AP 158 messages / 948 headers, NGAP 131 / 393,
  E1AP 72 / 432; total 361 messages / 1773 headers. The final artifact receipt
  input hashes match the final source snapshot. NGAP historical hashes cover
  BODY only; envelopes were generated without a historical byte comparator.
* Final typed 43/43, APER 10/10 and protocol PDU 3/3 logs had no FAIL/ERROR;
  `git diff --check` passed.

The MTC full closure is still blocked by MeasTiming known additions. The
closeout explicitly corrects the proposed P1 expectation without filtering
fields, changing schemas or publishing a partial graph. Semantic extraction
success does not qualify C++ generation, UPER wire or installed SDK delivery;
those operations remain NOT_RUN.

Thirteen-file snapshot, excluding this receipt: SHA-256
`05950251223c7cedc91093a4b74e518c47669124953ab05204a3a7b6e16520cd`.
Compute each file SHA-256, build `{path: hash}`, serialize with Python
`json.dumps(sort_keys=True, separators=(',', ':'))`, then SHA-256 the UTF-8 bytes.
The reviewer independently verified all 13 current file hashes.

| Reviewed path | SHA-256 |
|---|---|
| docs/rrc-cpp-uper-p2-root-graph-contract-and-closeout.md | `98f2967d646212702a6f4cfb81aae8d0b3be5ad94c2f61ad5e898ecfce8b9ae2` |
| libasn1typed/Makefile.am | `72ee9b4d5d8571c53a0dc90f8eac2406b0719e0e775a12d9f155fb59e0593f06` |
| libasn1typed/asn1typed_extract.c | `a3ac9a1aaa360d524035a960643eb2dad8fc5efd005882315a8722051e59edbb` |
| libasn1typed/asn1typed_extract.h | `d6fcf7e031328853fc579cd58565a5410414cc594325259195fe0e40fea8ce23` |
| libasn1typed/check_asn1typed_root_graph.c | `7e98d4aeb4012af7c75e56bdfda861a1324fa0ada881a157a48c2168dd5a8820` |
| libasn1typed/fixtures/ordinary-root-graph.asn1 | `207e9995c6f3f59508168dfcdae505d4bdaafb15e6827f6228018ff6ee037d0e` |
| tools/rrc-readiness/README.md | `0a87d21c14fa6b4529085909754d372d8c7955cc69e8c58ea49c94d8451ae298` |
| tools/rrc-readiness/aper-regression-rrc-p2.json | `34291c1ddb44e5917eff38713ea4032fadc912d6c54e896ea16e3f2f0369e49d` |
| tools/rrc-readiness/check_aper_regression.py | `0aa9f128559131d2e8e146921867ecb1e5d8f7047e6b7eb58400543ba345a9aa` |
| tools/rrc-readiness/readiness-rrc-p2.json | `8d67018a602ea8e68f230af71aecf54a4255f83d269e85156293e989b2832f2c` |
| tools/rrc-readiness/record_p2.py | `5c68598c510f4f7b4636971d90cc47d40849b22ce386732d76fdd2e2c06ab76a` |
| tools/rrc-readiness/root_probe.c | `f4728f9cdcd703b2f7f54f4dd354bb0b37d357abd3d397e081b0b30cfe0cb444` |
| tools/rrc-readiness/verification-rrc-p2.json | `ec941f3dadeef369684fb58ce1f9461f4f5eb3a0ac8fbf6753ef7f4e84294076` |
