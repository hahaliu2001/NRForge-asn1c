#ifndef ASN1TYPED_RENDER_CPP_IOC_INTERNAL_H
#define ASN1TYPED_RENDER_CPP_IOC_INTERNAL_H
#include "asn1typed.h"
/* Private emission plan; no synthetic IR escapes the renderer. */
struct compound_buf { char *text; size_t length; };
struct member_plan { char *name, *wrapper, *qualified_wrapper; const char *type, *mapping, *put, *get; };
struct type_plan {
    char *type, *qualified_type, *mapping, *qualified_mapping, *constraint;
    char *encode, *decode, *put, *get, *qualified_put, *qualified_get, *extension_member;
    struct member_plan *members;
    size_t member_count;
    char *unknown_wrapper, *qualified_unknown_wrapper, *value_member;
};
struct asn1typed_cpp_ioc_entry {
    const asn1typed_ioc_registry_t *registry;
    const char *value_source_name;
};
int asn1typed_render_cpp_compound_ioc(const asn1typed_module_t *, const char *,
    const struct asn1typed_cpp_ioc_entry *, int, char **, char *, size_t);
int asn1typed_render_cpp_ioc_emit(struct compound_buf *, const struct type_plan *,
    const struct asn1typed_cpp_ioc_entry *, int);
#endif
