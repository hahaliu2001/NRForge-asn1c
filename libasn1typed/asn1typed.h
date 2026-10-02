#ifndef ASN1TYPED_H
#define ASN1TYPED_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum asn1typed_type_kind_e {
	ASN1TYPED_TYPE_PRIMITIVE,
	ASN1TYPED_TYPE_SEQUENCE,
	ASN1TYPED_TYPE_SEQUENCE_OF,
	ASN1TYPED_TYPE_ENUMERATED
} asn1typed_type_kind_e;

typedef enum asn1typed_presence_e {
	ASN1TYPED_PRESENCE_MANDATORY,
	ASN1TYPED_PRESENCE_OPTIONAL,
	ASN1TYPED_PRESENCE_CONDITIONAL
} asn1typed_presence_e;

typedef struct asn1typed_source_location_s {
	char *file;
	unsigned line;
} asn1typed_source_location_t;

typedef struct asn1typed_type_identity_s {
	char *module;
	char *source_name;
} asn1typed_type_identity_t;

typedef struct asn1typed_type_ref_s {
	char *module;
	char *source_name;
} asn1typed_type_ref_t;

typedef struct asn1typed_field_s {
	char *source_name;
	asn1typed_type_ref_t type;
	asn1typed_presence_e presence;
	asn1typed_source_location_t location;
} asn1typed_field_t;

typedef struct asn1typed_enum_item_s {
	char *source_name;
	asn1typed_source_location_t location;
} asn1typed_enum_item_t;

typedef struct asn1typed_type_s {
	asn1typed_type_identity_t identity;
	asn1typed_type_kind_e kind;
	asn1typed_source_location_t location;
	asn1typed_field_t *fields;
	size_t field_count;
	size_t field_capacity;
	asn1typed_type_ref_t element_type;
	asn1typed_enum_item_t *enum_items;
	size_t enum_item_count;
	size_t enum_item_capacity;
} asn1typed_type_t;

typedef struct asn1typed_module_s {
	char *source_name;
	asn1typed_source_location_t location;
	asn1typed_type_t *types;
	size_t type_count;
	size_t type_capacity;
} asn1typed_module_t;

/* All string arguments are copied. The caller retains ownership of them. */
int asn1typed_source_location_init(asn1typed_source_location_t *location,
		const char *file, unsigned line);
void asn1typed_source_location_clear(asn1typed_source_location_t *location);
int asn1typed_type_ref_init(asn1typed_type_ref_t *ref,
		const char *module, const char *source_name);
void asn1typed_type_ref_clear(asn1typed_type_ref_t *ref);
int asn1typed_module_init(asn1typed_module_t *module,
		const char *source_name, const char *file, unsigned line);
void asn1typed_module_clear(asn1typed_module_t *module);
int asn1typed_module_add_type(asn1typed_module_t *module,
		const char *source_name, asn1typed_type_kind_e kind,
		const char *file, unsigned line, asn1typed_type_t **type_out);
int asn1typed_type_add_field(asn1typed_type_t *type,
		const char *source_name, const char *ref_module,
		const char *ref_source_name, asn1typed_presence_e presence,
		const char *file, unsigned line);
int asn1typed_type_set_element_type(asn1typed_type_t *type,
		const char *ref_module, const char *ref_source_name);
int asn1typed_type_add_enum_item(asn1typed_type_t *type,
		const char *source_name, const char *file, unsigned line);
void asn1typed_type_clear(asn1typed_type_t *type);

#ifdef __cplusplus
}
#endif

#endif
