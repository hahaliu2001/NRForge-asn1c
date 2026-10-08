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
	ASN1TYPED_TYPE_ENUMERATED,
	ASN1TYPED_TYPE_CHOICE
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
	ASN1TYPED_PRIMITIVE_PRINTABLE_STRING,
	ASN1TYPED_PRIMITIVE_VISIBLE_STRING,
	ASN1TYPED_PRIMITIVE_OCTET_STRING,
	ASN1TYPED_PRIMITIVE_BIT_STRING
} asn1typed_primitive_kind_e;

typedef struct asn1typed_size_constraint_s {
	int has_size_constraint;
	intmax_t lower_bound;
	intmax_t upper_bound;
	int is_extensible;
} asn1typed_size_constraint_t;

typedef struct asn1typed_integer_interval_s {
	intmax_t lower_bound;
	intmax_t upper_bound;
} asn1typed_integer_interval_t;

/* Owned bounded INTEGER permitted set. The first canonical interval is
 * stored inline; tail contains only intervals after that first interval. */
typedef struct asn1typed_integer_value_range_s {
	int has_value_range;
	intmax_t lower_bound;
	intmax_t upper_bound;
	int is_extensible;
	asn1typed_integer_interval_t *tail;
	size_t tail_count;
} asn1typed_integer_value_range_t;

typedef enum asn1typed_ref_kind_e {
	ASN1TYPED_REF_NAMED,
	ASN1TYPED_REF_PRIMITIVE
} asn1typed_ref_kind_e;

typedef enum asn1typed_actual_kind_e {
	ASN1TYPED_ACTUAL_OBJECT_SET_REFERENCE
} asn1typed_actual_kind_e;

typedef struct asn1typed_type_actual_s {
	asn1typed_actual_kind_e kind;
	char *module;
	char *source_name;
} asn1typed_type_actual_t;

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
	asn1typed_type_actual_t *actuals;
	size_t actual_count;
} asn1typed_type_ref_t;

typedef enum asn1typed_field_type_semantics_e {
	ASN1TYPED_FIELD_FIXED_TYPE,
	ASN1TYPED_FIELD_CLASS_FIELD_SELECTED_TYPE,
	/* The field owns an inline ENUMERATED body and has no named reference. */
	ASN1TYPED_FIELD_INLINE_ENUMERATED
} asn1typed_field_type_semantics_e;

/* Owned, target-neutral relation between a field and an information-object
 * class field. actual_index addresses the enclosing bound instance's key; this
 * B7b.2a API does not range-check it. Before B7b.2b attaches/materializes a
 * body on an enclosing bound instance, it must validate
 * actual_index < enclosing_instance.identity.actual_count. */
typedef struct asn1typed_class_field_relation_s {
	char *class_module;
	char *class_source_name;
	char *class_field_source_name;
	size_t actual_index;
	int has_selector;
	char *selector_source_name;
} asn1typed_class_field_relation_t;

typedef enum asn1typed_criticality_e {
	ASN1TYPED_CRITICALITY_REJECT,
	ASN1TYPED_CRITICALITY_IGNORE,
	ASN1TYPED_CRITICALITY_NOTIFY
} asn1typed_criticality_e;

/* Raw IOC identity and codec metadata. NULL denotes an ordinary ASN.1 field.
 * Naming consumers use symbolic_id, preserving the original leading id-.
 */
typedef struct asn1typed_ioc_metadata_s {
	char *symbolic_id;
	asn1typed_criticality_e criticality;
	int has_numeric_id;
	intmax_t numeric_id;
} asn1typed_ioc_metadata_t;

typedef struct asn1typed_type_s asn1typed_type_t;

typedef struct asn1typed_field_s {
	/* IOC compatibility: T3's identity with one conventional id- removed.
	 * New naming consumers should use ioc.symbolic_id for IOC fields.
	 */
	char *source_name;
	asn1typed_field_type_semantics_e type_semantics;
	asn1typed_type_ref_t type;
	/* Inline SIZE semantics owned by this SEQUENCE use-site, when present. */
	asn1typed_size_constraint_t size_constraint;
	/* Inline primitive INTEGER permitted set owned by this SEQUENCE use-site. */
	asn1typed_integer_value_range_t value_range;
	/* Owned inline ENUMERATED body for this SEQUENCE field, when present. */
	asn1typed_type_t *inline_enumerated;
	int has_class_field_relation;
	asn1typed_class_field_relation_t class_field_relation;
	asn1typed_presence_e presence;
	asn1typed_source_location_t location;
	asn1typed_ioc_metadata_t ioc;
} asn1typed_field_t;

typedef struct asn1typed_enum_item_s {
	char *source_name;
	asn1typed_source_location_t location;
	int is_extension_addition;
} asn1typed_enum_item_t;

typedef struct asn1typed_choice_alternative_s {
	char *source_name;
	asn1typed_type_ref_t type_ref;
	asn1typed_size_constraint_t size_constraint;
	/* Inline primitive INTEGER permitted set owned by this alternative. */
	asn1typed_integer_value_range_t value_range;
	asn1typed_source_location_t location;
} asn1typed_choice_alternative_t;

struct asn1typed_type_s {
	asn1typed_type_identity_t identity;
	asn1typed_type_kind_e kind;
	asn1typed_primitive_kind_e primitive_kind;
	asn1typed_size_constraint_t size_constraint;
	asn1typed_integer_value_range_t value_range;
	asn1typed_source_location_t location;
	asn1typed_field_t *fields;
	size_t field_count;
	size_t field_capacity;
	asn1typed_type_ref_t element_type;
	/* Generic IOC association retained even when the set has no known rows. */
	asn1typed_type_ref_t ioc_container;
	int has_ioc_table;
	int ioc_object_set_is_extensible;
	asn1typed_enum_item_t *enum_items;
	size_t enum_item_count;
	size_t enum_item_capacity;
	int is_extensible;
	asn1typed_choice_alternative_t *alternatives;
	size_t alternative_count;
	size_t alternative_capacity;
};

/* B7b.1 owns the identity before it owns the instance's semantic body. */
typedef struct asn1typed_bound_instance_s {
	asn1typed_type_ref_t identity;
	/* The body's enclosing identity is the bound instance above. Its type
	 * identity fields stay empty; all body contents are independently owned. */
	asn1typed_type_t body;
	int body_materialized;
} asn1typed_bound_instance_t;

typedef struct asn1typed_module_s {
	char *source_name;
	asn1typed_source_location_t location;
	asn1typed_type_t *types;
	size_t type_count;
	size_t type_capacity;
	asn1typed_bound_instance_t *bound_instances;
	size_t bound_instance_count;
	size_t bound_instance_capacity;
} asn1typed_module_t;

/* All string arguments are copied. The caller retains ownership of them. */
int asn1typed_source_location_init(asn1typed_source_location_t *location,
		const char *file, unsigned line);
void asn1typed_source_location_clear(asn1typed_source_location_t *location);
int asn1typed_type_ref_init(asn1typed_type_ref_t *ref,
		const char *module, const char *source_name);
int asn1typed_type_ref_init_primitive(asn1typed_type_ref_t *ref,
		asn1typed_primitive_kind_e primitive_kind);
int asn1typed_type_ref_init_parameterized(asn1typed_type_ref_t *ref,
		const char *module, const char *source_name,
		const asn1typed_type_actual_t *actuals, size_t actual_count);
int asn1typed_type_ref_copy(asn1typed_type_ref_t *ref,
		const asn1typed_type_ref_t *source);
int asn1typed_type_ref_equal(const asn1typed_type_ref_t *left,
		const asn1typed_type_ref_t *right);
void asn1typed_type_ref_clear(asn1typed_type_ref_t *ref);
int asn1typed_module_init(asn1typed_module_t *module,
		const char *source_name, const char *file, unsigned line);
void asn1typed_module_clear(asn1typed_module_t *module);
int asn1typed_module_add_bound_instance(asn1typed_module_t *module,
		const asn1typed_type_ref_t *identity,
		asn1typed_bound_instance_t **instance_out);
/* Atomically attach a complete, owned SEQUENCE or supported SEQUENCE OF body
 * to an existing instance. Relations are checked against enclosing actuals
 * and body selectors for SEQUENCE fields. */
int asn1typed_bound_instance_set_body(asn1typed_module_t *module,
		size_t instance_index, asn1typed_type_t *body);
int asn1typed_module_add_type(asn1typed_module_t *module,
		const char *source_name, asn1typed_type_kind_e kind,
		const char *file, unsigned line, asn1typed_type_t **type_out);
int asn1typed_module_add_type_identity(asn1typed_module_t *module,
		const char *module_name, const char *source_name,
		asn1typed_type_kind_e kind, const char *file, unsigned line,
		asn1typed_type_t **type_out);
int asn1typed_type_add_field(asn1typed_type_t *type,
		const char *source_name, const char *ref_module,
		const char *ref_source_name, asn1typed_presence_e presence,
		const char *file, unsigned line);
int asn1typed_type_add_field_ref(asn1typed_type_t *type,
		const char *source_name, const asn1typed_type_ref_t *type_ref,
		asn1typed_presence_e presence, const char *file, unsigned line);
/* type_ref is required for FIXED_TYPE and must be NULL for
 * CLASS_FIELD_SELECTED_TYPE. Relation strings are copied. */
int asn1typed_type_add_class_field(asn1typed_type_t *type,
		const char *source_name,
		asn1typed_field_type_semantics_e type_semantics,
		const asn1typed_type_ref_t *type_ref,
		const asn1typed_class_field_relation_t *relation,
		asn1typed_presence_e presence, const char *file, unsigned line);
/* Deep-copy an existing field into a sequence, including all owned metadata. */
int asn1typed_type_add_field_copy(asn1typed_type_t *type,
		const asn1typed_field_t *source);
int asn1typed_type_add_inline_enumerated_field(asn1typed_type_t *type,
		const char *source_name, const asn1typed_type_t *body,
		asn1typed_presence_e presence, const char *file, unsigned line);
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
int asn1typed_type_add_enum_item_ex(asn1typed_type_t *type,
		const char *source_name, int is_extension_addition,
		const char *file, unsigned line);
int asn1typed_type_add_choice_alternative(asn1typed_type_t *type,
		const char *source_name, const asn1typed_type_ref_t *type_ref,
		const asn1typed_size_constraint_t *size_constraint,
		const asn1typed_integer_value_range_t *value_range,
		const char *file, unsigned line);
void asn1typed_type_clear(asn1typed_type_t *type);
int asn1typed_field_set_ioc(asn1typed_field_t *field,
		const char *symbolic_id, asn1typed_criticality_e criticality,
		int has_numeric_id, intmax_t numeric_id);

#ifdef __cplusplus
}
#endif

#endif
