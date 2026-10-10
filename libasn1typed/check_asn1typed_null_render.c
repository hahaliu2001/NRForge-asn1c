#include "asn1typed_extract.h"
#include "asn1typed_render_cpp.h"
#include "asn1typed_render_cpp_ioc_internal.h"
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
    asn1typed_render_cpp_owned_null_types,
    asn1typed_render_cpp_owned_null_mapping,
    asn1typed_render_cpp_owned_null_codec
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
    asn1typed_module_t module = {0}, renamed = {0}; asn1p_t *tree;
    char diagnostic[512]; size_t i;
    const char *files[] = {"types.hpp", "mapping.hpp", "codec.hpp"};
    REQUIRE(argc == 4);
    tree = asn1p_parse_file(argv[1], A1P_NOFLAGS);
    REQUIRE(tree && asn1f_process(tree, A1F_NOFLAGS, NULL) >= 0);
    REQUIRE(asn1typed_extract_module(tree, "NullGeneration", &module, diagnostic, sizeof(diagnostic)) == 0);
    REQUIRE(asn1typed_extract_module(tree, "NullRenamed", &renamed, diagnostic, sizeof(diagnostic)) == 0);
    asn1p_delete(tree);
    REQUIRE(find(&module, "Nothing")->primitive_kind == ASN1TYPED_PRIMITIVE_NULL);
    REQUIRE(find(&module, "Packet")->fields[0].type.primitive_kind == ASN1TYPED_PRIMITIVE_NULL);
    REQUIRE(find(&module, "Select")->alternatives[1].per_root_index == 0);
    REQUIRE(find(&module, "Trigger")->alternatives[0].inline_enumerated != NULL);
    REQUIRE(find(&module, "Trigger")->alternatives[0].inline_enumerated->has_valid_per_enumeration_mapping);
    REQUIRE(!find(&module, "Trigger")->alternatives[0].type_ref.module);
    { long n; const asn1typed_type_t *source = find(&module, "Trigger")->alternatives[0].inline_enumerated;
      for(n = 0; n < 10000; ++n) {
        asn1typed_type_t copy = {0}; int rc; copy.kind = ASN1TYPED_TYPE_CHOICE;
        allocation_countdown = n;
        rc = asn1typed_type_add_inline_enumerated_alternative(&copy, "item", source, "owned-test", 1);
        allocation_countdown = -1;
        if(rc) REQUIRE(copy.alternative_count == 0);
        else { REQUIRE(copy.alternative_count == 1 && copy.alternatives[0].inline_enumerated != source);
               REQUIRE(copy.alternatives[0].inline_enumerated->enum_items[0].source_name != source->enum_items[0].source_name);
               REQUIRE(!copy.alternatives[0].type_ref.module && !copy.alternatives[0].has_per_root_index); }
        asn1typed_type_clear(&copy); if(!rc) break;
      }
      REQUIRE(n < 10000);
      /* Each malformed identity-free enum body must fail before publication;
       * a shallow local probe leaves the original owned body untouched. */
#define BAD_BODY(member, value) do { \
        asn1typed_type_t bad = *source, owner = {0}; \
        owner.kind = ASN1TYPED_TYPE_CHOICE; bad.member = (value); \
        REQUIRE(asn1typed_type_add_inline_enumerated_alternative( \
            &owner, "item", &bad, "owned-test", 1) == -1); \
        REQUIRE(!owner.alternative_count && !owner.alternatives); \
        asn1typed_type_clear(&owner); \
      } while(0)
      BAD_BODY(size_constraint.is_extensible, 1);
      BAD_BODY(size_constraint.lower_bound, 1);
      BAD_BODY(size_constraint.upper_bound, 1);
      BAD_BODY(value_range.is_extensible, 1);
      BAD_BODY(value_range.lower_bound, 1);
      BAD_BODY(value_range.upper_bound, 1);
      BAD_BODY(location.line, 1);
      BAD_BODY(element_type.kind, ASN1TYPED_REF_PRIMITIVE);
      BAD_BODY(element_type.primitive_kind, ASN1TYPED_PRIMITIVE_BOOLEAN);
      BAD_BODY(ioc_container.kind, ASN1TYPED_REF_PRIMITIVE);
      BAD_BODY(ioc_container.primitive_kind, ASN1TYPED_PRIMITIVE_BOOLEAN);
      BAD_BODY(has_valid_per_root_mapping, 1);
      BAD_BODY(sequence_extension_evidence, ASN1TYPED_WIRE_EVIDENCE_RESOLVED);
      BAD_BODY(sequence_root_field_count, 1);
      BAD_BODY(sequence_known_addition_count, 1);
      BAD_BODY(has_valid_sequence_extension_structure, 1);
      BAD_BODY(enum_item_capacity, SIZE_MAX);
#undef BAD_BODY
    }
    for(i = 0; i < 3; ++i) {
        char *a = NULL, *b = NULL; long n;
        REQUIRE(renderers[i](&module, "nulltest", &a, diagnostic, sizeof(diagnostic)) == 0);
        REQUIRE(renderers[i](&module, "nulltest", &b, diagnostic, sizeof(diagnostic)) == 0);
        REQUIRE(!strcmp(a, b)); write_file(argv[2], files[i], a); free(a); free(b);
        for(n = 0; n < 10000; ++n) {
            int rc; char *out = NULL;
            allocation_countdown = n;
            rc = renderers[i](&module, "nulltest", &out, diagnostic, sizeof(diagnostic));
            allocation_countdown = -1;
            if(!rc) { free(out); break; }
            REQUIRE(!out && diagnostic[0]);
            REQUIRE(find(&module, "Nothing")->primitive_kind == ASN1TYPED_PRIMITIVE_NULL);
        }
        REQUIRE(n < 10000);
    }
    { char *out = NULL; REQUIRE(asn1typed_render_cpp_owned_shape_types(&module, "nulltest", &out, diagnostic, sizeof(diagnostic)) == -1); REQUIRE(!out); }
    negative(&module, "std", "namespace");
    { asn1typed_type_t *body = find(&module, "Trigger")->alternatives[0].inline_enumerated;
      int saved = body->has_valid_per_enumeration_mapping;
      body->has_valid_per_enumeration_mapping = 0;
      negative(&module, "nulltest", ""); body->has_valid_per_enumeration_mapping = saved;
    }
    { asn1typed_size_constraint_t *site = &find(&module, "Trigger")->alternatives[0].size_constraint;
      site->has_extension_addition = 1; negative(&module, "nulltest", NULL); site->has_extension_addition = 0;
      site->extension_lower_bound = 16; negative(&module, "nulltest", NULL); site->extension_lower_bound = 0;
      site->extension_upper_bound = 16; negative(&module, "nulltest", NULL); site->extension_upper_bound = 0;
    }
    find(&module, "Nothing")->value_range.has_value_range = 1;
    negative(&module, "nulltest", "metadata");
    find(&module, "Nothing")->value_range.has_value_range = 0;
    find(&module, "Nothing")->size_constraint.is_extensible = 1;
    negative(&module, "nulltest", "metadata");
    find(&module, "Nothing")->size_constraint.is_extensible = 0;
    for(i = 0; i < 3; ++i) {
        char *out = NULL, file[32];
        REQUIRE(renderers[i](&renamed, "renamed_null", &out, diagnostic, sizeof(diagnostic)) == 0);
        REQUIRE(snprintf(file, sizeof(file), "renamed_%s", files[i]) > 0);
        write_file(argv[2], file, out); free(out);
    }
    { asn1typed_module_t physical = {0}; const char *safe[] = {"EnvelopeShell"}; const char *collision[] = {"IocNullPayloadsPayload"};
      tree = asn1p_parse_file(argv[3], A1P_NOFLAGS);
      REQUIRE(tree && asn1f_process(tree, A1F_NOFLAGS, NULL) >= 0);
      REQUIRE(asn1typed_extract_physical_message(tree, "IOCNullGeneration", "Message", &physical, diagnostic, sizeof(diagnostic)) == 0);
      asn1p_delete(tree);
      REQUIRE(asn1typed_render_cpp_ioc_check_names(&physical, "iocnull", safe, 1, diagnostic, sizeof(diagnostic)) == 0);
      REQUIRE(asn1typed_render_cpp_ioc_check_names(&physical, "iocnull", collision, 1, diagnostic, sizeof(diagnostic)) == -1);
      REQUIRE(diagnostic[0]); asn1typed_module_clear(&physical);
    }
    asn1typed_module_clear(&renamed);
    asn1typed_module_clear(&module);
    puts("PASS NULL owned extraction, naming, zero-payload shape and OOM cleanup"); return 0;
}
