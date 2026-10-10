# N15 bounded BIT STRING primitive qualification

Run with the pinned independent oracle:

```sh
python tools/n15-bit-qualification/qualify.py --output tools/n15-bit-qualification/qualification-summary.json
```

Requires `asn1tools==0.167.0` and a C++20 compiler. The tool snapshots runtime/header/driver/script SHA-256 before execution and refuses publication if any source changes. The summary records compiler version and exact flags.

The synthetic ASN.1 schema uses unnamed BIT STRINGs so trailing-zero bits retain their specified semantic length. Profiles cover fixed SIZE 0/1/8/16/17/65535, variable SIZE 0..3, 1..256, 0..65535 and unconstrained unfragmented lengths through 16383 bits. Every selected size runs at offsets 0–7 with alternating BOOLEAN prefix bits. Payloads have nonconstant octets and zero unused storage bits. The matrix contains 296 cases.

Three independently implemented paths compare complete bytes and decoded semantic bit counts/payloads: native asn1tools aligned `per`, a Python bit model following X.691 BIT STRING alignment/length rules, and the direct runtime primitive C++ driver. Fixed lengths <=16 are unaligned; fixed >16 align before payload. Variable bounded lengths encode constrained bit count then align payload, including zero length. Unconstrained lengths align their one/two-octet determinant and carry exactly the specified bit payload.

Examples with a leading true BOOLEAN: fixed16 payload `1035` produces `881a80`; fixed17 payload `113600` (17 meaningful bits) produces `80113600`; SIZE0..3 empty produces `80`; SIZE0..3 one true bit produces `a080`. These native results explicitly exercise the threshold and zero-length variable alignment.

One complete-boundary difference is reported separately: standalone fixed SIZE(0) produces empty native bytes, while the approved runtime substitutes `00`. This case is not counted as an exact native byte comparison; native decoding checks its empty-field semantics on the oracle's empty wire.

Evidence is limited to selected primitive positive cases, not malformed-input validation, named-bit lists, extension constraints, fragmented unconstrained payloads, generated codecs, all NGAP types or complete PDU qualification. Persistent runtime tests cover rejection/atomicity/budgets separately.
