# Public installed RRC-P17 consumers

Preparation imports authenticated native pycrate0.7.11 descriptors for all 85
roots and rechecks P16 decoded semantic receipts. Native descriptors supply
complete expected owned models. The generated C++ calls public out-of-line
encode/decode wrappers only. All 248 cases compare whole models, bytes and
semantic bit counts across octets, contained_bits and exact completion modes.
Input mutation after decode checks ownership. Public negatives cover logical
truncation, trailing octets, nonzero terminal padding, zero depth, exhausted
work bits and insufficient input octets. Other runtime quota categories retain
the P16 internal qualification scope. Canonical opaque carriers retain P16's
finite limitations; arbitrary inner-carrier codecs are not newly qualified.

```
python tools/rrc-sdk-expanded-consumer/prepare.py --asn1-root /path/to/schema \
  --generated /path/to/sdk-work/generated --work /path/to/new-preparation
python tools/rrc-sdk-expanded-consumer/isolate.py --repo "$PWD" \
  --schema /path/to/schema --sdk-work /path/to/sdk-work \
  --prefix /path/to/rrc-install --consumer-input /path/to/new-preparation \
  --work /path/to/new-isolation --jobs 1
```

Isolation relocates the RRC installed SDK, temporarily hides supplied
original inputs, disables CMake package registries and audits all 86 translation
unit dependency closures (85 root consumers and the separate MIB example).
It verifies exact per-case wire output receipts. RRC is 0.2.0.
Fingerprint and header/archive mismatch tests must reject.
Four-protocol coexistence belongs to P18 and is not a P17 acceptance gate.
Original paths are restored in finally, including on failed acceptance.

Native oracle files are preparation inputs and unavailable during installed
execution. Extra unhidden checkouts cannot enter the compiler dependency closure.
