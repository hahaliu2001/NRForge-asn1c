# RRC-P5 UPER primitive qualification

Install the exact versions in `requirements.txt`, then from the repository root:

```
python3 tools/uper-qualification/qualify.py --report /tmp/uper-qualification.json
make -C libuper check
```

The script builds the C++ probe in a temporary directory. Generated synthetic
ASN.1 wrappers supply BOOLEAN prefixes at offsets 0–7. The full schema hash,
individual wire/value hashes, semantic bit counts and runtime/probe/script
source hashes are in `qualification.json`. The deterministic script owns the
schema; it does not read or patch frozen RRC sources.

Every case requires native independent encode/decode, exact NRForge encoding
bytes and semantic bit count, and NRForge decode of native bytes. When both
native codecs support a case they must also agree on bytes and bit count.
Machine-domain extremes, fixed/bounded/unconstrained/extensible strings,
16K/64K fragmentation, ENUMERATED extension indexes 63/64, bitmap lengths
64/65 and open CHOICE payloads are included. SEQUENCE/CHOICE wrappers drive
primitive contracts only; no generated RRC compound implementation is claimed.

Known upstream oracle gaps are kept explicit: pycrate 0.7.11 fragmented BIT
STRING codecs mishandle total length/multiple fragments, and its long bitmap
length uses the wrong normally-small form. asn1tools 0.167.0 extensible BIT
STRING encoding is unimplemented and its fragmented open framing is defective.
Bitmap counts above 127 are clause-derived focused checks only. The other unmodified codec
owns the relevant rows. No patched oracle or local round-trip substitutes for
independent wire evidence. Empty complete encodings and NULL open payloads are
covered by clause-derived focused tests; the native compound bitmap fixture
uses BOOLEAN to avoid asn1tools' zero-length NULL open payload behavior.
