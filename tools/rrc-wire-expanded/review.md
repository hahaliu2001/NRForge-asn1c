# RRC-P16 expanded finite wire independent review

Verdict: **PASS**. Reviewer: `/root/e1_sdk_review`.
The reviewer explicitly approved the exact candidate plus this unique review
receipt for commit and push to `feature/f1ap-cpp-aper`, without merge.

- Canonical candidate snapshot SHA256:
  `3f37cecd4ae87d62ad7c7af1c1eccbfad11fbecef1f02379c68c2b95370ad2d8`
- verification.json SHA256:
  `28df54aef1b061bde2262e902a9b8c9cee0e80f3c080f1bcdda6dc5d729017bb`

The canonical snapshot is SHA256 of sorted compact JSON containing
`source_sha256` (12 files, including both qualification reports) and
`verification_sha256`, without a terminating newline. This unique receipt is
excluded. No approved source or verification changed after acceptance.

Independent checks confirmed all 12 sources, 2,692 retained baseline Git blobs,
85 current real headers, compound headers, and all artifact/producer/oracle
hashes. All 86 executables were actually rerun: 248 real cases plus 59 compound
cases, with 9,142 negative checks. Native bidirectional semantics, exact
wire/bits and real closure tag audits passed. The reviewer actually reran the
compound ASan/UBSan executable: 59 cases, 3,551 negative checks. The original
143 P7 real cases and all 59 compound cases retain their complete case data.

The reviewer independently rebuilt all seven descriptor-selected witnesses,
checked four frozen bare seeds including SIB1, and verified 55 native outcomes
(libasn1typed 52, libuper 3). The root006 compile cache was independently bound
to current emitter output, native snapshots, corpus, producer inputs, runtime,
compiler/flags and executable hashes. Its executable was rerun in both the
completed campaign and independent review; all receipts underwent native checks.

Evidence boundary: two scratch executables had mode 0644 at review. Their bound
ELF bytes were independently executed using `/lib64/ld-linux-x86-64.so.2` without
chmod or any artifact modification. The completed campaign had already executed
its programs successfully. This scratch mode boundary does not change bytes,
receipts or finite wire outcomes and is not an installed SDK mode claim.
Initial parallel/default-GC OOM attempts are excluded from PASS evidence.

The scope remains finite selected semantics, canonical native-produced inner
bytes used as opaque carriers, and compound-only sanitizers. No all-field,
all-value, arbitrary malformed-inner or full RRC qualification is claimed.
The SDK remains 64 roots. Production, schema and historical evidence remain
unchanged. LSan is NOT_RUN for environment task enumeration limits. Historical
APER evidence is retained with its existing disclosed scratch F1AP caveat; no
new APER campaign is claimed. No benchmark or merge. No remaining P16 blockers.
