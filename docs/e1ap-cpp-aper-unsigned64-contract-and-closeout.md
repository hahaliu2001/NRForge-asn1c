# E1-P2 — finite unsigned64 INTEGER contract and closeout

Baseline: `5ff8a2e2c95825f5d16077ed4ccfe8128caae702`, accepted E1-P1.
Scope: shared unsigned64 constraint ownership and generation, focused semantic
checks and full frozen E1AP readiness rescan. E1-P3 outer dispatch and E1-P4
complete-PDU wire qualification remain separate.

## Representation and fail-closed boundary

Owned `asn1typed_integer_value_range_t` appends a tagged `unsigned_bounds`
mode with `uint64_t` lower/upper endpoints. In that mode the legacy signed
endpoints must be zero, the range must be present, finite, ordered, contiguous
and nonextensible, and tail/addition storage must be empty. Without the tag,
unsigned endpoints must be zero. Copy/API and renderer validation reject
contradictory, absent or hidden evidence before publishing output.

Parser numeric evidence is checked against zero and UINT64_MAX before any cast.
Wide parser configuration follows `HAVE_128_BIT_INT`; this environment supports
it. Existing signed constraints and permitted sets retain their representation.
The new mode owns bounds above INTMAX_MAX without narrowing. Named finite
nonnegative refinements can intersect unsigned and legacy positive ranges,
including a narrowed interval entirely below INTMAX_MAX. Unsupported mixed
negative-to-high-unsigned ranges, high unsigned unions and high unsigned
extensions are explicitly rejected. This is not arbitrary-precision support.

Named primitive generation emits uint64_t aliases and exact UINT64_C bounds;
full cardinality records `cardinality_minus_one = UINT64_MAX` and a separate
full-cardinality flag, avoiding UINT64_MAX+1. SEQUENCE/OPTIONAL/CHOICE use-site
mappings preserve unsigned endpoints and named subsets. IOC dependency ordering
recognizes legal inline unsigned metadata. Legacy uint, enum/inline enum,
private local-ID and envelope ProcedureCode gates refuse unsigned residue;
the existing signed envelope-header contract is not widened in E1-P2.

Reuse existing bounded_uint APER operations. No runtime wire primitive change,
wire-format fork, handwritten per-message workaround or frozen schema edit is
needed. The compiler's owned C IR layout changes, so consumers of that internal
library must rebuild; no stable IR ABI is claimed. Installed NGAP/F1AP public
C++ SDK code and runtime layouts are unchanged.

## Focused verification

The new unsigned fixture covers a full range, high lower bound, UINT64_MAX
constant, signed full range, inline and named CHOICE, mandatory/optional fields,
and named high/small refinements. Rendering happens after parser deletion.
Public owned-copy and renderer negative tests cover invalid tag, residue,
reversed bounds, absent evidence and extension contradiction. Additional tests
reject enum/inline enum residue, private local-key residue, and a modified
synthetic envelope with ProcedureCode 0..UINT64_MAX (no signed-header loss).
Oversized, mixed-negative, high-extension and high-union sources are refused.

Generated C++20 tests compare finite unsigned bytes against the accepted
independent integer bit model at 0, 1, 255, 256, 65535, 65536, INT64_MAX,
2^63 and UINT64_MAX, including all eight alignment residues and a following
bit. High-range offsets and singleton substitution, compound optional/CHOICE
round trips, out-of-range refinements, truncation, trailing/nonminimal/alignment
errors and input/output/wire budgets pass. Existing runtime checks cover exact
errors, sticky status and rollback behavior; no new E1 complete-message wire
qualification is inferred from these synthetic tests.

Reproduce after building dependencies:

```sh
make -C libasn1typed CXX=/usr/bin/g++ check -j3
make -C libaper CXX=/usr/bin/g++ check -j3
make -C tools asn1typed_codec_coverage -j4
python3 tools/e1ap-readiness/scan.py \
  --asn1-root /path/NRForge-RAN/src --probe tools/asn1typed_codec_coverage \
  --work /new/e1p2-scan --output e1p2-readiness.json \
  --cxx /usr/bin/g++ --envelopes
python3 tools/e1ap-readiness/check_regression.py \
  --asn1-root /path/NRForge-RAN/src --work /new/e1p2-regression \
  --output regression.json
```

## Acceptance evidence

The typed IR suite passes 42/42, the APER suite passes 10/10, and the final
focused unsigned test passes. All 72 E1AP messages pass all six scan gates;
144 strict BODY/envelope compilations complete without diagnostics.
F1AP regenerates 158 messages with 948 historical headers byte-exact; NGAP
regenerates 131 messages with 393 historical BODY headers byte-exact. Ten
representative BODY/envelope translation units pass strict compilation.
Retained input fingerprints match. Independent acceptance is recorded in
`tools/e1ap-readiness/review-e1-p2.md`; machine-readable receipts are
`readiness-e1-p2.json` and `ngap-f1ap-regression-e1-p2.json`.
Historical E1-P1, NGAP and F1AP reports remain unchanged. No complete SDK build,
full native-reference corpus, benchmark, interoperability, alternate-platform
or complete-PDU qualification is part of this acceptance.
