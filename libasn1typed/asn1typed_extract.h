#ifndef ASN1TYPED_EXTRACT_H
#define ASN1TYPED_EXTRACT_H

#include "asn1typed.h"
#include <asn1parser.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Extract one already-fixed source module. On failure, out is cleared. */
int asn1typed_extract_module(asn1p_t *tree, const char *module_name,
		asn1typed_module_t *out, char *error, size_t error_size);

#ifdef __cplusplus
}
#endif

#endif
