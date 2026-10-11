# Direct NULL known-open strict compilation verification

```sh
make -C libasn1typed check
python tools/rrc-null-open/check.py --asn1-root /path/to/frozen/schema \
  --work /new/work/directory --output tools/rrc-null-open/readiness.json
```

Six SEQUENCE/CHOICE fixture roots cover direct NULL, BOOL and named NULL controls.
Strict C++20 checks use 12 hand-derived goldens in three completion modes and
NULL open-body octet substitution, malformed/truncated/trailing/padding/quota
failures. Current renderer/extractor are directly instrumented with ASan/UBSan;
LSan task enumeration is unavailable here.

The driver rebuilds both current and immutable-baseline renderers, regenerates
all 85 frozen roots and requires that removing only the new void-use markers
restores every baseline header exactly. The 83 unaffected roots keep their
hashes; LocationMeasurementIndication and UL-DCCH-Message pass strict compilation.
The 361-message/1773-header APER comparison and input drift guards remain active.
No wire behavior, schema, runtime or SDK expansion; SDK remains 64 roots. P16
will separately qualify the expanded real-message profile. No benchmark or merge.
