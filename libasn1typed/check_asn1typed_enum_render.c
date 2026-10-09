#include "asn1typed_extract.h"
#include "asn1typed_render_cpp.h"
#include <asn1fix.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define REQUIRE(x) do { if(!(x)) { fprintf(stderr,"%s:%d: %s\n",__FILE__,__LINE__,#x); abort(); } } while(0)
static long allocation_countdown=-1;
void *__real_malloc(size_t); void *__real_calloc(size_t,size_t); void *__real_realloc(void*,size_t);
void *__wrap_malloc(size_t); void *__wrap_calloc(size_t,size_t); void *__wrap_realloc(void*,size_t);
static int fails(void) { if(allocation_countdown<0) return 0; if(!allocation_countdown) {allocation_countdown=-1;return 1;} --allocation_countdown;return 0; }
void *__wrap_malloc(size_t n) { return fails()?NULL:__real_malloc(n); }
void *__wrap_calloc(size_t n,size_t z) { return fails()?NULL:__real_calloc(n,z); }
void *__wrap_realloc(void*p,size_t n) { return fails()?NULL:__real_realloc(p,n); }
typedef int (*renderer)(const asn1typed_module_t*,const char*,char**,char*,size_t);
static renderer renderers[]={asn1typed_render_cpp_owned_enum_types,asn1typed_render_cpp_owned_enum_mapping,asn1typed_render_cpp_owned_enum_codec};
static void negative(const asn1typed_module_t *m,const char *ns) { size_t i; for(i=0;i<3;i++) {char *s=NULL,d[512]={0}; REQUIRE(renderers[i](m,ns,&s,d,sizeof(d))==-1);REQUIRE(!s&&d[0]);} }
static void write_file(const char *directory,const char *name,const char *text) { char p[4096];FILE *f;int n=snprintf(p,sizeof(p),"%s/%s",directory,name);REQUIRE(n>0&&(size_t)n<sizeof(p)); f=fopen(p,"wb");REQUIRE(f&&fwrite(text,1,strlen(text),f)==strlen(text));REQUIRE(fclose(f)==0); }
int main(int argc,char **argv) {
 asn1typed_module_t m={0}; asn1p_t *tree; char diagnostic[512];size_t i; const char *files[]={"types.hpp","mapping.hpp","codec.hpp"};
 REQUIRE(argc==5); tree=asn1p_parse_file(argv[1],A1P_NOFLAGS);REQUIRE(tree&&asn1f_process(tree,A1F_NOFLAGS,NULL)>=0);
 REQUIRE(asn1typed_extract_module(tree,argv[2],&m,diagnostic,sizeof(diagnostic))==0);asn1p_delete(tree);
 for(i=0;i<3;i++) {char *a=NULL,*b=NULL;long point;REQUIRE(renderers[i](&m,argv[3],&a,diagnostic,sizeof(diagnostic))==0);REQUIRE(renderers[i](&m,argv[3],&b,diagnostic,sizeof(diagnostic))==0);REQUIRE(!strcmp(a,b));write_file(argv[4],files[i],a);free(a);free(b);
  for(point=0;point<20000;point++) {char *s=NULL;int result;allocation_countdown=point;result=renderers[i](&m,argv[3],&s,diagnostic,sizeof(diagnostic));allocation_countdown=-1;
   if(result==0){REQUIRE(s);free(s);break;} REQUIRE(result==-1&&!s&&diagnostic[0]); }
  REQUIRE(point>0&&point<20000);
 }
 { asn1typed_module_t large=m; asn1typed_type_t t={0}; size_t j;
  t.kind=ASN1TYPED_TYPE_ENUMERATED; t.identity=m.types[0].identity;
  for(j=0;j<256;j++){char name[32];REQUIRE(snprintf(name,sizeof(name),"item%zu",j)>0);REQUIRE(asn1typed_type_add_enum_item(&t,name,__FILE__,__LINE__)==0);REQUIRE(asn1typed_enum_item_set_numeric_evidence(&t,j,(intmax_t)j)==0);}
  REQUIRE(asn1typed_enumerated_evidence_finalize(&t,diagnostic,sizeof(diagnostic))==ASN1TYPED_WIRE_FINALIZE_OK);
  large.types=&t;large.type_count=1;large.type_capacity=1;negative(&large,argv[3]);
  memset(&t.identity,0,sizeof(t.identity));asn1typed_type_clear(&t);
 }
 for(i=0;i<3;i++) {diagnostic[0]=0;REQUIRE(renderers[i](&m,argv[3],NULL,diagnostic,sizeof(diagnostic))==-1);REQUIRE(diagnostic[0]);}
 {char *name=m.types[0].identity.source_name;m.types[0].identity.source_name="Known";negative(&m,argv[3]);m.types[0].identity.source_name=name;}
 {char *name=m.types[0].enum_items[0].source_name;m.types[0].enum_items[0].source_name="errno";negative(&m,argv[3]);m.types[0].enum_items[0].source_name=name;}
 negative(NULL,argv[3]);negative(&m,NULL);negative(&m,"std");negative(&m,"bad::");
 { asn1typed_type_t *t=&m.types[0]; int flag=t->has_valid_per_enumeration_mapping;
  t->has_valid_per_enumeration_mapping=0;negative(&m,argv[3]);t->has_valid_per_enumeration_mapping=flag;
  {size_t index=t->enum_items[0].per_enumeration_index;t->enum_items[0].per_enumeration_index=999;negative(&m,argv[3]);t->enum_items[0].per_enumeration_index=index;}
  {asn1typed_wire_evidence_e e=t->enum_items[0].numeric_evidence;t->enum_items[0].numeric_evidence=ASN1TYPED_WIRE_EVIDENCE_UNSUPPORTED;negative(&m,argv[3]);t->enum_items[0].numeric_evidence=e;}
  {asn1typed_type_kind_e k=t->kind;t->kind=ASN1TYPED_TYPE_SEQUENCE;negative(&m,argv[3]);t->kind=k;}
  {char *name=t->enum_items[1].source_name;if(t->enum_item_count>1){t->enum_items[1].source_name=t->enum_items[0].source_name;negative(&m,argv[3]);t->enum_items[1].source_name=name;}}
 }
 { asn1typed_type_t *t=&m.types[0];char *a=t->enum_items[0].source_name,*b=t->enum_items[1].source_name;
 t->enum_items[0].source_name="Foo-Bar";t->enum_items[1].source_name="fooBar";negative(&m,argv[3]);t->enum_items[0].source_name=a;t->enum_items[1].source_name=b;
 }
 if(m.type_count>1) {char *name=m.types[1].identity.source_name;m.types[1].identity.source_name=m.types[0].identity.source_name;negative(&m,argv[3]);m.types[1].identity.source_name=name;}
 {size_t count=m.bound_instance_count;m.bound_instance_count=1;negative(&m,argv[3]);m.bound_instance_count=count;}
 asn1typed_module_clear(&m);return 0;
}
