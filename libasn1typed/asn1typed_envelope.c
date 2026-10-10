#include "asn1typed_envelope.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int text(const char *s) { return s && *s; }
static char *dup(const char *s) {
    char *p;
    if(!s) return NULL;
    p = malloc(strlen(s) + 1);
    if(p) strcpy(p, s);
    return p;
}
static int named(const asn1typed_type_ref_t *r) {
    return r && r->kind == ASN1TYPED_REF_NAMED && text(r->module) && text(r->source_name)
        && r->primitive_kind == ASN1TYPED_PRIMITIVE_INVALID && !r->actuals && !r->actual_count;
}
static int empty(const asn1typed_type_ref_t *r) {
    return !r->module && !r->source_name && !r->actuals && !r->actual_count
        && r->kind == 0 && r->primitive_kind == ASN1TYPED_PRIMITIVE_INVALID;
}
static int ref_copy(asn1typed_type_ref_t *d, const asn1typed_type_ref_t *s) {
    memset(d, 0, sizeof(*d));
    if(empty(s)) return 0;
    if(!named(s)) return -1;
    d->kind = s->kind;
    d->module = dup(s->module); d->source_name = dup(s->source_name);
    if(!d->module || !d->source_name) { asn1typed_type_ref_clear(d); return -1; }
    return 0;
}
static int storage(size_t n, size_t capacity, const void *p) { return n <= capacity && (capacity ? p != NULL : p == NULL); }
static void header_clear(asn1typed_envelope_header_t *h) {
    size_t i;
    asn1typed_type_ref_clear(&h->pdu); asn1typed_type_ref_clear(&h->procedure_class);
    asn1typed_type_ref_clear(&h->object_set); asn1typed_type_ref_clear(&h->procedure_type);
    asn1typed_type_ref_clear(&h->criticality_type);
    for(i = 0; i < 3; ++i) free(h->criticalities[i].source_name);
    memset(h, 0, sizeof(*h));
}
static int header_copy(asn1typed_envelope_header_t *d, const asn1typed_envelope_header_t *s) {
    size_t i;
    *d = *s;
    memset(&d->pdu, 0, sizeof(d->pdu)); memset(&d->procedure_class, 0, sizeof(d->procedure_class));
    memset(&d->object_set, 0, sizeof(d->object_set)); memset(&d->procedure_type, 0, sizeof(d->procedure_type));
    memset(&d->criticality_type, 0, sizeof(d->criticality_type));
    for(i = 0; i < 3; ++i) d->criticalities[i].source_name = NULL;
    if(ref_copy(&d->pdu, &s->pdu) || ref_copy(&d->procedure_class, &s->procedure_class)
        || ref_copy(&d->object_set, &s->object_set) || ref_copy(&d->procedure_type, &s->procedure_type)
        || ref_copy(&d->criticality_type, &s->criticality_type)) goto fail;
    for(i = 0; i < 3; ++i) if(s->criticalities[i].source_name && !(d->criticalities[i].source_name = dup(s->criticalities[i].source_name))) goto fail;
    return 0;
fail: header_clear(d); return -1;
}
static void root_clear(asn1typed_envelope_root_t *r) {
    size_t i;
    free(r->source_name); asn1typed_type_ref_clear(&r->sequence); asn1typed_type_ref_clear(&r->procedure_class);
    asn1typed_type_ref_clear(&r->object_set); asn1typed_type_ref_clear(&r->procedure_type); asn1typed_type_ref_clear(&r->criticality_type);
    for(i = 0; i < 3; ++i) { free(r->field_names[i]); free(r->class_field_names[i]); }
    for(i = 0; i < 2; ++i) free(r->selectors[i]);
    memset(r, 0, sizeof(*r));
}
static int root_copy(asn1typed_envelope_root_t *d, const asn1typed_envelope_root_t *s) {
    size_t i;
    *d = *s; d->source_name = NULL;
    memset(&d->sequence, 0, sizeof(d->sequence)); memset(&d->procedure_class, 0, sizeof(d->procedure_class));
    memset(&d->object_set, 0, sizeof(d->object_set)); memset(&d->procedure_type, 0, sizeof(d->procedure_type));
    memset(&d->criticality_type, 0, sizeof(d->criticality_type));
    for(i = 0; i < 3; ++i) d->field_names[i] = d->class_field_names[i] = NULL;
    for(i = 0; i < 2; ++i) d->selectors[i] = NULL;
    if(s->source_name && !(d->source_name = dup(s->source_name))) goto fail;
    if(ref_copy(&d->sequence, &s->sequence) || ref_copy(&d->procedure_class, &s->procedure_class)
        || ref_copy(&d->object_set, &s->object_set) || ref_copy(&d->procedure_type, &s->procedure_type)
        || ref_copy(&d->criticality_type, &s->criticality_type)) goto fail;
    for(i = 0; i < 3; ++i) {
        if(s->field_names[i] && !(d->field_names[i] = dup(s->field_names[i]))) goto fail;
        if(s->class_field_names[i] && !(d->class_field_names[i] = dup(s->class_field_names[i]))) goto fail;
    }
    for(i = 0; i < 2; ++i) if(s->selectors[i] && !(d->selectors[i] = dup(s->selectors[i]))) goto fail;
    return 0;
fail: root_clear(d); return -1;
}
static void row_clear(asn1typed_envelope_row_t *r) {
    size_t i;
    free(r->symbolic_code);
    for(i = 0; i < 3; ++i) asn1typed_type_ref_clear(&r->payloads[i]);
    memset(r, 0, sizeof(*r));
}
static int row_copy(asn1typed_envelope_row_t *d, const asn1typed_envelope_row_t *s) {
    size_t i;
    *d = *s; d->symbolic_code = NULL;
    memset(d->payloads, 0, sizeof(d->payloads));
    if(s->symbolic_code && !(d->symbolic_code = dup(s->symbolic_code))) goto fail;
    for(i = 0; i < 3; ++i) if(ref_copy(&d->payloads[i], &s->payloads[i])) goto fail;
    return 0;
fail: row_clear(d); return -1;
}
static void invalidate(asn1typed_target_envelope_t *d) {
    size_t i;
    d->has_valid_envelope = 0; d->target_row_index = d->target_root_ordinal = 0;
    if(d->root_count <= 3 && storage(d->root_count, d->root_capacity, d->roots))
        for(i = 0; i < d->root_count; ++i) { d->roots[i].has_per_root_index = 0; d->roots[i].per_root_index = 0; }
}
void asn1typed_target_envelope_clear(asn1typed_target_envelope_t *d) {
    size_t i;
    if(!d) return;
    header_clear(&d->header);
    for(i = 0; i < d->root_count; ++i) root_clear(&d->roots[i]);
    for(i = 0; i < d->row_count; ++i) row_clear(&d->rows[i]);
    free(d->roots); free(d->rows); asn1typed_type_ref_clear(&d->target_body);
    memset(d, 0, sizeof(*d));
}
int asn1typed_target_envelope_set_header(asn1typed_target_envelope_t *d, const asn1typed_envelope_header_t *s) {
    asn1typed_envelope_header_t p = {0};
    if(!d || !s || !storage(d->root_count, d->root_capacity, d->roots)
        || !storage(d->row_count, d->row_capacity, d->rows) || d->root_count > 3 || d->row_count > 256 || header_copy(&p, s)) return -1;
    header_clear(&d->header); d->header = p; invalidate(d); return 0;
}
int asn1typed_target_envelope_add_root(asn1typed_target_envelope_t *d, const asn1typed_envelope_root_t *s) {
    asn1typed_envelope_root_t p = {0}, *all;
    if(!d || !s || !storage(d->root_count, d->root_capacity, d->roots) || !storage(d->row_count, d->row_capacity, d->rows)
        || d->root_count >= 3 || d->row_count > 256 || root_copy(&p, s)) return -1;
    all = realloc(d->roots, (d->root_count + 1) * sizeof(*all));
    if(!all) { root_clear(&p); return -1; }
    d->roots = all; all[d->root_count++] = p; d->root_capacity = d->root_count; invalidate(d); return 0;
}
int asn1typed_target_envelope_add_row(asn1typed_target_envelope_t *d, const asn1typed_envelope_row_t *s) {
    asn1typed_envelope_row_t p = {0}, *all;
    if(!d || !s || !storage(d->root_count, d->root_capacity, d->roots) || !storage(d->row_count, d->row_capacity, d->rows)
        || d->root_count > 3 || d->row_count >= 256 || row_copy(&p, s)) return -1;
    all = realloc(d->rows, (d->row_count + 1) * sizeof(*all));
    if(!all) { row_clear(&p); return -1; }
    d->rows = all; all[d->row_count++] = p; d->row_capacity = d->row_count; invalidate(d); return 0;
}
int asn1typed_target_envelope_set_target(asn1typed_target_envelope_t *d, const asn1typed_type_ref_t *s) {
    asn1typed_type_ref_t p = {0};
    if(!d || !s || !storage(d->root_count, d->root_capacity, d->roots)
        || !storage(d->row_count, d->row_capacity, d->rows) || d->root_count > 3 || d->row_count > 256 || ref_copy(&p, s)) return -1;
    asn1typed_type_ref_clear(&d->target_body); d->target_body = p; invalidate(d); return 0;
}
static int check(const asn1typed_target_envelope_t *d, int published, size_t indexes[3], size_t *target_row, size_t *target_root, char *error, size_t size) {
    static const char *const role_names[3] = {"InitiatingMessage", "SuccessfulOutcome", "UnsuccessfulOutcome"};
    static const char *const crit_names[3] = {"reject", "ignore", "notify"};
    const asn1typed_envelope_header_t *h;
    size_t i, j, k, matches = 0;
    unsigned roles = 0, criticalities = 0;
#define BAD(message) do { if(error && size) snprintf(error, size, "%s", message); return -1; } while(0)
    if(error && size) error[0] = 0;
    if(!d || !storage(d->root_count, d->root_capacity, d->roots) || !storage(d->row_count, d->row_capacity, d->rows)) BAD("envelope malformed storage");
    h = &d->header;
    if(h->evidence != ASN1TYPED_WIRE_EVIDENCE_RESOLVED || (published && d->has_valid_envelope != 1)) BAD("envelope evidence unavailable or stale");
    if(!named(&h->pdu) || !named(&h->procedure_class) || !named(&h->object_set) || !named(&h->procedure_type) || !named(&h->criticality_type) || !named(&d->target_body)) BAD("envelope identity unavailable");
    if(d->root_count != 3 || h->declared_root_count != 3 || h->known_addition_count || h->choice_is_extensible != 1
        || h->declared_row_count != d->row_count || !d->row_count || d->row_count > 256
        || (h->object_set_is_extensible != 0 && h->object_set_is_extensible != 1)) BAD("envelope declaration boundary unsupported");
    if(h->procedure_is_extensible || h->criticality_is_extensible || h->procedure_lower_bound != 0 || h->procedure_upper_bound != 255) BAD("envelope scalar domain unsupported");
    if(h->payload_optional[0] != 0 || h->payload_optional[1] != 1 || h->payload_optional[2] != 1
        || h->has_class_default != 1 || h->class_default_criticality > 2) BAD("envelope class OPTIONAL/DEFAULT evidence unavailable");
    for(i = 0; i < 3; ++i) {
        const asn1typed_envelope_criticality_t *c = &h->criticalities[i];
        if(!text(c->source_name) || c->source_ordinal != i || c->assigned_number < 0 || c->assigned_number > 2
            || c->per_index != (size_t)c->assigned_number || strcmp(c->source_name, crit_names[c->per_index]) || (criticalities & (1u << c->per_index))) BAD("envelope criticality mapping invalid");
        criticalities |= 1u << c->per_index;
    }
    for(i = 0; i < 3; ++i) {
        const asn1typed_envelope_root_t *r = &d->roots[i];
        if(!text(r->source_name) || !named(&r->sequence) || r->source_ordinal != i || r->role < 0 || r->role > ASN1TYPED_ENVELOPE_UNSUCCESSFUL
            || (roles & (1u << r->role)) || r->evidence != ASN1TYPED_WIRE_EVIDENCE_RESOLVED || r->field_count != 3 || r->sequence_is_extensible) BAD("envelope root structure unsupported");
        roles |= 1u << r->role;
        if(!named(&r->procedure_class) || !named(&r->object_set) || !named(&r->procedure_type) || !named(&r->criticality_type)
            || !asn1typed_type_ref_equal(&r->procedure_class, &h->procedure_class) || !asn1typed_type_ref_equal(&r->object_set, &h->object_set)
            || !asn1typed_type_ref_equal(&r->procedure_type, &h->procedure_type) || !asn1typed_type_ref_equal(&r->criticality_type, &h->criticality_type)) BAD("envelope root class/set/scalar mismatch");
        for(j = 0; j < 3; ++j) {
            if(!text(r->field_names[j]) || !text(r->class_field_names[j]) || r->role_ordinals[j] != j
                || strcmp(r->class_field_names[j], j == 0 ? "procedureCode" : j == 1 ? "criticality" : role_names[r->role])) BAD("envelope physical class roles invalid");
            for(k = 0; k < j; ++k) if(!strcmp(r->field_names[j], r->field_names[k])) BAD("envelope duplicate physical field name");
        }
        for(j = 0; j < 2; ++j) if(!text(r->selectors[j]) || strcmp(r->selectors[j], r->field_names[0])) BAD("envelope selector provenance invalid");
        if(r->effective_tag_class <= ASN1TYPED_TAG_CLASS_UNKNOWN || r->effective_tag_class > ASN1TYPED_TAG_CLASS_PRIVATE || r->effective_tag_number < 0) BAD("envelope effective tag unavailable");
        indexes[i] = 0;
        for(j = 0; j < 3; ++j) if(j != i) {
            const asn1typed_envelope_root_t *other = &d->roots[j];
            if(!text(other->source_name) || !named(&other->sequence)) BAD("envelope root identity unavailable");
            if(!strcmp(r->source_name, other->source_name) || asn1typed_type_ref_equal(&r->sequence, &other->sequence)) BAD("envelope duplicate root identity");
            if(r->effective_tag_class == other->effective_tag_class && r->effective_tag_number == other->effective_tag_number) BAD("envelope duplicate effective tag");
            if(other->effective_tag_class < r->effective_tag_class || (other->effective_tag_class == r->effective_tag_class && other->effective_tag_number < r->effective_tag_number)) ++indexes[i];
        }
        if(published && (r->has_per_root_index != 1 || r->per_root_index != indexes[i])) BAD("envelope root mapping stale");
    }
    for(i = 0; i < d->row_count; ++i) {
        const asn1typed_envelope_row_t *r = &d->rows[i];
        if(r->has_numeric_code != 1 || r->numeric_code < 0 || r->numeric_code > 255 || (r->symbolic_code && !*r->symbolic_code)
            || r->expected_criticality > 2 || r->default_provenance < ASN1TYPED_ENVELOPE_DEFAULT_UNAVAILABLE || r->default_provenance > ASN1TYPED_ENVELOPE_DEFAULT_CLASS) BAD("envelope procedure row scalar invalid");
        if(r->default_provenance == ASN1TYPED_ENVELOPE_DEFAULT_CLASS && r->expected_criticality != h->class_default_criticality) BAD("envelope class default row mismatch");
        if(r->payload_present[0] != 1) BAD("envelope initiating payload absent");
        for(j = 0; j < 3; ++j) {
            if((r->payload_present[j] != 0 && r->payload_present[j] != 1) || (r->payload_present[j] ? !named(&r->payloads[j]) : !empty(&r->payloads[j]))) BAD("envelope procedure payload presence inconsistent");
        }
        for(j = 0; j < i; ++j) if(r->numeric_code == d->rows[j].numeric_code) BAD("envelope duplicate procedure code");
        for(j = 0; j < 3; ++j) if(r->payload_present[j]
            && asn1typed_type_ref_equal(&r->payloads[j], &d->target_body)) {
            ++matches; *target_row = i;
            for(k = 0; k < 3; ++k) if(d->roots[k].role == (asn1typed_envelope_role_e)j) *target_root = k;
        }
    }
    if(matches != 1) BAD("envelope target association missing or ambiguous");
    if(published && (d->target_row_index != *target_row || d->target_root_ordinal != *target_root)) BAD("envelope target association stale");
    return 0;
#undef BAD
}
asn1typed_wire_finalize_result_e asn1typed_target_envelope_finalize(asn1typed_target_envelope_t *d, char *error, size_t size) {
    size_t indexes[3], row = 0, root = 0, i;
    if(!d) return ASN1TYPED_WIRE_FINALIZE_ERROR;
    invalidate(d);
    if(!storage(d->root_count, d->root_capacity, d->roots) || !storage(d->row_count, d->row_capacity, d->rows)
        || d->root_count > 3 || d->row_count > 256) {
        if(error && size) snprintf(error, size, "envelope malformed storage");
        return ASN1TYPED_WIRE_FINALIZE_ERROR;
    }
    if(check(d, 0, indexes, &row, &root, error, size)) return ASN1TYPED_WIRE_FINALIZE_UNAVAILABLE;
    for(i = 0; i < 3; ++i) { d->roots[i].has_per_root_index = 1; d->roots[i].per_root_index = indexes[i]; }
    d->target_row_index = row; d->target_root_ordinal = root; d->has_valid_envelope = 1;
    return ASN1TYPED_WIRE_FINALIZE_OK;
}
int asn1typed_target_envelope_validate(const asn1typed_target_envelope_t *d, char *error, size_t size) {
    size_t indexes[3], row = 0, root = 0;
    return check(d, 1, indexes, &row, &root, error, size);
}
int asn1typed_target_envelope_copy(asn1typed_target_envelope_t *d, const asn1typed_target_envelope_t *s) {
    asn1typed_target_envelope_t p = {0};
    size_t i;
    if(!d || !s || d == s) return -1;
    memset(d, 0, sizeof(*d));
    if(!storage(s->root_count, s->root_capacity, s->roots) || !storage(s->row_count, s->row_capacity, s->rows)
        || s->root_count > 3 || s->row_count > 256 || (s->has_valid_envelope && asn1typed_target_envelope_validate(s, NULL, 0))
        || asn1typed_target_envelope_set_header(&p, &s->header) || asn1typed_target_envelope_set_target(&p, &s->target_body)) goto fail;
    for(i = 0; i < s->root_count; ++i) if(asn1typed_target_envelope_add_root(&p, &s->roots[i])) goto fail;
    for(i = 0; i < s->row_count; ++i) if(asn1typed_target_envelope_add_row(&p, &s->rows[i])) goto fail;
    if(s->has_valid_envelope && asn1typed_target_envelope_finalize(&p, NULL, 0) != ASN1TYPED_WIRE_FINALIZE_OK) goto fail;
    *d = p; return 0;
fail: asn1typed_target_envelope_clear(&p); return -1;
}
