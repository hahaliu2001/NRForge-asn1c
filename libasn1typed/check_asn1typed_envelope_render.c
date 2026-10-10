#include "asn1typed_extract.h"
#include "asn1typed_render_cpp.h"
#include "../tools/developer_tree.h"
#include <asn1fix.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define REQUIRE(x) do { if(!(x)) { fprintf(stderr,"%s:%d: %s\n",__FILE__,__LINE__,#x); abort(); } } while(0)
static long countdown = -1;
static int inject_allocations = 1;
void *__real_malloc(size_t);
void *__real_calloc(size_t,size_t);
void *__real_realloc(void *,size_t);
void *__wrap_malloc(size_t);
void *__wrap_calloc(size_t,size_t);
void *__wrap_realloc(void *,size_t);
static int fails(void) {
    if(countdown < 0) return 0;
    if(!countdown) { countdown = -1; return 1; }
    --countdown; return 0;
}
void *__wrap_malloc(size_t n) { return fails() ? NULL : __real_malloc(n); }
void *__wrap_calloc(size_t n,size_t z) { return fails() ? NULL : __real_calloc(n,z); }
void *__wrap_realloc(void *p,size_t n) { return fails() ? NULL : __real_realloc(p,n); }
typedef int (*renderer)(const asn1typed_module_t *,const asn1typed_target_envelope_t *,const char *,char **,char *,size_t);
static renderer renderers[] = {asn1typed_render_cpp_target_envelope_types,asn1typed_render_cpp_target_envelope_mapping,asn1typed_render_cpp_target_envelope_codec};
static char diagnostic[512];
static void save(const char *prefix,const char *kind,const char *text) {
    char path[2048]; FILE *file;
    REQUIRE(snprintf(path,sizeof(path),"%s_%s.hpp",prefix,kind) > 0);
    file = fopen(path,"wb"); REQUIRE(file);
    REQUIRE(fwrite(text,1,strlen(text),file) == strlen(text)); REQUIRE(fclose(file) == 0);
}
static void refused(const asn1typed_module_t *body,const asn1typed_target_envelope_t *descriptor,const char *ns) {
    for(size_t i = 0; i < 3; ++i) {
        char *output = NULL;
        REQUIRE(renderers[i](body,descriptor,ns,&output,diagnostic,sizeof(diagnostic)) == -1);
        REQUIRE(!output && diagnostic[0]);
    }
}
static void associate(asn1typed_target_envelope_t *descriptor,const asn1typed_module_t *body) {
    asn1typed_type_ref_t ref = {0};
    asn1typed_type_ref_t *payload = &descriptor->rows[descriptor->target_row_index].payloads[descriptor->roots[descriptor->target_root_ordinal].role];
    REQUIRE(asn1typed_type_ref_init(&ref,body->types[0].identity.module,body->types[0].identity.source_name) == 0);
    asn1typed_type_ref_clear(payload);
    REQUIRE(asn1typed_type_ref_copy(payload,&ref) == 0);
    REQUIRE(asn1typed_target_envelope_set_target(descriptor,&ref) == 0);
    REQUIRE(asn1typed_target_envelope_finalize(descriptor,diagnostic,sizeof(diagnostic)) == ASN1TYPED_WIRE_FINALIZE_OK);
    asn1typed_type_ref_clear(&ref);
}
static void generate(const asn1typed_module_t *body,const asn1typed_target_envelope_t *descriptor,const char *ns,const char *prefix) {
    const char *kinds[] = {"envelope_types","envelope_mapping","envelope_codec"};
    int (*body_renderers[])(const asn1typed_module_t*,const char*,char**,char*,size_t) = {
        asn1typed_render_cpp_owned_ioc_types,asn1typed_render_cpp_owned_ioc_mapping,asn1typed_render_cpp_owned_ioc_codec};
    const char *body_kinds[] = {"body_types","body_mapping","body_codec"};
    for(size_t i = 0; i < 3; ++i) {
        char *output = NULL,*again = NULL; long point;
        REQUIRE(body_renderers[i](body,ns,&output,diagnostic,sizeof(diagnostic)) == 0);
        save(prefix,body_kinds[i],output); free(output); output = NULL;
        REQUIRE(renderers[i](body,descriptor,ns,&output,diagnostic,sizeof(diagnostic)) == 0);
        REQUIRE(renderers[i](body,descriptor,ns,&again,diagnostic,sizeof(diagnostic)) == 0);
        REQUIRE(!strcmp(output,again)); free(again); save(prefix,kinds[i],output);
        for(point = 0; inject_allocations && point < 30000; ++point) {
            int result; again = NULL; countdown = point;
            result = renderers[i](body,descriptor,ns,&again,diagnostic,sizeof(diagnostic)); countdown = -1;
            if(!result) { REQUIRE(!strcmp(output,again)); free(again); break; }
            REQUIRE(result == -1 && !again && diagnostic[0]);
        }
        REQUIRE(!inject_allocations || (point > 0 && point < 30000)); free(output);
    }
}
/* A target may occupy exactly one procedure-row/role cell. Keep the
 * initiating fixture unchanged; exercise role selection with owned copies. */
static void outcome_variants(const asn1typed_module_t *body,
        const asn1typed_target_envelope_t *original, const char *prefix) {
    for(size_t role = 1; role < 3; ++role) {
        asn1typed_target_envelope_t changed = {0};
        asn1typed_envelope_row_t *row;
        char path[2048];
        REQUIRE(asn1typed_target_envelope_copy(&changed, original) == 0);
        row = &changed.rows[changed.target_row_index];
        asn1typed_type_ref_clear(&row->payloads[role]);
        REQUIRE(asn1typed_type_ref_copy(&row->payloads[role], &changed.target_body) == 0);
        row->payload_present[role] = 1;
        /* The same identity in two roles is ambiguous and must not publish. */
        REQUIRE(asn1typed_target_envelope_finalize(&changed, diagnostic, sizeof(diagnostic)) == ASN1TYPED_WIRE_FINALIZE_UNAVAILABLE);
        REQUIRE(!changed.has_valid_envelope && strstr(diagnostic, "ambiguous"));
        asn1typed_type_ref_clear(&row->payloads[0]);
        REQUIRE(asn1typed_type_ref_copy(&row->payloads[0], &changed.rows[0].payloads[0]) == 0);
        REQUIRE(asn1typed_target_envelope_finalize(&changed, diagnostic, sizeof(diagnostic)) == ASN1TYPED_WIRE_FINALIZE_OK);
        REQUIRE(changed.roots[changed.target_root_ordinal].role == (asn1typed_envelope_role_e)role);
        REQUIRE(snprintf(path, sizeof(path), "%s_%s", prefix, role == 1 ? "success" : "failure") > 0);
        generate(body, &changed, role == 1 ? "outcome::success" : "outcome::failure", path);
        asn1typed_target_envelope_clear(&changed);
    }
}
static int actual_variants(const char *list,const char *root,const char *directory) {
    dev_tree_options_t options = {root,list};
    asn1p_t *tree = dev_tree_load(&options,NULL);
    asn1typed_module_t body = {0};
    asn1typed_target_envelope_t original = {0},changed = {0},outcomes[2] = {0};
    asn1typed_module_t outcome_bodies[2] = {0};
    char prefix[2048]; size_t other = SIZE_MAX;
    inject_allocations = 0; /* Exhaustive OOM paths are covered by the small owned fixture. */
    REQUIRE(tree && dev_tree_fix(tree) >= 0);
    REQUIRE(asn1typed_extract_physical_message(tree,"NGAP-PDU-Contents","UEContextReleaseCommand",&body,diagnostic,sizeof(diagnostic)) == 0);
    REQUIRE(asn1typed_extract_target_envelope(tree,"NGAP-PDU-Descriptions","NGAP-PDU","NGAP-PDU-Contents","UEContextReleaseCommand",&original,diagnostic,sizeof(diagnostic)) == 0);
    {
        const char *messages[] = {"UEContextReleaseComplete", "AMFConfigurationUpdateFailure"};
        for(size_t role = 0; role < 2; ++role) {
            asn1typed_module_t *outcome_body = &outcome_bodies[role];
            asn1typed_target_envelope_t *outcome = &outcomes[role];
            REQUIRE(asn1typed_extract_physical_message(tree,"NGAP-PDU-Contents",messages[role],outcome_body,diagnostic,sizeof(diagnostic)) == 0);
            REQUIRE(asn1typed_extract_target_envelope(tree,"NGAP-PDU-Descriptions","NGAP-PDU","NGAP-PDU-Contents",messages[role],outcome,diagnostic,sizeof(diagnostic)) == 0);
            REQUIRE(outcome->roots[outcome->target_root_ordinal].role == (asn1typed_envelope_role_e)(role + 1));
            REQUIRE(snprintf(prefix,sizeof(prefix),"%s/frozen_%s",directory,role == 0 ? "success" : "failure") > 0);

        }
    }
    asn1p_delete(tree);
    for(size_t role = 0; role < 2; ++role) {
        REQUIRE(snprintf(prefix,sizeof(prefix),"%s/frozen_%s",directory,role == 0 ? "success" : "failure") > 0);
        generate(&outcome_bodies[role],&outcomes[role],role == 0 ? "frozen::success" : "frozen::failure",prefix);
        asn1typed_module_clear(&outcome_bodies[role]); asn1typed_target_envelope_clear(&outcomes[role]);
    }
    /* These are deliberate validated owned-IR mutations, not evidence that
     * the frozen source set declares a different procedure or tag order. */
    REQUIRE(asn1typed_target_envelope_copy(&changed,&original) == 0);
    for(size_t i = 0; i < changed.row_count; ++i) if(changed.rows[i].numeric_code == 73) other = i;
    REQUIRE(other != SIZE_MAX && changed.rows[changed.target_row_index].numeric_code == 41);
    changed.rows[other].numeric_code = 41;
    changed.rows[changed.target_row_index].numeric_code = 73;
    for(size_t i = 0; i < changed.root_count; ++i) changed.roots[i].effective_tag_number = i == 0 ? 9 : i == 1 ? 5 : 2;
    REQUIRE(asn1typed_target_envelope_finalize(&changed,diagnostic,sizeof(diagnostic)) == ASN1TYPED_WIRE_FINALIZE_OK);
    REQUIRE(snprintf(prefix,sizeof(prefix),"%s/code73",directory) > 0);
    generate(&body,&changed,"n10::body",prefix);
    asn1typed_target_envelope_clear(&changed);
    REQUIRE(asn1typed_target_envelope_copy(&changed,&original) == 0);
    changed.header.object_set_is_extensible = 0;
    REQUIRE(asn1typed_target_envelope_finalize(&changed,diagnostic,sizeof(diagnostic)) == ASN1TYPED_WIRE_FINALIZE_OK);
    REQUIRE(snprintf(prefix,sizeof(prefix),"%s/closed",directory) > 0);
    generate(&body,&changed,"n10::body",prefix);
    asn1typed_target_envelope_clear(&changed); asn1typed_target_envelope_clear(&original); asn1typed_module_clear(&body);
    puts("PASS actual Parser-deleted owned metadata variants: procedure73/reversed tags and closed table");
    return 0;
}
int main(int argc,char **argv) {
    asn1p_t *body_tree,*envelope_tree;
    asn1typed_module_t body = {0}; asn1typed_target_envelope_t descriptor = {0};
    if(argc == 5 && !strcmp(argv[1],"--actual")) return actual_variants(argv[2],argv[3],argv[4]);
    REQUIRE(argc == 5);
    body_tree = asn1p_parse_file(argv[1],A1P_NOFLAGS); envelope_tree = asn1p_parse_file(argv[2],A1P_NOFLAGS);
    REQUIRE(body_tree && envelope_tree && asn1f_process(body_tree,A1F_NOFLAGS,NULL) >= 0 && asn1f_process(envelope_tree,A1F_NOFLAGS,NULL) >= 0);
    REQUIRE(asn1typed_extract_physical_message(body_tree,"IOCCppGeneration","DispatchMessage",&body,diagnostic,sizeof(diagnostic)) == 0);
    REQUIRE(asn1typed_extract_target_envelope(envelope_tree,"EnvelopeEvidence","Envelope","EnvelopeBodies","TargetBody",&descriptor,diagnostic,sizeof(diagnostic)) == 0);
    asn1p_delete(body_tree); asn1p_delete(envelope_tree);
    associate(&descriptor,&body);
    generate(&body,&descriptor,"normal::nrforge",argv[3]);
    outcome_variants(&body,&descriptor,argv[3]);
    refused(NULL,&descriptor,"normal::nrforge"); refused(&body,NULL,"normal::nrforge"); refused(&body,&descriptor,"class");
    {
        char *name = body.types[0].identity.source_name;
        body.types[0].identity.source_name = "MismatchedBody"; refused(&body,&descriptor,"normal::nrforge"); body.types[0].identity.source_name = name;
        descriptor.has_valid_envelope = 0; refused(&body,&descriptor,"normal::nrforge"); descriptor.has_valid_envelope = 1;
        descriptor.roots[0].has_per_root_index = 0; refused(&body,&descriptor,"normal::nrforge"); descriptor.roots[0].has_per_root_index = 1;
        {
            char *root_name = descriptor.roots[0].source_name;
            char *field_name = descriptor.roots[0].field_names[2];
            descriptor.roots[0].source_name = "opaque";
            REQUIRE(asn1typed_target_envelope_finalize(&descriptor,diagnostic,sizeof(diagnostic)) == ASN1TYPED_WIRE_FINALIZE_OK);
            refused(&body,&descriptor,"normal::nrforge");
            descriptor.roots[0].source_name = root_name;
            descriptor.roots[0].field_names[2] = "errno";
            REQUIRE(asn1typed_target_envelope_finalize(&descriptor,diagnostic,sizeof(diagnostic)) == ASN1TYPED_WIRE_FINALIZE_OK);
            refused(&body,&descriptor,"normal::nrforge");
            descriptor.roots[0].field_names[2] = "received-policy";
            REQUIRE(asn1typed_target_envelope_finalize(&descriptor,diagnostic,sizeof(diagnostic)) == ASN1TYPED_WIRE_FINALIZE_OK);
            refused(&body,&descriptor,"normal::nrforge");
            descriptor.roots[0].field_names[2] = field_name;
            REQUIRE(asn1typed_target_envelope_finalize(&descriptor,diagnostic,sizeof(diagnostic)) == ASN1TYPED_WIRE_FINALIZE_OK);
        }
    }
    free(body.types[0].identity.module); free(body.types[0].identity.source_name);
    body.types[0].identity.module = strdup("ABC"); body.types[0].identity.source_name = strdup("DEF");
    REQUIRE(body.types[0].identity.module && body.types[0].identity.source_name);
    associate(&descriptor,&body);
    generate(&body,&descriptor,"upper::nrforge",argv[4]);
    asn1typed_module_clear(&body); asn1typed_target_envelope_clear(&descriptor);
    puts("PASS synthetic owned envelope rendering, deterministic outputs, failures and acronym identities");
    return 0;
}
