# N14 bounded OCTET STRING qualification

Run from a Python environment containing `asn1tools==0.167.0`:

```sh
python tools/n14-octet-qualification/qualify.py --output tools/n14-octet-qualification/qualification-summary.json
```

The tool compiles a direct-runtime C++20 stdin driver with strict warnings. It compiles an independent synthetic ASN.1 schema using asn1tools' **aligned** `per` codec and compares complete encodings, runtime decoding of native encodings, and native decoding of runtime encodings. A separate Python bit model implements X.691 clause 17 OCTET STRING size/alignment rules and clause 11.9 unfragmented length determinants. It does not import runtime implementation logic.

Profiles cover fixed lengths 0–3, bounded SIZE 0..3, 1..256 and 0..65535, and unconstrained unfragmented lengths through 16383. Every profile runs at field offsets 0–7 with alternating BOOLEAN prefix bits and deterministic nonconstant payloads. Length boundaries include 0/1/2/3, 127/128, 255/256 and applicable domain endpoints. The run checks 264 selected cases in each runtime direction, not every possible payload or length.

One external-oracle difference is explicit: asn1tools emits **zero octets** for standalone fixed SIZE(0), while the approved runtime complete-encoding contract substitutes one `00` octet. For this one case, comparison applies the documented complete boundary substitution and native decode checks its empty-field semantics using its own empty wire. The summary separates this case from exact native byte comparisons. All other 263 cases compare exact native bytes without transformation.

This evidence qualifies these bounded primitive framing cases only. It does not qualify generated codecs, fragmented unconstrained strings, malformed-input handling, all ASN.1 schemas, NGAP bodies or complete PDUs. Runtime rejection/atomicity/resource-limit tests are maintained separately. No external implementation's acceptance of malformed input is treated as normative.
