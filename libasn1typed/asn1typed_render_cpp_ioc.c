#include "asn1typed_render_cpp.h"
#include "asn1typed_render_cpp_internal.h"
#include "asn1typed_render_cpp_ioc_internal.h"
#include "asn1typed_render_cpp_inline_enum_internal.h"
#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int emit(struct compound_buf *b, const char *fmt, ...) {
    va_list ap;
    int n;
    char *p;
    va_start(ap, fmt); n = vsnprintf(NULL, 0, fmt, ap); va_end(ap);
    if(n < 0 || (size_t)n > SIZE_MAX - b->length - 1) return -1;
    p = (char *)realloc(b->text, b->length + (size_t)n + 1);
    if(!p) return -1;
    b->text = p;
    va_start(ap, fmt); vsnprintf(p + b->length, (size_t)n + 1, fmt, ap); va_end(ap);
    b->length += (size_t)n; return 0;
}

int asn1typed_render_cpp_ioc_emit(struct compound_buf *b, const struct type_plan *p,
        const struct asn1typed_cpp_ioc_entry *entry, int mode) {
    const asn1typed_ioc_registry_t *r = entry->registry;
    size_t j;
    if(mode == 0) {
        if(emit(b, "struct %s { ::std::vector<::std::byte> payload{}; };\n", p->unknown_wrapper)) return -1;
        for(j = 0; j < r->row_count; ++j)
            if(emit(b, "struct %s { %s value{}; };\n", p->members[j + 2].wrapper, p->members[j + 2].type)) return -1;
        if(emit(b, "struct %s {\n    %s %s{};\n    %s %s{};\n    ::std::variant<%s", p->type, p->members[0].type, p->members[0].name, p->members[1].type, p->members[1].name, p->qualified_unknown_wrapper)) return -1;
        for(j = 0; j < r->row_count; ++j) if(emit(b, ", %s", p->members[j + 2].qualified_wrapper)) return -1;
        return emit(b, "> %s{};\n};\n", p->value_member);
    }
    if(mode == 1) {
        if(emit(b, "struct %s {\n    using value_type = %s;\n    static constexpr bool object_set_extensible = %s;\n    struct Row { ::std::uint64_t id; unsigned expected_criticality; bool has_presence; unsigned presence; };\n    static constexpr ::std::array<Row, %zu> rows{{", p->mapping, p->qualified_type, r->object_set_is_extensible ? "true" : "false", r->row_count)) return -1;
        for(j = 0; j < r->row_count; ++j) if(emit(b, "%s{%" PRIuMAX ", %u, %s, %u}", j ? ", " : "", (uintmax_t)r->rows[j].numeric_id, (unsigned)r->rows[j].criticality, r->rows[j].has_presence ? "true" : "false", (unsigned)r->rows[j].presence)) return -1;
        if(emit(b, "}};\n    using identifier_type = %s;\n    using identifier_mapping = %s;\n    using criticality_type = %s;\n    using criticality_mapping = %s;\n    using unknown_type = %s;\n", p->members[0].type, p->members[0].mapping, p->members[1].type, p->members[1].mapping, p->qualified_unknown_wrapper)) return -1;
        for(j = 0; j < r->row_count; ++j)
            if(emit(b, "    using wrapper_%zu = %s;\n    using payload_type_%zu = %s;\n    using payload_mapping_%zu = %s;\n", j, p->members[j + 2].qualified_wrapper, j, p->members[j + 2].type, j, p->members[j + 2].mapping)) return -1;
        return emit(b, "};\n");
    }
    if(emit(b, "namespace compound_codec {\ninline ::nrforge::aper::Result<void> %s(::nrforge::aper::FieldWriter& f, const %s& v) {\n", p->put, p->qualified_type)) return -1;
    /* Validate the representation before emitting any physical component. */
    if(emit(b, "    if(v.%s.valueless_by_exception() || ::std::holds_alternative<%s>(v.%s)) return f.record_failure({::nrforge::aper::ErrorCode::constraint_violation, f.cursor_bit()});\n", p->value_member, p->qualified_unknown_wrapper, p->value_member)) return -1;
    for(j = 0; j < r->row_count; ++j) {
        const struct member_plan *mp = &p->members[j + 2];
        if(emit(b, "    if(::std::holds_alternative<%s>(v.%s)) {\n        if(v.%s != %" PRIuMAX ") return f.record_failure({::nrforge::aper::ErrorCode::constraint_violation, f.cursor_bit()});\n        auto id = %s(f, v.%s); if(!id) return id;\n        auto criticality = %s(f, v.%s); if(!criticality) return criticality;\n        return f.write_known_open_type([&](::nrforge::aper::FieldWriter& child) { return %s(child, ::std::get<%s>(v.%s).value); });\n    }\n", mp->qualified_wrapper, p->value_member, p->members[0].name, (uintmax_t)r->rows[j].numeric_id, p->members[0].put, p->members[0].name, p->members[1].put, p->members[1].name, mp->put, mp->qualified_wrapper, p->value_member)) return -1;
    }
    if(emit(b, "    return f.record_failure({::nrforge::aper::ErrorCode::constraint_violation, f.cursor_bit()});\n}\ninline ::nrforge::aper::Result<%s> %s(::nrforge::aper::FieldReader& f) {\n    try {\n        %s value{};\n        auto id = %s(f);\n        if(!id) return ::nrforge::aper::Result<%s>::failure(id.error());\n        value.%s = ::std::move(id).value();\n        auto criticality = %s(f);\n        if(!criticality) return ::nrforge::aper::Result<%s>::failure(criticality.error());\n        value.%s = ::std::move(criticality).value();\n", p->qualified_type, p->get, p->qualified_type, p->members[0].get, p->qualified_type, p->members[0].name, p->members[1].get, p->qualified_type, p->members[1].name)) return -1;
    for(j = 0; j < r->row_count; ++j) {
        const struct member_plan *mp = &p->members[j + 2];
        if(emit(b, "        if(value.%s == %" PRIuMAX ") {\n            auto payload = f.read_known_open_type<%s>([](::nrforge::aper::FieldReader& child) { return %s(child); });\n            if(!payload) return ::nrforge::aper::Result<%s>::failure(payload.error());\n            value.%s = %s{::std::move(payload).value()};\n            return ::nrforge::aper::Result<%s>::success(::std::move(value));\n        }\n", p->members[0].name, (uintmax_t)r->rows[j].numeric_id, mp->type, mp->get, p->qualified_type, p->value_member, mp->qualified_wrapper, p->qualified_type)) return -1;
    }
    if(r->object_set_is_extensible) {
        if(emit(b, "        auto payload = f.read_open_type_owned();\n        if(!payload) return ::nrforge::aper::Result<%s>::failure(payload.error());\n        value.%s = %s{::std::move(payload).value()};\n        return ::nrforge::aper::Result<%s>::success(::std::move(value));\n", p->qualified_type, p->value_member, p->qualified_unknown_wrapper, p->qualified_type)) return -1;
    } else if(emit(b, "        auto error = f.record_failure({::nrforge::aper::ErrorCode::constraint_violation, f.cursor_bit()});\n        return ::nrforge::aper::Result<%s>::failure(error.error());\n", p->qualified_type)) return -1;
    if(emit(b, "    } catch(const ::std::bad_alloc&) {\n        auto error = f.record_failure({::nrforge::aper::ErrorCode::allocation_failure, f.cursor_bit()});\n        return ::nrforge::aper::Result<%s>::failure(error.error());\n    } catch(const ::std::length_error&) {\n        auto error = f.record_failure({::nrforge::aper::ErrorCode::resource_limit, f.cursor_bit()});\n        return ::nrforge::aper::Result<%s>::failure(error.error());\n    }\n}\n} // namespace compound_codec\n", p->qualified_type, p->qualified_type)) return -1;
    return emit(b, "inline ::nrforge::aper::Result<::nrforge::aper::CompleteEncoding> %s(const %s& v, const ::nrforge::aper::Limits& limits = {}) {\n    return ::nrforge::aper::encode_complete(v, limits, [&](::nrforge::aper::FieldWriter& f) { return %s(f, v); });\n}\ninline ::nrforge::aper::Result<%s> %s(::std::span<const ::std::byte> input, const ::nrforge::aper::Limits& limits = {}) {\n    return ::nrforge::aper::decode_complete<%s>(input, limits, [](::nrforge::aper::FieldReader& f) { return %s(f); });\n}\n", p->encode, p->qualified_type, p->qualified_put, p->qualified_type, p->decode, p->qualified_type, p->qualified_get);
}

struct node {
    const asn1typed_type_t *source;
    const asn1typed_type_ref_t *bound_key;
    const asn1typed_ioc_registry_t *registry;
    char *name;
    char **row_names;
    int private_empty;
    asn1typed_type_t lowered;
};
static int storage(size_t n, size_t cap, const void *p) { return n <= cap && !!cap == !!p; }
static int text(const char *s) { return s && s[0]; }
static char *join_name(char *prefix, const char *part) {
    size_t a = prefix ? strlen(prefix) : 0, z = strlen(part);
    char *p;
    if(z > SIZE_MAX - 2 || a > SIZE_MAX - z - 2) return NULL;
    p = (char *)malloc(a + z + 2);
    if(!p) return NULL;
    if(a) { memcpy(p, prefix, a); p[a++] = '-'; }
    memcpy(p + a, part, z + 1); return p;
}
static int add_name(char **name, const char *part) {
    char *p;
    const char *first = part;
    if(!text(part)) return -1;
    while(*first == '-') ++first;
    if(!((*first >= 'A' && *first <= 'Z') || (*first >= 'a' && *first <= 'z'))) return -1;
    if(!text(part) || !(p = join_name(*name, part))) return -1;
    free(*name); *name = p; return 0;
}
static size_t find_ref(const asn1typed_module_t *m, const asn1typed_type_ref_t *ref) {
    size_t i;
    if(ref->kind != ASN1TYPED_REF_NAMED || ref->primitive_kind != ASN1TYPED_PRIMITIVE_INVALID || !text(ref->module) || !text(ref->source_name)) return SIZE_MAX;
    if(ref->actual_count) {
        if(!ref->actuals) return SIZE_MAX;
        for(i = 0; i < m->bound_instance_count; ++i)
            if(asn1typed_type_ref_equal(ref, &m->bound_instances[i].identity)) return m->type_count + i;
    } else if(!ref->actuals) for(i = 0; i < m->type_count; ++i) {
        const asn1typed_type_identity_t *id = &m->types[i].identity;
        if(text(id->module) && text(id->source_name) && !strcmp(id->module, ref->module) && !strcmp(id->source_name, ref->source_name)) return i;
    }
    return SIZE_MAX;
}
static int reference(const asn1typed_module_t *m, const asn1typed_type_ref_t *r, const unsigned char *done) {
    size_t found;
    if(r->kind == ASN1TYPED_REF_PRIMITIVE)
        return (r->primitive_kind == ASN1TYPED_PRIMITIVE_NULL || r->primitive_kind == ASN1TYPED_PRIMITIVE_OBJECT_IDENTIFIER || r->primitive_kind == ASN1TYPED_PRIMITIVE_BOOLEAN || r->primitive_kind == ASN1TYPED_PRIMITIVE_OCTET_STRING || r->primitive_kind == ASN1TYPED_PRIMITIVE_BIT_STRING) && !r->module && !r->source_name && !r->actuals && !r->actual_count ? 1 : -1;
    found = find_ref(m, r);
    if(found == SIZE_MAX) return -1;
    return done ? !!done[found] : 1;
}
/* Only ordinary use-sites have owned INTEGER interval metadata. Registry
 * payload references and SEQUENCE OF elements have no such slot. */
static int inline_integer(const asn1typed_type_ref_t *r, const asn1typed_integer_value_range_t *v) {
    return r->kind == ASN1TYPED_REF_PRIMITIVE && r->primitive_kind == ASN1TYPED_PRIMITIVE_INTEGER &&
        !r->module && !r->source_name && !r->actuals && !r->actual_count && v->has_value_range == 1 &&
        (v->is_extensible == 0 || v->is_extensible == 1) && (!v->tail_count || v->tail) && v->lower_bound <= v->upper_bound &&
        v->lower_bound >= INT64_MIN && v->upper_bound <= INT64_MAX;
}
static int dependencies(const asn1typed_module_t *m, const struct node *n, const unsigned char *done) {
    size_t j;
    int r, ready = 1;
    const asn1typed_type_t *t = n->source;
#define DEP(ref) do { r = reference(m, ref, done); if(r < 0) return -1; if(!r) ready = 0; } while(0)
    if(n->private_empty) {
        DEP(&t->fields[0].type); DEP(&t->fields[1].type);
    } else if(n->registry) {
        DEP(&t->fields[0].type); DEP(&t->fields[1].type);
        for(j = 0; j < n->registry->row_count; ++j) DEP(&n->registry->rows[j].payload_type);
    } else if(t->kind == ASN1TYPED_TYPE_SEQUENCE) {
        for(j = 0; j < t->field_count; ++j) if(!inline_integer(&t->fields[j].type, &t->fields[j].value_range)) DEP(&t->fields[j].type);
    } else if(t->kind == ASN1TYPED_TYPE_CHOICE) {
        for(j = 0; j < t->alternative_count; ++j) if(!inline_integer(&t->alternatives[j].type_ref, &t->alternatives[j].value_range)) DEP(&t->alternatives[j].type_ref);
    } else if(t->kind == ASN1TYPED_TYPE_SEQUENCE_OF) DEP(&t->element_type);
#undef DEP
    return ready;
}
static void translate(const asn1typed_module_t *m, const struct node *nodes, asn1typed_type_ref_t *r) {
    size_t found;
    if(r->kind != ASN1TYPED_REF_NAMED) return;
    found = find_ref(m, r);
    memset(r, 0, sizeof(*r)); r->module = (char *)"IocLowered"; r->source_name = nodes[found].name;
}

static int render_ioc_raw(const asn1typed_module_t *m, const char *ns, char **out,
        char *diagnostic, size_t size, int mode, const char *const *names, size_t name_count) {
    struct node *nodes = NULL;
    asn1typed_module_t view;
    struct asn1typed_cpp_ioc_entry *entries = NULL;
    unsigned char *done = NULL;
    size_t count = 0, i, j, k;
    char why[512] = "invalid physical IOC renderer arguments";
    int result = -1;
    memset(&view, 0, sizeof(view));
    if(out) *out = NULL;
    if(diagnostic && size) diagnostic[0] = 0;
#define BAD(message) do { snprintf(why, sizeof(why), "%s", message); goto cleanup; } while(0)
    if(!out || !m || !text(m->source_name) || !storage(m->type_count, m->type_capacity, m->types) || !m->type_count || !storage(m->bound_instance_count, m->bound_instance_capacity, m->bound_instances) || !storage(m->ioc_registry_count, m->ioc_registry_capacity, m->ioc_registries) || !m->bound_instance_count) goto cleanup;
    if(m->type_count > SIZE_MAX - m->bound_instance_count) BAD("IOC graph node count overflow");
    count = m->type_count + m->bound_instance_count;
    if(count > SIZE_MAX / sizeof(*nodes) || count > SIZE_MAX / sizeof(*entries) || count > SIZE_MAX / sizeof(*view.types)) BAD("IOC graph allocation size overflow");
    nodes = (struct node *)calloc(count, sizeof(*nodes));
    entries = (struct asn1typed_cpp_ioc_entry *)calloc(count, sizeof(*entries));
    view.types = (asn1typed_type_t *)calloc(count, sizeof(*view.types));
    done = (unsigned char *)calloc(count, 1);
    if(!nodes || !entries || !view.types || !done) BAD("out of memory planning IOC graph");
    for(i = 0; i < m->ioc_registry_count; ++i) {
        if(asn1typed_ioc_registry_validate(&m->ioc_registries[i], why, sizeof(why))) goto cleanup;
        for(j = 0; j < i; ++j) {
            const asn1typed_ioc_registry_t *a = &m->ioc_registries[i], *b = &m->ioc_registries[j];
            if(!strcmp(a->class_module, b->class_module) && !strcmp(a->class_source_name, b->class_source_name) && !strcmp(a->object_set_module, b->object_set_module) && !strcmp(a->object_set_source_name, b->object_set_source_name)) BAD("duplicate IOC registry key");
        }
        for(j = 0; j < m->ioc_registries[i].row_count; ++j)
            if(reference(m, &m->ioc_registries[i].rows[j].payload_type, NULL) < 0) BAD("missing or unsupported IOC registry payload reference");
    }
    for(i = 0; i < count; ++i) {
        struct node *n = &nodes[i];
        if(i < m->type_count) {
            n->source = &m->types[i];
            if(!text(n->source->identity.module) || !text(n->source->identity.source_name)) BAD("invalid ordinary IOC graph identity");
            if(add_name(&n->name, n->source->identity.module) || add_name(&n->name, n->source->identity.source_name)) BAD("out of memory naming IOC graph");
            for(j = 0; j < i; ++j) if(!strcmp(n->source->identity.module, nodes[j].source->identity.module) && !strcmp(n->source->identity.source_name, nodes[j].source->identity.source_name)) BAD("duplicate ordinary IOC graph identity");
        } else {
            const asn1typed_bound_instance_t *instance = &m->bound_instances[i - m->type_count];
            const asn1typed_type_ref_t *key = &instance->identity;
            n->source = &instance->body; n->bound_key = key;
            if((instance->has_empty_private_binding != 0 && instance->has_empty_private_binding != 1) ||
                (!instance->has_empty_private_binding && (instance->empty_private_object_set.module || instance->empty_private_object_set.source_name)) ||
                (instance->has_empty_private_binding && n->source->kind != ASN1TYPED_TYPE_SEQUENCE)) BAD("inconsistent empty-private binding storage");
            if(instance->body_materialized != 1 || n->source->identity.module || n->source->identity.source_name || key->kind != ASN1TYPED_REF_NAMED || key->primitive_kind != ASN1TYPED_PRIMITIVE_INVALID || key->actual_count != 1 || !key->actuals || key->actuals[0].kind != ASN1TYPED_ACTUAL_OBJECT_SET_REFERENCE) BAD("unsupported IOC bound identity/body");
            if(add_name(&n->name, key->module) || add_name(&n->name, key->source_name) || add_name(&n->name, key->actuals[0].module) || add_name(&n->name, key->actuals[0].source_name)) BAD("invalid or unavailable bound IOC graph name");
            for(j = m->type_count; j < i; ++j) if(asn1typed_type_ref_equal(key, nodes[j].bound_key)) BAD("duplicate IOC bound identity");
            if(n->source->kind == ASN1TYPED_TYPE_SEQUENCE && instance->has_empty_private_binding == 1) {
                if(asn1typed_bound_instance_empty_private_validate(m, i - m->type_count, why, sizeof(why))) goto cleanup;
                if(n->source->field_count != 3 || n->source->fields[2].type_semantics != ASN1TYPED_FIELD_CLASS_FIELD_SELECTED_TYPE || n->source->fields[0].presence != ASN1TYPED_PRESENCE_MANDATORY || n->source->fields[1].presence != ASN1TYPED_PRESENCE_MANDATORY || n->source->fields[2].presence != ASN1TYPED_PRESENCE_MANDATORY) BAD("malformed empty-private binding");
                n->private_empty = 1;
            } else if(n->source->kind == ASN1TYPED_TYPE_SEQUENCE) {
                if(asn1typed_bound_instance_ioc_binding_validate(m, i - m->type_count, why, sizeof(why))) goto cleanup;
                n->registry = &m->ioc_registries[instance->ioc_binding.registry_index];
            } else if(n->source->kind == ASN1TYPED_TYPE_SEQUENCE_OF) {
                const asn1typed_ioc_binding_t *binding = &instance->ioc_binding;
                const asn1typed_type_ref_t *element = &n->source->element_type;
                if(binding->evidence != ASN1TYPED_WIRE_EVIDENCE_UNAVAILABLE || binding->has_valid_binding || binding->registry_index || binding->id_field_ordinal || binding->criticality_field_ordinal || binding->value_field_ordinal) BAD("unexpected collection IOC selector evidence");
                if(element->actual_count != 1 || !element->actuals || element->actuals[0].kind != key->actuals[0].kind || !text(element->actuals[0].module) || !text(element->actuals[0].source_name) || strcmp(element->actuals[0].module, key->actuals[0].module) || strcmp(element->actuals[0].source_name, key->actuals[0].source_name)) BAD("IOC container element actual key mismatch");
            } else BAD("unsupported IOC bound body kind");
        }
        for(j = 0; j < i; ++j) if(!strcmp(n->name, nodes[j].name)) BAD("ambiguous concatenated IOC graph identity");
        /* Check storage before walking source arrays; compound validates all
         * residual semantics after identity/reference translation. */
        if(!storage(n->source->field_count, n->source->field_capacity, n->source->fields) || !storage(n->source->alternative_count, n->source->alternative_capacity, n->source->alternatives)) BAD("malformed IOC graph member storage");
        if(dependencies(m, n, NULL) < 0) BAD("missing or unsupported physical graph reference");
    }
    for(i = 0; i < m->ioc_registry_count; ++i) {
        for(j = 0; j < count; ++j) if(nodes[j].registry == &m->ioc_registries[i]) break;
        if(j == count) BAD("IOC registry has no proven physical field binding");
    }
    for(i = 0; i < count; ++i) {
        struct node *n = &nodes[i];
        n->lowered = *n->source;
        n->lowered.identity.module = (char *)"IocLowered"; n->lowered.identity.source_name = n->name;
        /* Borrow scalar/enum metadata; own only arrays whose refs are remapped. */
        n->lowered.fields = NULL; n->lowered.alternatives = NULL;
        if(n->registry) {
            size_t rows = n->registry->row_count;
            if(rows > SIZE_MAX - 2 || rows + 2 > SIZE_MAX / sizeof(*n->lowered.fields) || rows > SIZE_MAX / sizeof(*n->row_names)) BAD("IOC row planning overflow");
            n->lowered.field_count = n->lowered.field_capacity = rows + 2;
            n->lowered.fields = (asn1typed_field_t *)calloc(rows + 2, sizeof(*n->lowered.fields));
            if(rows) n->row_names = (char **)calloc(rows, sizeof(*n->row_names));
            if(!n->lowered.fields || (rows && !n->row_names)) BAD("out of memory planning IOC rows");
            for(j = 0; j < 2; ++j) {
                n->lowered.fields[j] = n->source->fields[j];
                /* The original role proof was independently validated above. */
                n->lowered.fields[j].has_class_field_relation = 0;
                memset(&n->lowered.fields[j].class_field_relation, 0, sizeof(n->lowered.fields[j].class_field_relation));
                translate(m, nodes, &n->lowered.fields[j].type);
            }
            for(j = 0; j < rows; ++j) {
                const asn1typed_ioc_dispatch_row_t *row = &n->registry->rows[j];
                char numeric[64];
                const char *raw = row->symbolic_id ? asn1typed_name_ioc_identity(row->symbolic_id) : NULL;
                if(!raw) { snprintf(numeric, sizeof(numeric), "row-%" PRIuMAX, (uintmax_t)row->numeric_id); raw = numeric; }
                n->row_names[j] = join_name(NULL, raw);
                if(!n->row_names[j]) BAD("out of memory naming IOC rows");
                n->lowered.fields[j + 2].source_name = n->row_names[j];
                n->lowered.fields[j + 2].type = row->payload_type;
                translate(m, nodes, &n->lowered.fields[j + 2].type);
            }
        } else {
            if(n->source->field_count > SIZE_MAX / sizeof(*n->lowered.fields) || n->source->alternative_count > SIZE_MAX / sizeof(*n->lowered.alternatives)) BAD("IOC member allocation overflow");
            if(n->source->field_capacity) {
                size_t allocation_count = n->source->field_count ? n->source->field_count : 1;
                n->lowered.fields = (asn1typed_field_t *)malloc(allocation_count * sizeof(*n->lowered.fields));
                if(!n->lowered.fields) BAD("out of memory planning physical fields");
                memcpy(n->lowered.fields, n->source->fields, n->source->field_count * sizeof(*n->lowered.fields));
                for(j = 0; j < n->source->field_count; ++j)
                    if(!n->private_empty || j != 2) translate(m, nodes, &n->lowered.fields[j].type);
                if(n->private_empty) {
                    asn1typed_field_t *payload = &n->lowered.fields[2];
                    payload->type_semantics = ASN1TYPED_FIELD_FIXED_TYPE;
                    memset(&payload->type, 0, sizeof(payload->type));
                    payload->type.kind = ASN1TYPED_REF_PRIMITIVE;
                    payload->type.primitive_kind = ASN1TYPED_PRIMITIVE_OPEN_TYPE;
                    payload->has_class_field_relation = 0;
                    memset(&payload->class_field_relation, 0, sizeof(payload->class_field_relation));
                    n->lowered.fields[1].has_class_field_relation = 0;
                    memset(&n->lowered.fields[1].class_field_relation, 0, sizeof(n->lowered.fields[1].class_field_relation));
                }
            }
            if(n->source->alternative_count) {
                n->lowered.alternatives = (asn1typed_choice_alternative_t *)malloc(n->source->alternative_count * sizeof(*n->lowered.alternatives));
                if(!n->lowered.alternatives) BAD("out of memory planning physical alternatives");
                memcpy(n->lowered.alternatives, n->source->alternatives, n->source->alternative_count * sizeof(*n->lowered.alternatives));
                for(j = 0; j < n->source->alternative_count; ++j) translate(m, nodes, &n->lowered.alternatives[j].type_ref);
            }
            if(n->source->kind == ASN1TYPED_TYPE_SEQUENCE_OF) translate(m, nodes, &n->lowered.element_type);
        }
    }
    for(k = 0; k < count; ++k) {
        for(i = 0; i < count; ++i) if(!done[i] && dependencies(m, &nodes[i], done) == 1) break;
        if(i == count) BAD("recursive physical IOC graph");
        done[i] = 1; view.types[k] = nodes[i].lowered;
        if(nodes[i].registry) { entries[k].registry = nodes[i].registry; entries[k].value_source_name = nodes[i].source->fields[2].source_name; }
    }
    view.source_name = (char *)"IocLowered"; view.type_count = view.type_capacity = count;
    result = mode == 3 ? asn1typed_render_cpp_compound_ioc_check_names(&view, ns, entries, names, name_count, why, sizeof(why))
        : asn1typed_render_cpp_compound_ioc(&view, ns, entries, mode, out, why, sizeof(why));
cleanup:
    if(nodes) for(i = 0; i < count; ++i) {
        if(nodes[i].row_names) for(j = 0; nodes[i].registry && j < nodes[i].registry->row_count; ++j) free(nodes[i].row_names[j]);
        free(nodes[i].row_names); free(nodes[i].name); free(nodes[i].lowered.fields); free(nodes[i].lowered.alternatives);
    }
    free(nodes); free(entries); free(done); free(view.types);
    if(result && diagnostic && size) snprintf(diagnostic, size, "%s", why);
    return result;
#undef BAD
}

static int render_ioc(const asn1typed_module_t *m,const char *ns,char **out,
        char *why,size_t size,int mode,const char *const *names,size_t count) {
    struct asn1typed_cpp_inline_enum_view view={0}; int result;
    if(out) *out=NULL;
    if(!out) { if(why&&size) snprintf(why,size,"invalid IOC renderer output argument"); return -1; }
    if(asn1typed_cpp_inline_enum_view_init(&view,m,why,size)) return -1;
    result=render_ioc_raw(view.module,ns,out,why,size,mode,names,count);
    asn1typed_cpp_inline_enum_view_clear(&view); return result;
}
int asn1typed_render_cpp_owned_ioc_types(const asn1typed_module_t *m, const char *ns, char **out, char *why, size_t size) { return render_ioc(m, ns, out, why, size, 0, NULL, 0); }
int asn1typed_render_cpp_owned_ioc_mapping(const asn1typed_module_t *m, const char *ns, char **out, char *why, size_t size) { return render_ioc(m, ns, out, why, size, 1, NULL, 0); }
int asn1typed_render_cpp_owned_ioc_codec(const asn1typed_module_t *m, const char *ns, char **out, char *why, size_t size) { return render_ioc(m, ns, out, why, size, 2, NULL, 0); }
int asn1typed_render_cpp_ioc_check_names(const asn1typed_module_t *m, const char *ns, const char *const *names, size_t count, char *why, size_t size) {
    char *unused = NULL;
    return render_ioc(m, ns, &unused, why, size, 3, names, count);
}
