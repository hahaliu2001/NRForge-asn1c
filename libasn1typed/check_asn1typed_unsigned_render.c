#include "asn1typed_extract.h"
#include "asn1typed_render_cpp.h"
#include <asn1fix.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define REQUIRE(x) do { if(!(x)) { fprintf(stderr,"%d: %s\n",__LINE__,#x); abort(); } } while(0)
int main(int argc, char **argv) {
    asn1p_t *tree;
    asn1typed_module_t m = {0}, bad = {0};
    char why[512], path[4096];
    size_t i;
    const char *files[] = {"types.hpp", "mapping.hpp", "codec.hpp"};
    const char *unsupported[] = {"UnsignedExtension", "UnsignedOverflow", "UnsignedMixed", "UnsignedUnion"};
    int (*render[])(const asn1typed_module_t *, const char *, char **, char *, size_t) = {
        asn1typed_render_cpp_owned_shape_types, asn1typed_render_cpp_owned_shape_mapping,
        asn1typed_render_cpp_owned_shape_codec};
    REQUIRE(argc == 4);
    tree = asn1p_parse_file(argv[1], A1P_NOFLAGS);
    REQUIRE(tree && asn1f_process(tree, A1F_NOFLAGS, NULL) >= 0);
    REQUIRE(!asn1typed_extract_module(tree,"UnsignedValues",&m,why,sizeof(why)));
    for(i = 0; i < 4; ++i) {
        REQUIRE(asn1typed_extract_module(tree,unsupported[i],&bad,why,sizeof(why)) == -1);
        REQUIRE(!bad.types && !bad.type_count); asn1typed_module_clear(&bad);
    }
    asn1p_delete(tree);
    REQUIRE(asn1typed_integer_unsigned_valid(&m.types[0].value_range));
    REQUIRE(m.types[0].value_range.unsigned_upper_bound == UINT64_MAX);
    {
        asn1typed_integer_value_range_t save = m.types[0].value_range;
        asn1typed_type_t *choice; asn1typed_type_ref_t ref = {0}; asn1typed_module_t copied = {0};
        REQUIRE(!asn1typed_module_init(&copied,"Copy","copy",1));
        REQUIRE(!asn1typed_module_add_type(&copied,"Choice",ASN1TYPED_TYPE_CHOICE,"copy",1,&choice));
        REQUIRE(!asn1typed_type_ref_init_primitive(&ref,ASN1TYPED_PRIMITIVE_INTEGER));
        REQUIRE(!asn1typed_type_add_choice_alternative(choice,"good",&ref,NULL,&save,"copy",1));
        REQUIRE(choice->alternatives[0].value_range.unsigned_upper_bound == UINT64_MAX);
        for(i = 0; i < 6; ++i) {
            char *out = NULL;
            m.types[0].value_range = save;
            switch(i) {
            case 0: m.types[0].value_range.unsigned_bounds = 2; break;
            case 1: m.types[0].value_range.lower_bound = 1; break;
            case 2: m.types[0].value_range.is_extensible = 1; break;
            case 3: m.types[0].value_range.unsigned_lower_bound = UINT64_MAX; m.types[0].value_range.unsigned_upper_bound = 0; break;
            case 4: m.types[0].value_range.unsigned_bounds = 0; break;
            default: m.types[0].value_range.has_value_range = 0; break;
            }
            REQUIRE(asn1typed_type_add_choice_alternative(choice,"bad",&ref,NULL,&m.types[0].value_range,"copy",1) == -1);
            REQUIRE(render[0](&m,"unsignedtests",&out,why,sizeof(why)) == -1 && !out);
        }
        m.types[0].value_range = save; asn1typed_type_ref_clear(&ref); asn1typed_module_clear(&copied);
    }
    {
        asn1typed_module_t copied = {0}; asn1typed_type_t *sequence;
        asn1typed_type_t *state = NULL, *inline_state = NULL;
        for(i = 0; i < m.type_count; ++i) {
            if(!strcmp(m.types[i].identity.source_name,"State")) state = &m.types[i];
            if(!strcmp(m.types[i].identity.source_name,"InlineState")) inline_state = &m.types[i];
        }
        REQUIRE(state && inline_state && inline_state->fields[0].inline_enumerated);
        state->value_range.unsigned_upper_bound = 1;
        for(i = 0; i < 3; ++i) { char *out = NULL; REQUIRE(render[i](&m,"unsignedtests",&out,why,sizeof(why)) == -1 && !out); }
        state->value_range.unsigned_upper_bound = 0;
        REQUIRE(!asn1typed_module_init(&copied,"EnumCopy","copy",1));
        REQUIRE(!asn1typed_module_add_type(&copied,"Seq",ASN1TYPED_TYPE_SEQUENCE,"copy",1,&sequence));
        REQUIRE(!asn1typed_type_add_field_copy(sequence,&inline_state->fields[0]));
        inline_state->fields[0].inline_enumerated->value_range.unsigned_lower_bound = 1;
        REQUIRE(asn1typed_type_add_field_copy(sequence,&inline_state->fields[0]) == -1);
        for(i = 0; i < 3; ++i) { char *out = NULL; REQUIRE(render[i](&m,"unsignedtests",&out,why,sizeof(why)) == -1 && !out); }
        inline_state->fields[0].inline_enumerated->value_range.unsigned_lower_bound = 0;
        asn1typed_module_clear(&copied);
    }
    {
        asn1typed_target_envelope_t envelope = {0};
        tree = asn1p_parse_file(argv[3],A1P_NOFLAGS);
        REQUIRE(tree && asn1f_process(tree,A1F_NOFLAGS,NULL) >= 0);
        REQUIRE(asn1typed_extract_target_envelope(tree,"EnvelopeEvidence","Envelope","EnvelopeBodies","TargetBody",&envelope,why,sizeof(why)) == -1);
        REQUIRE(!envelope.has_valid_envelope && !envelope.rows && !envelope.roots);
        asn1typed_target_envelope_clear(&envelope); asn1p_delete(tree);
    }
    for(i = 0; i < 3; ++i) {
        char *out = NULL, *second = NULL; FILE *f;
        if(render[i](&m,"unsignedtests",&out,why,sizeof(why))) { fprintf(stderr,"render %zu: %s\n",i,why); abort(); }
        REQUIRE(!render[i](&m,"unsignedtests",&second,why,sizeof(why)));
        REQUIRE(!strcmp(out,second));
        REQUIRE(snprintf(path,sizeof(path),"%s/%s",argv[2],files[i]) > 0);
        f = fopen(path,"wb"); REQUIRE(f && fwrite(out,1,strlen(out),f) == strlen(out));
        REQUIRE(!fclose(f)); free(out); free(second);
    }
    asn1typed_module_clear(&m);
    return 0;
}
