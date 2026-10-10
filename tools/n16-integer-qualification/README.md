# N16 bounded INTEGER primitive qualification

```sh
python tools/n16-integer-qualification/qualify.py --output tools/n16-integer-qualification/qualification-summary.json
```

Pin `asn1tools==0.167.0`; compile with a C++20 compiler. Before execution the tool hashes runtime.hpp/runtime.cpp and its driver/script; publication fails if these change. Compiler version, exact flags, schema hash and sources are recorded.

The tool compares a direct runtime signed/unsigned primitive driver with native aligned `per` and an independent Python arbitrary-precision bit model. Sixteen profiles exercise constants, cardinalities 2/3/255/256/257/65536/65537, signed 32-bit, nonzero-based 40-bit, both signed endpoint regions, full signed 64-bit, full unsigned 64-bit and the unsigned upper endpoint region. Each selected value runs at offsets 0–7 with alternating BOOLEAN prefix bits. Values include endpoints, midpoint, and applicable octet-length transitions through 2^56. The matrix has 928 selected values/offset cases.

The model uses the unsigned offset `value-lower`: cardinality <=255 has minimum unaligned bits; cardinality256 has aligned8; 257..65536 aligned16; larger ranges have minimum-octet count-minus-one in `ceil(log2(maximum_octets))` unaligned bits, then alignment and minimum whole-octet unsigned offset (zero uses one octet). Python arbitrary precision handles cardinality2^64 without overflow. No runtime arithmetic is reused.

Two standalone constant profiles generate no native fields and therefore empty native bytes. The runtime's approved complete-encoding substitution is `00`; these two cases are explicitly separated from the 926 exact native comparisons. Native constant-field semantics are decoded using the oracle's own empty wire. All other runtime bytes are decoded directly by the native implementation.

This is bounded positive primitive evidence. It does not qualify extension-marked INTEGERs, union constraints, unbounded INTEGERs, malformed-input acceptance, generated codecs, all NGAP schemas or complete PDUs. Runtime rejection, sticky errors, resource budgets and arithmetic tests are separate.
