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

/* Extract a single-container IOC message and its reachable ordinary types.
 * Input must already be fixed. Output must be fresh or cleared; on failure it
 * is cleared. The message is types[0], represented as a flattened SEQUENCE.
 * T3 bounds dependencies to the selected source module and T2 type support.
 */
int asn1typed_extract_message(asn1p_t *tree, const char *module_name,
		const char *message_name, asn1typed_module_t *out,
		char *error, size_t error_size);

#ifdef __cplusplus
}
#endif

#endif
