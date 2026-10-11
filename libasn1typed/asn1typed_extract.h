#ifndef ASN1TYPED_EXTRACT_H
#define ASN1TYPED_EXTRACT_H

#include "asn1typed.h"
#include "asn1typed_envelope.h"
#include <asn1parser.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Extract one already-fixed source module. On failure, out is cleared. */
int asn1typed_extract_module(asn1p_t *tree, const char *module_name,
		asn1typed_module_t *out, char *error, size_t error_size);

/* Ordinary root graph, separate from IOC extraction. Input is a fixed tree;
 * output must be fresh/cleared. Owns root at types[0] and its supported named
 * and inline dependencies across imports; failure leaves out cleared. NULL
 * limits selects 4096 types, 65536 references, 262144 AST nodes. Non-NULL
 * limits require all positive members. Parameterized types remain rejected.
 * Graph cycles are retained as owned references, not recursively expanded;
 * this is semantic extraction, not proof of renderer/wire support. */
typedef struct asn1typed_graph_limits_s {
    size_t max_types;
    size_t max_references;
    size_t max_ast_nodes;
} asn1typed_graph_limits_t;
int asn1typed_extract_root_graph(asn1p_t *, const char *module_name,
        const char *root_name, const asn1typed_graph_limits_t *limits,
        asn1typed_module_t *, char *error, size_t error_size);

/* Extract a single-container IOC message and its reachable ordinary types.
 * Input must already be fixed. Output must be fresh or cleared; on failure it
 * is cleared. The message is types[0], represented as a flattened SEQUENCE.
 * T3 bounds dependencies to the selected source module and T2 type support.
 */
int asn1typed_extract_message(asn1p_t *tree, const char *module_name,
		const char *message_name, asn1typed_module_t *out,
		char *error, size_t error_size);

/* Opt-in physical single-container message graph, with owned dispatch tables
 * and explicit selector-role proofs. Old flattened extraction is unchanged. */
int asn1typed_extract_physical_message(asn1p_t *, const char *, const char *,
		asn1typed_module_t *, char *, size_t);

/* Own full procedure-table and scoped framing evidence, without materializing
 * unselected procedure bodies. Fresh/cleared output; failure clears it. */
int asn1typed_extract_target_envelope(asn1p_t *, const char *, const char *,
        const char *, const char *, asn1typed_target_envelope_t *, char *, size_t);

#ifdef __cplusplus
}
#endif

#endif
