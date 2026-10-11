# RRC-P3 independent review receipt

Result: **PASS**, 2026-10-11. Reviewer: independent agent
`/root/e1_sdk_review`. Baseline:
`e7b5b95ee70200cf110923c9262cd27cc1c01d08`.
No remaining blocking findings. The reviewer did not modify files, commit,
push, merge or run a benchmark.

Independent final acceptance:

* Strict GNU C11 focused test recompilation/execution passed. All 366 parameter
  graph and 62 DEFAULT graph allocation positions rolled back correctly.
  Source-identity, stale argument-cache, wrong specialization-cache, governed/
  wrong-arity formal, alias/forwarding and actual-budget negative cases passed.
* Fresh root probe and `record_p3 --check` reproduced 45/80 PASS and 35 FAIL,
  plus five DEFAULT seed roots with 13 unique default fields. Owned actual and
  semantic reference closure survives fixed-tree destruction.
* `audit_p3_inputs --check` independently reproduced 338 named source actual
  sites, 264 distinct namespace keys and 20 DEFAULT field types (15 ENUMERATED,
  five INTEGER). These are source shape/identity counts, not full instance or
  all-default-value qualification.
* The final APER probe JSON and every historical generated header were checked
  independently: F1AP 158 messages / 948 headers, NGAP 131 / 393, E1AP 72 / 432;
  361 messages / 1773 historical header hashes all match. Receipt input hashes
  correspond to the final sources. NGAP historical comparison covers BODY only.
* Typed 44/44, APER runtime 10/10 and PDU 3/3 suite logs show no FAIL/ERROR.
  ASan/UBSan focused test and 85 roots were independently rerun successfully with
  `detect_leaks=0`. No completed leak scan or fully instrumented parser/fixer is
  claimed. One review-harness command omitted that environment option and hit
  the known LeakSanitizer /proc environment fatal; correctly propagating the
  option reproduced the accepted address/undefined-behavior checks.
* `git diff --check` passed. The earlier APER run rejected by the input hash
  guard after a header comment change is excluded from accepted evidence.

The scalar DEFAULT helper's present-value domain validation is explicitly a
caller precondition. Remaining parameter shapes, 35 semantic first gaps, MTC
extension blocker and NOT_RUN C++ generation/UPER wire/installed SDK boundaries
are clearly stated. No schema patch, field filtering or partial success graph
is accepted. Existing profile-zero behavior and APER artifacts are preserved.

Eighteen-file snapshot, excluding this receipt: SHA-256
`b69dd07714829c7c83c76ecc36fbde59ea29f35590d131cd4bbe19f6cbfce55f`.
Compute each file SHA-256, build `{path: hash}`, serialize with Python
`json.dumps(sort_keys=True, separators=(',', ':'))`, then SHA-256 the UTF-8 bytes.
The reviewer independently verified all 18 current file hashes.

| Reviewed path | SHA-256 |
|---|---|
| docs/rrc-cpp-uper-p3-parameters-default-closeout.md | `8c5cd401eb18bfadc00e010066561078792819477ff84144f3867f7e17b8c8ad` |
| libasn1typed/Makefile.am | `92d8e401a46adc2431f93cff4208ff5e77caae58cc2e06a1f73d2517d74e7768` |
| libasn1typed/asn1typed.c | `cb0a73b89337dce442b9bfb51a86b2b8cc715e4943e35dc1cd12ddfd3f2c83d3` |
| libasn1typed/asn1typed.h | `638d7edbb6c2c752714a3a111423114ed504c389577c8be65c12e1d3e265e941` |
| libasn1typed/asn1typed_extract.c | `1d32075240e7b270973848fc829d4f04cf2fdc879aef58f353c8431226e9c801` |
| libasn1typed/asn1typed_extract.h | `190670b8691dba5774a3b07048184bfc8d4ced10385da8af871a9fec0ce0254c` |
| libasn1typed/check_asn1typed_parameters_default.c | `073a7497c8f4db7cd75e6dfa88b854139b080396ee6d62efd1b91aaa5ebeabe1` |
| libasn1typed/check_asn1typed_root_graph.c | `707a54c7ebe4a5c22d8ebf9751395517dc17c8ae46c64e3c391ceb4aefb41d7c` |
| libasn1typed/fixtures/ordinary-parameters-default.asn1 | `e43fb91de6f81c23f4e7d3ca9297b7866135c4dfc750bdfe97a4ed9a705caea5` |
| tools/rrc-readiness/README.md | `26ef7c559e1937c7434cf48ef1a9ab35e4544816a9aab63ac4a5efba4ba5c79e` |
| tools/rrc-readiness/aper-regression-rrc-p3.json | `04fa228bf7a0caa54716e3d6a3b238cdb1febe8679dbd36ebab186a1513919b1` |
| tools/rrc-readiness/audit_p3_inputs.py | `68d83b14c6808ff541be0838895e96a7af99bddfe51fd80fb1d441c145d7c85e` |
| tools/rrc-readiness/check_aper_regression_p3.py | `1c564bd85abfabdc7b459e9c9299e7d0fa4974815ef626e3aae6c34d3290dc80` |
| tools/rrc-readiness/input-shapes-rrc-p3.json | `f57e9bf8a81ca3589bb1e87574520abbae0f5755016a30cd05f6b82b42f8b49e` |
| tools/rrc-readiness/readiness-rrc-p3.json | `98f6416ca6b1e87b83f27327e857b21e8121fbc6a81d104d972c7aae9ddca68f` |
| tools/rrc-readiness/record_p3.py | `b184c3b742891edfa5b2b8037a06778e4772765ce06a5f15f6d38c739f5be80e` |
| tools/rrc-readiness/root_probe_p3.c | `a9357516ff393912c540c7b1a1a318e128401617731bac081edb897eb0069eb7` |
| tools/rrc-readiness/verification-rrc-p3.json | `75c10a6983a0fa9e6a27f4740a2e25596fd78ba26bdcb1f2bae1f4e6ff61a229` |
