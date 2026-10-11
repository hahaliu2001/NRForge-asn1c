# RRC-P15 inherited constructed alias verification

```sh
make -C libasn1typed check
python tools/rrc-inherited-alias/check.py --asn1-root /path/to/frozen/schema \
  --work /new/work/directory --output tools/rrc-inherited-alias/readiness.json
```

Focused native checks cover direct, multihop, imported aliases, duplicate lexical
names in a separate compound-names fixture, bounded/open/unbounded list SIZE,
named and anonymous elements, declaration/member source locations and owned
lifetime. Missing/narrowed/widened/forged effective SIZE, hidden declared
restrictions, cache mismatches, malformed constraints, rollback and quota fail.
Known explicit extension-addition bounds remain owned; existing renderer block
is checked separately and remains outside this codec profile.

Ten generated roots exercise 22 hand-derived goldens in three completion modes,
including list/value bounds, reserved length patterns, truncation and quotas.
Direct and alias encodings match the same goldens. ASan/UBSan directly instrument
the current extractor and focused checks; LSan is unavailable in this environment.

The driver authenticates the six frozen schema modules and current producer
inputs, recompiles probes, inspects the real alias/terminal and both new root
closures after AST destruction, measures all 85 roots, compares the prior 83
physical headers, compiles newly rendered roots, and compares 361 APER messages
and 1773 historical physical headers. Text rendering is distinct from strict
compile and wire qualification; inherited renderer compile blocks stay explicit.
SDK delivery remains 64 roots. New real-message qualification and SDK expansion
remain P16-P18. No benchmark, schema change or merge.
