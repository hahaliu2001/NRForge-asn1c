# RRC-P10 independent review

Status: **PASS**, with no remaining blockers.
Reviewer: independent agent `/root/e1_sdk_review`.
Baseline: `6cf38c3e6d8ab09d8298214b182eb8757c7aab80`;
branch: `feature/f1ap-cpp-aper`.

Approval binds canonical sorted compact JSON of `source_sha256` and
`verification_sha256`, without a trailing newline. This unique review receipt is
excluded from the snapshot. It may be added before commit/push; no bound source
may change afterward. No merge is approved or performed.

- Exact canonical snapshot SHA256:
  `05aa5050518e430e75454070406c1a185e7a94cdaf62006dbd884b2027d134f7`.
- Verification summary SHA256:
  `4830c284c8ccc792fe3f026b77b81f7d44c8861f087309d3151b78a8f802fb33`.
- Study report SHA256:
  `62d99eca6a69e4112d30663a0861dfc635ebaf593953ec776a5d9de08f01e0f0`.

The reviewer independently executed `study.py --check` in a new scratch work
directory. All four read-only probes were compiled with strict C warnings;
the resulting report matched byte for byte. All four ELF hashes and all five
raw JSON hashes match the final candidate and its separately repeated run.
All 64 freshly generated physical header hashes match accepted P6 evidence.
The 85-root ledger remains 64 rendered, 16 extraction blocked and five render
blocked. No new generated-header compilation or wire qualification is claimed.

Independent checks verified all six candidate source/report hashes, all 129
current producer input hashes and all 2,616 preserved baseline blobs. The sole
existing-file change is Makefile.am distribution entries. All production codec,
frozen schema, qualification and delivered SDK sources remain unchanged.

Targeted source, fixed-tree and owned graph evidence supports five families of
first diagnostics and 15 named seed inspections. Ten owned SetupRelease
instances lack tags: two matching fixed clones are tagged, eight are untagged.
All 264 fixed clones are accounted for (256 tagged, eight untagged). The
ordinary MobilityHistoryReport alias has no declared constraint but inherits
combined SIZE (1..16), while its direct list target renders successfully.

The initial independent reproduction caught an assumption that a specialization
actual's ref_expr cache target would exist. The final study resolves actual
identity through the authenticated lexical module/import namespace instead,
retains the original empty-cache evidence and does not weaken production IR
cache validation. Independent reproduction after this correction passed.

Static source-reference reachability and its 309 shape candidates are explicitly
conservative inspection leads, not instantiated closures, confirmed latent
failure counts or predictions of root unlocks. The plan distinguishes transfer
of existing tag evidence from resolution of missing evidence. P11 is one
primary objective: closed singleton INTEGER normalization; later ownership,
tagging, opaque container, alias, wire and SDK work is split into separate gates.

No performance benchmark or SDK rebuild is part of P10. The approved result is
this read-only study and plan, not expanded RRC coverage or changed SDK delivery.
