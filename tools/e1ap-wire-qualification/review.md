# E1-P4 independent review

Verdict: **ACCEPT** (2026-10-11). Reviewer: `/root/f1p6_review`, read-only independent review.
Candidate-SHA256: a947696d4aa505409c1df8972c8d0b44c5a41c93d73b6963e2f653af56eb8d2b

Reviewed the exact final candidate against 62 current source fingerprints, the
complete generated inventory, all 72 strict translation unit/object hashes and
empty diagnostic logs, native oracle, main, executable, archive, integration
attestation and recorded output hashes. Independently reran the public executable
and reproduced its output byte-exact: 1,341 wires, 3,824 physical/resource checks,
696 root opaque slots and 18 outer additions. Independently replayed the sealed
native oracle for every complete byte vector and semantic value: all passed.
The 72-message / 40-procedure closure, 317/317 declared rows, and all nine boundary
values on each of six unsigned64 UL/DL paths reconcile. No required fixes remain.

Review fixes re-enumerate the complete source/generated inventories at the final
gate and bind promotion to exactly one matching candidate SHA line. Independent
scratch tests rejected unrelated ACCEPT text, wrong hashes and duplicate hashes
before any accepted output; exact matching review roundtrip passed. Parent also
verified stale PASS output removal on existing-workdir failure and altered-archive
attestation failure. Only this exact reviewed candidate is authorized for promotion.

Shared production and frozen schema are unchanged. The regression receipt
preserves historical NGAP/F1AP profiles and prior generator evidence byte-exact;
three PDU tests pass. No new historical wire campaign is claimed. Native empty
private-object-set diagnostics remain visible; these cases passed exact opaque
bytes/semantics and do not qualify vendor payload interpretation.

Acceptance is finite complete-PDU agreement. Unreached sizes, typed fragmentation,
nested combinations, future typed extensions, contained/vendor interpretation,
application/state policy and live interoperability remain outside scope. The
framing model self-test alone does not establish large typed runtime coverage.
