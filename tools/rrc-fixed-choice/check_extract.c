/* Fixed evidence transfer is read-only; corrupt caches must fail closed. */
#include "asn1typed_extract.h"
#include <asn1fix.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef CHOICE_FIXTURE
#define CHOICE_FIXTURE "tools/rrc-fixed-choice/choices.asn1"
#endif
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"fixed-choice:%d: %s (%s)\n",__LINE__,#x,error); exit(1); } } while(0)
static char error[2048];
static asn1p_expr_t *decl(asn1p_t *t,const char *module,const char *name) {
    asn1p_module_t *m; asn1p_expr_t *e;
    TQ_FOR(m,&t->modules,mod_next) if(!strcmp(m->ModuleName,module))
        TQ_FOR(e,&m->members,next) if(!strcmp(e->Identifier,name)) return e;
    return NULL;
}
static asn1typed_module_t extract(asn1p_t *t,const char *module,const char *name) {
    asn1typed_module_t out={0};
    CHECK(!asn1typed_extract_root_graph(t,module,name,NULL,&out,error,sizeof(error)));
    return out;
}
static void reject(asn1p_t *t) {
    asn1typed_module_t out={0};
    CHECK(asn1typed_extract_root_graph(t,"AutoA","Roots",NULL,&out,error,sizeof(error))==-1);
    CHECK(error[0] && !out.types && !out.source_name);
}
int main(void) {
    asn1p_t *t=asn1p_parse_file(CHOICE_FIXTURE,A1P_NOFLAGS);
    CHECK(t && asn1f_process(t,A1F_COMPOUND_NAMES,NULL)>=0);
    asn1p_expr_t *g=decl(t,"AutoA","Pick");
    CHECK(g && g->specializations.pspecs_count==2);
    asn1typed_module_t owned=extract(t,"AutoA","Roots");
    CHECK(owned.types[1].alternatives[0].has_per_root_index);
    CHECK(owned.types[1].alternatives[0].per_root_index==0);
    CHECK(owned.types[1].alternatives[1].per_root_index==1);
    CHECK(!strcmp(owned.types[2].alternatives[1].type_ref.module,"AutoB"));
    /* Empty actual cache is valid; specialization array order is irrelevant. */
    asn1p_expr_t *actual=TQ_FIRST(&g->specializations.pspec[0].rhs_pspecs->members);
    asn1p_expr_t *cached=actual->reference->ref_expr;
    actual->reference->ref_expr=NULL;
    asn1typed_module_t out=extract(t,"AutoA","Roots"); asn1typed_module_clear(&out);
    actual->reference->ref_expr=cached;
    struct asn1p_pspec_s swap=g->specializations.pspec[0];
    g->specializations.pspec[0]=g->specializations.pspec[1];
    g->specializations.pspec[1]=swap;
    out=extract(t,"AutoA","Roots"); asn1typed_module_clear(&out);
    g->specializations.pspec[1]=g->specializations.pspec[0]; g->specializations.pspec[0]=swap;
    /* Untagged evidence remains unavailable, not an invented root index. */
    asn1p_expr_t *clone=g->specializations.pspec[0].my_clone;
    asn1p_expr_t *first=TQ_FIRST(&clone->members),*last=TQ_NEXT(first,next);
    asn1p_expr_t saved_first=*first,saved_last=*last;
    first->tag.tag_class=last->tag.tag_class=TC_NOCLASS;
    out=extract(t,"AutoA","Roots");
    CHECK(!out.types[1].alternatives[0].has_per_root_index && !out.types[1].alternatives[1].has_per_root_index);
    CHECK(out.types[2].alternatives[0].has_per_root_index);
    asn1typed_module_clear(&out); *first=saved_first; *last=saved_last;
    /* Tags select canonical order, not source branch order. */
    first->tag.tag_value=7; last->tag.tag_value=2;
    out=extract(t,"AutoA","Roots");
    CHECK(out.types[1].alternatives[0].per_root_index==1 && out.types[1].alternatives[1].per_root_index==0);
    CHECK(out.types[2].alternatives[0].per_root_index==0);
    asn1typed_module_clear(&out); *first=saved_first; *last=saved_last;
    /* Duplicate/invalid tags remain rejected by the existing evidence validator. */
    last->tag.tag_value=first->tag.tag_value;
    out=extract(t,"AutoA","Roots");
    CHECK(!out.types[1].alternatives[0].has_per_root_index);
    asn1typed_module_clear(&out); *last=saved_last;
    last->tag.tag_value=-1;
    out=extract(t,"AutoA","Roots"); CHECK(!out.types[1].alternatives[0].has_per_root_index);
    asn1typed_module_clear(&out); *last=saved_last;
    /* Wrong clone, namespace/cache, member shape and storage cannot borrow tags. */
    g->specializations.pspec[0].my_clone=g->specializations.pspec[1].my_clone;
    reject(t); g->specializations.pspec[0].my_clone=clone;
    last->module=decl(t,"AutoB","Shared")->module; reject(t); *last=saved_last;
    last->Identifier="wrong"; reject(t); *last=saved_last;
    struct asn1p_pspec_s *table=g->specializations.pspec;
    g->specializations.pspec=NULL; reject(t); g->specializations.pspec=table;
    int count=g->specializations.pspecs_count;
    g->specializations.pspecs_count=-1; reject(t); g->specializations.pspecs_count=count;
    struct asn1p_pspec_s duplicate[3]={table[0],table[1],table[0]};
    g->specializations.pspec=duplicate; g->specializations.pspecs_count=3;
    reject(t); g->specializations.pspec=table; g->specializations.pspecs_count=count;
    /* Source-use stale caches remain rejected; matcher never repairs the AST. */
    asn1p_expr_t *field=TQ_FIRST(&decl(t,"AutoA","Roots")->members);
    cached=field->reference->ref_expr; field->reference->ref_expr=g->specializations.pspec[1].my_clone;
    reject(t); field->reference->ref_expr=cached;
    /* Nonformal references must retain exact lexical identities and caches. */
    asn1p_expr_t *mg=decl(t,"AutoA","Mixed");
    asn1p_expr_t *mf=TQ_FIRST(&mg->specializations.pspec[0].my_clone->members);
    asn1p_expr_t saved_mf=*mf;
    out=extract(t,"AutoA","MixedRoot"); asn1typed_module_clear(&out);
    asn1p_expr_t *other=TQ_NEXT(mf,next);
    mf->reference=other->reference;
    CHECK(asn1typed_extract_root_graph(t,"AutoA","MixedRoot",NULL,&out,error,sizeof(error))==-1);
    CHECK(!out.types); *mf=saved_mf;
    mf->module=decl(t,"AutoB","Shared")->module;
    CHECK(asn1typed_extract_root_graph(t,"AutoA","MixedRoot",NULL,&out,error,sizeof(error))==-1);
    CHECK(!out.types); *mf=saved_mf;
    /* Nested parameter references compare their fully substituted actuals. */
    out=extract(t,"AutoA","Wrapped"); asn1typed_module_clear(&out);
    asn1p_expr_t *wg=decl(t,"AutoA","Wrap");
    asn1p_expr_t *wf=TQ_FIRST(&wg->specializations.pspec[0].my_clone->members);
    asn1p_expr_t *wa=TQ_FIRST(&wf->rhs_pspecs->members);
    asn1p_ref_t *saved_ref=wa->reference;
    wa->reference=TQ_FIRST(&g->specializations.pspec[0].rhs_pspecs->members)->reference;
    CHECK(asn1typed_extract_root_graph(t,"AutoA","Wrapped",NULL,&out,error,sizeof(error))==-1);
    CHECK(!out.types); wa->reference=saved_ref;
    /* Explicit tags preserve canonical context/application order. */
    out=extract(t,"Explicit","ExplicitRoot");
    CHECK(out.types[0].alternatives[0].per_root_index==1 && out.types[0].alternatives[1].per_root_index==0);
    asn1typed_module_clear(&out);
    asn1p_expr_t *eg=decl(t,"Explicit","ExplicitPick");
    asn1p_expr_t *ef=TQ_FIRST(&eg->specializations.pspec[0].my_clone->members);
    asn1c_integer_t tag=ef->tag.tag_value; ef->tag.tag_value=10;
    CHECK(asn1typed_extract_root_graph(t,"Explicit","ExplicitRoot",NULL,&out,error,sizeof(error))==-1);
    CHECK(!out.types); ef->tag.tag_value=tag;
    out=extract(t,"Implicit","ImplicitRoot");
    CHECK(out.types[0].alternatives[0].per_root_index==0 && out.types[0].alternatives[1].per_root_index==1);
    asn1typed_module_clear(&out);
    out=extract(t,"AutoA","Nested"); asn1typed_module_clear(&out);
    out=extract(t,"AutoA","Ext");
    CHECK(out.types[0].ordinary_root_count==2 && out.types[0].ordinary_addition_count==1);
    CHECK(out.types[0].alternatives[2].is_extension_addition && out.types[0].alternatives[2].has_per_extension_index);
    asn1typed_module_clear(&out);
    CHECK(first->tag.tag_class==saved_first.tag.tag_class && first->tag.tag_value==saved_first.tag.tag_value);
    CHECK(TQ_FIRST(&g->members)->tag.tag_class==TC_NOCLASS);
    asn1p_delete(t);
    CHECK(owned.types[1].alternatives[0].effective_tag_number==0 && owned.types[1].alternatives[1].per_root_index==1);
    asn1typed_module_clear(&owned);
    puts("fixed specialization matching, tags, cache guards and owned lifetime PASS");
    return 0;
}
