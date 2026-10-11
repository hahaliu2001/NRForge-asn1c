# RRC-P4 independent review receipt

**Verdict: PASS — no blocking findings.**

Reviewed baseline: `97d099f4edfb2d463bd564dc5cec8e586a81c8e4`.
Branch: `feature/f1ap-cpp-aper`; independent reviewer `/root/e1_sdk_review`.
Reviewed snapshot: 18 files, canonical SHA-256
`1ce969f367f09dc923c2c70d15bca0bc572323d904e5af0b9123f9a8397323d8`.
Canonical format: sorted path/hash JSON with compact separators. This receipt
is excluded from its own snapshot. Exact reviewed files are listed below.

## Review and independently confirmed evidence

* Initial blocker: ordinary finalize ERROR retained old validity/index flags.
  Fixed before the final snapshot with safe bounded invalidation and negative
  kind/storage tests. Final review found no remaining blocker.
* Independently strict rebuilt final focused test: Root 83 and NestedPick 79
  exhaustive allocation-failure positions PASS; group boundary/provenance,
  nested groups, bracketed/future CHOICE indexes, default presence and legacy
  APER renderer rejection checked.
* Independently strict rebuilt root probe and recorder --check: 64/80 PASS,
  16 FAIL; all 85 requests reproduce exact report bytes and raw graph hash
  `f7c5b0220f688a7cafd12983caf463da8d154340cb6c15bc958e51adfdaf8b0a`.
  634 explicit SEQUENCE wrappers and six CHOICE bracket lists reconcile the
  inherited 640 lexical pairs. Unique inventory matches across root closures.
* Independently rehashed actual final regression files, without rerunning the
  fixer: F1AP 158 messages / 948 headers, NGAP 131 / 393, E1AP 72 / 432.
  All 361 probe message results and 1773 historical comparisons PASS. Receipt
  guarded input hashes independently match current final production sources.
* Latest ASan/UBSan focused and 85-root binaries reexecuted with
  `ASAN_OPTIONS=detect_leaks=0`, `UBSAN_OPTIONS=halt_on_error=1`: PASS.
  Instrumentation boundaries are accurately scoped in the verification file;
  no completed leak scan is claimed.
* Complete suite logs: typed 45/45, parser 1/1, APER 10/10, PDU 3/3; no
  FAIL/ERROR. Additional final focused cases passed after full suite execution.
* All 18 file SHA-256 values and canonical digest independently verified.
  git diff --check PASS; frozen source/fixtures unchanged.

## Scope retained

Sixteen constraint/alias closures still fail. Ten semantic SetupRelease
closures lack root tag wire mappings. Unknown-extension skip/opaque policies
are owned contracts only; runtime, RRC C++ generation, UPER wire, installed SDK,
benchmark and merge remain NOT_RUN. NGAP historical header hashes cover BODY
only. No partial graph filtering or full-protocol wire claim.

Reviewer harness note: an early candidate check used an older review probe
without source_group_count and raised KeyError. Rebuilding the final probe
resolved this harness mismatch; final checks all PASS.

## Final reviewed files

| Path | SHA-256 |
|---|---|
| `docs/rrc-cpp-uper-p4-extensions-closeout.md` | `72c795138dcc1ac3f35cbb4edc0b7eb47d91ac2106c755a8c32d6bb1f54949f0` |
| `libasn1parser/asn1p_expr.c` | `8c532d9d5f5adc841e3b4a2b2ca536614e394d824007dad32cd11907b9c7f083` |
| `libasn1parser/asn1p_expr.h` | `f82a1ae2cdeb0f9e0b33cad4d161d8656ab565f7a8f23ea075bc9964d6049c7e` |
| `libasn1parser/asn1p_y.y` | `8d9af145b9bd679ef938fc47b48fc8c9e071174589944c5ef1c0c03a7e5079ed` |
| `libasn1typed/Makefile.am` | `64eaacb5b50ed54495a73bba2f971bde4ec315a71bd16f7c24f2c3871ccef843` |
| `libasn1typed/asn1typed.c` | `f1748c06645d5981203e8241ba8c76e6503df6f6e28cc5539340fe66bf71aaf1` |
| `libasn1typed/asn1typed.h` | `1565690a926f1c9cb0f1d188046b264f2c680cbf7c74936c788a4b7bf75f0268` |
| `libasn1typed/asn1typed_extract.c` | `61b17820d5d6a6ec54f60410aea14eb736d97ba34d6593c851b7cc15e8aa1bf7` |
| `libasn1typed/check_asn1typed_extensions.c` | `661cab29aadbd4f73472f013c26105ad480e92d9614473220d4e735cab02f17a` |
| `libasn1typed/check_asn1typed_root_graph.c` | `2f27e089acffcb7a8b2df7cad03a57f88566fcacaa7ab658584e0f6aae210c41` |
| `libasn1typed/fixtures/ordinary-extensions.asn1` | `97c2b4e890073540d693b424743ecc310cd0ca2c6ebdf175d491515262c59973` |
| `tools/rrc-readiness/README.md` | `27749bed46645fc95d5e27ecdb507ff196b735b60ed1428bb43f43e011ae4459` |
| `tools/rrc-readiness/aper-regression-rrc-p4.json` | `4bd494a339f30816b60248eed308a27bbd364ca22c7dbb47bbc65033cc164d37` |
| `tools/rrc-readiness/check_aper_regression_p4.py` | `a3d38c527ad837681d6de71ecb0ab593bddc33dd171e44caad6b21534ed15350` |
| `tools/rrc-readiness/readiness-rrc-p4.json` | `963ef3a1e9826d55b9aa846751a975e9411e69f9cc00a5ec21b8ffe9e55207cb` |
| `tools/rrc-readiness/record_p4.py` | `1302923171aa86f55b7607fcd777d115d2383dadd31dbf05b21b7e8733fd194b` |
| `tools/rrc-readiness/root_probe_p4.c` | `6bed58585ed9ca7e5362f92a64337c2716b53e778ada2a284a43169c6d3ae045` |
| `tools/rrc-readiness/verification-rrc-p4.json` | `598f40ed52ecac12cbafccc8a0477dc4f432609458f3954bb9469523fcdf823c` |
