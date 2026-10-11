#include "asn1typed_extract.h"
#include "asn1typed_render_cpp.h"
#include <asn1fix.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(c) do { if(!(c)) { fprintf(stderr,"extensions:%d: %s\n",__LINE__,#c); exit(1); } } while(0)
#ifndef EXTENSIONS_FIXTURE
#define EXTENSIONS_FIXTURE "fixtures/ordinary-extensions.asn1"
#endif
void *__real_malloc(size_t); void *__real_calloc(size_t,size_t); void *__real_realloc(void*,size_t);
static int armed; static size_t allocs, fail_at = (size_t)-1;
static int fail(void) { return armed && allocs++ == fail_at; }
void *__wrap_malloc(size_t n) { return fail() ? NULL : __real_malloc(n); }
void *__wrap_calloc(size_t n,size_t s) { return fail() ? NULL : __real_calloc(n,s); }
void *__wrap_realloc(void *p,size_t n) { return fail() ? NULL : __real_realloc(p,n); }
static asn1typed_module_t graph(asn1p_t *tree,const char *name) {
    asn1typed_module_t g={0}; char e[1024]={0};
    int rc=asn1typed_extract_root_graph(tree,"Extensions",name,NULL,&g,e,sizeof(e));
    if(rc) fprintf(stderr,"%s: %s\n",name,e);
    CHECK(!rc); return g;
}
static asn1typed_type_t *find(asn1typed_module_t *g,const char *name) {
    for(size_t i=0;i<g->type_count;++i) if(!strcmp(g->types[i].identity.source_name,name)) return &g->types[i];
    CHECK(0); return NULL;
}
static void empty(asn1typed_module_t *g) { CHECK(!g->types&&!g->type_count&&!g->source_name&&!g->bound_instances&&!g->ioc_registries);asn1typed_module_clear(g); }
static void reject(asn1p_t *tree,const char *name,const asn1typed_graph_limits_t *limits,const char *why) {
    asn1typed_module_t g={0};char e[1024]={0};
    CHECK(asn1typed_extract_root_graph(tree,"Extensions",name,limits,&g,e,sizeof(e))==-1);
    CHECK(strstr(e,why));empty(&g);
}
static void oom(asn1p_t *tree,const char *name) {
    asn1typed_module_t g={0};char e[1024];size_t n;
    armed=1;allocs=0;fail_at=(size_t)-1;
    CHECK(!asn1typed_extract_root_graph(tree,"Extensions",name,NULL,&g,e,sizeof(e)));
    armed=0;n=allocs;asn1typed_module_clear(&g);CHECK(n&&n<10000);
    for(fail_at=0;fail_at<n;++fail_at) {
        armed=1;allocs=0;int rc=asn1typed_extract_root_graph(tree,"Extensions",name,NULL,&g,e,sizeof(e));armed=0;
        CHECK(rc==-1&&e[0]);empty(&g);
    }
    fail_at=(size_t)-1;printf("%s allocation rollback: %zu positions PASS\n",name,n);
}
int main(void) {
    asn1p_t *tree=asn1p_parse_file(EXTENSIONS_FIXTURE,0);CHECK(tree);CHECK(asn1f_process(tree,0,NULL)>=0);
    asn1typed_module_t root=graph(tree,"Root"),alias=graph(tree,"Alias"),pick=graph(tree,"Pick"),future=graph(tree,"PickFuture"),nested=graph(tree,"NestedPick"),zero=graph(tree,"EmptyRoot"),terminal=graph(tree,"RootOnly");
    char e[1024];asn1p_expr_t *d,*group=NULL,*decl=NULL;asn1p_module_t *m=TQ_FIRST(&tree->modules);
    TQ_FOR(d,&m->members,next) if(!strcmp(d->Identifier,"Root")) decl=d;
    CHECK(decl);TQ_FOR(d,&decl->members,next) if(d->is_extension_addition_group) { group=d;break; }
    CHECK(group);asn1p_expr_t *copy=asn1p_expr_clone(decl,0);CHECK(copy);asn1p_expr_t *cg=TQ_FIRST(&copy->members);
    while(cg&&!cg->is_extension_addition_group) { cg=TQ_NEXT(cg,next); }
    CHECK(cg&&cg->is_extension_addition_group);CHECK(!asn1p_expr_compare(copy,decl));
    cg->is_extension_addition_group=0;CHECK(asn1p_expr_compare(copy,decl));asn1p_expr_free(copy);
    group->is_extension_addition_group=0;reject(tree,"Root",NULL,"unnamed SEQUENCE");group->is_extension_addition_group=1;
    group->is_extension_addition_group=2;reject(tree,"Root",NULL,"malformed extension addition group");group->is_extension_addition_group=1;
    asn1p_expr_t *first=TQ_FIRST(&decl->members);first->is_extension_addition_group=1;reject(tree,"Root",NULL,"malformed extension addition group");first->is_extension_addition_group=0;
    asn1typed_graph_limits_t l={1,1000,10000};reject(tree,"Root",&l,"type limit");l.max_types=1000;l.max_references=1;reject(tree,"Root",&l,"reference limit");l.max_references=1000;l.max_ast_nodes=1;reject(tree,"Root",&l,"AST limit");
    /* Legacy extraction still rejects known additions; no implicit APER upgrade. */
    asn1typed_module_t legacy={0};CHECK(asn1typed_extract_module(tree,"Extensions",&legacy,e,sizeof(e))==-1);empty(&legacy);
    oom(tree,"Root");oom(tree,"NestedPick");
    asn1p_delete(tree);
    asn1typed_type_t *t=&root.types[0];CHECK(t->field_count==6&&t->ordinary_root_count==2&&t->ordinary_addition_count==4);
    CHECK(t->unknown_extension_policy==ASN1TYPED_UNKNOWN_SEQUENCE_SKIP);CHECK(!asn1typed_ordinary_extension_structure_validate(t,e,sizeof(e)));
    CHECK(t->fields[2].component_role==ASN1TYPED_COMPONENT_ADDITION&&t->fields[2].presence==ASN1TYPED_PRESENCE_MANDATORY);
    CHECK(t->fields[3].component_role==ASN1TYPED_COMPONENT_GROUP&&t->fields[3].per_component_index==1);
    CHECK(t->fields[4].presence==ASN1TYPED_PRESENCE_DEFAULT&&t->fields[4].default_value.number==1);
    CHECK(t->fields[5].component_role==ASN1TYPED_COMPONENT_GROUP&&t->fields[5].per_component_index==3);
    asn1typed_type_t *g=find(&root,t->fields[3].type.source_name);CHECK(g->field_count==3&&!g->is_extensible);
    CHECK(g->fields[0].presence==ASN1TYPED_PRESENCE_MANDATORY&&g->fields[1].presence==ASN1TYPED_PRESENCE_OPTIONAL);
    asn1typed_type_t *inner=find(&root,g->fields[2].type.source_name);CHECK(inner->ordinary_root_count==1&&inner->ordinary_addition_count==1);
    asn1typed_type_t *ig=find(&root,inner->fields[1].type.source_name);CHECK(ig->fields[1].presence==ASN1TYPED_PRESENCE_DEFAULT&&ig->fields[1].default_value.number==2);
    CHECK(!asn1typed_sequence_extension_structure_validate(&terminal.types[0],e,sizeof(e)));
    CHECK(!zero.types[0].ordinary_root_count&&zero.types[0].ordinary_addition_count==1);
    CHECK(alias.types[0].ordinary_addition_count==4&&strstr(alias.types[0].fields[3].type.source_name,"Alias"));
    asn1typed_type_t *p=&pick.types[0],*f=&future.types[0];CHECK(!asn1typed_ordinary_extension_structure_validate(p,e,sizeof(e)));
    char *rendered=NULL;
    CHECK(asn1typed_render_cpp_owned_sequence_extension_types(&pick,"extension_check",&rendered,e,sizeof(e))==-1&&!rendered);
    CHECK(asn1typed_render_cpp_owned_sequence_extension_types(&zero,"extension_check",&rendered,e,sizeof(e))==-1&&!rendered);
    CHECK(p->ordinary_root_count==2&&p->ordinary_addition_count==2&&p->unknown_extension_policy==ASN1TYPED_UNKNOWN_CHOICE_PRESERVE_OPAQUE);
    CHECK(!p->choice_root_only_extension_owned&&!p->has_valid_per_root_mapping);
    CHECK(p->alternatives[0].per_root_index==1&&p->alternatives[1].per_root_index==0);
    CHECK(p->alternatives[2].per_extension_index==0&&p->alternatives[3].per_extension_index==1);
    CHECK(f->alternatives[4].per_extension_index==2);
    for(size_t i=0;i<4;++i)CHECK(p->alternatives[i].per_root_index==f->alternatives[i].per_root_index&&p->alternatives[i].per_extension_index==f->alternatives[i].per_extension_index);
    p->alternatives[3].per_extension_index=0;CHECK(asn1typed_ordinary_extension_structure_validate(p,e,sizeof(e))==-1);p->alternatives[3].per_extension_index=1;
    p->alternatives[1].effective_tag_number=9;CHECK(asn1typed_ordinary_extension_structure_finalize(p,e,sizeof(e))==ASN1TYPED_WIRE_FINALIZE_UNAVAILABLE);CHECK(!p->alternatives[0].has_per_root_index&&!p->alternatives[2].has_per_extension_index);p->alternatives[1].effective_tag_number=2;
    CHECK(asn1typed_ordinary_extension_structure_finalize(p,e,sizeof(e))==ASN1TYPED_WIRE_FINALIZE_OK);
    p->alternatives[0].wire_evidence=ASN1TYPED_WIRE_EVIDENCE_UNAVAILABLE;CHECK(asn1typed_ordinary_extension_structure_finalize(p,e,sizeof(e))==ASN1TYPED_WIRE_FINALIZE_UNAVAILABLE);p->alternatives[0].wire_evidence=ASN1TYPED_WIRE_EVIDENCE_RESOLVED;
    CHECK(asn1typed_ordinary_extension_structure_finalize(p,e,sizeof(e))==ASN1TYPED_WIRE_FINALIZE_OK);
    t->fields[3].per_component_index=0;CHECK(asn1typed_ordinary_extension_structure_validate(t,e,sizeof(e))==-1);t->fields[3].per_component_index=1;
    t->unknown_extension_policy=ASN1TYPED_UNKNOWN_CHOICE_PRESERVE_OPAQUE;CHECK(asn1typed_ordinary_extension_structure_finalize(t,e,sizeof(e))==ASN1TYPED_WIRE_FINALIZE_UNAVAILABLE);CHECK(!t->fields[0].has_per_component_index);t->unknown_extension_policy=ASN1TYPED_UNKNOWN_SEQUENCE_SKIP;
    CHECK(asn1typed_ordinary_extension_structure_finalize(t,e,sizeof(e))==ASN1TYPED_WIRE_FINALIZE_OK);
    p->kind=ASN1TYPED_TYPE_PRIMITIVE;
    CHECK(asn1typed_ordinary_extension_structure_finalize(p,e,sizeof(e))==ASN1TYPED_WIRE_FINALIZE_ERROR);
    CHECK(!p->has_valid_ordinary_extension_structure&&!p->has_valid_per_root_mapping&&!p->alternatives[0].has_per_root_index&&!p->alternatives[2].has_per_extension_index);
    p->kind=ASN1TYPED_TYPE_CHOICE;CHECK(asn1typed_ordinary_extension_structure_finalize(p,e,sizeof(e))==ASN1TYPED_WIRE_FINALIZE_OK);
    size_t old_count=p->alternative_count;p->alternative_count=p->alternative_capacity+1;
    CHECK(asn1typed_ordinary_extension_structure_finalize(p,e,sizeof(e))==ASN1TYPED_WIRE_FINALIZE_ERROR);
    CHECK(!p->has_valid_ordinary_extension_structure&&!p->alternatives[0].has_per_root_index);
    p->alternative_count=old_count;CHECK(asn1typed_ordinary_extension_structure_finalize(p,e,sizeof(e))==ASN1TYPED_WIRE_FINALIZE_OK);
    asn1typed_type_t fieldcopy={0};fieldcopy.kind=ASN1TYPED_TYPE_SEQUENCE;CHECK(!asn1typed_type_add_field_copy(&fieldcopy,&t->fields[3]));CHECK(fieldcopy.fields[0].component_role==ASN1TYPED_COMPONENT_GROUP&&fieldcopy.fields[0].per_component_index==1);asn1typed_type_clear(&fieldcopy);
    asn1typed_module_clear(&root);asn1typed_module_clear(&alias);asn1typed_module_clear(&pick);asn1typed_module_clear(&future);asn1typed_module_clear(&nested);asn1typed_module_clear(&zero);asn1typed_module_clear(&terminal);
    puts("owned extensions/groups and future CHOICE evidence PASS");return 0;
}
