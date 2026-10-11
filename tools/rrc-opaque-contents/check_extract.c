#include "asn1typed_extract.h"
#include "asn1typed_render_uper.h"
#include <asn1fix.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef OPAQUE_FIXTURE
#define OPAQUE_FIXTURE "tools/rrc-opaque-contents/opaque.asn1"
#endif
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"opaque:%d: %s (%s)\n",__LINE__,#x,error); exit(1); } } while(0)
static char error[2048];
static asn1p_expr_t *decl(asn1p_t *t,const char *name) {
    asn1p_module_t *m; asn1p_expr_t *e;
    TQ_FOR(m,&t->modules,mod_next) TQ_FOR(e,&m->members,next)
        if(!strcmp(e->Identifier,name)) return e;
    return NULL;
}
static void rejects(asn1p_t *t,const char *name) {
    asn1typed_module_t out={0}; error[0]=0;
    CHECK(asn1typed_extract_root_graph(t,"Opaque",name,NULL,&out,error,sizeof(error))<0);
    CHECK(error[0] && !out.types && !out.type_count); asn1typed_module_clear(&out);
}
static void mutate(asn1p_t *t,asn1p_expr_t *e,const char *root) {
    asn1p_constraint_t *declared=e->constraints,*effective=e->combined_constraints;
    CHECK(declared && effective);
    e->combined_constraints=decl(t,"Mixed")->combined_constraints; rejects(t,root);
    e->combined_constraints=effective;
    e->constraints=decl(t,"Sized")->constraints; rejects(t,root);
    e->constraints=declared;
    asn1p_constraint_t *c=declared->elements[0]; asn1p_value_t *v=c->value;
    c->value=NULL; rejects(t,root); c->value=v;
    enum asn1p_constraint_type_e kind=c->type;
    c->type=ACT_CT_SIZE; rejects(t,root); c->type=kind;
    unsigned count=declared->el_count;
    declared->el_count=0; rejects(t,root); declared->el_count=count;
    asn1p_value_t *range=declared->range_start;
    declared->range_start=v; rejects(t,root); declared->range_start=range;
}
int main(void) {
    asn1p_t *t=asn1p_parse_file(OPAQUE_FIXTURE,A1P_NOFLAGS);
    CHECK(t && asn1f_process(t,A1F_NOFLAGS,NULL)>=0);
    const char *names[]={"Payload","Alias","InlineChoice","NamedChoice","Carrier","Self","UnsupportedPayload"};
    const size_t counts[]={1,1,1,2,2,1,1}; asn1typed_module_t owned[7]={{0}};
    for(size_t i=0;i<7;++i) {
        CHECK(!asn1typed_extract_root_graph(t,"Opaque",names[i],NULL,&owned[i],error,sizeof(error)));
        CHECK(owned[i].type_count==counts[i]);
        for(size_t j=0;j<owned[i].type_count;++j) {
            asn1typed_type_t *body=&owned[i].types[j];
            CHECK(strcmp(body->identity.source_name,"Inner") && strcmp(body->identity.source_name,"UnsupportedInner"));
            if(body->kind==ASN1TYPED_TYPE_PRIMITIVE) {
                CHECK(body->primitive_kind==ASN1TYPED_PRIMITIVE_OCTET_STRING);
                CHECK(!body->size_constraint.has_size_constraint && !body->value_range.has_value_range);
            }
        }
    }
    CHECK(owned[2].types[0].alternatives[1].type_ref.kind==ASN1TYPED_REF_PRIMITIVE);
    CHECK(owned[2].types[0].alternatives[1].type_ref.primitive_kind==ASN1TYPED_PRIMITIVE_OCTET_STRING);
    CHECK(!owned[2].types[0].alternatives[1].size_constraint.has_size_constraint);
    CHECK(owned[4].types[0].fields[1].presence==ASN1TYPED_PRESENCE_OPTIONAL);
    const char *bad[]={"Mixed","BadChoice","BadSequence","Bits","Restricted","Indirect","UnsupportedInner"};
    for(size_t i=0;i<7;++i) rejects(t,bad[i]);
    mutate(t,decl(t,"Payload"),"Payload");
    mutate(t,TQ_NEXT(TQ_FIRST(&decl(t,"InlineChoice")->members),next),"InlineChoice");
    /* A modified effective constraint cannot conceal any alias declaration. */
    asn1p_expr_t *restricted=decl(t,"Restricted"),*indirect=decl(t,"Indirect");
    asn1p_constraint_t *r=restricted->combined_constraints,*d=indirect->combined_constraints;
    restricted->combined_constraints=indirect->combined_constraints=decl(t,"Payload")->combined_constraints;
    rejects(t,"Restricted"); rejects(t,"Indirect");
    restricted->combined_constraints=r; indirect->combined_constraints=d;
    asn1p_expr_t *alias=decl(t,"Alias"); asn1p_constraint_t *a=alias->combined_constraints;
    CHECK(!alias->constraints && a);
    alias->combined_constraints=NULL; rejects(t,"Alias");
    alias->combined_constraints=decl(t,"Sized")->combined_constraints; rejects(t,"Alias");
    alias->combined_constraints=a;
    asn1typed_module_t quota={0}; asn1typed_graph_limits_t limits={1,65536,262144};
    limits.max_types=1;
    CHECK(asn1typed_extract_root_graph(t,"Opaque","NamedChoice",&limits,&quota,error,sizeof(error))<0);
    CHECK(!quota.types); asn1typed_module_clear(&quota);
    /* ENCODED BY is outside the parser's supported ContentsConstraint grammar. */
    const char *encoded="E DEFINITIONS ::= BEGIN X ::= OCTET STRING (CONTAINING INTEGER ENCODED BY {1 2 3}) END";
    asn1p_t *bad_tree=asn1p_parse_buffer(encoded,-1,"encoded-by",1,A1P_NOFLAGS);
    CHECK(!bad_tree);
    asn1p_delete(t);
    for(size_t i=0;i<7;++i) {
        char *header=NULL;
        CHECK(!asn1typed_render_cpp_owned_uper(&owned[i],"opaque",&header,error,sizeof(error)));
        CHECK(header && strstr(header,"std::vector<std::byte>"));
        free(header); asn1typed_module_clear(&owned[i]);
    }
    puts("opaque contents: seven owned carriers, no nested closure, alias layers, malformed/mixed rejection, rollback and quota PASS");
    return 0;
}
