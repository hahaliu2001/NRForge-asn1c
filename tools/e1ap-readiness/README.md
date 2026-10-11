# E1-P1 — frozen-schema readiness scan

Read-only study of TS 37.483 V18.6.0 Release 18, from
`hahaliu2001/NRForge-RAN` commit `d6e514a33ec3695925c24aa292900514b371b074`.
The authority is `src/e1/asn1/README.md` at that commit. Its cover identity
supersedes stale DOCX core metadata referring to TS 38.463 Release 16.
This study consumes the accepted extracted modules; it does not reacquire or
requalify the original official ZIP/DOCX extraction.

Six modules, in normative order, total 249,289 bytes. Their direct concatenation
SHA-256 is `5cfd3832772f7898360ba1a6d3a09c5ca9c78c3d9a97e6b5e009880bfcc685b9`.
`source-manifest.json` records byte lengths, Git blob identities and SHA-256.
The external ASN.1 root corresponds to `NRForge-RAN/src`. No schema is copied
into this compiler repository or modified for the study.

## Reproduce

Configure/build the compiler using the repository's Autotools instructions.
For a fresh tree, build dependencies before the typed extractor:

```sh
autoreconf -iv
CC=/usr/bin/gcc CXX=/usr/bin/g++ ./configure --disable-shared
make -C libasn1common -j4
make -C libasn1parser -j4
make -C libasn1fix -j4
make -C libasn1typed -j4
make -C tools asn1typed_codec_coverage -j4
python3 tools/e1ap-readiness/test_scan.py --asn1-root /path/NRForge-RAN/src
python3 tools/e1ap-readiness/scan.py \
  --asn1-root /path/NRForge-RAN/src --probe tools/asn1typed_codec_coverage \
  --work /new/scan-directory --output report.json --cxx /usr/bin/g++ --envelopes
```

The work directory must not exist. The probe uses the existing generic explicit
module interface; NGAP and F1AP tools and production code are unchanged.
The bounded inventory recognizer is specific to this hash-pinned schema, not a
general ASN.1 parser. It reconciles both procedure root sets, declarations,
message bodies, procedure codes, roles and pinned counts: 40 procedures,
72 outcomes (40 initiating, 20 successful, 12 unsuccessful).

## Evidence boundaries

Parser/Fixer operate on all six modules. The common probe deletes the parser
tree before generating from owned evidence. Each row records physical
extraction, all three BODY generation families, strict C++20 syntax compilation,
and target-envelope extraction/generation/compilation. Generation eligibility
and compilation eligibility are separate gates. `NOT_RUN` records a masked gate;
it is not a failed invocation. Input identities are checked before and after.

No executable wire round-trip, complete PDU dispatch/linkage, native-reference
comparison, interoperability, SDK delivery, performance or benchmark is claimed.
First-failure clusters count observed affected messages; later dependencies
remain masked and these counts are not predicted unlock counts. Ordinary
inventory/use-site evidence does not fully inspect bound-instance internals.

`readiness.json` preserves per-message evidence and input fingerprints.
The plan is in `docs/e1ap-cpp-aper-readiness-and-batch-plan.md`;
`review.md` records independent review.

## E1-P2 shared unsigned64 follow-up

The new tagged finite unsigned interval preserves full uint64 endpoints through
owned extraction, named refinements, inline SEQUENCE/CHOICE and IOC generation.
`readiness-e1-p2.json` is the separate follow-up; historical `readiness.json`
is unchanged. See `docs/e1ap-cpp-aper-unsigned64-contract-and-closeout.md` for
boundaries, tests and reproduction commands.

`check_regression.py` regenerates all 158 F1AP and 131 NGAP messages against
their frozen sources. It checks every BODY header against the accepted F1-P3
baselines, every F1AP target-envelope header, all BODY/envelope generation
families and descriptors, unchanged accepted runtime headers, then strictly
compiles five representative messages per protocol. It does not claim a full
SDK rebuild or rerun of historical native wire qualification. Run Python
normally (without `-O`), since assertion failures are evidence gates.
