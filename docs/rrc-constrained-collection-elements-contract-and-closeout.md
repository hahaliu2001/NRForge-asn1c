# RRC-P12 constrained primitive collection elements

Baseline: `82ff13ff3172640887c6c847a258e1b8a2a07d13`.
Branch: `feature/f1ap-cpp-aper`. Frozen schemas and historical P1–P11
receipts remain immutable.

## Owned model decision

The existing model can represent the required element semantics without new
variable-length ownership or public IR fields. A `SEQUENCE OF` owns its list
`size_constraint` and an `element_type` reference. That reference can point to
an ordinary owned primitive type carrying its own `value_range` or
`size_constraint`. Existing `$inline$...$@element` identities and parser-side
path lookup already distinguish list elements from fields and extension groups.
UPER and APER renderers resolve named element references to owned types.

Independent reviewer `/root/e1_sdk_review` confirmed this mapping before
production edits and approved skipping a separate P12-A IR implementation batch.
P12-B uses that existing mapping for **direct constrained INTEGER and BIT STRING
elements**. List length bounds and element value/bit-length bounds remain
independent. Only direct primitive expressions are promoted; constrained named
use references, named bits, parameterized actuals and unsupported mixed
constraints retain their existing rejection paths. The established primitive
constraint parsers decide which domains are representable, including closed
singleton, integer union, extensible range and supported SIZE forms. Successful
extraction does not expand a renderer's supported profile.

The anonymous element receives an owned module-qualified path and normal
primitive type body. A borrowed AST view supplies its diagnostic identity if
needed; no parser/fixer expression or constraint is changed. Dependency closure
copies references before array reallocations and remains subject to existing
root-graph type/reference/AST quotas. Failure clears the pending graph. Existing
unconstrained primitive elements and named elements preserve their previous
representation and generated output.

## Acceptance boundary

Focused extraction checks must prove independent list/element bounds, module
and path identity, named and anonymous lists, nested/extension-group closure,
source preservation, ownership after tree deletion, failed extraction cleanup
and quotas. Strict generated codec checks cover INTEGER values, fixed and
bounded BIT STRING sizes, singleton zero-bit elements, list length and element
determinants, invalid values/sizes, truncation and runtime quotas.

An authenticated fresh 85-root rescan must compare all 65 P11-supported headers,
report newly exposed blockers and syntax-compile any newly rendered roots.
Shared APER regression must preserve all 361 messages and historically recorded
headers. Existing native regression suites remain required. Real-message wire
qualification and expanded C++/Python SDK packaging are deferred to P16–P18;
delivered SDK scope remains 64 roots. No benchmark, merge or schema change.

## Implementation and observed readiness

The production change is limited to the shared extractor: select a direct
constrained INTEGER/BIT STRING element for an anonymous named reference and
close it through the existing primitive body extractor. Legacy anonymous
dependency diagnostics use the owned identity on a read-only borrowed view.
That identity is read from the newly added type, since adding a type may
reallocate the array that held the incoming reference. No IR, renderer, runtime
or schema interface changes are required. The earlier closed-singleton-list
negative fixture now uses an extensible singleton, which remains unsupported.

The new native test covers eleven successful element closures and six rejected
forms. It checks list/element bounds independently, distinct inline fields,
CHOICE, nested constructed elements, extension groups, module-qualified
anonymous identities, signed/unsigned and union domains, source preservation
and ownership after tree deletion. All 37 allocation-failure positions in the
focused nested graph roll back to a cleared graph; type/reference/AST quotas
also reject atomically. Legacy module extraction is covered as well as ordinary
root-graph extraction.

Ten synthetic roots render and pass strict C++20 generated-code tests under all
three completion modes. Hand-calculated bytes/bits verify independent list
length and INTEGER/bit-length determinants, singleton zero-bit elements, fixed
32-bit elements and exact UINT64 bounds. Invalid values, sizes, unused indexes,
truncation, aggregate element quotas and allocation quotas reject. The eleventh
root owns an INTEGER union successfully but remains blocked by the existing
UPER renderer's union profile; this batch does not expand that profile.

Nested collection coverage uses a constructed element wrapper containing a
list. The existing parser asserts on directly nested SEQUENCE OF constraints;
P12 neither changes that grammar nor claims that direct nesting is repaired.
The changed extractor and element test sources pass ASAN/UBSAN checks. LSan is
explicitly disabled and recorded as not run because this environment cannot
enumerate `/proc` tasks; no leak-scan PASS is claimed.

The authenticated fresh 85-root matrix is **67 render PASS, eight extraction
blocked and ten render blocked**. All 65 previously supported P11 headers remain
byte-identical. Seven first-failure transitions are observed:

| Root | P12 result | Remaining boundary |
| --- | --- | --- |
| `SidelinkUEInformationNR-r16` | Render and strict C++20 compilation PASS | New real-message wire qualification deferred |
| `UEAssistanceInformation` | Render and strict C++20 compilation PASS | New real-message wire qualification deferred |
| `BCCH-DL-SCH-Message` | Extract PASS, render blocked | Ordinary CHOICE root tags unavailable |
| `DL-CCCH-Message` | Extract PASS, render blocked | Same tag evidence boundary |
| `LoggedMeasurementConfiguration-r16` | Extract PASS, render blocked | Same tag evidence boundary |
| `RRCSetup` | Extract PASS, render blocked | Same tag evidence boundary |
| `SIB1` | Extract PASS, render blocked | Same tag evidence boundary |

The higher render-blocked count reflects progress past extraction failures,
not regression of supported roots. New wire qualification and expanded sealed
SDK delivery remain deferred. The delivered C++/Python SDK stays at **64 roots**.

Native suites pass 47 libasn1typed, three libuper, ten libaper, three libngap and
one libasn1parser tests: **64 total**. Shared APER regression covers all 361
messages and 1,773 historically recorded BODY/available envelope headers;
NGAP's historical comparison remains BODY-only. Detailed current producer,
generated-header, executable and log hashes are recorded in
`tools/rrc-collection-elements/readiness.json` and `verification.json`.
Independent final acceptance is recorded separately in `review.md` before push.

The next planned capability batch is RRC-P13-A, transfer of authenticated fixed
parameterized CHOICE tag evidence. P13-B addresses missing tag evidence through
its own contract. P12 does not special-case RRCSetup, drop constraints, rebuild
SDKs, run a benchmark or merge the branch.
