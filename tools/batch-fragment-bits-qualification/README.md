# Bounded wide BIT SIZE native evidence

Install the pinned `asn1tools==0.167.0`, then run:

    python3 tools/batch-fragment-bits-qualification/qualify.py --output tools/batch-fragment-bits-qualification/qualification-summary.json

The tool uses actual ASN.1 `BIT STRING(SIZE(0..131072))` and
`BIT STRING(SIZE(1..131072))`, prefixed by 0..7 BOOLEAN fields. It compares
native aligned-PER encoding/decode, a separate bit model and the actual new
runtime FieldReader/FieldWriter methods at ten boundary lengths. There is no
surrogate schema or full-PDU claim. `source-evidence.json` records the newly
exposed frozen use-site that justifies this capability follow-up. Reports must
be replayed after shared runtime files are integrated to refresh fingerprints.
