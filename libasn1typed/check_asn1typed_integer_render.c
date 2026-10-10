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
    asn1typed_render_cpp_owned_value_types,
    asn1typed_render_cpp_owned_value_mapping,
    asn1typed_render_cpp_owned_value_codec
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
    asn1typed_module_t module = {0}, integers = {0};
    asn1p_t *tree;
    char diagnostic[512];
    size_t i;
    const char *files[] = {"types.hpp", "mapping.hpp", "codec.hpp"};
    REQUIRE(argc == 5);
    tree = asn1p_parse_file(argv[1], A1P_NOFLAGS);
    REQUIRE(tree && asn1f_process(tree, A1F_NOFLAGS, NULL) >= 0);
    REQUIRE(asn1typed_extract_module(tree, argv[2], &module, diagnostic, sizeof(diagnostic)) == 0);
    REQUIRE(asn1typed_extract_module(tree, "IntegerOnly", &integers, diagnostic, sizeof(diagnostic)) == 0);
    asn1p_delete(tree);
    for(i = 0; i < 3; ++i) {
        char *a = NULL, *b = NULL;
        long point;
        REQUIRE(renderers[i](&module, argv[3], &a, diagnostic, sizeof(diagnostic)) == 0);
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
        asn1typed_type_t *type = find(&module, "SignedSmall");
        asn1typed_integer_value_range_t save = type->value_range;
        asn1typed_integer_interval_t extra = {7, 8};
        type->value_range.has_value_range = 0; negative(&module, argv[3], NULL); type->value_range = save;
        type->value_range.is_extensible = 1; negative(&module, argv[3], NULL); type->value_range = save;
        type->value_range.lower_bound = 8; type->value_range.upper_bound = 7;
        negative(&module, argv[3], NULL); type->value_range = save;
        type->value_range.tail = &extra; type->value_range.tail_count = 1;
        negative(&module, argv[3], NULL); type->value_range = save;
    }
    {
        asn1typed_type_t *packet = find(&module, "Packet");
        asn1typed_type_t *pick = find(&module, "Pick");
        REQUIRE(packet->fields[1].value_range.has_value_range);
        REQUIRE(packet->fields[1].value_range.lower_bound == -4 && packet->fields[1].value_range.upper_bound == 4);
        REQUIRE(packet->fields[2].value_range.lower_bound == 2 && packet->fields[2].value_range.upper_bound == 7);
        REQUIRE(packet->fields[3].value_range.lower_bound == -2 && packet->fields[3].value_range.upper_bound == 3);
        REQUIRE(pick->alternatives[0].value_range.lower_bound == -9 && pick->alternatives[0].value_range.upper_bound == -2);
        REQUIRE(pick->alternatives[1].value_range.lower_bound == 3 && pick->alternatives[1].value_range.upper_bound == 9);
    }
    {
        renderer standalone[] = {asn1typed_render_cpp_owned_integer_types,
            asn1typed_render_cpp_owned_integer_mapping, asn1typed_render_cpp_owned_integer_codec};
        const char *standalone_files[] = {"integer_types.hpp", "integer_mapping.hpp", "integer_codec.hpp"};
        for(i = 0; i < 3; ++i) {
            char *a = NULL, *b = NULL;
            REQUIRE(standalone[i](&integers, "integeronly", &a, diagnostic, sizeof(diagnostic)) == 0);
            REQUIRE(standalone[i](&integers, "integeronly", &b, diagnostic, sizeof(diagnostic)) == 0);
            REQUIRE(!strcmp(a, b)); write_file(argv[4], standalone_files[i], a); free(a); free(b);
            {
                long point;
                for(point = 0; point < 30000; ++point) {
                    char *out = NULL; int result;
                    allocation_countdown = point;
                    result = standalone[i](&integers, "integeronly", &out, diagnostic, sizeof(diagnostic));
                    allocation_countdown = -1;
                    if(result == 0) { REQUIRE(out); free(out); break; }
                    REQUIRE(result == -1 && !out && diagnostic[0]);
                }
                REQUIRE(point > 0 && point < 30000);
            }
            REQUIRE(standalone[i](&module, argv[3], &a, diagnostic, sizeof(diagnostic)) == -1);
            REQUIRE(!a && diagnostic[0]);
            REQUIRE(standalone[i](&integers, "nrforge::aper", &a, diagnostic, sizeof(diagnostic)) == -1);
            REQUIRE(!a && strstr(diagnostic, "namespace"));
            REQUIRE(standalone[i](&integers, "nrforge::aper::nested", &a, diagnostic, sizeof(diagnostic)) == -1);
            REQUIRE(!a && strstr(diagnostic, "namespace"));
            {
                asn1typed_type_t *type = &integers.types[0];
                asn1typed_type_t save = *type;
                type->ioc_container.source_name = (char *)"stale";
                REQUIRE(standalone[i](&integers, "integeronly", &a, diagnostic, sizeof(diagnostic)) == -1);
                REQUIRE(!a && diagnostic[0]); *type = save;
                type->sequence_extension_evidence = ASN1TYPED_WIRE_EVIDENCE_RESOLVED;
                REQUIRE(standalone[i](&integers, "integeronly", &a, diagnostic, sizeof(diagnostic)) == -1);
                REQUIRE(!a && diagnostic[0]); *type = save;
                type->field_count = 1;
                REQUIRE(standalone[i](&integers, "integeronly", &a, diagnostic, sizeof(diagnostic)) == -1);
                REQUIRE(!a && diagnostic[0]); *type = save;
            }
        }
    }
    asn1typed_module_clear(&integers);
    asn1typed_module_clear(&module);
    puts("PASS owned bounded INTEGER generation, effective intervals, deterministic lifetime and fail-closed allocation paths");
    return 0;
}
