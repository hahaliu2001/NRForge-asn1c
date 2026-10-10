#include "asn1typed_extract.h"
#include <asn1fix.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#define REQUIRE(x) do { if(!(x)) { fprintf(stderr,"%s:%d: %s\n",__FILE__,__LINE__,#x); abort(); } } while(0)
#ifndef INTEGER_USE_RANGE_FIXTURE
#define INTEGER_USE_RANGE_FIXTURE "fixtures/integer-use-range-n16.asn1"
#endif
static char diagnostic[512];
static long countdown = -1;
void *n9_test_malloc(size_t);
void *n9_test_calloc(size_t,size_t);
void *n9_test_realloc(void*,size_t);
static int fails(void) { if(countdown < 0) return 0; if(!countdown) { countdown=-1; return 1; } --countdown; return 0; }
void *n9_test_malloc(size_t n) { return fails() ? NULL : malloc(n); }
void *n9_test_calloc(size_t n,size_t z) { return fails() ? NULL : calloc(n,z); }
void *n9_test_realloc(void *p,size_t n) { return fails() ? NULL : realloc(p,n); }
static int extract(asn1p_t *t,const char *n,asn1typed_module_t *m) {
    return asn1typed_extract_physical_message(t,"IntegerUseRange",n,m,diagnostic,sizeof(diagnostic));
}
static void rejects(asn1p_t *t,const char *n) {
    asn1typed_module_t m={0}; REQUIRE(extract(t,n,&m)==-1 && diagnostic[0]);
    REQUIRE(!m.types && !m.source_name && !m.bound_instances && !m.ioc_registries);
    asn1typed_module_clear(&m);
}
static asn1typed_type_t *find(asn1typed_module_t *m,const char *n) {
    size_t i; for(i=0;i<m->type_count;++i) if(!strcmp(m->types[i].identity.source_name,n)) return &m->types[i];
    REQUIRE(0); return NULL;
}
static asn1p_expr_t *source(asn1p_t *t,const char *n) {
    asn1p_module_t *m; asn1p_expr_t *e;
    TQ_FOR(m,&t->modules,mod_next) TQ_FOR(e,&m->members,next)
        if(e->Identifier && !strcmp(e->Identifier,n)) return e;
    REQUIRE(0); return NULL;
}
static void interval(const asn1typed_integer_value_range_t *r,int lo,int hi,int ext) {
    REQUIRE(r->has_value_range && r->lower_bound==lo && r->upper_bound==hi && r->is_extensible==ext);
    REQUIRE(!r->tail && !r->tail_count);
}
static void union_ok(const asn1typed_integer_value_range_t *r) {
    REQUIRE(r->has_value_range && !r->is_extensible && r->lower_bound==1 && r->upper_bound==3);
    REQUIRE(r->tail_count==1 && r->tail && r->tail[0].lower_bound==8 && r->tail[0].upper_bound==10);
}
static void malformed(asn1p_t *t) {
    asn1p_expr_t *f=TQ_FIRST(&source(t,"S")->members), *named=TQ_NEXT(f,next);
    asn1p_expr_t *alt=TQ_FIRST(&source(t,"C")->members);
    asn1p_constraint_t *save=f->combined_constraints, *saved_decl;
    asn1p_constraint_t *leaf=f->constraints->elements[0], *child=alt->constraints;
    asn1p_constraint_t **elements=leaf->elements; unsigned count=leaf->el_count;
    REQUIRE(leaf->type==ACT_EL_RANGE && !count && !elements);
    f->combined_constraints=alt->constraints; rejects(t,"Message"); f->combined_constraints=save;
    saved_decl=f->constraints; f->constraints=NULL; rejects(t,"Message"); f->constraints=saved_decl;
    leaf->elements=&child; rejects(t,"Message"); leaf->el_count=1; rejects(t,"Message");
    leaf->elements=elements; leaf->el_count=count;
    /* An unrelated effective range is not declaration-owned evidence. */
    save=named->combined_constraints; named->combined_constraints=alt->constraints;
    rejects(t,"Message"); named->combined_constraints=save;
    {
        asn1p_constraint_t *range;
        asn1p_value_t lower, upper;
        REQUIRE(save && save->type==ACT_CA_SET && save->el_count==2);
        range=save->elements[1]; REQUIRE(range->type==ACT_EL_RANGE);
        lower=*range->range_start; upper=*range->range_stop;
        range->range_start->value.v_integer=200; range->range_stop->value.v_integer=300;
        rejects(t,"Message"); /* Empty inherited intersection must not publish. */
        *range->range_start=lower; *range->range_stop=upper;
    }
    save=alt->constraints; alt->constraints=NULL; rejects(t,"Message"); alt->constraints=save;
}
int main(void) {
    asn1p_t *t=asn1p_parse_file(INTEGER_USE_RANGE_FIXTURE,A1P_NOFLAGS);
    asn1typed_module_t m={0}, plain={0}; long point;
    REQUIRE(t && asn1f_process(t,A1F_NOFLAGS,NULL)>=0);
    { int rc=extract(t,"Message",&m); if(rc) fprintf(stderr,"positive: %s\n",diagnostic); REQUIRE(rc==0); asn1typed_module_clear(&m); }
    REQUIRE(extract(t,"BadMessage",&m)==0);
    { const asn1typed_integer_value_range_t *r = &find(&m,"BadAddition")->fields[0].value_range;
      REQUIRE(r->extension_addition_count==1 && r->extension_additions[0].lower_bound==256 && r->extension_additions[0].upper_bound==300);
    }
    asn1typed_module_clear(&m);
    REQUIRE(extract(t,"SizedMessage",&m)==0);
    {
        const asn1typed_type_ref_t *ref = &m.ioc_registries[0].rows[0].payload_type;
        asn1typed_type_t *payload;
        REQUIRE(ref->kind==ASN1TYPED_REF_NAMED && ref->source_name);
        payload=find(&m,ref->source_name);
        REQUIRE(payload->kind==ASN1TYPED_TYPE_PRIMITIVE && payload->primitive_kind==ASN1TYPED_PRIMITIVE_INTEGER);
        interval(&payload->value_range,2,7,0);
        REQUIRE(!payload->value_range.extension_additions && !payload->value_range.extension_addition_count);
    }
    asn1typed_module_clear(&m); malformed(t);
    /* Historical physical evidence can own an unconstrained builtin INTEGER;
     * the unchanged IOC generator whitelist still refuses its wire codec. */
    REQUIRE(extract(t,"PlainMessage",&plain)==0);
    REQUIRE(plain.ioc_registries[0].rows[0].payload_type.primitive_kind==ASN1TYPED_PRIMITIVE_INTEGER);
    asn1typed_module_clear(&plain);
    for(point=0;point<30000;++point) {
        int rc; countdown=point; rc=extract(t,"Message",&m); countdown=-1;
        if(!rc) break;
        REQUIRE(diagnostic[0] && !m.types && !m.source_name && !m.bound_instances && !m.ioc_registries);
        asn1typed_module_clear(&m);
    }
    REQUIRE(point>0 && point<30000); asn1p_delete(t);
    {
        asn1typed_type_t *s=find(&m,"S"), *c=find(&m,"C");
        interval(&s->fields[0].value_range,-5,5,0); interval(&s->fields[1].value_range,-8,16,0);
        interval(&s->fields[2].value_range,0,255,1); union_ok(&s->fields[3].value_range);
        REQUIRE(s->fields[1].type.kind==ASN1TYPED_REF_NAMED && !strcmp(s->fields[1].type.source_name,"Base"));
        interval(&c->alternatives[0].value_range,10,265,0); interval(&c->alternatives[1].value_range,-9,9,0);
        interval(&c->alternatives[2].value_range,0,255,1); union_ok(&c->alternatives[3].value_range);
        REQUIRE(c->alternatives[1].type_ref.kind==ASN1TYPED_REF_NAMED);
        REQUIRE(find(&m,"Wide")->value_range.lower_bound==INT64_MIN && find(&m,"Wide")->value_range.upper_bound==INT64_MAX);
        REQUIRE(asn1typed_ioc_registry_validate(&m.ioc_registries[0],diagnostic,sizeof(diagnostic))==0);
    }
    asn1typed_module_clear(&m); asn1typed_module_clear(&m);
    puts("N16 INTEGER effective use-site ownership after Parser deletion PASS"); return 0;
}
