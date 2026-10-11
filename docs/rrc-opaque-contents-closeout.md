# RRC-P14: exact opaque CONTAINING contexts

Baseline: `9c5f18a18cdc68d335a1365995ff0a1c11250261` (RRC-P13-B).
Branch: `feature/f1ap-cpp-aper`. Primary objective: extend existing exact opaque
OCTET STRING ContentsConstraint recognition to named primitives and CHOICE.
The independently reviewed boundary is `tools/rrc-opaque-contents/contract.md`.

## Repair and ownership

The existing complete ContentsConstraint recognizer previously protected only
SEQUENCE fields and selected IOC contexts. CHOICE alternatives and named
primitive declarations fell through to ordinary SIZE parsing and rejected these
opaque byte carriers. Only `libasn1typed/asn1typed_extract.c` changes production
behavior. The recognizer is reused unchanged, with no new owned IR, renderer,
runtime, fixer or schema behavior.

CHOICE uses the same declared/effective exact Contents guard as SEQUENCE.
Named primitive extraction scans every ordinary primitive alias layer before
SIZE fallback. When any layer has exact Contents, outer effective evidence must
be exact and each declared/effective layer must be exact or absent. This avoids
widening an alias after its effective evidence is removed, replaced with pure
SIZE or forged to conceal another declared restriction. Ordinary non-Contents
SIZE handling is preserved. Existing parameterized-use rejection is preserved.

The result is owned unconstrained OCTET STRING bytes with ordinary length
framing. The contained type is not added to the typed dependency closure or
recursively decoded. Self-containing RRC response types and unsupported inner
payload types therefore do not require an inner codec. Carrier qualification
does not qualify the nested payload. Mixed SIZE/Contents, malformed wrappers,
non-OCTET forms and ENCODED BY remain rejected; ENCODED BY is outside the existing
parser grammar. No supported SIZE is silently discarded.

## Validation and readiness

Seven focused carrier graphs cover named reference-declared Contents,
intermediate aliases over a plain terminal OCTET STRING, inline/named CHOICE,
mandatory/optional SEQUENCE, self-containing and unsupported REAL inner types.
Extraction omits contained dependencies and owned rendering succeeds after AST
destruction. Declared/effective malformed/mixed mutations, hidden alias SIZE,
missing/forged effective evidence, failed graph rollback and quota tests pass.
An independent reviewer reproduced the intermediate-alias gap in an earlier
candidate; the complete-chain repair and matching negatives close that finding.

Seven generated fixture roots pass strict C++20 and sixteen hand-derived golden
encodings in all three completion modes. Arbitrary bytes, empty payloads,
CHOICE/null/optional presence, truncation, padding and encoder/decoder quotas
are covered. ASan/UBSan directly instrument the current extractor and focused
ownership checks; LSan is NOT_RUN due to environment task enumeration limits.
Native checks pass 50 typed, two fixer, three UPER, ten APER, three NGAP and one
parser tests: 69 total PASS.

The frozen 85-root matrix is 83 text-rendering PASS and two extraction blocks.
All 77 P13-B physical headers remain byte-identical. Six newly rendered roots
pass strict compilation:

| Module | Root | Strict C++20 |
| --- | --- | --- |
| NR-RRC-Definitions | DL-DCCH-Message | PASS |
| NR-RRC-Definitions | RRCReconfiguration | PASS |
| NR-RRC-Definitions | RRCReconfigurationComplete | PASS |
| NR-RRC-Definitions | RRCRelease | PASS |
| NR-RRC-Definitions | RRCResume | PASS |
| NR-RRC-Definitions | RRCResumeComplete | PASS |

UL-DCCH-Message now reaches the same MobilityHistoryReport-r16 constructed-alias
block as UEInformationResponse-r16; both remain P15 work. This is a changed
first diagnostic, not a newly accepted root. LocationMeasurementIndication
retains its P13-A unused-io strict compilation block and byte-identical header.
Text-rendering success remains distinct from strict compile and wire success.

The shared APER regression verifies 361 messages and 1,773 historical physical
headers: F1AP 158/948, NGAP 131/393 and E1AP 72/432, all byte-identical. Receipts
bind current sources, producer archives, binaries and logs with drift guards,
followed by independent exact-snapshot acceptance before commit/push.

Historical P7-P13-B evidence remains immutable. SDK delivery is still 64 roots;
new real-message wire qualification and SDK expansion remain P16-P18. The
known NULL-extension renderer warning is separate work. No benchmark or merge.
