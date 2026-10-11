# RRC-P9 independent review

Status: **PASS**. Reviewer: independent agent `/root/e1_sdk_review`.
Baseline: `4a7124a4e5314ad14a000abe790a7394f3aefb0c`;
branch: `feature/f1ap-cpp-aper`. Approval authorizes the exact candidate plus this
single review receipt for commit/push, without merge.

Canonical sorted compact JSON consists of `source_sha256` and
`verification_sha256`, without a trailing newline. This review receipt is
excluded to avoid recursion. No bound source changed after approval.

- Canonical snapshot SHA256:
  `a9ab5965a1cf2260e59b7ffa85d8e0eb8d7b1edcbf7c5abca3988010b9c0dac9`.
- Verification summary SHA256:
  `7729a637ade3bf1e3291f818746e80cdc87f65c8b15683e9ba85922abf4344cb`.
- SDK fingerprint:
  `33fba902362ec9de13f05f4bc2605df32c129c2638373fc74bad88e3de3c5a3f`.

The reviewer independently ran all 12 installed RRC tests and five generator
seal/grammar tests. Markers confirm 143 real cases, 64 roots, three completion
modes, 19 DEFAULT omissions, 998 malformed/quota checks and 24 four-protocol
import orders. Three additional independent native integration fixtures cover
optional false, future ENUM and unknown CHOICE. No all-field/all-value or
all-quota-boundary claim is made.

Independent hash checks cover all 19 candidate source files, every current SDK
producer input, public headers/metadata and installed archive, all 68 generated
files plus their receipt, 12 staged source files, all 14 sdist members and all
eight wheel members. All four installed package members exactly match their
receipt-bound wheels, including the original accepted NGAP `a360e926...` wheel.
All 2,596 preserved baseline blobs match. The only existing source change is
Makefile.am distribution entries; all shared APER sources remain unchanged.

The outside-checkout isolation receipt and logs verify RRC 12, NGAP 12, F1AP 9
and E1AP 10 tests with source/schema/SDK/build inputs unavailable. Actual CMake
build/install reject altered generated C++ and accept exact restoration. The
five native regression logs match the receipt: libuper 3, libaper 10, libngap 3,
libasn1typed 46 and libasn1parser 1. The wheel is built from the newly extracted
sdist against the fresh sealed SDK, rather than a development build directory.

ELF inspection confirms no SDK shared-library dependency, RPATH or RUNPATH and
no strong NRForge codec exports. Weak standard-library template instantiations
containing RRC type names are present; they are not codec entry points. The GIL
is released only during owned C++ calls; conversions/results hold it, and
returned values do not borrow temporary storage. Current public models contain
no optional NULL; future such models fail closed pending presence qualification.

No blockers remain. CPython 3.12 Linux x86_64 and the inherited finite 64-root
profile are qualified; 21 blocked roots, including SIB1, remain unavailable.
No benchmark or merge was performed.
