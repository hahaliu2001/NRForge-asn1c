#include "asn1typed_render_cpp_inline_enum_internal.h"
#include "asn1typed_render_cpp.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int text(const char *s) { return s && s[0]; }
static int storage(size_t n, size_t capacity, const void *p, size_t item) {
    return n <= capacity && capacity <= SIZE_MAX / item && (!!capacity == !!p);
}
static int empty_ref(const asn1typed_type_ref_t *r) {
    return r->kind == ASN1TYPED_REF_NAMED && !r->module && !r->source_name &&
        !r->actuals && !r->actual_count && r->primitive_kind == ASN1TYPED_PRIMITIVE_INVALID;
}
static int candidate(const asn1typed_field_t *f) {
    return f->inline_enumerated || f->type_semantics == ASN1TYPED_FIELD_INLINE_ENUMERATED;
}
static int field_ok(const asn1typed_field_t *f) {
    const asn1typed_class_field_relation_t *r = &f->class_field_relation;
    return text(f->source_name) && f->type_semantics == ASN1TYPED_FIELD_INLINE_ENUMERATED &&
        f->inline_enumerated && empty_ref(&f->type) && !f->has_class_field_relation &&
        !r->class_module && !r->class_source_name && !r->class_field_source_name &&
        !r->actual_index && !r->has_selector && !r->selector_source_name &&
        !f->size_constraint.has_size_constraint && !f->size_constraint.lower_bound &&
        !f->size_constraint.upper_bound && !f->size_constraint.is_extensible && !f->size_constraint.has_extension_addition && !f->size_constraint.extension_lower_bound && !f->size_constraint.extension_upper_bound &&
        asn1typed_integer_unsigned_empty(&f->value_range) && !f->value_range.has_value_range && !f->value_range.lower_bound &&
        !f->value_range.upper_bound && !f->value_range.is_extensible &&
        !f->value_range.tail && !f->value_range.tail_count && !f->value_range.extension_additions && !f->value_range.extension_addition_count &&
        !f->ioc.symbolic_id && !f->ioc.has_numeric_id && !f->ioc.numeric_id &&
        f->ioc.criticality == ASN1TYPED_CRITICALITY_REJECT &&
        (f->presence == ASN1TYPED_PRESENCE_MANDATORY || f->presence == ASN1TYPED_PRESENCE_OPTIONAL);
}
static int body_ok(const asn1typed_type_t *t) {
    return t && t->kind == ASN1TYPED_TYPE_ENUMERATED && !t->identity.module &&
        storage(t->enum_item_count,t->enum_item_capacity,t->enum_items,sizeof(*t->enum_items)) &&
        !t->identity.source_name && t->sequence_extension_evidence == ASN1TYPED_WIRE_EVIDENCE_UNAVAILABLE &&
        !t->sequence_root_field_count && !t->sequence_known_addition_count &&
        !t->has_valid_sequence_extension_structure;
}
static int count_body(const asn1typed_type_t *t, size_t *count, char *why, size_t size) {
    size_t j;
    if(!storage(t->field_count,t->field_capacity,t->fields,sizeof(*t->fields))) return -1;
    for(j=0;j<t->field_count;++j) if(candidate(&t->fields[j])) {
        if(t->kind != ASN1TYPED_TYPE_SEQUENCE || !field_ok(&t->fields[j]) ||
            !body_ok(t->fields[j].inline_enumerated) ||
            asn1typed_enumerated_evidence_validate(t->fields[j].inline_enumerated,why,size)) return -1;
        if(*count == SIZE_MAX) return -1;
        ++*count;
    }
    if(!storage(t->alternative_count,t->alternative_capacity,t->alternatives,sizeof(*t->alternatives))) return -1;
    for(j=0;j<t->alternative_count;++j) if(t->alternatives[j].inline_enumerated) {
        const asn1typed_choice_alternative_t *a = &t->alternatives[j];
        if(t->kind != ASN1TYPED_TYPE_CHOICE || !text(a->source_name) || !empty_ref(&a->type_ref) ||
            a->size_constraint.has_size_constraint || a->size_constraint.is_extensible || a->size_constraint.lower_bound || a->size_constraint.upper_bound || a->size_constraint.has_extension_addition || a->size_constraint.extension_lower_bound || a->size_constraint.extension_upper_bound ||
            !asn1typed_integer_unsigned_empty(&a->value_range) || a->value_range.has_value_range || a->value_range.is_extensible || a->value_range.lower_bound || a->value_range.upper_bound || a->value_range.tail || a->value_range.tail_count || a->value_range.extension_additions || a->value_range.extension_addition_count ||
            !body_ok(a->inline_enumerated) || asn1typed_enumerated_evidence_validate(a->inline_enumerated,why,size)) return -1;
        if(*count == SIZE_MAX) return -1;
        ++*count;
    }
    return 0;
}
static char *join(char *prefix,const char *part) {
    size_t a=prefix?strlen(prefix):0,b;
    char *p;
    if(!text(part)) { free(prefix); return NULL; }
    b=strlen(part);
    if(a > SIZE_MAX-b-2) { free(prefix); return NULL; }
    p=(char*)realloc(prefix,a+b+2);
    if(!p) { free(prefix); return NULL; }
    if(a) p[a++]='-';
    memcpy(p+a,part,b+1); return p;
}
static char *synthetic_name(const asn1typed_type_identity_t *id,
    const asn1typed_type_ref_t *bound,const char *field) {
    size_t i; char *name=NULL;
    if(bound) {
        if(bound->kind != ASN1TYPED_REF_NAMED || !text(bound->module) ||
            !text(bound->source_name) || bound->primitive_kind != ASN1TYPED_PRIMITIVE_INVALID ||
            bound->actual_count > SIZE_MAX/sizeof(*bound->actuals) ||
            (!!bound->actual_count != !!bound->actuals)) return NULL;
        name=join(NULL,bound->source_name);
        for(i=0;name && i<bound->actual_count;++i) {
            if(bound->actuals[i].kind != ASN1TYPED_ACTUAL_OBJECT_SET_REFERENCE) { free(name); return NULL; }
            name=join(name,bound->actuals[i].module);
            if(name) name=join(name,bound->actuals[i].source_name);
        }
    } else if(text(id->module) && text(id->source_name)) name=join(NULL,id->source_name);
    if(name) name=join(name,"inline");
    if(name) name=join(name,field);
    return name;
}
static void clear_fields(asn1typed_type_t *copy,const asn1typed_type_t *original) {
    size_t j;
    if(copy->alternatives != original->alternatives) {
        if(copy->alternatives) for(j=0;j<original->alternative_count;++j)
            if(original->alternatives[j].inline_enumerated && !copy->alternatives[j].inline_enumerated)
                asn1typed_type_ref_clear(&copy->alternatives[j].type_ref);
        free(copy->alternatives);
    }
    if(copy->fields != original->fields) {
        if(copy->fields) for(j=0;j<original->field_count;++j)
            if(candidate(&original->fields[j]) && copy->fields[j].type_semantics == ASN1TYPED_FIELD_FIXED_TYPE)
                asn1typed_type_ref_clear(&copy->fields[j].type);
        free(copy->fields);
    }
}
void asn1typed_cpp_inline_enum_view_clear(struct asn1typed_cpp_inline_enum_view *v) {
    size_t i,prefix;
    if(!v) return;
    if(v->storage.types && v->original) {
        prefix=v->storage.type_count-v->original_type_count;
        for(i=0;i<prefix;++i) free(v->storage.types[i].identity.source_name);
        for(i=0;i<v->original_type_count;++i)
            clear_fields(&v->storage.types[prefix+i],&v->original->types[i]);
        if(v->storage.bound_instances) for(i=0;i<v->original->bound_instance_count;++i)
            clear_fields(&v->storage.bound_instances[i].body,&v->original->bound_instances[i].body);
        free(v->storage.types); free(v->storage.bound_instances);
    }
    memset(v,0,sizeof(*v));
}
static int lower_alternatives(struct asn1typed_cpp_inline_enum_view *v,asn1typed_type_t *copy,
    const asn1typed_type_t *original,const asn1typed_type_ref_t *bound,size_t *next,char *why,size_t size) {
    size_t j,i; int needed=0;
    for(j=0;j<original->alternative_count;++j) if(original->alternatives[j].inline_enumerated) needed=1;
    if(!needed) return 0;
    copy->alternatives=(asn1typed_choice_alternative_t*)malloc(original->alternative_count*sizeof(*copy->alternatives));
    if(!copy->alternatives) return -1;
    memcpy(copy->alternatives,original->alternatives,original->alternative_count*sizeof(*copy->alternatives));
    copy->alternative_capacity=copy->alternative_count;
    for(j=0;j<copy->alternative_count;++j) if(original->alternatives[j].inline_enumerated) {
        asn1typed_choice_alternative_t *a=&copy->alternatives[j];
        asn1typed_type_t *t=&v->storage.types[(*next)++];
        char *module=bound?bound->module:original->identity.module;
        asn1typed_module_t enum_view={0}; char *validation=NULL;
        char *name=synthetic_name(&original->identity,bound,a->source_name);
        if(!name || !text(module)) { free(name); return -1; }
        for(i=0;i<v->storage.type_count;++i) {
            const asn1typed_type_identity_t *id=&v->storage.types[i].identity;
            if(text(id->module) && text(id->source_name) && !strcmp(id->module,module) && !strcmp(id->source_name,name)) {
                free(name); if(why&&size) snprintf(why,size,"synthetic inline ENUMERATED source-key collision"); return -1;
            }
        }
        *t=*a->inline_enumerated; t->identity.module=module; t->identity.source_name=name;
        enum_view.source_name=module; enum_view.types=t; enum_view.type_count=enum_view.type_capacity=1;
        if(asn1typed_render_cpp_owned_enum_types(&enum_view,"inline_enum_validation",&validation,why,size)) { free(validation); return -1; }
        free(validation);
        if(asn1typed_type_ref_init(&a->type_ref,module,name)) return -1;
        a->inline_enumerated=NULL;
    }
    return 0;
}
static int lower_body(struct asn1typed_cpp_inline_enum_view *v,asn1typed_type_t *copy,
    const asn1typed_type_t *original,const asn1typed_type_ref_t *bound,size_t *next,char *why,size_t size) {
    size_t j,i; int needed=0;
    for(j=0;j<original->field_count;++j) if(candidate(&original->fields[j])) needed=1;
    if(!needed) return lower_alternatives(v,copy,original,bound,next,why,size);
    copy->fields=(asn1typed_field_t*)malloc(original->field_count*sizeof(*copy->fields));
    if(!copy->fields) return -1;
    memcpy(copy->fields,original->fields,original->field_count*sizeof(*copy->fields));
    copy->field_capacity=copy->field_count;
    for(j=0;j<copy->field_count;++j) if(candidate(&original->fields[j])) {
        asn1typed_field_t *f=&copy->fields[j];
        asn1typed_type_t *t=&v->storage.types[(*next)++];
        char *module=bound?bound->module:original->identity.module;
        asn1typed_module_t enum_view={0}; char *validation=NULL;
        char *name=synthetic_name(&original->identity,bound,f->source_name);
        if(!name || !text(module)) { free(name); return -1; }
        for(i=0;i<v->storage.type_count;++i) {
            const asn1typed_type_identity_t *id=&v->storage.types[i].identity;
            if(text(id->module) && text(id->source_name) && !strcmp(id->module,module) && !strcmp(id->source_name,name)) {
                free(name); if(why&&size) snprintf(why,size,"synthetic inline ENUMERATED source-key collision"); return -1;
            }
        }
        *t=*f->inline_enumerated; t->identity.module=module; t->identity.source_name=name;
        enum_view.source_name=t->identity.module; enum_view.types=t; enum_view.type_count=enum_view.type_capacity=1;
        if(asn1typed_render_cpp_owned_enum_types(&enum_view,"inline_enum_validation",&validation,why,size)) { free(validation); return -1; }
        free(validation);
        if(asn1typed_type_ref_init(&f->type,module,name)) return -1;
        f->type_semantics=ASN1TYPED_FIELD_FIXED_TYPE; f->inline_enumerated=NULL;
    }
    return lower_alternatives(v,copy,original,bound,next,why,size);
}
int asn1typed_cpp_inline_enum_view_init(struct asn1typed_cpp_inline_enum_view *v,
    const asn1typed_module_t *m,char *why,size_t size) {
    size_t i,count=0,next=0,total;
    if(why&&size) why[0]=0;
    if(!v || !m || !text(m->source_name) || !storage(m->type_count,m->type_capacity,m->types,sizeof(*m->types)) ||
        !storage(m->bound_instance_count,m->bound_instance_capacity,m->bound_instances,sizeof(*m->bound_instances)) ||
        !storage(m->ioc_registry_count,m->ioc_registry_capacity,m->ioc_registries,sizeof(*m->ioc_registries))) goto bad;
    memset(v,0,sizeof(*v));
    for(i=0;i<m->type_count;++i) if(count_body(&m->types[i],&count,why,size)) goto bad;
    for(i=0;i<m->bound_instance_count;++i) {
        const asn1typed_bound_instance_t *b=&m->bound_instances[i]; size_t j;
        if(count_body(&b->body,&count,why,size)) goto bad;
        if(b->ioc_binding.has_valid_binding) for(j=0;j<b->body.field_count;++j)
            if(candidate(&b->body.fields[j]) && (j==b->ioc_binding.id_field_ordinal ||
                j==b->ioc_binding.criticality_field_ordinal || j==b->ioc_binding.value_field_ordinal)) goto bad;
    }
    if(!count) { v->module=m; return 0; }
    if(count > SIZE_MAX-m->type_count || count+m->type_count > SIZE_MAX/sizeof(*m->types)) goto bad;
    total=count+m->type_count;
    v->original=m; v->original_type_count=m->type_count; v->storage=*m;
    v->storage.types=(asn1typed_type_t*)calloc(total,sizeof(*m->types));
    v->storage.bound_instances=NULL;
    v->storage.type_count=v->storage.type_capacity=total;
    if(!v->storage.types) goto bad;
    if(m->type_count) memcpy(v->storage.types+count,m->types,m->type_count*sizeof(*m->types));
    if(m->bound_instance_count) {
        v->storage.bound_instances=(asn1typed_bound_instance_t*)malloc(m->bound_instance_count*sizeof(*m->bound_instances));
        if(!v->storage.bound_instances) goto bad;
        memcpy(v->storage.bound_instances,m->bound_instances,m->bound_instance_count*sizeof(*m->bound_instances));
        v->storage.bound_instance_capacity=v->storage.bound_instance_count;
    }
    for(i=0;i<m->type_count;++i) if(lower_body(v,&v->storage.types[count+i],&m->types[i],NULL,&next,why,size)) goto bad;
    for(i=0;i<m->bound_instance_count;++i) if(lower_body(v,&v->storage.bound_instances[i].body,&m->bound_instances[i].body,&m->bound_instances[i].identity,&next,why,size)) goto bad;
    v->module=&v->storage; return 0;
bad:
    if(why&&size&&!why[0]) snprintf(why,size,"unsupported inline ENUMERATED metadata, storage or allocation failure");
    if(v) asn1typed_cpp_inline_enum_view_clear(v);
    return -1;
}
