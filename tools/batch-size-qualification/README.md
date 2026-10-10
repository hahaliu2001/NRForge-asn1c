# Extensible SIZE framing evidence

Run with asn1tools0.167.0:

    python3 tools/batch-size-qualification/qualify.py --output tools/batch-size-qualification/qualification-summary.json

The C++ driver uses actual `FieldReader`/`FieldWriter` methods. Every vector
compares an independent bit model, native bytes, runtime encode/decode and
native decode. The native library does not implement actual BIT STRING
extension branches; extension evidence uses the explicit selector plus
unconstrained BIT STRING surrogate. Original root cases additionally compare
the true extensible schema. This limitation is recorded in the JSON.

`frozen-use-sites.json` records six focused real physical graphs. It is not a
full scan and does not claim wire or strict-compile qualification.
