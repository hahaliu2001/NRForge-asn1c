#include "asn1typed_extract.h"
#include "asn1typed_render_cpp.h"
#include <asn1fix.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define REQUIRE(x) do { if(!(x)) { fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #x); abort(); } } while(0)
typedef int (*renderer)(const asn1typed_module_t *,const char *,char **,char *,size_t);
static renderer renderers[] = {asn1typed_render_cpp_owned_size_types, asn1typed_render_cpp_owned_size_mapping, asn1typed_render_cpp_owned_size_codec};
static long countdown = -1;
void *__real_malloc(size_t);
void *__real_calloc(size_t,size_t);
void *__real_realloc(void *,size_t);
void *__wrap_malloc(size_t);
void *__wrap_calloc(size_t,size_t);
void *__wrap_realloc(void *,size_t);
static int fail_alloc(void) { if(countdown < 0) return 0; if(!countdown) { countdown=-1; return 1; } --countdown; return 0; }
void *__wrap_malloc(size_t n) { return fail_alloc() ? NULL : __real_malloc(n); }
void *__wrap_calloc(size_t n,size_t z) { return fail_alloc() ? NULL : __real_calloc(n,z); }
void *__wrap_realloc(void *p,size_t n) { return fail_alloc() ? NULL : __real_realloc(p,n); }
static void rejected(const asn1typed_module_t *m,const char *ns) {
    size_t i; char diag[512];
    for(i=0;i<3;++i) { char *out=NULL; REQUIRE(renderers[i](m,ns,&out,diag,sizeof(diag))==-1); REQUIRE(!out && diag[0]); }
}
int main(int argc,char **argv) {
    asn1typed_module_t m={0}; asn1p_t *tree; char diag[512]; size_t i;
    const char *files[]={"types.hpp","mapping.hpp","codec.hpp"};
    REQUIRE(argc==3); tree=asn1p_parse_file(argv[1],A1P_NOFLAGS);
    REQUIRE(tree && asn1f_process(tree,A1F_NOFLAGS,NULL)>=0);
    REQUIRE(asn1typed_extract_module(tree,"SizeGeneration",&m,diag,sizeof(diag))==0); asn1p_delete(tree);
    for(i=0;i<3;++i) {
        char *a=NULL,*b=NULL,path[4096]; FILE *f; long point;
        REQUIRE(renderers[i](&m,"sizetest",&a,diag,sizeof(diag))==0);
        REQUIRE(renderers[i](&m,"sizetest",&b,diag,sizeof(diag))==0); REQUIRE(!strcmp(a,b)); free(b);
        REQUIRE(snprintf(path,sizeof(path),"%s/%s",argv[2],files[i])>0); f=fopen(path,"wb"); REQUIRE(f);
        REQUIRE(fwrite(a,1,strlen(a),f)==strlen(a)); REQUIRE(fclose(f)==0); free(a);
        for(point=0;point<10000;++point) {
            countdown=point; a=NULL;
            { int rc=renderers[i](&m,"sizetest",&a,diag,sizeof(diag)); countdown=-1;
              if(!rc) { REQUIRE(a); free(a); break; } REQUIRE(rc==-1 && !a && diag[0]); }
        }
        REQUIRE(point>0 && point<10000);
    }
    /* New SIZE metadata cannot silently pass historical non-SIZE APIs. */
    for(i=0;i<m.type_count;++i) if(m.types[i].primitive_kind==ASN1TYPED_PRIMITIVE_INTEGER) {
        asn1typed_module_t one=m; asn1typed_type_t type=m.types[i];
        renderer legacy[]={asn1typed_render_cpp_owned_slice,asn1typed_render_cpp_owned_aper_mapping,asn1typed_render_cpp_owned_aper_codec,
            asn1typed_render_cpp_owned_uint_types,asn1typed_render_cpp_owned_uint_mapping,asn1typed_render_cpp_owned_uint_codec,
            asn1typed_render_cpp_owned_integer_types,asn1typed_render_cpp_owned_integer_mapping,asn1typed_render_cpp_owned_integer_codec};
        size_t j; one.types=&type;one.type_count=one.type_capacity=1;
        for(j=0;j<sizeof(legacy)/sizeof(*legacy);++j) {
            char *out=NULL;
            REQUIRE(legacy[j](&one,"residual",&out,diag,sizeof(diag))==0);free(out);out=NULL;
            type.size_constraint.extension_lower_bound=17;
            REQUIRE(legacy[j](&one,"residual",&out,diag,sizeof(diag))==-1 && !out && diag[0]);
            type.size_constraint.extension_lower_bound=0;
            type.size_constraint.has_extension_addition=1;
            REQUIRE(legacy[j](&one,"residual",&out,diag,sizeof(diag))==-1 && !out && diag[0]);
            type.size_constraint.has_extension_addition=0;
        }
        break;
    }
    for(i=0;i<m.type_count;++i) if(m.types[i].kind==ASN1TYPED_TYPE_SEQUENCE) {
        asn1typed_size_constraint_t saved=m.types[i].fields[3].size_constraint;
        m.types[i].fields[3].size_constraint.is_extensible=1;
        rejected(&m,"sizetest"); m.types[i].fields[3].size_constraint=saved; break;
    }
    rejected(NULL,"sizetest"); rejected(&m,"class");
    for(i=0;i<m.type_count;++i) if(m.types[i].kind==ASN1TYPED_TYPE_PRIMITIVE) {
        asn1typed_size_constraint_t saved=m.types[i].size_constraint;
        m.types[i].size_constraint.extension_lower_bound=42; m.types[i].size_constraint.has_extension_addition=0;
        rejected(&m,"sizetest"); m.types[i].size_constraint=saved; break;
    }
    asn1typed_module_clear(&m); puts("PASS extensible SIZE ownership/determinism/rejection/allocation"); return 0;
}
