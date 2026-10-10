#include "asn1typed_render_cpp.h"
#include "asn1typed_render_cpp_internal.h"
#include "asn1typed_render_cpp_ioc_internal.h"
#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct envelope_names {
    char *pdu, *mapping, *body, *criticality, *criticality_mapping, *opaque, *target, *extension;
    char *encode, *decode, *put, *get, *crit_put, *crit_get;
    char *roots[3], *fields[3][3];
};
static char *join(const char *a, const char *b) {
    size_t x = strlen(a), y = strlen(b);
    char *p;
    if(x > SIZE_MAX - y - 1) return NULL;
    p = malloc(x + y + 1);
    if(p) { memcpy(p, a, x); memcpy(p + x, b, y + 1); }
    return p;
}
static char *identity_name(const asn1typed_type_ref_t *ref, asn1typed_name_style_e style) {
    char *component = join(ref->module, "-"), *raw, *name;
    if(!component) return NULL;
    raw = join(component, ref->source_name); free(component);
    if(!raw) return NULL;
    name = asn1typed_render_cpp_final_name(raw, style); free(raw); return name;
}
static void clear_names(struct envelope_names *p) {
    size_t i, j;
    free(p->pdu); free(p->mapping); free(p->body); free(p->criticality); free(p->criticality_mapping);
    free(p->opaque); free(p->target); free(p->extension); free(p->encode); free(p->decode);
    free(p->put); free(p->get); free(p->crit_put); free(p->crit_get);
    for(i = 0; i < 3; ++i) { free(p->roots[i]); for(j = 0; j < 3; ++j) free(p->fields[i][j]); }
    memset(p, 0, sizeof(*p));
}
static int names(const asn1typed_module_t *body, const asn1typed_target_envelope_t *d, const char *ns,
        struct envelope_names *p, char *why, size_t size) {
    const char *symbols[24];
    char *base = NULL, *suffix = NULL;
    size_t count = 0, i, j;
#define NEED(v) do { if(!(v)) goto fail; } while(0)
#define SYMBOL(v) do { NEED(v); symbols[count++] = (v); } while(0)
    if(asn1typed_target_envelope_validate(d, why, size)) return -1;
    if(!body || !body->types || !body->type_count || body->type_count > body->type_capacity
        || !body->types[0].identity.module || !body->types[0].identity.source_name
        || strcmp(body->types[0].identity.module, d->target_body.module)
        || strcmp(body->types[0].identity.source_name, d->target_body.source_name)) {
        snprintf(why, size, "envelope target body graph identity mismatch"); return -1;
    }
    p->pdu = identity_name(&d->header.pdu, ASN1TYPED_NAME_TYPE); SYMBOL(p->pdu);
    p->body = identity_name(&d->target_body, ASN1TYPED_NAME_TYPE); NEED(p->body);
    p->mapping = join(p->pdu, "_aper"); SYMBOL(p->mapping);
    p->criticality = join(p->pdu, "_criticality"); SYMBOL(p->criticality);
    p->criticality_mapping = join(p->criticality, "_aper"); SYMBOL(p->criticality_mapping);
    p->opaque = join(p->pdu, "_opaque"); SYMBOL(p->opaque);
    p->target = join(p->pdu, "_target_body"); SYMBOL(p->target);
    p->extension = join(p->pdu, "_unknown_extension"); SYMBOL(p->extension);
    base = identity_name(&d->header.pdu, ASN1TYPED_NAME_FIELD); NEED(base);
    p->encode = join("encode_", base); SYMBOL(p->encode);
    p->decode = join("decode_", base); SYMBOL(p->decode);
    p->put = join("put_", p->pdu); SYMBOL(p->put);
    p->get = join("get_", p->pdu); SYMBOL(p->get);
    p->crit_put = join("put_", p->criticality); SYMBOL(p->crit_put);
    p->crit_get = join("get_", p->criticality); SYMBOL(p->crit_get);
    symbols[count++] = "envelope_codec";
    for(i = 0; i < 3; ++i) {
        suffix = asn1typed_render_cpp_final_name(d->roots[i].source_name, ASN1TYPED_NAME_FIELD); NEED(suffix);
        p->roots[i] = join(p->pdu, "_"); NEED(p->roots[i]);
        { char *old = p->roots[i]; p->roots[i] = join(old, suffix); free(old); }
        free(suffix); suffix = NULL; SYMBOL(p->roots[i]);
        for(j = 0; j < 3; ++j) {
            p->fields[i][j] = asn1typed_render_cpp_final_name(d->roots[i].field_names[j], ASN1TYPED_NAME_FIELD); NEED(p->fields[i][j]);
            if(asn1typed_render_cpp_header_macro(p->fields[i][j]) || !strcmp(p->fields[i][j], p->roots[i])) goto fail;
            for(size_t k = 0; k < j; ++k) if(!strcmp(p->fields[i][j], p->fields[i][k])) goto fail;
        }
    }
    if(asn1typed_render_cpp_ioc_check_names(body, ns, symbols, count, why, size)) { free(base); return -1; }
    free(base); return 0;
fail:
    free(base); free(suffix); snprintf(why, size, "envelope unavailable final spelling, member collision/macro, or allocation failure"); return -1;
#undef NEED
#undef SYMBOL
}
#if defined(__GNUC__) || defined(__clang__)
static int emit(struct compound_buf *b, const char *fmt, ...) __attribute__((format(printf,2,3)));
#endif
static int emit(struct compound_buf *b, const char *fmt, ...) {
    va_list ap;
    int n;
    char *p;
    va_start(ap, fmt); n = vsnprintf(NULL, 0, fmt, ap); va_end(ap);
    if(n < 0 || (size_t)n > SIZE_MAX - b->length - 1) return -1;
    p = realloc(b->text, b->length + (size_t)n + 1); if(!p) return -1;
    b->text = p;
    va_start(ap, fmt); vsnprintf(p + b->length, (size_t)n + 1, fmt, ap); va_end(ap);
    b->length += (size_t)n; return 0;
}
static int types(struct compound_buf *b, const struct envelope_names *p, const asn1typed_target_envelope_t *d, const char *ns) {
    size_t i;
    if(emit(b,"#include <cstddef>\n#include <cstdint>\n#include <vector>\n#include <variant>\nnamespace %s {\nstruct %s { enum class Known : ::std::int64_t {", ns, p->criticality)) return -1;
    for(i = 0; i < 3; ++i) if(emit(b,"%s%s = %jd", i ? ", " : "", d->header.criticalities[i].source_name, d->header.criticalities[i].assigned_number)) return -1;
    if(emit(b,"}; Known value{}; };\nstruct %s { ::std::vector<::std::byte> payload{}; };\nstruct %s { ::%s::%s value{}; };\nstruct %s { ::std::uint64_t index{}; ::std::vector<::std::byte> payload{}; };\n",p->opaque,p->target,ns,p->body,p->extension)) return -1;
    for(i = 0; i < 3; ++i) {
        if(emit(b,"struct %s { ::std::uint64_t %s{}; ::%s::%s %s{}; ",p->roots[i],p->fields[i][0],ns,p->criticality,p->fields[i][1])) return -1;
        if(i == d->target_root_ordinal) {
            if(emit(b,"::std::variant<::%s::%s, ::%s::%s> %s{}; };\n",ns,p->opaque,ns,p->target,p->fields[i][2])) return -1;
        } else if(emit(b,"::%s::%s %s{}; };\n",ns,p->opaque,p->fields[i][2])) return -1;
    }
    return emit(b,"struct %s { ::std::variant<::%s::%s, ::%s::%s, ::%s::%s, ::%s::%s> value{}; };\n} // namespace\n",p->pdu,ns,p->roots[0],ns,p->roots[1],ns,p->roots[2],ns,p->extension);
}
static int mapping(struct compound_buf *b, const struct envelope_names *p, const asn1typed_target_envelope_t *d, const char *ns) {
    size_t i, j;
    unsigned procedure_bits = 0;
    uintmax_t range = (uintmax_t)(d->header.procedure_upper_bound - d->header.procedure_lower_bound);
    while(range) { ++procedure_bits; range >>= 1; }
    if(emit(b,"#include <array>\n#include <cstddef>\n#include <cstdint>\nnamespace %s {\nstruct %s { using value_type = ::%s::%s; static constexpr unsigned root_count = 3; static constexpr bool extensible = false; struct Entry { ::std::int64_t assigned_number; unsigned per_index; }; static constexpr ::std::array<Entry, 3> entries{{",ns,p->criticality_mapping,ns,p->criticality)) return -1;
    for(i=0;i<3;++i) if(emit(b,"%s{%jd, %zu}",i?", ":"",d->header.criticalities[i].assigned_number,d->header.criticalities[i].per_index)) return -1;
    if(emit(b,"}}; };\nstruct %s {\nusing value_type = ::%s::%s;\nusing body_type = ::%s::%s;\nusing body_mapping = ::%s::%s_aper;\nusing criticality_type = ::%s::%s;\nusing criticality_mapping = ::%s::%s;\nusing target_wrapper_type = ::%s::%s;\nusing opaque_type = ::%s::%s;\nusing unknown_extension_type = ::%s::%s;\nstatic constexpr ::std::size_t root_count = %zu;\nstatic constexpr bool extensible = %s;\nstatic constexpr bool object_set_extensible = %s;\nstatic constexpr unsigned procedure_root_bits = %u;\nstatic constexpr ::std::uint64_t procedure_lower_bound = %jd, procedure_upper_bound = %jd;\nstatic constexpr ::std::uint64_t target_code = %jd;\nstatic constexpr unsigned expected_criticality = %u;\nstatic constexpr ::std::size_t target_root_ordinal = %zu;\n",p->mapping,ns,p->pdu,ns,p->body,ns,p->body,ns,p->criticality,ns,p->criticality_mapping,ns,p->target,ns,p->opaque,ns,p->extension,d->root_count,d->header.choice_is_extensible?"true":"false",d->header.object_set_is_extensible?"true":"false",procedure_bits,d->header.procedure_lower_bound,d->header.procedure_upper_bound,d->rows[d->target_row_index].numeric_code,d->rows[d->target_row_index].expected_criticality,d->target_root_ordinal)) return -1;
    if(emit(b,"static constexpr ::std::array<::std::size_t, 3> source_ordinal_to_per_root_index{{%zu, %zu, %zu}};\nstatic constexpr ::std::array<::std::size_t, 3> per_root_index_to_source_ordinal{{",d->roots[0].per_root_index,d->roots[1].per_root_index,d->roots[2].per_root_index)) return -1;
    for(i=0;i<3;++i) { for(j=0;j<3;++j) if(d->roots[j].per_root_index==i) break; if(emit(b,"%s%zu",i?", ":"",j)) return -1; }
    if(emit(b,"}};\nstatic constexpr ::std::array<unsigned,3> root_roles{{%u,%u,%u}};\n",(unsigned)d->roots[0].role,(unsigned)d->roots[1].role,(unsigned)d->roots[2].role)) return -1;
    for(i=0;i<3;++i) if(emit(b,"using wrapper_%zu = ::%s::%s;\nstatic constexpr auto root_%zu_procedure_member = &wrapper_%zu::%s;\nstatic constexpr auto root_%zu_criticality_member = &wrapper_%zu::%s;\nstatic constexpr auto root_%zu_value_member = &wrapper_%zu::%s;\n",i,ns,p->roots[i],i,i,p->fields[i][0],i,i,p->fields[i][1],i,i,p->fields[i][2])) return -1;
    if(emit(b,"struct Row { ::std::uint64_t code; unsigned expected_criticality; unsigned default_provenance; ::std::array<bool,3> payload_present; };\nstatic constexpr ::std::array<Row,%zu> rows{{",d->row_count)) return -1;
    for(i=0;i<d->row_count;++i) if(emit(b,"%s{%jd,%u,%u,{{%s,%s,%s}}}",i?", ":"",d->rows[i].numeric_code,d->rows[i].expected_criticality,(unsigned)d->rows[i].default_provenance,d->rows[i].payload_present[0]?"true":"false",d->rows[i].payload_present[1]?"true":"false",d->rows[i].payload_present[2]?"true":"false")) return -1;
    return emit(b,"}};\nstatic constexpr bool has_procedure_code(::std::uint64_t code) { for(const auto& row : rows) if(row.code == code) return true; return false; }\n};\n} // namespace\n");
}
static int codec(struct compound_buf *b, const struct envelope_names *p, const asn1typed_target_envelope_t *d, const char *ns) {
    size_t i, target = d->target_root_ordinal;
    if(emit(b,"#include <new>\n#include <stdexcept>\n#include <utility>\n#include <span>\nnamespace %s { namespace envelope_codec {\n",ns)) return -1;
    if(emit(b,"inline ::nrforge::aper::Result<void> %s(::nrforge::aper::FieldWriter& f, const ::%s::%s& v) {\nfor(const auto& entry : ::%s::%s::entries) if(static_cast<::std::int64_t>(v.value) == entry.assigned_number) return f.write_enumerated({false,entry.per_index}, ::%s::%s::root_count, false);\nreturn f.record_failure({::nrforge::aper::ErrorCode::constraint_violation,f.cursor_bit()});\n}\ninline ::nrforge::aper::Result<::%s::%s> %s(::nrforge::aper::FieldReader& f) {\nauto index = f.read_enumerated(::%s::%s::root_count,false);\nif(!index) return ::nrforge::aper::Result<::%s::%s>::failure(index.error());\nfor(const auto& entry : ::%s::%s::entries) if(entry.per_index == index.value().index) return ::nrforge::aper::Result<::%s::%s>::success({static_cast<::%s::%s::Known>(entry.assigned_number)});\nauto failure = f.record_failure({::nrforge::aper::ErrorCode::constraint_violation,f.cursor_bit()});\nreturn ::nrforge::aper::Result<::%s::%s>::failure(failure.error());\n}\n",p->crit_put,ns,p->criticality,ns,p->criticality_mapping,ns,p->criticality_mapping,ns,p->criticality,p->crit_get,ns,p->criticality_mapping,ns,p->criticality,ns,p->criticality_mapping,ns,p->criticality,ns,p->criticality,ns,p->criticality)) return -1;
    if(emit(b,"inline ::nrforge::aper::Result<void> %s(::nrforge::aper::FieldWriter& f, const ::%s::%s& v) {\nconst auto* root = ::std::get_if<::%s::%s>(&v.value);\nif(!root || root->%s.valueless_by_exception() || !::std::holds_alternative<::%s::%s>(root->%s) || root->%s != ::%s::%s::target_code) return f.record_failure({::nrforge::aper::ErrorCode::constraint_violation,f.cursor_bit()});\nauto selector = f.write_enumerated({false,::%s::%s::source_ordinal_to_per_root_index[::%s::%s::target_root_ordinal]},static_cast<unsigned>(::%s::%s::root_count),::%s::%s::extensible); if(!selector) return selector;\nauto code = f.write_constrained_uint(root->%s,::%s::%s::procedure_root_bits); if(!code) return code;\nauto criticality = ::%s::envelope_codec::%s(f,root->%s); if(!criticality) return criticality;\nreturn f.write_known_open_type([&](::nrforge::aper::FieldWriter& child) { return ::%s::compound_codec::put_%s(child,::std::get<::%s::%s>(root->%s).value); });\n}\n",p->put,ns,p->pdu,ns,p->roots[target],p->fields[target][2],ns,p->target,p->fields[target][2],p->fields[target][0],ns,p->mapping,ns,p->mapping,ns,p->mapping,ns,p->mapping,ns,p->mapping,p->fields[target][0],ns,p->mapping,ns,p->crit_put,p->fields[target][1],ns,p->body,ns,p->target,p->fields[target][2])) return -1;
    if(emit(b,"inline ::nrforge::aper::Result<::%s::%s> %s(::nrforge::aper::FieldReader& f) {\ntry {\nauto selector = f.read_enumerated(static_cast<unsigned>(::%s::%s::root_count),::%s::%s::extensible);\nif(!selector) return ::nrforge::aper::Result<::%s::%s>::failure(selector.error());\nif(selector.value().is_extension) {\nauto payload = f.read_open_type_owned(); if(!payload) return ::nrforge::aper::Result<::%s::%s>::failure(payload.error());\nreturn ::nrforge::aper::Result<::%s::%s>::success({::%s::%s{selector.value().index,::std::move(payload).value()}});\n}\nconst auto ordinal = ::%s::%s::per_root_index_to_source_ordinal[static_cast<::std::size_t>(selector.value().index)];\n",ns,p->pdu,p->get,ns,p->mapping,ns,p->mapping,ns,p->pdu,ns,p->pdu,ns,p->pdu,ns,p->extension,ns,p->mapping)) return -1;
    for(i=0;i<3;++i) {
        if(emit(b,"if(ordinal == %zu) {\n::%s::%s root{};\nauto code = f.read_constrained_uint(::%s::%s::procedure_root_bits); if(!code) return ::nrforge::aper::Result<::%s::%s>::failure(code.error()); root.%s = code.value();\nif(!::%s::%s::object_set_extensible && !::%s::%s::has_procedure_code(code.value())) { auto failure = f.record_failure({::nrforge::aper::ErrorCode::constraint_violation,f.cursor_bit()}); return ::nrforge::aper::Result<::%s::%s>::failure(failure.error()); }\nauto criticality = ::%s::envelope_codec::%s(f); if(!criticality) return ::nrforge::aper::Result<::%s::%s>::failure(criticality.error()); root.%s = ::std::move(criticality).value();\n",i,ns,p->roots[i],ns,p->mapping,ns,p->pdu,p->fields[i][0],ns,p->mapping,ns,p->mapping,ns,p->pdu,ns,p->crit_get,ns,p->pdu,p->fields[i][1])) return -1;
        if(i == target && emit(b,"if(code.value() == ::%s::%s::target_code) {\nauto payload = f.read_known_open_type<::%s::%s>([](::nrforge::aper::FieldReader& child) { return ::%s::compound_codec::get_%s(child); });\nif(!payload) return ::nrforge::aper::Result<::%s::%s>::failure(payload.error());\nroot.%s = ::%s::%s{::std::move(payload).value()};\n} else {\n",ns,p->mapping,ns,p->body,ns,p->body,ns,p->pdu,p->fields[i][2],ns,p->target)) return -1;
        if(emit(b,"auto payload = f.read_open_type_owned(); if(!payload) return ::nrforge::aper::Result<::%s::%s>::failure(payload.error());\nroot.%s = ::%s::%s{::std::move(payload).value()};\n",ns,p->pdu,p->fields[i][2],ns,p->opaque)) return -1;
        if(i==target && emit(b,"}\n")) return -1;
        if(emit(b,"return ::nrforge::aper::Result<::%s::%s>::success({::std::move(root)});\n}\n",ns,p->pdu)) return -1;
    }
    if(emit(b,"auto failure = f.record_failure({::nrforge::aper::ErrorCode::constraint_violation,f.cursor_bit()}); return ::nrforge::aper::Result<::%s::%s>::failure(failure.error());\n} catch(const ::std::bad_alloc&) { auto failure = f.record_failure({::nrforge::aper::ErrorCode::allocation_failure,f.cursor_bit()}); return ::nrforge::aper::Result<::%s::%s>::failure(failure.error()); } catch(const ::std::length_error&) { auto failure = f.record_failure({::nrforge::aper::ErrorCode::resource_limit,f.cursor_bit()}); return ::nrforge::aper::Result<::%s::%s>::failure(failure.error()); }\n}\n} // namespace envelope_codec\n",ns,p->pdu,ns,p->pdu,ns,p->pdu)) return -1;
    return emit(b,"inline ::nrforge::aper::Result<::nrforge::aper::CompleteEncoding> %s(const ::%s::%s& v, const ::nrforge::aper::Limits& limits = {}) { return ::nrforge::aper::encode_complete(v,limits,[&](::nrforge::aper::FieldWriter& f) { return ::%s::envelope_codec::%s(f,v); }); }\ninline ::nrforge::aper::Result<::%s::%s> %s(::std::span<const ::std::byte> input, const ::nrforge::aper::Limits& limits = {}) { return ::nrforge::aper::decode_complete<::%s::%s>(input,limits,[](::nrforge::aper::FieldReader& f) { return ::%s::envelope_codec::%s(f); }); }\n} // namespace\n",p->encode,ns,p->pdu,ns,p->put,ns,p->pdu,p->decode,ns,p->pdu,ns,p->get);
}
static int render(const asn1typed_module_t *body, const asn1typed_target_envelope_t *d, const char *ns, char **out, char *diagnostic, size_t size, int mode) {
    struct envelope_names p = {0}; struct compound_buf b = {0};
    char why[512] = "invalid envelope renderer arguments";
    int result = -1;
    if(out) *out = NULL;
    if(diagnostic && size) diagnostic[0] = 0;
    if(!out || !d || !ns || names(body,d,ns,&p,why,sizeof(why))) goto done;
    if((mode==0 ? types(&b,&p,d,ns) : mode==1 ? mapping(&b,&p,d,ns) : codec(&b,&p,d,ns))) { snprintf(why,sizeof(why),"allocation failure rendering envelope"); goto done; }
    *out = b.text; b.text = NULL; result = 0;
done:
    free(b.text); clear_names(&p);
    if(result && diagnostic && size) snprintf(diagnostic,size,"%s",why);
    return result;
}
int asn1typed_render_cpp_target_envelope_types(const asn1typed_module_t *m,const asn1typed_target_envelope_t *d,const char *ns,char **out,char *why,size_t size) { return render(m,d,ns,out,why,size,0); }
int asn1typed_render_cpp_target_envelope_mapping(const asn1typed_module_t *m,const asn1typed_target_envelope_t *d,const char *ns,char **out,char *why,size_t size) { return render(m,d,ns,out,why,size,1); }
int asn1typed_render_cpp_target_envelope_codec(const asn1typed_module_t *m,const asn1typed_target_envelope_t *d,const char *ns,char **out,char *why,size_t size) { return render(m,d,ns,out,why,size,2); }
