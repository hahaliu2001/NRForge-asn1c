# N4 — ENUMERATED C++ Types / Mapping / Codec Contract

2026-10-09. **Approved / Accepted under Owner continuous authorization — 2026-10-09.**
Base: `6ce59be416e2dee512ddb6added0f425ec4e4fa1`.
Authorities: approved N2 ENUMERATED runtime and N3 owned numeric/PER evidence contracts.
One objective: freeze standalone named ENUMERATED generation using those existing contracts. This study changes no production code.

## Scope and implementation boundary

Add a separately restricted enum-only output family, with three C APIs:

```c
int asn1typed_render_cpp_owned_enum_types(const asn1typed_module_t *,
    const char *, char **, char *, size_t);
int asn1typed_render_cpp_owned_enum_mapping(const asn1typed_module_t *,
    const char *, char **, char *, size_t);
int asn1typed_render_cpp_owned_enum_codec(const asn1typed_module_t *,
    const char *, char **, char *, size_t);
```

Arguments follow existing renderers: owned module, namespace, output, diagnostic buffer and size. On failure return -1, keep output NULL and provide a specific diagnostic when the buffer is usable; success returns 0 and caller owns malloc-backed text. Output generation is transactional and deterministic. The three outputs use the same unmodified IR and exact namespace; include runtime.hpp, types, mapping, then codec. Types and mapping do not require runtime. All three outputs perform the same evidence and naming preflight rather than trusting a previous call.

A module contains one or more named ENUMERATED types only. Each must have a complete mapping accepted by N3 validate, 1..255 roots, and assigned numbers representable by int64_t. N3's wider intmax_t domain remains unchanged; this renderer explicitly rejects values outside its declared C++ storage domain before emitting text. Root counts outside the N2 runtime domain fail during generation. Zero known additions is valid for extensible types. Do not impose a fixture/type-name/type-count identity gate.

Inline ENUMERATED, fields, CHOICE, SEQUENCE, aliases, parameterized actuals, bound instances, IOC, collections and real NGAP integration are outside this entry family. Do not silently filter a mixed module to its enum subset. Legacy renderer and existing S1/S2/S4 APIs, their output and acceptance remain unchanged. Later work must separately integrate enum references and inline naming into compound generation. No N2 runtime or N3 IR changes are proposed.

## Generated value representation

For each source type T, generate a distinct struct T containing a nested `enum class Known : std::int64_t`. Each known enumerator's underlying value is its actual assigned ASN.1 number, including negative, sparse or reordered numbers. It is never a PER index or source ordinal. Both known roots and known additions are enumerators of that same schema-specific Known type; mapping metadata retains their membership.

For a non-extensible type:

```cpp
struct T {
    enum class Known : std::int64_t { /* source-ordered known items */ };
    Known value{Known::first_canonical_root};
};
```

For an extensible type:

```cpp
struct T {
    enum class Known : std::int64_t { /* source-ordered known items */ };
    struct UnknownExtension { std::uint64_t index; };
    std::variant<Known, UnknownExtension> value{Known::first_canonical_root};
};
```

`first_canonical_root` above is explanatory: emit the actual final spelling of the root with PER index zero. Default construction therefore denotes a known valid root even when zero is absent from assigned numbers. UnknownExtension is nested in T and carries the independent extension index, never an assigned number. Distinct schema types cannot interchange Known or UnknownExtension values without explicit conversion. No heap storage, string lookup, implicit numeric conversion or shared generic enum wrapper is required.

UnknownExtension is valid only for an index not present in this schema's known additions (index >= known_addition_count). Public representation permits invalid enum casts and invalid unknown objects; encode must reject them, not reinterpret or silently normalize them. Decode of a known addition produces Known, and only genuinely unknown additions produce UnknownExtension. Thus all valid values preserve their category through encode/decode. Unknown UINT64_MAX is supported as authorized by N2; this does not authorize encoding opaque unknown open-type data.

Generated declarations use signed constants without overflowing positive intermediates: INT64_MIN uses `(-INT64_C(9223372036854775807) - INT64_C(1))`; other negative constants negate only representable positive constants.

## Mapping metadata

Generate `T_aper` with root_count, known_addition_count and extensible, plus source-ordered entries retaining:

- T::Known value / assigned number;
- root versus addition membership;
- independent N3 per_enumeration_index.

Generate numeric source-ordinal-to-per-index metadata and separate root-index-to-source-ordinal / known-addition-index-to-source-ordinal tables. Known additions may be absent; do not emit a zero-sized raw C array. Use C++20 std::array and explicit lengths. Indexing and counts use size_t; conversion to N2 unsigned root_count and uint64_t wire indexes is validated before emission and documented with generated compile-time checks as needed.

All canonical tables come from N3 evidence, with validate recomputing uniqueness, continuity and numeric ordering. Preserve declaration/source order in Known and metadata, never sort the Owned IR or use enum underlying value / variant index as wire index. Root and addition tables have distinct domains; a combined source ordinal is not itself a wire index. Compile-time tests prove both directions are inverse within each part and check all Known underlying values.

## Generated codec API and errors

For each T, use shared final Naming spelling to generate complete `encode_<base>(const T &, const Limits & = {})` and `decode_<base>(::std::span<const ::std::byte>, const Limits & = {})`, returning the existing runtime Result<CompleteEncoding> and Result<T>. Exact signatures follow S4 complete codecs. Internal field helpers take only FieldWriter / FieldReader; they cannot finish a nested value.

Encode translates Known by validated metadata into `{is_extension, index}` and calls FieldWriter::write_enumerated with the generated root_count/extensible. UnknownExtension delegates its index to that same primitive after checking that it is genuinely unknown. It never narrows an index or writes extension bits independently.

A Known value not in the schema, or UnknownExtension duplicating a known addition, must produce a sticky constraint_violation at field start. Do not construct a non-sticky Result error in the helper. Without a new runtime API, delegate this rejection to write_enumerated using an out-of-range root index equal to root_count. This intentionally invalid sentinel is only an error path; it is never published as wire data. Failed/finished primitive priority and exact first error come from N2. Such semantic rejection must still call the primitive on an already-failed view so the original sticky error wins.

Decode calls read_enumerated atomically, translates a root or known addition through its own per-part reverse table, and preserves an unknown extension index in T::UnknownExtension. A non-extensible representation has no UnknownExtension alternative. Temporary decoded objects publish only after decode_complete checks final padding and trailing data. Complete singleton encoding uses S3's one-zero-octet substitution when the field consumes no bits. All limits, strict malformed encodings, sticky errors, offsets and whole-primitive atomicity remain N2/S3 behavior. No new ErrorCode, Limits, context or finish API is introduced.

With scalar noexcept Known/UnknownExtension alternatives the extensible variant cannot become valueless through supported operations. Any defensive impossible-state path must not create a conflicting non-sticky failure; no UB or corrupt object is used to exercise it.

## Naming, macros and include contract

Reuse asn1typed_render_cpp_final_name and the existing namespace safety rule. Compute final spellings once, reuse declarations/references, and compare names in their actual C++ scopes. Preflight all type/mapping/helper/API names, nested names and known item spellings; reject collisions and header object-macro conflicts before producing output. Do not duplicate Naming policy, overwrite item spelling, or blanket-reserve a type base merely because encode_<base> differs from it.

Use global `::nrforge::aper::` runtime qualification. Standard-library qualification is global `::std::` for new output. No using declarations are injected into user namespace. Types headers include their own required standard headers; mapping requires types first; codec requires runtime/types/mapping first. Namespace safety retains its documented 127-character per-segment limit and GNU/platform macro exclusion. Do not claim portability to arbitrary platform macros.

## Required focused evidence after approval

Real Parser -> Fixer -> Owned IR fixtures must delete Parser trees before generating all three outputs and must generate twice for deterministic comparison. Cover reordered/sparse/negative/implicit root values, signed endpoints, known additions below the root maximum, extensible enums with no known additions, a non-extensible singleton, multiple fully renamed types and normalized-name collisions.

Strict C++20 compile/run assertions verify distinct schema identities, assigned values, valid default root, source order, both per-part inverse tables and known/unknown representations. Independent byte/length vectors must include root indexes and known additions plus unknown extension indexes 0 (when no additions), 63/64/255/256/UINT64_MAX and embedded Boolean prefixes at all residues 0..7. Reuse documented external oracle limitations: asn1tools long-extension alignment is not authoritative; pycrate or an independent bit model is needed there. Standalone singleton substitution follows S3 even if an oracle emits an empty string.

Check complete bytes, lengths and all decoded values; cast-invalid Known, a fake UnknownExtension for a known addition, truncation, unused root patterns, malformed/nonminimal long forms, nonzero alignment/final padding, trailing data, exact/one-less budgets, and encode/decode sticky replay through helpers and wrappers. No result is published after failure.

Generator negative tests assert -1, output NULL and specific diagnostic for missing/unsupported/stale evidence, invalid root count, out-of-int64 assigned value where the IR platform supports it, unexpected kinds/metadata, namespace/macro/name conflicts, and invalid API arguments. Allocation injection verifies transactional output and cleanup. Run relevant typed/runtime regressions and focused sanitizer checks with honest Parser/environment limitations; independent review precedes implementation commit.

The following arithmetic expectations are carried forward from approved N2, not claimed as a new N4 implementation result: non-extensible three-root indexes 0/1/2 give 00/40/80; extensible three-root index2 gives40; extension0/63/64/256 gives80/bf/c00140/c0020100, and UINT64_MAX givesc008ffffffffffffffff, with complete zero padding. Assigned numbers may differ completely from these indexes. No full frozen-schema qualification, message codec or benchmark is part of N4.

## Owner decisions

| ID | Proposed freeze | Status |
| --- | --- | --- |
| N4-01 | Separate enum-only three-output APIs; preserve prior renderer behavior | Approved / Accepted |
| N4-02 | Schema-specific Known assigned numbers and typed unknown uint64 extension index | Approved / Accepted |
| N4-03 | int64 storage, root_count 1..255, N3 complete evidence required | Approved / Accepted |
| N4-04 | Source-ordered metadata and two separate canonical reverse tables | Approved / Accepted |
| N4-05 | N2 primitive delegation, canonical known/unknown categories and sticky rejection | Approved / Accepted |
| N4-06 | Shared final spelling, scoped collisions and global runtime qualification | Approved / Accepted |
| N4-07 | Focused deterministic/vector/error/allocation verification before commit | Approved / Accepted |
| N4-08 | Inline and compound enum integration remain separate subsequent work | Approved / Accepted |

Independent read-only agent review on 2026-10-09: **PASS as an Owner approval candidate**, no remaining blocking items. Review checked schema identity, known/unknown canonicality, signed storage bounds, canonical defaults, separate inverse tables, sticky rejection and scoped naming. One decode-signature mismatch was corrected to span<const std::byte> and const Limits&, matching S4/runtime. A separate read-only design agent corroborated the enum-only scope and primitive-based sticky rejection. These reviews did not build or qualify an implementation. Document whitespace and git diff --check pass. Only this new document is changed; no stage, commit, push or production implementation in this task.

Owner continuous authorization on 2026-10-09 accepts this independently reviewed N4 scope and authorizes implementation, tests, independent review, fixes, commit and push within the first-message target.
