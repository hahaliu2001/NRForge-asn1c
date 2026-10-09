#include "asn1typed_extract.h"
#include <asn1fix.h>
#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define REQUIRE(x) do { if(!(x)) { fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #x); abort(); } } while(0)
#ifndef ENUM_FIXTURE
#define ENUM_FIXTURE "fixtures/enumerated-evidence-n3.asn1"
#endif
static long allocation_countdown = -1;
void *__wrap_malloc(size_t);
void *__wrap_calloc(size_t, size_t);
void *__wrap_realloc(void *, size_t);
void *__real_malloc(size_t);
void *__real_calloc(size_t, size_t);
void *__real_realloc(void *, size_t);
static int allocation_fails(void) {
 if(allocation_countdown < 0) return 0;
 if(allocation_countdown == 0) { allocation_countdown = -1; return 1; }
 --allocation_countdown; return 0;
}
void *__wrap_malloc(size_t n) { return allocation_fails() ? NULL : __real_malloc(n); }
void *__wrap_calloc(size_t n, size_t z) { return allocation_fails() ? NULL : __real_calloc(n,z); }
void *__wrap_realloc(void *p, size_t n) { return allocation_fails() ? NULL : __real_realloc(p,n); }
static char diagnostic[256];
static asn1typed_type_t *find(asn1typed_module_t *m, const char *name) {
 size_t i; for(i=0;i<m->type_count;i++) if(!strcmp(m->types[i].identity.source_name,name)) return &m->types[i];
 return NULL;
}
static void no_indexes(const asn1typed_type_t *t) {
 size_t i; REQUIRE(!t->has_valid_per_enumeration_mapping);
 for(i=0;i<t->enum_item_count;i++) REQUIRE(!t->enum_items[i].has_per_enumeration_index && t->enum_items[i].per_enumeration_index==0);
}
static void verify(asn1typed_type_t *t, const intmax_t *values, const size_t *ranks, size_t n) {
 size_t i; REQUIRE(t && t->enum_item_count==n);
 REQUIRE(asn1typed_enumerated_evidence_validate(t,diagnostic,sizeof(diagnostic))==0);
 for(i=0;i<n;i++) { REQUIRE(t->enum_items[i].numeric_evidence==ASN1TYPED_WIRE_EVIDENCE_RESOLVED);
  REQUIRE(t->enum_items[i].assigned_number==values[i]); REQUIRE(t->enum_items[i].has_per_enumeration_index);
  REQUIRE(t->enum_items[i].per_enumeration_index==ranks[i]); }
}
static void real_and_copies(void) {
 asn1typed_module_t m={0}; asn1p_t *tree=asn1p_parse_file(ENUM_FIXTURE,A1P_NOFLAGS);
 asn1typed_type_t destination={0}; asn1typed_type_t *body; asn1typed_field_t *field;
 const intmax_t plain[]={0,1,2},reordered[]={9,2,5},implicit[]={5,1,0},negative[]={-3,4,0}, additions[]={0,25,1,30},below[]={0,3,1},singleton[]={0};
 const size_t p[]={0,1,2},r[]={2,0,1},im[]={2,1,0},neg[]={0,2,1},ad[]={0,1,0,1},bl[]={0,1,0},one[]={0};
 REQUIRE(tree && asn1f_process(tree,A1F_NOFLAGS,NULL)>=0);
 REQUIRE(asn1typed_extract_module(tree,"EnumeratedEvidence",&m,diagnostic,sizeof(diagnostic))==0);
 asn1p_delete(tree);
 verify(find(&m,"Plain"),plain,p,3); verify(find(&m,"Reordered"),reordered,r,3);
 verify(find(&m,"Implicit"),implicit,im,3); verify(find(&m,"Negative"),negative,neg,3);
 verify(find(&m,"Additions"),additions,ad,4); verify(find(&m,"BelowRoot"),below,bl,3);
 verify(find(&m,"Singleton"),singleton,one,1); verify(find(&m,"EmptyAdditions"),singleton,one,1);
 if(INTMAX_MAX == INT64_MAX) {
 const intmax_t endpoints[]={INTMAX_MIN,INTMAX_MAX}; const size_t er[]={0,1};
 verify(find(&m,"Endpoints"),endpoints,er,2);
 body=find(&m,"Wide"); no_indexes(body);
 REQUIRE(body->enum_items[1].numeric_evidence==ASN1TYPED_WIRE_EVIDENCE_UNSUPPORTED);
}
 body=find(&m,"Holder")->fields[0].inline_enumerated; verify(body,reordered,r,3);
 destination.kind=ASN1TYPED_TYPE_SEQUENCE;
 REQUIRE(asn1typed_type_add_inline_enumerated_field(&destination,"copied",body,ASN1TYPED_PRESENCE_MANDATORY,__FILE__,__LINE__)==0);
 field=&find(&m,"Holder")->fields[0];
 REQUIRE(asn1typed_type_add_field_copy(&destination,field)==0);
 body->enum_items[0].per_enumeration_index=0;
 REQUIRE(asn1typed_type_add_inline_enumerated_field(&destination,"stale",body,ASN1TYPED_PRESENCE_MANDATORY,__FILE__,__LINE__)==-1);
 REQUIRE(asn1typed_type_add_field_copy(&destination,field)==-1); REQUIRE(destination.field_count==2);
 REQUIRE(asn1typed_enumerated_evidence_finalize(body,diagnostic,sizeof(diagnostic))==ASN1TYPED_WIRE_FINALIZE_OK);
 REQUIRE(asn1typed_enum_item_set_numeric_unavailable(body,0)==0); no_indexes(body);
 REQUIRE(asn1typed_type_add_inline_enumerated_field(&destination,"incomplete",body,ASN1TYPED_PRESENCE_MANDATORY,__FILE__,__LINE__)==0);
 REQUIRE(asn1typed_type_add_field_copy(&destination,field)==0);
 asn1typed_module_clear(&m);
 verify(destination.fields[0].inline_enumerated,reordered,r,3); verify(destination.fields[1].inline_enumerated,reordered,r,3);
 no_indexes(destination.fields[2].inline_enumerated); no_indexes(destination.fields[3].inline_enumerated);
 REQUIRE(destination.fields[2].inline_enumerated->enum_items[0].numeric_evidence==ASN1TYPED_WIRE_EVIDENCE_UNAVAILABLE);
 REQUIRE(destination.fields[3].inline_enumerated->enum_items[1].assigned_number==2);
 asn1typed_type_clear(&destination);
}
static void allocation_tests(void) {
 asn1typed_type_t body={0}, holder={0}, destination={0}; long point; int copied;
 body.kind=ASN1TYPED_TYPE_ENUMERATED; holder.kind=ASN1TYPED_TYPE_SEQUENCE;
 REQUIRE(asn1typed_type_add_enum_item(&body,"a",__FILE__,__LINE__)==0);
 REQUIRE(asn1typed_type_add_enum_item(&body,"b",__FILE__,__LINE__)==0);
 REQUIRE(asn1typed_enum_item_set_numeric_evidence(&body,0,0)==0);
 REQUIRE(asn1typed_enum_item_set_numeric_evidence(&body,1,1)==0);
 allocation_countdown=0;
 REQUIRE(asn1typed_enumerated_evidence_finalize(&body,diagnostic,sizeof(diagnostic))==ASN1TYPED_WIRE_FINALIZE_ERROR);
 REQUIRE(allocation_countdown==-1); no_indexes(&body);
 REQUIRE(asn1typed_enumerated_evidence_finalize(&body,diagnostic,sizeof(diagnostic))==ASN1TYPED_WIRE_FINALIZE_OK);
 REQUIRE(asn1typed_type_add_inline_enumerated_field(&holder,"source",&body,ASN1TYPED_PRESENCE_MANDATORY,__FILE__,__LINE__)==0);
 for(copied=0;copied<2;copied++) {
  for(point=0;point<100;point++) {
   int result; memset(&destination,0,sizeof(destination)); destination.kind=ASN1TYPED_TYPE_SEQUENCE;
   allocation_countdown=point;
   result=copied ? asn1typed_type_add_field_copy(&destination,&holder.fields[0]) :
    asn1typed_type_add_inline_enumerated_field(&destination,"target",&body,ASN1TYPED_PRESENCE_MANDATORY,__FILE__,__LINE__);
   allocation_countdown=-1;
   if(result==0) { REQUIRE(destination.field_count==1); REQUIRE(asn1typed_enumerated_evidence_validate(destination.fields[0].inline_enumerated,diagnostic,sizeof(diagnostic))==0); asn1typed_type_clear(&destination); break; }
   REQUIRE(result==-1 && destination.field_count==0); asn1typed_type_clear(&destination);
  }
  REQUIRE(point>0 && point<100);
 }
 asn1typed_type_clear(&holder); asn1typed_type_clear(&body);
}
static void handbuilt(void) {
 asn1typed_type_t t={0},wrong={0}; size_t i; const intmax_t numbers[]={INTMAX_MAX,INTMAX_MIN,0};
 const size_t ranks[]={2,0,1}; t.kind=ASN1TYPED_TYPE_ENUMERATED;
 for(i=0;i<3;i++) { const char *names[]={"high","low","zero"}; REQUIRE(asn1typed_type_add_enum_item(&t,names[i],__FILE__,__LINE__)==0); }
 REQUIRE(asn1typed_enumerated_evidence_finalize(&t,diagnostic,sizeof(diagnostic))==ASN1TYPED_WIRE_FINALIZE_UNAVAILABLE); no_indexes(&t);
 for(i=0;i<3;i++) REQUIRE(asn1typed_enum_item_set_numeric_evidence(&t,i,numbers[i])==0);
 REQUIRE(asn1typed_enumerated_evidence_finalize(&t,diagnostic,sizeof(diagnostic))==ASN1TYPED_WIRE_FINALIZE_OK); verify(&t,numbers,ranks,3);
 REQUIRE(asn1typed_enum_item_set_numeric_evidence(&t,3,1)==-1); verify(&t,numbers,ranks,3);
 REQUIRE(asn1typed_enum_item_set_numeric_evidence(&wrong,0,1)==-1);
 wrong.kind=ASN1TYPED_TYPE_SEQUENCE; wrong.has_valid_per_enumeration_mapping=1;
 REQUIRE(asn1typed_enumerated_evidence_finalize(&wrong,diagnostic,sizeof(diagnostic))==ASN1TYPED_WIRE_FINALIZE_ERROR);
 REQUIRE(wrong.kind==ASN1TYPED_TYPE_SEQUENCE && wrong.has_valid_per_enumeration_mapping==1);
 REQUIRE(asn1typed_enumerated_evidence_finalize(NULL,diagnostic,sizeof(diagnostic))==ASN1TYPED_WIRE_FINALIZE_ERROR);
 t.has_valid_per_enumeration_mapping=0;
 REQUIRE(asn1typed_enumerated_evidence_validate(&t,diagnostic,sizeof(diagnostic))==-1);
 t.has_valid_per_enumeration_mapping=1; verify(&t,numbers,ranks,3);
 t.enum_items[0].per_enumeration_index=1; REQUIRE(asn1typed_enumerated_evidence_validate(&t,diagnostic,sizeof(diagnostic))==-1);
 REQUIRE(asn1typed_enumerated_evidence_finalize(&t,diagnostic,sizeof(diagnostic))==ASN1TYPED_WIRE_FINALIZE_OK);
 REQUIRE(asn1typed_enum_item_set_numeric_evidence(&t,0,0)==0); no_indexes(&t);
 REQUIRE(asn1typed_enumerated_evidence_finalize(&t,diagnostic,sizeof(diagnostic))==ASN1TYPED_WIRE_FINALIZE_UNAVAILABLE); no_indexes(&t);
 REQUIRE(asn1typed_enum_item_set_numeric_unsupported(&t,0)==0); REQUIRE(t.enum_items[0].assigned_number==0);
 REQUIRE(asn1typed_enumerated_evidence_finalize(&t,diagnostic,sizeof(diagnostic))==ASN1TYPED_WIRE_FINALIZE_UNAVAILABLE);
 REQUIRE(asn1typed_enum_item_set_numeric_evidence(&t,0,INTMAX_MAX)==0);
 t.enum_items[1].is_extension_addition=1;
 REQUIRE(asn1typed_enumerated_evidence_finalize(&t,diagnostic,sizeof(diagnostic))==ASN1TYPED_WIRE_FINALIZE_UNAVAILABLE); no_indexes(&t);
 t.enum_items[1].is_extension_addition=0;
 t.is_extensible=1; t.enum_items[1].is_extension_addition=1; t.enum_items[2].is_extension_addition=0;
 REQUIRE(asn1typed_enumerated_evidence_finalize(&t,diagnostic,sizeof(diagnostic))==ASN1TYPED_WIRE_FINALIZE_UNAVAILABLE); no_indexes(&t);
 t.is_extensible=0; t.enum_items[1].is_extension_addition=0;
 REQUIRE(asn1typed_enumerated_evidence_finalize(&t,diagnostic,sizeof(diagnostic))==ASN1TYPED_WIRE_FINALIZE_OK);
 { char *name=t.enum_items[1].source_name; t.enum_items[1].source_name=t.enum_items[0].source_name;
 REQUIRE(asn1typed_enumerated_evidence_finalize(&t,diagnostic,sizeof(diagnostic))==ASN1TYPED_WIRE_FINALIZE_UNAVAILABLE); no_indexes(&t);
 t.enum_items[1].source_name=name; }
 t.is_extensible=1; t.enum_items[1].is_extension_addition=1; t.enum_items[2].is_extension_addition=1;
 REQUIRE(asn1typed_enum_item_set_numeric_evidence(&t,1,5)==0);
 REQUIRE(asn1typed_enum_item_set_numeric_evidence(&t,2,2)==0);
 REQUIRE(asn1typed_enumerated_evidence_finalize(&t,diagnostic,sizeof(diagnostic))==ASN1TYPED_WIRE_FINALIZE_UNAVAILABLE); no_indexes(&t);
 REQUIRE(asn1typed_enum_item_set_numeric_evidence(&t,2,6)==0);
 REQUIRE(asn1typed_enumerated_evidence_finalize(&t,diagnostic,sizeof(diagnostic))==ASN1TYPED_WIRE_FINALIZE_OK);
 t.enum_items[2].has_per_enumeration_index=0;
 REQUIRE(asn1typed_enumerated_evidence_validate(&t,diagnostic,sizeof(diagnostic))==-1);
 t.enum_items[1].is_extension_addition=0; t.enum_items[2].is_extension_addition=0; t.is_extensible=0;
 REQUIRE(asn1typed_enum_item_set_numeric_evidence(&t,1,INTMAX_MIN)==0);
 REQUIRE(asn1typed_enum_item_set_numeric_evidence(&t,2,0)==0);
 REQUIRE(asn1typed_enumerated_evidence_finalize(&t,diagnostic,sizeof(diagnostic))==ASN1TYPED_WIRE_FINALIZE_OK);
 REQUIRE(asn1typed_type_add_enum_item(&t,"later",__FILE__,__LINE__)==0); no_indexes(&t);
 REQUIRE(asn1typed_enum_item_set_numeric_evidence(&t,3,1)==0);
 REQUIRE(asn1typed_enumerated_evidence_finalize(&t,diagnostic,sizeof(diagnostic))==ASN1TYPED_WIRE_FINALIZE_OK);
 { size_t capacity=t.enum_item_capacity; t.enum_item_capacity=1; REQUIRE(asn1typed_enumerated_evidence_finalize(&t,diagnostic,sizeof(diagnostic))==ASN1TYPED_WIRE_FINALIZE_ERROR); t.enum_item_capacity=capacity; }
 asn1typed_type_clear(&t); REQUIRE(!t.has_valid_per_enumeration_mapping && !t.enum_item_count);
}
int main(void) { real_and_copies(); handbuilt(); allocation_tests(); return 0; }
