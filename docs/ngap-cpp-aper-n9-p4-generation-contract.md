# N9-P4 — Physical IOC C++ APER generation contract

2026-10-09. Independently reviewed design **PASS** after the N9-P2/P3 API
contracts were frozen. Continuous execution authority applies. This is bounded
IOC entry/container generation, not complete NGAP-PDU qualification.

## Public boundary and compatibility

Add `asn1typed_render_cpp_owned_ioc_types`, `_mapping`, and `_codec` with the
existing `(module, namespace, out, diagnostic, diagnostic_size)` signature.
Generate all three from unchanged, independently owned physical IR and the same
namespace. Include runtime, types, mapping, codec in that order. Each entry
preflights the complete graph independently; failure leaves `*out == NULL` and
does not mutate the input. Parser/Fixer trees may already have been deleted.
Legacy flattened extraction is not physical evidence and is rejected. Existing
rendering entry points, output bytes, acceptance and rejection remain unchanged.

The supported ordinary payload graph is the N6/N7/N8 domain: resolved bounded
INTEGERs, BOOLEANs, complete named ENUMERATED evidence, root CHOICE, mandatory
and OPTIONAL fixed SEQUENCEs, root-only extensible SEQUENCEs and finite bounded
SEQUENCE OF. Unsupported declarations anywhere in the supplied graph reject;
the renderer never drops unsupported or unreferenced declarations. CONDITIONAL
registry row presence is policy metadata, not an OPTIONAL wire component.

## Structural plan and naming

The new `asn1typed_render_cpp_ioc.c` validates every P2 registry and bound
selector-role proof before building a temporary lowering plan. References are
resolved by full ordinary module/source identity or full parameterized identity
including ordered actual kind/module/source keys. Registry indexes are scalar
associations, never guessed from selected-field spelling. No procedure, object
set, class, type, row count or selector value is hardcoded.

Every ordinary declaration and every materialized bound body is a node.
Dependencies include all fields, CHOICE alternatives, collection elements and
known dispatch payloads. A dependency-first topological plan chooses the lowest
original node index among ready nodes; missing references, identity duplicates,
cycles and unsupported bound bodies fail closed. A bound collection remains an
identity-preserving owned vector, not a flattened logical IE list.
Every supplied registry must have a proven physical field binding; an orphan
registry is rejected rather than silently omitted from generated mapping.

Generated source identities concatenate ordinary module/source components and,
for bound bodies, the complete template and actual identity components. They
pass through the shared final-name function exactly once per generated symbol.
Ambiguous concatenations or normalized collisions reject rather than silently
suffixing or merging identities. Preflight covers types, mapping names,
constraints, complete wrappers, field helpers, known-row wrappers and unknown
wrappers in their actual C++ scopes. Row wrappers use symbolic identity when
present, otherwise the numeric ID; repeated payload types remain distinct.
Original physical component names are preserved through shared field naming.
The runtime namespace `nrforge::aper` and descendants remain reserved. All
runtime and generated references are explicitly globally qualified, including
when the consumer namespace contains a `nrforge` segment outside the reserved
runtime namespace. Existing namespace validation continues to reject `std`
segments; it is not relaxed by this mode.

The existing compound implementation is reused through an explicitly internal
IOC-mode hook. Its legacy modes do not enter lowering or IOC emission. Lowering
borrows unchanged primitive/enum evidence and owns its temporary identity/member
arrays; it never publishes the synthetic plan as ASN.1 semantic IR. Validated
IOC entries have custom types/mapping/codec emission, while ordinary nodes use
the existing N6/N7/N8 emitter. No source metadata is cleared to evade validation.

## Value and mapping model

Each field entry owns its original identifier component, received criticality
component, and selected-value component. The value is a variant of one wrapper
per registry row plus a schema-specific unknown wrapper containing owned
`vector<byte>` payload. Unknown-first storage makes explicitly empty extensible
registries valid; variant index is neither selector ID nor row identity.
Collections preserve entry order and duplicates. Mapping publishes numeric row
IDs, expected criticality, presence and presence-availability separately from
the received entry data. Missing mandatory entries, duplicate IDs and received
criticality mismatch do not fabricate values and are not codec failures.

Known encoding requires ID to match the selected wrapper's authoritative row
ID. Criticality is encoded as received, independently of expected registry
criticality. Unknown payload encoding, valueless variants and ID/wrapper
mismatch record a sticky failure; opaque retention never grants re-encoding.
Unknown decoding is allowed only for a validated extensible registry. A known
ID always invokes its typed child decoder; malformed known payload cannot fall
back to unknown. Unknown-only empty extensible registries use the same framing.

## Runtime integration

Known selected values use only the N9-P3 scoped known-open callback boundary:
the child's field reader/writer shares parent sticky errors and cumulative
budgets, validates local complete encoding, and does not finish the parent.
Generated read calls are `f.read_known_open_type<Payload>(lambda)` where the
synchronous lambda takes `FieldReader&` and returns `Result<Payload>`. Generated
write calls are `f.write_known_open_type(lambda)` where the synchronous lambda
takes `FieldWriter&` and returns `Result<void>`. Callbacks invoke field helpers,
never `decode_complete` or `encode_complete`, and never retain child views.
Generated aggregates must be nothrow move constructible, as P3 requires.
Unknown values use existing `read_open_type_owned()`. Unknown encoding and
ID/wrapper mismatch call the P3 symmetric `FieldWriter::record_failure(Error)`
with `constraint_violation` at the current local cursor; P3 preserves sticky
first-error semantics and anchors staged encode diagnostics to the enclosing
physical known-field start. No unrelated wire primitive is used for refusal.

The decoder stages a complete entry/value locally and publishes only on
success. Allocation and length exceptions become sticky runtime failures.
Collection cardinality and extension retention remain governed by existing
N8/N7 shared budgets; typed open scratch/depth/wire rules belong to P3.

## Acceptance

Persistent tests cover renamed/reordered/cross-module graphs, full actual-key
resolution, generated spelling collisions, Parser destruction, stale registry
or binding edits, unsupported nodes, missing dependencies and cycles. Generated
C++20 compilation covers namespace shadowing and duplicate payload types.
Full-byte/value comparisons use native IOC-aware synthetic reference vectors,
including known BOOLEAN/integer, multiple rows, ordered duplicates, missing
mandatory rows and received criticality mismatch. Negative runtime cases cover
malformed known complete payloads, unknown retention and sticky encode refusal,
empty extensible sets, ID/wrapper mismatch, shared limits and atomic failure.
All legacy ordinary renderer regressions remain in scope. Untouched target
generation is readiness evidence only; complete message/envelope integration
and independent target qualification remain later milestones.

## Execution status

The contract is design-approved; implementation and qualification evidence are
recorded separately when their dependency and implementation gates close. No
runtime test, independent native comparison or target qualification is implied
by the design PASS.
