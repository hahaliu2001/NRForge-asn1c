#ifndef ASN1TYPED_RENDER_CPP_H
#define ASN1TYPED_RENDER_CPP_H

#include "asn1typed.h"

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

#ifdef __cplusplus
}
#endif
#endif
