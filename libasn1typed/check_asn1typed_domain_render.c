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
    asn1typed_render_cpp_owned_domain_types,
    asn1typed_render_cpp_owned_domain_mapping,
    asn1typed_render_cpp_owned_domain_codec
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
    asn1typed_module_t module = {0};
    asn1p_t *tree;
    char diagnostic[512];
    asn1typed_type_t *owned_types;
    asn1typed_integer_interval_t *owned_tail;
    size_t owned_count;
    size_t i;
    const char *files[] = {"types.hpp", "mapping.hpp", "codec.hpp"};
    REQUIRE(argc == 5);
    tree = asn1p_parse_file(argv[1], A1P_NOFLAGS);
    REQUIRE(tree && asn1f_process(tree, A1F_NOFLAGS, NULL) >= 0);
    REQUIRE(asn1typed_extract_module(tree, argv[2], &module, diagnostic, sizeof(diagnostic)) == 0);
    { const asn1typed_integer_value_range_t *r = &find(&module, "SRBID")->value_range;
      REQUIRE(r->is_extensible && r->lower_bound == 0 && r->upper_bound == 3);
      REQUIRE(r->extension_addition_count == 1 && r->extension_additions[0].lower_bound == 4 && r->extension_additions[0].upper_bound == 5);
      r = &find(&module, "SparseAddition")->value_range;
      REQUIRE(r->extension_addition_count == 2 && r->extension_additions[0].upper_bound == 4 && r->extension_additions[1].lower_bound == 7);
    }
    asn1p_delete(tree);
    owned_types = module.types; owned_count = module.type_count; owned_tail = find(&module, "Gap")->value_range.tail;
    for(i = 0; i < 3; ++i) {
        char *a = NULL, *b = NULL;
        long point;
        { int result = renderers[i](&module, argv[3], &a, diagnostic, sizeof(diagnostic)); if(result) fprintf(stderr, "renderer %zu: %s\n", i, diagnostic); REQUIRE(result == 0); }
        REQUIRE(renderers[i](&module, argv[3], &b, diagnostic, sizeof(diagnostic)) == 0);
        REQUIRE(!strcmp(a, b));
        write_file(argv[4], files[i], a);
        free(b);
        free(a);
        for(point = 0; point < 30000; ++point) {
            char *out = NULL;
            int result;
            allocation_countdown = point;
            result = renderers[i](&module, argv[3], &out, diagnostic, sizeof(diagnostic));
            allocation_countdown = -1;
            REQUIRE(module.types == owned_types && module.type_count == owned_count);
            REQUIRE(find(&module, "Gap")->value_range.tail == owned_tail);
            REQUIRE(owned_tail[7].lower_bound == 180 && owned_tail[7].upper_bound == 181);
            if(result == 0) { REQUIRE(out); free(out); break; }
            REQUIRE(result == -1 && !out && diagnostic[0]);
        }
        REQUIRE(point > 0 && point < 30000);
        REQUIRE(renderers[i](&module, argv[3], NULL, diagnostic, sizeof(diagnostic)) == -1);
        REQUIRE(diagnostic[0]);
    }
    {
        asn1typed_type_t *type = find(&module, "SRBID");
        asn1typed_integer_value_range_t save = type->value_range;
        asn1typed_integer_interval_t malformed = {8, 7};
        type->value_range.extension_additions = NULL; negative(&module, argv[3], NULL); type->value_range = save;
        type->value_range.extension_addition_count = 0; negative(&module, argv[3], NULL); type->value_range = save;
        type->value_range.is_extensible = 0; negative(&module, argv[3], NULL); type->value_range = save;
        type->value_range.extension_additions = &malformed; negative(&module, argv[3], NULL); type->value_range = save;
    }
    negative(NULL, argv[3], NULL);
    negative(&module, NULL, NULL);
    negative(&module, "class", "namespace");
    negative(&module, "nrforge::aper", "namespace");
    negative(&module, "nrforge::aper::nested", "namespace");
    {
        asn1typed_type_t *type = find(&module, "Gap");
        asn1typed_integer_value_range_t save = type->value_range;
        asn1typed_integer_interval_t extra = {2, 3};
        REQUIRE(save.tail_count == 8);
        REQUIRE(save.tail[7].lower_bound == 180 && save.tail[7].upper_bound == 181);
        type->value_range.has_value_range = 0; negative(&module, argv[3], NULL); type->value_range = save;
        type->value_range.is_extensible = 2; negative(&module, argv[3], NULL); type->value_range = save;
        type->value_range.lower_bound = 8; type->value_range.upper_bound = 7;
        negative(&module, argv[3], NULL); type->value_range = save;
        type->value_range.tail = &extra; type->value_range.tail_count = 1;
        negative(&module, argv[3], NULL); type->value_range = save;
        type->value_range.tail = NULL; negative(&module, argv[3], NULL); type->value_range = save;
    }
    {
        asn1typed_type_t *packet = find(&module, "Packet");
        asn1typed_type_t *base = find(&module, "P5");
        asn1typed_field_t save = packet->fields[0];
        asn1typed_integer_value_range_t range = base->value_range;
        packet->fields[0].type.kind = ASN1TYPED_REF_NAMED;
        packet->fields[0].type.primitive_kind = ASN1TYPED_PRIMITIVE_INVALID;
        packet->fields[0].type.module = module.source_name;
        packet->fields[0].type.source_name = base->identity.source_name;
        base->value_range.is_extensible = 0;
        negative(&module, argv[3], "named use-site refinement");
        base->value_range = range;
        packet->fields[0].value_range.is_extensible = 0;
        negative(&module, argv[3], "not a subset");
        packet->fields[0] = save;
    }
    asn1typed_module_clear(&module);
    puts("PASS owned extensible INTEGER root sets, immutable IR, deterministic lifetime and allocation paths");
    return 0;
}
