#include "asn1typed_extract.h"
#include <asn1fix.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define REQUIRE(x) do { if(!(x)) { fprintf(stderr,"%s:%d: %s\n",__FILE__,__LINE__,#x); abort(); } } while(0)
#ifndef BIT_USE_SIZE_FIXTURE
#define BIT_USE_SIZE_FIXTURE "fixtures/bit-use-size-n15.asn1"
#endif
static char diagnostic[512];
static long countdown=-1;
void *n9_test_malloc(size_t);
void *n9_test_calloc(size_t,size_t);
void *n9_test_realloc(void*,size_t);
static int fails(void) { if(countdown<0)return 0; if(!countdown){countdown=-1;return 1;} --countdown;return 0; }
void *n9_test_malloc(size_t n){return fails()?NULL:malloc(n);}
void *n9_test_calloc(size_t n,size_t z){return fails()?NULL:calloc(n,z);}
void *n9_test_realloc(void*p,size_t n){return fails()?NULL:realloc(p,n);}
static int extract(asn1p_t*t,const char*n,asn1typed_module_t*m){return asn1typed_extract_physical_message(t,"BitUseSize",n,m,diagnostic,sizeof(diagnostic));}
static void rejects(asn1p_t*t,const char*n){asn1typed_module_t m={0};REQUIRE(extract(t,n,&m)==-1&&diagnostic[0]);REQUIRE(!m.types&&!m.source_name&&!m.bound_instances&&!m.ioc_registries);asn1typed_module_clear(&m);}
static asn1typed_type_t *find(asn1typed_module_t*m,const char*n){size_t i;for(i=0;i<m->type_count;++i)if(!strcmp(m->types[i].identity.source_name,n))return &m->types[i];REQUIRE(0);return NULL;}
static void size_ok(const asn1typed_size_constraint_t*s,int lo,int hi,int ext){REQUIRE(s->has_size_constraint&&s->lower_bound==lo&&s->upper_bound==hi&&s->is_extensible==ext);}
static asn1p_expr_t *field(asn1p_t*t,const char*n){asn1p_module_t*m;asn1p_expr_t*e;TQ_FOR(m,&t->modules,mod_next)TQ_FOR(e,&m->members,next)if(e->Identifier&&!strcmp(e->Identifier,n))return TQ_FIRST(&e->members);REQUIRE(0);return NULL;}
int main(void){
 asn1p_t*t=asn1p_parse_file(BIT_USE_SIZE_FIXTURE,A1P_NOFLAGS);asn1typed_module_t m={0};long point;
 REQUIRE(t&&asn1f_process(t,A1F_NOFLAGS,NULL)>=0);
 {int rc=extract(t,"Message",&m);if(rc)fprintf(stderr,"positive extraction: %s\n",diagnostic);REQUIRE(rc==0);asn1typed_module_clear(&m);}
 rejects(t,"BadMessage");rejects(t,"SetMessage");{ asn1typed_module_t addition={0}; REQUIRE(extract(t,"AdditionMessage",&addition)==0); asn1typed_type_t *a=find(&addition,"BadAddition"); REQUIRE(a && a->fields[0].size_constraint.is_extensible && a->fields[0].size_constraint.has_extension_addition); asn1typed_module_clear(&addition); }rejects(t,"NamedMessage");
 {asn1p_expr_t*f=field(t,"S"),*bad=field(t,"BadSet");asn1p_constraint_t*save=f->combined_constraints;
  f->combined_constraints=bad->constraints;rejects(t,"Message");f->combined_constraints=save;
  save=f->constraints;f->constraints=NULL;rejects(t,"Message");f->constraints=save;
  {asn1p_constraint_t*leaf=f->constraints->elements[0]->elements[0]->elements[0];
   asn1p_constraint_t**elements=leaf->elements;unsigned count=leaf->el_count;
   asn1p_constraint_t*child=bad->constraints;
   REQUIRE(leaf->type==ACT_EL_RANGE&&!count&&!elements);
   leaf->elements=&child;rejects(t,"Message");
   leaf->el_count=1;rejects(t,"Message");
   leaf->elements=elements;leaf->el_count=count;
  }
 }
 for(point=0;point<20000;++point){int rc;countdown=point;rc=extract(t,"Message",&m);countdown=-1;if(!rc)break;REQUIRE(!m.types&&!m.source_name&&!m.bound_instances&&!m.ioc_registries);asn1typed_module_clear(&m);}
 REQUIRE(point>0&&point<20000);asn1p_delete(t);
 {asn1typed_type_t*s=find(&m,"S"),*c=find(&m,"C");
 size_ok(&s->fields[0].size_constraint,3,17,0);size_ok(&s->fields[1].size_constraint,8,16,0);size_ok(&s->fields[2].size_constraint,2,3,0);size_ok(&s->fields[3].size_constraint,8,8,1);
 REQUIRE(s->fields[1].type.kind==ASN1TYPED_REF_NAMED&&!strcmp(s->fields[1].type.source_name,"Bits"));
 size_ok(&c->alternatives[0].size_constraint,1,2,0);size_ok(&c->alternatives[1].size_constraint,4,9,0);size_ok(&c->alternatives[2].size_constraint,3,3,1);
 REQUIRE(c->alternatives[1].type_ref.kind==ASN1TYPED_REF_NAMED);
 REQUIRE(m.ioc_registries[0].rows[0].payload_type.primitive_kind==ASN1TYPED_PRIMITIVE_BIT_STRING);
 REQUIRE(asn1typed_ioc_registry_validate(&m.ioc_registries[0],diagnostic,sizeof(diagnostic))==0);
 }
 asn1typed_module_clear(&m);asn1typed_module_clear(&m);puts("N15 use-site SIZE owned after Parser deletion PASS");return 0;
}
