#ifndef ASN1TYPED_RENDER_UPER_H
#define ASN1TYPED_RENDER_UPER_H
#include "asn1typed.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Dedicated ordinary UPER family. Borrowed graph is never modified. Output
 * is caller-owned text (free), NULL on failure. Requires fresh out slot,
 * complete acyclic module-qualified closure, validated ordinary structure and
 * enum indexes. All constraints/defaults are preflighted, not erased. Header
 * owns types, mapping and codecs, rooted at graph types[0]. Namespace must be
 * C++20 safe and outside nrforge::uper. Requires uper/runtime.hpp on include path.
 * Ordinal Tn/Aj identifiers preserve graph identity without sanitizing private
 * specialization keys; public fields use shared C++ name policy with f_ prefix.
 * Unsupported closures fail before any output is published. No IOC lowering.
 */
int asn1typed_render_cpp_owned_uper(const asn1typed_module_t *, const char *, char **, char *,
                                    size_t);
#ifdef __cplusplus
}
#endif
#endif
