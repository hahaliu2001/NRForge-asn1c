#include "asn1typed_extract.h"
#include "asn1typed_render_cpp.h"
#include "asn1typed_render_cpp_internal.h"
#include <asn1fix.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define REQUIRE(x) do { if(!(x)) { fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #x); abort(); } } while(0)
static long countdown = -1;
void *__real_malloc(size_t);
void *__real_calloc(size_t, size_t);
void *__real_realloc(void *, size_t);
void *__wrap_malloc(size_t);
void *__wrap_calloc(size_t, size_t);
void *__wrap_realloc(void *, size_t);
static int fail_alloc(void) {
    if(countdown < 0) return 0;
    if(!countdown) { countdown = -1; return 1; }
    --countdown; return 0;
}
void *__wrap_malloc(size_t n) { return fail_alloc() ? NULL : __real_malloc(n); }
void *__wrap_calloc(size_t n, size_t z) { return fail_alloc() ? NULL : __real_calloc(n, z); }
void *__wrap_realloc(void *p, size_t n) { return fail_alloc() ? NULL : __real_realloc(p, n); }
typedef int (*renderer)(const asn1typed_module_t *, const char *, char **, char *, size_t);
static renderer renderers[] = {asn1typed_render_cpp_owned_ioc_types, asn1typed_render_cpp_owned_ioc_mapping, asn1typed_render_cpp_owned_ioc_codec};
static const char *suffixes[] = {"types", "mapping", "codec"};
static char diagnostic[512];
static void rejected(const asn1typed_module_t *m, const char *ns) {
    size_t i;
    for(i = 0; i < 3; ++i) {
        char *output = NULL;
        REQUIRE(renderers[i](m, ns, &output, diagnostic, sizeof(diagnostic)) == -1);
        REQUIRE(!output && diagnostic[0]);
    }
}
static void save(const char *prefix, const char *kind, const char *text) {
    char path[2048]; FILE *file;
    REQUIRE(snprintf(path, sizeof(path), "%s_%s.hpp", prefix, kind) > 0);
    file = fopen(path, "wb"); REQUIRE(file);
    REQUIRE(fwrite(text, 1, strlen(text), file) == strlen(text)); REQUIRE(fclose(file) == 0);
}
static char *identity_name(const asn1typed_type_ref_t *ref, asn1typed_name_style_e style) {
    char raw[2048]; size_t j, used;
    int n = snprintf(raw, sizeof(raw), "%s-%s", ref->module, ref->source_name);
    REQUIRE(n > 0 && (size_t)n < sizeof(raw)); used = (size_t)n;
    for(j = 0; j < ref->actual_count; ++j) {
        n = snprintf(raw + used, sizeof(raw) - used, "-%s-%s", ref->actuals[j].module, ref->actuals[j].source_name);
        REQUIRE(n > 0 && (size_t)n < sizeof(raw) - used); used += (size_t)n;
    }
    return asn1typed_render_cpp_final_name(raw, style);
}
int main(int argc, char **argv) {
    asn1p_t *tree; asn1typed_module_t m = {0};
    size_t i, entry = SIZE_MAX; char *outputs[3] = {0};
    char *type_name, *api_name, *entry_name, *entry_api;
    asn1typed_type_ref_t root = {0};
    REQUIRE(argc == 6);
    tree = asn1p_parse_file(argv[1], A1P_NOFLAGS);
    REQUIRE(tree && asn1f_process(tree, A1F_NOFLAGS, NULL) >= 0);
    REQUIRE(asn1typed_extract_physical_message(tree, argv[2], argv[3], &m, diagnostic, sizeof(diagnostic)) == 0);
    asn1p_delete(tree);
    for(i = 0; i < m.bound_instance_count; ++i)
        if(m.bound_instances[i].has_empty_private_binding) { REQUIRE(entry == SIZE_MAX); entry = i; }
    REQUIRE(entry != SIZE_MAX);
    REQUIRE(asn1typed_type_ref_init(&root, argv[2], argv[3]) == 0);
    type_name = identity_name(&root, ASN1TYPED_NAME_TYPE);
    api_name = identity_name(&root, ASN1TYPED_NAME_FIELD);
    entry_name = identity_name(&m.bound_instances[entry].identity, ASN1TYPED_NAME_TYPE);
    entry_api = identity_name(&m.bound_instances[entry].identity, ASN1TYPED_NAME_FIELD);
    REQUIRE(type_name && api_name && entry_name && entry_api);
    for(i = 0; i < 3; ++i) {
        char *again = NULL; long point;
        REQUIRE(renderers[i](&m, argv[4], &outputs[i], diagnostic, sizeof(diagnostic)) == 0);
        REQUIRE(renderers[i](&m, argv[4], &again, diagnostic, sizeof(diagnostic)) == 0);
        REQUIRE(!strcmp(outputs[i], again)); free(again);
        save(argv[5], suffixes[i], outputs[i]);
        for(point = 0; point < 30000; ++point) {
            int rc; again = NULL; countdown = point;
            rc = renderers[i](&m, argv[4], &again, diagnostic, sizeof(diagnostic)); countdown = -1;
            if(rc == 0) { REQUIRE(!strcmp(outputs[i], again)); free(again); break; }
            REQUIRE(rc == -1 && !again && diagnostic[0]);
        }
        REQUIRE(point > 0 && point < 30000);
    }
    {
        asn1typed_bound_instance_t *b = &m.bound_instances[entry];
        char *saved = b->identity.actuals[0].source_name;
        b->identity.actuals[0].source_name = "WrongSet";
        rejected(&m, argv[4]); b->identity.actuals[0].source_name = saved;
        for(long point = 0; point < 2; ++point) {
            char *module = b->empty_private_object_set.module;
            char *source = b->empty_private_object_set.source_name;
            countdown = point;
            REQUIRE(asn1typed_bound_instance_set_empty_private_binding(&m, entry) == -1);
            countdown = -1;
            REQUIRE(b->empty_private_object_set.module == module && b->empty_private_object_set.source_name == source && b->has_empty_private_binding == 1);
        }
    }
    rejected(NULL, argv[4]); rejected(&m, "class");
    {
        m.bound_instances[entry].has_empty_private_binding = 0;
        rejected(&m, argv[4]);
        m.bound_instances[entry].has_empty_private_binding = 1;
        char *saved = m.bound_instances[entry].body.fields[2].class_field_relation.selector_source_name;
        m.bound_instances[entry].body.fields[2].class_field_relation.selector_source_name = "wrong";
        rejected(&m, argv[4]);
        m.bound_instances[entry].body.fields[2].class_field_relation.selector_source_name = saved;
    }
    {
        char adapter[8192];
        int n = snprintf(adapter, sizeof(adapter),
            "namespace %s {\nusing Message = %s;\nusing MessageMapping = %s_aper;\n"
            "using Container = MessageMapping::field_0_type;\nusing ContainerMapping = MessageMapping::field_0_payload_mapping;\n"
            "using Entry = ContainerMapping::element_type;\nusing EntryMapping = ContainerMapping::element_payload_mapping;\n"
            "inline auto encode_message(const Message& v, const ::nrforge::aper::Limits& l = {}) { return encode_%s(v,l); }\n"
            "inline auto decode_message(::std::span<const ::std::byte> v, const ::nrforge::aper::Limits& l = {}) { return decode_%s(v,l); }\n"
            "inline auto encode_entry(const Entry& v, const ::nrforge::aper::Limits& l = {}) { return encode_%s(v,l); }\n"
            "inline auto decode_entry(::std::span<const ::std::byte> v, const ::nrforge::aper::Limits& l = {}) { return decode_%s(v,l); }\n"
            "inline auto put_entry(::nrforge::aper::FieldWriter& f, const Entry& v) { return compound_codec::put_%s(f,v); }\n"
            "inline auto get_entry(::nrforge::aper::FieldReader& f) { return compound_codec::get_%s(f); }\n}\n",
            argv[4], type_name, type_name, api_name, api_name, entry_api, entry_api, entry_name, entry_name);
        REQUIRE(n > 0 && (size_t)n < sizeof(adapter)); save(argv[5], "adapters", adapter);
    }
    for(i = 0; i < 3; ++i) free(outputs[i]);
    free(type_name); free(api_name); free(entry_name); free(entry_api);
    asn1typed_type_ref_clear(&root); asn1typed_module_clear(&m);
    puts("PASS IOC owned lifecycle, deterministic generation, refusals and allocation failure sweep");
    return 0;
}
