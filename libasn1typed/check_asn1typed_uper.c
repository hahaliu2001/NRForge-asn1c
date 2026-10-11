#include "asn1typed_extract.h"
#include "asn1typed_render_uper.h"
#include <asn1fix.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef UPER_FIXTURE
#define UPER_FIXTURE "../tools/rrc-uper/compound.asn1"
#endif
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            fprintf(stderr, "uper-render:%d: %s\n", __LINE__, #x);                                 \
            abort();                                                                               \
        }                                                                                          \
    } while (0)
static long countdown = -1;
void *__real_malloc(size_t);
void *__real_calloc(size_t, size_t);
void *__real_realloc(void *, size_t);
static int fail(void) {
    if (countdown < 0)
        return 0;
    if (countdown-- == 0) {
        countdown = -1;
        return 1;
    }
    return 0;
}
void *__wrap_malloc(size_t n) { return fail() ? NULL : __real_malloc(n); }
void *__wrap_calloc(size_t n, size_t z) { return fail() ? NULL : __real_calloc(n, z); }
void *__wrap_realloc(void *p, size_t n) { return fail() ? NULL : __real_realloc(p, n); }
static char diagnostic[512];
static void reject(asn1typed_module_t *m) {
    char *out = NULL;
    CHECK(asn1typed_render_cpp_owned_uper(m, "test::generated", &out, diagnostic,
                                          sizeof(diagnostic)) < 0);
    CHECK(!out && diagnostic[0]);
}
int main(void) {
    asn1p_t *tree = asn1p_parse_file(UPER_FIXTURE, A1P_NOFLAGS);
    asn1typed_module_t m = {0};
    char *out = NULL;
    CHECK(tree && asn1f_process(tree, A1F_NOFLAGS, NULL) >= 0);
    CHECK(asn1typed_extract_root_graph(tree, "UperFixture", "Root", NULL, &m, diagnostic,
                                       sizeof(diagnostic)) == 0);
    asn1p_delete(tree);
    CHECK(asn1typed_render_cpp_owned_uper(&m, "test::generated", &out, diagnostic,
                                          sizeof(diagnostic)) == 0);
    CHECK(out);
    char *expected = out;
    long point;
    for (point = 0; point < 10000; ++point) {
        out = NULL;
        countdown = point;
        int rc = asn1typed_render_cpp_owned_uper(&m, "test::generated", &out, diagnostic,
                                                 sizeof(diagnostic));
        countdown = -1;
        if (rc == 0) {
            CHECK(out && !strcmp(out, expected));
            free(out);
            break;
        }
        CHECK(!out && diagnostic[0]);
    }
    CHECK(point > 0 && point < 10000);
    printf("UPER render all %ld allocation positions PASS\n", point);
    out = NULL;
    CHECK(asn1typed_render_cpp_owned_uper(&m, "nrforge::uper", &out, diagnostic,
                                          sizeof(diagnostic)) < 0 &&
          !out);
    size_t cap = m.type_capacity;
    m.type_capacity = 0;
    reject(&m);
    m.type_capacity = cap;
    int mapping = m.types[0].ordinary_structure_owned;
    m.types[0].ordinary_structure_owned = 0;
    reject(&m);
    m.types[0].ordinary_structure_owned = mapping;
    char *module = m.types[1].identity.module;
    m.types[1].identity.module = NULL;
    reject(&m);
    m.types[1].identity.module = module;
    asn1typed_field_t *f = &m.types[1].fields[0];
    asn1typed_presence_e presence = f->presence;
    f->presence = (asn1typed_presence_e)99;
    reject(&m);
    f->presence = presence;
    asn1typed_field_type_semantics_e semantics = f->type_semantics;
    f->type_semantics = ASN1TYPED_FIELD_INLINE_ENUMERATED;
    reject(&m);
    f->type_semantics = semantics;
    asn1typed_enum_item_t *items = m.types[5].enum_items;
    m.types[5].enum_items = NULL;
    reject(&m);
    m.types[5].enum_items = items;
    char *symbol = items[0].source_name;
    items[0].source_name = NULL;
    reject(&m);
    items[0].source_name = symbol;
    free(expected);
    asn1typed_module_clear(&m);
    return 0;
}
