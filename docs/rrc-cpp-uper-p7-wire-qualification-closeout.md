# RRC-P7 finite independent wire qualification

Baseline: `e3d7f359f7dac56bfe6d4f461b1c6991665dc912` on
`feature/f1ap-cpp-aper`. P7 adds qualification tooling and evidence only; the
accepted P6 production renderer, runtime, public model and frozen authorities
remain unchanged. This closes the finite wire gate before P8 SDK delivery.

## Profile and independent comparisons

| Profile | Scope | Qualification |
|---|---|---|
| Selected real roots | 59 of 80, including seven channels | Mandatory low/contrast plus channel branches reached in the mandatory baseline |
| Additional DEFAULT roots | Five | Presence, normalized defaults and contrast values |
| Frozen bare assets | MIB, SIB2, MTC | Native decode, exact C++ model, bytes and semantic bits |
| SIB1 | Frozen 117-bit asset | Retained, extraction-blocked; no wire PASS |
| Separate compound fixture | 59 cases | DEFAULT, groups, root/extensions, fragmentation, offsets and future policies |
| Remaining selected roots | 16 extraction gaps, five CHOICE evidence gaps | Retained with identities and first errors; no qualification claim |

The full campaign has 143 real and 59 compound cases. Pycrate 0.7.11 compiles
all six original authenticated modules in a fresh process without schema
specialization patches. Independent descriptor-driven values are built before
C++ model conversion. The bridge inspects public declaration/source-name metadata,
not codec bodies or IR wire instructions. It constructs the same semantics;
C++ native-wire decode must equal the entire expected owned value and consume the
exact independent semantic length. C++ output must equal native bytes and bit
length, and native decode of that output must equal normalized semantics.

This finite profile is not an all-field or all-value proof: optional fields are
not exhaustively populated, every SIZE endpoint is not covered, and all nested
choice combinations are not enumerated. Header generation remains the sealed
P6 59/16/5 matrix plus five DEFAULT seeds. All 64 header fingerprints are checked;
real native CHOICE closures are audited for tag order compatible with the pinned
reference implementation, including declarations not selected by a case.

## Framing and negative evidence

Compound values test canonical root indexes, known and unknown extensions,
ENUM extensions, SEQUENCE addition groups, absent/explicit/nondefault DEFAULT,
non-octet offsets 0..7, OCTET STRING length determinants 127/128 and fragments
16384/16385, and collection lengths 0/1/16383/16384/16385/65536. Future sender
schemas are separately owned test fixtures. Unknown SEQUENCE additions are
validated then discarded: decode input and reencode projection intentionally
differ. Unknown CHOICE open-type payloads are retained byte-for-byte, including
one-, two-octet and fragmented inner length determinants and offset 3 framing.

Negative cases check every truncation point for inputs <=256 semantic bits;
larger inputs use five sampled points. They check excess octets, nonzero terminal
padding, input cap, depth/work caps and measured value/collection/bitmap caps.
Unknown CHOICE quotas explicitly require `resource_limit`, consume any prefix,
and roll back to the prefix cursor. Failed compound parsing rolls back work,
value and collection usage. These generated tests supplement the unchanged P5
primitive invalid-code/length matrix and the freshly rerun P6 allocation and
nested rollback harness; those are separately identified regression evidence.

ASan/UBSan qualification covers the 59 compound cases, with leak inspection
disabled, plus fresh P6 C++ nested/generated allocation and rollback tests. The
C renderer allocation-injection test is rerun normally, not rebuilt under ASan
by this harness. The full real profile
is strict C++20, not advertised as all-real sanitizer coverage. No benchmark is
run. Exact executed counts and hashes live in the machine-readable reports.

## Explicit-tag reference gate

The original P6 `Pick` declares `first [2] Counter` before `nothing [0] NULL`
under AUTOMATIC TAGS. X.680 (02/2021) 29.2 suppresses automatic tagging for this
alternative list because textual tags are present; 31.2.4 makes both tags
context-specific. X.691 (02/2021) 10.2 and 23.2 require canonical outermost tag
order, so `nothing` has root index zero. Root `pick` index two, the two extension
flags and that index yield semantic bits `01000`, padded byte `40`.

Fresh actual C++ encode/decode confirms `40` / five bits. Asn1tools 0.167.0 and
pycrate 0.7.11 both emit `48` for the original synthetic case. Their implementation
audit identifies declaration-order CHOICE indexes (asn1tools omits tag sorting;
pycrate uses `_root.index`). This is a diagnosed reference limitation, not a
native independent PASS for reversed explicit tags. The normative mapping is
retained. No oracle patch, frozen source reorder or production workaround was
made. Four other original P6 synthetic observations still match both references.

Primary sources: [X.680 (02/2021)](https://www.itu.int/rec/T-REC-X.680-202102-I/en)
and [X.691 (02/2021)](https://www.itu.int/rec/T-REC-X.691-202102-I/en).
`reference-boundary.json` binds versions, fixture/header and implementation hashes,
actual C++ receipt and the explicitly expanded P6 BoxCounter specialization needed
by pycrate for that exploratory fixture. Actual frozen RRC modules are unmodified.
Pycrate also requires explicit DEFAULT `fallback=2` in an addition-group input;
its encoder suppresses it and decode restores it. This input limitation is
recorded, with no reference modification.

## Delivery boundary

`qualification.json` is the full finite campaign;
`qualification-sanitized.json` is explicitly compound-only. `verification.json`
binds tools, reports, documentation, fresh matrix and regression evidence.
`review.md` binds the reviewed canonical snapshot. No production or existing APER
SDK file changes are allowed in this batch. The 361-message regeneration campaign
is not rerun for this test-only batch; P6 evidence remains historical and intact.

P7 does not remove the 21 selected-root gaps or qualify all 2,630 declarations.
P8 must explicitly carry the supported finite manifest and blocked-root ledger
into a fresh relocated installed C++ SDK, with public bit-aware APIs and
F1AP/NGAP/E1AP/RRC coexistence. Python delivery remains P9. No merge is performed.
