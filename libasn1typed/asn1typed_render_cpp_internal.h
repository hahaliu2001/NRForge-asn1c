#ifndef ASN1TYPED_RENDER_CPP_INTERNAL_H
#define ASN1TYPED_RENDER_CPP_INTERNAL_H

#include "asn1typed_name.h"

/* Final spelling shared by the owned type, mapping, and codec renderers. */
char *asn1typed_render_cpp_final_name(const char *source,
		asn1typed_name_style_e style);
int asn1typed_render_cpp_header_macro(const char *spelling);

#endif
