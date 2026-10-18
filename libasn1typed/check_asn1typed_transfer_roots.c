/* M6.003 bounded NGAP Transfer roots through the physical owned extractor. */
#include "asn1typed_extract.h"
#include <asn1fix.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define REQUIRE(x) do { if(!(x)) { fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #x); abort(); } } while(0)
#ifndef TRANSFER_ROOTS_FIXTURE
#define TRANSFER_ROOTS_FIXTURE "fixtures/ngap-transfer-roots.asn1"
#endif
static long allocation_countdown = -1;
void *n9_test_malloc(size_t);
void *n9_test_calloc(size_t, size_t);
void *n9_test_realloc(void *, size_t);
static int fails(void) {
    if(allocation_countdown < 0) return 0;
    if(!allocation_countdown) { allocation_countdown = -1; return 1; }
    --allocation_countdown;
    return 0;
}
void *n9_test_malloc(size_t n) { return fails() ? NULL : malloc(n); }
void *n9_test_calloc(size_t n, size_t z) { return fails() ? NULL : calloc(n, z); }
void *n9_test_realloc(void *p, size_t n) { return fails() ? NULL : realloc(p, n); }
static char diagnostic[1024];

static const asn1typed_type_t *find(const asn1typed_module_t *m, const char *module, const char *name) {
    size_t i;
    for(i = 0; i < m->type_count; ++i)
        if(!strcmp(m->types[i].identity.module, module) && !strcmp(m->types[i].identity.source_name, name)) return &m->types[i];
    fprintf(stderr, "missing owned type %s.%s\n", module, name);
    REQUIRE(0); return NULL;
}
static size_t instance(const asn1typed_module_t *m, const asn1typed_type_ref_t *ref) {
    size_t i;
    for(i = 0; i < m->bound_instance_count; ++i)
        if(asn1typed_type_ref_equal(ref, &m->bound_instances[i].identity)) return i;
    REQUIRE(0); return 0;
}
static void cleared(const asn1typed_module_t *out) {
    REQUIRE(!out->source_name && !out->types && !out->type_count && !out->bound_instances &&
        !out->bound_instance_count && !out->ioc_registries && !out->ioc_registry_count);
}

typedef struct expected_field_s {
    const char *name, *module, *type;
    asn1typed_presence_e presence;
} expected_field_t;
typedef struct expected_row_s {
    intmax_t id;
    const char *symbol, *payload;
} expected_row_t;

/* Owned root fields, presence, extension evidence and iE-Extensions binding. */
static void transfer_root(const asn1typed_module_t *m, const char *root, const expected_field_t *fields, size_t field_count,
                          const char *set, const expected_row_t *rows, size_t row_count) {
    const asn1typed_type_t *t = &m->types[0];
    const asn1typed_type_ref_t *ext;
    const asn1typed_bound_instance_t *container, *element;
    const asn1typed_ioc_registry_t *registry;
    size_t i, e;
    REQUIRE(!strcmp(m->source_name, "NGAP-IEs"));
    REQUIRE(!strcmp(t->identity.module, "NGAP-IEs") && !strcmp(t->identity.source_name, root));
    REQUIRE(t->kind == ASN1TYPED_TYPE_SEQUENCE && !t->has_ioc_table);
    REQUIRE(t->is_extensible == 1 && t->has_valid_sequence_extension_structure == 1);
    REQUIRE(t->sequence_root_field_count == field_count && t->sequence_known_addition_count == 0);
    REQUIRE(asn1typed_sequence_extension_structure_validate(t, diagnostic, sizeof(diagnostic)) == 0);
    REQUIRE(t->field_count == field_count);
    for(i = 0; i < field_count; ++i) {
        const asn1typed_field_t *f = &t->fields[i];
        REQUIRE(!strcmp(f->source_name, fields[i].name));
        REQUIRE(f->presence == fields[i].presence);
        REQUIRE(f->type_semantics == ASN1TYPED_FIELD_FIXED_TYPE && f->type.kind == ASN1TYPED_REF_NAMED);
        REQUIRE(!strcmp(f->type.module, fields[i].module) && !strcmp(f->type.source_name, fields[i].type));
        REQUIRE(f->type.actual_count == (i + 1 == field_count ? 1u : 0u));
        if(!f->type.actual_count) (void)find(m, f->type.module, f->type.source_name);
    }
    ext = &t->fields[field_count - 1].type;
    REQUIRE(!strcmp(ext->actuals[0].module, "NGAP-IEs") && !strcmp(ext->actuals[0].source_name, set));
    container = &m->bound_instances[instance(m, ext)];
    REQUIRE(container->body.kind == ASN1TYPED_TYPE_SEQUENCE_OF && !container->body.is_extensible);
    REQUIRE(container->body.size_constraint.has_size_constraint && !container->body.size_constraint.is_extensible);
    REQUIRE(container->body.size_constraint.lower_bound == 1 && container->body.size_constraint.upper_bound == 65535);
    REQUIRE(!strcmp(container->identity.source_name, "ProtocolExtensionContainer"));
    e = instance(m, &container->body.element_type);
    element = &m->bound_instances[e];
    REQUIRE(!strcmp(element->identity.source_name, "ProtocolExtensionField"));
    REQUIRE(element->body.kind == ASN1TYPED_TYPE_SEQUENCE && element->body.field_count == 3);
    REQUIRE(!strcmp(element->body.fields[0].source_name, "id") && !strcmp(element->body.fields[1].source_name, "criticality") &&
        !strcmp(element->body.fields[2].source_name, "extensionValue"));
    REQUIRE(element->ioc_binding.has_valid_binding);
    REQUIRE(element->ioc_binding.id_field_ordinal == 0 && element->ioc_binding.criticality_field_ordinal == 1 && element->ioc_binding.value_field_ordinal == 2);
    REQUIRE(asn1typed_bound_instance_ioc_binding_validate(m, e, diagnostic, sizeof(diagnostic)) == 0);
    registry = &m->ioc_registries[element->ioc_binding.registry_index];
    REQUIRE(!strcmp(registry->class_module, "NGAP-Containers") && !strcmp(registry->class_source_name, "NGAP-PROTOCOL-EXTENSION"));
    REQUIRE(!strcmp(registry->object_set_module, "NGAP-IEs") && !strcmp(registry->object_set_source_name, set));
    REQUIRE(!strcmp(registry->selected_class_field_source_name, "Extension"));
    REQUIRE(registry->has_valid_dispatch && registry->object_set_is_extensible == 1);
    REQUIRE(registry->declared_row_count == row_count && registry->row_count == row_count);
    REQUIRE(asn1typed_ioc_registry_validate(registry, diagnostic, sizeof(diagnostic)) == 0);
    for(i = 0; i < row_count; ++i) {
        const asn1typed_ioc_dispatch_row_t *r = &registry->rows[i];
        REQUIRE(r->has_numeric_id && r->numeric_id == rows[i].id && !strcmp(r->symbolic_id, rows[i].symbol));
        REQUIRE(r->criticality == ASN1TYPED_CRITICALITY_IGNORE);
        REQUIRE(r->has_presence && r->presence == ASN1TYPED_PRESENCE_OPTIONAL);
        REQUIRE(r->payload_type.kind == ASN1TYPED_REF_NAMED && !strcmp(r->payload_type.source_name, rows[i].payload));
        (void)find(m, r->payload_type.module, r->payload_type.source_name);
    }
    for(i = 0; i < m->bound_instance_count; ++i) REQUIRE(m->bound_instances[i].body_materialized);
}

static const expected_field_t setup_response[] = {
    {"dLQosFlowPerTNLInformation", "NGAP-IEs", "QosFlowPerTNLInformation", ASN1TYPED_PRESENCE_MANDATORY},
    {"additionalDLQosFlowPerTNLInformation", "NGAP-IEs", "QosFlowPerTNLInformationList", ASN1TYPED_PRESENCE_OPTIONAL},
    {"securityResult", "NGAP-IEs", "SecurityResult", ASN1TYPED_PRESENCE_OPTIONAL},
    {"qosFlowFailedToSetupList", "NGAP-IEs", "QosFlowListWithCause", ASN1TYPED_PRESENCE_OPTIONAL},
    {"iE-Extensions", "NGAP-Containers", "ProtocolExtensionContainer", ASN1TYPED_PRESENCE_OPTIONAL}
};
static const expected_row_t setup_response_rows[] = {
    {192, "id-RedundantDLQosFlowPerTNLInformation", "QosFlowPerTNLInformation"},
    {195, "id-UsedRSNInformation", "RedundantPDUSessionInformation"}
};
static const expected_field_t release_command[] = {
    {"cause", "NGAP-IEs", "Cause", ASN1TYPED_PRESENCE_MANDATORY},
    {"iE-Extensions", "NGAP-Containers", "ProtocolExtensionContainer", ASN1TYPED_PRESENCE_OPTIONAL}
};
static const expected_row_t release_command_rows[] = {
    {284, "id-QosFlowAdditionalInfoList", "QosFlowAdditionalInfoList"}
};
static const expected_field_t release_response[] = {
    {"iE-Extensions", "NGAP-Containers", "ProtocolExtensionContainer", ASN1TYPED_PRESENCE_OPTIONAL}
};
static const expected_row_t release_response_rows[] = {
    {144, "id-SecondaryRATUsageInformation", "SecondaryRATUsageInformation"},
    {284, "id-QosFlowAdditionalInfoList", "QosFlowAdditionalInfoList"}
};

static void selected_roots(void) {
    asn1p_t *tree = asn1p_parse_file(TRANSFER_ROOTS_FIXTURE, A1P_NOFLAGS);
    asn1typed_module_t roots[3] = {{0}}, control = {0}, out = {0};
    const char *names[] = {"PDUSessionResourceSetupResponseTransfer", "PDUSessionResourceReleaseCommandTransfer", "PDUSessionResourceReleaseResponseTransfer"};
    size_t i;
    long point;
    REQUIRE(tree && asn1f_process(tree, A1F_NOFLAGS, NULL) >= 0);
    for(i = 0; i < 3; ++i) {
        int rc = asn1typed_extract_physical_message(tree, "NGAP-IEs", names[i], &roots[i], diagnostic, sizeof(diagnostic));
        if(rc) fprintf(stderr, "%s: %s\n", names[i], diagnostic);
        REQUIRE(rc == 0);
    }
    /* Existing IOC-message path: single ProtocolIE-Container bound to &Value. */
    REQUIRE(asn1typed_extract_physical_message(tree, "NGAP-IEs", "PDUSessionResourceSetupRequestTransfer", &control, diagnostic, sizeof(diagnostic)) == 0);
    /* Name selection is bounded: identical shapes outside the three roots or
     * outside NGAP-IEs keep the original single-container rejection. */
    REQUIRE(asn1typed_extract_physical_message(tree, "NGAP-IEs", "PathSwitchRequestUnsuccessfulTransfer", &out, diagnostic, sizeof(diagnostic)) == -1);
    REQUIRE(!strcmp(diagnostic, "unsupported physical single-container IOC message graph")); cleared(&out);
    /* Allocation failure at every point publishes nothing. */
    for(point = 0; point < 60000; ++point) {
        int rc;
        allocation_countdown = point;
        rc = asn1typed_extract_physical_message(tree, "NGAP-IEs", names[0], &out, diagnostic, sizeof(diagnostic));
        allocation_countdown = -1;
        if(rc == 0) { asn1typed_module_clear(&out); break; }
        REQUIRE(rc == -1 && diagnostic[0]); cleared(&out);
    }
    REQUIRE(point > 0 && point < 60000);
    asn1p_delete(tree);

    /* Everything below reads only owned results after Parser deletion. */
    transfer_root(&roots[0], names[0], setup_response, 5, "PDUSessionResourceSetupResponseTransfer-ExtIEs", setup_response_rows, 2);
    transfer_root(&roots[1], names[1], release_command, 2, "PDUSessionResourceReleaseCommandTransfer-ExtIEs", release_command_rows, 1);
    transfer_root(&roots[2], names[2], release_response, 1, "PDUSessionResourceReleaseResponseTransfer-ExtIEs", release_response_rows, 2);
    REQUIRE(find(&roots[0], "NGAP-IEs", "GTPTunnel")->kind == ASN1TYPED_TYPE_SEQUENCE);
    REQUIRE(find(&roots[0], "NGAP-IEs", "Cause")->kind == ASN1TYPED_TYPE_CHOICE);
    REQUIRE(find(&roots[1], "NGAP-IEs", "Cause")->kind == ASN1TYPED_TYPE_CHOICE);
    REQUIRE(roots[0].ioc_registry_count == 2); /* root ExtIEs + nested GTPTunnel-ExtIEs */
    REQUIRE(roots[1].ioc_registry_count == 1 && roots[2].ioc_registry_count == 1);
    {
        const asn1typed_type_t *t = &control.types[0];
        size_t c = instance(&control, &t->fields[0].type);
        size_t f = instance(&control, &control.bound_instances[c].body.element_type);
        const asn1typed_ioc_registry_t *r = &control.ioc_registries[control.bound_instances[f].ioc_binding.registry_index];
        REQUIRE(t->field_count == 1 && t->fields[0].presence == ASN1TYPED_PRESENCE_MANDATORY && !strcmp(t->fields[0].source_name, "protocolIEs"));
        REQUIRE(control.bound_instances[c].body.kind == ASN1TYPED_TYPE_SEQUENCE_OF && control.bound_instances[c].body.size_constraint.lower_bound == 0);
        REQUIRE(!strcmp(r->selected_class_field_source_name, "Value") && r->row_count == 2);
        REQUIRE(r->rows[1].presence == ASN1TYPED_PRESENCE_MANDATORY && r->rows[1].criticality == ASN1TYPED_CRITICALITY_REJECT);
    }
    for(i = 0; i < 3; ++i) asn1typed_module_clear(&roots[i]);
    asn1typed_module_clear(&control);
}

/* Out-of-boundary variants of the selected names. */
static const char ies_head[] =
    "NGAP-IEs DEFINITIONS AUTOMATIC TAGS ::= BEGIN\n"
    "IMPORTS ProtocolExtensionID, ProtocolIE-ID FROM NGAP-CommonDataTypes\n"
    "  NGAP-PROTOCOL-EXTENSION, NGAP-PROTOCOL-IES, ProtocolExtensionContainer, ProtocolExtensionField,\n"
    "  ProtocolIE-Container FROM NGAP-Containers;\n"
    "id-Extra ProtocolExtensionID ::= 7\n"
    "id-Value ProtocolIE-ID ::= 8\n"
    "id-Other ProtocolExtensionID ::= 9\n"
    "CauseMisc ::= ENUMERATED { unspecified, ... }\n"
    "Cause ::= CHOICE { misc CauseMisc, ... }\n"
    "Rows NGAP-PROTOCOL-EXTENSION ::= { { ID id-Extra CRITICALITY ignore EXTENSION CauseMisc PRESENCE optional }, ... }\n"
    "OtherRows NGAP-PROTOCOL-EXTENSION ::= { { ID id-Other CRITICALITY ignore EXTENSION CauseMisc PRESENCE optional }, ... }\n"
    "FixedSetContainer {NGAP-PROTOCOL-EXTENSION : S} ::= SEQUENCE (SIZE (1..65535)) OF ProtocolExtensionField {{OtherRows}}\n"
    "IERows NGAP-PROTOCOL-IES ::= { { ID id-Value CRITICALITY reject TYPE CauseMisc PRESENCE mandatory }, ... }\n"
    "ValueField {NGAP-PROTOCOL-IES : S} ::= SEQUENCE {\n"
    "  id NGAP-PROTOCOL-IES.&id ({S}),\n"
    "  criticality NGAP-PROTOCOL-IES.&criticality ({S}{@id}),\n"
    "  value NGAP-PROTOCOL-IES.&Value ({S}{@id})\n"
    "}\n"
    "ValueContainer {NGAP-PROTOCOL-IES : S} ::= SEQUENCE (SIZE (1..65535)) OF ValueField {{S}}\n";
#define EXT "iE-Extensions ProtocolExtensionContainer { {Rows} } OPTIONAL"
static const struct bad_root_s {
    const char *root, *body, *diagnostic;
} bad_roots[] = {
    /* Would satisfy the IOC-message predicate; must not be disguised as one. */
    {"PDUSessionResourceReleaseResponseTransfer",
     "PDUSessionResourceReleaseResponseTransfer ::= SEQUENCE { iE-Extensions ProtocolExtensionContainer { {Rows} }, ... }\n",
     "last component is not OPTIONAL single-set iE-Extensions"},
    {"PDUSessionResourceReleaseCommandTransfer",
     "PDUSessionResourceReleaseCommandTransfer ::= SEQUENCE { cause Cause, " EXT " }\n",
     "root lacks root-only SEQUENCE extension evidence"},
    {"PDUSessionResourceReleaseCommandTransfer",
     "PDUSessionResourceReleaseCommandTransfer ::= SEQUENCE { cause Cause, " EXT ", ..., extra CauseMisc OPTIONAL }\n",
     "SEQUENCE component after extension marker is unsupported"},
    {"PDUSessionResourceReleaseCommandTransfer",
     "PDUSessionResourceReleaseCommandTransfer ::= SEQUENCE { cause Cause, ... }\n",
     "last component is not OPTIONAL single-set iE-Extensions"},
    {"PDUSessionResourceReleaseCommandTransfer",
     "PDUSessionResourceReleaseCommandTransfer ::= SEQUENCE { cause Cause, extensions ProtocolExtensionContainer { {Rows} } OPTIONAL, ... }\n",
     "last component is not OPTIONAL single-set iE-Extensions"},
    {"PDUSessionResourceReleaseCommandTransfer",
     "PDUSessionResourceReleaseCommandTransfer ::= SEQUENCE { " EXT ", cause Cause, ... }\n",
     "parameterized component other than iE-Extensions"},
    {"PDUSessionResourceSetupResponseTransfer",
     "PDUSessionResourceSetupResponseTransfer ::= SEQUENCE { protocolIEs ProtocolIE-Container { {IERows} }, " EXT ", ... }\n",
     "parameterized component other than iE-Extensions"},
    {"PDUSessionResourceReleaseCommandTransfer",
     "PDUSessionResourceReleaseCommandTransfer ::= SEQUENCE { cause ENUMERATED { a, b }, " EXT ", ... }\n",
     "component is not a fixed named type reference"},
    {"PDUSessionResourceReleaseCommandTransfer",
     "PDUSessionResourceReleaseCommandTransfer ::= SEQUENCE { cause BOOLEAN, " EXT ", ... }\n",
     "component is not a fixed named type reference"},
    {"PDUSessionResourceReleaseCommandTransfer",
     "PDUSessionResourceReleaseCommandTransfer ::= SEQUENCE { cause CauseMisc DEFAULT unspecified, " EXT ", ... }\n",
     "DEFAULT presence is unsupported"},
    /* A type-reference alias is refused by the shared entry guard. */
    {"PDUSessionResourceReleaseCommandTransfer",
     "PDUSessionResourceReleaseCommandTransfer ::= Body\nBody ::= SEQUENCE { cause Cause, " EXT ", ... }\n",
     "unsupported physical single-container IOC message graph"},
    {"PDUSessionResourceReleaseResponseTransfer",
     "PDUSessionResourceReleaseResponseTransfer ::= SEQUENCE { iE-Extensions ValueContainer { {IERows} } OPTIONAL, ... }\n",
     "iE-Extensions element has no valid &Extension IOC binding"},
    {"PDUSessionResourceReleaseResponseTransfer",
     "PDUSessionResourceReleaseResponseTransfer ::= SEQUENCE { iE-Extensions ProtocolIE-Container { {IERows} } OPTIONAL, ... }\n",
     "iE-Extensions container SIZE lower bound is not positive"},
    /* B1: the container ignores its parameter and binds a different valid
     * &Extension set; the root's declared set must be the bound one. */
    {"PDUSessionResourceReleaseResponseTransfer",
     "PDUSessionResourceReleaseResponseTransfer ::= SEQUENCE { iE-Extensions FixedSetContainer { {Rows} } OPTIONAL, ... }\n",
     "iE-Extensions binding does not use the root's declared object set"},
    /* Shape passes; an unsupported dependency stops closure without output. */
    {"PDUSessionResourceReleaseCommandTransfer",
     "PDUSessionResourceReleaseCommandTransfer ::= SEQUENCE { cause Unsupported, " EXT ", ... }\nUnsupported ::= REAL\n",
     NULL},
};

static void rejected_roots(void) {
    FILE *file = fopen(TRANSFER_ROOTS_FIXTURE, "rb");
    char *fixture, *ies;
    long length;
    size_t i;
    REQUIRE(file && !fseek(file, 0, SEEK_END) && (length = ftell(file)) > 0 && !fseek(file, 0, SEEK_SET));
    fixture = malloc((size_t)length + 1);
    REQUIRE(fixture && fread(fixture, 1, (size_t)length, file) == (size_t)length);
    fclose(file); fixture[length] = 0;
    REQUIRE((ies = strstr(fixture, "NGAP-IEs DEFINITIONS")) != NULL);
    *ies = 0; /* keep NGAP-CommonDataTypes and NGAP-Containers */
    for(i = 0; i < sizeof(bad_roots) / sizeof(bad_roots[0]); ++i) {
        size_t size = strlen(fixture) + sizeof(ies_head) + strlen(bad_roots[i].body) + 8;
        char *source = malloc(size);
        asn1p_t *tree;
        asn1typed_module_t out = {0};
        REQUIRE(source);
        snprintf(source, size, "%s%s%sEND\n", fixture, ies_head, bad_roots[i].body);
        tree = asn1p_parse_buffer(source, -1, "transfer-bad.asn", 1, A1P_NOFLAGS);
        REQUIRE(tree && asn1f_process(tree, A1F_NOFLAGS, NULL) >= 0);
        diagnostic[0] = 0;
        REQUIRE(asn1typed_extract_physical_message(tree, "NGAP-IEs", bad_roots[i].root, &out, diagnostic, sizeof(diagnostic)) == -1);
        if(bad_roots[i].diagnostic && !strstr(diagnostic, bad_roots[i].diagnostic))
            fprintf(stderr, "case %zu: unexpected diagnostic '%s'\n", i, diagnostic);
        REQUIRE(diagnostic[0] && (!bad_roots[i].diagnostic || strstr(diagnostic, bad_roots[i].diagnostic)));
        cleared(&out);
        asn1typed_module_clear(&out);
        asn1p_delete(tree);
        free(source);
    }
    {
        /* A selected name outside NGAP-IEs keeps the original rejection. */
        static const char shadow[] =
            "Shadow-IEs DEFINITIONS AUTOMATIC TAGS ::= BEGIN\n"
            "IMPORTS NGAP-PROTOCOL-EXTENSION, ProtocolExtensionContainer FROM NGAP-Containers;\n"
            "Cause ::= CHOICE { misc ENUMERATED { unspecified, ... }, ... }\n"
            "PDUSessionResourceReleaseCommandTransfer ::= SEQUENCE {\n"
            "  cause Cause, iE-Extensions ProtocolExtensionContainer { {Shadow-ExtIEs} } OPTIONAL, ...\n"
            "}\n"
            "Shadow-ExtIEs NGAP-PROTOCOL-EXTENSION ::= { ... }\n"
            "END\n";
        size_t size = strlen(fixture) + sizeof(shadow);
        char *source = malloc(size);
        asn1p_t *tree;
        asn1typed_module_t out = {0};
        REQUIRE(source);
        snprintf(source, size, "%s%s", fixture, shadow);
        tree = asn1p_parse_buffer(source, -1, "transfer-shadow.asn", 1, A1P_NOFLAGS);
        REQUIRE(tree && asn1f_process(tree, A1F_NOFLAGS, NULL) >= 0);
        REQUIRE(asn1typed_extract_physical_message(tree, "Shadow-IEs", "PDUSessionResourceReleaseCommandTransfer", &out, diagnostic, sizeof(diagnostic)) == -1);
        REQUIRE(!strcmp(diagnostic, "unsupported physical single-container IOC message graph")); cleared(&out);
        asn1p_delete(tree);
        free(source);
    }
    free(fixture);
}

int main(void) {
    selected_roots();
    rejected_roots();
    puts("PASS NGAP Transfer ordinary roots: owned fields/presence/extension/container binding, Parser lifetime, bounded selection, rejections and transactions");
    return 0;
}
