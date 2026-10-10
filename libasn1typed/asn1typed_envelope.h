#ifndef ASN1TYPED_ENVELOPE_H
#define ASN1TYPED_ENVELOPE_H
#include "asn1typed.h"
#ifdef __cplusplus
extern "C" {
#endif

/* Separate target-envelope evidence, not an IE dispatch registry or a claim
 * that the referenced procedure bodies have been materialized. */
typedef enum asn1typed_envelope_role_e {
    ASN1TYPED_ENVELOPE_INITIATING,
    ASN1TYPED_ENVELOPE_SUCCESSFUL,
    ASN1TYPED_ENVELOPE_UNSUCCESSFUL,
    /* A physical root alternative, never an outer CHOICE extension addition.
     * Its payload is deliberately unsupported by the target codec. */
    ASN1TYPED_ENVELOPE_CHOICE_EXTENSION
} asn1typed_envelope_role_e;
typedef enum asn1typed_envelope_default_provenance_e {
    ASN1TYPED_ENVELOPE_DEFAULT_UNAVAILABLE,
    ASN1TYPED_ENVELOPE_DEFAULT_EXPLICIT,
    ASN1TYPED_ENVELOPE_DEFAULT_CLASS
} asn1typed_envelope_default_provenance_e;
typedef struct asn1typed_envelope_criticality_s {
    char *source_name;
    intmax_t assigned_number;
    size_t source_ordinal, per_index;
} asn1typed_envelope_criticality_t;
typedef struct asn1typed_envelope_header_s {
    asn1typed_type_ref_t pdu, procedure_class, object_set, procedure_type, criticality_type;
    asn1typed_wire_evidence_e evidence;
    size_t declared_root_count, declared_row_count, known_addition_count;
    int choice_is_extensible, object_set_is_extensible;
    int procedure_is_extensible, criticality_is_extensible;
    intmax_t procedure_lower_bound, procedure_upper_bound;
    asn1typed_envelope_criticality_t criticalities[3];
    int payload_optional[3];
    int has_class_default;
    unsigned class_default_criticality;
} asn1typed_envelope_header_t;
typedef struct asn1typed_envelope_root_s {
    char *source_name;
    asn1typed_type_ref_t sequence, procedure_class, object_set, procedure_type, criticality_type;
    asn1typed_envelope_role_e role;
    char *field_names[3];
    char *class_field_names[3];
    char *selectors[2];
    size_t source_ordinal, role_ordinals[3];
    size_t field_count;
    int sequence_is_extensible;
    asn1typed_wire_evidence_e evidence;
    asn1typed_tag_class_e effective_tag_class;
    intmax_t effective_tag_number;
    int has_per_root_index;
    size_t per_root_index;
    int unsupported_payload;
    int object_set_is_extensible;
    size_t declared_row_count;
} asn1typed_envelope_root_t;
typedef struct asn1typed_envelope_row_s {
    char *symbolic_code;
    int has_numeric_code;
    intmax_t numeric_code;
    unsigned expected_criticality;
    asn1typed_envelope_default_provenance_e default_provenance;
    int payload_present[3];
    asn1typed_type_ref_t payloads[3];
} asn1typed_envelope_row_t;
typedef struct asn1typed_target_envelope_s {
    asn1typed_envelope_header_t header;
    asn1typed_envelope_root_t *roots;
    size_t root_count, root_capacity;
    asn1typed_envelope_row_t *rows;
    size_t row_count, row_capacity;
    asn1typed_type_ref_t target_body;
    int has_valid_envelope;
    /* Unique payload identity across all procedure rows and all three roles.
     * Root ordinal refers to source order, independently of role/PER order. */
    size_t target_row_index, target_root_ordinal;
} asn1typed_target_envelope_t;

/* Fresh or cleared output; source is borrowed and is deep-copied. Successful
 * mutations invalidate all published mapping/target proof. Failure is atomic. */
int asn1typed_target_envelope_set_header(asn1typed_target_envelope_t *, const asn1typed_envelope_header_t *);
int asn1typed_target_envelope_add_root(asn1typed_target_envelope_t *, const asn1typed_envelope_root_t *);
int asn1typed_target_envelope_add_row(asn1typed_target_envelope_t *, const asn1typed_envelope_row_t *);
int asn1typed_target_envelope_set_target(asn1typed_target_envelope_t *, const asn1typed_type_ref_t *);
void asn1typed_target_envelope_clear(asn1typed_target_envelope_t *);
int asn1typed_target_envelope_copy(asn1typed_target_envelope_t *, const asn1typed_target_envelope_t *);
asn1typed_wire_finalize_result_e asn1typed_target_envelope_finalize(asn1typed_target_envelope_t *, char *, size_t);
int asn1typed_target_envelope_validate(const asn1typed_target_envelope_t *, char *, size_t);
#ifdef __cplusplus
}
#endif
#endif
