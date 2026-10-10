#ifndef ASN1TYPED_RENDER_CPP_INLINE_ENUM_INTERNAL_H
#define ASN1TYPED_RENDER_CPP_INLINE_ENUM_INTERNAL_H
#include "asn1typed.h"
/* Zero-initialize before init. Generation-local view; original Owned IR remains immutable and alive.
 * Synthetic enums precede the original ordered types. Never module_clear it. */
struct asn1typed_cpp_inline_enum_view {
    const asn1typed_module_t *module;
    asn1typed_module_t storage;
    size_t original_type_count;
    const asn1typed_module_t *original;
};
int asn1typed_cpp_inline_enum_view_init(struct asn1typed_cpp_inline_enum_view *,
    const asn1typed_module_t *, char *, size_t);
void asn1typed_cpp_inline_enum_view_clear(struct asn1typed_cpp_inline_enum_view *);
#endif
