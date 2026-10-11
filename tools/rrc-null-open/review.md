# Direct NULL known-open lambda independent review

Verdict: **PASS**. Reviewer: `/root/e1_sdk_review`.
The reviewer explicitly approved this exact candidate plus this unique review
receipt for commit and push to `feature/f1ap-cpp-aper`, without merge.

- Canonical candidate snapshot SHA256:
  `0334b7473c69655456c138413d3b6ba63291a5c187e55368add115319eb9c9c5`
- verification.json SHA256:
  `3c034179999affb539436b15f74e03a1220963e8bc4f70a9a605f5233b6cdda2`
- readiness.json SHA256:
  `cf734a6ee28f43fc706b7eeb308bcb532d865749ac3b3c8b78b71bb23102a2a2`

The canonical snapshot is SHA256 of sorted, compact JSON containing
`source_sha256` (12 bound files, including readiness.json) and
`verification_sha256`, without a terminating newline. This receipt is excluded
from the snapshot. No approved source or verification changed after acceptance.

Independent validation confirmed all source/producer/binary/log hashes, 71 native
outcomes and all 2,679 retained baseline blobs against Git bytes. The inventory
uses baseline git ls-tree -r text excluding the four changed paths, in original
order with its trailing newline. The baseline renderer source is the immutable
Git blob. All 85 baseline RRC headers match P15; 83 current headers are identical,
and LocationMeasurementIndication/UL-DCCH-Message each add only two void-use
markers. Stripping these exact markers restores all 85 baseline headers. The two
previously blocked roots now pass unchanged strict C++20 flags.

The reviewer independently directly compiled current renderer/extractor with
strict GNU11 and ASan/UBSan and ran six owned NULL/BOOL/named-NULL fixture graphs.
Six generated roots independently pass strict C++20, 12 hand-derived goldens in
three completion modes and malformed/truncated/trailing/padding/quota negatives.
Named NULL and non-NULL are unaffected. No wire operation or runtime change.

All 1,773 historical APER physical headers were independently checked:
F1AP 948, NGAP 393 and E1AP 432. The SHA-bound completed harness log records
361-message extraction/generation PASS, and the SHA-bound helper requires full
JSON parsing and checks each message before printing those PASS results.

Evidence boundary: the scratch F1AP probe.json retained at final review is
incomplete (2,600,960 bytes; SHA256
52928b3c5153ce95d7fc324181b9ea8ece9b57c7fb6c6929b0381973cd779b68).
Its current raw outcomes cannot be independently parsed. The cause of this
retention anomaly is unknown. This unbound scratch raw file is not claimed as
independently parsed evidence. The reviewer verified F1AP's 948 physical headers
and the bound completed harness PASS log; the anomaly does not block this
UPER-only renderer repair. NGAP/E1AP retained JSON is complete.

Historical receipts remain immutable and SDK remains 64 roots. Expanded real
wire qualification remains P16. LSan was NOT_RUN due to environment task
enumeration limits. No benchmark or merge. No remaining blockers for this repair.
