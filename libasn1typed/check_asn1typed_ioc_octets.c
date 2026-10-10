#include "asn1typed_extract.h"
#include <asn1fix.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define REQUIRE(x) do { if(!(x)) { fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #x); abort(); } } while(0)
#ifndef IOC_OCTETS_FIXTURE
#define IOC_OCTETS_FIXTURE "fixtures/ioc-octets-n14.asn1"
#endif
static long countdown = -1;
void *n9_test_malloc(size_t);
void *n9_test_calloc(size_t, size_t);
void *n9_test_realloc(void *, size_t);
static int fails(void) {
    if(countdown < 0) return 0;
    if(!countdown) { countdown = -1; return 1; }
    --countdown; return 0;
}
void *n9_test_malloc(size_t n) { return fails() ? NULL : malloc(n); }
void *n9_test_calloc(size_t n, size_t z) { return fails() ? NULL : calloc(n, z); }
void *n9_test_realloc(void *p, size_t n) { return fails() ? NULL : realloc(p, n); }
static char diagnostic[512];
static int extract(asn1p_t *tree, const char *name, asn1typed_module_t *out) {
    return asn1typed_extract_physical_message(tree, "IOCOctets", name, out,
                                            diagnostic, sizeof(diagnostic));
}
static void owned_ok(asn1typed_module_t *m, int extension) {
    size_t i;
    asn1typed_ioc_registry_t *r;
    REQUIRE(m->ioc_registry_count == 1 && m->bound_instance_count == 2);
    r = &m->ioc_registries[0];
    REQUIRE(asn1typed_ioc_registry_validate(r, diagnostic, sizeof(diagnostic)) == 0);
    REQUIRE(r->row_count == (extension ? 1u : 3u));
    REQUIRE(!strcmp(r->selected_class_field_source_name, extension ? "Extension" : "Value"));
    for(i = 0; i < (extension ? 1u : 2u); ++i) {
        asn1typed_type_ref_t *ref = &r->rows[i].payload_type;
        REQUIRE(ref->kind == ASN1TYPED_REF_PRIMITIVE);
        REQUIRE(ref->primitive_kind == ASN1TYPED_PRIMITIVE_OCTET_STRING);
        REQUIRE(!ref->module && !ref->source_name && !ref->actuals && !ref->actual_count);
    }
    if(!extension) {
        REQUIRE(r->rows[0].numeric_id == 91 && r->rows[1].numeric_id == 7 && r->rows[2].numeric_id == 42);
        REQUIRE(r->rows[2].payload_type.kind == ASN1TYPED_REF_NAMED);
        REQUIRE(!strcmp(r->rows[2].payload_type.source_name, "NamedOctets"));
    }
    for(i = 0; i < m->type_count; ++i)
    {
        asn1typed_type_t *t = &m->types[i];
        REQUIRE(strcmp(t->identity.source_name, "Contained"));
        if(!strcmp(t->identity.source_name, "NamedOctets")) {
            REQUIRE(t->primitive_kind == ASN1TYPED_PRIMITIVE_OCTET_STRING);
            REQUIRE(t->size_constraint.has_size_constraint && !t->size_constraint.is_extensible);
            REQUIRE(t->size_constraint.lower_bound == 3 && t->size_constraint.upper_bound == 3);
        }
    }
    for(i = 0; i < m->bound_instance_count; ++i)
        if(m->bound_instances[i].ioc_binding.has_valid_binding)
            REQUIRE(asn1typed_bound_instance_ioc_binding_validate(m, i, diagnostic, sizeof(diagnostic)) == 0);
}
static void rejects(asn1p_t *tree, const char *name) {
    asn1typed_module_t m = {0};
    REQUIRE(extract(tree, name, &m) == -1 && diagnostic[0]);
    REQUIRE(strstr(diagnostic, "unsupported constrained IOC payload evidence"));
    REQUIRE(!m.types && !m.bound_instances && !m.ioc_registries && !m.source_name);
    asn1typed_module_clear(&m);
}
static asn1p_expr_t *cell(asn1p_t *tree, const char *set, size_t row) {
    asn1p_module_t *module;
    asn1p_expr_t *expr;
    TQ_FOR(module, &tree->modules, mod_next) {
        TQ_FOR(expr, &module->members, next) {
            size_t i;
            if(!expr->Identifier || strcmp(expr->Identifier, set)) continue;
            REQUIRE(expr->ioc_table && row < expr->ioc_table->rows);
            for(i = 0; i < expr->ioc_table->row[row]->columns; ++i) {
                struct asn1p_ioc_cell_s *c = &expr->ioc_table->row[row]->column[i];
                if(!strcmp(c->field->Identifier, "&Value")) return c->value;
            }
        }
    }
    REQUIRE(0); return NULL;
}
static void fixed_residue_rejects(asn1p_t *tree) {
    asn1p_expr_t *contents = cell(tree, "Rows", 0), *plain = cell(tree, "Rows", 1);
    asn1p_expr_t *sized = cell(tree, "SizedRows", 0);
    asn1p_constraint_t *saved = contents->combined_constraints;
    REQUIRE(contents->constraints && sized->constraints && !plain->constraints);
    /* Test adversarial residue on real fixed cells, restoring Parser ownership
     * before deleting it. No use-site SIZE is silently erased. */
    contents->combined_constraints = sized->constraints;
    rejects(tree, "Message"); contents->combined_constraints = saved;
    saved = plain->combined_constraints;
    plain->combined_constraints = sized->constraints;
    rejects(tree, "Message"); plain->combined_constraints = saved;
}
int main(void) {
    asn1p_t *tree = asn1p_parse_file(IOC_OCTETS_FIXTURE, A1P_NOFLAGS);
    asn1typed_module_t main_graph = {0}, extension_graph = {0}, sized_graph = {0};
    const char *names[] = { "Message", "ExtensionMessage" };
    size_t n;
    REQUIRE(tree && asn1f_process(tree, A1F_NOFLAGS, NULL) >= 0);
    REQUIRE(extract(tree, "SizedMessage", &sized_graph) == 0);
    rejects(tree, "CompoundMessage");
    fixed_residue_rejects(tree);
    for(n = 0; n < 2; ++n) {
        long point;
        for(point = 0; point < 10000; ++point) {
            asn1typed_module_t m = {0};
            int result;
            countdown = point; result = extract(tree, names[n], &m); countdown = -1;
            if(result == 0) { owned_ok(&m, n != 0); asn1typed_module_clear(&m); break; }
            REQUIRE(result == -1 && diagnostic[0]);
            REQUIRE(!m.types && !m.bound_instances && !m.ioc_registries && !m.source_name);
            asn1typed_module_clear(&m);
        }
        REQUIRE(point > 0 && point < 10000);
    }
    REQUIRE(extract(tree, "Message", &main_graph) == 0);
    REQUIRE(extract(tree, "ExtensionMessage", &extension_graph) == 0);
    asn1p_delete(tree);
    {
        const asn1typed_type_ref_t *ref = &sized_graph.ioc_registries[0].rows[0].payload_type;
        size_t i;
        REQUIRE(ref->kind == ASN1TYPED_REF_NAMED);
        for(i = 0; i < sized_graph.type_count; ++i)
            if(!strcmp(sized_graph.types[i].identity.source_name, ref->source_name)) break;
        REQUIRE(i < sized_graph.type_count);
        REQUIRE(sized_graph.types[i].primitive_kind == ASN1TYPED_PRIMITIVE_OCTET_STRING);
        REQUIRE(sized_graph.types[i].size_constraint.has_size_constraint &&
            sized_graph.types[i].size_constraint.lower_bound == 3 && sized_graph.types[i].size_constraint.upper_bound == 3);
        asn1typed_module_clear(&sized_graph);
    }
    /* Containing type is deliberately unsupported: outer-octet ownership
     * must neither traverse it nor retain any Parser/Fixer pointer. */
    owned_ok(&main_graph, 0); owned_ok(&extension_graph, 1);
    asn1typed_module_clear(&main_graph); asn1typed_module_clear(&extension_graph);
    asn1typed_module_clear(&main_graph); asn1typed_module_clear(&extension_graph);
    puts("IOC outer-octet evidence PASS (Parser deleted, allocation sweep)");
    return 0;
}
