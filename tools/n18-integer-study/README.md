# N18 INTEGER domain study evidence

Study only: no production support enabled and no NRForge codec qualification claimed. Baseline N17 commit `0bda141dea90f9df8baba0d6ecc64598c2330f00`. Accepted readiness remains 69/131 strict BODY compilation passes.

## Actual owned domain inventory

`inventory.py` selects exactly the 20 INTEGER and four reference first failures from committed N17 readiness. It verifies the six frozen TS38.413 V18.10.0 source SHA256 values, calls a read-only probe built from the existing runtime/IR/renderers, and records scalar/tail evidence after Parser deletion. Relevant domains further down an owned graph are included; their owned ordinal is not a proven physical lowering failure position. No predicted unlock count is recorded.

Build the existing parser/fixer/common/typed libraries first with the repository's ordinary Autotools flow. From the repository root, build the study probe outside the repository:

```sh
cc -DHAVE_CONFIG_H -I. -Ilibasn1common -Ilibasn1parser -Ilibasn1fix -Ilibasn1typed -Itools \
  tools/n18-integer-study/probe.c tools/developer_tree.c \
  libasn1typed/.libs/libasn1typed_extract.a libasn1typed/.libs/libasn1typed.a \
  libasn1fix/.libs/libasn1fix.a libasn1parser/.libs/libasn1parser.a \
  libasn1common/.libs/libasn1common.a -o /tmp/n18-inventory-probe
python3 tools/n18-integer-study/inventory.py --asn1-root /path/to/frozen-ngap \
  --probe /tmp/n18-inventory-probe --output /tmp/n18-domain-inventory.json
```

Compare the output with `domain-inventory.json`. It records 20 unique relevant extensible domains: 14 named (12 contiguous/two discontinuous), four inline fields and two inline alternatives. Source fingerprints and baseline failure diagnostics are retained. Do not treat the first stored interval of a discontinuous root as the entire root.

## Native/model study

In an isolated Python environment install `requirements.txt`, then run:

```sh
python3 tools/n18-integer-study/reference.py --output /tmp/n18-reference-summary.json
```

Compare with `reference-summary.json`. Accepted count: 768 model/native APER encodes and native decodes, comprising 280 root and 488 extension cases, eight root profiles and all initial bit residues. A following BOOLEAN exposes the field boundary. Selected vector `field_end_bit` includes that Boolean; it is not the INTEGER end cursor.

The model uses actual signed extension values with minimum two's-complement octets and a length determinant, versus root-relative constrained offsets. The primary source is ITU-T X.691 (02/2021), clauses 11.4/11.5/11.8/11.9/13. The downloaded standard is not redistributed here.

These checks exercise only the study model and pinned asn1tools 0.167.0. They do not exercise a new NRForge primitive, malformed-input policy, operation atomicity, sticky state, budgets, generated codecs or complete NGAP PDUs. The implementation and qualification gates are in `docs/ngap-cpp-aper-n18-extensible-integer-contract-study.md`.
