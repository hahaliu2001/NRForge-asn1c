#include "asn1typed_extract.h"
#include <asn1fix.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define REQUIRE(x) do { if(!(x)) { fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #x); abort(); } } while(0)
#ifndef IOC_EVIDENCE_FIXTURE
#define IOC_EVIDENCE_FIXTURE "fixtures/ioc-evidence-n9.asn1"
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
static char diagnostic[512];
static asn1typed_type_t *find(asn1typed_module_t *m, const char *name) {
    size_t i;
    for(i = 0; i < m->type_count; ++i)
        if(!strcmp(m->types[i].identity.source_name, name)) return &m->types[i];
    REQUIRE(0); return NULL;
}
static void registry_ok(const asn1typed_ioc_registry_t *registry, size_t count) {
    REQUIRE(registry->evidence == ASN1TYPED_WIRE_EVIDENCE_RESOLVED);
    REQUIRE(registry->has_valid_dispatch && registry->declared_row_count == count && registry->row_count == count);
    REQUIRE(asn1typed_ioc_registry_validate(registry, diagnostic, sizeof(diagnostic)) == 0);
}
static void registry_bad(const asn1typed_ioc_registry_t *registry) {
    REQUIRE(asn1typed_ioc_registry_validate(registry, diagnostic, sizeof(diagnostic)) == -1);
    REQUIRE(diagnostic[0]);
}
static void core_ownership(void) {
    asn1typed_module_t m = {0};
    asn1typed_ioc_dispatch_row_t row = {0}, other = {0};
    asn1typed_ioc_registry_t *registry;
    size_t index, same, count;
    long point;
    char class_module[] = "Core", class_name[] = "Class", set_module[] = "Core", set_name[] = "Rows";
    char field[] = "Value", symbol[] = "id-First";
    REQUIRE(asn1typed_module_init(&m, "Core", __FILE__, __LINE__) == 0);
    REQUIRE(asn1typed_module_add_ioc_registry(&m, class_module, class_name, set_module, set_name, field, &index) == 0);
    REQUIRE(index == 0 && m.ioc_registry_count == 1);
    registry = &m.ioc_registries[index];
    class_name[0] = 'X'; set_name[0] = 'X'; field[0] = 'X';
    REQUIRE(!strcmp(registry->class_source_name, "Class") && !strcmp(registry->object_set_source_name, "Rows"));
    REQUIRE(!strcmp(registry->selected_class_field_source_name, "Value"));
    REQUIRE(asn1typed_module_add_ioc_registry(&m, "Core", "Class", "Core", "Rows", "Value", &same) == 0);
    REQUIRE(same == index && m.ioc_registry_count == 1);
    REQUIRE(asn1typed_module_add_ioc_registry(&m, "Core", "Class", "Core", "Rows", "Extension", &same) == -1);
    REQUIRE(m.ioc_registry_count == 1);
    row.symbolic_id = symbol; row.has_numeric_id = 1; row.numeric_id = 91;
    row.criticality = ASN1TYPED_CRITICALITY_REJECT; row.has_presence = 1; row.presence = ASN1TYPED_PRESENCE_MANDATORY;
    REQUIRE(asn1typed_type_ref_init(&row.payload_type, "Core", "Flag") == 0);
    REQUIRE(asn1typed_ioc_registry_add_row(registry, &row) == 0);
    REQUIRE(registry->rows[0].symbolic_id != row.symbolic_id);
    REQUIRE(registry->rows[0].payload_type.source_name != row.payload_type.source_name);
    symbol[0] = 'X'; row.payload_type.source_name[0] = 'X';
    REQUIRE(!strcmp(registry->rows[0].symbolic_id, "id-First"));
    REQUIRE(!strcmp(registry->rows[0].payload_type.source_name, "Flag"));
    REQUIRE(asn1typed_ioc_registry_set_evidence(registry, 1, 1) == 0);
    REQUIRE(asn1typed_ioc_registry_finalize(registry, diagnostic, sizeof(diagnostic)) == ASN1TYPED_WIRE_FINALIZE_OK);
    registry_ok(registry, 1);
    count = registry->row_count;
    REQUIRE(asn1typed_ioc_registry_set_evidence(registry, 2, 1) == -1);
    REQUIRE(asn1typed_ioc_registry_set_evidence(registry, 1, 2) == -1);
    registry_ok(registry, count);
    other.symbolic_id = "id-Second";
    other.has_numeric_id = 1; other.numeric_id = 7; other.criticality = ASN1TYPED_CRITICALITY_IGNORE;
    other.has_presence = 1; other.presence = ASN1TYPED_PRESENCE_CONDITIONAL;
    REQUIRE(asn1typed_type_ref_init_primitive(&other.payload_type, ASN1TYPED_PRIMITIVE_BOOLEAN) == 0);
    /* Advertise only the initialized extent so the next append exercises
     * reserve as well as row cloning; the allocation itself may be larger. */
    registry->row_capacity = registry->row_count;
    for(point = 0; point < 1000; ++point) {
        asn1typed_ioc_dispatch_row_t *rows = registry->rows;
        size_t capacity = registry->row_capacity;
        int result;
        allocation_countdown = point;
        result = asn1typed_ioc_registry_add_row(registry, &other);
        allocation_countdown = -1;
        if(result == 0) break;
        REQUIRE(result == -1 && registry->rows == rows && registry->row_capacity == capacity);
        registry_ok(registry, count);
    }
    REQUIRE(point > 0 && point < 1000);
    REQUIRE(registry->row_count == 2 && !registry->has_valid_dispatch && !registry->declared_row_count);
    REQUIRE(registry->evidence == ASN1TYPED_WIRE_EVIDENCE_UNAVAILABLE);
    REQUIRE(asn1typed_ioc_registry_set_evidence(registry, 2, 1) == 0);
    REQUIRE(asn1typed_ioc_registry_finalize(registry, diagnostic, sizeof(diagnostic)) == ASN1TYPED_WIRE_FINALIZE_OK);
    registry_ok(registry, 2);
    {
        asn1typed_type_ref_t save = other.payload_type;
        asn1typed_type_actual_t actual = {0};
        other.payload_type.actuals = &actual;
        REQUIRE(asn1typed_ioc_registry_add_row(registry, &other) == -1);
        registry_ok(registry, 2); other.payload_type = save;
        other.payload_type.module = "Stray";
        REQUIRE(asn1typed_ioc_registry_add_row(registry, &other) == -1);
        registry_ok(registry, 2); other.payload_type = save;
        other.payload_type.source_name = "Stray";
        REQUIRE(asn1typed_ioc_registry_add_row(registry, &other) == -1);
        registry_ok(registry, 2); other.payload_type = save;
        save = row.payload_type;
        row.payload_type.primitive_kind = ASN1TYPED_PRIMITIVE_BOOLEAN;
        REQUIRE(asn1typed_ioc_registry_add_row(registry, &row) == -1);
        registry_ok(registry, 2); row.payload_type = save;
    }
    {
        asn1typed_ioc_dispatch_row_t save = registry->rows[1];
        registry->rows[1].numeric_id = 91; registry_bad(registry); registry->rows[1] = save;
        registry->rows[1].has_numeric_id = 0; registry_bad(registry); registry->rows[1] = save;
        registry->rows[1].numeric_id = -1; registry_bad(registry); registry->rows[1] = save;
        registry->rows[1].numeric_id = 65536; registry_bad(registry); registry->rows[1] = save;
        registry->rows[1].criticality = (asn1typed_criticality_e)99; registry_bad(registry); registry->rows[1] = save;
        registry->rows[1].payload_type.actual_count = 1; registry_bad(registry); registry->rows[1] = save;
    }
    {
        size_t capacity = registry->row_capacity;
        registry->row_capacity = 1; registry_bad(registry);
        REQUIRE(asn1typed_ioc_registry_finalize(registry, diagnostic, sizeof(diagnostic)) == ASN1TYPED_WIRE_FINALIZE_ERROR);
        registry->row_capacity = capacity;
        REQUIRE(asn1typed_ioc_registry_finalize(registry, diagnostic, sizeof(diagnostic)) == ASN1TYPED_WIRE_FINALIZE_OK);
    }
    REQUIRE(asn1typed_ioc_registry_set_unsupported(registry) == 0); registry_bad(registry);
    REQUIRE(asn1typed_ioc_registry_finalize(registry, diagnostic, sizeof(diagnostic)) == ASN1TYPED_WIRE_FINALIZE_UNAVAILABLE);
    REQUIRE(asn1typed_ioc_registry_set_unavailable(registry) == 0); registry_bad(registry);
    REQUIRE(asn1typed_ioc_registry_set_evidence(registry, 2, 1) == 0);
    REQUIRE(asn1typed_ioc_registry_finalize(registry, diagnostic, sizeof(diagnostic)) == ASN1TYPED_WIRE_FINALIZE_OK);
    other.numeric_id = 91;
    REQUIRE(asn1typed_ioc_registry_add_row(registry, &other) == 0);
    REQUIRE(asn1typed_ioc_registry_set_evidence(registry, 3, 1) == 0);
    REQUIRE(asn1typed_ioc_registry_finalize(registry, diagnostic, sizeof(diagnostic)) == ASN1TYPED_WIRE_FINALIZE_UNAVAILABLE);
    REQUIRE(!registry->has_valid_dispatch);
    REQUIRE(asn1typed_ioc_registry_finalize(NULL, diagnostic, sizeof(diagnostic)) == ASN1TYPED_WIRE_FINALIZE_ERROR);
    REQUIRE(asn1typed_ioc_registry_add_row(NULL, &row) == -1);
    m.ioc_registry_capacity = m.ioc_registry_count;
    for(point = 0; point < 1000; ++point) {
        asn1typed_ioc_registry_t *registries = m.ioc_registries;
        size_t capacity = m.ioc_registry_capacity;
        int result;
        allocation_countdown = point;
        result = asn1typed_module_add_ioc_registry(&m, "Core", "Empty", "Core", "EmptyRows", "Value", &index);
        allocation_countdown = -1;
        if(result == 0) break;
        REQUIRE(result == -1 && m.ioc_registry_count == 1 && m.ioc_registries == registries && m.ioc_registry_capacity == capacity);
    }
    REQUIRE(point > 0 && point < 1000);
    registry = &m.ioc_registries[index];
    registry_bad(registry);
    REQUIRE(asn1typed_ioc_registry_set_evidence(registry, 0, 0) == -1);
    REQUIRE(asn1typed_ioc_registry_set_evidence(registry, 0, 1) == 0);
    REQUIRE(asn1typed_ioc_registry_finalize(registry, diagnostic, sizeof(diagnostic)) == ASN1TYPED_WIRE_FINALIZE_OK);
    registry_ok(registry, 0);
    asn1typed_type_ref_clear(&row.payload_type); asn1typed_type_ref_clear(&other.payload_type);
    asn1typed_module_clear(&m);
}
static void binding_ok(const asn1typed_module_t *m, size_t index) {
    REQUIRE(m->bound_instances[index].ioc_binding.has_valid_binding);
    REQUIRE(asn1typed_bound_instance_ioc_binding_validate(m, index, diagnostic, sizeof(diagnostic)) == 0);
}
static void binding_bad(const asn1typed_module_t *m, size_t index) {
    REQUIRE(asn1typed_bound_instance_ioc_binding_validate(m, index, diagnostic, sizeof(diagnostic)) == -1);
    REQUIRE(diagnostic[0]);
}
static asn1p_expr_t *declaration(asn1p_t *tree, const char *name) {
    asn1p_module_t *module;
    asn1p_expr_t *member;
    TQ_FOR(module, &tree->modules, mod_next) {
        if(strcmp(module->ModuleName, "IOCEvidence")) continue;
        TQ_FOR(member, &module->members, next)
            if(member->Identifier && !strcmp(member->Identifier, name)) return member;
    }
    REQUIRE(0); return NULL;
}
static void message_bad(asn1p_t *tree, const char *message) {
    asn1typed_module_t out = {0};
    REQUIRE(asn1typed_extract_physical_message(tree, "IOCEvidence", message, &out,
                                              diagnostic, sizeof(diagnostic)) == -1);
    REQUIRE(diagnostic[0] && !out.source_name && !out.types && !out.bound_instances && !out.ioc_registries);
    asn1typed_module_clear(&out);
}
static void extraction_bad(asn1p_t *tree) { message_bad(tree, "DispatchMessage"); }
static void fixed_tree_rejections(asn1p_t *tree) {
    asn1p_expr_t *class_decl = declaration(tree, "REGISTRY-CLASS");
    asn1p_expr_t *unique_id = TQ_FIRST(&class_decl->members);
    asn1p_expr_t *set = declaration(tree, "SourceRows"), *cell = NULL;
    size_t i;
    int unique;
    REQUIRE(unique_id && unique_id->unique);
    unique = unique_id->unique; unique_id->unique = 0;
    extraction_bad(tree); unique_id->unique = unique;
    {
        asn1p_expr_t *policy = TQ_NEXT(unique_id, next);
        struct asn1p_expr_marker_s marker;
        asn1p_value_t default_value = {0};
        asn1p_constraint_t constraint = {0}, *saved_constraint;
        REQUIRE(policy);
        marker = policy->marker; policy->marker.flags = EM_DEFAULT;
        extraction_bad(tree); policy->marker = marker;
        default_value.type = ATV_INTEGER; default_value.value.v_integer = 0;
        policy->marker.default_value = &default_value;
        extraction_bad(tree); policy->marker = marker;
        saved_constraint = policy->constraints; policy->constraints = &constraint;
        extraction_bad(tree); policy->constraints = saved_constraint;
    }
    REQUIRE(set->ioc_table && set->ioc_table->rows == 3);
    for(i = 0; i < set->ioc_table->row[2]->columns; ++i) {
        const char *name = set->ioc_table->row[2]->column[i].field->Identifier;
        if(!strcmp(name, "&id") || !strcmp(name, "id")) cell = set->ioc_table->row[2]->column[i].value;
    }
    REQUIRE(cell && cell->value);
    {
        asn1p_value_t replacement = *cell->value;
        asn1p_value_t *saved = cell->value;
        replacement.type = ATV_INTEGER; replacement.value.v_integer = 7;
        cell->value = &replacement; extraction_bad(tree); cell->value = saved;
    }
}
static void physical_owned(asn1typed_module_t *m, const char *message, const char *root,
                            const char *set, const char *selected, size_t rows, int extensible) {
    asn1typed_type_t *type = find(m, message);
    asn1typed_ioc_registry_t *registry;
    size_t i, fields = SIZE_MAX, containers = 0;
    REQUIRE(type->kind == ASN1TYPED_TYPE_SEQUENCE && type->field_count == 1);
    REQUIRE(!strcmp(type->fields[0].source_name, root));
    REQUIRE(type->fields[0].type.actual_count == 1);
    REQUIRE(!strcmp(type->fields[0].type.actuals[0].source_name, set));
    REQUIRE(type->is_extensible == extensible);
    if(extensible) REQUIRE(asn1typed_sequence_extension_structure_validate(type, diagnostic, sizeof(diagnostic)) == 0);
    REQUIRE(m->ioc_registry_count == 1);
    registry = &m->ioc_registries[0]; registry_ok(registry, rows);
    REQUIRE(!strcmp(registry->object_set_source_name, set));
    REQUIRE(!strcmp(registry->selected_class_field_source_name, selected));
    REQUIRE(registry->object_set_is_extensible == 1);
    for(i = 0; i < m->bound_instance_count; ++i) {
        asn1typed_bound_instance_t *instance = &m->bound_instances[i];
        if(instance->body.kind == ASN1TYPED_TYPE_SEQUENCE_OF) {
            ++containers;
            REQUIRE(instance->body.size_constraint.has_size_constraint);
            REQUIRE(instance->body.size_constraint.lower_bound == (!strcmp(selected, "Value") ? 0 : 1));
            REQUIRE(instance->body.size_constraint.upper_bound == 65535);
            REQUIRE(instance->body.element_type.actual_count == 1);
            REQUIRE(!instance->ioc_binding.has_valid_binding);
        } else if(instance->body.kind == ASN1TYPED_TYPE_SEQUENCE) {
            REQUIRE(fields == SIZE_MAX); fields = i;
            REQUIRE(instance->body.field_count == 3);
            REQUIRE(instance->ioc_binding.registry_index == 0);
            REQUIRE(instance->ioc_binding.id_field_ordinal == 0 && instance->ioc_binding.criticality_field_ordinal == 1 && instance->ioc_binding.value_field_ordinal == 2);
            REQUIRE(!strcmp(instance->body.fields[0].source_name, !strcmp(selected, "Value") ? "number" : "tag"));
            REQUIRE(!strcmp(instance->body.fields[1].source_name, !strcmp(selected, "Value") ? "policy" : "receivedPolicy"));
            REQUIRE(!strcmp(instance->body.fields[2].source_name, !strcmp(selected, "Value") ? "content" : "extensionContent"));
            binding_ok(m, i);
        }
    }
    REQUIRE(containers == 1 && fields != SIZE_MAX);
    if(rows == 3) {
        REQUIRE(registry->rows[0].numeric_id == 91 && registry->rows[1].numeric_id == 7 && registry->rows[2].numeric_id == 42);
        REQUIRE(registry->rows[0].criticality == ASN1TYPED_CRITICALITY_REJECT && registry->rows[1].criticality == ASN1TYPED_CRITICALITY_IGNORE && registry->rows[2].criticality == ASN1TYPED_CRITICALITY_NOTIFY);
        REQUIRE(registry->rows[0].has_presence && registry->rows[0].presence == ASN1TYPED_PRESENCE_MANDATORY);
        REQUIRE(registry->rows[1].presence == ASN1TYPED_PRESENCE_OPTIONAL && registry->rows[2].presence == ASN1TYPED_PRESENCE_CONDITIONAL);
        REQUIRE(!strcmp(registry->rows[0].payload_type.source_name, "Flag") && !strcmp(registry->rows[2].payload_type.source_name, "Flag"));
        REQUIRE(registry->rows[0].payload_type.source_name != registry->rows[2].payload_type.source_name);
    } else if(rows == 1) {
        REQUIRE(registry->rows[0].numeric_id == 13);
        if(!strcmp(set, "FourRoleRows")) {
            REQUIRE(registry->rows[0].has_presence && registry->rows[0].presence == ASN1TYPED_PRESENCE_CONDITIONAL);
        } else REQUIRE(!registry->rows[0].has_presence);
    }
    {
        asn1typed_ioc_binding_t save = m->bound_instances[fields].ioc_binding;
        REQUIRE(asn1typed_bound_instance_set_ioc_binding(m, fields, 0, 0, 1, 2) == 0);
        REQUIRE(!m->bound_instances[fields].ioc_binding.has_valid_binding);
        REQUIRE(asn1typed_bound_instance_ioc_binding_finalize(m, fields, diagnostic, sizeof(diagnostic)) == ASN1TYPED_WIRE_FINALIZE_OK);
        binding_ok(m, fields);
        REQUIRE(asn1typed_bound_instance_set_ioc_binding(m, fields, m->ioc_registry_count, 0, 1, 2) == -1);
        binding_ok(m, fields);
        m->bound_instances[fields].ioc_binding.value_field_ordinal = 1; binding_bad(m, fields);
        m->bound_instances[fields].ioc_binding = save;
    }
    {
        asn1typed_bound_instance_t *instance = &m->bound_instances[fields];
        asn1typed_field_t save = instance->body.fields[2];
        instance->body.fields[2].class_field_relation.selector_source_name = "wrong"; binding_bad(m, fields);
        instance->body.fields[2] = save;
        instance->body.fields[2].presence = ASN1TYPED_PRESENCE_OPTIONAL; binding_bad(m, fields);
        instance->body.fields[2] = save;
        instance->body.fields[2].class_field_relation.class_module = "Other"; binding_bad(m, fields);
        instance->body.fields[2] = save;
        REQUIRE(asn1typed_ioc_registry_set_unavailable(registry) == 0); binding_bad(m, fields);
        REQUIRE(asn1typed_ioc_registry_set_evidence(registry, rows, 1) == 0);
        REQUIRE(asn1typed_ioc_registry_finalize(registry, diagnostic, sizeof(diagnostic)) == ASN1TYPED_WIRE_FINALIZE_OK);
        binding_ok(m, fields);
    }
}
static void real_extraction(void) {
    asn1p_t *tree = asn1p_parse_file(IOC_EVIDENCE_FIXTURE, A1P_NOFLAGS);
    asn1typed_module_t outputs[5] = {{0}}, legacy = {0};
    const char *messages[] = {"DispatchMessage", "EmptyMessage", "ExtensionMessage", "EmptyExtensionMessage", "FourRoleMessage"};
    const char *roots[] = {"entries", "items", "additions", "additions", "additions"};
    const char *sets[] = {"SourceRows", "EmptyRows", "ExtensionRows", "EmptyExtensionRows", "FourRoleRows"};
    size_t i;
    long point;
    REQUIRE(tree && asn1f_process(tree, A1F_NOFLAGS, NULL) >= 0);
    for(i = 0; i < 5; ++i) {
        int result = asn1typed_extract_physical_message(tree, "IOCEvidence", messages[i], &outputs[i], diagnostic, sizeof(diagnostic));
        if(result) fprintf(stderr, "physical %s: %s\n", messages[i], diagnostic);
        REQUIRE(result == 0);
    }
    REQUIRE(asn1typed_extract_message(tree, "IOCEvidence", "DispatchMessage", &legacy, diagnostic, sizeof(diagnostic)) == 0);
    REQUIRE(legacy.types[0].field_count == 3 && legacy.ioc_registry_count == 0);
    REQUIRE(legacy.types[0].fields[0].ioc.numeric_id == 91 && legacy.types[0].fields[1].ioc.numeric_id == 7 && legacy.types[0].fields[2].ioc.numeric_id == 42);
    message_bad(tree, "InlineMessage");
    message_bad(tree, "Identifier");
    message_bad(NULL, "DispatchMessage");
    fixed_tree_rejections(tree);
    for(point = 0; point < 30000; ++point) {
        asn1typed_module_t out = {0};
        int result;
        allocation_countdown = point;
        result = asn1typed_extract_physical_message(tree, "IOCEvidence", "DispatchMessage", &out, diagnostic, sizeof(diagnostic));
        allocation_countdown = -1;
        if(result == 0) { asn1typed_module_clear(&out); break; }
        REQUIRE(result == -1 && diagnostic[0] && !out.source_name && !out.types && !out.bound_instances && !out.ioc_registries);
        asn1typed_module_clear(&out);
    }
    REQUIRE(point > 0 && point < 30000);
    asn1p_delete(tree);
    for(i = 0; i < 5; ++i) {
        physical_owned(&outputs[i], messages[i], roots[i], sets[i], i < 2 ? "Value" : "Extension", i == 0 ? 3 : (i == 2 || i == 4) ? 1 : 0, i == 0);
        asn1typed_module_clear(&outputs[i]);
    }
    asn1typed_module_clear(&legacy);
}
int main(void) {
    core_ownership(); real_extraction();
    puts("PASS owned IOC registries, physical selectors, renamed fields, empty sets and transactions");
    return 0;
}
