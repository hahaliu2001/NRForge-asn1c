#include "asn1typed_extract.h"
#include "asn1typed_render_cpp.h"
#include <asn1fix.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef NDEBUG
#error "Checks must remain active under NDEBUG"
#endif
#define REQUIRE(x) do { if(!(x)) { fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #x); abort(); } } while(0)
typedef int (*renderer)(const asn1typed_module_t *,const char *,char **,char *,size_t);
static renderer renderers[] = {asn1typed_render_cpp_owned_bit_types, asn1typed_render_cpp_owned_bit_mapping, asn1typed_render_cpp_owned_bit_codec};
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
    REQUIRE(asn1typed_extract_module(tree,"BitsGeneration",&m,diag,sizeof(diag))==0); asn1p_delete(tree);
    for(i=0;i<3;++i) {
        char *a=NULL,*b=NULL,path[4096]; FILE *f; long point;
        REQUIRE(renderers[i](&m,"bittests",&a,diag,sizeof(diag))==0);
        REQUIRE(renderers[i](&m,"bittests",&b,diag,sizeof(diag))==0); REQUIRE(!strcmp(a,b)); free(b);
        REQUIRE(snprintf(path,sizeof(path),"%s/%s",argv[2],files[i])>0); f=fopen(path,"wb"); REQUIRE(f);
        REQUIRE(fwrite(a,1,strlen(a),f)==strlen(a)); REQUIRE(fclose(f)==0); free(a);
        for(point=0;point<10000;++point) {
            countdown=point; a=NULL;
            { int rc=renderers[i](&m,"bittests",&a,diag,sizeof(diag)); countdown=-1;
              if(!rc) { REQUIRE(a); free(a); break; } REQUIRE(rc==-1 && !a && diag[0]); }
        }
        REQUIRE(point>0 && point<10000);
    }
    rejected(NULL,"bittests"); rejected(&m,"class");
    for(i=0;i<3;++i) REQUIRE(renderers[i](&m,"bittests",NULL,diag,sizeof(diag))==-1 && diag[0]);
    for(i=0;i<m.type_count;++i) if(m.types[i].primitive_kind==ASN1TYPED_PRIMITIVE_BIT_STRING) {
        asn1typed_size_constraint_t saved=m.types[i].size_constraint;
        m.types[i].size_constraint.is_extensible=1; rejected(&m,"bittests"); m.types[i].size_constraint=saved;
        m.types[i].size_constraint.has_size_constraint=1; m.types[i].size_constraint.upper_bound=65536;
        rejected(&m,"bittests"); m.types[i].size_constraint=saved; break;
    }
    for(i=0;i<m.type_count;++i) if(m.types[i].kind==ASN1TYPED_TYPE_SEQUENCE && m.types[i].field_count) {
        asn1typed_size_constraint_t saved=m.types[i].fields[0].size_constraint;
        m.types[i].fields[0].size_constraint.has_size_constraint=1;
        rejected(&m,"bittests"); m.types[i].fields[0].size_constraint=saved; break;
    }
    asn1typed_module_clear(&m); puts("PASS N15 bit generation/ownership/determinism/rejection/allocation"); return 0;
}
