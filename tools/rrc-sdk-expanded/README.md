# RRC-P17 sealed C++ SDK 0.2.0

This parallel delivery exposes the finite P16 profile: 85 roots (80 selected,
five DEFAULT seeds), all 12 selected channel roots, 170 public archive wrappers.
The historical P8/P9 SDK 0.1.0 and its qualification receipts are unchanged.
This is not a full TS38.331 coverage claim. Public owned models and UPER
completion semantics remain the previously qualified API.

Use authenticated TS38.331 18.10.0 schema files from the source manifest.
Python preparation requires pycrate0.7.11. CMake >=3.20, GCC13 and lld are used
for the recorded build. The generator gates raw headers against all P16 hashes,
seals source/generated inventories and installs relocatable NRForgeRRC 0.2.0
exports. Build jobs default to one; GCC GC expand5/heapsize16384 and lld threads1
are explicit resource controls for this 8GiB environment, not performance data.

```
python tools/rrc-sdk-expanded/build.py --asn1-root /path/to/schema \
  --work /path/to/new-work --prefix /path/to/new-install --jobs 1
python tools/rrc-sdk-expanded/build.py --asn1-root /path/to/schema \
  --work /path/to/new-repeat --prefix /path/to/unused --generate-only
python tools/rrc-sdk-expanded/verify_seal.py --repo "$PWD" \
  --sdk-work /path/to/new-work --work /path/to/new-seal-check
```

Source/generated/archive mutation, inventory changes, missing receipt and
omitted encode/decode wrapper tests must reject. The actual 85-root anchor
links both public wrappers for every root before the archive receipt exists.
See contract.md and ../rrc-sdk-expanded-consumer/README.md for acceptance scope.

On constrained disks, `verify_seal.py --discard-archive-copy-after-pass` removes
only its dedicated restored archive copy after all ten checks pass and records
the verified hash; the original SDK archive remains available for rechecking.
