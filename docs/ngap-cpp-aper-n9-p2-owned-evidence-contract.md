# N9-P2 — Owned physical IOC dispatch evidence

2026-10-09. Accepted after independent design review **PASS**, with no blocking findings. Execution is authorized under the continuous complete UEContextReleaseCommand objective. The N9 IOC/open-type readiness study is authoritative. This milestone changes ownership/extraction evidence only, not runtime, Naming, generated code or protocol policy.

## Scope and compatibility

Add `asn1typed_extract_physical_message(tree,module,message,out,diag,size)` for a message with one ordinary physical root field referencing a parameterized IOC container, followed optionally by one trailing SEQUENCE extension marker. Preserve the root field and its reference, declaration order and N7 physical extension evidence. Expand reachable ordinary declarations and specialized container/field bodies, including the selected main container, rather than flattening object-set rows into SEQUENCE fields.

The existing `asn1typed_extract_message` remains the flattened compatibility API. Its output, acceptance and errors must remain unchanged. Existing ordinary module extraction and bound-body ownership contracts remain unchanged except for zero-initialized additive metadata. New evidence is unavailable by default; names and zero counts never imply valid dispatch.

Initial IOC domain is one actual object-set reference per bound identity and one selector on a three-component non-extensible physical SEQUENCE: fixed UNIQUE identifier, fixed criticality selected by that identifier, and selected open-type value selected by the same identifier. Supported class setting shapes are `id/criticality/Value/presence` and `id/criticality/Extension` with or without a `presence` role. The untouched frozen NGAP-Containers module declares `NGAP-PROTOCOL-EXTENSION` with all four roles, including `&presence`; the three-role synthetic form remains accepted. The fixed class shape explicitly determines presence availability, rather than inferring it from rows. This is structural shape recognition, not a generator rule keyed by class/type names. Container bodies are non-extensible bounded SEQUENCE OF with one parameterized element and resolved finite contiguous SIZE. Empty extensible object sets are valid unknown-only sets only when the fixed tree explicitly supplies the empty IOC table. No procedure envelope/all-procedure dispatch, multi-selector, IE pairs, DEFAULT, extension groups, parameter actual categories beyond object sets, ambiguous cells or partial row acceptance.

## Owned module registry

Append a module-owned `ioc_registries` array with count/capacity. Each `asn1typed_ioc_registry_t` owns:

- class identity (`class_module`, `class_source_name`), object-set identity (`object_set_module`, `object_set_source_name`) and selected class-field spelling (`selected_class_field_source_name`);
- ordered `rows` storage/count/capacity; each `asn1typed_ioc_dispatch_row_t` owns optional raw symbolic ID, `intmax_t numeric_id`, explicit `has_numeric_id`, an independently owned payload `asn1typed_type_ref_t`, criticality, `has_presence` and presence;
- evidence status, declared row count, object-set extensibility and finalized-valid flag.

The key is the full class + object-set identities; selected-field spelling is checked consistently rather than creating a second conflicting table under the same key. Source row order is retained, but numeric ID, never row ordinal, is the dispatch key. This initial identifier domain is 0..65535. All payload references are ordinary named identities or supported primitive references with no actuals; exact payload-codec support is a later generator decision. CONDITIONAL presence may be owned as policy metadata and never becomes a physical OPTIONAL bit. Value-class rows require `has_presence=1`. Extension-class rows preserve the class-declared presence availability as 0 or 1, consistently across every row; absent metadata is not silently changed to mandatory. An explicitly empty table supplies no fabricated row presence semantics.

Public ownership operations:

```c
int asn1typed_module_add_ioc_registry(module*,
    const char *class_module, const char *class_name,
    const char *set_module, const char *set_name,
    const char *selected_class_field, size_t *index_out);
int asn1typed_ioc_registry_add_row(registry*, const dispatch_row*);
int asn1typed_ioc_registry_set_evidence(registry*, size_t declared_rows,
    int object_set_is_extensible);
int asn1typed_ioc_registry_set_unavailable(registry*);
int asn1typed_ioc_registry_set_unsupported(registry*);
asn1typed_wire_finalize_result_e asn1typed_ioc_registry_finalize(registry*, char*, size_t);
int asn1typed_ioc_registry_validate(const registry*, char*, size_t);
```

Adding a key deduplicates an exactly matching identity/selected-field association; mismatch rejects without mutation. Add-row deep copies strings/reference before publication; allocation failure leaves prior rows, evidence and flags unchanged. Successful row addition invalidates evidence and declaration count as well as the valid flag. `set_evidence` requires valid storage, exact row count, a boolean extensibility flag, and an explicit call even for zero rows. Empty non-extensible tables are rejected in this initial domain. Finalize invalidates its flag then validates the complete table, identity, count/storage, numeric evidence, unique IDs, metadata and references; only success publishes valid. Missing/unsupported evidence returns UNAVAILABLE; malformed API/storage/internal failures return ERROR. Direct public edits require validate; finalized flags alone are never trusted. Validate checks the same facts without mutation.

## Explicit physical selector-role proof

Append `asn1typed_ioc_binding_t ioc_binding` to each bound instance. It stores scalar evidence status, registry index, identifier/criticality/value field ordinals and finalized-valid flag. The selected object-set actual is fixed to index zero in this domain. No borrowed pointers into module arrays are retained.

```c
int asn1typed_bound_instance_set_ioc_binding(module*, size_t instance_index,
    size_t registry_index, size_t id_ordinal, size_t criticality_ordinal,
    size_t value_ordinal);
asn1typed_wire_finalize_result_e asn1typed_bound_instance_ioc_binding_finalize(
    module*, size_t instance_index, char*, size_t);
int asn1typed_bound_instance_ioc_binding_validate(
    const module*, size_t instance_index, char*, size_t);
```

Setter explicitly supplies physical source evidence and invalidates valid state; it never infers roles solely from a three-field body. Validate/finalize require a materialized, storage-consistent, non-extensible three-field SEQUENCE; ordinals must be exactly source order 0/1/2. All components are physically mandatory. ID is FIXED_TYPE with no selector relation; criticality is FIXED_TYPE with a relation to the registry class's criticality field; value is CLASS_FIELD_SELECTED_TYPE with the registry's selected field. Both relations select the identifier field's actual spelling, match class identity and address actual zero. The instance's single object-set actual matches the registry key, whose evidence revalidates. Identifier type must be a named INTEGER with exact non-extensible 0..65535 constraints, no tail intervals/SIZE/IOC/collection/other-category metadata and valid storage; criticality type must be a named non-extensible ENUMERATED with resolved reject/ignore/notify numbers 0/1/2, independently validated numeric/PER evidence. Matching resolves full module-qualified identities, not source spelling alone.

Container bindings keep their owned element references to these specialized field instances; they do not receive a fake field-selector proof themselves. Successful body transfer starts with unavailable binding; direct later public body/registry mutation requires binding revalidation. Binding setter misuse leaves state unchanged. Binding finalize returns ERROR without mutation when malformed module storage/index prevents safely locating the instance; once an instance is safely located its valid flag is cleared before any semantic/evidence failure. This milestone provides explicit proof only for non-extensible physical IE/Extension fields; unsupported body shapes remain unavailable/rejected.

## Opt-in extraction and closure

The new extractor builds a pending module, owns the ordinary physical message body, recursively expands every physical field/alternative/element reference through the existing dependency/materialization machinery, and separately registers every encountered selected object-set table. It validates the fixed class shape, source UNIQUE identifier, matching selector relations and all row settings before publishing evidence. Registry payload references enter dependency closure as well, so the physical message's known rows are retained without fabricating root fields.

This closure preserves cross-module identities and specialized actual keys and must not retain pointers across reallocation. Main container SIZE and its ProtocolIE-Field specialization must appear in addition to single containers and extension containers already reachable from payloads. Each explicitly empty extensible table still owns its class/set association and resolved zero-row proof. Source table absence, class/set mismatch, duplicate numeric IDs, missing/unresolved numeric IDs, unrecognized criticality/presence, malformed or unsupported payload references, mismatched selectors and missing declared evidence reject the entire opt-in extraction with a nonempty diagnostic. On failure, clear the pending module and leave `out` zeroed. On success, transfer ownership once; deleting Parser/Fixer trees leaves all metadata valid.

No registry/binding is added by the old flattened API. Module clear frees registry strings/rows/payload references, bound identities/bodies and existing ordinary contents exactly once. Future copy operations must deep-copy this new storage explicitly; no shallow module-copy API is introduced here.

## Acceptance and deferred work

Persistent tests cover valid known and explicitly empty extensible sets, IE and Extension shapes, class/set-key deduplication, physical source order and main-container expansion, Parser destruction, stale/public edits, mutation invalidation, selector/key/type mismatches, numeric collisions, count/storage inconsistencies, deep copies and allocation-failure transactions. Old flattened extraction/output regressions remain unchanged. The untouched six-module frozen UEContextReleaseCommand graph must extract with a one-field physical root, main SIZE0..65535 container and selected field proof, while retaining the existing ID114/reject/mandatory and ID15/ignore/mandatory rows.

No wire primitive, child decoder, opaque encoder, generated type/codec, protocol presence enforcement, deduplication policy or PDU envelope is implemented in P2. Runtime open-type integration and IOC generation remain separately reviewed N9 milestones; ordinary type/collection/extension qualification is not whole NGAP qualification.

Allocation-failure tests inject only allocations in the owned IR and extractor translation units. Existing parser/fixer/common resolver allocations are not injected: the legacy void namespace API has unchecked allocation paths outside this milestone. This is a scoped ownership guarantee, not full-stack allocation-failure qualification.
