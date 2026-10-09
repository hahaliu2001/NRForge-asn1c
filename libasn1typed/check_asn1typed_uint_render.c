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
static renderer renderers[]={asn1typed_render_cpp_owned_uint_types,asn1typed_render_cpp_owned_uint_mapping,asn1typed_render_cpp_owned_uint_codec};
static void negative(const asn1typed_module_t *m,const char *ns) { size_t i; for(i=0;i<3;i++) {char *s=NULL,d[512]={0}; REQUIRE(renderers[i](m,ns,&s,d,sizeof(d))==-1);REQUIRE(!s&&d[0]);} }
static void negative_diagnostic(const asn1typed_module_t *m,const char *ns,const char *part) {size_t i;for(i=0;i<3;i++){char *s=NULL,d[512]={0};REQUIRE(renderers[i](m,ns,&s,d,sizeof(d))==-1);REQUIRE(!s&&strstr(d,part));}}
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
 for(i=0;i<3;i++) {diagnostic[0]=0;REQUIRE(renderers[i](&m,argv[3],NULL,diagnostic,sizeof(diagnostic))==-1);REQUIRE(diagnostic[0]);}
 {size_t capacity=m.type_capacity;m.type_capacity=m.type_count-1;negative_diagnostic(&m,argv[3],"storage");m.type_capacity=capacity;}
 {char *name=m.types[0].identity.module;m.types[0].identity.module="OtherModule";negative_diagnostic(&m,argv[3],"identity");m.types[0].identity.module=name;}
 negative_diagnostic(&m,"a::INT64_MAX","namespace");
 negative(NULL,argv[3]);negative(&m,NULL);negative(&m,"std");negative(&m,"bad::");negative(&m,"errno");
 { asn1typed_type_t *t=&m.types[0];asn1typed_type_t save=*t;
  t->value_range.lower_bound=1;negative(&m,argv[3]);*t=save;
  t->value_range.lower_bound=-1;negative(&m,argv[3]);*t=save;
  t->value_range.upper_bound=254;negative(&m,argv[3]);*t=save;
  t->value_range.upper_bound=16777215;negative(&m,argv[3]);*t=save;
  t->value_range.has_value_range=0;negative(&m,argv[3]);*t=save;
  t->value_range.is_extensible=1;negative(&m,argv[3]);*t=save;
  t->is_extensible=1;negative(&m,argv[3]);*t=save;
  t->value_range.tail_count=1;negative(&m,argv[3]);*t=save;
  t->kind=ASN1TYPED_TYPE_SEQUENCE;negative(&m,argv[3]);*t=save;
  t->size_constraint.has_size_constraint=1;negative(&m,argv[3]);*t=save;
  t->field_count=1;negative(&m,argv[3]);*t=save;
  t->primitive_kind=ASN1TYPED_PRIMITIVE_BOOLEAN;negative(&m,argv[3]);*t=save;
  t->element_type.kind=ASN1TYPED_REF_NAMED;t->element_type.source_name="Alias";negative(&m,argv[3]);*t=save;
  t->has_ioc_table=1;negative(&m,argv[3]);*t=save;
 }
 {char *a=m.types[0].identity.source_name,*b=m.types[1].identity.source_name;
  m.types[0].identity.source_name="Foo-Bar";m.types[1].identity.source_name="fooBar";negative(&m,argv[3]);m.types[0].identity.source_name=a;m.types[1].identity.source_name=b;}
 {size_t count=m.bound_instance_count;m.bound_instance_count=1;negative(&m,argv[3]);m.bound_instance_count=count;}
 asn1typed_module_clear(&m);return 0;
}
