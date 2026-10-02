#include "asn1_common.h"
#include "typed_poc.h"
#include <asn1fix_export.h>
#include <asn1_namespace.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct field_s { char *name, *type, *element; int optional, list; } field_t;
typedef struct message_s { char *name; field_t *fields; size_t count; } message_t;

static char *
identifier(const char *s, int type_name) {
    size_t n = strlen(s), j = 0;
    char *out = calloc(n * 2 + 2, 1);
    for(size_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char)s[i];
        if(!isalnum(c)) { if(!type_name && j && out[j-1] != '_') out[j++]='_'; continue; }
        if(type_name) {
            if(isupper(c) && i && (islower((unsigned char)s[i-1]) || (isupper((unsigned char)s[i-1]) && i+1<n && islower((unsigned char)s[i+1])))) out[j++] = ' ';
            out[j++] = (char)c;
        } else {
            if(isupper(c) && i && (islower((unsigned char)s[i-1]) || isdigit((unsigned char)s[i-1]) || (isupper((unsigned char)s[i-1]) && i+1<n && islower((unsigned char)s[i+1]))) && j && out[j-1] != '_') out[j++]='_';
            out[j++] = (char)tolower(c);
        }
    }
    if(!type_name) while(j && out[j-1]=='_') j--;
    out[j]=0;
    if(type_name) { int cap=1; size_t w=0; for(size_t i=0;i<j;i++){ if(out[i]==' '){cap=1;continue;} out[w++]=cap ? (char)toupper((unsigned char)out[i]) : (char)tolower((unsigned char)out[i]); cap=0; } j=w; out[j]=0; }
    return out;
}

static const char *
ref_name(asn1p_expr_t *e) {
    if(!e) return NULL;
    if(e->value && e->value->type==ATV_REFERENCED && e->value->value.reference && e->value->value.reference->comp_count) return e->value->value.reference->components[e->value->value.reference->comp_count-1].name;
    if(e->reference && e->reference->comp_count) return e->reference->components[e->reference->comp_count-1].name;
    return e->Identifier;
}

static asn1p_expr_t *
lookup_named(asn1p_t *asn, asn1p_expr_t *context, asn1p_ref_t *ref, int require_ioc) {
    asn1p_expr_t *r=ref ? ref->ref_expr : NULL;
    asn1p_module_t *mod; asn1p_expr_t *e;
    if(r && (!require_ioc || r->ioc_table)) return r;
    if(ref && ref->comp_count) {
        const char *name=ref->components[ref->comp_count-1].name;
        if(ref->module) mod=ref->module;
        else if(context) mod=context->module;
        else mod=NULL;
        if(mod) TQ_FOR(e,&mod->members,next)
            if((!require_ioc || e->ioc_table) && e->Identifier && !strcmp(e->Identifier,name)) return e;
        TQ_FOR(mod,&asn->modules,mod_next) TQ_FOR(e,&mod->members,next)
            if((!require_ioc || e->ioc_table) && e->Identifier && !strcmp(e->Identifier,name)) return e;
    }
    return NULL;
}

static asn1p_expr_t *
lookup_ioc(asn1p_t *asn, asn1p_expr_t *context, asn1p_ref_t *ref) {
    return lookup_named(asn,context,ref,1);
}

static asn1p_expr_t *
find_terminal(asn1p_t *asn, asn1p_expr_t *e) {
    asn1_namespace_t *ns=asn1_namespace_new_from_module(e->module, 1);
    asn1p_expr_t *r=asn1f_find_terminal_type_ex(asn,ns,e);
    asn1_namespace_free(ns);
    return r;
}

static asn1p_expr_t *
find_table_ref(asn1p_t *asn, asn1p_expr_t *e) {
    asn1p_expr_t *m, *p;
    if(e->reference) { asn1p_expr_t *r=lookup_ioc(asn,e,e->reference); if(r) return r; }
    /* Parameterized types retain the actual object-set argument as a typed
       expression inside the specialization's parameter constraints. */
    if(e->rhs_pspecs) TQ_FOR(p,&e->rhs_pspecs->members,next) {
        asn1p_constraint_t *c=p->constraints;
        if(c && c->containedSubtype && c->containedSubtype->type==ATV_TYPE) {
            asn1p_expr_t *actual=c->containedSubtype->value.v_type;
            if(actual && actual->reference) {
                asn1p_expr_t *r=lookup_ioc(asn,actual,actual->reference);
                if(r) return r;
            }
        }
    }
    TQ_FOR(m, &e->members, next) { asn1p_expr_t *r=find_table_ref(asn,m); if(r) return r; }
    return NULL;
}

static int
extract(asn1p_t *asn, asn1p_expr_t *msg, message_t *ir) {
    asn1p_expr_t *set=find_table_ref(asn,msg); asn1p_ioc_table_t *table;
    if(!set || !(table=set->ioc_table)) return -1;
    ir->name=identifier(msg->Identifier,1);
    for(size_t i=0;i<table->rows;i++) {
        asn1p_ioc_row_t *row=table->row[i]; struct asn1p_ioc_cell_s *vc=NULL,*pc=NULL;
        for(size_t c=0;c<row->columns;c++) {
            const char *id=row->column[c].field->Identifier;
            if(id && id[0]=='&') id++;
            if(id && !strcasecmp(id,"Value")) vc=&row->column[c];
            if(id && !strcasecmp(id,"presence")) pc=&row->column[c];
        }
        if(!vc || !vc->value) continue;
        const char *tn=ref_name(vc->value); if(!tn) continue;
        asn1p_expr_t *te=vc->value->reference ? vc->value->reference->ref_expr : NULL;
        if(!te && vc->value->reference) te=lookup_named(asn,vc->value,vc->value->reference,0);
        if(!te) te=vc->value;
        asn1p_expr_t *terminal_type=find_terminal(asn,te);
        if(terminal_type) te=terminal_type;
        field_t f={0}; f.type=identifier(tn,1);
        /* The semantic IE name comes from the &id value-reference (typically
           id-Foo). If that cell has no symbolic identifier, fall back to the
           ASN.1 value type name; this bounded fallback keeps the POC generic. */
        const char *semantic=tn;
        for(size_t c=0;c<row->columns;c++) {
            const char *id=row->column[c].field->Identifier;
            if(id && id[0]=='&' && !strcasecmp(id+1,"id") && row->column[c].value) {
                const char *symbol=ref_name(row->column[c].value);
                if(symbol && *symbol) semantic=(!strncmp(symbol,"id-",3) && symbol[3]) ? symbol+3 : symbol;
                break;
            }
        }
        f.name=strdup(semantic);
        if(te->expr_type==ASN_CONSTR_SEQUENCE_OF || te->expr_type==ASN_CONSTR_SET_OF) {
            asn1p_expr_t *el=TQ_FIRST(&te->members); f.list=1;
            const char *en=ref_name(el); f.element=identifier(en?en:"Any",1);
        }
        if(pc && pc->value) { const char *p=ref_name(pc->value); f.optional=p && !strcasecmp(p,"optional"); }
        field_t *nf=realloc(ir->fields,(ir->count+1)*sizeof(*nf)); if(!nf)return -1; ir->fields=nf; ir->fields[ir->count++]=f;
    }
    return ir->count ? 0 : -1;
}

static void cpp(message_t *m) {
    printf("#include <optional>\n#include <vector>\n\nstruct %s {\n",m->name);
    for(size_t i=0;i<m->count;i++){field_t *f=&m->fields[i]; char *n=identifier(f->name,0); printf("    "); if(f->optional) printf("std::optional<%s>",f->type); else if(f->list) printf("std::vector<%s>",f->element); else printf("%s",f->type); printf(" %s;\n",n); free(n); } puts("};");
}
static void python(message_t *m) {
    puts("from dataclasses import dataclass, field\n\n@dataclass(kw_only=True)"); printf("class %s:\n",m->name);
    for(size_t i=0;i<m->count;i++){field_t *f=&m->fields[i]; char *n=identifier(f->name,0); printf("    %s: ",n); free(n); if(f->optional) printf("%s | None = None",f->type); else if(f->list) printf("list[%s] = field(default_factory=list)",f->element); else printf("%s",f->type); puts(""); }
}
int asn1c_typed_poc(asn1p_t *asn, const char *name) {
    asn1p_module_t *mod; asn1p_expr_t *e, *msg=NULL; message_t ir={0};
    TQ_FOR(mod,&asn->modules,mod_next) TQ_FOR(e,&mod->members,next) if(e->Identifier && !strcmp(e->Identifier,name) && e->expr_type==ASN_CONSTR_SEQUENCE) { msg=e; break; }
    if(!msg || extract(asn,msg,&ir)) { fprintf(stderr,"typed-poc: cannot extract %s from fixed IOC tables\n",name); return -1; }
    cpp(&ir); puts("\n# --- Python ---"); python(&ir); return 0;
}
