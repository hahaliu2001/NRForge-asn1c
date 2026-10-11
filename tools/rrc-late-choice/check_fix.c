#include "asn1typed_extract.h"
#include "asn1typed_render_uper.h"
#include <asn1fix.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef LATE_FIXTURE
#define LATE_FIXTURE "tools/rrc-late-choice/late.asn1"
#endif
#define CHECK(x) do{if(!(x)){fprintf(stderr,"late-choice:%d: %s (%s)\n",__LINE__,#x,error);exit(1);}}while(0)
static char error[2048];
static asn1p_expr_t *decl(asn1p_t*t,const char*mod,const char*name){asn1p_module_t*m;asn1p_expr_t*e;TQ_FOR(m,&t->modules,mod_next)if(!strcmp(m->ModuleName,mod))TQ_FOR(e,&m->members,next)if(!strcmp(e->Identifier,name))return e;return NULL;}
static asn1p_expr_t *clone(asn1p_t*t,const char*mod,const char*name){asn1p_expr_t*g=decl(t,mod,name);CHECK(g&&g->specializations.pspecs_count);return g->specializations.pspec[0].my_clone;}
static void tagged(asn1p_expr_t*c){asn1p_expr_t*m;long n=0;TQ_FOR(m,&c->members,next){if(m->expr_type==A1TC_EXTENSIBLE)continue;if(m->tag.tag_class!=TC_CONTEXT_SPECIFIC || m->tag.tag_value!=n)fprintf(stderr,"bad tag %s.%s=%d/%ld auto=%d owner=%s\n",c->Identifier,m->Identifier,m->tag.tag_class,(long)m->tag.tag_value,c->auto_tags_OK,c->module->ModuleName);CHECK(m->tag.tag_class==TC_CONTEXT_SPECIFIC&&m->tag.tag_value==n++);}}
static void check_tree(asn1p_t*t,int late){
 asn1p_expr_t*g=decl(t,"Templates","Pick"),*m;CHECK(g->specializations.pspecs_count==3);
 CHECK(!g->auto_tags_OK);TQ_FOR(m,&g->members,next)CHECK(m->tag.tag_class==TC_NOCLASS);
 for(int i=0;i<3;i++){asn1p_expr_t*c=g->specializations.pspec[i].my_clone;tagged(c);asn1p_expr_t*arg=TQ_FIRST(&g->specializations.pspec[i].rhs_pspecs->members);const char*n=arg->reference->components[arg->reference->comp_count-1].name;m=TQ_NEXT(TQ_FIRST(&c->members),next);CHECK(m->tag.tag_mode==(!strcmp(n,"ChoiceArg")?TM_EXPLICIT:TM_IMPLICIT));}
 tagged(clone(t,"Templates","NestedActual"));tagged(clone(t,"Templates","ExtPick"));tagged(clone(t,"Templates","NestedChoice"));
 tagged(TQ_FIRST(&clone(t,"Templates","InlineChoice")->members));
 if(late){CHECK(!clone(t,"Templates","Outer")->auto_tags_OK);CHECK(TQ_FIRST(&clone(t,"Templates","Outer")->members)->tag.tag_class==TC_NOCLASS);}
 asn1p_expr_t*c=clone(t,"Templates","Manual");CHECK(!c->auto_tags_OK);CHECK(TQ_FIRST(&c->members)->tag.tag_value==7&&TQ_NEXT(TQ_FIRST(&c->members),next)->tag.tag_value==2);
 c=clone(t,"Templates","Mixed");CHECK(!c->auto_tags_OK);CHECK(TQ_FIRST(&c->members)->tag.tag_class==TC_APPLICATION&&TQ_NEXT(TQ_FIRST(&c->members),next)->tag.tag_class==TC_NOCLASS);
 c=clone(t,"OwnerExplicit","EChoice");CHECK(!c->auto_tags_OK);TQ_FOR(m,&c->members,next)CHECK(m->tag.tag_mode==TM_EXPLICIT);
 c=clone(t,"OwnerImplicit","IChoice");CHECK(!c->auto_tags_OK);TQ_FOR(m,&c->members,next)CHECK(m->tag.tag_class==TC_NOCLASS);
}
static char *render(asn1p_t*t,const char*mod,const char*name){asn1typed_module_t out={0};CHECK(!asn1typed_extract_root_graph(t,mod,name,NULL,&out,error,sizeof(error)));CHECK(out.types[0].ordinary_structure_owned);char*header=NULL;/* Actual owner metadata survives parser destruction, checked separately below. */
 CHECK(!asn1typed_render_cpp_owned_uper(&out,"qualification",&header,error,sizeof(error)));asn1typed_module_clear(&out);return header;}
static void quiet(int level,const char*fmt,...){(void)level;(void)fmt;}
static void negative(const char*source){asn1p_t*t=asn1p_parse_buffer(source,strlen(source),"negative",1,A1P_NOFLAGS);CHECK(t);int result=asn1f_process(t,A1F_NOFLAGS,quiet);if(result>=0)fprintf(stderr,"unexpected accepted negative: %s\n",source);CHECK(result<0);asn1p_delete(t);}
int main(void){
 asn1p_t*t=asn1p_parse_file(LATE_FIXTURE,A1P_NOFLAGS);CHECK(t&&asn1f_process(t,A1F_COMPOUND_NAMES,NULL)>=0);check_tree(t,1);
 const char*mods[]={"Consumer","Consumer","Consumer","Consumer","Consumer","Later","Later"};
 const char*names[]={"Late","Nested","NestedActualUse","ExtUse","ManualUse","Qualified","ExplicitUse"};char*headers[7];for(size_t i=0;i<7;i++)headers[i]=render(t,mods[i],names[i]);
 asn1typed_module_t owned={0};CHECK(!asn1typed_extract_root_graph(t,"Consumer","Late",NULL,&owned,error,sizeof(error)));asn1p_delete(t);
 CHECK(owned.types[0].alternatives[0].per_root_index==0&&owned.types[0].alternatives[1].per_root_index==1);asn1typed_module_clear(&owned);
 /* Reverse module discovery: the earlier and later instantiation paths agree. */
 t=asn1p_parse_file(LATE_FIXTURE,A1P_NOFLAGS);CHECK(t);asn1p_module_t*modules[16];size_t count=0;asn1p_module_t*m;while((m=TQ_REMOVE(&t->modules,mod_next))){CHECK(count<16);modules[count++]=m;}while(count){m=modules[--count];TQ_ADD(&t->modules,m,mod_next);}
 CHECK(asn1f_process(t,A1F_COMPOUND_NAMES,NULL)>=0);check_tree(t,0);
 for(size_t i=0;i<7;i++){char*h=render(t,mods[i],names[i]);CHECK(!strcmp(h,headers[i]));free(h);free(headers[i]);}
 asn1p_delete(t);
 negative("O DEFINITIONS AUTOMATIC TAGS ::= BEGIN P{T} ::= [0] IMPLICIT CHOICE {a NULL,b T} END C DEFINITIONS ::= BEGIN IMPORTS P FROM O; X ::= INTEGER(0..3) R ::= P{X} END");
 negative("O DEFINITIONS AUTOMATIC TAGS ::= BEGIN P{T} ::= CHOICE {a [1] NULL,b [1] T} END C DEFINITIONS ::= BEGIN IMPORTS P FROM O; X ::= INTEGER(0..3) R ::= P{X} END");
 negative("O DEFINITIONS AUTOMATIC TAGS ::= BEGIN P{T} ::= CHOICE {a NULL,b T,...,c [2] BOOLEAN} END C DEFINITIONS ::= BEGIN IMPORTS P FROM O; X ::= INTEGER(0..3) R ::= P{X} END");
 /* The legacy fixer permits unresolved actuals; owned extraction still rejects
  * them. Tag completion must never make an unresolved graph usable. */
 const char*unresolved="O DEFINITIONS AUTOMATIC TAGS ::= BEGIN P{T} ::= CHOICE {a NULL,b T} END C DEFINITIONS ::= BEGIN IMPORTS P FROM O; R ::= P{Missing} END";
 t=asn1p_parse_buffer(unresolved,-1,"unresolved",1,A1P_NOFLAGS);CHECK(t);
 (void)asn1f_process(t,A1F_NOFLAGS,quiet);
 CHECK(asn1typed_extract_root_graph(t,"C","R",NULL,&owned,error,sizeof(error))<0);
 CHECK(!owned.types);asn1p_delete(t);
 puts("late choice: owner modes, nested clones/inline choice, module-order identical codecs, owned lifetime, three fixer errors + unresolved extraction rejection PASS");return 0;
}
