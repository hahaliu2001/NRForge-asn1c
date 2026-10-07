#include <stddef.h>
#include <asn1fix.h>
#include <asn1p_list.h>
#include <asn1typed.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "developer_tree.h"

static void help(FILE *f) {
    fprintf(f,"Usage: asn1typed_tree_inspect --module-list FILE [--asn1-root DIR] --type MODULE.TYPE [--member NAME]\n"
        "Inspect one declaration or its direct member in the fixed ASN.1 tree.\n"
        "Example: asn1typed_tree_inspect --asn1-root asn1 --module-list modules.txt --type Example.Request --member payload\n"
        "Exit: 0 success; 2 invalid CLI; 3 module-list/load/parse failure; 4 fix failure; 5 lookup failure.\n");
}
static const char *meta_name(asn1p_expr_meta_e m) {
    static const char *n[]={"INVALID","TYPE","TYPEREF","VALUE","VALUESET","OBJECT","OBJECTCLASS","OBJECTFIELD"};
    return (unsigned)m < sizeof(n)/sizeof(n[0]) ? n[m] : "UNKNOWN";
}
static const char *expr_name(asn1p_expr_type_e t) {
    const char *name=ASN_EXPR_TYPE2STR(t);
    if(name) return name;
    switch(t) {
    case A1TC_INVALID: return "INVALID";
    case A1TC_REFERENCE: return "REFERENCE";
    case A1TC_EXPORTVAR: return "EXPORTVAR";
    case A1TC_UNIVERVAL: return "UNIVERSAL-VALUE";
    case A1TC_BITVECTOR: return "BITVECTOR";
    case A1TC_OPAQUE: return "OPAQUE";
    case A1TC_EXTENSIBLE: return "EXTENSIBLE";
    case A1TC_COMPONENTS_OF: return "COMPONENTS-OF";
    case A1TC_VALUESET: return "VALUESET";
    case A1TC_CLASSDEF: return "CLASS";
    case A1TC_INSTANCE: return "INSTANCE";
    default: return "INTERNAL";
    }
}
static void print_value(const asn1p_value_t *v) {
    if(!v) { printf("UNKNOWN"); return; }
    if(v->type==ATV_INTEGER) printf("INTEGER %lld",(long long)v->value.v_integer);
    else if(v->type==ATV_TYPE) {
        const asn1p_expr_t *type=v->value.v_type;
        printf("TYPE");
        if(type && type->Identifier) printf(" %s",type->Identifier);
        if(type && type->reference) printf(" REFERENCE %s",asn1p_ref_string(type->reference));
        if(type) printf(" META_TYPE %s EXPR_TYPE %s (%d)",meta_name(type->meta_type),expr_name(type->expr_type),type->expr_type);
    }
    else if(v->type==ATV_MIN) printf("MIN");
    else if(v->type==ATV_MAX) printf("MAX");
    else if(v->type==ATV_REFERENCED) printf("REFERENCE %s",asn1p_ref_string(v->value.reference));
    else printf("VALUE_TYPE %d",v->type);
}
static void print_constraint(const asn1p_constraint_t *c, unsigned depth) {
    unsigned i;
    if(!c) return;
    for(i=0;i<depth;i++) printf("  ");
    printf("%s",asn1p_constraint_type2str(c->type));
    if(c->type==ACT_EL_TYPE && c->containedSubtype) {
        printf(" CONTAINED_SUBTYPE "); print_value(c->containedSubtype); printf("\n"); return;
    }
    if(c->type==ACT_EL_RANGE || c->type==ACT_EL_LLRANGE || c->type==ACT_EL_RLRANGE || c->type==ACT_EL_ULRANGE) {
        printf("\n"); for(i=0;i<depth+1;i++) printf("  "); printf("LOWER "); print_value(c->range_start);
        printf("\n"); for(i=0;i<depth+1;i++) printf("  "); printf("UPPER "); print_value(c->range_stop); printf("\n"); return;
    }
    if(c->type==ACT_EL_VALUE) { printf(" "); print_value(c->value); printf("\n"); return; }
    printf("\n");
    for(i=0;i<c->el_count;i++) print_constraint(c->elements[i],depth+1);
}
static void print_formals(const asn1p_paramlist_t *params, const char *prefix) {
    int i;
    if(!params) return;
    printf("%s_PARAMETER_COUNT %d\n",prefix,params->params_count);
    for(i=0;i<params->params_count;i++) {
        const struct asn1p_param_s *p=&params->params[i];
        printf("%s_PARAMETER %d NAME %s",prefix,i,p->argument?p->argument:"<anonymous>");
        if(p->governor) printf(" GOVERNOR %s",asn1p_ref_string(p->governor));
        else printf(" GOVERNOR NONE");
        putchar('\n');
    }
}
static void print_actual_members(const asn1p_expr_t *expr, unsigned depth, size_t *index) {
    const asn1p_expr_t *member;
    if(depth>8) return;
    TQ_FOR(member,&expr->members,next) {
        printf("ACTUAL_NODE %zu DEPTH %u META_TYPE %s EXPR_TYPE %s (%d)",
            (*index)++,depth,meta_name(member->meta_type),expr_name(member->expr_type),member->expr_type);
        if(member->Identifier) printf(" IDENTIFIER %s",member->Identifier);
        if(member->reference) printf(" REFERENCE %s",asn1p_ref_string(member->reference));
        putchar('\n');
        print_actual_members(member,depth+1,index);
    }
}
static void print_actuals(const asn1p_expr_t *expr, const char *prefix) {
    const asn1p_expr_t *actual;
    size_t n=0, i=0;
    if(!expr || !expr->rhs_pspecs) return;
    TQ_FOR(actual,&expr->rhs_pspecs->members,next) n++;
    printf("%s_PARAMETER_COUNT %zu\n",prefix,n);
    TQ_FOR(actual,&expr->rhs_pspecs->members,next) {
        const char *kind=actual->reference?"REFERENCE":
            (actual->meta_type==AMT_VALUE ? "VALUE" :
            (actual->meta_type==AMT_VALUESET ? "VALUESET" : "EXPRESSION"));
        printf("%s_PARAMETER %zu KIND %s META_TYPE %s EXPR_TYPE %s (%d)",
            prefix,i++,kind,meta_name(actual->meta_type),expr_name(actual->expr_type),actual->expr_type);
        if(actual->Identifier) printf(" IDENTIFIER %s",actual->Identifier);
        if(actual->reference) printf(" REFERENCE %s",asn1p_ref_string(actual->reference));
        if(actual->value) printf(" VALUE_TYPE %d",actual->value->type);
        putchar('\n');
        if(actual->value && actual->value->type==ATV_VALUESET && actual->value->value.constraint) {
            printf("%s_PARAMETER %zu VALUE_SET\n",prefix,i-1);
            print_constraint(actual->value->value.constraint,1);
        }
        if(actual->constraints) {
            printf("%s_PARAMETER %zu CONSTRAINT\n",prefix,i-1);
            print_constraint(actual->constraints,1);
        }
        {
            size_t node_index=0;
            print_actual_members(actual,1,&node_index);
        }
    }
}
static void report(const asn1p_expr_t *e) {
    asn1p_expr_t *child;
    size_t count=0;
    const char *file=(e->module && e->module->source_file_name)?e->module->source_file_name:"<unknown>";
    printf("SOURCE %s:%d\nMETA_TYPE %s (%d)\nEXPR_TYPE %s (%d)\n",file,e->_lineno,meta_name(e->meta_type),e->meta_type,expr_name(e->expr_type),e->expr_type);
    if(e->Identifier) printf("IDENTIFIER %s\n",e->Identifier);
    if(e->marker.flags==EM_OPTIONAL) puts("PRESENCE optional");
    else if(e->marker.flags==EM_DEFAULT) puts("PRESENCE default");
    else if(e->marker.flags==EM_OMITABLE) puts("PRESENCE omitable");
    else puts("PRESENCE mandatory");
    if(e->reference) printf("REFERENCE %s\n",asn1p_ref_string(e->reference));
    if(e->reference && e->reference->ref_expr && e->reference->ref_expr->module)
        printf("RESOLVED_TARGET %s.%s\n",e->reference->ref_expr->module->ModuleName,e->reference->ref_expr->Identifier);
    print_formals(e->lhs_params,"LHS");
    print_actuals(e,"RHS");
    count=0; TQ_FOR(child,&e->members,next) count++;
    printf("DIRECT_CHILDREN %zu\n",count);
    TQ_FOR(child,&e->members,next) {
        printf("CHILD %s META_TYPE %s EXPR_TYPE %s (%d)\n",
            child->Identifier?child->Identifier:"<anonymous>",meta_name(child->meta_type),
            expr_name(child->expr_type),child->expr_type);
        if(child->reference) printf("CHILD_REFERENCE %s\n",asn1p_ref_string(child->reference));
        print_formals(child->lhs_params,"CHILD_LHS");
        print_actuals(child,"CHILD_RHS");
    }
    if(!e->constraints) puts("DECLARED_CONSTRAINT NONE");
    else { puts("DECLARED_CONSTRAINT"); print_constraint(e->constraints,1); }
    if(!e->combined_constraints) puts("COMBINED_CONSTRAINT NONE");
    else { puts("COMBINED_CONSTRAINT"); print_constraint(e->combined_constraints,1); }
}
int main(int argc,char **argv) {
    const char *list=NULL,*root=NULL,*qualified=NULL,*member=NULL,*failed=NULL,*dot;
    char *module_name=NULL,*type_name=NULL;
    dev_tree_options_t options; asn1p_t *tree; asn1p_module_t *mod,*found_mod=NULL; asn1p_expr_t *decl=NULL,*e;
    int i,rc=0,mods=0,matches=0;
    for(i=1;i<argc;i++) {
        if(!strcmp(argv[i],"--help")) { help(stdout); return 0; }
        if(i+1>=argc) { help(stderr); return 2; }
        if(!strcmp(argv[i],"--module-list")) list=argv[++i];
        else if(!strcmp(argv[i],"--asn1-root")) root=argv[++i];
        else if(!strcmp(argv[i],"--type")) qualified=argv[++i];
        else if(!strcmp(argv[i],"--member")) member=argv[++i];
        else { help(stderr); return 2; }
    }
    if(!list || !qualified || !(dot=strchr(qualified,'.')) || dot==qualified || !dot[1]) { help(stderr); return 2; }
    module_name=strndup(qualified,(size_t)(dot-qualified)); type_name=strdup(dot+1);
    if(!module_name || !type_name) { free(module_name); free(type_name); return 2; }
    options.asn1_root=root; options.module_list=list; tree=dev_tree_load(&options,&failed);
    if(!tree) { printf("LOAD/PARSE FAIL\n"); if(failed) printf("FILE %s\n",failed); free(module_name);free(type_name);return 3; }
    if(dev_tree_fix(tree)<0) { puts("FIX FAIL"); rc=4; goto done; }
    TQ_FOR(mod,&tree->modules,mod_next) { if(mod->ModuleName && !strcmp(mod->ModuleName,module_name)) { found_mod=mod; mods++; } }
    if(mods!=1) { printf("LOOKUP FAIL MODULE %s %s\n",module_name,mods?"AMBIGUOUS":"MISSING");rc=5;goto done; }
    TQ_FOR(e,&found_mod->members,next) if(e->Identifier && !strcmp(e->Identifier,type_name)) { decl=e;matches++; }
    if(matches!=1) { printf("LOOKUP FAIL TYPE %s.%s %s\n",module_name,type_name,matches?"AMBIGUOUS":"MISSING");rc=5;goto done; }
    printf("MODULE %s\nTYPE %s\n",module_name,type_name);
    if(member) {
        asn1p_expr_t *selected=NULL; int found=0;
        TQ_FOR(e,&decl->members,next) if(e->Identifier && !strcmp(e->Identifier,member)) { selected=e;found++; }
        if(found!=1) { printf("LOOKUP FAIL MEMBER %s %s\n",member,found?"AMBIGUOUS":"MISSING");rc=5;goto done; }
        printf("MEMBER %s\n",member); report(selected);
    } else report(decl);
done:
    asn1p_delete(tree); free(module_name); free(type_name); return rc;
}
