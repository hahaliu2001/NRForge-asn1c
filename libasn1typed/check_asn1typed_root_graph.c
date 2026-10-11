#include "asn1typed_extract.h"
#include <asn1fix.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define REQUIRE(c) do { if(!(c)) { fprintf(stderr, "root graph:%d: %s\n", __LINE__, #c); exit(1); } } while(0)
#ifndef ROOT_GRAPH_FIXTURE
#define ROOT_GRAPH_FIXTURE "fixtures/ordinary-root-graph.asn1"
#endif

/* Fail each allocation in the new extraction path, with parsing/fixing outside
 * the failure window. Cleanup must leave no partial published graph. */
void *__real_malloc(size_t);
void *__real_calloc(size_t, size_t);
void *__real_realloc(void *, size_t);
static int armed;
static size_t allocation_index, fail_at = (size_t)-1;
static int fail(void) { return armed && allocation_index++ == fail_at; }
void *__wrap_malloc(size_t n) { return fail() ? NULL : __real_malloc(n); }
void *__wrap_calloc(size_t n, size_t s) { return fail() ? NULL : __real_calloc(n, s); }
void *__wrap_realloc(void *p, size_t n) { return fail() ? NULL : __real_realloc(p, n); }

static asn1typed_type_t *find(asn1typed_module_t *m, const char *module, const char *name) {
    size_t i;
    for(i = 0; i < m->type_count; ++i)
        if(!strcmp(m->types[i].identity.module, module) && !strcmp(m->types[i].identity.source_name, name)) return &m->types[i];
    return NULL;
}
static void cleared(asn1typed_module_t *m) {
    REQUIRE(!m->types && !m->type_count && !m->type_capacity && !m->source_name && !m->location.file);
    REQUIRE(!m->bound_instances && !m->bound_instance_count && !m->ioc_registries && !m->ioc_registry_count);
    asn1typed_module_clear(m);
    asn1typed_module_clear(m);
}
static void rejected(asn1p_t *tree, const char *name, const asn1typed_graph_limits_t *limits, const char *why) {
    asn1typed_module_t m = {0};
    char error[1024] = {0};
    REQUIRE(asn1typed_extract_root_graph(tree, "GraphA", name, limits, &m, error, sizeof(error)) == -1);
    if(!strstr(error, why)) { fprintf(stderr, "expected %s: %s\n", why, error); exit(1); }
    cleared(&m);
}
int main(void) {
    asn1p_t *tree = asn1p_parse_file(ROOT_GRAPH_FIXTURE, A1P_NOFLAGS);
    asn1typed_module_t root = {0}, alias = {0}, cycle = {0}, opaque = {0}, ext = {0}, again = {0}, qualified = {0}, wrap = {0};
    char error[1024] = {0};
    asn1typed_type_t *t;
    asn1typed_graph_limits_t limits = {1, 100, 1000};
    size_t i;
    REQUIRE(tree && asn1f_process(tree, A1F_COMPOUND_NAMES, NULL) >= 0);
    REQUIRE(asn1typed_extract_root_graph(tree, "GraphA", "Root", NULL, &root, error, sizeof(error)) == 0);
    REQUIRE(asn1typed_extract_root_graph(tree, "GraphA", "Root", NULL, &again, error, sizeof(error)) == 0);
    REQUIRE(root.type_count == again.type_count);
    for(i = 0; i < root.type_count; ++i) {
        REQUIRE(!strcmp(root.types[i].identity.module, again.types[i].identity.module));
        REQUIRE(!strcmp(root.types[i].identity.source_name, again.types[i].identity.source_name));
        REQUIRE(root.types[i].kind == again.types[i].kind);
    }
    asn1typed_module_clear(&again);
    REQUIRE(asn1typed_extract_root_graph(tree, "GraphA", "AliasRoot", NULL, &alias, error, sizeof(error)) == 0);
    REQUIRE(asn1typed_extract_root_graph(tree, "GraphA", "Left", NULL, &cycle, error, sizeof(error)) == 0);
    REQUIRE(asn1typed_extract_root_graph(tree, "GraphA", "Contained", NULL, &opaque, error, sizeof(error)) == 0);
    REQUIRE(asn1typed_extract_root_graph(tree, "GraphA", "RootOnly", NULL, &ext, error, sizeof(error)) == 0);
    REQUIRE(asn1typed_extract_root_graph(tree, "GraphA", "Qualified", NULL, &qualified, error, sizeof(error)) == 0);
    {
        asn1p_module_t *remote = TQ_NEXT(TQ_FIRST(&tree->modules), mod_next);
        char *original_file = remote->source_file_name;
        remote->source_file_name = "remote-body-provenance.asn1";
        REQUIRE(asn1typed_extract_root_graph(tree, "GraphA", "WrapAlias", NULL, &wrap, error, sizeof(error)) == 0);
        remote->source_file_name = original_file;
    }
    {
        asn1p_expr_t *decl, *field = NULL;
        asn1p_module_t *mod = TQ_FIRST(&tree->modules);
        char *old;
        TQ_FOR(decl, &mod->members, next)
            if(decl->Identifier && !strcmp(decl->Identifier, "Root")) field = TQ_FIRST(&decl->members);
        REQUIRE(field && field->reference && field->reference->comp_count == 1);
        old = field->reference->components[0].name;
        field->reference->components[0].name = "Hidden";
        /* A stale cache points at Base, and Hidden exists in another loaded
         * module but is not imported. Neither may rescue this reference. */
        rejected(tree, "Root", NULL, "ordinary reference cache");
        field->reference->components[0].name = old;
        TQ_FOR(decl, &mod->members, next)
            if(decl->Identifier && !strcmp(decl->Identifier, "AliasRoot")) field = decl;
        old = field->reference->components[0].name;
        field->reference->components[0].name = "Hidden";
        rejected(tree, "AliasRoot", NULL, "ordinary reference cache");
        field->reference->components[0].name = old;
        {
            asn1p_expr_t *cached = field->reference->ref_expr;
            TQ_FOR(decl, &mod->members, next)
                if(decl->Identifier && !strcmp(decl->Identifier, "Base")) field->reference->ref_expr = decl;
            rejected(tree, "AliasRoot", NULL, "ordinary reference cache");
            field->reference->ref_expr = cached;
        }
    }
    rejected(tree, "Root", &limits, "type limit");
    limits.max_types = 100; limits.max_references = 1;
    rejected(tree, "Root", &limits, "reference limit");
    limits.max_references = 100; limits.max_ast_nodes = 1;
    rejected(tree, "Root", &limits, "AST limit");
    limits.max_ast_nodes = 0;
    rejected(tree, "Root", &limits, "invalid ordinary");
    rejected(tree, "BadDefault", NULL, "DEFAULT");
    rejected(tree, "BadExtension", NULL, "after extension marker");
    rejected(tree, "BadAlias", NULL, "constrained ordinary alias");
    rejected(tree, "BadActual", NULL, "ordinary type parameters");
    rejected(tree, "Box", NULL, "ordinary type parameters");
    rejected(tree, "Unsupported", NULL, "unsupported ordinary declaration");
    rejected(tree, "Absent", NULL, "root not found");
    REQUIRE(asn1typed_extract_root_graph(NULL, "GraphA", "Root", NULL, &again, error, sizeof(error)) == -1);
    cleared(&again);
    armed = 1; allocation_index = 0;
    REQUIRE(asn1typed_extract_root_graph(tree, "GraphA", "Root", NULL, &again, error, sizeof(error)) == 0);
    armed = 0;
    {
        size_t allocations = allocation_index;
        asn1typed_module_clear(&again);
        REQUIRE(allocations > 0 && allocations < 10000);
        for(fail_at = 0; fail_at < allocations; ++fail_at) {
            int rc;
            armed = 1; allocation_index = 0;
            rc = asn1typed_extract_root_graph(tree, "GraphA", "Root", NULL, &again, error, sizeof(error));
            armed = 0;
            REQUIRE(rc == -1 && error[0]);
            cleared(&again);
        }
        printf("allocation failure rollback: %zu positions PASS\n", allocations);
    }
    /* Borrowed tree destroyed before every substantive success assertion. */
    asn1p_delete(tree);
    REQUIRE(!strcmp(root.types[0].identity.source_name, "Root"));
    REQUIRE(root.type_count == 9 && !root.bound_instance_count && !root.ioc_registry_count);
    t = find(&root, "GraphA", "Base");
    REQUIRE(t && t->value_range.lower_bound == 0 && t->value_range.upper_bound == 3);
    t = find(&root, "GraphB", "Base");
    REQUIRE(t && t->value_range.lower_bound == 10 && t->value_range.upper_bound == 20);
    t = find(&root, "GraphB", "Shared");
    REQUIRE(t && t->kind == ASN1TYPED_TYPE_PRIMITIVE && t->value_range.lower_bound == 10 && t->value_range.upper_bound == 20);
    t = &root.types[0];
    REQUIRE(!strcmp(t->fields[1].type.module, "GraphB") && !strcmp(t->fields[1].type.source_name, "Shared"));
    REQUIRE(find(&root, "GraphA", "$inline$Root$pick"));
    REQUIRE(find(&root, "GraphA", "$inline$Root$pick$nested"));
    t = find(&root, "GraphA", "$inline$Root$list");
    REQUIRE(t && t->size_constraint.upper_bound == 4 && !strcmp(t->element_type.source_name, "Shared"));
    t = find(&root, "GraphA", "Node");
    REQUIRE(t && !strcmp(t->fields[0].type.source_name, "Node"));
    REQUIRE(alias.type_count == 1 && !strcmp(alias.types[0].identity.source_name, "AliasRoot"));
    REQUIRE(alias.types[0].value_range.lower_bound == 10 && alias.types[0].value_range.upper_bound == 20);
    REQUIRE(cycle.type_count == 2 && find(&cycle, "GraphA", "Right"));
    REQUIRE(qualified.type_count == 2 && find(&qualified, "GraphB", "Shared"));
    REQUIRE(wrap.type_count == 2 && find(&wrap, "GraphB", "Base"));
    REQUIRE(strcmp(wrap.types[0].location.file, "remote-body-provenance.asn1"));
    REQUIRE(!strcmp(wrap.types[0].fields[0].location.file, "remote-body-provenance.asn1"));
    REQUIRE(opaque.type_count == 1 && opaque.types[0].fields[0].type.primitive_kind == ASN1TYPED_PRIMITIVE_OCTET_STRING);
    REQUIRE(ext.types[0].has_valid_sequence_extension_structure && ext.types[0].sequence_root_field_count == 1);
    asn1typed_module_clear(&root); asn1typed_module_clear(&alias); asn1typed_module_clear(&cycle);
    asn1typed_module_clear(&opaque); asn1typed_module_clear(&ext);
    asn1typed_module_clear(&qualified);
    asn1typed_module_clear(&wrap);
    puts("ordinary root graph: PASS");
    return 0;
}
