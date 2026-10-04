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
 * Supports primitive aliases, enums and mandatory sequences only. INTEGER's
 * int64_t mapping is a baseline, not a claim about unmodeled constraints.
 * Names are derived once from source identity, then keyword-escaped. Each
 * scope enforces one spelling per source identity and unique final spellings;
 * duplicate declarations fail, even if their source identities are identical.
 * No files, namespaces, numeric enum values, constraints or codecs are emitted.
 */
int asn1typed_render_cpp(const asn1typed_module_t *module, char **out,
		char *diagnostic, size_t diagnostic_size);

#ifdef __cplusplus
}
#endif
#endif
