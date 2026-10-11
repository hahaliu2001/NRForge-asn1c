# E1-P2 independent review

Verdict: **ACCEPT**. Baseline: `5ff8a2e2c95825f5d16077ed4ccfe8128caae702`.
Independent read-only reviewer: `/root/f1p6_review`; review/fix loop completed.

Reviewed finite unsigned64 owned IR extraction, copy validation, named use-site
refinements, integer/compound/IOC rendering and rejection gates. Initial
findings on hidden unsigned residue in legacy/enum/private-ID paths, signed
envelope ProcedureCode loss, and parser-width configuration guards were fixed
and covered by negative tests. No blocking findings remain.

Independent evidence checks:

- Focused generated unsigned C++20 script: PASS, including parser deletion,
  independent integer bit-model bytes and malformed/budget cases.
- Typed IR suite: 42/42 PASS; APER suite: 10/10 PASS. Final focused rebuild
  also passes after parser-width guard and high-union negative fixture changes.
- E1AP: all 72 messages pass six readiness gates; all 144 strict compiler
  invocations have rc0 and empty diagnostics. Raw rows, retained inputs and
  generated-header hashes independently match the final report.
- Real DataUsageReport/MRDC-DataUsageReport UL and DL types and mappings
  preserve exact unsigned endpoints 0..UINT64_MAX.
- F1AP: all 158 physical/envelope descriptors and three generation families
  pass; 948 historical generated headers independently match byte-for-byte.
- NGAP: all 131 physical/envelope descriptors and three generation families
  pass; 393 historical BODY headers independently match byte-for-byte.
- Ten representative BODY/envelope strict translation units pass with empty
  diagnostics. Aggregate fingerprints and unchanged historical runtime headers
  were independently checked.

Receipts: `readiness-e1-p2.json`, `ngap-f1ap-regression-e1-p2.json`.
Contract: `docs/e1ap-cpp-aper-unsigned64-contract-and-closeout.md`.

Limits: finite contiguous nonextensible unsigned64 only; high unsigned
sets/extensions, mixed negative-to-wide intervals and arbitrary precision
remain rejected. Internal C IR consumers must rebuild. NGAP historical byte
comparison covers BODY only. No complete SDK rebuild, full native-wire corpus,
live interoperability, benchmark, alternate-platform qualification or E1-P3
complete-PDU dispatch claim is made. Frozen ASN.1 and historical reports remain
unchanged.
