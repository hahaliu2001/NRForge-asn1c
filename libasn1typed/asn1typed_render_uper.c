#include "asn1typed_render_uper.h"
#include "asn1typed_render_cpp_internal.h"
#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct buf {
    char *data;
    size_t n;
};
struct link {
    size_t node;
    asn1typed_primitive_kind_e primitive;
    char *name, *def;
    const asn1typed_size_constraint_t *size;
    const asn1typed_integer_value_range_t *range;
};
struct node {
    const asn1typed_type_t *type;
    struct link *members;
    size_t count;
};
static const asn1typed_size_constraint_t empty_sz = {0};
static const asn1typed_integer_value_range_t empty_r = {0};
struct plan {
    struct node *nodes;
    size_t n, *order;
    const asn1typed_module_t *graph;
    char why[512];
};
static int fmt(struct buf *b, const char *format, ...) {
    va_list args;
    int count;
    char *next;
    va_start(args, format);
    count = vsnprintf(NULL, 0, format, args);
    va_end(args);
    if (count < 0 || (size_t)count > SIZE_MAX - b->n - 1)
        return -1;
    next = realloc(b->data, b->n + (size_t)count + 1);
    if (!next)
        return -1;
    b->data = next;
    va_start(args, format);
    vsnprintf(b->data + b->n, (size_t)count + 1, format, args);
    va_end(args);
    b->n += (size_t)count;
    return 0;
}
#define EMIT(...)                                                                                  \
    do {                                                                                           \
        if (fmt(b, __VA_ARGS__))                                                                   \
            return -1;                                                                             \
    } while (0)
static int quoted(struct buf *b, const char *s) {
    EMIT("\"");
    for (const unsigned char *p = (const unsigned char *)s; *p; ++p) {
        if (*p == '"' || *p == '\\')
            EMIT("\\%c", *p);
        else if (*p < 32 || *p >= 127)
            EMIT("\\%03o", *p);
        else
            EMIT("%c", *p);
    }
    EMIT("\"");
    return 0;
}
static int bad(struct plan *p, const char *why) {
    if (why != p->why)
        snprintf(p->why, sizeof(p->why), "%s", why);
    return -1;
}
static int storage(size_t n, size_t cap, const void *ptr) { return n <= cap && !!cap == !!ptr; }
static int range_ok(const asn1typed_integer_value_range_t *r) {
    if (r->unsigned_bounds)
        return asn1typed_integer_unsigned_valid(r);
    if (!asn1typed_integer_unsigned_empty(r) || r->tail || r->tail_count ||
        r->extension_additions || r->extension_addition_count)
        return 0;
    if (r->has_value_range == 0)
        return !r->is_extensible && !r->lower_bound && !r->upper_bound;
    return r->has_value_range == 1 && (r->is_extensible == 0 || r->is_extensible == 1) &&
           r->lower_bound >= INT64_MIN && r->upper_bound <= INT64_MAX &&
           r->lower_bound <= r->upper_bound;
}
static int size_ok(const asn1typed_size_constraint_t *s) {
    if (s->has_extension_addition || s->extension_lower_bound || s->extension_upper_bound)
        return 0;
    if (!s->has_size_constraint)
        return !s->is_extensible && !s->lower_bound && !s->upper_bound;
    return s->has_size_constraint == 1 && (s->is_extensible == 0 || s->is_extensible == 1) &&
           s->lower_bound >= 0 && s->upper_bound >= s->lower_bound &&
           (uintmax_t)s->upper_bound <= SIZE_MAX;
}
static int empty_size(const asn1typed_size_constraint_t *s) {
    return size_ok(s) && !s->has_size_constraint;
}
static int empty_range(const asn1typed_integer_value_range_t *r) {
    return range_ok(r) && !r->has_value_range;
}
static int primitive_ok(asn1typed_primitive_kind_e k) {
    return k == ASN1TYPED_PRIMITIVE_BOOLEAN || k == ASN1TYPED_PRIMITIVE_INTEGER ||
           k == ASN1TYPED_PRIMITIVE_NULL || k == ASN1TYPED_PRIMITIVE_BIT_STRING ||
           k == ASN1TYPED_PRIMITIVE_OCTET_STRING;
}
static size_t find(const struct plan *p, const asn1typed_type_ref_t *r) {
    if (!r->module || !r->source_name)
        return SIZE_MAX;
    for (size_t i = 0; i < p->graph->type_count; ++i) {
        const asn1typed_type_t *t = p->nodes[i].type;
        if (!strcmp(t->identity.module, r->module) &&
            !strcmp(t->identity.source_name, r->source_name))
            return i;
    }
    return SIZE_MAX;
}
static const asn1typed_type_t *target(const struct plan *p, const struct link *l) {
    return l->node == SIZE_MAX ? NULL : p->nodes[l->node].type;
}
static asn1typed_primitive_kind_e kind(const struct plan *p, const struct link *l) {
    const asn1typed_type_t *t = target(p, l);
    return t ? t->primitive_kind : l->primitive;
}
static const asn1typed_size_constraint_t *size_for(const struct plan *p, const struct link *l) {
    const asn1typed_type_t *t = target(p, l);
    return l->size->has_size_constraint ? l->size : t ? &t->size_constraint : l->size;
}
static const asn1typed_integer_value_range_t *range_for(const struct plan *p,
                                                        const struct link *l) {
    const asn1typed_type_t *t = target(p, l);
    return l->range->has_value_range ? l->range : t ? &t->value_range : l->range;
}
static int resolve(struct plan *p, const asn1typed_type_ref_t *r,
                   const asn1typed_type_t *inline_enum, struct link *l) {
    l->node = SIZE_MAX;
    if (inline_enum) {
        for (size_t i = p->graph->type_count; i < p->n; ++i)
            if (p->nodes[i].type == inline_enum) {
                l->node = i;
                return 0;
            }
        return bad(p, "unowned inline enum");
    }
    if (r->kind == ASN1TYPED_REF_PRIMITIVE) {
        if (r->module || r->source_name || r->actuals || r->actual_count ||
            !primitive_ok(r->primitive_kind))
            return bad(p, "unsupported primitive reference");
        l->primitive = r->primitive_kind;
        return 0;
    }
    if (r->kind != ASN1TYPED_REF_NAMED || r->primitive_kind != ASN1TYPED_PRIMITIVE_INVALID ||
        (l->node = find(p, r)) == SIZE_MAX)
        return bad(p, "missing module-qualified dependency");
    const asn1typed_type_t *t = target(p, l);
    if (r->actual_count && !asn1typed_type_ref_equal(r, &t->ordinary_instance))
        return bad(p, "specialization actual identity mismatch");
    if (!r->actual_count && r->actuals)
        return bad(p, "invalid actual storage");
    return 0;
}
static int validate_link(struct plan *p, struct link *l) {
    const asn1typed_type_t *t = target(p, l);
    asn1typed_primitive_kind_e k = kind(p, l);
    if (!size_ok(l->size) || !range_ok(l->range))
        return bad(p, "unsupported effective use-site constraint");
    if (l->size->has_size_constraint) {
        if (k != ASN1TYPED_PRIMITIVE_BIT_STRING && k != ASN1TYPED_PRIMITIVE_OCTET_STRING)
            return bad(p, "constructed use-site SIZE unsupported");
        if (t && t->size_constraint.has_size_constraint &&
            (l->size->is_extensible != t->size_constraint.is_extensible ||
             l->size->lower_bound < t->size_constraint.lower_bound ||
             l->size->upper_bound > t->size_constraint.upper_bound))
            return bad(p, "use-site SIZE widens named constraint");
    }
    if (l->range->has_value_range) {
        if (k != ASN1TYPED_PRIMITIVE_INTEGER)
            return bad(p, "non-INTEGER range");
        if (t && t->value_range.has_value_range) {
            const asn1typed_integer_value_range_t *r = &t->value_range, *u = l->range;
            if (r->unsigned_bounds != u->unsigned_bounds || r->is_extensible != u->is_extensible ||
                (!r->unsigned_bounds &&
                 (u->lower_bound < r->lower_bound || u->upper_bound > r->upper_bound)) ||
                (r->unsigned_bounds && (u->unsigned_lower_bound < r->unsigned_lower_bound ||
                                        u->unsigned_upper_bound > r->unsigned_upper_bound)))
                return bad(p, "use-site INTEGER widens named constraint");
        }
    }
    return 0;
}
static int default_for(struct plan *p, struct link *l, const asn1typed_default_value_t *d) {
    struct buf b = {0};
    const asn1typed_type_t *t = target(p, l);
    const asn1typed_integer_value_range_t *r = range_for(p, l);
    asn1typed_primitive_kind_e k = kind(p, l);
    char scalar[128];
    if (d->kind == ASN1TYPED_DEFAULT_ENUMERATED && t && t->kind == ASN1TYPED_TYPE_ENUMERATED &&
        d->enum_symbol) {
        size_t i;
        for (i = 0; i < t->enum_item_count; ++i)
            if (!strcmp(t->enum_items[i].source_name, d->enum_symbol) &&
                t->enum_items[i].assigned_number == d->number)
                break;
        if (i == t->enum_item_count)
            return bad(p, "DEFAULT enum symbol/number mismatch");
        snprintf(scalar, sizeof(scalar), "::nrforge::uper::Index{%s,%zu}",
                 t->enum_items[i].is_extension_addition ? "true" : "false",
                 t->enum_items[i].per_enumeration_index);
    } else if (d->kind == ASN1TYPED_DEFAULT_BOOLEAN && k == ASN1TYPED_PRIMITIVE_BOOLEAN &&
               (d->number == 0 || d->number == 1) && !d->enum_symbol)
        snprintf(scalar, sizeof(scalar), "%s", d->number ? "true" : "false");
    else if (d->kind == ASN1TYPED_DEFAULT_INTEGER && k == ASN1TYPED_PRIMITIVE_INTEGER &&
             !d->enum_symbol) {
        if (r->unsigned_bounds) {
            if (d->number < 0 || (uintmax_t)d->number < r->unsigned_lower_bound ||
                (uintmax_t)d->number > r->unsigned_upper_bound)
                return bad(p, "DEFAULT out of unsigned range");
            snprintf(scalar, sizeof(scalar), "UINT64_C(%ju)", (uintmax_t)d->number);
        } else {
            if (d->number < INT64_MIN || d->number > INT64_MAX ||
                (r->has_value_range && !r->is_extensible &&
                 (d->number < r->lower_bound || d->number > r->upper_bound)))
                return bad(p, "DEFAULT outside range");
            if (d->number == INT64_MIN)
                snprintf(scalar, sizeof(scalar), "INT64_MIN");
            else
                snprintf(scalar, sizeof(scalar), "INT64_C(%jd)", d->number);
        }
    } else
        return bad(p, "unsupported DEFAULT");
    if (l->node != SIZE_MAX) {
        if (fmt(&b, "T%zu{%s}", l->node, scalar))
            return -1;
    } else if (fmt(&b, "%s", scalar))
        return -1;
    l->def = b.data;
    return 0;
}
static int build(struct plan *p, const asn1typed_module_t *g) {
    if (!g || !storage(g->type_count, g->type_capacity, g->types) || !g->type_count ||
        g->type_count > 4096 || g->ioc_registry_count || g->bound_instance_count)
        return bad(p, "invalid ordinary graph storage/IOC");
    p->graph = g;
    p->n = g->type_count;
    for (size_t i = 0; i < g->type_count; ++i) {
        const asn1typed_type_t *t = &g->types[i];
        if (!t->identity.module || !t->identity.source_name)
            return bad(p, "missing type identity");
        for (size_t j = 0; j < i; ++j)
            if (!strcmp(t->identity.module, g->types[j].identity.module) &&
                !strcmp(t->identity.source_name, g->types[j].identity.source_name))
                return bad(p, "duplicate type identity");
        if (!storage(t->field_count, t->field_capacity, t->fields) ||
            !storage(t->alternative_count, t->alternative_capacity, t->alternatives))
            return bad(p, "invalid component storage");
        for (size_t j = 0; j < t->field_count; ++j)
            p->n += !!t->fields[j].inline_enumerated;
        for (size_t j = 0; j < t->alternative_count; ++j)
            p->n += !!t->alternatives[j].inline_enumerated;
        if (p->n > 8192)
            return bad(p, "UPER render node cap");
    }
    p->nodes = calloc(p->n, sizeof(*p->nodes));
    p->order = calloc(p->n, sizeof(*p->order));
    if (!p->nodes || !p->order)
        return bad(p, "allocation failure");
    size_t n = g->type_count;
    for (size_t i = 0; i < g->type_count; ++i) {
        p->nodes[i].type = &g->types[i];
        const asn1typed_type_t *t = &g->types[i];
        for (size_t j = 0; j < t->field_count; ++j)
            if (t->fields[j].inline_enumerated)
                p->nodes[n++].type = t->fields[j].inline_enumerated;
        for (size_t j = 0; j < t->alternative_count; ++j)
            if (t->alternatives[j].inline_enumerated)
                p->nodes[n++].type = t->alternatives[j].inline_enumerated;
    }
    for (size_t i = 0; i < p->n; ++i) {
        const asn1typed_type_t *t = p->nodes[i].type;
        char why[256];
        if (i < g->type_count) {
            if (!t->identity.module || !t->identity.source_name)
                return bad(p, "missing type identity");
            for (size_t j = 0; j < i; ++j)
                if (!strcmp(t->identity.module, p->nodes[j].type->identity.module) &&
                    !strcmp(t->identity.source_name, p->nodes[j].type->identity.source_name))
                    return bad(p, "duplicate type identity");
        }
        if (!size_ok(&t->size_constraint) || !range_ok(&t->value_range) || t->has_ioc_table ||
            t->ioc_object_set_is_extensible)
            return bad(p, "unsupported type constraints/IOC");
        if (t->kind == ASN1TYPED_TYPE_PRIMITIVE) {
            if (!primitive_ok(t->primitive_kind) ||
                (t->primitive_kind != ASN1TYPED_PRIMITIVE_INTEGER &&
                 !empty_range(&t->value_range)) ||
                ((t->primitive_kind != ASN1TYPED_PRIMITIVE_BIT_STRING &&
                  t->primitive_kind != ASN1TYPED_PRIMITIVE_OCTET_STRING) &&
                 !empty_size(&t->size_constraint)) ||
                t->field_count || t->alternative_count || t->enum_item_count || t->is_extensible)
                return bad(p, "unsupported primitive body");
        } else {
            if (t->primitive_kind != ASN1TYPED_PRIMITIVE_INVALID || !empty_range(&t->value_range))
                return bad(p, "non-primitive range");
            if (t->kind != ASN1TYPED_TYPE_SEQUENCE_OF && !empty_size(&t->size_constraint))
                return bad(p, "non-collection SIZE");
        }
        if (t->kind == ASN1TYPED_TYPE_ENUMERATED) {
            if (asn1typed_enumerated_evidence_validate(t, why, sizeof(why)))
                return bad(p, why);
            if (t->field_count || t->alternative_count)
                return bad(p, "enum residual components");
        } else if (t->enum_item_count)
            return bad(p, "non-enum enum items");
        if (t->kind == ASN1TYPED_TYPE_SEQUENCE || t->kind == ASN1TYPED_TYPE_CHOICE) {
            if (asn1typed_ordinary_extension_structure_validate(t, why, sizeof(why)))
                return bad(p, why);
            if (t->ordinary_addition_count >= 16384)
                return bad(p, "addition bitmap profile limit");
        } else if (t->ordinary_structure_owned || t->field_count || t->alternative_count)
            return bad(p, "residual ordinary structure");
        if (t->kind < ASN1TYPED_TYPE_PRIMITIVE || t->kind > ASN1TYPED_TYPE_CHOICE)
            return bad(p, "invalid kind");
    }
    /* Validate every body before resolving forward references and DEFAULTs. */
    for (size_t i = 0; i < p->n; ++i) {
        const asn1typed_type_t *t = p->nodes[i].type;
        p->nodes[i].count = t->kind == ASN1TYPED_TYPE_SEQUENCE      ? t->field_count
                            : t->kind == ASN1TYPED_TYPE_CHOICE      ? t->alternative_count
                            : t->kind == ASN1TYPED_TYPE_SEQUENCE_OF ? 1
                                                                    : 0;
        if (p->nodes[i].count) {
            p->nodes[i].members = calloc(p->nodes[i].count, sizeof(struct link));
            if (!p->nodes[i].members)
                return bad(p, "allocation failure");
        }
        asn1typed_name_scope_t names = {0};
        for (size_t j = 0; j < p->nodes[i].count; ++j) {
            struct link *l = &p->nodes[i].members[j];
            const asn1typed_type_ref_t *ref;
            const asn1typed_type_t *en = NULL;
            const asn1typed_size_constraint_t *sz = &t->size_constraint;
            const asn1typed_integer_value_range_t *r = &t->value_range;
            const char *name = "element";
            char group_name[64];
            if (t->kind == ASN1TYPED_TYPE_SEQUENCE) {
                const asn1typed_field_t *f = &t->fields[j];
                ref = &f->type;
                en = f->inline_enumerated;
                sz = &f->size_constraint;
                r = &f->value_range;
                name = f->source_name;
                if (f->has_class_field_relation ||
                    (f->presence != ASN1TYPED_PRESENCE_MANDATORY &&
                     f->presence != ASN1TYPED_PRESENCE_OPTIONAL &&
                     f->presence != ASN1TYPED_PRESENCE_DEFAULT) ||
                    ((f->type_semantics == ASN1TYPED_FIELD_INLINE_ENUMERATED) != !!en) ||
                    (f->type_semantics != ASN1TYPED_FIELD_FIXED_TYPE &&
                     f->type_semantics != ASN1TYPED_FIELD_INLINE_ENUMERATED)) {
                    asn1typed_name_scope_clear(&names);
                    return bad(p, "unsupported field semantics/presence");
                }
            } else if (t->kind == ASN1TYPED_TYPE_CHOICE) {
                const asn1typed_choice_alternative_t *a = &t->alternatives[j];
                ref = &a->type_ref;
                en = a->inline_enumerated;
                sz = &a->size_constraint;
                r = &a->value_range;
                name = a->source_name;
            } else {
                ref = &t->element_type;
                sz = &empty_sz;
                r = &empty_r;
            }
            l->size = sz;
            l->range = r;
            const char *naming = name;
            if (t->kind == ASN1TYPED_TYPE_SEQUENCE &&
                t->fields[j].component_role == ASN1TYPED_COMPONENT_GROUP) {
                snprintf(group_name, sizeof(group_name), "group-%zu",
                         t->fields[j].per_component_index);
                naming = group_name;
            }
            l->name = asn1typed_render_cpp_final_name(naming, ASN1TYPED_NAME_FIELD);
            if (!l->name || strlen(l->name) > 127 ||
                asn1typed_name_scope_add(&names, name, l->name) != ASN1TYPED_NAME_OK ||
                resolve(p, ref, en, l) ||
                (t->kind != ASN1TYPED_TYPE_SEQUENCE_OF && validate_link(p, l))) {
                asn1typed_name_scope_clear(&names);
                char prior[512];
                snprintf(prior, sizeof(prior), "%s",
                         p->why[0] ? p->why : "name allocation/collision");
                snprintf(p->why, sizeof(p->why), "%s.%s/%s: %.200s",
                         t->identity.module ? t->identity.module : "inline",
                         t->identity.source_name ? t->identity.source_name : "enum", name, prior);
                return -1;
            }
            if (t->kind == ASN1TYPED_TYPE_SEQUENCE) {
                const asn1typed_field_t *f = &t->fields[j];
                if (f->presence == ASN1TYPED_PRESENCE_DEFAULT) {
                    if (default_for(p, l, &f->default_value)) {
                        asn1typed_name_scope_clear(&names);
                        return -1;
                    }
                } else if (f->default_value.kind != ASN1TYPED_DEFAULT_NONE ||
                           f->default_value.number || f->default_value.enum_symbol) {
                    asn1typed_name_scope_clear(&names);
                    return bad(p, "residual DEFAULT");
                }
            }
        }
        asn1typed_name_scope_clear(&names);
    }
    unsigned char *done = calloc(p->n, 1);
    if (!done)
        return bad(p, "allocation failure");
    for (size_t out = 0; out < p->n; ++out) {
        size_t i;
        for (i = 0; i < p->n; ++i) {
            if (done[i])
                continue;
            size_t j;
            for (j = 0; j < p->nodes[i].count; ++j) {
                size_t dep = p->nodes[i].members[j].node;
                if (dep != SIZE_MAX && !done[dep])
                    break;
            }
            if (j == p->nodes[i].count)
                break;
        }
        if (i == p->n) {
            free(done);
            return bad(p, "recursive value closure unsupported");
        }
        done[i] = 1;
        p->order[out] = i;
    }
    free(done);
    return 0;
}
static void clear(struct plan *p) {
    if (p->nodes)
        for (size_t i = 0; i < p->n; ++i) {
            for (size_t j = 0; p->nodes[i].members && j < p->nodes[i].count; ++j) {
                free(p->nodes[i].members[j].name);
                free(p->nodes[i].members[j].def);
            }
            free(p->nodes[i].members);
        }
    free(p->nodes);
    free(p->order);
}
static int spelling(struct buf *b, const struct plan *p, const struct link *l) {
    if (l->node != SIZE_MAX) {
        EMIT("T%zu", l->node);
        return 0;
    }
    const char *name = kind(p, l) == ASN1TYPED_PRIMITIVE_BOOLEAN      ? "bool"
                       : kind(p, l) == ASN1TYPED_PRIMITIVE_NULL       ? "std::monostate"
                       : kind(p, l) == ASN1TYPED_PRIMITIVE_BIT_STRING ? "::nrforge::uper::BitString"
                       : kind(p, l) == ASN1TYPED_PRIMITIVE_OCTET_STRING ? "std::vector<std::byte>"
                       : l->range->unsigned_bounds                      ? "std::uint64_t"
                                                                        : "std::int64_t";
    EMIT("%s", name);
    return 0;
}
static int optional(const asn1typed_type_t *t, size_t j) {
    const asn1typed_field_t *f = &t->fields[j];
    return f->presence == ASN1TYPED_PRESENCE_OPTIONAL ||
           (j >= t->ordinary_root_count && f->presence != ASN1TYPED_PRESENCE_DEFAULT);
}
static int model(struct buf *b, const struct plan *p, size_t i) {
    const asn1typed_type_t *t = p->nodes[i].type;
    EMIT("struct T%zu {\n", i);
    if (t->kind == ASN1TYPED_TYPE_PRIMITIVE) {
        struct link l = {.node = SIZE_MAX,
                         .primitive = t->primitive_kind,
                         .size = &t->size_constraint,
                         .range = &t->value_range};
        if (spelling(b, p, &l))
            return -1;
        EMIT(" value{};\n");
    }
    if (t->kind == ASN1TYPED_TYPE_ENUMERATED) {
        EMIT("::nrforge::uper::Index value{};\n");
        for (size_t j = 0; j < t->enum_item_count; ++j)
            EMIT("static constexpr ::nrforge::uper::Index e%zu{%s,%zu};\n", j,
                 t->enum_items[j].is_extension_addition ? "true" : "false",
                 t->enum_items[j].per_enumeration_index);
    }
    if (t->kind == ASN1TYPED_TYPE_ENUMERATED)
        for (size_t j = 0; j < t->enum_item_count; ++j) {
            EMIT("static constexpr std::int64_t number%zu=", j);
            if (t->enum_items[j].assigned_number == INT64_MIN)
                EMIT("INT64_MIN");
            else
                EMIT("INT64_C(%jd)", t->enum_items[j].assigned_number);
            EMIT(";static constexpr std::string_view symbol%zu=", j);
            if (quoted(b, t->enum_items[j].source_name))
                return -1;
            EMIT(";\n");
        }
    if (t->kind == ASN1TYPED_TYPE_SEQUENCE)
        for (size_t j = 0; j < t->field_count; ++j) {
            EMIT("static constexpr std::string_view field_source%zu=", j);
            if (quoted(b, t->fields[j].source_name))
                return -1;
            EMIT(";\n");
        }
    if (t->kind == ASN1TYPED_TYPE_CHOICE)
        for (size_t j = 0; j < t->alternative_count; ++j) {
            const asn1typed_choice_alternative_t *a = &t->alternatives[j];
            EMIT("static constexpr std::string_view alternative%zu=", j);
            if (quoted(b, a->source_name))
                return -1;
            EMIT(";static constexpr ::nrforge::uper::Index wire%zu{%s,%zu};\n", j,
                 a->is_extension_addition ? "true" : "false",
                 a->is_extension_addition ? a->per_extension_index : a->per_root_index);
        }
    if (t->kind == ASN1TYPED_TYPE_SEQUENCE) {
        for (size_t j = 0; j < t->field_count; ++j) {
            const struct link *l = &p->nodes[i].members[j];
            if (optional(t, j))
                EMIT("std::optional<");
            if (spelling(b, p, l))
                return -1;
            if (optional(t, j))
                EMIT(">");
            EMIT(" f_%s", l->name);
            if (l->def)
                EMIT(" = %s", l->def);
            else
                EMIT("{}");
            EMIT(";\n");
        }
    }
    if (t->kind == ASN1TYPED_TYPE_SEQUENCE_OF) {
        EMIT("std::vector<");
        if (spelling(b, p, &p->nodes[i].members[0]))
            return -1;
        EMIT("> value;\n");
    }
    if (t->kind == ASN1TYPED_TYPE_CHOICE) {
        for (size_t j = 0; j < t->alternative_count; ++j) {
            EMIT("struct A%zu { ", j);
            if (spelling(b, p, &p->nodes[i].members[j]))
                return -1;
            EMIT(" value{}; bool operator==(const A%zu&) const = default; };\n", j);
        }
        EMIT("std::variant<std::monostate");
        for (size_t j = 0; j < t->alternative_count; ++j)
            EMIT(",A%zu", j);
        if (t->is_extensible)
            EMIT(",Unknown");
        EMIT("> value;\n");
    }
    EMIT("bool operator==(const T%zu&) const = default;\n};\n", i);
    return 0;
}
static int size_expr(struct buf *b, const asn1typed_size_constraint_t *s) {
    EMIT("::nrforge::uper::Size{%ju,%ju,%s,%s}", (uintmax_t)s->lower_bound,
         (uintmax_t)s->upper_bound, s->has_size_constraint ? "false" : "true",
         s->is_extensible ? "true" : "false");
    return 0;
}
static int integer_expr(struct buf *b, const asn1typed_integer_value_range_t *r, int encode,
                        const char *value) {
    if (!r->has_value_range) {
        EMIT("io.%s_unconstrained_int(%s)", encode ? "write" : "read", encode ? value : "");
        return 0;
    }
    if (r->unsigned_bounds) {
        EMIT("io.%s_uint(", encode ? "write" : "read");
        if (encode)
            EMIT("%s,", value);
        EMIT("UINT64_C(%" PRIu64 "),UINT64_C(%" PRIu64 "))", r->unsigned_lower_bound,
             r->unsigned_upper_bound);
        return 0;
    }
    EMIT("io.%s_%sint(", encode ? "write" : "read", r->is_extensible ? "extensible_" : "");
    if (encode)
        EMIT("%s,", value);
    if (r->lower_bound == INT64_MIN)
        EMIT("INT64_MIN");
    else
        EMIT("INT64_C(%jd)", r->lower_bound);
    if (r->upper_bound == INT64_MIN)
        EMIT(",INT64_MIN)");
    else
        EMIT(",INT64_C(%jd))", r->upper_bound);
    return 0;
}
static int primitive_expr(struct buf *b, const struct plan *p, const struct link *l, int encode,
                          const char *value) {
    asn1typed_primitive_kind_e k = kind(p, l);
    if (k == ASN1TYPED_PRIMITIVE_NULL) {
        EMIT("::nrforge::uper::Result<%s>::success(%s)", encode ? "void" : "std::monostate",
             encode ? "" : "std::monostate{}");
        return 0;
    }
    if (k == ASN1TYPED_PRIMITIVE_BOOLEAN) {
        if (encode)
            EMIT("io.write_bits(%s,1)", value);
        else
            EMIT("io.read_bits(1)");
        return 0;
    }
    if (k == ASN1TYPED_PRIMITIVE_INTEGER)
        return integer_expr(b, range_for(p, l), encode, value);
    EMIT("io.%s_%s(", encode ? "write" : "read",
         k == ASN1TYPED_PRIMITIVE_BIT_STRING ? "bit_string" : "octets");
    if (encode)
        EMIT("%s,", value);
    if (size_expr(b, size_for(p, l)))
        return -1;
    EMIT(")");
    return 0;
}
static int member_put(struct buf *b, const struct plan *p, const struct link *l, const char *expr) {
    int override = l->size->has_size_constraint || l->range->has_value_range;
    if (l->node != SIZE_MAX && !override) {
        EMIT("take(put%zu(io,%s));\n", l->node, expr);
        return 0;
    }
    EMIT("take(");
    struct buf value = {0};
    if (fmt(&value, "%s%s", expr, l->node != SIZE_MAX ? ".value" : ""))
        return -1;
    int rc = primitive_expr(b, p, l, 1, value.data);
    free(value.data);
    if (rc)
        return -1;
    EMIT(");\n");
    return 0;
}
static int member_get(struct buf *b, const struct plan *p, const struct link *l) {
    int override = l->size->has_size_constraint || l->range->has_value_range;
    if (l->node != SIZE_MAX && !override) {
        EMIT("take(get%zu(io))", l->node);
        return 0;
    }
    if (l->node != SIZE_MAX)
        EMIT("T%zu{", l->node);
    if (kind(p, l) == ASN1TYPED_PRIMITIVE_BOOLEAN)
        EMIT("static_cast<bool>(");
    EMIT("take(");
    if (primitive_expr(b, p, l, 0, ""))
        return -1;
    EMIT(")");
    if (kind(p, l) == ASN1TYPED_PRIMITIVE_BOOLEAN)
        EMIT(")");
    if (l->node != SIZE_MAX)
        EMIT("}");
    return 0;
}
static int present(struct buf *b, const asn1typed_type_t *t, const struct link *l, size_t j) {
    if (l->def)
        EMIT("(v.f_%s != %s)", l->name, l->def);
    else if (optional(t, j))
        EMIT("v.f_%s.has_value()", l->name);
    else
        EMIT("true");
    return 0;
}
static int field_put(struct buf *b, const struct plan *p, const struct link *l, int opt) {
    char expr[512];
    snprintf(expr, sizeof(expr), "%sv.f_%s%s", opt ? "(" : "", l->name, opt ? ".value())" : "");
    return member_put(b, p, l, expr);
}
static int sequence(struct buf *b, const struct plan *p, size_t i, int encode) {
    const asn1typed_type_t *t = p->nodes[i].type;
    size_t roots = t->ordinary_root_count, adds = t->ordinary_addition_count;
    if (encode) {
        if (t->is_extensible) {
            EMIT("bool extended=false;\n");
            for (size_t j = roots; j < t->field_count; ++j) {
                EMIT("extended=extended||");
                if (present(b, t, &p->nodes[i].members[j], j))
                    return -1;
                EMIT(";\n");
            }
            EMIT("take(io.write_bits(extended,1));\n");
        }
        for (size_t j = 0; j < roots; ++j) {
            const struct link *l = &p->nodes[i].members[j];
            if (optional(t, j) || l->def) {
                EMIT("const bool p%zu=", j);
                if (present(b, t, l, j))
                    return -1;
                EMIT(";take(io.write_bits(p%zu,1));\n", j);
            }
        }
        for (size_t j = 0; j < roots; ++j) {
            const struct link *l = &p->nodes[i].members[j];
            if (optional(t, j) || l->def)
                EMIT("if(p%zu){\n", j);
            if (field_put(b, p, l, optional(t, j)))
                return -1;
            if (optional(t, j) || l->def)
                EMIT("}\n");
        }
        if (t->is_extensible && adds) {
            EMIT("if(extended){std::array<std::byte,%zu> bitmap{};\n", (adds + 7) / 8);
            for (size_t j = roots; j < t->field_count; ++j) {
                EMIT("if(");
                if (present(b, t, &p->nodes[i].members[j], j))
                    return -1;
                EMIT(")bitmap[%zu]|=std::byte{0x%02x};\n", (j - roots) / 8,
                     0x80u >> (unsigned)((j - roots) % 8));
            }
            EMIT("take(io.write_extension_bitmap(bitmap,%zu));\n", adds);
            for (size_t j = roots; j < t->field_count; ++j) {
                const struct link *l = &p->nodes[i].members[j];
                EMIT("if(");
                if (present(b, t, l, j))
                    return -1;
                EMIT(")take(io.write_known_open([&](::nrforge::uper::Writer& io){\n");
                if (field_put(b, p, l, optional(t, j)))
                    return -1;
                EMIT("return ::nrforge::uper::Result<void>::success();}));\n");
            }
            EMIT("}\n");
        }
    } else {
        if (t->is_extensible)
            EMIT("const bool extended=take(io.read_bits(1))!=0;\n");
        for (size_t j = 0; j < roots; ++j) {
            const struct link *l = &p->nodes[i].members[j];
            if (optional(t, j) || l->def)
                EMIT("const bool p%zu=take(io.read_bits(1))!=0;\n", j);
        }
        for (size_t j = 0; j < roots; ++j) {
            const struct link *l = &p->nodes[i].members[j];
            if (optional(t, j) || l->def)
                EMIT("if(p%zu)", j);
            EMIT("v.f_%s=", l->name);
            if (member_get(b, p, l))
                return -1;
            EMIT(";\n");
        }
        if (t->is_extensible) {
            EMIT("if(extended){auto bitmap=take(io.read_extension_bitmap());bool "
                 "any=false;for(std::size_t "
                 "k=0;k<bitmap.bit_count;++k){if((std::to_integer<unsigned>(bitmap.octets[k/"
                 "8])&(0x80u>>(k%%8)))==0)continue;any=true;switch(k){\n");
            for (size_t j = roots; j < t->field_count; ++j) {
                const struct link *l = &p->nodes[i].members[j];
                EMIT("case %zu:v.f_%s=take(io.read_known_open<", j - roots, l->name);
                if (spelling(b, p, l))
                    return -1;
                EMIT(">([&](::nrforge::uper::Reader& io){return ::nrforge::uper::Result<");
                if (spelling(b, p, l))
                    return -1;
                EMIT(">::success(");
                if (member_get(b, p, l))
                    return -1;
                EMIT(");}));break;\n");
            }
            EMIT("default:take(io.skip_open_type());break;}}if(!any)throw "
                 "::nrforge::uper::detail::Fault{{::nrforge::uper::ErrorCode::constraint_violation,"
                 "io.cursor_bit()}};}\n");
        }
    }
    return 0;
}
static int choice(struct buf *b, const struct plan *p, size_t i, int encode) {
    const asn1typed_type_t *t = p->nodes[i].type;
    if (encode) {
        for (size_t j = 0; j < t->alternative_count; ++j) {
            const asn1typed_choice_alternative_t *a = &t->alternatives[j];
            const struct link *l = &p->nodes[i].members[j];
            EMIT("if(auto "
                 "item=std::get_if<T%zu::A%zu>(&v.value)){(void)item;take(io.write_index({%s,%zu},%"
                 "zu,%s));\n",
                 i, j, a->is_extension_addition ? "true" : "false",
                 a->is_extension_addition ? a->per_extension_index : a->per_root_index,
                 t->ordinary_root_count, t->is_extensible ? "true" : "false");
            if (a->is_extension_addition)
                EMIT("take(io.write_known_open([&](::nrforge::uper::Writer& io){\n");
            if (member_put(b, p, l, "item->value"))
                return -1;
            if (a->is_extension_addition)
                EMIT("return ::nrforge::uper::Result<void>::success();}));\n");
            EMIT("return ::nrforge::uper::Result<void>::success();}\n");
        }
        if (t->is_extensible)
            EMIT(
                "if(auto "
                "item=std::get_if<Unknown>(&v.value)){if(known_index(item->index,%zu))throw "
                "::nrforge::uper::detail::Fault{{::nrforge::uper::ErrorCode::constraint_violation,"
                "io.cursor_bit()}};take(io.write_index({true,item->index},%zu,true));take(io.write_"
                "open_type(item->payload));return ::nrforge::uper::Result<void>::success();}\n",
                t->ordinary_addition_count, t->ordinary_root_count);
        EMIT("throw "
             "::nrforge::uper::detail::Fault{{::nrforge::uper::ErrorCode::constraint_violation,io."
             "cursor_bit()}};\n");
    } else {
        EMIT("auto index=take(io.read_index(%zu,%s));\n", t->ordinary_root_count,
             t->is_extensible ? "true" : "false");
        for (size_t j = 0; j < t->alternative_count; ++j) {
            const asn1typed_choice_alternative_t *a = &t->alternatives[j];
            const struct link *l = &p->nodes[i].members[j];
            EMIT("if(index.extension==%s&&index.index==%zu){v.value=T%zu::A%zu{",
                 a->is_extension_addition ? "true" : "false",
                 a->is_extension_addition ? a->per_extension_index : a->per_root_index, i, j);
            if (a->is_extension_addition) {
                EMIT("take(io.read_known_open<");
                if (spelling(b, p, l))
                    return -1;
                EMIT(">([&](::nrforge::uper::Reader& io){return ::nrforge::uper::Result<");
                if (spelling(b, p, l))
                    return -1;
                EMIT(">::success(");
                if (member_get(b, p, l))
                    return -1;
                EMIT(");}))");
            } else if (member_get(b, p, l))
                return -1;
            EMIT("};return ::nrforge::uper::Result<T%zu>::success(std::move(v));}\n", i);
        }
        if (t->is_extensible)
            EMIT("if(index.extension){v.value=Unknown{index.index,take(io.read_open_type(true))};"
                 "return ::nrforge::uper::Result<T%zu>::success(std::move(v));}\n",
                 i);
        EMIT("throw "
             "::nrforge::uper::detail::Fault{{::nrforge::uper::ErrorCode::constraint_violation,io."
             "cursor_bit()}};\n");
    }
    return 0;
}
static int collection(struct buf *b, const struct plan *p, size_t i, int encode) {
    const asn1typed_type_t *t = p->nodes[i].type;
    const asn1typed_size_constraint_t *sz = &t->size_constraint;
    const struct link *l = &p->nodes[i].members[0];
    int general = !sz->has_size_constraint || sz->upper_bound >= 65536;
    if (encode) {
        EMIT("auto count=v.value.size();\n");
        if (sz->has_size_constraint) {
            EMIT("bool outside=!in_size(count,%ju,%ju);\n", (uintmax_t)sz->lower_bound,
                 (uintmax_t)sz->upper_bound);
            if (sz->is_extensible)
                EMIT("take(io.write_bits(outside,1));\n");
            else
                EMIT("if(outside)throw "
                     "::nrforge::uper::detail::Fault{{::nrforge::uper::ErrorCode::constraint_"
                     "violation,io.cursor_bit()}};\n");
        }
        EMIT("std::size_t done=0;do{\n");
        if (general)
            EMIT("bool fragment=count-done>=16384;auto "
                 "n=fragment?std::min(std::size_t{4},(count-done)/"
                 "16384)*16384:count-done;take(io.write_collection_segment(n,fragment));\n");
        else if (sz->is_extensible)
            EMIT("bool fragment=outside&&count-done>=16384;auto "
                 "n=fragment?std::min(std::size_t{4},(count-done)/"
                 "16384)*16384:count-done;if(outside)take(io.write_collection_segment(n,fragment));"
                 "else take(io.write_collection_length(n,%ju,%ju));\n",
                 (uintmax_t)sz->lower_bound, (uintmax_t)sz->upper_bound);
        else
            EMIT("bool fragment=false;auto n=count;take(io.write_collection_length(n,%ju,%ju));\n",
                 (uintmax_t)sz->lower_bound, (uintmax_t)sz->upper_bound);
        EMIT("for(std::size_t k=done;k<done+n;++k){\n");
        if (member_put(b, p, l, "v.value[k]"))
            return -1;
        EMIT("}done+=n;if(!fragment)break;}while(true);\n");
    } else {
        if (sz->is_extensible)
            EMIT("bool outside=take(io.read_bits(1))!=0;\n");
        EMIT("std::size_t total=0;do{\n");
        if (general)
            EMIT("auto segment=take(io.read_collection_segment());\n");
        else if (sz->is_extensible)
            EMIT(
                "auto segment=outside?take(io.read_collection_segment()) : "
                "::nrforge::uper::LengthSegment{take(io.read_collection_length(%ju,%ju)),false};\n",
                (uintmax_t)sz->lower_bound, (uintmax_t)sz->upper_bound);
        else
            EMIT("::nrforge::uper::LengthSegment "
                 "segment{take(io.read_collection_length(%ju,%ju)),false};\n",
                 (uintmax_t)sz->lower_bound, (uintmax_t)sz->upper_bound);
        EMIT("if(segment.count>std::numeric_limits<std::size_t>::max()-total)throw "
             "::nrforge::uper::detail::Fault{{::nrforge::uper::ErrorCode::resource_limit,io.cursor_"
             "bit()}};\ntotal+=segment.count;\n");
        if (sz->has_size_constraint)
            EMIT("if(%stotal>%ju)throw "
                 "::nrforge::uper::detail::Fault{{::nrforge::uper::ErrorCode::constraint_violation,"
                 "io.cursor_bit()}};\n",
                 sz->is_extensible ? "!outside&&" : "", (uintmax_t)sz->upper_bound);
        EMIT("using Element=typename "
             "decltype(v.value)::value_type;if(segment.count>std::numeric_limits<std::size_t>::max("
             ")/sizeof(Element))throw "
             "::nrforge::uper::detail::Fault{{::nrforge::uper::ErrorCode::resource_limit,io.cursor_"
             "bit()}};take(io.claim_value_octets(segment.count*sizeof(Element)));v.value.reserve("
             "total);for(std::size_t k=0;k<segment.count;++k)v.value.push_back(");
        if (member_get(b, p, l))
            return -1;
        EMIT(");\nif(!segment.fragmented)break;}while(true);\n");
        if (sz->has_size_constraint) {
            if (sz->is_extensible)
                EMIT("if(outside==(total>=%ju&&total<=%ju))throw "
                     "::nrforge::uper::detail::Fault{{::nrforge::uper::ErrorCode::constraint_"
                     "violation,io.cursor_bit()}};\n",
                     (uintmax_t)sz->lower_bound, (uintmax_t)sz->upper_bound);
            else
                EMIT("if(!in_size(total,%ju,std::numeric_limits<std::size_t>::max()))throw "
                     "::nrforge::uper::detail::Fault{{::nrforge::uper::ErrorCode::constraint_"
                     "violation,io.cursor_bit()}};\n",
                     (uintmax_t)sz->lower_bound);
        }
    }
    return 0;
}
static int codec(struct buf *b, const struct plan *p, size_t i, int encode) {
    const asn1typed_type_t *t = p->nodes[i].type;
    if (encode)
        EMIT("inline ::nrforge::uper::Result<void> put%zu(::nrforge::uper::Writer& stream,const "
             "T%zu& v){return stream.compound([&](::nrforge::uper::Writer& io){(void)io;(void)v;\n",
             i, i);
    else
        EMIT("inline ::nrforge::uper::Result<T%zu> get%zu(::nrforge::uper::Reader& stream){return "
             "stream.compound<T%zu>([&](::nrforge::uper::Reader& io){(void)io;T%zu v{};\n",
             i, i, i, i);
    if (t->kind == ASN1TYPED_TYPE_PRIMITIVE) {
        struct link l = {.node = SIZE_MAX,
                         .primitive = t->primitive_kind,
                         .size = &t->size_constraint,
                         .range = &t->value_range};
        if (encode) {
            if (member_put(b, p, &l, "v.value"))
                return -1;
        } else {
            EMIT("v.value=");
            if (member_get(b, p, &l))
                return -1;
            EMIT(";\n");
        }
    } else if (t->kind == ASN1TYPED_TYPE_ENUMERATED) {
        size_t roots = 0;
        for (size_t j = 0; j < t->enum_item_count; ++j)
            roots += !t->enum_items[j].is_extension_addition;
        if (encode)
            EMIT("take(io.write_index(v.value,%zu,%s));\n", roots,
                 t->is_extensible ? "true" : "false");
        else
            EMIT("v.value=take(io.read_index(%zu,%s));\n", roots,
                 t->is_extensible ? "true" : "false");
    } else if (t->kind == ASN1TYPED_TYPE_SEQUENCE) {
        if (sequence(b, p, i, encode))
            return -1;
    } else if (t->kind == ASN1TYPED_TYPE_CHOICE) {
        if (choice(b, p, i, encode))
            return -1;
    } else if (collection(b, p, i, encode))
        return -1;
    if (t->kind != ASN1TYPED_TYPE_CHOICE) {
        if (encode)
            EMIT("return ::nrforge::uper::Result<void>::success();\n");
        else
            EMIT("return ::nrforge::uper::Result<T%zu>::success(std::move(v));\n", i);
    }
    EMIT("});}\n");
    return 0;
}
int asn1typed_render_cpp_owned_uper(const asn1typed_module_t *g, const char *ns, char **out,
                                    char *error, size_t size) {
    struct plan p = {0};
    struct buf text = {0};
    struct buf *b = &text;
    int rc = -1;
    if (out)
        *out = NULL;
    if (error && size)
        error[0] = 0;
    if (!out || !ns || !asn1typed_render_cpp_safe_namespace(ns) ||
        !strncmp(ns, "nrforge::uper", 13) || !strcmp(ns, "nrforge")) {
        snprintf(p.why, sizeof(p.why), "invalid/reserved UPER namespace");
        goto end;
    }
    if (build(&p, g))
        goto end;
    if (fmt(b,
            "#pragma once\n#include <uper/runtime.hpp>\n#include <algorithm>\n#include "
            "<array>\n#include <limits>\n#include <string_view>\nnamespace %s {\nstruct Unknown "
            "{std::uint64_t index=0;std::vector<std::byte> payload;bool operator==(const "
            "Unknown&)const=default;};\ninline bool in_size(std::size_t n,std::size_t "
            "lower,std::size_t upper){return n>=lower&&n<=upper;}\ninline bool "
            "known_index(std::uint64_t n,std::size_t count){return n<count;}\ntemplate<class T>T "
            "take(::nrforge::uper::Result<T> r){if(!r)throw "
            "::nrforge::uper::detail::Fault{r.error()};return std::move(r).value();}\ninline void "
            "take(::nrforge::uper::Result<void> r){if(!r)throw "
            "::nrforge::uper::detail::Fault{r.error()};}\n",
            ns))
        goto end;
    for (size_t i = 0; i < p.n; ++i) {
        if (fmt(b, "inline constexpr std::string_view source%zu=", i))
            goto end;
        const asn1typed_type_t *t = p.nodes[i].type;
        struct buf id = {0};
        if (fmt(&id, "%s.%s", t->identity.module ? t->identity.module : "inline",
                t->identity.source_name ? t->identity.source_name : "enum")) {
            free(id.data);
            goto end;
        }
        int q = quoted(b, id.data);
        free(id.data);
        if (q || fmt(b, ";\n"))
            goto end;
    }
    for (size_t n = 0; n < p.n; ++n)
        if (model(b, &p, p.order[n]) || codec(b, &p, p.order[n], 1) || codec(b, &p, p.order[n], 0))
            goto end;
    if (fmt(b, "using Root=T0;\nstruct Decoded {Root value;std::size_t semantic_bits;};\ninline "
               "::nrforge::uper::Result<::nrforge::uper::CompleteEncoding> encode(const Root& "
               "value,::nrforge::uper::Limits limits={},::nrforge::uper::Completion "
               "completion=::nrforge::uper::Completion::octets){::nrforge::uper::Writer "
               "w(limits);auto r=put0(w,value);if(!r)return "
               "::nrforge::uper::Result<::nrforge::uper::CompleteEncoding>::failure(r.error());"
               "return w.finish(completion);}\ninline ::nrforge::uper::Result<Decoded> "
               "decode(std::span<const std::byte> bytes,::nrforge::uper::Limits "
               "limits={},std::optional<std::size_t> logical_bits={},::nrforge::uper::Completion "
               "completion=::nrforge::uper::Completion::octets){auto "
               "m=::nrforge::uper::Reader::make(bytes,limits,logical_bits);if(!m)return "
               "::nrforge::uper::Result<Decoded>::failure(m.error());auto "
               "r=std::move(m).value();auto value=get0(r);if(!value)return "
               "::nrforge::uper::Result<Decoded>::failure(value.error());auto "
               "n=r.cursor_bit();auto end=r.finish(completion);if(!end)return "
               "::nrforge::uper::Result<Decoded>::failure(end.error());return "
               "::nrforge::uper::Result<Decoded>::success(Decoded{std::move(value).value(),n});}\n}"
               " // namespace\n"))
        goto end;
    *out = text.data;
    text.data = NULL;
    rc = 0;
end:
    if (rc && error && size)
        snprintf(error, size, "%s", p.why[0] ? p.why : "UPER render allocation failure");
    clear(&p);
    free(text.data);
    return rc;
}
