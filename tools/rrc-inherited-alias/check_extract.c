#include "asn1typed_extract.h"
#include "asn1typed_render_uper.h"
#include <asn1fix.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef ALIAS_FIXTURE
#define ALIAS_FIXTURE "tools/rrc-inherited-alias/aliases.asn1"
#endif
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"alias:%d: %s (%s)\n",__LINE__,#x,error); exit(1); } } while(0)
static char error[2048];
static asn1p_expr_t *decl(asn1p_t *t,const char *module,const char *name) {
    asn1p_module_t *m; asn1p_expr_t *e;
    TQ_FOR(m,&t->modules,mod_next) if(!strcmp(m->ModuleName,module))
        TQ_FOR(e,&m->members,next) if(!strcmp(e->Identifier,name)) return e;
    return NULL;
}
static int equal(const asn1typed_size_constraint_t *a,const asn1typed_size_constraint_t *b) {
    return a->has_size_constraint==b->has_size_constraint && a->lower_bound==b->lower_bound &&
        a->upper_bound==b->upper_bound && a->is_extensible==b->is_extensible &&
        a->has_extension_addition==b->has_extension_addition &&
        a->extension_lower_bound==b->extension_lower_bound && a->extension_upper_bound==b->extension_upper_bound;
}
static void rejects(asn1p_t *t,const char *name) {
    asn1typed_module_t out={0}; error[0]=0;
    CHECK(asn1typed_extract_root_graph(t,"Owner",name,NULL,&out,error,sizeof(error))<0);
    CHECK(error[0] && !out.types && !out.type_count); asn1typed_module_clear(&out);
}
static void swap_effective(asn1p_t *t,const char *name,const char *replacement,const char *root) {
    asn1p_expr_t *e=decl(t,"Owner",name); asn1p_constraint_t *saved=e->combined_constraints;
    e->combined_constraints=replacement ? decl(t,"Owner",replacement)->combined_constraints : NULL;
    rejects(t,root); e->combined_constraints=saved;
}
int main(void) {
    asn1p_t *t=asn1p_parse_file(ALIAS_FIXTURE,A1P_NOFLAGS);
    CHECK(t && asn1f_process(t,A1F_COMPOUND_NAMES,NULL)>=0);
    const char *names[]={"List","Alias","Chain","RemoteAlias","TupleList","TupleAlias","ExtList","ExtAlias","UnboundedList","UnboundedAlias"};
    asn1typed_module_t owned[10]={{0}}; const size_t target[]={0,0,0,0,4,4,6,6,8,8};
    for(size_t i=0;i<10;++i) {
        const char *module=i==3 ? "Consumer" : "Owner";
        CHECK(!asn1typed_extract_root_graph(t,module,names[i],NULL,&owned[i],error,sizeof(error)));
        asn1typed_type_t *root=&owned[i].types[0];
        CHECK(root->kind==ASN1TYPED_TYPE_SEQUENCE_OF && owned[i].type_count==2);
        CHECK(!strcmp(root->identity.module,module) && !strcmp(root->identity.source_name,names[i]));
        CHECK(root->location.line==(unsigned)decl(t,module,names[i])->_lineno);
        CHECK(equal(&root->size_constraint,&owned[target[i]].types[0].size_constraint));
        CHECK(!strcmp(root->element_type.module,"Owner"));
        if(i!=5) CHECK(!strcmp(root->element_type.source_name,owned[target[i]].types[0].element_type.source_name));
        else {
            CHECK(owned[i].types[1].kind==ASN1TYPED_TYPE_SEQUENCE && owned[i].types[1].field_count==2);
            CHECK(owned[i].types[1].fields[1].value_range.lower_bound==4 && owned[i].types[1].fields[1].value_range.upper_bound==4);
            CHECK(owned[i].types[1].fields[0].location.line==owned[4].types[1].fields[0].location.line);
        }
    }
    CHECK(owned[0].types[0].size_constraint.lower_bound==1 && owned[0].types[0].size_constraint.upper_bound==3);
    CHECK(owned[3].types[1].value_range.lower_bound==0 && owned[3].types[1].value_range.upper_bound==3);
    CHECK(owned[6].types[0].size_constraint.is_extensible && !owned[6].types[0].size_constraint.has_extension_addition);
    CHECK(!owned[8].types[0].size_constraint.has_size_constraint);
    for(size_t i=0;i<2;++i) {
        const char *n=i ? "PickAlias" : "StructAlias"; asn1typed_module_t out={0};
        CHECK(!asn1typed_extract_root_graph(t,"Owner",n,NULL,&out,error,sizeof(error))); asn1typed_module_clear(&out);
    }
    asn1typed_module_t additions={0};
    CHECK(!asn1typed_extract_root_graph(t,"Owner","AdditionAlias",NULL,&additions,error,sizeof(error)));
    CHECK(additions.types[0].size_constraint.has_extension_addition && additions.types[0].size_constraint.extension_lower_bound==3 && additions.types[0].size_constraint.extension_upper_bound==3);
    char *addition_header=NULL;
    CHECK(asn1typed_render_cpp_owned_uper(&additions,"alias",&addition_header,error,sizeof(error))<0 && !addition_header);
    asn1typed_module_clear(&additions);
    swap_effective(t,"AdditionAlias","WrongExt","AdditionAlias");
    rejects(t,"Restricted"); rejects(t,"Hidden"); rejects(t,"EqualDeclared"); rejects(t,"BadStruct");
    swap_effective(t,"Alias",NULL,"Alias"); swap_effective(t,"Alias","Wide","Alias");
    swap_effective(t,"Alias","Narrow","Alias"); swap_effective(t,"Alias","ExtList","Alias");
    swap_effective(t,"Alias",NULL,"Chain"); swap_effective(t,"Alias","Wide","Chain");
    swap_effective(t,"ExtAlias","WrongExt","ExtAlias"); swap_effective(t,"ExtAlias","List","ExtAlias");
    swap_effective(t,"UnboundedAlias","List","UnboundedAlias");
    swap_effective(t,"List","Wide","Alias"); swap_effective(t,"StructAlias","List","StructAlias");
    /* An apparently equal outer inherited tree cannot hide a declared SIZE. */
    asn1p_expr_t *hidden=decl(t,"Owner","Hidden"); asn1p_constraint_t *h=hidden->combined_constraints;
    hidden->combined_constraints=decl(t,"Owner","List")->combined_constraints;
    rejects(t,"Hidden"); hidden->combined_constraints=h;
    asn1p_expr_t *alias=decl(t,"Owner","Alias"); asn1p_constraint_t *c=alias->combined_constraints;
    unsigned count=c->el_count; c->el_count=0; rejects(t,"Alias"); c->el_count=count;
    asn1p_value_t *value=c->value; c->value=decl(t,"Owner","Tiny")->constraints->elements[0]->range_start;
    rejects(t,"Alias"); c->value=value;
    asn1p_expr_t *cache=alias->reference->ref_expr;
    alias->reference->ref_expr=decl(t,"Owner","Wide"); rejects(t,"Alias"); alias->reference->ref_expr=cache;
    asn1typed_graph_limits_t limits={1,65536,262144}; asn1typed_module_t quota={0};
    CHECK(asn1typed_extract_root_graph(t,"Owner","Alias",&limits,&quota,error,sizeof(error))<0);
    CHECK(!quota.types); asn1typed_module_clear(&quota);
    asn1p_delete(t);
    /* Same spelling in different lexical modules is exercised separately;
     * the standard generation probe rejects global name collisions. */
    const char *shadow="S1 DEFINITIONS ::= BEGIN Tiny ::= INTEGER(0..3) List ::= SEQUENCE(SIZE(1..3)) OF Tiny END S2 DEFINITIONS ::= BEGIN IMPORTS List FROM S1; Tiny ::= INTEGER(10..13) Alias ::= List END";
    t=asn1p_parse_buffer(shadow,-1,"shadow",1,A1P_NOFLAGS);
    CHECK(t && asn1f_process(t,A1F_COMPOUND_NAMES,NULL)>=0);
    asn1typed_module_t shadow_owned={0};
    CHECK(!asn1typed_extract_root_graph(t,"S2","Alias",NULL,&shadow_owned,error,sizeof(error)));
    CHECK(!strcmp(shadow_owned.types[0].element_type.module,"S1") && shadow_owned.types[1].value_range.lower_bound==0);
    asn1p_delete(t); asn1typed_module_clear(&shadow_owned);
    for(size_t i=0;i<10;++i) {
        char *header=NULL; CHECK(!asn1typed_render_cpp_owned_uper(&owned[i],"alias",&header,error,sizeof(error)));
        CHECK(header); free(header); asn1typed_module_clear(&owned[i]);
    }
    puts("inherited alias: ten graphs, bounds/elements/locations, cross-module and intermediate evidence, negatives/rollback/quota/lifetime PASS");
    return 0;
}
