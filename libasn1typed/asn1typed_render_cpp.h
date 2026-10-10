#ifndef ASN1TYPED_RENDER_CPP_H
#define ASN1TYPED_RENDER_CPP_H

#include "asn1typed.h"
#include "asn1typed_envelope.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Render one module as C++20 text. Returns 0 on success, -1 on failure.
 * On success *out is NUL-terminated, caller-owned (free it). On failure
 * *out is NULL; diagnostic receives a message when non-NULL and size > 0.
 * Pass an empty output slot: existing allocations are not freed.
 * IR is borrowed and never changed. Supported local acyclic declarations may
 * appear in any order. A temporary dependency-first plan chooses the smallest
 * original IR index among currently ready declarations. Missing/external
 * references and cycles are rejected; no forward declarations are emitted.
 * Supports primitive aliases, enums, mandatory/optional/conditional sequence
 * fields and named SequenceOf aliases. Mandatory fields use T; Optional and
 * Conditional both use std::optional<T> for generated storage presence only.
 * Optional and Conditional remain distinct production IR semantics. This
 * renderer does not evaluate Conditional predicates, validate condition-dependent
 * presence, enforce protocol presence rules, or infer semantic presence from
 * std::optional<T>. Future validated codec/protocol processing must use
 * authoritative IR-derived presence metadata plus condition rules/context,
 * and fail closed when a Conditional rule is unavailable or cannot be evaluated.
 * No such validation is implemented here. INTEGER's
 * int64_t mapping is a baseline, not a claim about unmodeled constraints.
 * Names pass once through T4 normalization, keyword suffix escaping (_),
 * reserved-form rejection (leading _ or any __), and header-macro escaping
 * (reachable standard C header names gain cpp_; see the implementation list).
 * Development probes use g++ -std=c++20 with the emitted headers:
 * <cstdint>, <optional>, <string>, <vector>.
 * The portable protection set is bounded by relevant standard-header
 * interfaces, including the complete C++20 <cerrno> synopsis. GNU/platform
 * extensions, consumer/compiler macros outside that set and extra headers
 * are outside the contract and may require a later isolation strategy.
 * Each scope enforces one spelling per source identity and unique final spellings;
 * duplicate declarations fail, even if their source identities are identical.
 * No files, namespaces, numeric enum values, constraints or codecs are emitted.
 */
int asn1typed_render_cpp(const asn1typed_module_t *module, char **out,
		char *diagnostic, size_t diagnostic_size);

/* Restricted owned C++20 value model for synthetic protocol fixtures. This
 * opt-in entry emits a namespace, bounded INTEGER metadata, CHOICE wrappers,
 * variant storage and OPTIONAL sequence fields. Alternative wrapper types
 * carry identity; std::variant::index() is not a PER index. Unsupported IR
 * semantics fail closed. It emits no wire mapping or codec. namespace_name is
 * emitted verbatim after strict C++20 namespace validation. Each namespace
 * segment is limited to 127 characters; any segment that is a C++ keyword,
 * reserved identifier, or object-like standard macro from an included
 * standard header (including <cstdint> limit macros) is rejected rather than
 * rewritten. The emitted text is
 * validated with strict C++20 compilation; GNU/platform macros are outside
 * the naming guarantee. */
int asn1typed_render_cpp_owned_slice(const asn1typed_module_t *module,
		const char *namespace_name, char **out, char *diagnostic,
		size_t diagnostic_size);

/* Emit synthetic basic APER mapping metadata for the restricted owned slice.
 * Generate both headers from the same unchanged Owned IR contents and exact
 * namespace. Include the owned value header first, then this mapping header;
 * the mapping metadata reuses its final type and CHOICE wrapper spellings.
 * This emits constraints/layout traits only; it does not encode values. */
int asn1typed_render_cpp_owned_aper_mapping(const asn1typed_module_t *module,
		const char *namespace_name, char **out, char *diagnostic,
		size_t diagnostic_size);

/* Restricted S4 synthetic typed codec. Generate from the same unchanged IR
 * and namespace. Include runtime, type, mapping, then codec headers, in that order. */
int asn1typed_render_cpp_owned_aper_codec(const asn1typed_module_t *module,
		const char *namespace_name, char **out, char *diagnostic,
		size_t diagnostic_size);

/* Standalone named ENUMERATED output family, from the same unchanged owned IR
 * and namespace: include runtime, types, mapping, then codec. Each entry checks
 * complete N3 evidence, int64 assigned numbers, root_count 1..255 and naming.
 * Types/mapping need no runtime. Mixed modules and inline enums are rejected.
 * Unknown extension indexes preserve uint64 values and are distinct per schema;
 * known additions must use Known. Failure keeps *out NULL; caller frees success. */
int asn1typed_render_cpp_owned_enum_types(const asn1typed_module_t *,
		const char *, char **, char *, size_t);
int asn1typed_render_cpp_owned_enum_mapping(const asn1typed_module_t *,
		const char *, char **, char *, size_t);
int asn1typed_render_cpp_owned_enum_codec(const asn1typed_module_t *,
		const char *, char **, char *, size_t);

/* Standalone named INTEGER output family for exact non-extensible zero-based
 * 8/16/32/40-bit domains. uint64 storage does not supply schema type identity;
 * choose the schema-specific API. Generate unchanged IR/exact namespace and
 * include runtime, types, mapping, codec. Types/mapping do not need runtime.
 * Each entry independently rejects unsupported shape/naming. Output follows
 * the caller-owned/NULL-on-failure contract above. Runtime performs range checks. */
int asn1typed_render_cpp_owned_uint_types(const asn1typed_module_t *,
		const char *, char **, char *, size_t);
int asn1typed_render_cpp_owned_uint_mapping(const asn1typed_module_t *,
		const char *, char **, char *, size_t);
int asn1typed_render_cpp_owned_uint_codec(const asn1typed_module_t *,
		const char *, char **, char *, size_t);

/* Mixed N4/N5 primitives plus BOOLEAN, non-extensible root CHOICE (1..255)
 * and mandatory/OPTIONAL fixed SEQUENCE. References must be earlier/local;
 * inline BOOLEAN only. Same unchanged IR/namespace and runtime/types/mapping/
 * codec include order apply. Do not also include standalone primitive outputs
 * for the same declarations. Every entry preflights the whole module. */
int asn1typed_render_cpp_owned_compound_types(const asn1typed_module_t *,
		const char *, char **, char *, size_t);
int asn1typed_render_cpp_owned_compound_mapping(const asn1typed_module_t *,
		const char *, char **, char *, size_t);
int asn1typed_render_cpp_owned_compound_codec(const asn1typed_module_t *,
		const char *, char **, char *, size_t);


/* Opt-in N7 root-only extensible SEQUENCE family. Requires validated structural
 * evidence and N6 payload semantics; keeps owned opaque additions on decode,
 * rejects any retained extension sidecar on encode. Do not mix output families
 * for the same graph. Generate unchanged IR/namespace; include runtime, types,
 * mapping, codec, with sequence_extensions.hpp available on the include path.
 * The runtime namespace nrforge::aper and its descendants are reserved. */
int asn1typed_render_cpp_owned_sequence_extension_types(const asn1typed_module_t *,
        const char *, char **, char *, size_t);
int asn1typed_render_cpp_owned_sequence_extension_mapping(const asn1typed_module_t *,
        const char *, char **, char *, size_t);
int asn1typed_render_cpp_owned_sequence_extension_codec(const asn1typed_module_t *,
        const char *, char **, char *, size_t);


/* Opt-in N8 bounded SEQUENCE OF family, including preceding local collections
 * and N6/N7 graphs. Requires finite non-extensible SIZE within 0..65535.
 * Uses owned identity-preserving vectors and cumulative runtime element limits.
 * Same unchanged IR/namespace/include order and reserved runtime namespace
 * apply; do not mix output families for declarations in the same graph. */
int asn1typed_render_cpp_owned_collection_types(const asn1typed_module_t *,
        const char *, char **, char *, size_t);
int asn1typed_render_cpp_owned_collection_mapping(const asn1typed_module_t *,
        const char *, char **, char *, size_t);
int asn1typed_render_cpp_owned_collection_codec(const asn1typed_module_t *,
        const char *, char **, char *, size_t);

/* Opt-in physical N9 IOC graph family. Requires independently validated owned
 * registry and selector-role evidence, complete module-qualified dependency
 * closure and full bound actual identities. Includes N6/N7/N8 ordinary graphs;
 * declarations need not be source ordered. Unknown entries are retained on
 * decode but sticky-refused on encode. Duplicate/missing rows and received
 * criticality are preserved; protocol policy is not evaluated. Same unchanged
 * IR/namespace/include order and reserved runtime namespace rules apply. */
int asn1typed_render_cpp_owned_ioc_types(const asn1typed_module_t *,
        const char *, char **, char *, size_t);
int asn1typed_render_cpp_owned_ioc_mapping(const asn1typed_module_t *,
        const char *, char **, char *, size_t);
int asn1typed_render_cpp_owned_ioc_codec(const asn1typed_module_t *,
        const char *, char **, char *, size_t);

/* Separate bounded N11 envelope outputs; include after the unchanged N9
 * body outputs from the same namespace/graph. Opaque reception is owned but
 * receive-only. Every renderer validates evidence and shared final names. */
int asn1typed_render_cpp_target_envelope_types(const asn1typed_module_t *, const asn1typed_target_envelope_t *, const char *, char **, char *, size_t);
int asn1typed_render_cpp_target_envelope_mapping(const asn1typed_module_t *, const asn1typed_target_envelope_t *, const char *, char **, char *, size_t);
int asn1typed_render_cpp_target_envelope_codec(const asn1typed_module_t *, const asn1typed_target_envelope_t *, const char *, char **, char *, size_t);

#ifdef __cplusplus
}
#endif
#endif
