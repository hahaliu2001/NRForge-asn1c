#include "asn1typed_envelope.h"
#include "asn1typed_extract.h"
#include <asn1fix.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define REQUIRE(x) do { if(!(x)) { fprintf(stderr,"%s:%d: %s\n",__FILE__,__LINE__,#x); abort(); } } while(0)
#ifndef ENVELOPE_EVIDENCE_FIXTURE
#define ENVELOPE_EVIDENCE_FIXTURE "fixtures/envelope-evidence-n11.asn1"
#endif
static long countdown = -1;
void *n11_test_malloc(size_t);
void *n11_test_calloc(size_t,size_t);
void *n11_test_realloc(void *,size_t);
static int fails(void) {
    if(countdown < 0) return 0;
    if(!countdown) { countdown = -1; return 1; }
    --countdown; return 0;
}
void *n11_test_malloc(size_t n) { return fails() ? NULL : malloc(n); }
void *n11_test_calloc(size_t n,size_t z) { return fails() ? NULL : calloc(n,z); }
void *n11_test_realloc(void *p,size_t n) { return fails() ? NULL : realloc(p,n); }
static char diagnostic[512];
static void valid(const asn1typed_target_envelope_t *descriptor) {
    REQUIRE(descriptor->has_valid_envelope);
    REQUIRE(asn1typed_target_envelope_validate(descriptor,diagnostic,sizeof(diagnostic)) == 0);
}
static void invalid(const asn1typed_target_envelope_t *descriptor) {
    REQUIRE(asn1typed_target_envelope_validate(descriptor,diagnostic,sizeof(diagnostic)) == -1 && diagnostic[0]);
}
static void check_owned(const asn1typed_target_envelope_t *descriptor) {
    size_t i;
    valid(descriptor);
    REQUIRE(descriptor->root_count == 3 && descriptor->row_count == 3);
    REQUIRE(descriptor->header.choice_is_extensible && !descriptor->header.known_addition_count);
    REQUIRE(descriptor->header.object_set_is_extensible == 1);
    REQUIRE(descriptor->header.procedure_lower_bound == 0 && descriptor->header.procedure_upper_bound == 255);
    REQUIRE(!descriptor->header.procedure_is_extensible && !descriptor->header.criticality_is_extensible);
    REQUIRE(descriptor->header.has_class_default && descriptor->header.class_default_criticality == 1);
    REQUIRE(descriptor->header.payload_optional[0] == 0 && descriptor->header.payload_optional[1] == 1 && descriptor->header.payload_optional[2] == 1);
    REQUIRE(descriptor->target_row_index == 1 && descriptor->target_root_ordinal == 1);
    REQUIRE(!strcmp(descriptor->target_body.module,"EnvelopeBodies") && !strcmp(descriptor->target_body.source_name,"TargetBody"));
    REQUIRE(descriptor->roots[0].role == ASN1TYPED_ENVELOPE_SUCCESSFUL && descriptor->roots[0].per_root_index == 2);
    REQUIRE(descriptor->roots[1].role == ASN1TYPED_ENVELOPE_INITIATING && descriptor->roots[1].per_root_index == 1);
    REQUIRE(descriptor->roots[2].role == ASN1TYPED_ENVELOPE_UNSUCCESSFUL && descriptor->roots[2].per_root_index == 0);
    for(i = 0; i < 3; ++i) {
        const asn1typed_envelope_root_t *root = &descriptor->roots[i];
        REQUIRE(root->source_ordinal == i && root->has_per_root_index && root->field_count == 3 && !root->sequence_is_extensible);
        REQUIRE(!strcmp(root->field_names[0],"number") && !strcmp(root->field_names[1],"receivedPolicy") && !strcmp(root->field_names[2],"content"));
        REQUIRE(root->role_ordinals[0] == 0 && root->role_ordinals[1] == 1 && root->role_ordinals[2] == 2);
        REQUIRE(!strcmp(root->selectors[0],"number") && !strcmp(root->selectors[1],"number"));
        REQUIRE(root->effective_tag_class == ASN1TYPED_TAG_CLASS_CONTEXT_SPECIFIC);
    }
    REQUIRE(descriptor->rows[0].numeric_code == 9 && descriptor->rows[1].numeric_code == 73 && descriptor->rows[2].numeric_code == 2);
    REQUIRE(descriptor->rows[0].default_provenance == ASN1TYPED_ENVELOPE_DEFAULT_CLASS && descriptor->rows[0].expected_criticality == 1);
    REQUIRE(descriptor->rows[1].default_provenance == ASN1TYPED_ENVELOPE_DEFAULT_EXPLICIT && descriptor->rows[1].expected_criticality == 0);
    REQUIRE(descriptor->rows[2].expected_criticality == 2);
    REQUIRE(descriptor->rows[0].payload_present[0] && !descriptor->rows[0].payload_present[1] && !descriptor->rows[0].payload_present[2]);
    REQUIRE(descriptor->rows[1].payload_present[0] && descriptor->rows[1].payload_present[1] && !descriptor->rows[1].payload_present[2]);
    REQUIRE(descriptor->rows[2].payload_present[0] && descriptor->rows[2].payload_present[1] && descriptor->rows[2].payload_present[2]);
}
static void mutations(asn1typed_target_envelope_t *descriptor) {
    asn1typed_target_envelope_t copy = {0};
    asn1typed_envelope_root_t root;
    asn1typed_envelope_row_t row;
    REQUIRE(asn1typed_target_envelope_copy(&copy,descriptor) == 0);
    REQUIRE(copy.roots != descriptor->roots && copy.rows != descriptor->rows);
    REQUIRE(copy.header.pdu.source_name != descriptor->header.pdu.source_name);
    REQUIRE(copy.roots[0].field_names[0] != descriptor->roots[0].field_names[0]);
    REQUIRE(copy.rows[0].payloads[0].source_name != descriptor->rows[0].payloads[0].source_name);
    check_owned(&copy);
    root = copy.roots[0]; copy.roots[0].effective_tag_number = copy.roots[1].effective_tag_number;
    invalid(&copy); copy.roots[0] = root;
    copy.roots[0].per_root_index = 0; invalid(&copy); copy.roots[0] = root;
    copy.roots[0].sequence_is_extensible = 1; invalid(&copy); copy.roots[0] = root;
    copy.roots[0].selectors[0] = "missing"; invalid(&copy); copy.roots[0] = root;
    copy.roots[0].role_ordinals[2] = 1; invalid(&copy); copy.roots[0] = root;
    row = copy.rows[0]; copy.rows[0].numeric_code = copy.rows[1].numeric_code;
    invalid(&copy); copy.rows[0] = row;
    copy.rows[0].default_provenance = ASN1TYPED_ENVELOPE_DEFAULT_UNAVAILABLE;
    valid(&copy); copy.rows[0] = row; // Resolved value does not invent provenance.
    copy.rows[0].expected_criticality = 0; invalid(&copy); copy.rows[0] = row;
    copy.rows[0].payload_present[1] = 1; invalid(&copy); copy.rows[0] = row;
    copy.header.procedure_upper_bound = 256; invalid(&copy); copy.header.procedure_upper_bound = 255;
    copy.header.has_class_default = 0; invalid(&copy); copy.header.has_class_default = 1;
    copy.header.object_set_is_extensible = 0;
    valid(&copy); // A closed full registry is valid evidence, not unknown codec support.
    copy.header.object_set_is_extensible = 1;
    copy.target_row_index = 0; invalid(&copy); copy.target_row_index = 1;
    valid(&copy);
    {
        char *source = copy.roots[1].source_name;
        asn1typed_type_actual_t actual = {0};
        copy.roots[1].source_name = NULL; invalid(&copy);
        REQUIRE(asn1typed_target_envelope_finalize(&copy,diagnostic,sizeof(diagnostic)) != ASN1TYPED_WIRE_FINALIZE_OK);
        copy.roots[1].source_name = source;
        REQUIRE(asn1typed_target_envelope_finalize(&copy,diagnostic,sizeof(diagnostic)) == ASN1TYPED_WIRE_FINALIZE_OK);
        copy.roots[0].procedure_type.primitive_kind = ASN1TYPED_PRIMITIVE_BOOLEAN; invalid(&copy);
        REQUIRE(asn1typed_target_envelope_finalize(&copy,diagnostic,sizeof(diagnostic)) != ASN1TYPED_WIRE_FINALIZE_OK);
        copy.roots[0].procedure_type.primitive_kind = ASN1TYPED_PRIMITIVE_INVALID;
        REQUIRE(asn1typed_target_envelope_finalize(&copy,diagnostic,sizeof(diagnostic)) == ASN1TYPED_WIRE_FINALIZE_OK);
        copy.roots[0].object_set.actuals = &actual; invalid(&copy);
        REQUIRE(asn1typed_target_envelope_finalize(&copy,diagnostic,sizeof(diagnostic)) != ASN1TYPED_WIRE_FINALIZE_OK);
        copy.roots[0].object_set.actuals = NULL;
        REQUIRE(asn1typed_target_envelope_finalize(&copy,diagnostic,sizeof(diagnostic)) == ASN1TYPED_WIRE_FINALIZE_OK);
    }
    {
        asn1typed_envelope_root_t *roots = copy.roots;
        asn1typed_envelope_row_t *rows = copy.rows;
        size_t capacity;
        copy.roots = NULL;
        REQUIRE(asn1typed_target_envelope_finalize(&copy,diagnostic,sizeof(diagnostic)) == ASN1TYPED_WIRE_FINALIZE_ERROR);
        copy.roots = roots;
        copy.rows = NULL;
        REQUIRE(asn1typed_target_envelope_finalize(&copy,diagnostic,sizeof(diagnostic)) == ASN1TYPED_WIRE_FINALIZE_ERROR);
        copy.rows = rows;
        capacity = copy.root_capacity; copy.root_capacity = 2;
        REQUIRE(asn1typed_target_envelope_finalize(&copy,diagnostic,sizeof(diagnostic)) == ASN1TYPED_WIRE_FINALIZE_ERROR);
        copy.root_capacity = capacity;
        copy.root_count = SIZE_MAX;
        REQUIRE(asn1typed_target_envelope_finalize(&copy,diagnostic,sizeof(diagnostic)) == ASN1TYPED_WIRE_FINALIZE_ERROR);
        copy.root_count = 3;
        copy.row_count = SIZE_MAX;
        REQUIRE(asn1typed_target_envelope_finalize(&copy,diagnostic,sizeof(diagnostic)) == ASN1TYPED_WIRE_FINALIZE_ERROR);
        copy.row_count = 3;
        REQUIRE(asn1typed_target_envelope_finalize(&copy,diagnostic,sizeof(diagnostic)) == ASN1TYPED_WIRE_FINALIZE_OK);
    }
    REQUIRE(asn1typed_target_envelope_set_target(&copy,&copy.target_body) == 0);
    REQUIRE(!copy.has_valid_envelope);
    for(size_t i = 0; i < copy.root_count; ++i) REQUIRE(!copy.roots[i].has_per_root_index);
    REQUIRE(asn1typed_target_envelope_finalize(&copy,diagnostic,sizeof(diagnostic)) == ASN1TYPED_WIRE_FINALIZE_OK);
    check_owned(&copy);
    REQUIRE(asn1typed_target_envelope_set_header(&copy,&copy.header) == 0);
    REQUIRE(!copy.has_valid_envelope);
    REQUIRE(asn1typed_target_envelope_finalize(&copy,diagnostic,sizeof(diagnostic)) == ASN1TYPED_WIRE_FINALIZE_OK);
    REQUIRE(asn1typed_target_envelope_add_row(&copy,&copy.rows[0]) == 0);
    REQUIRE(!copy.has_valid_envelope);
    REQUIRE(asn1typed_target_envelope_finalize(&copy,diagnostic,sizeof(diagnostic)) != ASN1TYPED_WIRE_FINALIZE_OK);
    for(size_t i = 0; i < copy.root_count; ++i) REQUIRE(!copy.roots[i].has_per_root_index);
    asn1typed_target_envelope_clear(&copy);
    check_owned(descriptor);
}
static void allocation_tests(asn1p_t *tree,const asn1typed_target_envelope_t *source) {
    long point;
    for(point = 0; point < 20000; ++point) {
        asn1typed_target_envelope_t out = {0}; int result;
        countdown = point;
        result = asn1typed_extract_target_envelope(tree,"EnvelopeEvidence","Envelope","EnvelopeBodies","TargetBody",&out,diagnostic,sizeof(diagnostic));
        countdown = -1;
        if(!result) { check_owned(&out); asn1typed_target_envelope_clear(&out); break; }
        REQUIRE(result == -1 && diagnostic[0] && !out.roots && !out.rows && !out.header.pdu.source_name && !out.has_valid_envelope);
        asn1typed_target_envelope_clear(&out);
    }
    REQUIRE(point > 0 && point < 20000);
    for(point = 0; point < 20000; ++point) {
        asn1typed_target_envelope_t out = {0}; int result;
        countdown = point; result = asn1typed_target_envelope_copy(&out,source); countdown = -1;
        if(!result) { check_owned(&out); asn1typed_target_envelope_clear(&out); break; }
        REQUIRE(!out.roots && !out.rows && !out.header.pdu.source_name && !out.has_valid_envelope);
        asn1typed_target_envelope_clear(&out);
    }
    REQUIRE(point > 0 && point < 20000);
    for(int operation = 0; operation < 3; ++operation) {
        asn1typed_target_envelope_t out = {0};
        REQUIRE(asn1typed_target_envelope_copy(&out,source) == 0);
        for(point = 0; point < 20000; ++point) {
            asn1typed_envelope_root_t *roots = out.roots;
            asn1typed_envelope_row_t *rows = out.rows;
            char *pdu = out.header.pdu.source_name;
            int result;
            countdown = point;
            if(operation == 0) result = asn1typed_target_envelope_set_header(&out,&source->header);
            else if(operation == 1) result = asn1typed_target_envelope_set_target(&out,&source->target_body);
            else result = asn1typed_target_envelope_add_row(&out,&source->rows[0]);
            countdown = -1;
            if(!result) { REQUIRE(!out.has_valid_envelope); break; }
            REQUIRE(out.roots == roots && out.rows == rows && out.header.pdu.source_name == pdu);
            check_owned(&out);
        }
        REQUIRE(point > 0 && point < 20000);
        asn1typed_target_envelope_clear(&out);
    }
}
static void four_root_evidence(const asn1typed_target_envelope_t *source) {
    asn1typed_target_envelope_t d = {0}, copied = {0};
    asn1typed_envelope_root_t root = {0};
    asn1typed_type_actual_t actual = {ASN1TYPED_ACTUAL_OBJECT_SET_REFERENCE,"EnvelopeEvidence","UnusedIEs"};
    char *saved;
    REQUIRE(!asn1typed_target_envelope_copy(&d, source));
    d.header.choice_is_extensible = 0; d.header.declared_root_count = 4;
    root.source_name = "choice-extension"; root.source_ordinal = 3;
    root.role = ASN1TYPED_ENVELOPE_CHOICE_EXTENSION;
    root.unsupported_payload = 1; root.object_set_is_extensible = 1;
    root.evidence = ASN1TYPED_WIRE_EVIDENCE_RESOLVED;
    root.effective_tag_class = ASN1TYPED_TAG_CLASS_CONTEXT_SPECIFIC;
    root.effective_tag_number = 100; root.field_count = 3;
    root.field_names[0] = "id"; root.field_names[1] = "criticality"; root.field_names[2] = "value";
    root.class_field_names[0] = "id"; root.class_field_names[1] = "criticality"; root.class_field_names[2] = "Value";
    root.role_ordinals[1] = 1; root.role_ordinals[2] = 2;
    root.selectors[0] = root.selectors[1] = "id";
    REQUIRE(!asn1typed_type_ref_init_parameterized(&root.sequence,"Containers","SingleContainer",&actual,1));
    REQUIRE(!asn1typed_type_ref_init(&root.procedure_class,"Containers","IEs"));
    REQUIRE(!asn1typed_type_ref_init(&root.object_set,"EnvelopeEvidence","UnusedIEs"));
    REQUIRE(!asn1typed_target_envelope_add_root(&d,&root));
    asn1typed_type_ref_clear(&root.sequence); asn1typed_type_ref_clear(&root.procedure_class); asn1typed_type_ref_clear(&root.object_set);
    REQUIRE(asn1typed_target_envelope_finalize(&d,diagnostic,sizeof(diagnostic)) == ASN1TYPED_WIRE_FINALIZE_OK);
    valid(&d); REQUIRE(d.roots[3].per_root_index == 3);
    REQUIRE(!asn1typed_target_envelope_copy(&copied,&d));
    REQUIRE(copied.roots[3].sequence.actuals != d.roots[3].sequence.actuals);
    asn1typed_target_envelope_clear(&d); valid(&copied);
    saved = copied.roots[3].sequence.actuals[0].module;
    copied.roots[3].sequence.actuals[0].module = NULL; invalid(&copied);
    copied.roots[3].sequence.actuals[0].module = saved;
    copied.roots[3].sequence.actuals[0].kind = (asn1typed_actual_kind_e)99; invalid(&copied);
    copied.roots[3].sequence.actuals[0].kind = ASN1TYPED_ACTUAL_OBJECT_SET_REFERENCE;
    saved = copied.roots[3].field_names[2]; copied.roots[3].field_names[2] = copied.roots[3].field_names[0]; invalid(&copied);
    copied.roots[3].field_names[2] = saved;
    copied.header.choice_is_extensible = 1; invalid(&copied); copied.header.choice_is_extensible = 0;
    copied.roots[3].declared_row_count = 1; invalid(&copied); copied.roots[3].declared_row_count = 0;
    valid(&copied); asn1typed_target_envelope_clear(&copied);
}
int main(void) {
    asn1p_t *tree = asn1p_parse_file(ENVELOPE_EVIDENCE_FIXTURE,A1P_NOFLAGS);
    asn1typed_target_envelope_t descriptor = {0}, copy = {0}, closed = {0};
    REQUIRE(tree && asn1f_process(tree,A1F_NOFLAGS,NULL) >= 0);
    REQUIRE(asn1typed_extract_target_envelope(tree,"EnvelopeEvidence","Envelope","EnvelopeBodies","TargetBody",&descriptor,diagnostic,sizeof(diagnostic)) == 0);
    REQUIRE(asn1typed_extract_target_envelope(tree,"EnvelopeEvidence","ClosedEnvelope","EnvelopeBodies","TargetBody",&closed,diagnostic,sizeof(diagnostic)) == 0);
    {
        asn1typed_target_envelope_t four = {0};
        int rc = asn1typed_extract_target_envelope(tree,"EnvelopeEvidence","FourEnvelope","EnvelopeBodies","TargetBody",&four,diagnostic,sizeof(diagnostic));
        if(rc) fprintf(stderr,"four-root extraction: %s\n",diagnostic);
        REQUIRE(!rc); valid(&four); REQUIRE(four.root_count == 4 && !four.header.choice_is_extensible);
        REQUIRE(four.roots[3].unsupported_payload && four.roots[3].sequence.actual_count == 1);
        asn1typed_target_envelope_clear(&four);
    }
    allocation_tests(tree,&descriptor);
    REQUIRE(asn1typed_target_envelope_copy(&copy,&descriptor) == 0);
    asn1p_delete(tree); asn1typed_target_envelope_clear(&descriptor);
    valid(&closed); REQUIRE(closed.header.object_set_is_extensible == 0 && closed.row_count == 3 && closed.target_row_index == 1);
    REQUIRE(!strcmp(closed.header.object_set.source_name,"ClosedRows"));
    asn1typed_target_envelope_clear(&closed);
    check_owned(&copy); mutations(&copy); four_root_evidence(&copy);
    asn1typed_target_envelope_clear(&copy);
    REQUIRE(asn1typed_target_envelope_finalize(NULL,diagnostic,sizeof(diagnostic)) == ASN1TYPED_WIRE_FINALIZE_ERROR);
    puts("PASS owned envelope evidence, source tags, default provenance, lifecycle and scoped allocation failures");
    return 0;
}
