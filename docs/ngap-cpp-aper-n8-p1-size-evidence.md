# N8-P1 — Owned collection SIZE evidence

2026-10-09. Base `5e6ef747593828d6eef1376c7b13a1cf3e51e8cf`.

The [N8 contract](ngap-cpp-aper-n8-bounded-collection-contract.md) passed independent design review before implementation. Existing owned IR already stores scalar SIZE and owned element references. This task preserves ordinary SEQUENCE OF declaration SIZE evidence rather than introducing new IR ownership or enabling collection codecs.

Ordinary `populate_type` now passes declaration `combined_constraints` (falling back to declaration constraints) through the shared SIZE extractor with exact sizes enabled, then stores the scalar result. Unrepresentable/unsupported/negative SIZE fails with a concrete diagnostic; recognized extensible or over-generation-domain bounds remain faithfully retained. Missing SIZE remains extractable. Bound/IOC materialization, element ownership and all other type paths are unchanged.

Executed verification:

- Real Parser/Fixer extraction, then Parser deletion: SIZE(0), SIZE(2), 0..65535 and 1..3 remain readable in owned IR; BOOLEAN/named element references are retained.
- Missing SIZE has no constraint flag; extensible range/exact forms retain their extension flag; union SIZE(1 | 3) extraction fails with diagnostic and empty output.
- Persistent `check_asn1typed_collection_size` uses REQUIRE/abort and compiles with NDEBUG, so no silent assertion bypass.
- Fresh typed regression: **16/16 PASS**, including the new focused test.
- Extractor GNU99 strict warnings/missing-prototypes syntax and whitespace checks: PASS.

Independent source and focused evidence review: **PASS**, no blocking findings. No collection renderer/runtime capability or full NGAP qualification is claimed by this commit. P2/P3 remain separate commits in the authorized autonomous cycle.
