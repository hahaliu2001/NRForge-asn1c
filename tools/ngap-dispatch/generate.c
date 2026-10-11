/* Complete owned-evidence controller. No wire layout is inferred from message
 * names: every registration is proven against the finalized procedure table. */
#include <asn1typed_extract.h>
#include <asn1typed_render_cpp.h>
#include <asn1typed_render_cpp_internal.h>
#include <asn1typed_render_cpp_ioc_internal.h>
#include "developer_tree.h"
#include <dirent.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define MAX_MESSAGES 768
static const char *profile = "ngap";
static const char *profile_upper = "NGAP";
static const char *profile_namespace = "nrforge::ngap";
static const char *runtime_header = "pdu.hpp";
struct message {
    char source[128];
    char *name, *body_name, *ns;
    asn1typed_module_t body;
    asn1typed_target_envelope_t envelope;
    unsigned role, criticality;
    intmax_t code;
};
static int same_text(const char *a, const char *b) {
    return a && b ? !strcmp(a,b) : !a && !b;
}
static int empty_ref(const asn1typed_type_ref_t *r) {
    return r->kind==ASN1TYPED_REF_NAMED && !r->module && !r->source_name
        && !r->actual_count && !r->actuals && r->primitive_kind==ASN1TYPED_PRIMITIVE_INVALID;
}
static int same_optional_ref(const asn1typed_type_ref_t *a,const asn1typed_type_ref_t *b) {
    return (empty_ref(a) && empty_ref(b)) || asn1typed_type_ref_equal(a,b);
}
static int same_schema(const asn1typed_target_envelope_t *a,
        const asn1typed_target_envelope_t *b) {
    const asn1typed_envelope_header_t *x = &a->header, *y = &b->header;
    size_t i,j;
#define EQ(field) if(x->field != y->field) return 0
#define REF(field) if(!asn1typed_type_ref_equal(&x->field,&y->field)) return 0
    if(a->root_count != b->root_count || a->row_count != b->row_count) return 0;
    REF(pdu); REF(procedure_class); REF(object_set); REF(procedure_type); REF(criticality_type);
    EQ(evidence); EQ(declared_root_count); EQ(declared_row_count); EQ(known_addition_count);
    EQ(choice_is_extensible); EQ(object_set_is_extensible); EQ(procedure_is_extensible);
    EQ(criticality_is_extensible); EQ(procedure_lower_bound); EQ(procedure_upper_bound);
    EQ(has_class_default); EQ(class_default_criticality);
#undef EQ
#undef REF
    for(i=0;i<a->root_count;++i) {
        const asn1typed_envelope_root_t *r=&a->roots[i], *s=&b->roots[i];
        if(i<3 && (x->payload_optional[i] != y->payload_optional[i]
            || !same_text(x->criticalities[i].source_name,y->criticalities[i].source_name)
            || x->criticalities[i].assigned_number != y->criticalities[i].assigned_number
            || x->criticalities[i].source_ordinal != y->criticalities[i].source_ordinal
            || x->criticalities[i].per_index != y->criticalities[i].per_index)) return 0;
#define EQ(field) if(r->field != s->field) return 0
#define REF(field) if(!asn1typed_type_ref_equal(&r->field,&s->field)) return 0
        if(!same_text(r->source_name,s->source_name)) return 0;
        EQ(unsupported_payload); EQ(object_set_is_extensible); EQ(declared_row_count);
        REF(sequence); REF(procedure_class); REF(object_set);
        if(r->unsupported_payload) {
            if(!same_optional_ref(&r->procedure_type,&s->procedure_type)
                || !same_optional_ref(&r->criticality_type,&s->criticality_type)) return 0;
        } else { REF(procedure_type); REF(criticality_type); }
        EQ(role); EQ(source_ordinal); EQ(field_count); EQ(sequence_is_extensible); EQ(evidence);
        EQ(effective_tag_class); EQ(effective_tag_number); EQ(has_per_root_index); EQ(per_root_index);
#undef EQ
#undef REF
        for(j=0;j<3;++j) if(!same_text(r->field_names[j],s->field_names[j])
            || !same_text(r->class_field_names[j],s->class_field_names[j])
            || r->role_ordinals[j] != s->role_ordinals[j]) return 0;
        for(j=0;j<2;++j) if(!same_text(r->selectors[j],s->selectors[j])) return 0;
    }
    for(i=0;i<a->row_count;++i) {
        const asn1typed_envelope_row_t *r=&a->rows[i], *s=&b->rows[i];
        if(!same_text(r->symbolic_code,s->symbolic_code)
            || r->has_numeric_code != s->has_numeric_code || r->numeric_code != s->numeric_code
            || r->expected_criticality != s->expected_criticality
            || r->default_provenance != s->default_provenance) return 0;
        for(j=0;j<3;++j) if(r->payload_present[j] != s->payload_present[j]
            || (r->payload_present[j] && !asn1typed_type_ref_equal(&r->payloads[j],&s->payloads[j]))) return 0;
    }
    return 1;
}
static char *join(const char *a,const char *b) {
    size_t x=strlen(a),y=strlen(b);
    char *p;
    if(x > SIZE_MAX-y-1) return NULL;
    p=malloc(x+y+1); if(p) { memcpy(p,a,x); memcpy(p+x,b,y+1); } return p;
}
static char *identity_name(const asn1typed_type_ref_t *r) {
    char *a=join(r->module,"-"),*b,*n;
    if(!a) return NULL;
    b=join(a,r->source_name); free(a); if(!b) return NULL;
    n=asn1typed_render_cpp_final_name(b,ASN1TYPED_NAME_TYPE); free(b); return n;
}
static int empty_directory(const char *path) {
    DIR *dir=opendir(path);
    struct dirent *entry;
    int empty=1;
    if(!dir) return 0;
    while((entry=readdir(dir))) if(strcmp(entry->d_name,".") && strcmp(entry->d_name,"..")) { empty=0; break; }
    return closedir(dir)==0 && empty;
}
static int path_name(char path[4096],const char *dir,const char *name) {
    int n=snprintf(path,4096,"%s/%s",dir,name);
    return n>=0 && n<4096 ? 0 : -1;
}
static FILE *new_file(const char *dir,const char *relative) {
    char path[4096];
    return path_name(path,dir,relative) ? NULL : fopen(path,"wx");
}
static int finish_file(FILE *f) {
    int valid=!ferror(f), closed=fclose(f)==0;
    return valid && closed ? 0 : -1;
}
static int save(const char *dir,const char *relative,const char *text) {
    FILE *f=new_file(dir,relative);
    if(!f) return -1;
    if(fputs(text,f)==EOF) { fclose(f); return -1; }
    return finish_file(f);
}
static int json_text(FILE *f,const char *text) {
    const unsigned char *p=(const unsigned char *)text;
    if(fputc('"',f)==EOF) return -1;
    for(;*p;++p) {
        if(*p=='"' || *p=='\\') { if(fputc('\\',f)==EOF) return -1; }
        if(*p<32) { if(fprintf(f,"\\u%04x",(unsigned)*p)<0) return -1; }
        else if(fputc(*p,f)==EOF) return -1;
    }
    return fputc('"',f)==EOF ? -1 : 0;
}
static int append_line(const char *line,size_t n,struct message *rows,size_t *count) {
    size_t k;
    if(n && line[n-1]=='\r') --n;
    if(!n || line[0]=='#') return 0;
    if(*count==MAX_MESSAGES || n>=sizeof(rows[0].source)) return -1;
    for(k=0;k<n;++k) if(!((line[k]>='A' && line[k]<='Z') || (line[k]>='a' && line[k]<='z')
        || (k && ((line[k]>='0' && line[k]<='9') || line[k]=='-')))) return -1;
    for(k=0;k<*count;++k) if(strlen(rows[k].source)==n && !memcmp(rows[k].source,line,n)) return -1;
    memcpy(rows[*count].source,line,n); rows[(*count)++].source[n]='\0';
    return 0;
}
static int read_list(const char *path,struct message *rows,size_t *count) {
    FILE *f=fopen(path,"rb");
    char line[256];
    size_t n=0;
    int c,valid=1;
    if(!f) return -1;
    while((c=fgetc(f))!=EOF) {
        if(c=='\n') { if(append_line(line,n,rows,count)) { valid=0; break; } n=0; }
        else {
            /* Check bytes before C-string interpretation: embedded NUL and
             * non-ASCII cannot truncate or conceal a duplicate entry. */
            if(!c || c>127 || (c<32 && c!='\r' && c!='\t') || n==sizeof(line)) { valid=0; break; }
            line[n++]=(char)c;
        }
    }
    if(valid && n && append_line(line,n,rows,count)) valid=0;
    if(ferror(f) || !*count) valid=0;
    if(fclose(f)) valid=0;
    return valid ? 0 : -1;
}
static int preflight(struct message *rows,size_t count,char *why,size_t why_size) {
    const asn1typed_target_envelope_t *schema=&rows[0].envelope;
    size_t i,j,role,declared=0;
#define BAD(text) do { snprintf(why,why_size,"%s",text); return -1; } while(0)
    if(!strcmp(profile,"f1ap")) {
        if(schema->root_count!=4 || schema->header.choice_is_extensible) BAD("F1AP framing profile mismatch");
    } else if(schema->root_count!=3 || !schema->header.choice_is_extensible) BAD("NGAP framing profile mismatch");
    for(i=0;i<count;++i) {
        struct message *m=&rows[i];
        const asn1typed_target_envelope_t *d=&m->envelope;
        const char *reserved[]={"Body"};
        if(asn1typed_target_envelope_validate(d,why,why_size)) return -1;
        if(!same_schema(schema,d)) BAD("inconsistent owned envelope schema");
        if(!m->body.type_count || !m->body.types
            || !same_text(m->body.types[0].identity.module,d->target_body.module)
            || !same_text(m->body.types[0].identity.source_name,d->target_body.source_name)) BAD("target BODY identity mismatch");
        m->role=(unsigned)d->roots[d->target_root_ordinal].role;
        m->code=d->rows[d->target_row_index].numeric_code;
        m->criticality=d->rows[d->target_row_index].expected_criticality;
        m->name=asn1typed_render_cpp_final_name(m->source,ASN1TYPED_NAME_TYPE);
        m->body_name=identity_name(&d->target_body);
        if(!m->name || !m->body_name || !(m->ns=join(!strcmp(profile,"f1ap") ? "nrforge::f1ap::messages::" : "nrforge::ngap::messages::",m->name))) BAD("allocation failure planning final spelling");
        if(asn1typed_render_cpp_ioc_check_names(&m->body,m->ns,reserved,1,why,why_size)) return -1;
        for(j=0;j<i;++j) if(!strcmp(m->name,rows[j].name)
            || (m->role==rows[j].role && m->code==rows[j].code)) BAD("duplicate final spelling or dispatch key");
    }
    for(i=0;i<schema->row_count;++i) for(role=0;role<3;++role) if(schema->rows[i].payload_present[role]) {
        size_t matches=0;
        ++declared;
        for(j=0;j<count;++j) if(rows[j].role==role && rows[j].code==schema->rows[i].numeric_code
            && asn1typed_type_ref_equal(&rows[j].envelope.target_body,&schema->rows[i].payloads[role])) ++matches;
        if(matches!=1) BAD("missing or duplicate declared procedure payload slot");
    }
    if(declared!=count) BAD("selected message count differs from declared payload count");
    return 0;
#undef BAD
}
static int public_header(const char *dir,const struct message *m) {
    char relative[256]; FILE *f;
    int n=snprintf(relative,sizeof(relative),"messages/%s.hpp",m->name);
    if(n<0 || (size_t)n>=sizeof(relative) || !(f=new_file(dir,relative))) return -1;
    fprintf(f,"#ifndef NRFORGE_%s_MESSAGE_%s_HPP\n#define NRFORGE_%s_MESSAGE_%s_HPP\n#include <runtime.hpp>\n#include <sequence_extensions.hpp>\n#include \"%s_types.hpp\"\nnamespace %s { using Body = ::%s::%s; }\n#endif\n",profile_upper,m->name,profile_upper,m->name,m->name,m->ns,m->ns,m->body_name);
    return finish_file(f);
}
static int adapter(const char *dir,const struct message *m,size_t ordinal) {
    char relative[256]; FILE *f;
    int n=snprintf(relative,sizeof(relative),"adapters/%s.cpp",m->name);
    if(n<0 || (size_t)n>=sizeof(relative) || !(f=new_file(dir,relative))) return -1;
    fprintf(f,"#include \"messages/%s.hpp\"\n#include \"messages/%s_mapping.hpp\"\n#include \"messages/%s_codec.hpp\"\n#include <%s>\nnamespace {\nusing Body = ::%s::Body;\nusing Model = ::%s::detail::Model<Body>;\n",m->name,m->name,m->name,runtime_header,m->ns,profile_namespace);
    fprintf(f,"::nrforge::aper::Result<::std::unique_ptr<::%s::detail::Body>> decode(::nrforge::aper::FieldReader& f) {\nusing Result = ::nrforge::aper::Result<::std::unique_ptr<::%s::detail::Body>>;\nauto value = ::%s::compound_codec::get_%s(f);\nif(!value) return Result::failure(value.error());\ntry { ::std::unique_ptr<::%s::detail::Body> owned = ::std::make_unique<Model>(::std::move(value).value()); return Result::success(::std::move(owned)); }\ncatch(const ::std::bad_alloc&) { auto failure = f.record_failure({::nrforge::aper::ErrorCode::allocation_failure,f.cursor_bit()}); return Result::failure(failure.error()); }\ncatch(const ::std::length_error&) { auto failure = f.record_failure({::nrforge::aper::ErrorCode::resource_limit,f.cursor_bit()}); return Result::failure(failure.error()); }\n}\n",profile_namespace,profile_namespace,m->ns,m->body_name,profile_namespace);
    fprintf(f,"::nrforge::aper::Result<void> encode(::nrforge::aper::FieldWriter& f,const ::%s::detail::Body& v) {\nconst auto* model = dynamic_cast<const Model*>(&v);\nif(!model) return f.record_failure({::nrforge::aper::ErrorCode::invalid_argument,f.cursor_bit()});\nreturn ::%s::compound_codec::put_%s(f,model->value);\n}\n} // namespace\nnamespace %s::generated {\nRegistration registration_%03zu() noexcept { return {static_cast<Role>(%u),UINT64_C(%jd),static_cast<Criticality>(%u),\"%s\",\"%s\",&typeid(Body),&typeid(Model),&decode,&encode}; }\n}\n",profile_namespace,m->ns,m->body_name,profile_namespace,ordinal,m->role,m->code,m->criticality,m->envelope.target_body.module,m->source);
    return finish_file(f);
}
static int registry(const char *dir,const struct message *rows,size_t count) {
    const asn1typed_target_envelope_t *d=&rows[0].envelope;
    unsigned mapping[3]={0};
    FILE *f=new_file(dir,"registry.cpp");
    size_t i;
    if(!f) return -1;
    for(i=0;i<d->root_count;++i) if((unsigned)d->roots[i].role<3) mapping[d->roots[i].role]=(unsigned)d->roots[i].per_root_index;
    fprintf(f,"#include <%s>\nnamespace %s::generated {\n",runtime_header,profile_namespace);
    for(i=0;i<count;++i) fprintf(f,"Registration registration_%03zu() noexcept;\n",i);
    fprintf(f,"}\nnamespace %s {\nconst aper::Result<Registry>& %s_registry_state() noexcept {\nstatic const auto state = []() noexcept {\nstatic const ::std::array<ProcedureInfo,%zu> procedures{{\n",profile_namespace,profile,d->row_count);
    for(i=0;i<d->row_count;++i) fprintf(f,"{UINT64_C(%jd),static_cast<Criticality>(%u),{{%s,%s,%s}}},\n",d->rows[i].numeric_code,d->rows[i].expected_criticality,d->rows[i].payload_present[0]?"true":"false",d->rows[i].payload_present[1]?"true":"false",d->rows[i].payload_present[2]?"true":"false");
    fprintf(f,"}};\nconst ::std::array<Registration,%zu> messages{{\n",count);
    for(i=0;i<count;++i) fprintf(f,"generated::registration_%03zu(),\n",i);
    fprintf(f,"}};\nreturn Registry::create({{%u,%u,%u}},%s,procedures,messages",mapping[0],mapping[1],mapping[2],d->header.object_set_is_extensible?"true":"false");
    if(!strcmp(profile,"f1ap")) fprintf(f,",{%zu,%s}",d->root_count,d->header.choice_is_extensible?"true":"false");
    fprintf(f,");\n}(); return state;\n}\n}\n");
    return finish_file(f);
}
static int build_file(const char *dir,const struct message *rows,size_t count) {
    FILE *f=new_file(dir,"CMakeLists.txt");
    size_t i;
    if(!f) return -1;
    if(!strcmp(profile,"f1ap")) {
        fprintf(f,"cmake_minimum_required(VERSION 3.20)\nproject(nrforge_f1ap_dispatch LANGUAGES CXX)\nset(NRFORGE_SOURCE_ROOT \"\" CACHE PATH \"NRForge-asn1c source directory\")\nif(NOT EXISTS \"${NRFORGE_SOURCE_ROOT}/libngap/f1ap_pdu.cpp\")\n  message(FATAL_ERROR \"Set NRFORGE_SOURCE_ROOT to NRForge-asn1c\")\nendif()\nadd_library(nrforge_f1ap STATIC\n  \"${NRFORGE_SOURCE_ROOT}/libngap/f1ap_pdu.cpp\"\n  \"${NRFORGE_SOURCE_ROOT}/libaper/runtime.cpp\"\n  registry.cpp\n");
        for(i=0;i<count;++i) fprintf(f,"  adapters/%s.cpp\n",rows[i].name);
        fprintf(f,")\nif(EXISTS \"${CMAKE_CURRENT_SOURCE_DIR}/sdk-lock.cmake\")\n  include(\"${NRFORGE_SOURCE_ROOT}/cmake/NrforgeNgapSdk.cmake\")\n  nrforge_verify_sdk_lock()\n  target_sources(nrforge_f1ap PRIVATE sdk_identity.cpp)\n  nrforge_package_sdk(nrforge_f1ap f1ap)\nelse()\ntarget_compile_features(nrforge_f1ap PUBLIC cxx_std_20)\ntarget_include_directories(nrforge_f1ap PUBLIC \"${CMAKE_CURRENT_SOURCE_DIR}\" \"${NRFORGE_SOURCE_ROOT}/libngap\" \"${NRFORGE_SOURCE_ROOT}/libaper\")\nendif()\n");
        return finish_file(f);
    }
    fprintf(f,"cmake_minimum_required(VERSION 3.20)\nproject(nrforge_ngap VERSION 0.1.0 LANGUAGES CXX)\nset(NRFORGE_SOURCE_ROOT \"\" CACHE PATH \"NRForge-asn1c source directory\")\nif(NOT EXISTS \"${NRFORGE_SOURCE_ROOT}/cmake/NrforgeNgapSdk.cmake\")\n  message(FATAL_ERROR \"Set NRFORGE_SOURCE_ROOT to NRForge-asn1c\")\nendif()\ninclude(\"${NRFORGE_SOURCE_ROOT}/cmake/NrforgeNgapSdk.cmake\")\nnrforge_verify_sdk_lock()\nadd_library(nrforge_ngap STATIC\n  \"${NRFORGE_SOURCE_ROOT}/libngap/pdu.cpp\"\n  \"${NRFORGE_SOURCE_ROOT}/libaper/runtime.cpp\"\n  registry.cpp sdk_identity.cpp\n");
    for(i=0;i<count;++i) fprintf(f,"  adapters/%s.cpp\n",rows[i].name);
    fprintf(f,")\nnrforge_package_sdk(nrforge_ngap)\n");
    return finish_file(f);
}
static int manifest(const char *dir,const struct message *rows,size_t count,const char *const *inputs) {
    const asn1typed_target_envelope_t *d=&rows[0].envelope;
    unsigned mapping[3]={0};
    FILE *f=new_file(dir,"manifest.json");
    size_t i;
    if(!f) return -1;
    for(i=0;i<d->root_count;++i) if((unsigned)d->roots[i].role<3) mapping[d->roots[i].role]=(unsigned)d->roots[i].per_root_index;
    fprintf(f,"{\"parse\":\"PASS\",\"fix\":\"PASS\",\"parser_deleted\":true,\"deterministic\":true,\"message_count\":%zu,\"registry\":{\"role_to_per_index\":[%u,%u,%u],\"object_set_extensible\":%s},\"schema\":{\"pdu_module\":",count,mapping[0],mapping[1],mapping[2],d->header.object_set_is_extensible?"true":"false");
    json_text(f,d->header.pdu.module); fputs(",\"pdu_type\":",f); json_text(f,d->header.pdu.source_name);
    if(!strcmp(profile,"f1ap")) fprintf(f,",\"profile\":\"f1ap\",\"root_count\":%zu,\"choice_is_extensible\":%s,\"unsupported_root_policy\":\"reject_before_header\"",d->root_count,d->header.choice_is_extensible?"true":"false");
    fputs(",\"procedure_class_module\":",f); json_text(f,d->header.procedure_class.module);
    fputs(",\"procedure_class\":",f); json_text(f,d->header.procedure_class.source_name);
    fputs(",\"object_set_module\":",f); json_text(f,d->header.object_set.module);
    fputs(",\"object_set\":",f); json_text(f,d->header.object_set.source_name);
    fprintf(f,",\"procedures\":[");
    for(i=0;i<d->row_count;++i) fprintf(f,"%s{\"code\":%jd,\"criticality\":%u,\"payload_present\":[%s,%s,%s]}",i?",":"",d->rows[i].numeric_code,d->rows[i].expected_criticality,d->rows[i].payload_present[0]?"true":"false",d->rows[i].payload_present[1]?"true":"false",d->rows[i].payload_present[2]?"true":"false");
    fputs("]},\"source_inputs\":{\"module_list\":",f); json_text(f,inputs[1]);
    fputs(",\"asn1_root\":",f); json_text(f,inputs[2]);
    fputs(",\"message_list\":",f); json_text(f,inputs[3]);
    fputs("},\"messages\":[\n",f);
    for(i=0;i<count;++i) {
        const struct message *m=&rows[i];
        fprintf(f,"%s{\"message\":",i?",\n":""); json_text(f,m->source);
        fputs(",\"module\":",f); json_text(f,m->envelope.target_body.module);
        fputs(",\"namespace\":",f); json_text(f,m->ns);
        fprintf(f,",\"cpp_body_type\":\"%s\",\"public_header\":\"messages/%s.hpp\",\"types_header\":\"messages/%s_types.hpp\",\"mapping_header\":\"messages/%s_mapping.hpp\",\"codec_header\":\"messages/%s_codec.hpp\",\"adapter\":\"adapters/%s.cpp\",\"role\":%u,\"code\":%jd,\"criticality\":%u,\"deterministic\":true}",m->body_name,m->name,m->name,m->name,m->name,m->name,m->role,m->code,m->criticality);
    }
    fputs("\n]}\n",f);
    if(finish_file(f)) {
        char path[4096];
        /* Never leave success-shaped evidence after a failed flush/close. */
        if(!path_name(path,dir,"manifest.json")) (void)remove(path);
        return -1;
    }
    return 0;
}
int main(int argc,char **argv) {
    struct message *rows=NULL;
    dev_tree_options_t options;
    asn1p_t *tree=NULL;
    const char *failed=NULL;
    char why[1024]="invalid, duplicate or truncated message list",path[4096],relative[256];
    size_t count=0,i=0,family;
    int result=1;
    int (*renderers[])(const asn1typed_module_t*,const char*,char**,char*,size_t)={
        asn1typed_render_cpp_owned_ioc_types,asn1typed_render_cpp_owned_ioc_mapping,asn1typed_render_cpp_owned_ioc_codec};
    const char *suffixes[]={"types","mapping","codec"};
    if(argc==6 && !strcmp(argv[5],"--f1ap")) {
        profile="f1ap"; profile_upper="F1AP"; profile_namespace="nrforge::f1ap"; runtime_header="f1ap_pdu.hpp";
    } else if(argc!=5) { fprintf(stderr,"Usage: generate MODULE_LIST ASN1_ROOT MESSAGE_LIST NEW_OUTPUT_DIR [--f1ap]\n"); return 2; }
    rows=calloc(MAX_MESSAGES,sizeof(*rows));
    if(!rows || read_list(argv[3],rows,&count)) goto done;
    if(!empty_directory(argv[4])) { snprintf(why,sizeof(why),"output directory must exist and be empty"); goto done; }
    options.module_list=argv[1]; options.asn1_root=argv[2];
    tree=dev_tree_load(&options,&failed);
    if(!tree) { snprintf(why,sizeof(why),"parse failed: %s",failed?failed:"unknown"); goto done; }
    if(dev_tree_fix(tree)<0) { snprintf(why,sizeof(why),"fix failed"); goto done; }
    for(i=0;i<count;++i) if(asn1typed_extract_physical_message(tree,!strcmp(profile,"f1ap")?"F1AP-PDU-Contents":"NGAP-PDU-Contents",rows[i].source,&rows[i].body,why,sizeof(why))
        || asn1typed_extract_target_envelope(tree,!strcmp(profile,"f1ap")?"F1AP-PDU-Descriptions":"NGAP-PDU-Descriptions",!strcmp(profile,"f1ap")?"F1AP-PDU":"NGAP-PDU",!strcmp(profile,"f1ap")?"F1AP-PDU-Contents":"NGAP-PDU-Contents",rows[i].source,&rows[i].envelope,why,sizeof(why))) goto done;
    snprintf(why,sizeof(why),"incomplete/duplicate registry, inconsistent owned schema, body identity or final spelling collision");
    if(preflight(rows,count,why,sizeof(why))) goto done;
    asn1p_delete(tree); tree=NULL;
    if(path_name(path,argv[4],"messages") || mkdir(path,0700)
        || path_name(path,argv[4],"adapters") || mkdir(path,0700)) { snprintf(why,sizeof(why),"output subdirectory creation failed"); goto done; }
    for(i=0;i<count;++i) {
        for(family=0;family<3;++family) {
            char *first=NULL,*second=NULL;
            int a=renderers[family](&rows[i].body,rows[i].ns,&first,why,sizeof(why));
            int b=renderers[family](&rows[i].body,rows[i].ns,&second,why,sizeof(why));
            int n=snprintf(relative,sizeof(relative),"messages/%s_%s.hpp",rows[i].name,suffixes[family]);
            int valid=!a && !b && first && second && !strcmp(first,second) && n>=0 && (size_t)n<sizeof(relative)
                && !save(argv[4],relative,first);
            free(first); free(second); if(!valid) goto done;
        }
        if(public_header(argv[4],&rows[i]) || adapter(argv[4],&rows[i],i)) { snprintf(why,sizeof(why),"adapter/public-header output failed"); goto done; }
    }
    if(registry(argv[4],rows,count) || save(argv[4],!strcmp(profile,"f1ap")?"f1ap.hpp":"ngap.hpp",!strcmp(profile,"f1ap")?"#ifndef NRFORGE_GENERATED_F1AP_HPP\n#define NRFORGE_GENERATED_F1AP_HPP\n#include <f1ap_pdu.hpp>\n#endif\n":"#ifndef NRFORGE_GENERATED_NGAP_HPP\n#define NRFORGE_GENERATED_NGAP_HPP\n#include <pdu.hpp>\n#include <sdk_version.hpp>\n#endif\n") || build_file(argv[4],rows,count)) { snprintf(why,sizeof(why),"registry/build output failed"); goto done; }
    /* Success evidence is deliberately last; partial generation never has it. */
    if(manifest(argv[4],rows,count,(const char *const *)argv)) { snprintf(why,sizeof(why),"manifest output failed"); goto done; }
    printf("Generated complete typed dispatch: %zu owned declared message slots\n",count); result=0;
done:
    if(result) fprintf(stderr,"generation failed at input %zu: %s\n",i,why);
    if(tree) asn1p_delete(tree);
    if(rows) for(i=0;i<count;++i) { free(rows[i].name); free(rows[i].body_name); free(rows[i].ns); asn1typed_module_clear(&rows[i].body); asn1typed_target_envelope_clear(&rows[i].envelope); }
    free(rows); return result;
}
