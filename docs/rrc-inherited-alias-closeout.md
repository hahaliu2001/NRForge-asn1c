# RRC-P15: ordinary constructed alias inheritance

Baseline: `fc2877dd5c52b52c3f8dbe7134ae50fc3192a28a` (RRC-P14).
Branch: `feature/f1ap-cpp-aper`. Primary objective: preserve terminal collection
semantics for ordinary aliases without supporting new constructed restrictions.
See `tools/rrc-inherited-alias/contract.md` for the independently reviewed contract.

## Cause and repair

MobilityHistoryReport-r16 ::= VisitedCellInfoList-r16 has no declared constraint.
VisitedCellInfoList-r16 owns SIZE(1..16); the fixer copies effective SIZE to the
alias. The ordinary alias loop rejected any combined constraint, so the inherited
bounds blocked UEInformationResponse-r16 and UL-DCCH-Message after P14.

Only `libasn1typed/asn1typed_extract.c` changes production behavior. For ordinary
nonparameterized constructed aliases, every additional declared constraint is
still rejected. A terminal SEQUENCE OF can transfer inherited effective SIZE
only when every alias layer agrees with its clean, supported terminal SIZE.
The terminal declared/effective constraints must also agree. Equality explicitly
compares presence, lower/upper, extensibility, known extension-addition presence
and bounds; it does not use padding-dependent memcmp or the parser's node-kind
constraint comparator. Missing inherited bounds, narrowing/widening, mixed or
malformed trees and hidden intermediate restrictions remain rejected. Unbounded
terminal and unconstrained aliases remain valid; other constructed kinds keep
the existing constraint rejection.

Owned alias declaration identity/location is preserved. Named element module
and source identity remain those of the terminal; anonymous elements retain the
existing authenticated alias-context identity and terminal member semantics and
locations. Namespace/cache preflight, parameterized-use guards and graph budgets
are unchanged. Extraction is read-only. There is no new IR, renderer, runtime,
fixer, schema or SDK implementation.

## Focused and real evidence

Ten generated roots cover direct, simple/multihop/imported aliases, bounded/open
extensible/unbounded lists, named and inline constructed elements. A separate
compound-names fixture verifies same-spelling types in different lexical modules.
Twenty-two hand-derived golden encodings in three completion modes prove matching
direct/alias bits and values; list/value bounds, reserved length patterns,
truncation and encode/decode quotas fail correctly. Native checks cover malformed
constraints, each alias layer, terminal declared/effective mismatch, hidden new
restrictions, reference-cache mismatch, failed rollback and AST-deleted ownership.
Known explicit SIZE-addition bounds remain owned and unequal additions reject;
their existing renderer rejection is preserved, with no codec expansion claim.

The current extractor and focused test pass direct ASan/UBSan. LSan is NOT_RUN
due to environment task enumeration limits. All 70 native outcomes pass:
51 typed, two fixer, three UPER, ten APER, three NGAP and one parser.
An independent real-schema check owns the direct terminal, named alias and both
new root closures after AST destruction, retaining SIZE1..16 and
NR-RRC-Definitions.VisitedCellInfo-r16 identity and source locations.

## Readiness and delivery boundary

The fresh frozen 85-root matrix has 85 text-rendering PASS, with no extraction or
text-rendering blocks. All 83 P14 physical headers remain byte-identical.

| Root | Text generation | Strict C++20 |
| --- | --- | --- |
| UEInformationResponse-r16 | PASS | PASS |
| UL-DCCH-Message | PASS | BLOCKED_EXISTING_RENDERER |

UL-DCCH-Message includes LocationMeasurementIndication and inherits the same
existing two unused-io NULL-extension renderer diagnostics. The standalone
LocationMeasurementIndication header and its previous compilation block remain
unchanged. Thus 85 generated roots does not mean 85 strictly compilable or
wire-qualified roots. Fix this renderer issue separately before qualifying the
complete expanded channel profile; P15 does not mask it with relaxed warnings.

The shared APER regression verifies 361 messages and 1,773 historical physical
headers: F1AP 158/948, NGAP 131/393 and E1AP 72/432, all byte-identical. Current
sources, producer archives, binaries and logs are bound by drift-checked receipts,
followed by independent exact-snapshot approval before commit/push.

Historical receipts remain immutable and SDK delivery remains 64 roots. Expanded
real-message wire qualification and SDK delivery remain P16-P18 after applicable
compile blockers are resolved. No benchmark or merge.
