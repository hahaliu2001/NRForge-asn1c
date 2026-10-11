# RRC-P10 remaining blocker study and implementation batches

## Decision and frozen scope

RRC-P10 is a read-only shared-capability study, based on RRC-P9 commit
`6cf38c3e6d8ab09d8298214b182eb8757c7aab80` on `feature/f1ap-cpp-aper`.
The source remains NRForge-RAN `abc850e5461e1b15cf5c81907d0d0fdd5e3a1179`,
TS 38.331 V18.10.0, six frozen modules with combined SHA256
`97a7f13be4c515c9c78e5f752647807621d59568ca33eeea6258513731462d2b`.
No schema, extractor, renderer, runtime, qualification or delivered SDK source
changes in P10. P1–P9 records remain immutable historical evidence.

Fresh linked probes reproduce the same 85-root ledger: 64 render successfully,
16 fail extraction and five extract but fail rendering. All 64 generated header
hashes match P6. This study does not compile those headers or qualify their wire
again; accepted P7/P8/P9 evidence remains the delivery authority. The 85 roots
are the selected 80 plus five DEFAULT seeds, not every type in the six modules.

**Next implementation: RRC-P11, closed singleton INTEGER normalization.** Its
primary objective is the shared primitive capability, followed by a measured
readiness refresh; it is not a promise to complete RRCSetup or the entire schema.
Repair shared semantics rather than special-casing RRC messages. Tier-A RRC
integration must wait for required payloads and channel containers to pass their
own extraction, rendering, wire qualification and updated SDK gates.

## Observed first failures

The family assignment below combines exact first diagnostics with targeted
source and fixed-tree inspection. Counts describe the first observed error only;
a repaired first error can reveal further blockers in the same root.

| Shared family | First-blocked roots | Direct evidence |
| --- | ---: | --- |
| Closed singleton INTEGER | 4 | `SRB-Identity-v1700 ::= INTEGER (4)`; `MCCH-RepetitionPeriodAndOffset-r17.rf1-r17 INTEGER (0)` |
| Constrained primitive SEQUENCE OF element | 5 | INTEGER ranges and BIT STRING SIZE on inline elements are rejected before ownership |
| Opaque CONTAINING in CHOICE/named primitive contexts | 6 | SEQUENCE-field recognition exists; CHOICE/named primitive paths reject these exact forms |
| Constructed alias inherited constraints | 1 | Alias has no declared constraint but inherits a combined SIZE constraint |
| Parameterized CHOICE tag evidence | 5 | Ten distinct owned `SetupRelease<T>` instances lack validated root tags |

The full 21-root ledger, exact diagnostics, source locations, conservative source
paths and named seed results are in `tools/rrc-blockers/study.json`.

| Family | Roots with that first failure |
| --- | --- |
| Singleton INTEGER | DL-CCCH-Message, RRCSetup, RRCReconfiguration, SystemInformation |
| Constrained collection element | BCCH-DL-SCH-Message, SIB1, LoggedMeasurementConfiguration-r16, SidelinkUEInformationNR-r16, UEAssistanceInformation |
| Opaque CONTAINING context | DL-DCCH-Message, RRCReconfigurationComplete, RRCRelease, RRCResume, RRCResumeComplete, UL-DCCH-Message |
| Constructed alias | UEInformationResponse-r16 |
| Parameterized CHOICE tags | LocationMeasurementIndication, RRCReestablishment, RRCReconfigurationSidelink, RemoteUEInformationSidelink-r17, SCCH-Message |

## Semantic findings and repair boundaries

### Singleton INTEGER

Both named `INTEGER (4)` and inline `INTEGER (0)` have a `SET/SingleValue` fixed
constraint. `extract_integer_value_range` accepts a closed range or a union at
this root position, but does not normalize a solitary root value. It already
normalizes an explicit known INTEGER extension addition's single value using a
local range view. Reuse the owned domain representation as `[v,v]` rather than
inventing an RRC-specific integer type. Preserve root versus extension semantics;
P11 initially covers the finite nonextensible single-value form, with malformed,
unsupported or unrepresentable forms still rejected.

The constrained UPER cardinality is one, so the value contributes zero semantic
bits. This must not be confused with octet completion/empty substitution or
with a DEFAULT presence decision. Out-of-domain encode must fail even when no
value bits are emitted. Test named, field, CHOICE and collection contexts only
where the corresponding ownership capability exists; collection ownership is
P12, not implicitly supplied by P11.

### Constrained primitive collection elements

PCCH-Config's `firstPDCCH-MonitoringOccasionOfPO` alternatives contain bounded
lists of `INTEGER (0..139)` and related ranges. `SL-InterestedFreqList-r16` and
`PropagationDelayDifference-r17` are named lists of constrained INTEGER;
CAG-Config-r18 contains a list of `BIT STRING (SIZE (32))`. P6 owns constructed
inline elements, but the primitive-element path calls
`reject_unowned_inline_constraint` before storing `element_type`.

Own list SIZE and element value/SIZE constraints independently. A list of fixed
32-bit values must retain the element width; an INTEGER range must not become
an unconstrained storage integer. Prefer reuse of existing owned type references
and authenticated anonymous element identities. If that cannot represent the
semantics, freeze the smallest necessary IR contract before implementing it.
Do not drop element constraints or widen an alias. Named and anonymous lists,
inline groups, bounds, failed extraction rollback and quotas need focused checks.

### Opaque CONTAINING contexts

The exact existing `is_opaque_contents_constraint` recognizer is used for
SEQUENCE fields. It does not protect the CHOICE alternative paths for
`nr-SCG-Response`/`nr-SCG-r16`, nor named `SRS-PosRRC-Inactive-r17` in the same
way. These payloads remain OCTET STRING bytes; do not recursively instantiate
or decode the contained type just to unblock the carrier. In particular,
RRCReconfigurationComplete contains itself through a response carrier, which
must not become a semantic recursion requirement.

Reuse recognition only for the exact supported complete ContentsConstraint and
validate combined constraints. Distinguish unsupported mixed constraints from
an allowed opaque container; SIZE/value constraints cannot be silently discarded.
Qualification of a carrier's bytes never qualifies the nested payload codec.

### Constructed alias inheritance

`MobilityHistoryReport-r16 ::= VisitedCellInfoList-r16` has no source-side
constraint. The fixed tree gives it an inherited combined `SIZE (1..16)`.
The target `VisitedCellInfoList-r16` extracts and renders successfully by itself,
while the alias fails `constrained ordinary alias unsupported`.

The alias loop tests `alias->constraints || alias->combined_constraints` before
it reaches the terminal body. Separate inherited effective constraints from an
additional declared use restriction. First support this ordinary alias without
losing terminal list bounds or element identity. Arbitrary constructed subtype
narrowing/intersections remain outside that initial repair. An unrecognized
restriction must still fail; removing the check wholesale is incorrect.

### Parameterized CHOICE tags

All five render-blocked roots extract successfully. Owned graph inspection
identifies ten distinct `SetupRelease<T>` instances whose `release`/`setup`
root tags and PER indexes are unavailable. The template's generic source body
has no fixed tags. Parameterized extraction populates from `profile.generic`,
while the existing tag transfer reads only `member->tag`.

The 264 fixed specialization clones divide into 256 with context-specific
`[0]`/`[1]` member tags and eight with no member tags. Of the ten instances
actually missing owned tags, only these two have authenticated tagged clones:

- `SetupRelease { LocationMeasurementInfo }`
- `SetupRelease { SL-L2RemoteUE-Config-r17 }`

The other eight match untagged clones: SL-DRX-ConfigUC-r17, SL-MeasConfig-r16,
SL-CSI-RS-Config-r16, SL-LatencyBoundIUC-Report-r17, SL-PagingInfo-RemoteUE-r17,
SL-RequestedPosSIB-List-r18, SL-RequestedSIB-List-r17 and SL-SFN-DFN-Offset-r18.
Their clone `auto_tags_OK` is zero. This establishes missing final tag evidence;
it does not by itself prove a particular fixer scheduling bug.

Split transfer of existing fixed evidence from resolution of currently missing
evidence. Match complete module/name actual identities and template identity,
not clone order or type spelling. A specialization actual's `ref_expr` cache can
be empty; the read-only study resolves its reference in its lexical module and
imports instead. Current IR extraction's separate cache consistency checks must
not be weakened by this study's source-level resolution.

Preserve canonical tag-based root ordering, explicit tags, module tagging mode,
root/addition distinction and duplicate/unresolved rejection. Blindly assigning
release/setup indexes 0/1 to every template is not an acceptable general fix.
The eight untagged cases require a separately frozen generic-instance tagging
contract or correctly finalized fixed specialization evidence.

## Latent blockers and Tier-A priority

The fixed AST contains 2,630 named type identities. Static reference/actual
traversal resolves all inspected references and identifies 309 source shape
inspection leads across singleton INTEGER, constrained primitive list elements
and the selected CONTAINING contexts. These are **candidates**, not 309 confirmed
failures. Traversal is conservative, includes optional/addition paths and does
not instantiate a typed closure. It is neither a wire campaign nor an exhaustive
latent blocker inventory. Constraint-contained payload references are not
traversed. Paths in the report locate source dependencies, not executable wire
paths or proof that every potential site will fail.

Targeted probes establish additional dependency blockers without changing code:
RadioBearerConfig fails at SRB-Identity-v1700, MeasConfig extracts but fails tags,
and CellGroupConfig fails at a constrained primitive list inside
PhysicalCellGroupConfig's extension group. Thus fixing SRB-Identity-v1700 alone
cannot justify claiming that RRCSetup or RRCReconfiguration is delivered.

First Tier-A priorities are SIB1/BCCH-DL-SCH, RRCSetup/DL-CCCH,
RRCReconfiguration/Complete and RRCRelease, followed by complete DL/UL-DCCH
carriers. The existing supported RRCSetupRequest, RRCSetupComplete and security
mode messages remain useful, but do not close these missing procedures.
Only new measurements after each repair may change supported-root counts.

## Proposed subsequent batches

These are implementation priorities, not advance acceptance or an instruction
to implement all phases in one task. Reclassify after each batch; split further
if a contract requires substantial new ownership or specialization work.

| Batch | One primary objective | Acceptance boundary |
| --- | --- | --- |
| RRC-P11 | Closed singleton INTEGER normalization | Shared owned domain, focused negative/zero-bit tests, 85-root readiness refresh; no predicted unlock count |
| RRC-P12-A | Freeze constrained primitive collection element ownership contract if required | Smallest representable IR/model mapping; no codec implementation |
| RRC-P12-B | Implement that collection element contract | INTEGER/BIT STRING bounds retained, ownership/rollback and focused codec checks; fresh readiness |
| RRC-P13-A | Transfer existing fixed parameterized CHOICE tag evidence | Exact actual identity matching and tag/index validity; cover the two evidenced instances |
| RRC-P13-B | Resolve missing tag evidence for the other instance contexts | Separate tagging contract and generic/explicit/nested cases; no magic branch indexes |
| RRC-P14 | Extend exact opaque CONTAINING recognition to named/CHOICE contexts | Owned bytes, no nested decode, unsupported combined/mixed forms rejected |
| RRC-P15 | Preserve ordinary constructed alias inherited semantics | Alias and direct target agree; bounds retained, extra unsupported restriction rejected |
| RRC-P16 | Qualify newly unblocked real wire profile | Independent native bytes/bits and full semantics; accepted old corpus preserved |
| RRC-P17 | Deliver expanded sealed C++ SDK | Updated finite manifest/identity, authenticated headers/archive and isolated installed consumer |
| RRC-P18 | Deliver expanded Python SDK | Bind new public models, wheel/sdist, full owned semantic checks and four-protocol coexistence |

P12-A can be skipped only if review confirms the existing owned model is already
sufficient and records that decision. P13-B may need another study/contract batch
before implementation. P16–P18 start only once their actual codec profile is
measured; keep remaining blocked identities explicit. New exposed optional NULL
shapes would require separately qualified Python presence semantics, since P9
correctly rejects them. Do not reopen frozen schema or modify historical P7
qualification in place. Core semantic changes must preserve all three APER
protocols with relevant regression and historical generated-artifact checks;
SDK rebuilding and packaging belong to their own later delivery batches.

## P10 closeout and reproduction

`tools/rrc-blockers/study.py` builds four probes with strict C warnings, authenticates
six source modules and every current P9 SDK producer input, runs the entire
85-root extract/render matrix, examines owned graphs after tree destruction,
inspects fixed constraints/specializations and runs 15 focused named seeds.
It hashes raw evidence, input source/archive files and binaries, then rechecks
input hashes to reject drift during the study. All 64 accepted generated headers
remain byte-identical; blocked roots leak no generated header.

No production source or package source changes, no new wire/correctness coverage,
no SDK rebuild, no source-tree fallback, no performance benchmark and no merge.
P10's new evidence and plan are distributed via Makefile.am entries. Independent
approval binds the exact candidate plus a separate review receipt before push.
