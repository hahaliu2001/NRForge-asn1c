# RRC-P6 — ordinary UPER compounds and channel framing

Baseline: `7e63ac6123f621dd3287db22b93de338117be3d5` (accepted P5).
Branch: `feature/f1ap-cpp-aper`. No merge or performance benchmark.

P6 adds a dedicated owned-graph C++20 renderer and nested transactional UPER
operations. It accounts for every selected identity; it does not establish full
RRC wire coverage or an installed SDK. P7 owns independent finite wire
qualification; P8 owns installation and sealing.

## Representation and encoding contract

`asn1typed_render_cpp_owned_uper` borrows a complete module-qualified ordinary
root graph, with the root at `types[0]`. It does not mutate that graph or access
parser/fixer state. The probe destroys the tree before generation. The caller
owns successful header text and releases it with `free`; errors leave a fresh
output slot NULL and a diagnostic. Whole-graph storage, type bodies, constraints
and mapping evidence are checked before resolving forward references or
DEFAULT targets. Missing/stale evidence, recursive value graphs, name collisions,
IOC, unsupported types or constraints fail before publishing output.

The header includes `uper/runtime.hpp`, uses a caller-selected safe namespace,
and exposes `Root`, `encode`, `decode`, owned `Tn` types and `Aj` CHOICE wrappers.
Stable ordinal identifiers avoid guessing private specialization names. Source
identity/field/alternative strings and enum numeric/index metadata remain
available. Field names use the existing shared naming policy with `f_` prefix.
Validated extension groups receive a derived group field name and retain their
original source identity. Complete headers compile under strict C++20 flags.

The inherited authority is ITU-T X.691 (02/2021), as pinned by P5. This renderer
composes the finite P5 primitive profile with the following rules:

| Construct | P6 behavior |
|---|---|
| SEQUENCE / ordinary channel | Root extension bit, then root optional/DEFAULT bitmap, then root payload in validated component order; a channel encodes its ordinary `message` field without an IOC envelope |
| DEFAULT | Owned scalar/enum default initialization; equal defaults are suppressed, absent defaults restored; unsupported defaults remain blocked |
| Extension additions | One bitmap slot per validated addition/group, followed by a complete child open encoding; group members keep their own presence/default logic |
| CHOICE | Root indexes use validated canonical PER mapping; known additions use extension index and complete child open encoding; monostate is invalid |
| Future SEQUENCE addition | Validate framing and discard unknown payload; decoded known value intentionally omits it |
| Future CHOICE addition | Preserve owned `Unknown{index,payload}`; reject an opaque value whose index names a known addition |
| SEQUENCE OF | Constrained root counts or segmented lengths, including 16K fragmentation and required final determinant; validate total count and source SIZE |
| Ordinary specialization | Follow owned module-qualified dependencies and actuals; no new SetupRelease mapping inference |

Finite signed/unsigned integers, BIT/OCTET STRING, BOOLEAN, NULL and ENUMERATED
use the dedicated `nrforge::uper` runtime. Character strings, REAL, OBJECT
IDENTIFIER, semi-constrained/disjoint ranges and other shapes outside P5 remain
blocked. Generated codec completion rejects trailing octets, malformed child
padding and invalid range/size/index codes.

## Nested transactions and budgets

`compound` restores the entire operation's cursor, quota counters, depth and
writer prefix after any typed failure, including a nested failure ignored by a
callback. Sticky first errors block publication. Known-open child errors map to
the outer frame's starting bit; its complete encoding is validated before
merging usage. All child streams inherit parent quotas and absolute depth.

Callbacks borrow their stream and must not move or finish it. Generated callbacks
respect that condition. Allocation and length exceptions become typed errors;
other callback exceptions restore state and are rethrown. Returning failure is
preferred. A caller must not invalidate borrowed input or pass writer staging
aliases; bitmap/octet/open writes reject aliases before prefix mutation.

In addition to P5 limits, default caps are depth 64, cumulative work 16 Mi bits
and concurrent child open staging 1 MiB. Work includes primitive accesses,
discarded open payload and completion padding. Child open bytes are charged both
for their inner work and outer framing. Concurrent staged child bytes are
charged before growth and released after successful framing; nested peaks share
one cap. Owned decoded collection storage is charged as `count*sizeof(Element)`
before reserve, and nested owned payload is charged separately. These counters
are deterministic safety budgets, not a performance measurement.

## Matrix and retained gaps

`tools/rrc-uper/readiness-rrc-p6.json` records authenticated frozen inputs, all
80 selected roots, five extra DEFAULT seeds and strict compilation hashes.
Role counts overlap because some bare seeds are channel payloads.

| Scope | Strict generation PASS | Extraction blocked | Render blocked | Total |
|---|---:|---:|---:|---:|
| Selected roots | 59 | 16 | 5 | 80 |
| Channel roots | 7 | 4 | 1 | 12 |
| Distinct channel payload identities | 50 | 12 | 4 | 66 |
| Bare seeds | 3 | 1 | 0 | 4 |
| Additional DEFAULT seeds | 5 | 0 | 0 | 5 |

All 64 generated headers pass strict C++20. The five render-blocked roots are
LocationMeasurementIndication, RRCReestablishment, RRCReconfigurationSidelink,
RemoteUEInformationSidelink-r17 and SCCH-Message: their closure contains ordinary
CHOICE root tags without available mapping evidence. The 16 extraction gaps
remain the accepted P4 constraints/alias gaps. No unsupported closure is lowered
by erasing constraints or guessing tags.

MIB (23 bits), SIB2 (48 bits) and MeasurementTimingConfiguration (5 bits) decode
and reencode the inherited frozen bare fixture bytes exactly. SIB1 remains
extraction-blocked. These are smoke checks, not P7 independent qualification.
All six frozen modules are parsed/fixed; NR-UE-Variables,
NR-Sidelink-Preconf and NR-Sidelink-DiscoveryMessage are scoped as dependencies,
without inventing standalone channel roots.

Focused tests cover DEFAULT suppression/restoration, specialization, groups,
known/future extensions, canonical mapping metadata, 65536-element fragmentation,
all truncation points of a 110-bit compound, late rollback, budget exhaustion,
child error anchors, alias rejection and ignored nested failures. Exhaustive
allocation injection covers 1068 renderer positions, 7/5 generated encode/decode
positions and 5/2 nested-open runtime positions. ASan/UBSan covers the compound
runtime and generated codecs; container leak detection is unavailable.

The verification file binds exact source/report hashes and regression results.
F1AP/NGAP/E1AP delivery files and existing APER/extractor/parser implementation
remain unchanged relative to the baseline. Independent review must pass before
commit/push. The reviewed snapshot and receipt are recorded alongside evidence.

## Exploratory oracle discrepancy retained for P7

`oracle-observation.json` records a separate unqualified experiment. For the
synthetic AUTOMATIC TAGS `Pick` with `first [2] Counter` and `nothing [0] NULL`,
the owned P4 canonical mapping selects `nothing` at root index 0. Generated
`Root.pick/nothing` is `40` (five semantic bits). Both asn1tools 0.167.0 and
pycrate 0.7.11 emit `48`, compatible with declaration index 1. The other four
finite compound byte examples match. pycrate required explicitly expanding the
single `Box { Counter }` specialization because its compiler asserts on the
original type formal; its oracle implementation was not patched.

This experiment is excluded from independent qualification PASS. X.691 10.2
and 23.2 require canonical root ordering; P7 must audit X.680 automatic/explicit
tag rules and reference implementations to resolve this discrepancy before
qualifying that case. P6 retains validated owned mapping and does not silently
switch to declaration order. Reproduce with `observe_oracles.py` and the pinned
versions. The official X.691 source is linked in the P5 contract.
