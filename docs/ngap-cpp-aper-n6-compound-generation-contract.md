# N6 — Mixed Primitive / Root CHOICE / SEQUENCE Generation

2026-10-09. Accepted under [continuous authority](ngap-cpp-aper-autonomous-execution-authority.md); independent contract review PASS before implementation.
Base: `38cb53c36ff4c93547c0a6d9291a53da4c7fff39`.
One objective: integrate approved N4/N5 primitive generation into independently named, non-extensible compound types. Do not change existing output families, runtime, IR or frozen schemas.

## Scope and public APIs

Add `asn1typed_render_cpp_owned_compound_types`, `_compound_mapping`, and `_compound_codec`, each with `(const asn1typed_module_t *, const char *, char **, char *, size_t)` arguments and the existing borrowed-IR, caller-frees-success, deterministic, NULL-output-on-failure contract. Each entry performs complete semantic/name preflight before output; modules are not filtered.

Accept one or more types in a consistent module:

- Named BOOLEAN, exact N5 non-extensible INTEGER domains, and complete N4 named ENUMERATED evidence/model (including enum additions and owned unknown extension indexes).
- Non-extensible CHOICE with 1..255 root alternatives and complete validated owned tag/PER-order evidence.
- Non-extensible fixed-type SEQUENCE, including empty, with mandatory or OPTIONAL fields.
- Compound members refer to an earlier named type in the same module, or inline unconstrained BOOLEAN. Inline INTEGER/ENUMERATED are deferred. Reject forward, recursive, missing/external references, actuals and inconsistent reference metadata.

Reject extension markers on CHOICE/SEQUENCE, DEFAULT/CONDITIONAL, union/other integer domains, SIZE/collections, IOC/container/bound instances, class-field or inline-enum semantics, and unsupported/residual metadata. Validate count/capacity/storage and identities. ENUMERATED extensibility is distinct and remains supported through N4; it does not authorize CHOICE or SEQUENCE extensions.

## Reuse and value model

Delegate named INTEGER and ENUMERATED output to the approved N5/N4 renderers through read-only single-type module views. Reuse their type, mapping, helper and complete API semantics; do not duplicate enum representation or constrained-integer algorithms. Mixed-family validation and global collision checks must precede delegated emission. Delegated failures remain transactional and propagate concrete diagnostics without overlapping buffers.

Named BOOLEAN is a bool alias. CHOICE emits an independent wrapper per alternative and a variant in unchanged Owned IR source order; wrapper `value` is the actual resolved payload type. SEQUENCE emits fields in unchanged `fields[]` order, with optional fields represented by std::optional. Schema INTEGER/BOOLEAN aliases do not create distinct C++ identities. N4 enum identities remain distinct. Default CHOICE selects storage ordinal zero, not necessarily PER index zero; all supported payload defaults are valid.

Use shared final Naming once, store/reuse declaration and reference spellings, and check collisions in actual scopes across types, constraints, mappings, wrappers, complete APIs and helper namespaces `enum_codec`, `uint_codec`, `compound_codec`. No user-namespace using declarations. User type/mapping/helper references, runtime and std references are globally qualified. Standard-header macro and namespace safeguards remain authoritative.

## Mapping and wire behavior

Primitive output retains N4/N5 traits. Named BOOLEAN and inline `COMPOUND_BOOLEAN` describe one bit, false/true=0/1, no alignment; reserve that generated name in module scope.

CHOICE traits contain root_count, selector_bits=ceil(log2(root_count)), extensible=false, selector_align_to_octet=false, numeric storage-to-PER and PER-to-storage std::array<size_t,N> permutations. Derive the tables from owned PER evidence, never storage/variant order. Emit ordinal-indexed wrapper/payload-type/payload-mapping aliases only; no alternative-derived member aliases. Mapping references are globally qualified. All permutation elements and wrapper associations must be verifiable at compile time.

The non-extensible root selector uses N2 `write_enumerated({false,index},root_count,false)` / `read_enumerated(root_count,false)`: for the supported 1..255 counts this has the same unaligned constrained root-index layout as a root CHOICE. Count 1 uses zero selector bits. Spare indexes are rejected by runtime. Selector index is distinct from source/variant ordinal. Payload helpers follow the mapped wrapper/reference, including nested compounds and N4 known/unknown enum values.

Normative source checked: [ITU-T X.691 (02/2021)](https://www.itu.int/rec/dologin_pub.asp?id=T-REC-X.691-202102-I!!PDF-E&lang=e&type=items), clauses 23.2, 23.4 and 23.6 (canonical root index, singleton omission and constrained-integer selector), with clauses 11.5/14 for the delegated small-range layout.

SEQUENCE traits contain extensible=false, optional_bitmap_bit_count, and ordinal-indexed declaration/presence/payload associations. Mandatory bitmap ordinal is size_t(-1); OPTIONAL ordinals count optional declarations only. Emit all OPTIONAL presence bits first in declaration order, then present payloads in field order. No extension bit, DEFAULT or per-type padding. Preserve absent/false/true and enum unknown identities.

## Codec/state contract

Compound field helpers live in compound_codec and use only FieldReader/FieldWriter. Primitive named helpers call their N4/N5 namespaces; inline/named BOOLEAN call bit primitives. Complete APIs follow schema-specific encode_<base>/decode_<base>, S3 Limits and Result publication.

No runtime changes, premature narrowing, local non-sticky range errors or complete finalization inside nested helpers. Each primitive is atomic; compound helpers may have consumed earlier fields before a later primitive fails, but complete wrappers publish no partial object/encoding. Decoder builds a temporary and returns it only through complete validation. Supported values are allocation-free/nothrow; defensive valueless/mapping fallthrough branches, if emitted, report through runtime invalid-selector calls so first error remains sticky. Empty no-op helpers may return success on an already failed view, as documented in S4; complete wrapper still replays the first error.

Generate all three outputs from identical unchanged IR/namespace; include runtime, types, mapping, codec in that order. Headers include standard dependencies outside namespaces. Composed primitive fragments must never put standard includes inside a user namespace. Do not include both standalone and compound outputs for the same schema declarations in one translation unit.

## Real NGAP boundary and required evidence

The untouched frozen Cause has six root alternatives; UE-NGAP-IDs has three. Their `choice-Extensions` alternatives are ordinary root bound open-container references, not CHOICE extension markers. UE-NGAP-ID-pair has OPTIONAL ProtocolExtensionContainer and a SEQUENCE extension marker. N6 does not omit those members, modify schemas, or claim these complete types can be generated. Inspect actual evidence and demonstrate a concrete generation/extraction boundary; unsupported full graphs must fail. Later extension/collection/open-type tasks remain necessary.

Focused synthetic fixtures must exercise mixed modules with 3 and 6 root alternatives, all four integer widths, named extensible enums with known additions and unknown indexes, nested SEQUENCE/CHOICE, multiple OPTIONALs, duplicate payload C++ types, singleton/empty cases, nonidentity tag order, and fully renamed/type-count-varied graphs. Parser/Fixer trees are destroyed before deterministic three-output generation.

Compile strict C++20 with active checks under NDEBUG. Assert whole permutations and wrapper/payload mapping, sequence ordinals, primitive traits/defaults. Independently derive full bytes/length/decoded fields, bitmap ordering, unknown enum values and 32/40-bit variable lengths; native external APER compare actual generated compound codecs in both directions without wire normalization. At least 3/6-alternative and nested/OPTIONAL examples must be external compared, not only primitive helpers.

Test root spare selectors, every short byte prefix, first-nonzero alignment/final padding and trailing offsets, exact/one-less input/output/wire budgets, nested integer overflow/fabricated enum/fake-unknown rejection and sticky helper/complete replay. Reuse authoritative N1/N2/S3 deeper atomic/lifecycle/error-priority suites rather than duplicate all.

Generator negatives cover missing/stale/duplicate evidence, shape/metadata/storage/identity/references, extensions/bound instances, scoped name/macro collisions, NULL arguments and each allocation failure until success. Check concrete diagnostics and NULL output. Run relevant prior typed/runtime regressions, distribution and focused sanitizers with honest Parser/LSan boundaries. Independent implementation review and closure precede commit/push.

## Decisions

N6-01 mixed standalone family; N6-02 N4/N5 delegation; N6-03 root CHOICE 1..255; N6-04 non-extensible fixed SEQUENCE; N6-05 scoped shared spelling and globally qualified lookup; N6-06 generated compound external qualification and fail-closed real-schema boundary: accepted under continuous authority. Independent contract review PASS: no blocking scope, selector, value-model or verification contradictions.

No benchmark, general PER, frozen schema pruning, sequence additions, collections, IOC/open-type dispatch or complete NGAP-PDU qualification in N6.
