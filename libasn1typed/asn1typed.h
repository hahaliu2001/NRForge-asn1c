#ifndef ASN1TYPED_H
#define ASN1TYPED_H

#include <stddef.h>
#include <stdint.h>

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

/* Built-in ASN.1 primitive semantics, independent of any target language. */
typedef enum asn1typed_primitive_kind_e {
	ASN1TYPED_PRIMITIVE_INVALID,
	ASN1TYPED_PRIMITIVE_BOOLEAN,
	ASN1TYPED_PRIMITIVE_INTEGER,
	ASN1TYPED_PRIMITIVE_UTF8_STRING,
	ASN1TYPED_PRIMITIVE_PRINTABLE_STRING
} asn1typed_primitive_kind_e;

typedef enum asn1typed_ref_kind_e {
	ASN1TYPED_REF_NAMED,
	ASN1TYPED_REF_PRIMITIVE
} asn1typed_ref_kind_e;

typedef struct asn1typed_source_location_s {
	char *file;
	unsigned line;
} asn1typed_source_location_t;

typedef struct asn1typed_type_identity_s {
	char *module;
	char *source_name;
} asn1typed_type_identity_t;

typedef struct asn1typed_type_ref_s {
	asn1typed_ref_kind_e kind;
	char *module;
	char *source_name;
	asn1typed_primitive_kind_e primitive_kind;
} asn1typed_type_ref_t;

typedef enum asn1typed_criticality_e {
	ASN1TYPED_CRITICALITY_REJECT,
	ASN1TYPED_CRITICALITY_IGNORE,
	ASN1TYPED_CRITICALITY_NOTIFY
} asn1typed_criticality_e;

/* Codec metadata only. A NULL symbolic_id denotes an ordinary ASN.1 field. */
typedef struct asn1typed_ioc_metadata_s {
	char *symbolic_id;
	asn1typed_criticality_e criticality;
	int has_numeric_id;
	intmax_t numeric_id;
} asn1typed_ioc_metadata_t;

typedef struct asn1typed_field_s {
	char *source_name;
	asn1typed_type_ref_t type;
	asn1typed_presence_e presence;
	asn1typed_source_location_t location;
	asn1typed_ioc_metadata_t ioc;
} asn1typed_field_t;

typedef struct asn1typed_enum_item_s {
	char *source_name;
	asn1typed_source_location_t location;
} asn1typed_enum_item_t;

typedef struct asn1typed_type_s {
	asn1typed_type_identity_t identity;
	asn1typed_type_kind_e kind;
	asn1typed_primitive_kind_e primitive_kind;
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
int asn1typed_type_ref_init_primitive(asn1typed_type_ref_t *ref,
		asn1typed_primitive_kind_e primitive_kind);
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
int asn1typed_type_set_primitive(asn1typed_type_t *type,
		asn1typed_primitive_kind_e primitive_kind);
int asn1typed_type_set_element_primitive(asn1typed_type_t *type,
		asn1typed_primitive_kind_e primitive_kind);
int asn1typed_type_add_primitive_field(asn1typed_type_t *type,
		const char *source_name, asn1typed_primitive_kind_e primitive_kind,
		asn1typed_presence_e presence, const char *file, unsigned line);
int asn1typed_type_add_enum_item(asn1typed_type_t *type,
		const char *source_name, const char *file, unsigned line);
void asn1typed_type_clear(asn1typed_type_t *type);
int asn1typed_field_set_ioc(asn1typed_field_t *field,
		const char *symbolic_id, asn1typed_criticality_e criticality,
		int has_numeric_id, intmax_t numeric_id);

#ifdef __cplusplus
}
#endif

#endif
