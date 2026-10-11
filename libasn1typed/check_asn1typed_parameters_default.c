#include "asn1typed_extract.h"
#include <asn1fix.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef PARAM_DEFAULT_FIXTURE
#define PARAM_DEFAULT_FIXTURE "fixtures/ordinary-parameters-default.asn1"
#endif
#define REQUIRE(c) do { if(!(c)) { fprintf(stderr,"P3:%d: %s (%s)\n",__LINE__,#c,error); exit(1); } } while(0)
void *__wrap_malloc(size_t);
void *__wrap_calloc(size_t,size_t);
void *__wrap_realloc(void *,size_t);
void *__real_malloc(size_t);
void *__real_calloc(size_t,size_t);
void *__real_realloc(void *,size_t);
static int armed;
static size_t index_alloc, fail_at = (size_t)-1;
static int fail(void) { return armed && index_alloc++ == fail_at; }
void *__wrap_malloc(size_t n) { return fail() ? NULL : __real_malloc(n); }
void *__wrap_calloc(size_t n,size_t z) { return fail() ? NULL : __real_calloc(n,z); }
void *__wrap_realloc(void *p,size_t n) { return fail() ? NULL : __real_realloc(p,n); }
static asn1p_expr_t *decl(asn1p_t *tree, const char *name) {
 asn1p_expr_t *e; asn1p_module_t *m=TQ_FIRST(&tree->modules);
 TQ_FOR(e,&m->members,next) if(e->Identifier && !strcmp(e->Identifier,name)) return e;
 return NULL;
}
int main(void) {
 char error[2048]={0};
 asn1p_t *tree=asn1p_parse_file(PARAM_DEFAULT_FIXTURE,A1P_NOFLAGS);
 asn1typed_module_t root={0}, defaults={0}, temp={0};
 asn1typed_type_t copied={0};
 asn1typed_default_value_t value={0}, effective={0};
 size_t i,n; int suppress;
 REQUIRE(tree && asn1f_process(tree,A1F_COMPOUND_NAMES,NULL)>=0);
 REQUIRE(!asn1typed_extract_root_graph(tree,"ParamA","Root",NULL,&root,error,sizeof(error)));
 REQUIRE(root.types[0].fields[0].type.actual_count==1);
 REQUIRE(root.types[0].fields[0].type.actuals[0].kind==ASN1TYPED_ACTUAL_TYPE_REFERENCE);
 REQUIRE(!strcmp(root.types[0].fields[0].type.actuals[0].module,"ParamA"));
 REQUIRE(!strcmp(root.types[0].fields[1].type.actuals[0].module,"ParamB"));
 REQUIRE(!strcmp(root.types[0].fields[1].type.actuals[0].source_name,"Shared"));
 REQUIRE(asn1typed_type_ref_equal(&root.types[0].fields[0].type,&root.types[0].fields[2].type));
 REQUIRE(!asn1typed_type_ref_equal(&root.types[0].fields[0].type,&root.types[0].fields[1].type));
 REQUIRE(!asn1typed_extract_root_graph(tree,"ParamA","Nested",NULL,&temp,error,sizeof(error)));
 REQUIRE(temp.types[1].ordinary_instance.actual_count==1); asn1typed_module_clear(&temp);
 REQUIRE(!asn1typed_extract_root_graph(tree,"ParamA","Unused",NULL,&temp,error,sizeof(error)));
 REQUIRE(temp.type_count==2 && !strcmp(temp.types[1].identity.module,"ParamB") && !strcmp(temp.types[1].identity.source_name,"Shared"));
 asn1typed_module_clear(&temp);
 {
  asn1typed_graph_limits_t limit={100,1,1000};
  REQUIRE(asn1typed_extract_root_graph(tree,"ParamA","Unused",&limit,&temp,error,sizeof(error))==-1);
  REQUIRE(!temp.types && strstr(error,"reference limit"));
  limit.max_references=100; limit.max_ast_nodes=2;
  REQUIRE(asn1typed_extract_root_graph(tree,"ParamA","Root",&limit,&temp,error,sizeof(error))==-1);
  REQUIRE(!temp.types && strstr(error,"AST limit"));
 }
 REQUIRE(asn1typed_extract_root_graph(tree,"ParamA","BadBuiltin",NULL,&temp,error,sizeof(error))==-1);
 REQUIRE(!temp.types && strstr(error,"unsupported"));
 REQUIRE(!strcmp(root.types[0].fields[3].type.actuals[0].module,"ParamB"));
 REQUIRE(!strcmp(root.types[0].fields[3].type.actuals[0].source_name,"Base"));
 REQUIRE(!asn1typed_type_ref_equal(&root.types[0].fields[0].type,&root.types[0].fields[3].type));
 REQUIRE(!root.bound_instance_count && !root.ioc_registry_count);
 REQUIRE(!asn1typed_extract_root_graph(tree,"ParamA","Defaults",NULL,&defaults,error,sizeof(error)));
 copied.kind=ASN1TYPED_TYPE_SEQUENCE;
 REQUIRE(!asn1typed_type_add_field_copy(&copied,&defaults.types[0].fields[2]));
 REQUIRE(copied.fields[0].default_value.enum_symbol!=defaults.types[0].fields[2].default_value.enum_symbol);
 /* Frozen ordinary template shapes with anonymous constructed bodies remain explicit gaps. */
 REQUIRE(asn1typed_extract_root_graph(tree,"ParamA","BadInline",NULL,&temp,error,sizeof(error))==-1);
 REQUIRE(!temp.types && strstr(error,"unsupported"));
 REQUIRE(asn1typed_extract_root_graph(tree,"ParamA","BadString",NULL,&temp,error,sizeof(error))==-1);
 REQUIRE(!temp.types && strstr(error,"DEFAULT"));
 /* Reject corrupt actual key/cache and governed formals on a fixed tree. */
 {
  asn1p_expr_t *field=TQ_FIRST(&decl(tree,"Root")->members);
  asn1p_expr_t *arg=TQ_FIRST(&field->rhs_pspecs->members);
  char *name=arg->reference->components[0].name;
  arg->reference->components[0].name="Missing";
  REQUIRE(asn1typed_extract_root_graph(tree,"ParamA","Root",NULL,&temp,error,sizeof(error))==-1);
  REQUIRE(!temp.types); arg->reference->components[0].name=name;
  {
   asn1p_expr_t *cached=arg->reference->ref_expr;
   arg->reference->ref_expr=decl(tree,"SetupRelease");
   REQUIRE(asn1typed_extract_root_graph(tree,"ParamA","Root",NULL,&temp,error,sizeof(error))==-1);
   REQUIRE(!temp.types); arg->reference->ref_expr=cached;
  }
  {
   asn1p_expr_t *cached=field->reference->ref_expr;
   field->reference->ref_expr=decl(tree,"Base");
   REQUIRE(asn1typed_extract_root_graph(tree,"ParamA","Root",NULL,&temp,error,sizeof(error))==-1);
   REQUIRE(!temp.types); field->reference->ref_expr=cached;
   field->reference->ref_expr=TQ_NEXT(field,next)->reference->ref_expr;
   REQUIRE(asn1typed_extract_root_graph(tree,"ParamA","Root",NULL,&temp,error,sizeof(error))==-1);
   REQUIRE(!temp.types); field->reference->ref_expr=cached;
  }
 }
 {
  asn1p_expr_t *field=TQ_FIRST(&decl(tree,"Defaults")->members);
  asn1p_value_t *v=field->marker.default_value; asn1c_integer_t saved=v->value.v_integer;
  v->value.v_integer=99;
  REQUIRE(asn1typed_extract_root_graph(tree,"ParamA","Defaults",NULL,&temp,error,sizeof(error))==-1);
  REQUIRE(!temp.types && strstr(error,"DEFAULT")); v->value.v_integer=saved;
 }
 {
  asn1p_expr_t *g=decl(tree,"SetupRelease");
  asn1p_ref_t *saved=g->lhs_params->params[0].governor;
  g->lhs_params->params[0].governor=TQ_FIRST(&decl(tree,"Root")->members)->reference;
  REQUIRE(asn1typed_extract_root_graph(tree,"ParamA","Root",NULL,&temp,error,sizeof(error))==-1);
  REQUIRE(!temp.types); g->lhs_params->params[0].governor=saved;
  g->lhs_params->params_count=2;
  REQUIRE(asn1typed_extract_root_graph(tree,"ParamA","Root",NULL,&temp,error,sizeof(error))==-1);
  REQUIRE(!temp.types); g->lhs_params->params_count=1;
 }
 {
  asn1p_expr_t *f=TQ_NEXT(TQ_FIRST(&decl(tree,"Defaults")->members),next);
  asn1p_value_t bad={0}, *saved=f->marker.default_value; bad.type=ATV_INTEGER; bad.value.v_integer=2;
  f->marker.default_value=&bad;
  REQUIRE(asn1typed_extract_root_graph(tree,"ParamA","Defaults",NULL,&temp,error,sizeof(error))==-1);
  REQUIRE(!temp.types); f->marker.default_value=saved;
  f=TQ_NEXT(f,next); saved=f->marker.default_value; bad.value.v_integer=99; f->marker.default_value=&bad;
  REQUIRE(asn1typed_extract_root_graph(tree,"ParamA","Defaults",NULL,&temp,error,sizeof(error))==-1);
  REQUIRE(!temp.types); f->marker.default_value=saved;
 }
 /* Every allocation in a parameter graph and enum/default graph is injected. */
 for(i=0;i<2;++i) {
  const char *name=i ? "Defaults":"Root"; size_t j;
  armed=1; fail_at=(size_t)-1; index_alloc=0;
  REQUIRE(!asn1typed_extract_root_graph(tree,"ParamA",name,NULL,&temp,error,sizeof(error)));
  n=index_alloc; armed=0; asn1typed_module_clear(&temp);
  for(j=0;j<n;++j) {
   armed=1; index_alloc=0; fail_at=j;
   REQUIRE(asn1typed_extract_root_graph(tree,"ParamA",name,NULL,&temp,error,sizeof(error))==-1);
   armed=0; REQUIRE(error[0] && !temp.types && !temp.source_name); asn1typed_module_clear(&temp);
  }
  printf("P3 %s allocation rollback: %zu positions PASS\n",name,n);
 }
 asn1p_delete(tree);
 REQUIRE(root.types[1].kind==ASN1TYPED_TYPE_CHOICE);
 REQUIRE(!strcmp(root.types[1].alternatives[1].type_ref.module,"ParamA"));
 REQUIRE(!strcmp(root.types[2].alternatives[1].type_ref.source_name,"Shared"));
 REQUIRE(defaults.types[0].fields[0].presence==ASN1TYPED_PRESENCE_DEFAULT);
 REQUIRE(defaults.types[0].fields[0].default_value.number==3);
 REQUIRE(!strcmp(defaults.types[0].fields[2].default_value.enum_symbol,"dB0"));
 REQUIRE(defaults.types[0].fields[3].default_value.number==7);
 REQUIRE(defaults.types[0].fields[4].presence==ASN1TYPED_PRESENCE_OPTIONAL);
 REQUIRE(defaults.types[0].fields[5].presence==ASN1TYPED_PRESENCE_MANDATORY);
 REQUIRE(!asn1typed_field_default_effective(&defaults.types[0].fields[2],0,NULL,&effective,&suppress));
 REQUIRE(suppress && !strcmp(effective.enum_symbol,"dB0")); asn1typed_default_value_clear(&effective);
 value.kind=ASN1TYPED_DEFAULT_INTEGER; value.number=3;
 REQUIRE(!asn1typed_field_default_effective(&defaults.types[0].fields[0],1,&value,&effective,&suppress));
 REQUIRE(suppress); asn1typed_default_value_clear(&effective); value.number=4;
 REQUIRE(!asn1typed_field_default_effective(&defaults.types[0].fields[0],1,&value,&effective,&suppress));
 REQUIRE(!suppress && effective.number==4); asn1typed_default_value_clear(&effective);
 REQUIRE(asn1typed_field_default_effective(&defaults.types[0].fields[4],0,NULL,&effective,&suppress)==-1);
 REQUIRE(!effective.enum_symbol && !effective.kind);
 asn1typed_type_clear(&copied); asn1typed_module_clear(&root); asn1typed_module_clear(&defaults);
 return 0;
}
