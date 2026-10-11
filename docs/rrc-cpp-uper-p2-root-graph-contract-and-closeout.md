# RRC-P2 — ordinary multi-module root graph extraction

Status: **Implementation complete (2026-10-11)**; independent acceptance is
recorded in `tools/rrc-readiness/review-rrc-p2.md`.
Baseline: `21eac8aaba515dce8e03aecb78de3799035dff3c` (accepted RRC-P1).
Branch: `feature/f1ap-cpp-aper`. No schema/fixture edits, merge, UPER runtime,
renderer expansion, SDK rebuild or performance benchmark.

## Public contract

`asn1typed_extract_root_graph(tree, module, root, limits, out, error, size)`
is a new opt-in entry point in `asn1typed_extract.h`. It consumes an already
fixed tree and publishes a fully owned `asn1typed_module_t`. The caller supplies
a fresh or cleared output, as with existing extraction APIs. On any failure the
output remains zero/clearable and no partial graph is published; diagnostics may
be omitted by passing a NULL/zero-sized error buffer.

* `types[0]` is the requested module-qualified root. A root may be a primitive,
  ENUMERATED, SEQUENCE, SEQUENCE OF, CHOICE or supported ordinary alias. Closure
  is breadth-first in source/member order; named and synthetic inline identities
  are appended once. Type-array pointers are reacquired after every append.
* Ordinary references resolve through local declarations, explicit named
  IMPORTS or a two-component module-qualified reference. The path deliberately
  ignores a fixer's cached terminal `ref_expr` when storing a use-site identity.
  It never chooses a same-named type by scanning unrelated modules. OID-renamed
  imports and longer/object-class references are outside this initial contract.
  The fixed-tree precondition includes the fixer's import/export validity checks.
  Preflight additionally compares every ordinary reference's cache with its
  explicit namespace target, including aliases, and rejects inconsistency.
* An alias retains its own module/name and materializes the supported terminal
  semantic shape. Imported `GraphB.Shared ::= Base` remains GraphB.Shared in
  owned use-site references, even when another module defines Base. Flattening
  its semantic body does not require a separate Base entry unless a remaining
  owned reference targets Base. Alias chains inherit the existing 128-hop bound.
  Constrained constructed aliases are rejected; supported inherited primitive
  constraints are transferred by the existing primitive population rules.
  Alias type location points to the alias declaration; inherited fields and
  enum items use the terminal body's source file.
* Anonymous constructed bodies retain collision-free `$inline$Owner$member`
  and `$inline$Owner$collection$@element` keys. Inline ENUMERATED bodies remain
  owned within their field/alternative, rather than inventing named types.
  Supported existing enum, integer, SIZE, CHOICE tag and root-only extension
  evidence is reused without claiming UPER wire readiness.
* Structural cycles (self and mutual references) are represented by finite
  owned references and deduplicated by identity. They do not cause recursive
  graph expansion. Success is not proof that a renderer can represent a cycle
  by value or that a codec can decode it within a runtime budget.
* Ordinary type formal/actual parameters are rejected before population. The
  new graph never enters IOC bound-instance or dispatch materialization. The
  diagnostic includes the owning type, source member and template reference;
  no actual identity or specialization body is falsely published. RRC-P3 owns
  that additional semantic work.
* DEFAULT, SEQUENCE known additions/groups, CHOICE known additions and other
  unsupported shapes remain rejected by the existing semantic helpers.
  OCTET STRING CONTAINING stays opaque according to the existing common helper:
  its contained type does not become a graph dependency. This initial IR does
  not copy the contained-type annotation or add a nested codec helper.

NULL limits use **4096 types / 65536 references / 262144 expression AST nodes**.
Explicit limits require all three members positive. Types count the root plus
named/inline bodies; references count primitive/named field, alternative and
collection-element references, including repeated identities, excluding inline
ENUMERATED payloads. AST nodes count semantic expression visits during preflight,
including repeated alias-body visits; constraint nodes are not counted.
An additional 128-level expression depth guard bounds preflight C recursion.
These limits bound graph traversal, not the prior parser/fixer, total source
module declarations, output byte allocation, or future runtime decoding.

The shared population helpers now take an explicit profile internally.
Existing whole-module, flattened/physical IOC and target-envelope callers keep
profile zero and their previous behavior. There is no mutable global extraction
mode. The ordinary reference path also avoids the legacy namespace constructor's
unchecked allocation failure; no namespace/parser/fixer implementation is changed.

## Frozen RRC matrix

Authority remains the RRC-P1 six-module pin, TS 38.331 V18.10.0:
NRForge-RAN `abc850e5461e1b15cf5c81907d0d0fdd5e3a1179`.
`record_p2.py` authenticates those exact bytes, takes all 12 channel roots,
66 distinct channel payloads and four bare seed identities from P1, and
deduplicates MIB/SIB1 (already payloads): **80 distinct selected roots**.
This is not all 2,630 named declarations. Each graph is fully traversed; no
optional field, selector branch or nonCriticalExtension chain is filtered by a
fixture value. The tree is destroyed before any successful graph is serialized.
The recorder validates root-first order, unique identities and every serialized
named reference's membership in the owned closure.

| Scope | PASS | FAIL |
|---|---:|---:|
| All selected roots, deduplicated | **44** | **36** |
| Channel roots | 5 | 7 |
| Channel payloads | 39 | 27 |
| Bare seeds (overlap MIB/SIB1 with payloads) | 1 | 3 |

Successful channels: **BCCH-BCH-Message (7 types), PCCH-Message (26),
UL-CCCH-Message (25), UL-CCCH1-Message (8), SBCCH-SL-BCH-Message (5)**.
Bare MIB succeeds with four owned types. All exact identities, references and
first diagnostics are in `tools/rrc-readiness/readiness-rrc-p2.json`.

| Observed first gap | Failed roots |
|---|---:|
| SEQUENCE fields after extension marker (including groups) | 24 |
| Ordinary type actual parameters (SetupRelease) | 8 |
| Inline constrained type | 2 |
| CHOICE known additions | 1 |
| Constrained constructed alias | 1 |

Preflight parameter checks precede shared population within each declaration;
these are first observed failures in that deterministic algorithm, not a census
of every unsupported field. DEFAULT remains a real semantic gap even though
none of these 80 roots reports it first. Subsequent fixes can reveal masked gaps.

**Correction to the P1 proposed MTC exit expectation:** the full
MeasurementTimingConfiguration closure reaches `NR-InterNodeDefinitions.MeasTiming`
and fails on its known SEQUENCE additions. The five-bit frozen MTC value omits
the optional path but does not remove it from the type graph. MTC is attempted
and its exact P4 blocker is retained, rather than declaring full graph support,
changing the schema or treating a finite value as the entire type. SIB1 likewise
stops at CellAccessRelatedInfo additions; SIB2 at its own additions.
The P2 ordinary graph mechanism and selected-root rerun are complete; the
original hope of an MTC success in this minimal phase is explicitly not met.
MTC's full closure is a future RRC-P4 regression gate. No partial MTC extraction
or codec is shipped.

Two newly exposed inline-constraint sites are SL-InterestedFreqList-r16 and
PropagationDelayDifference-r17. UEInformationResponse-r16 reaches constrained
MobilityHistoryReport-r16; SystemInformation reaches inline CHOICE additions.
Keep these identities in the shared-gap ledger; do not create per-message
patches. RRC-P3 is still next for ordinary type actuals and DEFAULT; ordinary
constrained aliases/use sites need a reviewed follow-up if not covered there.

## Validation and practical limits

* Focused Automake test covers imported/local name collision, imported alias and
  alias root, explicit module qualification, nested inline bodies, collection
  closure, self/mutual cycles, unreachable unsupported definitions, root-only
  extensions and opaque CONTAINING behavior. Assertions inspect owned values
  after the fixed tree is destroyed. Unsupported parameter/default/addition/
  constructed-alias cases, missing/invalid roots, stale reference cache and
  unimported foreign-name fallback fail without partial output.
* All three graph budget exhaustion cases and invalid zero limits are tested.
  Every **140 allocation positions** in the successful fixture's extraction
  path is failed in turn, with a diagnostic and clearable output each time.
* GCC strict focused/probe builds use `-std=gnu11 -Wall -Wextra -Werror`.
  Shared typed suite **43/43**, APER runtime **10/10**, protocol PDU suite **3/3**.
  The configured legacy extractor build retains its two pre-existing envelope
  const-cast warnings; no new warning waiver is introduced.
* ASan/UBSan instruments the changed extractor and common typed core, checks the
  focused success/failure/lifetime test and all 80 RRC probes. Parser/fixer and
  remaining linked libraries are the ordinary static build. LeakSanitizer cannot
  finish in this environment (`/proc/2/task` unreadable); successful sanitizer
  runs therefore use `detect_leaks=0`. This is address/undefined-behavior evidence,
  not a claim of a passing leak scan or fully instrumented toolchain.
* A fresh P1 probe/scanner run stays byte-identical to the committed P1 report.
  Full legacy APER extraction and generation is rerun for all **361 messages**
  (NGAP 131, F1AP 158, E1AP 72); **1773 historical header hashes** match exactly.
  NGAP has historical BODY hashes only; F1AP/E1AP also compare envelope headers.
  This is artifact compatibility, not a new all-message wire/SDK campaign.
* New RRC C++ generation, strict generated compile, UPER wire qualification and
  installed SDK checks are **NOT_RUN**. Graph success alone qualifies none of
  them. The four frozen fixtures and all existing protocol deliveries are kept.

`tools/rrc-readiness/verification-rrc-p2.json` records commands, hashes and results.
Independent review must pass before commit/push. No merge or benchmark.
