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
 * IR is borrowed and never changed. Named references must resolve to earlier
 * declarations in this module (dependency-ready order); recursion is rejected.
 * Supports primitive aliases, enums, mandatory/optional sequence fields and
 * named SequenceOf aliases. Conditional presence is rejected. INTEGER's
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

#ifdef __cplusplus
}
#endif
#endif
