# N5 — Named Constrained INTEGER Types / Mapping / Codec

2026-10-09. Approved under [continuous Owner authority](ngap-cpp-aper-autonomous-execution-authority.md); independent contract review PASS before implementation.
Base: `93bdc5955b49b675a214ddd373de372358ab1f26`.
Parent authority: N1 constrained-integer runtime contract. One objective: standalone named generation for its four supported domains.

## Scope and APIs

Add enum-family-shaped, separately restricted C entry points:

```c
int asn1typed_render_cpp_owned_uint_types(const asn1typed_module_t *,
    const char *, char **, char *, size_t);
int asn1typed_render_cpp_owned_uint_mapping(const asn1typed_module_t *,
    const char *, char **, char *, size_t);
int asn1typed_render_cpp_owned_uint_codec(const asn1typed_module_t *,
    const char *, char **, char *, size_t);
```

Arguments are module, namespace, output and optional diagnostic buffer/size. IR is borrowed unchanged. Each call independently preflights all declarations; success returns 0 with malloc-backed NUL-terminated output; failure returns -1, output NULL and a concrete diagnostic when possible. Caller passes an empty output slot and frees successful output. No partial output, byte-level postprocessing or fixture/name/type-count gate. Repeated generation must be byte-identical.

Accept one or more named primitive INTEGER types only, with exactly `(0..255)`, `(0..65535)`, `(0..4294967295)` or `(0..1099511627775)`. Require one resolved bounded range, non-extensible, no union/tail or additional metadata. A 32/40-bit domain describes a constraint, not fixed payload width. Derive root_bits solely from exact Owned IR constraint equality; never sizeof(storage), type name or source/variant ordinal.

Require consistent module/type storage and identity; reject unsupported fields/alternatives/enum metadata, size constraints, element/IOC references, parameterized actuals, bound instances and unused residual metadata. Do not filter mixed modules. Other widths, signed/nonzero lower bounds, unbounded or extensible integers, BOOLEAN/ENUMERATED/CHOICE/SEQUENCE, aliases and inline constrained fields are outside this family.

N1 runtime, Owned IR, legacy and S1/S2/S4/N4 renderer contracts and outputs remain unchanged. Compound integration will later consume these approved primitive domains through a separately reviewed task.

## C++ value and mapping model

Generate `using T = ::std::uint64_t;` and `T_constraint` containing uint64 lower_bound=0 / upper_bound. This follows S1 numeric storage: these aliases do not provide distinct C++ schema identities, and caller chooses the schema-specific codec API. Zero initialization is a valid root value. Do not introduce boxed integers, validation setters or narrow storage.

Generate `T_aper` with globally qualified value_type, uint64 bounds, unsigned root_bits, extensible=false and:

| root_bits | length_prefix_bits | minimum_payload_octets | maximum_payload_octets | minimal_payload |
| ---: | ---: | ---: | ---: | --- |
| 8 | 0 | 1 | 1 | false |
| 16 | 0 | 2 | 2 | false |
| 32 | 2 | 1 | 4 | true |
| 40 | 3 | 1 | 5 | true |

All use align_before_payload_to_octet=true and most_significant_octet_first=true. 32/40 length prefixes are unaligned at field start and encode payload_octets-1; alignment is after the prefix. 8/16 have no length prefix. No generic fixed encoded-length metadata or per-type final-padding rule. Bounds use safe UINT64_C constants. Root width and all metadata derive from constraints.

## Codec and state/error contract

Generate schema-specific complete encode_<base>(const T&, const Limits&={}) and decode_<base>(::std::span<const ::std::byte>, const Limits&={}) returning existing runtime Result<CompleteEncoding> / Result<T>. Shared final Naming spelling determines base/type/helper names once. Field helpers in uint_codec use only FieldWriter / FieldReader and call write_constrained_uint(v,T_aper::root_bits) / read_constrained_uint(T_aper::root_bits).

Pass uint64 values directly to runtime without casts or pre-emptive local range errors. N1 performs width/range validation before narrowing and atomically includes prefix/alignment/payload. Runtime records the first code and exact offset, and later helpers/primitives/wrappers replay it. Decode returns only a temporary value until decode_complete accepts zero final padding and absence of trailing data. No new ErrorCode, Limits, context or finish API.

8/16 use fixed payload widths; 32/40 use minimal 1..4/1..5 payloads. N1 strict spare length / nonminimal decode and error priorities remain authoritative. Prefix and alignment count against shared wire limits, as do final complete padding rules. Full outermost result publication follows S3.

## Naming and header contract

Reuse shared final_name, standard header macro protection and namespace validation. Preserve final spellings in declarations, references and scoped collision checks. Common preflight compares type, constraint, mapping, helper namespace and complete API names in their real C++ scopes, and rejects standard object-macro conflicts before output. Qualify user type references globally inside mapping to avoid nested-name shadowing. Use ::nrforge::aper:: and ::std::; no using declarations in user namespace.

Generate all three outputs from the same unchanged IR and exact namespace. Include runtime.hpp, types, mapping, codec in that order. Types/mapping do not require runtime. Each emitted header includes its own standard-header dependencies, but mapping needs types and codec needs prior runtime/types/mapping. Existing namespace segment limit and documented GNU/platform macro exclusion apply.

## Required focused verification

Real Parser -> Fixer -> Owned IR fixtures, tree deleted before rendering, include multiple types, all four widths and fully renamed domains. Render all outputs twice, compile/run strict C++20 with checks active under NDEBUG. Assert storage uint64, bounds, all mapping width/prefix/alignment/minimality metadata and zero default.

Independently derive complete bytes, lengths and decoded values for zero/domain maximum and all payload-octet transitions at cursor residues0..7. Cross-check the actual generated field codecs in both directions with an external APER implementation, including 32/40-bit variable-length layout. Reuse N1 normative vectors and external qualification without assuming generator correctness follows from runtime correctness.

Test max+1/UINT64_MAX rejection without narrowing, helper/wrapper sticky replay, all short byte prefixes, first-nonzero alignment offsets, final padding/trailing bytes, invalid 40-bit selectors and nonminimal payloads, and exact/one-less input/output/wire budgets. N1 tests remain the authoritative deeper primitive atomic/lifecycle and combined-priority suite; do not duplicate every existing runtime test.

Generator negative tests include NULL arguments/output, malformed storage/identity, mixed/unsupported kinds, missing/extensible/signed/nonzero-lower/other-width/union constraints, unexpected metadata, names/namespace/macros, and allocation failure at each emission/planning point. Verify NULL output and specific diagnostics; relevant legacy/synthetic/N4/runtime regressions, distribution and focused sanitizers must pass within honestly documented environment/Parser boundaries. Independent review and finding closure precede commit.

## Frozen decisions under continuous authority

| ID | Decision | Status |
| --- | --- | --- |
| N5-01 | Separate standalone named INTEGER-only output family | Accepted under continuous authority |
| N5-02 | Exact four N1 domains; uint64 alias storage and bounds | Accepted under continuous authority |
| N5-03 | IR-derived mapping, including variable 32/40 payloads | Accepted under continuous authority |
| N5-04 | Runtime-only primitive delegation and sticky/publication rules | Accepted under continuous authority |
| N5-05 | Shared scoped Naming/qualified lookup and transactional output | Accepted under continuous authority |
| N5-06 | Independent vectors/external comparison, negatives and focused review | Accepted under continuous authority |

No benchmark, inline/compound integration, schema editing or full NGAP qualification in N5. Independent contract review: PASS on 2026-10-09; implementation may proceed. The review confirmed the exact domains, alias storage, variable payload metadata, runtime delegation and verification scope without blockers.
