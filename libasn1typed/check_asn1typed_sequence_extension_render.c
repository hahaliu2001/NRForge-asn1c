#include "asn1typed_extract.h"
#include "asn1typed_render_cpp.h"
#include <asn1fix.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef NDEBUG
#error "Focused checks must remain active under NDEBUG."
#endif
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
    asn1typed_render_cpp_owned_sequence_extension_types,
    asn1typed_render_cpp_owned_sequence_extension_mapping,
    asn1typed_render_cpp_owned_sequence_extension_codec
};
static renderer ordinary[] = {
    asn1typed_render_cpp_owned_compound_types,
    asn1typed_render_cpp_owned_compound_mapping,
    asn1typed_render_cpp_owned_compound_codec
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
    size_t i;
    int compatibility;
    const char *files[] = {"types.hpp", "mapping.hpp", "codec.hpp"};
    REQUIRE(argc == 5 || argc == 6);
    compatibility = argc == 6;
    tree = asn1p_parse_file(argv[1], A1P_NOFLAGS);
    REQUIRE(tree && asn1f_process(tree, A1F_NOFLAGS, NULL) >= 0);
    REQUIRE(asn1typed_extract_module(tree, argv[2], &module, diagnostic, sizeof(diagnostic)) == 0);
    asn1p_delete(tree);
    for(i = 0; i < 3; ++i) {
        char *a = NULL, *b = NULL;
        long point;
        REQUIRE(renderers[i](&module, argv[3], &a, diagnostic, sizeof(diagnostic)) == 0);
        REQUIRE(renderers[i](&module, argv[3], &b, diagnostic, sizeof(diagnostic)) == 0);
        REQUIRE(!strcmp(a, b));
        write_file(argv[4], files[i], a);
        free(b);
        if(compatibility) {
            REQUIRE(ordinary[i](&module, argv[3], &b, diagnostic, sizeof(diagnostic)) == 0);
            if(i < 2) REQUIRE(!strcmp(a, b));
            free(b);
        } else {
            REQUIRE(ordinary[i](&module, argv[3], &b, diagnostic, sizeof(diagnostic)) == -1);
            REQUIRE(!b && diagnostic[0]);
        }
        free(a);
        for(point = 0; point < 20000; ++point) {
            char *out = NULL;
            int result;
            allocation_countdown = point;
            result = renderers[i](&module, argv[3], &out, diagnostic, sizeof(diagnostic));
            allocation_countdown = -1;
            if(result == 0) { REQUIRE(out); free(out); break; }
            REQUIRE(result == -1 && !out && diagnostic[0]);
        }
        REQUIRE(point > 0 && point < 20000);
        REQUIRE(renderers[i](&module, argv[3], NULL, diagnostic, sizeof(diagnostic)) == -1);
        REQUIRE(diagnostic[0]);
    }
    negative(NULL, argv[3], NULL);
    negative(&module, NULL, NULL);
    negative(&module, "a::INT64_MAX", "namespace");
    negative(&module, "nrforge::aper", "namespace");
    negative(&module, "nrforge::aper::Result", "namespace");
    negative(&module, "nrforge::aper::custom", "namespace");
    if(!compatibility) {
        for(i = 0; i < module.type_count; ++i) {
            asn1typed_type_t *type = &module.types[i];
            if(type->kind == ASN1TYPED_TYPE_SEQUENCE && type->is_extensible) {
                asn1typed_type_t save = *type;
                type->has_valid_sequence_extension_structure = 0;
                negative(&module, argv[3], NULL);
                *type = save;
                type->sequence_extension_evidence = ASN1TYPED_WIRE_EVIDENCE_UNSUPPORTED;
                negative(&module, argv[3], NULL);
                *type = save;
                ++type->sequence_root_field_count;
                negative(&module, argv[3], NULL);
                *type = save;
                type->sequence_known_addition_count = 1;
                negative(&module, argv[3], NULL);
                *type = save;
                if(type->field_count) {
                    asn1typed_presence_e presence = type->fields[0].presence;
                    type->fields[0].presence = ASN1TYPED_PRESENCE_CONDITIONAL;
                    negative(&module, argv[3], NULL);
                    type->fields[0].presence = presence;
                }
            } else if(type->kind == ASN1TYPED_TYPE_CHOICE) {
                type->is_extensible = 1;
                negative(&module, argv[3], NULL);
                type->is_extensible = 0;
            }
        }
        {
            asn1typed_module_t two = module;
            char *a = module.types[0].identity.source_name;
            char *b = module.types[1].identity.source_name;
            two.type_count = 2;
            module.types[0].identity.source_name = "Foo-Bar";
            module.types[1].identity.source_name = "fooBar";
            negative(&two, argv[3], "collision");
            module.types[0].identity.source_name = a;
            module.types[1].identity.source_name = b;
        }
        {
            size_t count = module.bound_instance_count;
            module.bound_instance_count = 1;
            negative(&module, argv[3], NULL);
            module.bound_instance_count = count;
        }
    }
    asn1typed_module_clear(&module);
    return 0;
}
