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

typedef enum asn1typed_tag_default_e {
	ASN1TYPED_TAG_DEFAULT_UNKNOWN,
	ASN1TYPED_TAG_DEFAULT_EXPLICIT,
	ASN1TYPED_TAG_DEFAULT_IMPLICIT,
	ASN1TYPED_TAG_DEFAULT_AUTOMATIC
} asn1typed_tag_default_e;

typedef enum asn1typed_tag_class_e {
	ASN1TYPED_TAG_CLASS_UNKNOWN,
	ASN1TYPED_TAG_CLASS_UNIVERSAL,
	ASN1TYPED_TAG_CLASS_APPLICATION,
	ASN1TYPED_TAG_CLASS_CONTEXT_SPECIFIC,
	ASN1TYPED_TAG_CLASS_PRIVATE
} asn1typed_tag_class_e;

typedef enum asn1typed_wire_evidence_e {
	ASN1TYPED_WIRE_EVIDENCE_UNAVAILABLE,
	ASN1TYPED_WIRE_EVIDENCE_RESOLVED,
	ASN1TYPED_WIRE_EVIDENCE_UNSUPPORTED
} asn1typed_wire_evidence_e;

typedef enum asn1typed_wire_finalize_result_e {
	ASN1TYPED_WIRE_FINALIZE_ERROR = -1,
	ASN1TYPED_WIRE_FINALIZE_OK = 0,
	ASN1TYPED_WIRE_FINALIZE_UNAVAILABLE = 1
} asn1typed_wire_finalize_result_e;

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
	ASN1TYPED_PRIMITIVE_BIT_STRING,
	ASN1TYPED_PRIMITIVE_NULL, /* appended: existing primitive values stay stable */
	/* Canonical BER content octets, lossless including arbitrary-size arcs. */
	ASN1TYPED_PRIMITIVE_OBJECT_IDENTIFIER,
	/* Opaque complete encoding selected by an unknown open-type key. */
	ASN1TYPED_PRIMITIVE_OPEN_TYPE
} asn1typed_primitive_kind_e;

typedef struct asn1typed_size_constraint_s {
	int has_size_constraint;
	intmax_t lower_bound;
	intmax_t upper_bound;
	int is_extensible;
	/* Bounded explicit SIZE extension addition, when present. Root wire
	 * length still uses lower/upper; extension lengths use unconstrained PER. */
	int has_extension_addition;
	intmax_t extension_lower_bound;
	intmax_t extension_upper_bound;
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
	/* Known extension additions; open extension values remain representable. */
	asn1typed_integer_interval_t *extension_additions;
	size_t extension_addition_count;
	/* Tagged finite unsigned interval. Signed slots are zero in this mode;
	 * unsigned sets/extensions are deliberately outside this contract. */
	int unsigned_bounds;
	uint64_t unsigned_lower_bound;
	uint64_t unsigned_upper_bound;
} asn1typed_integer_value_range_t;

static inline int
asn1typed_integer_unsigned_valid(const asn1typed_integer_value_range_t *r) {
    return r && r->has_value_range == 1 && r->unsigned_bounds == 1 &&
        r->unsigned_lower_bound <= r->unsigned_upper_bound &&
        !r->lower_bound && !r->upper_bound && !r->is_extensible &&
        !r->tail && !r->tail_count && !r->extension_additions && !r->extension_addition_count;
}
static inline int
asn1typed_integer_unsigned_empty(const asn1typed_integer_value_range_t *r) {
    return !r->unsigned_bounds && !r->unsigned_lower_bound && !r->unsigned_upper_bound;
}


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
	/* Effective INTEGER permitted set owned by this SEQUENCE use-site; named identity is retained. */
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
	asn1typed_wire_evidence_e numeric_evidence;
	intmax_t assigned_number;
	int has_per_enumeration_index;
	size_t per_enumeration_index;
} asn1typed_enum_item_t;

typedef struct asn1typed_choice_alternative_s {
	/* Owned inline ENUMERATED payload; type_ref must be empty when present. */
	asn1typed_type_t *inline_enumerated;
	char *source_name;
	asn1typed_type_ref_t type_ref;
	asn1typed_size_constraint_t size_constraint;
	/* Effective INTEGER permitted set owned by this alternative; named identity is retained. */
	asn1typed_integer_value_range_t value_range;
	/* Optional owned tag evidence; index is published only by finalization. */
	asn1typed_wire_evidence_e wire_evidence;
	asn1typed_tag_class_e effective_tag_class;
	intmax_t effective_tag_number;
	int has_per_root_index;
	size_t per_root_index;
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
	int has_valid_per_enumeration_mapping;
	int is_extensible;
	/* Explicit physical root-only SEQUENCE structure evidence; extensibility
	 * alone never proves the root/addition boundary. Direct edits need validate. */
	asn1typed_wire_evidence_e sequence_extension_evidence;
	size_t sequence_root_field_count;
	size_t sequence_known_addition_count;
	int has_valid_sequence_extension_structure;
	asn1typed_choice_alternative_t *alternatives;
	size_t alternative_count;
	size_t alternative_capacity;
	/* Set only after every root alternative is resolved and validated. */
	int has_valid_per_root_mapping;
	/* Extraction owns a terminal CHOICE marker; known additions remain rejected. */
	int choice_root_only_extension_owned;
};

/* N9 opt-in physical IOC tables. Zero-initialized evidence is unavailable. */
typedef struct asn1typed_ioc_dispatch_row_s {
	char *symbolic_id;
	int has_numeric_id;
	intmax_t numeric_id;
	asn1typed_type_ref_t payload_type;
	asn1typed_criticality_e criticality;
	int has_presence;
	asn1typed_presence_e presence;
} asn1typed_ioc_dispatch_row_t;

typedef struct asn1typed_ioc_registry_s {
	char *class_module, *class_source_name;
	char *object_set_module, *object_set_source_name;
	char *selected_class_field_source_name;
	asn1typed_ioc_dispatch_row_t *rows;
	size_t row_count, row_capacity;
	asn1typed_wire_evidence_e evidence;
	size_t declared_row_count;
	int object_set_is_extensible;
	int has_valid_dispatch;
} asn1typed_ioc_registry_t;

typedef struct asn1typed_ioc_binding_s {
	asn1typed_wire_evidence_e evidence;
	size_t registry_index;
	size_t id_field_ordinal, criticality_field_ordinal, value_field_ordinal;
	int has_valid_binding;
} asn1typed_ioc_binding_t;

/* B7b.1 owns the identity before it owns the instance's semantic body. */
typedef struct asn1typed_bound_instance_s {
	asn1typed_type_ref_t identity;
	/* The body's enclosing identity is the bound instance above. Its type
	 * identity fields stay empty; all body contents are independently owned. */
	asn1typed_type_t body;
	int body_materialized;
	asn1typed_ioc_binding_t ioc_binding;
	/* Proven empty extensible non-UNIQUE private-key object set. */
	int has_empty_private_binding;
	asn1typed_type_actual_t empty_private_object_set;
} asn1typed_bound_instance_t;

typedef struct asn1typed_module_s {
	char *source_name;
	asn1typed_tag_default_e tag_default;
	asn1typed_source_location_t location;
	asn1typed_type_t *types;
	size_t type_count;
	size_t type_capacity;
	asn1typed_bound_instance_t *bound_instances;
	size_t bound_instance_count;
	size_t bound_instance_capacity;
	asn1typed_ioc_registry_t *ioc_registries;
	size_t ioc_registry_count, ioc_registry_capacity;
} asn1typed_module_t;

/* Registry keys and rows are independently owned. Successful row mutation
 * invalidates declaration evidence; direct edits require validation. */
int asn1typed_module_add_ioc_registry(asn1typed_module_t *, const char *, const char *,
		const char *, const char *, const char *, size_t *);
int asn1typed_ioc_registry_add_row(asn1typed_ioc_registry_t *, const asn1typed_ioc_dispatch_row_t *);
int asn1typed_ioc_registry_set_evidence(asn1typed_ioc_registry_t *, size_t, int);
int asn1typed_ioc_registry_set_unavailable(asn1typed_ioc_registry_t *);
int asn1typed_ioc_registry_set_unsupported(asn1typed_ioc_registry_t *);
asn1typed_wire_finalize_result_e asn1typed_ioc_registry_finalize(asn1typed_ioc_registry_t *, char *, size_t);
int asn1typed_ioc_registry_validate(const asn1typed_ioc_registry_t *, char *, size_t);
/* Only after source evidence establishes an empty extensible private object set.
 * Setter owns the current actual identity transactionally; validation rejects
 * stale identities, unavailable key order and unsupported key domains. */
int asn1typed_bound_instance_set_empty_private_binding(asn1typed_module_t *, size_t);
int asn1typed_bound_instance_empty_private_validate(const asn1typed_module_t *, size_t, char *, size_t);
int asn1typed_bound_instance_set_ioc_binding(asn1typed_module_t *, size_t, size_t, size_t, size_t, size_t);
asn1typed_wire_finalize_result_e asn1typed_bound_instance_ioc_binding_finalize(asn1typed_module_t *, size_t, char *, size_t);
int asn1typed_bound_instance_ioc_binding_validate(const asn1typed_module_t *, size_t, char *, size_t);

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
/* Successful mutations invalidate every published enum index. Direct public
 * edits require validation before use. Missing evidence never implies zero. */
int asn1typed_enum_item_set_numeric_evidence(asn1typed_type_t *, size_t, intmax_t);
int asn1typed_enum_item_set_numeric_unavailable(asn1typed_type_t *, size_t);
int asn1typed_enum_item_set_numeric_unsupported(asn1typed_type_t *, size_t);
/* ERROR includes malformed API/storage, allocation and internal failures;
 * UNAVAILABLE means unusable evidence/semantics. Only OK publishes indexes. */
asn1typed_wire_finalize_result_e asn1typed_enumerated_evidence_finalize(
		asn1typed_type_t *, char *, size_t);
int asn1typed_enumerated_evidence_validate(const asn1typed_type_t *, char *, size_t);
int asn1typed_type_add_choice_alternative(asn1typed_type_t *type,
		const char *source_name, const asn1typed_type_ref_t *type_ref,
		const asn1typed_size_constraint_t *size_constraint,
		const asn1typed_integer_value_range_t *value_range,
		const char *file, unsigned line);
/* Deep-copies owned enum items/evidence; successful addition invalidates selector mapping. */
int asn1typed_type_add_inline_enumerated_alternative(asn1typed_type_t *, const char *, const asn1typed_type_t *, const char *, unsigned);
/* These CHOICE-scoped mutations invalidate the mapping and clear every
 * published alternative index. The public structs can still be modified
 * directly; callers must validate before using wire evidence. */
int asn1typed_choice_alternative_set_wire_evidence(asn1typed_type_t *choice,
		size_t alternative_index, asn1typed_tag_class_e tag_class,
		intmax_t tag_number);
void asn1typed_choice_alternative_set_wire_unavailable(
		asn1typed_type_t *choice, size_t alternative_index);
void asn1typed_choice_alternative_set_wire_unsupported(
		asn1typed_type_t *choice, size_t alternative_index);
/* Finalize publishes indexes only after the entire mapping checks. Returns
 * UNAVAILABLE for incomplete/unsupported/duplicate evidence, ERROR for bad
 * API arguments, and OK on success. */
asn1typed_wire_finalize_result_e asn1typed_choice_wire_evidence_finalize(
		asn1typed_type_t *type, char *error, size_t error_size);
/* Rechecks tags, index uniqueness/continuity/order, and the valid flag. */
int asn1typed_choice_wire_evidence_validate(const asn1typed_type_t *type,
		char *error, size_t error_size);
/* These SEQUENCE-scoped setters do not infer structure. Resolved requires
 * is_extensible == 1, roots == field_count and zero known additions. Misuse
 * leaves state unchanged. Successful field insertions erase the evidence. */
int asn1typed_sequence_set_extension_structure(asn1typed_type_t *, size_t, size_t);
int asn1typed_sequence_set_extension_unavailable(asn1typed_type_t *);
int asn1typed_sequence_set_extension_unsupported(asn1typed_type_t *);
/* Finalize requires explicit RESOLVED evidence; only OK publishes validation.
 * ERROR means invalid kind/storage; UNAVAILABLE means unsupported/missing or
 * inconsistent structure. Validate rechecks all scalar/storage invariants. */
asn1typed_wire_finalize_result_e asn1typed_sequence_extension_structure_finalize(
		asn1typed_type_t *, char *, size_t);
int asn1typed_sequence_extension_structure_validate(const asn1typed_type_t *,
		char *, size_t);
void asn1typed_type_clear(asn1typed_type_t *type);
int asn1typed_field_set_ioc(asn1typed_field_t *field,
		const char *symbolic_id, asn1typed_criticality_e criticality,
		int has_numeric_id, intmax_t numeric_id);

#ifdef __cplusplus
}
#endif

#endif
