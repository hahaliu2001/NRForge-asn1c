# RRC-P8 independent review

Status: **PASS**. Reviewer: independent agent `/root/e1_sdk_review`.
Baseline: `0bc5b860ffc11378f88dbf5a0bef07b1d0c8f908`;
branch: `feature/f1ap-cpp-aper`. Approval authorizes this exact candidate plus
this review receipt for commit/push, without merge.

The canonical JSON object consists of `source_sha256` (the 13 files listed in
`verification-summary.json`) and `verification_sha256`, serialized with sorted
keys and compact separators, without a trailing newline. This receipt is excluded
from that snapshot to avoid a circular hash. No bound source changed after PASS.

- Canonical snapshot SHA256:
  `4fdae65d68abbac11e4aae23a9978495560aed3b024b47180d2b701369bef2ac`.
- Verification summary SHA256:
  `893556b4c881b8571f687d265dde35639ac55801a6e837b1ebb0f2dfb67aadd4`.
- SDK fingerprint:
  `804b916ab4c40203e0d40399d4776d07eb2718c125f549f49b6e4002708e7d47`.

Independent checks confirmed all 13 current candidate source hashes, all 126
producer input hashes, 2,582 preserved baseline blobs and the sole existing-file
change (`Makefile.am` distribution entries). All 138 regenerated files match.
The 74 installed files form the declared closure, installed RRC metadata matches
generation, the archive matches its receipt and `nm` independently confirms 128
strong public encode/decode wrappers. Relocated NGAP/F1AP archives match their
fresh builds and installed provenance; relocated E1AP matches its accepted install.

The reviewer independently reran all 10 actual seal tests, including separately
omitted encode/decode real link failures. All 64 installed public-root executables
were rerun: 143 real cases and 2,498 inherited negative checks matched the saved
logs. The four-protocol mixed executable was independently rerun successfully.
The 65-test CTest campaign, separate one-test MIB example, two genuine identity
rejections and 66 translation units' dependency evidence were inspected. Independent
dependency parsing confirmed all 407 paths and their manifest hash. Consumer
source hashes also match preparation. Regression suites pass 3/10/3/46/1.

Initial review findings about producer input sampling, wrapper anchor references
and evidence binding were resolved and rechecked. The final record binds both
installed RRC metadata files and the exact coexistence archive identities; existing
APER SDKs install provenance, not a separate manifest. Strong-symbol checks use
one parsed symbol set. The mixed consumer's strict-warning formatting was corrected.

GNU linkers exceeded the local 8 GiB memory limit; only the successful LLVM lld
18.1.3 single-thread relocated campaign is accepted. F1AP compiler GC flags affect
one large compilation unit's memory use. SDK archives, codec algorithms and schemas
are unchanged by these consumer/toolchain adjustments. Failed resource attempts
are not passing evidence.

No remaining blocker. Approval retains the finite P7/reference limitations:
64 supported/21 blocked roots, seven of 12 channels, SIB1 blocked, no full RRC
coverage or new 361-message APER wire campaign. No benchmark or merge.
