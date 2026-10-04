#ifndef ASN1TYPED_NAME_H
#define ASN1TYPED_NAME_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum asn1typed_name_style_e {
	ASN1TYPED_NAME_TYPE,
	ASN1TYPED_NAME_FIELD,
	ASN1TYPED_NAME_IOC_FIELD,
	ASN1TYPED_NAME_MODULE
} asn1typed_name_style_e;

typedef enum asn1typed_name_result_e {
	ASN1TYPED_NAME_OK = 0,
	ASN1TYPED_NAME_INVALID = -1,
	ASN1TYPED_NAME_NOMEM = -2,
	ASN1TYPED_NAME_COLLISION = -3
} asn1typed_name_result_e;

/* Remove exactly one case-sensitive leading "id-". Borrowed result, valid
 * as long as source; NULL stays NULL. No validation or target spelling here.
 * Also used by extraction solely to preserve T3 source_name compatibility.
 */
const char *asn1typed_name_ioc_identity(const char *source);

/* Plain source strings only; no parser/fixer or renderer dependencies.
 * ASCII letters/digits and hyphen separators are accepted. Hyphen runs,
 * including leading/trailing runs, are separators. At least one token is
 * required and the first token must start with a letter. Other bytes fail.
 * One tokenizer splits lower->upper, acronym->word (ABC|Value), and both
 * letter/digit transitions. Consecutive digits form one token.
 * TYPE: capitalize each lowercased token and concatenate.
 * FIELD/MODULE: lowercase tokens joined with underscores.
 * IOC_FIELD: remove one leading id- before applying FIELD.
 * ASCII rules are locale-independent. Keywords (e.g. class, def) are left
 * unchanged: future target renderers must escape them and check collisions
 * again after escaping. MODULE decides no extension or directory layout.
 * On success *out is owned, free() it. On failure *out is NULL (if out is
 * non-NULL). Pass an empty output slot; an existing allocation is not freed.
 */
asn1typed_name_result_e asn1typed_name_make(const char *source,
		asn1typed_name_style_e style, char **out);

/* Small caller-owned scope: initialize to {0}, clear when finished.
 * One scope represents one emitted identifier namespace. Use separate scopes
 * for unrelated namespaces. Register every emitted name to detect collisions;
 * name_make alone performs no scope checking. Source keys must distinguish
 * identities within that scope (e.g. module-qualified when necessary).
 */
struct asn1typed_name_entry_s;
typedef struct asn1typed_name_scope_s {
	struct asn1typed_name_entry_s *entries;
} asn1typed_name_scope_t;

/* Copies source_key and normalized name. Re-registering the same pair is
 * idempotent. A different source_key with the same name returns COLLISION;
 * errors leave the scope unchanged. Names must be ASCII identifiers.
 */
asn1typed_name_result_e asn1typed_name_scope_add(asn1typed_name_scope_t *scope,
		const char *source_key, const char *name);
void asn1typed_name_scope_clear(asn1typed_name_scope_t *scope);

#ifdef __cplusplus
}
#endif
#endif
