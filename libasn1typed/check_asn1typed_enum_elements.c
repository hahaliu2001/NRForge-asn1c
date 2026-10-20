#include "asn1typed_extract.h"
#include "asn1typed_render_uper.h"
#include <asn1fix.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(c) do { if(!(c)) { fprintf(stderr, "enum elements:%d: %s\n", __LINE__, #c); exit(1); } } while(0)
void *__real_malloc(size_t);
void *__real_calloc(size_t, size_t);
void *__real_realloc(void *, size_t);
static int armed;
static size_t index_, fail_at = (size_t)-1;
static int fail(void) { return armed && index_++ == fail_at; }
void *__wrap_malloc(size_t n) { return fail() ? NULL : __real_malloc(n); }
void *__wrap_calloc(size_t n, size_t z) { return fail() ? NULL : __real_calloc(n, z); }
void *__wrap_realloc(void *p, size_t n) { return fail() ? NULL : __real_realloc(p, n); }
static asn1typed_type_t *find(asn1typed_module_t *g, const char *name) {
    for(size_t i = 0; i < g->type_count; ++i)
        if(!strcmp(g->types[i].identity.source_name, name)) return &g->types[i];
    return NULL;
}
static asn1typed_type_t *enum_element(asn1typed_module_t *g, const char *list) {
    asn1typed_type_t *t = find(g, list);
    CHECK(t && t->kind == ASN1TYPED_TYPE_SEQUENCE_OF);
    CHECK(t->element_type.kind == ASN1TYPED_REF_NAMED);
    t = find(g, t->element_type.source_name);
    CHECK(t && t->kind == ASN1TYPED_TYPE_ENUMERATED);
    CHECK(t->has_valid_per_enumeration_mapping);
    return t;
}
int main(void) {
    const char source[] = "Enums DEFINITIONS AUTOMATIC TAGS ::= BEGIN\n"
        "List ::= SEQUENCE (SIZE (1..2)) OF ENUMERATED {semiStatic, dynamic}\n"
        "Fields ::= SEQUENCE { first SEQUENCE (SIZE (1..3)) OF ENUMERATED {noInterruption, interruption}, second SEQUENCE (SIZE (1..2)) OF ENUMERATED {ms0, notSupported} OPTIONAL }\n"
        "ExtList ::= SEQUENCE (SIZE (1..2)) OF ENUMERATED {zero, one, ..., extra}\n"
        "Nested ::= SEQUENCE (SIZE (1..2)) OF SEQUENCE (SIZE (1..2)) OF ENUMERATED {a, b}\n"
        "SetupRelease {T} ::= CHOICE {release NULL, setup T}\n"
        "Wrapped ::= SEQUENCE {list SetupRelease {List} OPTIONAL}\nEND\n";
    const char *roots[] = {"List", "Fields", "ExtList", "Nested", "Wrapped"};
    asn1typed_module_t graphs[5] = {{0}}, scratch = {0};
    char error[1024] = {0};
    asn1p_t *tree = asn1p_parse_buffer(source, -1, "enum-elements.asn", 1, A1P_NOFLAGS);
    CHECK(tree && asn1f_process(tree, A1F_NOFLAGS, NULL) >= 0);
    for(size_t i = 0; i < 5; ++i) {
        int rc = asn1typed_extract_root_graph(tree, "Enums", roots[i], NULL, &graphs[i], error, sizeof(error));
        if(rc) fprintf(stderr, "%s: %s\n", roots[i], error);
        CHECK(rc == 0);
    }
    armed = 1; index_ = 0;
    CHECK(asn1typed_extract_root_graph(tree, "Enums", "Fields", NULL, &scratch, error, sizeof(error)) == 0);
    armed = 0;
    size_t allocations = index_;
    asn1typed_module_clear(&scratch);
    for(size_t i = 0; i < allocations; ++i) {
        armed = 1; index_ = 0; fail_at = i;
        int rc = asn1typed_extract_root_graph(tree, "Enums", "Fields", NULL, &scratch, error, sizeof(error));
        armed = 0;
        CHECK(rc == -1 && !scratch.types && !scratch.type_count);
        asn1typed_module_clear(&scratch);
    }
    asn1p_delete(tree);
    asn1typed_type_t *e = enum_element(&graphs[0], "List");
    CHECK(e->enum_item_count == 2 && !e->is_extensible);
    CHECK(!strcmp(e->enum_items[0].source_name, "semiStatic"));
    CHECK(!strcmp(e->enum_items[1].source_name, "dynamic"));
    CHECK(find(&graphs[0], "List")->size_constraint.upper_bound == 2);
    e = enum_element(&graphs[1], "$inline$Fields$first");
    CHECK(!strcmp(e->enum_items[0].source_name, "noInterruption"));
    asn1typed_type_t *other = enum_element(&graphs[1], "$inline$Fields$second");
    CHECK(e != other && !strcmp(other->enum_items[0].source_name, "ms0"));
    e = enum_element(&graphs[2], "ExtList");
    CHECK(e->is_extensible && e->enum_item_count == 3 && e->enum_items[2].is_extension_addition);
    CHECK(enum_element(&graphs[3], "$inline$Nested$@element")->enum_item_count == 2);
    CHECK(enum_element(&graphs[4], "List")->enum_item_count == 2);
    for(size_t i = 0; i < 5; ++i) {
        char *header = NULL;
        CHECK(asn1typed_render_cpp_owned_uper(&graphs[i], "enum_elements", &header, error, sizeof(error)) == 0);
        CHECK(header && strstr(header, "enum_elements"));
        free(header); asn1typed_module_clear(&graphs[i]);
    }
    printf("PASS anonymous enum collection ownership/rendering after parser destruction; %zu allocation rollback positions\n", allocations);
    return 0;
}
