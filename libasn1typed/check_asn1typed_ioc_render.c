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
static void structural_rejections(asn1typed_module_t *m, size_t entry, const char *ns) {
    size_t i;
    if(m->ioc_registries[0].row_count) {
        asn1typed_type_ref_t *payload = &m->ioc_registries[0].rows[0].payload_type;
        char *saved_module = payload->module, *saved_source = payload->source_name;
        payload->module = "AbsentModule"; rejected(m,ns); payload->module = saved_module;
        payload->source_name = "AbsentType"; rejected(m,ns); payload->source_name = saved_source;
    }
    {
        asn1typed_type_actual_t *actual = &m->bound_instances[entry].identity.actuals[0];
        char *saved = actual->source_name;
        actual->source_name = "OtherRows"; rejected(m,ns); actual->source_name = saved;
    }
    for(i = 0; i < m->type_count; ++i) {
        asn1typed_type_t *type = &m->types[i];
        if(type->kind == ASN1TYPED_TYPE_PRIMITIVE && type->primitive_kind == ASN1TYPED_PRIMITIVE_BOOLEAN) {
            asn1typed_size_constraint_t size = type->size_constraint;
            type->size_constraint.has_size_constraint = 1;
            rejected(m,ns); type->size_constraint = size;
            break;
        }
    }
    if(m->type_count > 1) {
        asn1typed_type_identity_t saved = m->types[1].identity;
        m->types[1].identity = m->types[0].identity;
        rejected(m,ns); m->types[1].identity = saved;
    }
    for(i = 0; i < m->type_count; ++i) {
        asn1typed_type_t *type = &m->types[i];
        if(type->kind == ASN1TYPED_TYPE_SEQUENCE && type->field_count && !strcmp(type->identity.source_name,"Pair")) {
            asn1typed_type_ref_t save = type->fields[0].type;
            type->fields[0].type.module = type->identity.module;
            type->fields[0].type.source_name = type->identity.source_name;
            rejected(m,ns); type->fields[0].type = save;
        }
    }
    {
        size_t index;
        REQUIRE(asn1typed_module_add_ioc_registry(m,"Orphan","ORPHAN-CLASS","Orphan","Rows","Value",&index) == 0);
        REQUIRE(asn1typed_ioc_registry_set_evidence(&m->ioc_registries[index],0,1) == 0);
        REQUIRE(asn1typed_ioc_registry_finalize(&m->ioc_registries[index],diagnostic,sizeof(diagnostic)) == ASN1TYPED_WIRE_FINALIZE_OK);
        rejected(m,ns);
        /* The orphan stays owned until module_clear; no subsequent success
         * generation is attempted on this deliberately unsupported graph. */
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
        if(m.bound_instances[i].ioc_binding.has_valid_binding) { REQUIRE(entry == SIZE_MAX); entry = i; }
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
    rejected(NULL, argv[4]); rejected(&m, "class");
    {
        asn1typed_field_t *value = &m.bound_instances[entry].body.fields[m.bound_instances[entry].ioc_binding.value_field_ordinal];
        char *saved = value->source_name;
        value->source_name = "errno"; rejected(&m, argv[4]); value->source_name = saved;
        value->source_name = m.bound_instances[entry].body.fields[0].source_name;
        rejected(&m, argv[4]); value->source_name = saved;
        m.ioc_registries[0].has_valid_dispatch = 0; rejected(&m, argv[4]); m.ioc_registries[0].has_valid_dispatch = 1;
        m.bound_instances[entry].ioc_binding.has_valid_binding = 0; rejected(&m, argv[4]); m.bound_instances[entry].ioc_binding.has_valid_binding = 1;
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
    structural_rejections(&m,entry,argv[4]);
    for(i = 0; i < 3; ++i) free(outputs[i]);
    free(type_name); free(api_name); free(entry_name); free(entry_api);
    asn1typed_type_ref_clear(&root); asn1typed_module_clear(&m);
    puts("PASS IOC owned lifecycle, deterministic generation, refusals and allocation failure sweep");
    return 0;
}
