#include "asn1typed_extract.h"
#include "asn1typed_render_cpp.h"
#include <asn1fix.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define REQUIRE(x) do { if(!(x)) { fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #x); abort(); } } while(0)
static long allocation_countdown = -1;
void *__real_malloc(size_t);
void *__real_calloc(size_t, size_t);
void *__real_realloc(void *, size_t);
void *__wrap_malloc(size_t);
void *__wrap_calloc(size_t, size_t);
void *__wrap_realloc(void *, size_t);
static int fails(void) {
    if(allocation_countdown < 0) return 0;
    if(!allocation_countdown) { allocation_countdown = -1; return 1; }
    --allocation_countdown;
    return 0;
}
void *__wrap_malloc(size_t n) { return fails() ? NULL : __real_malloc(n); }
void *__wrap_calloc(size_t n, size_t z) { return fails() ? NULL : __real_calloc(n, z); }
void *__wrap_realloc(void *p, size_t n) { return fails() ? NULL : __real_realloc(p, n); }
typedef int (*renderer)(const asn1typed_module_t *, const char *, char **, char *, size_t);
static renderer renderers[] = {
    asn1typed_render_cpp_owned_shape_types,
    asn1typed_render_cpp_owned_shape_mapping,
    asn1typed_render_cpp_owned_shape_codec
};
static void negative(const asn1typed_module_t *m, const char *ns, const char *part) {
    size_t i;
    for(i = 0; i < 3; ++i) {
        char *out = NULL, diagnostic[512] = {0};
        REQUIRE(renderers[i](m, ns, &out, diagnostic, sizeof(diagnostic)) == -1);
        REQUIRE(!out && diagnostic[0]);
        if(part) REQUIRE(strstr(diagnostic, part));
    }
}
static asn1typed_type_t *find(asn1typed_module_t *m, const char *name) {
    size_t i;
    for(i = 0; i < m->type_count; ++i)
        if(!strcmp(m->types[i].identity.source_name, name)) return &m->types[i];
    REQUIRE(0);
    return NULL;
}
static void unchanged(asn1typed_module_t *m, size_t count, asn1typed_type_t *panel, asn1typed_type_t *body) {
    REQUIRE(m->type_count == count && find(m, "Panel") == panel);
    REQUIRE(panel->field_count == 6 && panel->fields[1].inline_enumerated == body);
    REQUIRE(panel->fields[1].type_semantics == ASN1TYPED_FIELD_INLINE_ENUMERATED);
    REQUIRE(body->enum_item_count == 3 && body->has_valid_per_enumeration_mapping);
    REQUIRE(body->enum_items[0].assigned_number == 9 && body->enum_items[0].per_enumeration_index == 2);
    REQUIRE(body->enum_items[1].assigned_number == -2 && body->enum_items[1].per_enumeration_index == 0);
    REQUIRE(body->enum_items[2].assigned_number == 5 && body->enum_items[2].per_enumeration_index == 1);
    REQUIRE(!strcmp(panel->fields[1].source_name, "phase"));
}
static void write_file(const char *directory, const char *name, const char *text) {
    char path[4096];
    FILE *file;
    int n = snprintf(path, sizeof(path), "%s/%s", directory, name);
    REQUIRE(n > 0 && (size_t)n < sizeof(path));
    file = fopen(path, "wb");
    REQUIRE(file && fwrite(text, 1, strlen(text), file) == strlen(text));
    REQUIRE(fclose(file) == 0);
}
int main(int argc, char **argv) {
    asn1typed_module_t module = {0}, renamed = {0}, collision = {0}, unsafe = {0};
    asn1p_t *tree;
    char diagnostic[512];
    size_t i, original_count;
    asn1typed_type_t *original_panel, *original_enum;
    const char *files[] = {"types.hpp", "mapping.hpp", "codec.hpp"};
    REQUIRE(argc == 5);
    tree = asn1p_parse_file(argv[1], A1P_NOFLAGS);
    REQUIRE(tree && asn1f_process(tree, A1F_NOFLAGS, NULL) >= 0);
    REQUIRE(asn1typed_extract_module(tree, argv[2], &module, diagnostic, sizeof(diagnostic)) == 0);
    REQUIRE(asn1typed_extract_module(tree, "ShapeRenamed", &renamed, diagnostic, sizeof(diagnostic)) == 0);
    REQUIRE(asn1typed_extract_module(tree, "ShapeCollision", &collision, diagnostic, sizeof(diagnostic)) == 0);
    REQUIRE(asn1typed_extract_module(tree, "ShapeUnsafe", &unsafe, diagnostic, sizeof(diagnostic)) == 0);
    {
        asn1typed_module_t rejected = {0};
        REQUIRE(asn1typed_extract_module(tree, "ShapeDefault", &rejected, diagnostic, sizeof(diagnostic)) == -1);
        REQUIRE(diagnostic[0]);
        asn1typed_module_clear(&rejected);
    }
    asn1p_delete(tree);
    original_count = module.type_count; original_panel = find(&module, "Panel");
    original_enum = original_panel->fields[1].inline_enumerated;
    for(i = 0; i < 3; ++i) {
        char *a = NULL, *b = NULL;
        long point;
        REQUIRE(renderers[i](&module, argv[3], &a, diagnostic, sizeof(diagnostic)) == 0);
        REQUIRE(renderers[i](&module, argv[3], &b, diagnostic, sizeof(diagnostic)) == 0);
        REQUIRE(!strcmp(a, b));
        unchanged(&module, original_count, original_panel, original_enum);
        write_file(argv[4], files[i], a);
        free(b);
        free(a);
        for(point = 0; point < 30000; ++point) {
            char *out = NULL;
            int result;
            allocation_countdown = point;
            result = renderers[i](&module, argv[3], &out, diagnostic, sizeof(diagnostic));
            allocation_countdown = -1;
            unchanged(&module, original_count, original_panel, original_enum);
            if(result == 0) { REQUIRE(out); free(out); break; }
            REQUIRE(result == -1 && !out && diagnostic[0]);
        }
        REQUIRE(point > 0 && point < 30000);
        REQUIRE(renderers[i](&module, argv[3], NULL, diagnostic, sizeof(diagnostic)) == -1);
        REQUIRE(diagnostic[0]);
    }
    negative(NULL, argv[3], NULL);
    negative(&module, NULL, NULL);
    negative(&module, "class", "namespace");
    negative(&module, "nrforge::aper", "namespace");
    negative(&module, "nrforge::aper::nested", "namespace");

    {
        asn1typed_type_t *panel = find(&module, "Panel");
        asn1typed_type_t *body = panel->fields[1].inline_enumerated;
        asn1typed_type_t snapshot = *body;
        asn1typed_field_t field_snapshot = panel->fields[1];
        size_t type_count = module.type_count;
        REQUIRE(body && body->has_valid_per_enumeration_mapping);
        REQUIRE(panel->fields[1].type_semantics == ASN1TYPED_FIELD_INLINE_ENUMERATED);
        REQUIRE(body->enum_items[0].assigned_number == 9 && body->enum_items[0].per_enumeration_index == 2);
        REQUIRE(body->enum_items[1].assigned_number == -2 && body->enum_items[1].per_enumeration_index == 0);
        REQUIRE(body->enum_items[2].assigned_number == 5 && body->enum_items[2].per_enumeration_index == 1);
        body->has_valid_per_enumeration_mapping = 0; negative(&module, argv[3], NULL); *body = snapshot;
        body->enum_items[0].has_per_enumeration_index = 0; negative(&module, argv[3], NULL);
        body->enum_items[0].has_per_enumeration_index = 1;
        body->enum_items[0].per_enumeration_index = 0; negative(&module, argv[3], NULL);
        body->enum_items[0].per_enumeration_index = 2;
        body->ioc_container.source_name = (char *)"stale"; negative(&module, argv[3], NULL); *body = snapshot;
        panel->fields[1].presence = ASN1TYPED_PRESENCE_CONDITIONAL; negative(&module, argv[3], NULL); panel->fields[1] = field_snapshot;
        panel->fields[1].ioc.has_numeric_id = 1; negative(&module, argv[3], NULL); panel->fields[1] = field_snapshot;
        panel->fields[1].has_class_field_relation = 1; negative(&module, argv[3], NULL); panel->fields[1] = field_snapshot;
        REQUIRE(module.type_count == type_count && panel->fields[1].inline_enumerated == body);
        REQUIRE(body->has_valid_per_enumeration_mapping && body->enum_items[0].per_enumeration_index == 2);
    }
    negative(&collision, argv[3], NULL);
    negative(&unsafe, argv[3], NULL);
    {
        const char *renamed_files[] = {"renamed_types.hpp", "renamed_mapping.hpp", "renamed_codec.hpp"};
        size_t j;
        for(j = 0; j < 3; ++j) {
            char *a = NULL, *b = NULL;
            REQUIRE(renderers[j](&renamed, "renamedshapes", &a, diagnostic, sizeof(diagnostic)) == 0);
            REQUIRE(renderers[j](&renamed, "renamedshapes", &b, diagnostic, sizeof(diagnostic)) == 0);
            REQUIRE(!strcmp(a,b)); write_file(argv[4], renamed_files[j], a); free(a); free(b);
        }
    }
    asn1typed_module_clear(&unsafe); asn1typed_module_clear(&collision);
    asn1typed_module_clear(&renamed); asn1typed_module_clear(&module);
    puts("PASS owned inline ENUMERATED lowering, lifetime, deterministic output and fail-closed allocation/evidence paths");
    return 0;
}
