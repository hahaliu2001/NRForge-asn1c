# RRC-P14 opaque ContentsConstraint verification

```sh
make -C libasn1typed check
python tools/rrc-opaque-contents/check.py --asn1-root /path/to/frozen/schema \
  --work /new/work/directory --output tools/rrc-opaque-contents/readiness.json
```

The native test verifies seven carrier graphs, empty nested dependency closure,
self-containing and unsupported-inner types, alias-layer restrictions, exact
both declared/effective constraints, malformed/mixed rejection, failed extraction
rollback, quotas and owned rendering after AST destruction. A removed effective
constraint or forged pure SIZE must not widen an opaque alias. ENCODED BY remains
rejected by existing parser grammar. No new owned IR is needed.

Strict C++20 generated checks use sixteen hand-derived goldens in three completion
modes. Arbitrary payload bytes, empty bytes, inline/named CHOICE and optional
SEQUENCE framing, truncation, padding and encode/decode quotas are covered.
ASan/UBSan directly instrument the current extractor and ownership checks;
LSan is not run where environment process-task enumeration is unavailable.

The driver authenticates the six frozen schema files and current producer
inputs, recompiles probes, measures all 85 roots, compares the prior 77 physical
headers, compiles newly rendered roots, and compares 361 APER messages and 1773
historical physical headers. Existing NULL-extension renderer compile limitations
are kept separate from text-rendering PASS. SDK delivery stays at 64 roots;
expanded real-message wire and packaging qualification remain P16-P18.
No schema change, benchmark or merge.
