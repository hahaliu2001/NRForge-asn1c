#include "asn1typed_extract.h"
#include <asn1fix.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef COLLECTION_ELEMENT_FIXTURE
#define COLLECTION_ELEMENT_FIXTURE "../tools/rrc-collection-elements/collections.asn1"
#endif
#define CHECK(c) do { if(!(c)) { fprintf(stderr, "collection elements:%d: %s\n", __LINE__, #c); exit(1); } } while(0)

void *__real_malloc(size_t);
void *__real_calloc(size_t, size_t);
void *__real_realloc(void *, size_t);
static int armed;
static size_t allocation_index, fail_at = (size_t)-1;
static int fail(void) { return armed && allocation_index++ == fail_at; }
void *__wrap_malloc(size_t n) { return fail() ? NULL : __real_malloc(n); }
void *__wrap_calloc(size_t n, size_t s) { return fail() ? NULL : __real_calloc(n, s); }
void *__wrap_realloc(void *p, size_t n) { return fail() ? NULL : __real_realloc(p, n); }

static asn1typed_type_t *find(asn1typed_module_t *m, const char *name) {
    for(size_t i = 0; i < m->type_count; ++i)
        if(!strcmp(m->types[i].identity.source_name, name)) return &m->types[i];
    return NULL;
}
static void cleared(asn1typed_module_t *m) {
    CHECK(!m->types && !m->type_count && !m->type_capacity && !m->source_name && !m->location.file);
    CHECK(!m->bound_instances && !m->bound_instance_count && !m->ioc_registries && !m->ioc_registry_count);
    asn1typed_module_clear(m);
    asn1typed_module_clear(m);
}
static asn1typed_type_t *element(asn1typed_module_t *m, const char *list) {
    asn1typed_type_t *t = find(m, list);
    CHECK(t && t->kind == ASN1TYPED_TYPE_SEQUENCE_OF && t->element_type.kind == ASN1TYPED_REF_NAMED);
    CHECK(!strcmp(t->element_type.module, "CollectionCodec"));
    t = find(m, t->element_type.source_name);
    CHECK(t && t->kind == ASN1TYPED_TYPE_PRIMITIVE && !strcmp(t->identity.module, "CollectionCodec"));
    return t;
}
int main(void) {
    const char *roots[] = {"Integers", "Singletons", "Bits", "BoundedBits", "Fields", "Pick",
                          "Nested", "Grouped", "Wide", "Union", "ExtInts"};
    const char *bad[] = {"BadExtSingleton", "BadOverflow", "BadExcept", "BadNamedBits", "BadNamedUse", "BadOctets"};
    asn1typed_module_t graphs[11] = {{0}}, rejected = {0};
    char error[1024] = {0};
    asn1p_t *tree = asn1p_parse_file(COLLECTION_ELEMENT_FIXTURE, A1P_NOFLAGS);
    CHECK(tree && asn1f_process(tree, A1F_NOFLAGS, NULL) >= 0);
    /* Capture borrowed source element pointers/constraints before extraction. */
    asn1p_expr_t *decl = TQ_FIRST(&TQ_FIRST(&tree->modules)->members);
    CHECK(decl && !strcmp(decl->Identifier, "Integers"));
    asn1p_expr_t *leaf = TQ_FIRST(&decl->members);
    char *identifier = leaf->Identifier;
    asn1p_constraint_t *declared = leaf->constraints, *combined = leaf->combined_constraints;
    for(size_t i = 0; i < 11; ++i) {
        int rc = asn1typed_extract_root_graph(tree, "CollectionCodec", roots[i], NULL, &graphs[i], error, sizeof(error));
        if(rc) fprintf(stderr, "%s: %s\n", roots[i], error);
        CHECK(rc == 0);
    }
    CHECK(leaf->Identifier == identifier && leaf->constraints == declared && leaf->combined_constraints == combined);
    for(size_t i = 0; i < 6; ++i) {
        CHECK(asn1typed_extract_root_graph(tree, "CollectionCodec", bad[i], NULL, &rejected, error, sizeof(error)) == -1);
        CHECK(error[0]); cleared(&rejected);
    }
    const asn1typed_graph_limits_t limits[] = {{1, 100, 1000}, {100, 1, 1000}, {100, 100, 1}};
    for(size_t i = 0; i < 3; ++i) {
        CHECK(asn1typed_extract_root_graph(tree, "CollectionCodec", "Nested", &limits[i], &rejected, error, sizeof(error)) == -1);
        CHECK(strstr(error, "limit exceeded")); cleared(&rejected);
    }
    armed = 1; allocation_index = 0;
    CHECK(asn1typed_extract_root_graph(tree, "CollectionCodec", "Nested", NULL, &rejected, error, sizeof(error)) == 0);
    armed = 0;
    size_t allocations = allocation_index;
    asn1typed_module_clear(&rejected);
    for(size_t i = 0; i < allocations; ++i) {
        armed = 1; allocation_index = 0; fail_at = i;
        int rc = asn1typed_extract_root_graph(tree, "CollectionCodec", "Nested", NULL, &rejected, error, sizeof(error));
        armed = 0;
        CHECK(rc == -1); cleared(&rejected);
    }
    printf("collection element allocation rollback: %zu positions PASS\n", allocations);
    asn1p_delete(tree);
    asn1typed_type_t *e = element(&graphs[0], "Integers");
    CHECK(e->primitive_kind == ASN1TYPED_PRIMITIVE_INTEGER && e->value_range.has_value_range);
    CHECK(e->value_range.lower_bound == -2 && e->value_range.upper_bound == 3 && !e->size_constraint.has_size_constraint);
    CHECK(find(&graphs[0], "Integers")->size_constraint.lower_bound == 1 && find(&graphs[0], "Integers")->size_constraint.upper_bound == 3);
    e = element(&graphs[1], "Singletons"); CHECK(e->value_range.lower_bound == 4 && e->value_range.upper_bound == 4);
    e = element(&graphs[2], "Bits"); CHECK(e->primitive_kind == ASN1TYPED_PRIMITIVE_BIT_STRING && e->size_constraint.lower_bound == 32 && e->size_constraint.upper_bound == 32);
    e = element(&graphs[3], "BoundedBits"); CHECK(e->size_constraint.lower_bound == 1 && e->size_constraint.upper_bound == 3);
    CHECK(find(&graphs[3], "BoundedBits")->size_constraint.lower_bound == 0 && find(&graphs[3], "BoundedBits")->size_constraint.upper_bound == 2);
    CHECK(element(&graphs[4], "$inline$Fields$first")->value_range.upper_bound == 7);
    CHECK(element(&graphs[4], "$inline$Fields$second")->value_range.upper_bound == 15);
    CHECK(element(&graphs[5], "$inline$Pick$integers")->value_range.upper_bound == 139);
    CHECK(element(&graphs[6], "$inline$Nested$@element$values")->value_range.upper_bound == 7);
    CHECK(element(&graphs[7], "$inline$Grouped$@group0$values")->value_range.upper_bound == 7);
    CHECK(element(&graphs[7], "$inline$Grouped$extra")->size_constraint.upper_bound == 32);
    e = element(&graphs[8], "Wide"); CHECK(e->value_range.unsigned_bounds && e->value_range.unsigned_lower_bound == UINT64_MAX - 1 && e->value_range.unsigned_upper_bound == UINT64_MAX);
    e = element(&graphs[9], "Union"); CHECK(e->value_range.tail_count == 1 && e->value_range.tail[0].lower_bound == 4 && e->value_range.tail[0].upper_bound == 5);
    e = element(&graphs[10], "ExtInts"); CHECK(e->value_range.is_extensible && e->value_range.lower_bound == 0 && e->value_range.upper_bound == 3);
    for(size_t i = 0; i < 11; ++i) asn1typed_module_clear(&graphs[i]);

    /* Legacy closure shares the same promotion, and distinct modules own
     * identical local element paths without a global-name collision. */
    const char source[] = "ElementsA DEFINITIONS ::= BEGIN\nList ::= SEQUENCE (SIZE (1..2)) OF INTEGER (0..7)\nEND\n"
                          "ElementsB DEFINITIONS ::= BEGIN\nList ::= SEQUENCE (SIZE (2..3)) OF INTEGER (10..15)\nEND\n";
    tree = asn1p_parse_buffer(source, -1, "element-modules.asn", 1, A1P_NOFLAGS);
    CHECK(tree && asn1f_process(tree, A1F_COMPOUND_NAMES, NULL) >= 0);
    for(size_t i = 0; i < 2; ++i) {
        CHECK(asn1typed_extract_module(tree, i ? "ElementsB" : "ElementsA", &graphs[i], error, sizeof(error)) == 0);
        CHECK(graphs[i].type_count == 2);
    }
    asn1p_delete(tree);
    CHECK(find(&graphs[0], "$inline$List$@element")->value_range.upper_bound == 7);
    CHECK(find(&graphs[1], "$inline$List$@element")->value_range.upper_bound == 15);
    CHECK(strcmp(find(&graphs[0], "$inline$List$@element")->identity.module, find(&graphs[1], "$inline$List$@element")->identity.module));
    asn1typed_module_clear(&graphs[0]); asn1typed_module_clear(&graphs[1]);
    puts("PASS constrained collection element ownership, identities, ranges/SIZE, source lifetime, negatives and quotas");
    return 0;
}
