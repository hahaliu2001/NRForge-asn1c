#include "asn1typed_render_cpp.h"
#include "asn1typed_render_cpp_internal.h"
#include "asn1typed_render_cpp_ioc_internal.h"
#include "asn1typed_render_cpp_inline_enum_internal.h"
#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
struct plan { struct type_plan *types; char *boolean_mapping, *boolean_put, *boolean_get, *octets_mapping, *octets_put, *octets_get; char *bits_mapping, *bits_put, *bits_get; int nulls, has_nulls; char *null_mapping, *null_put, *null_get; int extensible_sizes, characters, values, has_integers, bits, has_bits, octets, has_octets, extensions, has_extension_sequence, collections; const struct asn1typed_cpp_ioc_entry *ioc; const char *const *extra_names; size_t extra_count; };
static int
append(struct compound_buf *b, const char *text) {
	size_t n = strlen(text);
	char *p;
	if(n > SIZE_MAX - b->length - 1) return -1;
	p = (char *)realloc(b->text, b->length + n + 1);
	if(!p) return -1;
	memcpy(p + b->length, text, n + 1);
	b->text = p; b->length += n;
	return 0;
}
static int
format(struct compound_buf *b, const char *fmt, ...) {
	va_list args;
	int n;
	char *text;
	va_start(args, fmt); n = vsnprintf(NULL, 0, fmt, args); va_end(args);
	if(n < 0 || !(text = (char *)malloc((size_t)n + 1))) return -1;
	va_start(args, fmt); vsnprintf(text, (size_t)n + 1, fmt, args); va_end(args);
	n = append(b, text); free(text); return n;
}
static char *
join(const char *prefix, const char *name, const char *suffix) {
	size_t a = strlen(prefix), b = strlen(name), c = strlen(suffix);
	char *p;
	if(a > SIZE_MAX - b || a + b > SIZE_MAX - c - 1) return NULL;
	p = (char *)malloc(a + b + c + 1);
	if(p) { memcpy(p, prefix, a); memcpy(p + a, name, b); memcpy(p + a + b, suffix, c + 1); }
	return p;
}
static int
empty_ref(const asn1typed_type_ref_t *r) {
	return r->kind == ASN1TYPED_REF_NAMED && !r->module && !r->source_name &&
		!r->actuals && !r->actual_count && r->primitive_kind == ASN1TYPED_PRIMITIVE_INVALID;
}
static int
empty_size(const asn1typed_size_constraint_t *s) {
	return !s->has_size_constraint && !s->is_extensible && !s->lower_bound && !s->upper_bound && !s->has_extension_addition && !s->extension_lower_bound && !s->extension_upper_bound;
}
static int
empty_range(const asn1typed_integer_value_range_t *r) {
	return asn1typed_integer_unsigned_empty(r) && !r->has_value_range && !r->is_extensible && !r->lower_bound && !r->upper_bound && !r->tail && !r->tail_count && !r->extension_additions && !r->extension_addition_count;
}
static int
storage(size_t count, size_t capacity, const void *pointer) {
	return count <= capacity && !!capacity == !!pointer;
}
static int size_metadata_valid(const asn1typed_size_constraint_t *s) {
    if(s->has_size_constraint != 0 && s->has_size_constraint != 1) return 0;
    if(s->is_extensible != 0 && s->is_extensible != 1) return 0;
    if(s->has_extension_addition != 0 && s->has_extension_addition != 1) return 0;
    if(!s->has_extension_addition) return !s->extension_lower_bound && !s->extension_upper_bound;
    return s->has_size_constraint && s->is_extensible && s->extension_lower_bound >= 0 && s->extension_upper_bound >= s->extension_lower_bound;
}
static int
character_kind(asn1typed_primitive_kind_e kind) {
    return kind == ASN1TYPED_PRIMITIVE_PRINTABLE_STRING || kind == ASN1TYPED_PRIMITIVE_VISIBLE_STRING || kind == ASN1TYPED_PRIMITIVE_UTF8_STRING;
}
static const char *
character_spelling(asn1typed_primitive_kind_e kind) {
    return kind == ASN1TYPED_PRIMITIVE_UTF8_STRING ? "utf8" : kind == ASN1TYPED_PRIMITIVE_VISIBLE_STRING ? "visible" : "printable";
}
static int
shape(const asn1typed_type_t *t, int extensions, int collections, int octets, int bits, int characters, int nulls, int extensible_sizes) {
    if(!size_metadata_valid(&t->size_constraint)) return 0;
	if((!((collections && t->kind == ASN1TYPED_TYPE_SEQUENCE_OF) || ((octets || bits) && t->kind == ASN1TYPED_TYPE_PRIMITIVE && (t->primitive_kind == ASN1TYPED_PRIMITIVE_OCTET_STRING || (bits && t->primitive_kind == ASN1TYPED_PRIMITIVE_BIT_STRING) || (characters && character_kind(t->primitive_kind))))) && (!empty_size(&t->size_constraint) || !empty_ref(&t->element_type))) || !empty_ref(&t->ioc_container) ||
		t->has_ioc_table || t->ioc_object_set_is_extensible) return 0;
	if(t->kind != ASN1TYPED_TYPE_ENUMERATED && (t->enum_items || t->enum_item_count || t->enum_item_capacity || t->has_valid_per_enumeration_mapping)) return 0;
	if(t->kind != ASN1TYPED_TYPE_SEQUENCE && (t->fields || t->field_count || t->field_capacity)) return 0;
	if(t->kind != ASN1TYPED_TYPE_CHOICE && (t->alternatives || t->alternative_count || t->alternative_capacity || t->has_valid_per_root_mapping || t->choice_root_only_extension_owned)) return 0;
	if(t->kind != ASN1TYPED_TYPE_PRIMITIVE && t->primitive_kind != ASN1TYPED_PRIMITIVE_INVALID) return 0;
	if(t->kind != ASN1TYPED_TYPE_ENUMERATED && t->is_extensible && !(extensions && (t->kind == ASN1TYPED_TYPE_SEQUENCE || (t->kind == ASN1TYPED_TYPE_CHOICE && t->choice_root_only_extension_owned == 1)))) return 0;
	if(!(t->kind == ASN1TYPED_TYPE_PRIMITIVE && t->primitive_kind == ASN1TYPED_PRIMITIVE_INTEGER) && !empty_range(&t->value_range)) return 0;
	if(characters && t->kind == ASN1TYPED_TYPE_PRIMITIVE && character_kind(t->primitive_kind))
        return empty_ref(&t->element_type) && !t->size_constraint.has_extension_addition && (empty_size(&t->size_constraint) || (t->size_constraint.has_size_constraint == 1 &&
            (t->size_constraint.is_extensible == 0 || t->size_constraint.is_extensible == 1) &&
            t->size_constraint.lower_bound >= 0 && t->size_constraint.upper_bound >= t->size_constraint.lower_bound && t->size_constraint.upper_bound <= 65535));
	if(t->kind == ASN1TYPED_TYPE_PRIMITIVE) return (octets && t->primitive_kind == ASN1TYPED_PRIMITIVE_OBJECT_IDENTIFIER && empty_size(&t->size_constraint)) || (nulls && t->primitive_kind == ASN1TYPED_PRIMITIVE_NULL) || t->primitive_kind == ASN1TYPED_PRIMITIVE_BOOLEAN || t->primitive_kind == ASN1TYPED_PRIMITIVE_INTEGER ||
        ((octets || bits) && (t->primitive_kind == ASN1TYPED_PRIMITIVE_OCTET_STRING || (bits && t->primitive_kind == ASN1TYPED_PRIMITIVE_BIT_STRING)) && empty_ref(&t->element_type) &&
         (empty_size(&t->size_constraint) || (t->size_constraint.has_size_constraint == 1 && (!t->size_constraint.is_extensible || extensible_sizes) &&
          t->size_constraint.lower_bound >= 0 && t->size_constraint.upper_bound >= t->size_constraint.lower_bound && (t->size_constraint.upper_bound <= 65535 || (extensible_sizes && t->primitive_kind == ASN1TYPED_PRIMITIVE_BIT_STRING && !t->size_constraint.is_extensible && t->size_constraint.upper_bound <= 131072)))));
	if(t->kind == ASN1TYPED_TYPE_ENUMERATED) return storage(t->enum_item_count, t->enum_item_capacity, t->enum_items);
	if(t->kind == ASN1TYPED_TYPE_CHOICE) return t->alternative_count >= 1 && t->alternative_count <= 255 && storage(t->alternative_count, t->alternative_capacity, t->alternatives);
	if(t->kind == ASN1TYPED_TYPE_SEQUENCE) return storage(t->field_count, t->field_capacity, t->fields);
	if(collections && t->kind == ASN1TYPED_TYPE_SEQUENCE_OF) return t->size_constraint.has_size_constraint == 1 && !t->size_constraint.is_extensible && t->size_constraint.lower_bound >= 0 && t->size_constraint.upper_bound >= t->size_constraint.lower_bound && (uintmax_t)t->size_constraint.upper_bound <= SIZE_MAX;
	return 0;
}
static int
field_semantics(const asn1typed_field_t *f) {
	const asn1typed_class_field_relation_t *r = &f->class_field_relation;
	return f->type_semantics == ASN1TYPED_FIELD_FIXED_TYPE && !f->inline_enumerated &&
		!f->has_class_field_relation && !r->class_module && !r->class_source_name && !r->class_field_source_name &&
		!r->actual_index && !r->has_selector && !r->selector_source_name &&
		!f->ioc.symbolic_id && !f->ioc.has_numeric_id && !f->ioc.numeric_id && f->ioc.criticality == ASN1TYPED_CRITICALITY_REJECT &&
		(f->presence == ASN1TYPED_PRESENCE_MANDATORY || f->presence == ASN1TYPED_PRESENCE_OPTIONAL);
}
static void
clear(struct plan *p, const asn1typed_module_t *m) {
	size_t i, j;
	if(p->types) for(i = 0; i < m->type_count; ++i) {
		struct type_plan *t = &p->types[i];
		free(t->type); free(t->qualified_type); free(t->mapping); free(t->qualified_mapping); free(t->constraint);
		free(t->encode); free(t->decode); free(t->put); free(t->get); free(t->qualified_put); free(t->qualified_get); free(t->extension_member);
		free(t->unknown_wrapper); free(t->qualified_unknown_wrapper); free(t->value_member);
		if(t->members) for(j = 0; j < t->member_count; ++j) {
			free(t->members[j].value_name); free(t->members[j].value_mapping); free(t->members[j].value_put); free(t->members[j].value_get);
			free(t->members[j].size_name); free(t->members[j].size_mapping); free(t->members[j].size_put); free(t->members[j].size_get);
			free(t->members[j].name); free(t->members[j].wrapper); free(t->members[j].qualified_wrapper);
		}
		free(t->members);
	}
	free(p->null_mapping); free(p->null_put); free(p->null_get);
	free(p->bits_mapping); free(p->bits_put); free(p->bits_get);
	free(p->types); free(p->boolean_mapping); free(p->boolean_put); free(p->boolean_get); free(p->octets_mapping); free(p->octets_put); free(p->octets_get);
	memset(p, 0, sizeof(*p));
}
static char *
qualified(const char *ns, const char *scope, const char *name) {
	char *prefix = join("::", ns, "::"), *middle, *result;
	if(!prefix) return NULL;
	middle = join(prefix, scope, ""); free(prefix);
	if(!middle) return NULL;
	result = join(middle, name, ""); free(middle); return result;
}
static int
resolve(const asn1typed_module_t *m, struct plan *p, size_t owner,
		const asn1typed_type_ref_t *r, const asn1typed_integer_value_range_t *range, struct member_plan *member) {
	size_t i;
	if(r->actuals || r->actual_count) return -1;
	if(r->kind == ASN1TYPED_REF_PRIMITIVE) {
		if(r->module || r->source_name) return -1;
        if(p->octets && r->primitive_kind == ASN1TYPED_PRIMITIVE_OPEN_TYPE) {
            member->type = "::std::vector<::std::byte>";
            member->mapping = "::nrforge::aper::OpaqueOpenTypeMapping";
            member->put = "::nrforge::aper::write_private_open_payload";
            member->get = "::nrforge::aper::read_private_open_payload";
            return 0;
        }
        if(p->octets && r->primitive_kind == ASN1TYPED_PRIMITIVE_OBJECT_IDENTIFIER) {
            member->type = "::std::vector<::std::byte>";
            member->mapping = "::nrforge::aper::ObjectIdentifierMapping";
            member->put = "::nrforge::aper::write_object_identifier";
            member->get = "::nrforge::aper::read_object_identifier";
            return 0;
        }
        if((p->octets || p->bits) && r->primitive_kind == ASN1TYPED_PRIMITIVE_OCTET_STRING) {
            p->has_octets = 1; member->type = "::std::vector<::std::byte>"; member->mapping = p->octets_mapping;
            member->put = p->octets_put; member->get = p->octets_get; return 0;
        }
        if(p->bits && r->primitive_kind == ASN1TYPED_PRIMITIVE_BIT_STRING) {
            p->has_bits = 1; member->type = "::nrforge::aper::BitString"; member->mapping = p->bits_mapping;
            member->put = p->bits_put; member->get = p->bits_get; return 0;
        }
        if(p->values && r->primitive_kind == ASN1TYPED_PRIMITIVE_INTEGER && range && range->has_value_range == 1) {
            member->type = range->lower_bound < 0 || (p->values > 1 && (range->is_extensible || range->tail_count)) ? "::std::int64_t" : "::std::uint64_t";
            return 0;
        }
        if(p->nulls && r->primitive_kind == ASN1TYPED_PRIMITIVE_NULL) {
            p->has_nulls = 1; member->type = "::std::monostate"; member->mapping = p->null_mapping;
            member->put = p->null_put; member->get = p->null_get; return 0;
        }
        if(r->primitive_kind != ASN1TYPED_PRIMITIVE_BOOLEAN) return -1;
		member->type = "bool"; member->mapping = p->boolean_mapping;
		member->put = p->boolean_put; member->get = p->boolean_get;
		return 0;
	}
	if(r->kind != ASN1TYPED_REF_NAMED || r->primitive_kind != ASN1TYPED_PRIMITIVE_INVALID ||
		!r->module || strcmp(r->module, m->source_name) || !r->source_name) return -1;
	for(i = 0; i < owner; ++i) if(!strcmp(m->types[i].identity.source_name, r->source_name)) break;
	if(i >= owner) return -1;
	member->type = p->types[i].qualified_type; member->mapping = p->types[i].qualified_mapping;
	member->put = p->types[i].qualified_put; member->get = p->types[i].qualified_get;
	return 0;
}
static int
legacy_uint(const asn1typed_type_t *t) {
    const asn1typed_integer_value_range_t *r = &t->value_range;
    return t->kind == ASN1TYPED_TYPE_PRIMITIVE && t->primitive_kind == ASN1TYPED_PRIMITIVE_INTEGER &&
        asn1typed_integer_unsigned_empty(r) && r->has_value_range == 1 && !r->lower_bound && !r->is_extensible && !r->tail && !r->tail_count && !r->extension_additions && !r->extension_addition_count &&
        (r->upper_bound == 255 || r->upper_bound == 65535 || r->upper_bound == INT64_C(4294967295) || r->upper_bound == INT64_C(1099511627775));
}
typedef int (*renderer)(const asn1typed_module_t *, const char *, char **, char *, size_t);
static renderer
delegate(const asn1typed_type_t *t, int mode, int values) {
	if(t->kind == ASN1TYPED_TYPE_ENUMERATED) return mode == 0 ? asn1typed_render_cpp_owned_enum_types : mode == 1 ? asn1typed_render_cpp_owned_enum_mapping : asn1typed_render_cpp_owned_enum_codec;
    if(values > 1 && (t->value_range.is_extensible || t->value_range.tail_count)) return mode == 0 ? asn1typed_render_cpp_owned_integer_set_types : mode == 1 ? asn1typed_render_cpp_owned_integer_set_mapping : asn1typed_render_cpp_owned_integer_set_codec;
    if(values && !legacy_uint(t)) return mode == 0 ? asn1typed_render_cpp_owned_integer_types : mode == 1 ? asn1typed_render_cpp_owned_integer_mapping : asn1typed_render_cpp_owned_integer_codec;
	return mode == 0 ? asn1typed_render_cpp_owned_uint_types : mode == 1 ? asn1typed_render_cpp_owned_uint_mapping : asn1typed_render_cpp_owned_uint_codec;
}
static int
primitive(const asn1typed_type_t *t) {
	return t->kind == ASN1TYPED_TYPE_ENUMERATED || (t->kind == ASN1TYPED_TYPE_PRIMITIVE && t->primitive_kind == ASN1TYPED_PRIMITIVE_INTEGER);
}
static int
single_view(const asn1typed_module_t *m, size_t i, const char *ns, int mode, char **text, char *why, size_t size, int values) {
	asn1typed_module_t view = *m;
	view.types = &m->types[i]; view.type_count = 1; view.type_capacity = 1;
	return delegate(&m->types[i], mode, values)(&view, ns, text, why, size);
}
static int
register_symbol(const char *symbol, const char ***list, size_t *count, char *why, size_t size) {
	const char **next;
	size_t i;
	if(asn1typed_render_cpp_header_macro(symbol)) {
		if(why && size) snprintf(why, size, "compound final spelling conflicts with a standard header macro");
		return -1;
	}
	for(i = 0; i < *count; ++i) if(!strcmp(symbol, (*list)[i])) {
		if(why && size) snprintf(why, size, "compound generated final name collision: %s", symbol);
		return -1;
	}
	if(*count >= SIZE_MAX / sizeof(*next) - 1) {
		if(why && size) snprintf(why, size, "compound symbol allocation size overflow");
		return -1;
	}
	next = (const char **)realloc((void *)*list, (*count + 1) * sizeof(*next));
	if(!next) {
		if(why && size) snprintf(why, size, "out of memory planning compound symbols");
		return -1;
	}
	*list = next; next[(*count)++] = symbol; return 0;
}
static int
preflight(const asn1typed_module_t *m, const char *ns, struct plan *p, char *why, size_t size) {
	size_t i, j, k, symbol_count = 0;
	const char **symbols = NULL;
	static const char *const reserved[] = {"enum_codec", "uint_codec", "compound_codec", "COMPOUND_BOOLEAN"};
#define FAIL(message) do { snprintf(why, size, "%s", message); goto fail; } while(0)
	if(!m || !m->source_name || !m->source_name[0] || !m->types || !m->type_count || m->type_count > m->type_capacity ||
		m->type_count > SIZE_MAX / sizeof(*p->types) || m->bound_instances || m->bound_instance_count || m->bound_instance_capacity || m->ioc_registries || m->ioc_registry_count || m->ioc_registry_capacity)
		FAIL("compound module has invalid storage or unsupported bound instances");
	if(!asn1typed_render_cpp_safe_namespace(ns)) FAIL("invalid or unsafe compound namespace");
	if(p->extensions && (!strcmp(ns, "nrforge::aper") || !strncmp(ns, "nrforge::aper::", sizeof("nrforge::aper::") - 1)))
		FAIL("sequence extension namespace overlaps reserved runtime namespace nrforge::aper");
	if(p->nulls) {
        p->null_mapping = qualified(ns, "", "COMPOUND_NULL");
        p->null_put = qualified(ns, "compound_codec::", "put_null");
        p->null_get = qualified(ns, "compound_codec::", "get_null");
        if(!p->null_mapping || !p->null_put || !p->null_get) FAIL("out of memory planning NULL helpers");
        if(register_symbol("COMPOUND_NULL", &symbols, &symbol_count, why, size)) goto fail;
    }
    p->types = (struct type_plan *)calloc(m->type_count, sizeof(*p->types));
	p->boolean_mapping = qualified(ns, "", "COMPOUND_BOOLEAN");
	p->boolean_put = qualified(ns, "compound_codec::", "put_boolean");
	p->boolean_get = qualified(ns, "compound_codec::", "get_boolean");
    if(p->bits) {
        p->bits_mapping = qualified(ns, "", "COMPOUND_BITS");
        p->bits_put = qualified(ns, "compound_codec::", "put_bits");
        p->bits_get = qualified(ns, "compound_codec::", "get_bits");
        if(!p->bits_mapping || !p->bits_put || !p->bits_get) FAIL("out of memory planning bit helpers");
    }
    if(p->octets || p->bits) {
        p->octets_mapping = qualified(ns, "", "COMPOUND_OCTETS");
        p->octets_put = qualified(ns, "compound_codec::", "put_octets");
        p->octets_get = qualified(ns, "compound_codec::", "get_octets");
        if(!p->octets_mapping || !p->octets_put || !p->octets_get) FAIL("out of memory planning octet helpers");
    }
	if(!p->types || !p->boolean_mapping || !p->boolean_put || !p->boolean_get) FAIL("out of memory planning compound output");
	for(i = 0; i < sizeof(reserved) / sizeof(reserved[0]); ++i)
		if(register_symbol(reserved[i], &symbols, &symbol_count, why, size)) goto fail;
	for(i = 0; i < m->type_count; ++i) {
        const asn1typed_type_t *t = &m->types[i];
		struct type_plan *tp = &p->types[i];
		char *base;
		const char *scope = t->kind == ASN1TYPED_TYPE_ENUMERATED ? "enum_codec::" :
			(t->kind == ASN1TYPED_TYPE_PRIMITIVE && t->primitive_kind == ASN1TYPED_PRIMITIVE_INTEGER) ? (p->values && !legacy_uint(t) ? "integer_codec::" : "uint_codec::") : "compound_codec::";
		if(p->extensions && !(t->kind == ASN1TYPED_TYPE_SEQUENCE && t->is_extensible) &&
			(t->sequence_extension_evidence != ASN1TYPED_WIRE_EVIDENCE_UNAVAILABLE || t->sequence_root_field_count || t->sequence_known_addition_count || t->has_valid_sequence_extension_structure)) FAIL("unexpected SEQUENCE extension structure metadata");
		if(!shape(t, p->extensions, p->collections, p->octets, p->bits, p->characters, p->nulls, p->extensible_sizes)) {
            snprintf(why, size, "unsupported compound type shape, storage or metadata: %s.%s (kind=%u primitive=%u SIZE=%d/%" PRIdMAX "..%" PRIdMAX " extensible=%d)",
                t->identity.module ? t->identity.module : "?", t->identity.source_name ? t->identity.source_name : "?",
                (unsigned)t->kind, (unsigned)t->primitive_kind, t->size_constraint.has_size_constraint,
                t->size_constraint.lower_bound, t->size_constraint.upper_bound, t->size_constraint.is_extensible);
            goto fail;
        }
        if(t->kind == ASN1TYPED_TYPE_PRIMITIVE && t->primitive_kind == ASN1TYPED_PRIMITIVE_NULL) p->has_nulls = 1;
        if(t->kind == ASN1TYPED_TYPE_PRIMITIVE && t->primitive_kind == ASN1TYPED_PRIMITIVE_OCTET_STRING) p->has_octets = 1;
        if(t->kind == ASN1TYPED_TYPE_PRIMITIVE && t->primitive_kind == ASN1TYPED_PRIMITIVE_BIT_STRING) p->has_bits = 1;
		if(!t->identity.module || strcmp(t->identity.module, m->source_name) || !t->identity.source_name || !t->identity.source_name[0]) FAIL("invalid compound type identity");
		tp->type = asn1typed_render_cpp_final_name(t->identity.source_name, ASN1TYPED_NAME_TYPE);
		base = asn1typed_render_cpp_final_name(t->identity.source_name, ASN1TYPED_NAME_FIELD);
		if(!tp->type || !base) { free(base); FAIL("invalid compound name or out of memory naming type"); }
		tp->qualified_type = qualified(ns, "", tp->type);
		tp->mapping = join("", tp->type, "_aper");
		tp->constraint = t->kind == ASN1TYPED_TYPE_PRIMITIVE && t->primitive_kind == ASN1TYPED_PRIMITIVE_INTEGER ? join("", tp->type, "_constraint") : NULL;
		tp->qualified_mapping = tp->mapping ? qualified(ns, "", tp->mapping) : NULL;
		tp->encode = join("encode_", base, ""); tp->decode = join("decode_", base, ""); free(base);
		tp->put = join("put_", tp->type, ""); tp->get = join("get_", tp->type, "");
		tp->qualified_put = tp->put ? qualified(ns, scope, tp->put) : NULL;
		tp->qualified_get = tp->get ? qualified(ns, scope, tp->get) : NULL;
		if(!tp->qualified_type || !tp->mapping || !tp->qualified_mapping || !tp->encode || !tp->decode || !tp->put || !tp->get || !tp->qualified_put || !tp->qualified_get ||
			(t->kind == ASN1TYPED_TYPE_PRIMITIVE && t->primitive_kind == ASN1TYPED_PRIMITIVE_INTEGER && !tp->constraint)) FAIL("out of memory constructing compound names");
		if(register_symbol(tp->type, &symbols, &symbol_count, why, size) || register_symbol(tp->mapping, &symbols, &symbol_count, why, size) ||
			register_symbol(tp->encode, &symbols, &symbol_count, why, size) || register_symbol(tp->decode, &symbols, &symbol_count, why, size) ||
			(tp->constraint && register_symbol(tp->constraint, &symbols, &symbol_count, why, size))) goto fail;
		/* Qualified helper names compare only within their actual namespace. */
		if(register_symbol(tp->qualified_put, &symbols, &symbol_count, why, size) || register_symbol(tp->qualified_get, &symbols, &symbol_count, why, size)) goto fail;
		if(primitive(t)) {
			char *text = NULL;
			if(single_view(m, i, ns, 0, &text, why, size, p->values)) { free(text); goto fail; }
			free(text); if(p->values && t->kind == ASN1TYPED_TYPE_PRIMITIVE && !legacy_uint(t)) p->has_integers = 1; continue;
		}
		if(t->kind == ASN1TYPED_TYPE_SEQUENCE && t->is_extensible) {
			if(asn1typed_sequence_extension_structure_validate(t, why, size)) goto fail;
			p->has_extension_sequence = 1;
		}
		if(t->kind == ASN1TYPED_TYPE_CHOICE) {
			if(t->has_valid_per_root_mapping != 1) FAIL("CHOICE wire mapping has not been finalized");
			for(j = 0; j < t->alternative_count; ++j)
				if(t->alternatives[j].has_per_root_index != 1) FAIL("CHOICE has missing or invalid PER index flag");
			if(asn1typed_choice_wire_evidence_validate(t, why, size)) goto fail;
		}
		tp->member_count = t->kind == ASN1TYPED_TYPE_CHOICE ? t->alternative_count : t->kind == ASN1TYPED_TYPE_SEQUENCE_OF ? 1 : t->field_count;
		if(tp->member_count > SIZE_MAX / sizeof(*tp->members)) FAIL("compound member allocation size overflow");
		if(tp->member_count) tp->members = (struct member_plan *)calloc(tp->member_count, sizeof(*tp->members));
		if(tp->member_count && !tp->members) FAIL("out of memory planning compound members");
		for(j = 0; j < tp->member_count; ++j) {
			struct member_plan *mp = &tp->members[j];
			const asn1typed_type_ref_t *ref;
            const asn1typed_size_constraint_t *site_size = NULL;
            const asn1typed_integer_value_range_t *site_range = NULL;
			const char *source;
			if(t->kind == ASN1TYPED_TYPE_CHOICE) {
				const asn1typed_choice_alternative_t *a = &t->alternatives[j];
				if(a->inline_enumerated || (!p->bits && !empty_size(&a->size_constraint)) || (!p->values && !empty_range(&a->value_range))) FAIL("unsupported CHOICE alternative constraint metadata");
				ref = &a->type_ref; source = a->source_name; site_size = &a->size_constraint; site_range = &a->value_range;
			} else if(t->kind == ASN1TYPED_TYPE_SEQUENCE_OF) {
				ref = &t->element_type; source = "elements";
			} else {
				const asn1typed_field_t *f = &t->fields[j];
				if(!field_semantics(f) || (!p->bits && !empty_size(&f->size_constraint)) || (!p->values && !empty_range(&f->value_range))) FAIL("unsupported SEQUENCE field semantics or metadata");
				ref = &f->type; source = f->source_name; site_size = &f->size_constraint; site_range = &f->value_range;
			}
			if(!source || !source[0]) FAIL("compound member has missing source name");
			mp->name = asn1typed_render_cpp_final_name(source, ASN1TYPED_NAME_FIELD);
			if(!mp->name || (asn1typed_render_cpp_header_macro(mp->name) && !(p->ioc && p->ioc[i].registry && j >= 2))) FAIL("compound member spelling is unsafe, a standard header macro, or unavailable");
			for(k = (p->ioc && p->ioc[i].registry && j >= 2) ? 2 : 0; k < j; ++k) if(!strcmp(mp->name, tp->members[k].name)) FAIL("compound member final name collision");
			if(resolve(m, p, i, ref, site_range, mp)) FAIL("unsupported, missing, external, forward or recursive compound reference");
            if(site_range && !empty_range(site_range)) {
                size_t target = 0;
                const asn1typed_integer_value_range_t *decl_range = NULL;
                char ordinal[3 * sizeof(size_t) + 32], *local;
                if(!p->values || site_range->has_value_range != 1 || (site_range->is_extensible && p->values < 2) ||
                   (site_range->is_extensible != 0 && site_range->is_extensible != 1) ||
                   (p->values < 2 && (site_range->tail || site_range->tail_count || site_range->extension_additions || site_range->extension_addition_count)) ||
                   (!!site_range->extension_additions != !!site_range->extension_addition_count) ||
                   (site_range->extension_addition_count && !site_range->is_extensible) ||
                   site_range->extension_addition_count > SIZE_MAX / sizeof(*site_range->extension_additions) ||
                   (!!site_range->tail != !!site_range->tail_count) || site_range->tail_count > SIZE_MAX / sizeof(*site_range->tail) ||
                   site_range->lower_bound > site_range->upper_bound ||
                   (site_range->unsigned_bounds ? !asn1typed_integer_unsigned_valid(site_range) :
                    (!asn1typed_integer_unsigned_empty(site_range) || site_range->lower_bound < INT64_MIN || site_range->upper_bound > INT64_MAX)) ||
                   (site_size && !empty_size(site_size))) FAIL("unsupported INTEGER use-site interval metadata");
                if(ref->kind == ASN1TYPED_REF_NAMED) {
                    if(site_range->is_extensible || site_range->tail_count) FAIL("unsupported extensible/set INTEGER named use-site refinement");
                    for(target = 0; target < i; ++target) if(!strcmp(m->types[target].identity.source_name, ref->source_name)) break;
                    if(target == i || m->types[target].kind != ASN1TYPED_TYPE_PRIMITIVE || m->types[target].primitive_kind != ASN1TYPED_PRIMITIVE_INTEGER)
                        FAIL("INTEGER interval use-site requires INTEGER reference");
                    decl_range = &m->types[target].value_range;
                    if(decl_range->has_value_range != 1 || decl_range->is_extensible || decl_range->tail || decl_range->tail_count || decl_range->extension_additions || decl_range->extension_addition_count ||
                       (site_range->unsigned_bounds || decl_range->unsigned_bounds ?
                         (decl_range->lower_bound < 0 ||
                          (site_range->unsigned_bounds ? site_range->unsigned_lower_bound : (uint64_t)site_range->lower_bound) <
                          (decl_range->unsigned_bounds ? decl_range->unsigned_lower_bound : (uint64_t)decl_range->lower_bound) ||
                          (site_range->unsigned_bounds ? site_range->unsigned_upper_bound : (uint64_t)site_range->upper_bound) >
                          (decl_range->unsigned_bounds ? decl_range->unsigned_upper_bound : (uint64_t)decl_range->upper_bound)) :
                         (site_range->lower_bound < decl_range->lower_bound || site_range->upper_bound > decl_range->upper_bound)))
                        FAIL("INTEGER use-site interval is not a subset of named declaration");
                } else if(ref->primitive_kind != ASN1TYPED_PRIMITIVE_INTEGER) FAIL("INTEGER interval use-site requires INTEGER primitive");
                {
                    size_t z;
                    for(z = 0; z < site_range->extension_addition_count; ++z) {
                        const asn1typed_integer_interval_t *a = &site_range->extension_additions[z];
                        if(a->lower_bound < INT64_MIN || a->upper_bound > INT64_MAX || a->lower_bound > a->upper_bound ||
                            (z && (site_range->extension_additions[z-1].upper_bound == INTMAX_MAX || a->lower_bound <= site_range->extension_additions[z-1].upper_bound + 1))) FAIL("invalid INTEGER use-site known additions");
                    }
                    intmax_t previous = site_range->upper_bound;
                    size_t n;
                    for(n = 0; n < site_range->tail_count; ++n) {
                        const asn1typed_integer_interval_t *interval = &site_range->tail[n];
                        if(interval->lower_bound < INT64_MIN || interval->upper_bound > INT64_MAX || interval->lower_bound > interval->upper_bound ||
                            previous == INT64_MAX || interval->lower_bound <= previous + 1) FAIL("invalid canonical INTEGER use-site root set");
                        previous = interval->upper_bound;
                    }
                }
                mp->range = *site_range; mp->value_signed = site_range->is_extensible || site_range->tail_count || (decl_range ? decl_range->lower_bound : site_range->lower_bound) < 0;
                snprintf(ordinal, sizeof(ordinal), "_member_%zu_value_aper", j);
                mp->value_name = join("", tp->type, ordinal);
                mp->value_mapping = mp->value_name ? qualified(ns, "", mp->value_name) : NULL;
                snprintf(ordinal, sizeof(ordinal), "_member_%zu_value", j);
                local = join("put_", tp->type, ordinal);
                mp->value_put = local ? qualified(ns, "compound_codec::", local) : NULL; free(local);
                local = join("get_", tp->type, ordinal);
                mp->value_get = local ? qualified(ns, "compound_codec::", local) : NULL; free(local);
                if(!mp->value_name || !mp->value_mapping || !mp->value_put || !mp->value_get) FAIL("out of memory planning INTEGER interval helpers");
                if(register_symbol(mp->value_name, &symbols, &symbol_count, why, size) || register_symbol(mp->value_put, &symbols, &symbol_count, why, size) ||
                   register_symbol(mp->value_get, &symbols, &symbol_count, why, size)) goto fail;
                mp->mapping = mp->value_mapping; mp->put = mp->value_put; mp->get = mp->value_get;
                p->has_integers = 1;
            }
            if(site_size && !empty_size(site_size)) {
                size_t target = 0;
                asn1typed_primitive_kind_e kind = ref->primitive_kind;
                const asn1typed_size_constraint_t *decl_size = NULL;
                char ordinal[3 * sizeof(size_t) + 32], *local;
                if(!size_metadata_valid(site_size) || !p->bits || site_size->has_size_constraint != 1 || (site_size->is_extensible && !p->extensible_sizes) ||
                   site_size->lower_bound < 0 || site_size->upper_bound < site_size->lower_bound || site_size->upper_bound > (p->extensible_sizes ? 131072 : 65535))
                    FAIL("unsupported use-site SIZE metadata");
                if(ref->kind == ASN1TYPED_REF_NAMED) {
                    for(target = 0; target < i; ++target) if(!strcmp(m->types[target].identity.source_name, ref->source_name)) break;
                    if(target == i || m->types[target].kind != ASN1TYPED_TYPE_PRIMITIVE) FAIL("SIZE use-site requires primitive BIT/OCTET reference");
                    kind = m->types[target].primitive_kind; decl_size = &m->types[target].size_constraint;
                }
                if(kind != ASN1TYPED_PRIMITIVE_BIT_STRING && kind != ASN1TYPED_PRIMITIVE_OCTET_STRING)
                    FAIL("SIZE use-site requires BIT/OCTET STRING");
                if(site_size->upper_bound > 65535 && (kind != ASN1TYPED_PRIMITIVE_BIT_STRING || site_size->is_extensible))
                    FAIL("wide SIZE requires nonextensible BIT STRING");
                if(decl_size && decl_size->has_size_constraint &&
                   ((decl_size->is_extensible != site_size->is_extensible && site_size->is_extensible) || site_size->lower_bound < decl_size->lower_bound || site_size->upper_bound > decl_size->upper_bound))
                    FAIL("use-site SIZE is not a subset of named declaration");
                if(decl_size && decl_size->is_extensible && site_size->is_extensible &&
                   (decl_size->lower_bound != site_size->lower_bound || decl_size->upper_bound != site_size->upper_bound ||
                    decl_size->has_extension_addition != site_size->has_extension_addition ||
                    decl_size->extension_lower_bound != site_size->extension_lower_bound || decl_size->extension_upper_bound != site_size->extension_upper_bound))
                    FAIL("named extensible SIZE root/addition evidence differs from declaration");
                mp->size = *site_size; mp->size_kind = kind;
                snprintf(ordinal, sizeof(ordinal), "_member_%zu_size_aper", j);
                local = join("", tp->type, ordinal);
                if(!local) FAIL("out of memory naming SIZE mapping");
                mp->size_name = local; mp->size_mapping = qualified(ns, "", local);
                if(register_symbol(local, &symbols, &symbol_count, why, size)) goto fail;
                snprintf(ordinal, sizeof(ordinal), "_member_%zu_size", j);
                local = join("put_", tp->type, ordinal);
                mp->size_put = local ? qualified(ns, "compound_codec::", local) : NULL; free(local);
                local = join("get_", tp->type, ordinal);
                mp->size_get = local ? qualified(ns, "compound_codec::", local) : NULL; free(local);
                if(!mp->size_mapping || !mp->size_put || !mp->size_get) FAIL("out of memory naming SIZE helpers");
                if(register_symbol(mp->size_put, &symbols, &symbol_count, why, size) || register_symbol(mp->size_get, &symbols, &symbol_count, why, size)) goto fail;
                mp->mapping = mp->size_mapping; mp->put = mp->size_put; mp->get = mp->size_get;
            }
			if(t->kind == ASN1TYPED_TYPE_CHOICE || (p->ioc && p->ioc[i].registry && j >= 2)) {
				char *prefix = join("", tp->type, "_");
				if(!prefix) FAIL("out of memory naming CHOICE wrapper");
				mp->wrapper = join(prefix, mp->name, ""); free(prefix);
				mp->qualified_wrapper = mp->wrapper ? qualified(ns, "", mp->wrapper) : NULL;
				if(!mp->wrapper || !mp->qualified_wrapper) FAIL("out of memory qualifying CHOICE wrapper");
				if(register_symbol(mp->wrapper, &symbols, &symbol_count, why, size)) goto fail;
			}
		}
		if(p->ioc && p->ioc[i].registry) {
			tp->value_member = asn1typed_render_cpp_final_name(p->ioc[i].value_source_name, ASN1TYPED_NAME_FIELD);
			tp->unknown_wrapper = join("", tp->type, "_unknown");
			tp->qualified_unknown_wrapper = tp->unknown_wrapper ? qualified(ns, "", tp->unknown_wrapper) : NULL;
			if(!tp->value_member || asn1typed_render_cpp_header_macro(tp->value_member) || !tp->unknown_wrapper || !tp->qualified_unknown_wrapper) FAIL("invalid or unsafe IOC value name or allocation failure");
			if(!strcmp(tp->value_member, tp->type) || !strcmp(tp->value_member, tp->members[0].name) || !strcmp(tp->value_member, tp->members[1].name)) FAIL("IOC physical member name collision");
			if(register_symbol(tp->unknown_wrapper, &symbols, &symbol_count, why, size)) goto fail;
		}
		if(t->kind == ASN1TYPED_TYPE_SEQUENCE && t->is_extensible) {
			size_t suffix = 0;
			for(;;) {
				char ending[3 * sizeof(size_t) + 2];
				int collision = 0;
				if(suffix) snprintf(ending, sizeof(ending), "_%zu", suffix);
				else ending[0] = 0;
				tp->extension_member = join("", "sequence_extensions", ending);
				if(!tp->extension_member) FAIL("out of memory naming SEQUENCE extension sidecar");
				if(!strcmp(tp->extension_member, tp->type)) collision = 1;
				for(j = 0; j < tp->member_count; ++j)
					if(!strcmp(tp->extension_member, tp->members[j].name)) collision = 1;
				if(!collision) break;
				free(tp->extension_member); tp->extension_member = NULL;
				if(suffix == SIZE_MAX) FAIL("SEQUENCE extension member suffix overflow");
				++suffix;
			}
		}

	}
    if(p->has_integers && register_symbol("integer_codec", &symbols, &symbol_count, why, size)) goto fail;
    if(p->has_bits && (register_symbol("COMPOUND_BITS", &symbols, &symbol_count, why, size) ||
        register_symbol(p->bits_put, &symbols, &symbol_count, why, size) ||
        register_symbol(p->bits_get, &symbols, &symbol_count, why, size))) goto fail;
    if(p->has_octets && (register_symbol("COMPOUND_OCTETS", &symbols, &symbol_count, why, size) ||
        register_symbol(p->octets_put, &symbols, &symbol_count, why, size) ||
        register_symbol(p->octets_get, &symbols, &symbol_count, why, size))) goto fail;
	for(i = 0; i < p->extra_count; ++i)
		if(register_symbol(p->extra_names[i], &symbols, &symbol_count, why, size)) goto fail;
	free(symbols);
#undef FAIL
	return 0;
fail:
	free(symbols);
#undef FAIL
	return -1;
}
static int
emit_types(struct compound_buf *b, const asn1typed_type_t *t, const struct type_plan *p) {
	size_t j;
	if(t->kind == ASN1TYPED_TYPE_PRIMITIVE) return format(b, "using %s = %s;\n", p->type,
        t->primitive_kind == ASN1TYPED_PRIMITIVE_NULL ? "::std::monostate" : character_kind(t->primitive_kind) ? "::std::string" : (t->primitive_kind == ASN1TYPED_PRIMITIVE_OCTET_STRING || t->primitive_kind == ASN1TYPED_PRIMITIVE_OBJECT_IDENTIFIER) ? "::std::vector<::std::byte>" : t->primitive_kind == ASN1TYPED_PRIMITIVE_BIT_STRING ? "::nrforge::aper::BitString" : "bool");
	if(t->kind == ASN1TYPED_TYPE_SEQUENCE_OF) return format(b, "struct %s { ::std::vector<%s> elements{}; };\n", p->type, p->members[0].type);
	if(t->kind == ASN1TYPED_TYPE_CHOICE) {
		for(j = 0; j < p->member_count; ++j)
			if(format(b, "struct %s { %s value{}; };\n", p->members[j].wrapper, p->members[j].type)) return -1;
		if(format(b, "using %s = ::std::variant<", p->type)) return -1;
		for(j = 0; j < p->member_count; ++j) if(format(b, "%s%s", j ? ", " : "", p->members[j].qualified_wrapper)) return -1;
		return append(b, ">;\n");
	}
	if(format(b, "struct %s {\n", p->type)) return -1;
	for(j = 0; j < p->member_count; ++j) {
		const struct member_plan *mp = &p->members[j];
		if(t->fields[j].presence == ASN1TYPED_PRESENCE_OPTIONAL) {
			if(format(b, "    ::std::optional<%s> %s{};\n", mp->type, mp->name)) return -1;
		} else if(format(b, "    %s %s{};\n", mp->type, mp->name)) return -1;
	}
	if(p->extension_member && format(b, "    ::nrforge::aper::SequenceExtensionData %s{};\n", p->extension_member)) return -1;
	return append(b, "};\n");
}
static int
emit_boolean_mapping(struct compound_buf *b) {
	return append(b, "struct COMPOUND_BOOLEAN {\n    using value_type = bool;\n    static constexpr unsigned bit_count = 1;\n    static constexpr bool false_bit = false;\n    static constexpr bool true_bit = true;\n    static constexpr bool align_before_payload_to_octet = false;\n};\n");
}
static int
emit_mapping(struct compound_buf *b, const asn1typed_type_t *t, const struct type_plan *p, const struct plan *all) {
	size_t j, k;
    if(t->kind == ASN1TYPED_TYPE_PRIMITIVE && character_kind(t->primitive_kind)) {
        if(!t->size_constraint.has_size_constraint && format(b, "// Unconstrained character SIZE: no extension selector; unfragmented determinant.\n")) return -1;
        return format(b, "struct %s {\n    using value_type = %s;\n    static constexpr ::std::size_t lower_bound = %" PRIuMAX ";\n    static constexpr ::std::size_t upper_bound = %" PRIuMAX ";\n    static constexpr bool extensible = %s;\n%s    static constexpr unsigned bits_per_character = %u;\n    static constexpr bool ascii_codepoint_encoding = %s;\n    static constexpr bool size_constraint_per_visible = %s;\n    static constexpr bool fragmented_supported = false;\n};\n", p->mapping, p->qualified_type, (uintmax_t)t->size_constraint.lower_bound, (uintmax_t)t->size_constraint.upper_bound, t->size_constraint.is_extensible ? "true" : "false", t->size_constraint.has_size_constraint ? "" : "    static constexpr bool unconstrained = true;\n", t->primitive_kind == ASN1TYPED_PRIMITIVE_UTF8_STRING ? 0u : 8u, t->primitive_kind == ASN1TYPED_PRIMITIVE_UTF8_STRING ? "false" : "true", t->primitive_kind == ASN1TYPED_PRIMITIVE_UTF8_STRING || !t->size_constraint.has_size_constraint ? "false" : "true");
    }
	if(t->kind == ASN1TYPED_TYPE_PRIMITIVE && t->primitive_kind == ASN1TYPED_PRIMITIVE_BIT_STRING && t->size_constraint.upper_bound > 65535)
        return format(b, "struct %s { using value_type = %s; static constexpr bool unconstrained = false; static constexpr ::std::size_t lower_bound = %" PRIuMAX "; static constexpr ::std::size_t upper_bound = %" PRIuMAX "; static constexpr bool fragmented_supported = true; static constexpr bool length_units_are_bits = true; };\n", p->mapping, p->qualified_type, (uintmax_t)t->size_constraint.lower_bound, (uintmax_t)t->size_constraint.upper_bound);
	if(t->kind == ASN1TYPED_TYPE_PRIMITIVE && t->size_constraint.is_extensible &&
		(t->primitive_kind == ASN1TYPED_PRIMITIVE_OCTET_STRING || t->primitive_kind == ASN1TYPED_PRIMITIVE_BIT_STRING))
		return format(b, "struct %s { using value_type = %s; static constexpr bool unconstrained = false; static constexpr bool extensible = true; static constexpr ::std::size_t lower_bound = %" PRIuMAX "; static constexpr ::std::size_t upper_bound = %" PRIuMAX "; static constexpr bool fragmented_supported = false; static constexpr bool has_extension_addition = %s; static constexpr ::std::size_t extension_lower_bound = %" PRIuMAX "; static constexpr ::std::size_t extension_upper_bound = %" PRIuMAX "; };\n", p->mapping, p->qualified_type, (uintmax_t)t->size_constraint.lower_bound, (uintmax_t)t->size_constraint.upper_bound, t->size_constraint.has_extension_addition ? "true" : "false", (uintmax_t)t->size_constraint.extension_lower_bound, (uintmax_t)t->size_constraint.extension_upper_bound);
	if(t->kind == ASN1TYPED_TYPE_PRIMITIVE && (t->primitive_kind == ASN1TYPED_PRIMITIVE_OCTET_STRING || t->primitive_kind == ASN1TYPED_PRIMITIVE_BIT_STRING))
        return format(b, "struct %s {\n    using value_type = %s;\n    static constexpr bool unconstrained = %s;\n    static constexpr ::std::size_t lower_bound = %" PRIuMAX ";\n    static constexpr ::std::size_t upper_bound = %" PRIuMAX ";\n    static constexpr bool payload_align_to_octet = %s;\n    static constexpr bool has_length_determinant = %s;\n    static constexpr bool fragmented_supported = false;\n};\n", p->mapping, p->qualified_type,
            t->size_constraint.has_size_constraint ? "false" : "true", (uintmax_t)t->size_constraint.lower_bound,
            (uintmax_t)t->size_constraint.upper_bound,
            t->size_constraint.has_size_constraint && t->size_constraint.lower_bound == t->size_constraint.upper_bound && t->size_constraint.upper_bound <= (t->primitive_kind == ASN1TYPED_PRIMITIVE_BIT_STRING ? 16 : 2) ? "false" : "true",
            t->size_constraint.has_size_constraint && t->size_constraint.lower_bound == t->size_constraint.upper_bound ? "false" : "true");
	if(t->kind == ASN1TYPED_TYPE_PRIMITIVE && t->primitive_kind == ASN1TYPED_PRIMITIVE_OBJECT_IDENTIFIER)
		return format(b, "struct %s { using value_type = %s; static constexpr bool canonical_ber_content = true; static constexpr bool octet_aligned = true; };\n", p->mapping, p->qualified_type);
	if(t->kind == ASN1TYPED_TYPE_PRIMITIVE)
		return format(b, "struct %s : %s { using value_type = %s; };\n", p->mapping, t->primitive_kind == ASN1TYPED_PRIMITIVE_NULL ? all->null_mapping : all->boolean_mapping, p->qualified_type);
	if(format(b, "struct %s {\n    using value_type = %s;\n    static constexpr bool extensible = %s;\n", p->mapping, p->qualified_type, (p->extension_member || (t->kind == ASN1TYPED_TYPE_CHOICE && t->is_extensible)) ? "true" : "false")) return -1;
	if(t->kind == ASN1TYPED_TYPE_SEQUENCE_OF) {
        if(t->size_constraint.upper_bound >= 65536)
            return format(b, "    using element_type = %s;\n    using element_payload_mapping = %s;\n    static constexpr ::std::size_t lower_bound = %" PRIuMAX ";\n    static constexpr ::std::size_t upper_bound = %" PRIuMAX ";\n    static constexpr bool fragmented_supported = true;\n    static constexpr bool length_align_to_octet = true;\n    static constexpr auto elements_member = &%s::elements;\n};\n", p->members[0].type,p->members[0].mapping,(uintmax_t)t->size_constraint.lower_bound,(uintmax_t)t->size_constraint.upper_bound,p->qualified_type);
		uintmax_t cardinality = (uintmax_t)t->size_constraint.upper_bound - (uintmax_t)t->size_constraint.lower_bound + 1;
		unsigned bits = 0;
		uintmax_t n;
		if(cardinality == 256) bits = 8;
		else if(cardinality > 256) bits = 16;
		else for(n = cardinality - 1; n; n /= 2) ++bits;
		if(format(b, "    using element_type = %s;\n    using element_payload_mapping = %s;\n    static constexpr ::std::size_t lower_bound = %" PRIuMAX ";\n    static constexpr ::std::size_t upper_bound = %" PRIuMAX ";\n    static constexpr unsigned length_bits = %u;\n    static constexpr bool length_align_to_octet = %s;\n    static constexpr auto elements_member = &%s::elements;\n", p->members[0].type, p->members[0].mapping, (uintmax_t)t->size_constraint.lower_bound, (uintmax_t)t->size_constraint.upper_bound, bits, cardinality >= 256 ? "true" : "false", p->qualified_type)) return -1;
	} else if(t->kind == ASN1TYPED_TYPE_CHOICE) {
		unsigned bits = 0;
		for(k = p->member_count - 1; k; k /= 2) ++bits;
		if(format(b, "    static constexpr ::std::size_t root_count = %zu;\n    static constexpr unsigned selector_bits = %u;\n    static constexpr bool selector_align_to_octet = false;\n    static constexpr ::std::array<::std::size_t, %zu> storage_ordinal_to_per_index{{", p->member_count, bits, p->member_count)) return -1;
		for(j = 0; j < p->member_count; ++j) if(format(b, "%s%zu", j ? ", " : "", t->alternatives[j].per_root_index)) return -1;
		if(format(b, "}};\n    static constexpr ::std::array<::std::size_t, %zu> per_index_to_storage_ordinal{{", p->member_count)) return -1;
		for(j = 0; j < p->member_count; ++j) {
			for(k = 0; k < p->member_count; ++k) if(t->alternatives[k].per_root_index == j) break;
			if(k == p->member_count || format(b, "%s%zu", j ? ", " : "", k)) return -1;
		}
		if(append(b, "}};\n")) return -1;
		for(j = 0; j < p->member_count; ++j) {
			const struct member_plan *mp = &p->members[j];
			if(format(b, "    using wrapper_%zu = %s;\n    using payload_type_%zu = %s;\n    using payload_mapping_%zu = %s;\n", j, mp->qualified_wrapper, j, mp->type, j, mp->mapping)) return -1;
		}
	} else {
		size_t optional = 0;
		if(p->extension_member && format(b, "    static constexpr ::std::size_t root_field_count = %zu;\n    static constexpr ::std::size_t known_addition_count = 0;\n    using extension_data_type = ::nrforge::aper::SequenceExtensionData;\n    static constexpr auto extension_data_member = &%s::%s;\n", t->sequence_root_field_count, p->qualified_type, p->extension_member)) return -1;
		for(j = 0; j < p->member_count; ++j) if(t->fields[j].presence == ASN1TYPED_PRESENCE_OPTIONAL) ++optional;
		if(format(b, "    static constexpr ::std::size_t optional_bitmap_bit_count = %zu;\n", optional)) return -1;
		optional = 0;
		for(j = 0; j < p->member_count; ++j) {
			int is_optional = t->fields[j].presence == ASN1TYPED_PRESENCE_OPTIONAL;
			const struct member_plan *mp = &p->members[j];
			if(format(b, "    using field_%zu_type = %s;\n    using field_%zu_payload_mapping = %s;\n    static constexpr ::std::size_t field_%zu_declaration_ordinal = %zu;\n    static constexpr bool field_%zu_mandatory = %s;\n    static constexpr ::std::size_t field_%zu_optional_bitmap_ordinal = ", j, mp->type, j, mp->mapping, j, j, j, is_optional ? "false" : "true", j)) return -1;
			if(is_optional) { if(format(b, "%zu;\n", optional++)) return -1; }
			else if(append(b, "static_cast<::std::size_t>(-1);\n")) return -1;
		}
	}
	return append(b, "};\n");
}
static int
emit_boolean_helpers(struct compound_buf *b) {
	return append(b, "namespace compound_codec {\ninline ::nrforge::aper::Result<void> put_boolean(::nrforge::aper::FieldWriter& f, bool v) { return f.write_bit(v); }\ninline ::nrforge::aper::Result<bool> get_boolean(::nrforge::aper::FieldReader& f) { return f.read_bit(); }\n} // namespace compound_codec\n");
}
static int
failure(struct compound_buf *b, const char *type, const char *result) {
	return format(b, "if(!%s) return ::nrforge::aper::Result<%s>::failure(%s.error());\n", result, type, result);
}
static int
emit_codec(struct compound_buf *b, const asn1typed_type_t *t, const struct type_plan *p, const struct plan *all) {
	size_t j;
	if(format(b, "namespace compound_codec {\ninline ::nrforge::aper::Result<void> %s(::nrforge::aper::FieldWriter& f, const %s& v) {\n", p->put, p->qualified_type)) return -1;
	if(t->kind == ASN1TYPED_TYPE_PRIMITIVE) {
        if(character_kind(t->primitive_kind)) {
            if(format(b, "    return f.write_character_string(v, %s::lower_bound, %s::upper_bound, %s::extensible, ::nrforge::aper::CharacterStringKind::%s%s);\n", p->qualified_mapping, p->qualified_mapping, p->qualified_mapping, character_spelling(t->primitive_kind), t->size_constraint.has_size_constraint ? "" : ", true")) return -1;
        } else if(t->primitive_kind == ASN1TYPED_PRIMITIVE_NULL) {
            if(format(b, "    return %s(f, v);\n", all->null_put)) return -1;
        } else if(t->primitive_kind == ASN1TYPED_PRIMITIVE_OBJECT_IDENTIFIER) {
            if(append(b, "    return ::nrforge::aper::write_object_identifier(f, v);\n")) return -1;
        } else if(t->primitive_kind == ASN1TYPED_PRIMITIVE_BIT_STRING) {
            if(t->size_constraint.upper_bound > 65535) {
                if(format(b, "    return f.write_bit_string_fragmented_size(v, %s::lower_bound, %s::upper_bound);\n", p->qualified_mapping, p->qualified_mapping)) return -1;
            } else if(format(b, "    return f.write_bit_string(v, %s::lower_bound, %s::upper_bound, %s::unconstrained%s);\n", p->qualified_mapping, p->qualified_mapping, p->qualified_mapping, t->size_constraint.is_extensible ? ", true" : "")) return -1;
        } else if(t->primitive_kind == ASN1TYPED_PRIMITIVE_OCTET_STRING) {
            if(format(b, "    return f.write_octet_string(v, %s::lower_bound, %s::upper_bound, %s::unconstrained%s);\n", p->qualified_mapping, p->qualified_mapping, p->qualified_mapping, t->size_constraint.is_extensible ? ", true" : "")) return -1;
        } else if(format(b, "    return %s(f, v);\n", all->boolean_put)) return -1;
	} else if(t->kind == ASN1TYPED_TYPE_SEQUENCE_OF) {
        if(t->size_constraint.upper_bound >= 65536) {
            if(format(b, "    const auto count = v.elements.size();\n    if(count < %s::lower_bound || count > %s::upper_bound) return f.record_failure({::nrforge::aper::ErrorCode::constraint_violation,f.cursor_bit()});\n    ::std::size_t position = 0;\n    bool more;\n    do {\n        const auto remaining = count-position;\n        more = remaining >= 16384;\n        const auto chunk = more ? %s : remaining;\n        auto length = f.write_collection_segment(chunk,more); if(!length) return length;\n        for(::std::size_t i=0;i<chunk;++i) { auto element = %s(f,v.elements[position+i]); if(!element) return element; }\n        position += chunk;\n    } while(more);\n    return ::nrforge::aper::Result<void>::success();\n",p->qualified_mapping,p->qualified_mapping,t->size_constraint.upper_bound > 65536 ? "(remaining >= 65536 ? 65536 : (remaining/16384)*16384)" : "(remaining/16384)*16384",p->members[0].put)) return -1;
        } else
		if(format(b, "    const auto count = v.elements.size();\n    auto length = f.write_bounded_collection_length(count > 65535 ? ::std::numeric_limits<::std::uint64_t>::max() : static_cast<::std::uint64_t>(count), %s::lower_bound, %s::upper_bound);\n    if(!length) return length;\n    for(::std::size_t i = 0; i < count; ++i) { auto element = %s(f, v.elements[i]); if(!element) return element; }\n    return ::nrforge::aper::Result<void>::success();\n", p->qualified_mapping, p->qualified_mapping, p->members[0].put)) return -1;
	} else if(t->kind == ASN1TYPED_TYPE_CHOICE) {
        if(t->is_extensible && append(b, "    { auto extension = f.write_bit(false); if(!extension) return extension; }\n")) return -1;
		if(format(b, "    if(v.valueless_by_exception() || v.index() >= %s::root_count) return f.write_enumerated({false, static_cast<::std::uint64_t>(%s::root_count)}, static_cast<unsigned>(%s::root_count), false);\n    const auto ordinal = v.index();\n    auto selector = f.write_enumerated({false, static_cast<::std::uint64_t>(%s::storage_ordinal_to_per_index[ordinal])}, static_cast<unsigned>(%s::root_count), false);\n    if(!selector) return selector;\n", p->qualified_mapping, p->qualified_mapping, p->qualified_mapping, p->qualified_mapping, p->qualified_mapping)) return -1;
		for(j = 0; j < p->member_count; ++j)
			if(format(b, "    if(ordinal == %zu) return %s(f, ::std::get<%s>(v).value);\n", j, p->members[j].put, p->members[j].qualified_wrapper)) return -1;
		if(format(b, "    return f.write_enumerated({false, static_cast<::std::uint64_t>(%s::root_count)}, static_cast<unsigned>(%s::root_count), false);\n", p->qualified_mapping, p->qualified_mapping)) return -1;
	} else {
		if(append(b, "    (void)f; (void)v;\n")) return -1;
		if(p->extension_member && format(b, "    if(v.%s.received_bitmap_bit_count || !v.%s.unknown_additions.empty()) return f.reject_sequence_extension_data();\n    { auto extension = f.write_bit(false); if(!extension) return extension; }\n", p->extension_member, p->extension_member)) return -1;
		for(j = 0; j < p->member_count; ++j) if(t->fields[j].presence == ASN1TYPED_PRESENCE_OPTIONAL)
			if(format(b, "    { auto present = f.write_bit(v.%s.has_value()); if(!present) return present; }\n", p->members[j].name)) return -1;
		for(j = 0; j < p->member_count; ++j) {
			const struct member_plan *mp = &p->members[j];
			if(t->fields[j].presence == ASN1TYPED_PRESENCE_OPTIONAL) {
				if(format(b, "    if(v.%s) { auto field = %s(f, *v.%s); if(!field) return field; }\n", mp->name, mp->put, mp->name)) return -1;
			} else if(format(b, "    { auto field = %s(f, v.%s); if(!field) return field; }\n", mp->put, mp->name)) return -1;
		}
		if(append(b, "    return ::nrforge::aper::Result<void>::success();\n")) return -1;
	}
	if(format(b, "}\ninline ::nrforge::aper::Result<%s> %s(::nrforge::aper::FieldReader& f) {\n", p->qualified_type, p->get)) return -1;
	if(all->extensions && append(b, "    try {\n")) return -1;
	if(t->kind == ASN1TYPED_TYPE_PRIMITIVE) {
        if(character_kind(t->primitive_kind)) {
            if(format(b, "    return f.read_character_string_owned(%s::lower_bound, %s::upper_bound, %s::extensible, ::nrforge::aper::CharacterStringKind::%s%s);\n", p->qualified_mapping, p->qualified_mapping, p->qualified_mapping, character_spelling(t->primitive_kind), t->size_constraint.has_size_constraint ? "" : ", true")) return -1;
        } else if(t->primitive_kind == ASN1TYPED_PRIMITIVE_NULL) {
            if(format(b, "    return %s(f);\n", all->null_get)) return -1;
        } else if(t->primitive_kind == ASN1TYPED_PRIMITIVE_OBJECT_IDENTIFIER) {
            if(append(b, "    return ::nrforge::aper::read_object_identifier(f);\n")) return -1;
        } else if(t->primitive_kind == ASN1TYPED_PRIMITIVE_BIT_STRING) {
            if(t->size_constraint.upper_bound > 65535) {
                if(format(b, "    return f.read_bit_string_owned_fragmented_size(%s::lower_bound, %s::upper_bound);\n", p->qualified_mapping, p->qualified_mapping)) return -1;
            } else if(format(b, "    return f.read_bit_string_owned(%s::lower_bound, %s::upper_bound, %s::unconstrained%s);\n", p->qualified_mapping, p->qualified_mapping, p->qualified_mapping, t->size_constraint.is_extensible ? ", true" : "")) return -1;
        } else if(t->primitive_kind == ASN1TYPED_PRIMITIVE_OCTET_STRING) {
            if(format(b, "    return f.read_octet_string_owned(%s::lower_bound, %s::upper_bound, %s::unconstrained%s);\n", p->qualified_mapping, p->qualified_mapping, p->qualified_mapping, t->size_constraint.is_extensible ? ", true" : "")) return -1;
        } else if(format(b, "    return %s(f);\n", all->boolean_get)) return -1;
	} else if(t->kind == ASN1TYPED_TYPE_SEQUENCE_OF) {
        if(t->size_constraint.upper_bound >= 65536) {
            if(format(b, "    %s value{};\n    bool more;\n    do {\n        auto length = f.read_collection_segment();\n        if(!length) return ::nrforge::aper::Result<%s>::failure(length.error());\n        more = length.value().fragmented;\n        const auto count = length.value().count;\n        if(count > %s::upper_bound-value.elements.size()) { auto e=f.record_failure({::nrforge::aper::ErrorCode::constraint_violation,f.cursor_bit()}); return ::nrforge::aper::Result<%s>::failure(e.error()); }\n        if(count > value.elements.max_size()-value.elements.size()) { auto e=f.record_failure({::nrforge::aper::ErrorCode::resource_limit,f.cursor_bit()}); return ::nrforge::aper::Result<%s>::failure(e.error()); }\n        value.elements.reserve(value.elements.size()+count);\n        for(::std::size_t i=0;i<count;++i) { auto element=%s(f); if(!element) return ::nrforge::aper::Result<%s>::failure(element.error()); value.elements.push_back(::std::move(element).value()); }\n    } while(more);\n    if(value.elements.size() < %s::lower_bound) { auto e=f.record_failure({::nrforge::aper::ErrorCode::constraint_violation,f.cursor_bit()}); return ::nrforge::aper::Result<%s>::failure(e.error()); }\n    return ::nrforge::aper::Result<%s>::success(::std::move(value));\n",p->qualified_type,p->qualified_type,p->qualified_mapping,p->qualified_type,p->qualified_type,p->members[0].get,p->qualified_type,p->qualified_mapping,p->qualified_type,p->qualified_type)) return -1;
        } else
		if(format(b, "    auto length = f.read_bounded_collection_length(%s::lower_bound, %s::upper_bound);\n    ", p->qualified_mapping, p->qualified_mapping) || failure(b, p->qualified_type, "length") ||
			format(b, "    %s value{};\n    if(length.value() > value.elements.max_size()) {\n        auto error = f.record_failure({::nrforge::aper::ErrorCode::resource_limit, f.cursor_bit()});\n        return ::nrforge::aper::Result<%s>::failure(error.error());\n    }\n    value.elements.reserve(length.value());\n    for(::std::size_t i = 0; i < length.value(); ++i) {\n        auto element = %s(f);\n        ", p->qualified_type, p->qualified_type, p->members[0].get) || failure(b, p->qualified_type, "element") ||
			format(b, "        value.elements.push_back(::std::move(element).value());\n    }\n    return ::nrforge::aper::Result<%s>::success(::std::move(value));\n", p->qualified_type)) return -1;
	} else if(t->kind == ASN1TYPED_TYPE_CHOICE) {
        if(t->is_extensible && format(b, "    { auto extension = f.read_bit(); if(!extension) return ::nrforge::aper::Result<%s>::failure(extension.error()); if(extension.value()) { auto e = f.record_failure({::nrforge::aper::ErrorCode::constraint_violation,f.cursor_bit()}); return ::nrforge::aper::Result<%s>::failure(e.error()); } }\n", p->qualified_type, p->qualified_type)) return -1;
		if(format(b, "    auto selector = f.read_enumerated(static_cast<unsigned>(%s::root_count), false);\n    ", p->qualified_mapping) || failure(b, p->qualified_type, "selector") ||
			format(b, "    const auto ordinal = %s::per_index_to_storage_ordinal[static_cast<::std::size_t>(selector.value().index)];\n", p->qualified_mapping)) return -1;
		for(j = 0; j < p->member_count; ++j) {
			const struct member_plan *mp = &p->members[j];
			if(format(b, "    if(ordinal == %zu) {\n        auto field = %s(f);\n        ", j, mp->get) || failure(b, p->qualified_type, "field") ||
				format(b, "        return ::nrforge::aper::Result<%s>::success(%s{%s{%s}});\n    }\n", p->qualified_type, p->qualified_type, mp->qualified_wrapper, all->extensions ? "::std::move(field).value()" : "field.value()")) return -1;
		}
		/* Unreachable for validated constexpr tables; still records a sticky error. */
		if(format(b, "    auto invalid = f.read_enumerated(0, false);\n    return ::nrforge::aper::Result<%s>::failure(invalid.error());\n", p->qualified_type)) return -1;
	} else {
		if(format(b, "    (void)f;\n    %s value{};\n", p->qualified_type)) return -1;
		if(p->extension_member && format(b, "    auto extension = f.read_bit();\n    if(!extension) return ::nrforge::aper::Result<%s>::failure(extension.error());\n", p->qualified_type)) return -1;
		for(j = 0; j < p->member_count; ++j) if(t->fields[j].presence == ASN1TYPED_PRESENCE_OPTIONAL) {
			if(format(b, "    auto presence_%zu = f.read_bit();\n    if(!presence_%zu) return ::nrforge::aper::Result<%s>::failure(presence_%zu.error());\n", j, j, p->qualified_type, j)) return -1;
		}
		for(j = 0; j < p->member_count; ++j) {
			const struct member_plan *mp = &p->members[j];
			if(t->fields[j].presence == ASN1TYPED_PRESENCE_OPTIONAL) {
				if(format(b, "    if(presence_%zu.value()) {\n", j)) return -1;
			} else if(append(b, "    {\n")) return -1;
			if(format(b, "        auto field = %s(f);\n        ", mp->get) || failure(b, p->qualified_type, "field") || format(b, "        value.%s = %s;\n    }\n", mp->name, all->extensions ? "::std::move(field).value()" : "field.value()")) return -1;
		}
		if(p->extension_member && format(b,
			"    if(extension.value()) {\n"
			"        auto bitmap = f.read_sequence_extension_bitmap();\n"
			"        if(!bitmap) return ::nrforge::aper::Result<%s>::failure(bitmap.error());\n"
			"        value.%s.received_bitmap_bit_count = bitmap.value().bit_count;\n"
			"        for(::std::size_t i = 0; i < bitmap.value().bit_count; ++i) {\n"
			"            if((::std::to_integer<unsigned>(bitmap.value().packed_bits[i / 8]) & (1u << (7u - static_cast<unsigned>(i %% 8)))) == 0) continue;\n"
			"            if constexpr (::std::numeric_limits<::std::size_t>::digits > ::std::numeric_limits<::std::uint64_t>::digits) {\n"
			"                if(i > ::std::numeric_limits<::std::uint64_t>::max()) {\n"
			"                    auto error = f.record_failure({::nrforge::aper::ErrorCode::resource_limit, f.cursor_bit()});\n"
			"                    return ::nrforge::aper::Result<%s>::failure(error.error());\n"
			"                }\n"
			"            }\n"
			"            auto payload = f.read_open_type_owned();\n"
			"            if(!payload) return ::nrforge::aper::Result<%s>::failure(payload.error());\n"
			"            value.%s.unknown_additions.push_back({static_cast<::std::uint64_t>(i), ::std::move(payload).value()});\n"
			"        }\n"
			"    }\n", p->qualified_type, p->extension_member, p->qualified_type, p->qualified_type, p->extension_member)) return -1;
		if(format(b, "    return ::nrforge::aper::Result<%s>::success(%s);\n", p->qualified_type, all->extensions ? "::std::move(value)" : "value")) return -1;
	}
	if(all->extensions && format(b, "    } catch(const ::std::bad_alloc&) {\n        auto error = f.record_failure({::nrforge::aper::ErrorCode::allocation_failure, f.cursor_bit()});\n        return ::nrforge::aper::Result<%s>::failure(error.error());\n    } catch(const ::std::length_error&) {\n        auto error = f.record_failure({::nrforge::aper::ErrorCode::resource_limit, f.cursor_bit()});\n        return ::nrforge::aper::Result<%s>::failure(error.error());\n    }\n", p->qualified_type, p->qualified_type)) return -1;
	return format(b, "}\n} // namespace compound_codec\ninline ::nrforge::aper::Result<::nrforge::aper::CompleteEncoding> %s(const %s& v, const ::nrforge::aper::Limits& limits = {}) {\n    return ::nrforge::aper::encode_complete(v, limits, [&](::nrforge::aper::FieldWriter& f) { return %s(f, v); });\n}\ninline ::nrforge::aper::Result<%s> %s(::std::span<const ::std::byte> input, const ::nrforge::aper::Limits& limits = {}) {\n    return ::nrforge::aper::decode_complete<%s>(input, limits, [](::nrforge::aper::FieldReader& f) { return %s(f); });\n}\n", p->encode, p->qualified_type, p->qualified_put, p->qualified_type, p->decode, p->qualified_type, p->qualified_get);
}
/* Use-site payload framing remains distinct from the named declaration. */
static int
emit_member_sizes(struct compound_buf *b, const struct type_plan *p, int mode) {
    size_t j;
    for(j = 0; j < p->member_count; ++j) {
        const struct member_plan *mp = &p->members[j];
        const char *operation;
        const char *put, *get;
        if(!mp->size_name || mode == 0) continue;
        if(mode == 1) {
            if(mp->size.upper_bound > 65535) {
                if(format(b, "struct %s { using value_type = %s; static constexpr bool unconstrained = false; static constexpr ::std::size_t lower_bound = %" PRIuMAX "; static constexpr ::std::size_t upper_bound = %" PRIuMAX "; static constexpr bool fragmented_supported = true; static constexpr bool length_units_are_bits = true; };\n", mp->size_name, mp->type, (uintmax_t)mp->size.lower_bound, (uintmax_t)mp->size.upper_bound)) return -1;
            } else if(mp->size.has_extension_addition) {
                if(format(b, "struct %s { using value_type = %s; static constexpr bool unconstrained = false; static constexpr bool extensible = true; static constexpr ::std::size_t lower_bound = %" PRIuMAX "; static constexpr ::std::size_t upper_bound = %" PRIuMAX "; static constexpr bool fragmented_supported = false; static constexpr bool has_extension_addition = true; static constexpr ::std::size_t extension_lower_bound = %" PRIuMAX "; static constexpr ::std::size_t extension_upper_bound = %" PRIuMAX "; };\n",mp->size_name,mp->type,(uintmax_t)mp->size.lower_bound,(uintmax_t)mp->size.upper_bound,(uintmax_t)mp->size.extension_lower_bound,(uintmax_t)mp->size.extension_upper_bound)) return -1;
            } else {
            if(format(b, "struct %s { using value_type = %s; static constexpr bool unconstrained = false;%s static constexpr ::std::size_t lower_bound = %" PRIuMAX "; static constexpr ::std::size_t upper_bound = %" PRIuMAX "; static constexpr bool fragmented_supported = false; };\n", mp->size_name, mp->type, mp->size.is_extensible ? " static constexpr bool extensible = true;" : "", (uintmax_t)mp->size.lower_bound, (uintmax_t)mp->size.upper_bound)) return -1;
            }
            continue;
        }
        operation = mp->size_kind == ASN1TYPED_PRIMITIVE_BIT_STRING ? "bit" : "octet";
        put = strrchr(mp->size_put, ':') + 1; get = strrchr(mp->size_get, ':') + 1;
        if(mp->size.upper_bound > 65535) {
            if(format(b, "namespace compound_codec {\ninline ::nrforge::aper::Result<void> %s(::nrforge::aper::FieldWriter& f, const %s& v) { return f.write_bit_string_fragmented_size(v, %s::lower_bound, %s::upper_bound); }\ninline ::nrforge::aper::Result<%s> %s(::nrforge::aper::FieldReader& f) { return f.read_bit_string_owned_fragmented_size(%s::lower_bound, %s::upper_bound); }\n}\n", put, mp->type, mp->size_mapping, mp->size_mapping, mp->type, get, mp->size_mapping, mp->size_mapping)) return -1;
        } else if(format(b, "namespace compound_codec {\ninline ::nrforge::aper::Result<void> %s(::nrforge::aper::FieldWriter& f, const %s& v) { return f.write_%s_string(v, %s::lower_bound, %s::upper_bound, false%s); }\ninline ::nrforge::aper::Result<%s> %s(::nrforge::aper::FieldReader& f) { return f.read_%s_string_owned(%s::lower_bound, %s::upper_bound, false%s); }\n}\n", put, mp->type, operation, mp->size_mapping, mp->size_mapping, mp->size.is_extensible ? ", true" : "", mp->type, get, operation, mp->size_mapping, mp->size_mapping, mp->size.is_extensible ? ", true" : "")) return -1;
    }
    return 0;
}
static void
bound_literal(char *out, size_t size, intmax_t value, int is_signed) {
    if(!is_signed) snprintf(out, size, "UINT64_C(%" PRIuMAX ")", (uintmax_t)value);
    else if(value == INT64_MIN) snprintf(out, size, "(-INT64_C(9223372036854775807) - INT64_C(1))");
    else if(value < 0) snprintf(out, size, "(-INT64_C(%" PRIuMAX "))", (uintmax_t)(-value));
    else snprintf(out, size, "INT64_C(%" PRIuMAX ")", (uintmax_t)value);
}
static int
emit_member_values(struct compound_buf *b, const struct type_plan *p, int mode) {
    size_t j;
    for(j = 0; j < p->member_count; ++j) {
        const struct member_plan *mp = &p->members[j];
        char lower[96], upper[96];
        const char *suffix, *put, *get;
        if(!mp->value_name || mode == 0) continue;
        bound_literal(lower, sizeof(lower), mp->range.lower_bound, mp->value_signed);
        bound_literal(upper, sizeof(upper), mp->range.tail_count ? mp->range.tail[mp->range.tail_count - 1].upper_bound : mp->range.upper_bound, mp->value_signed);
        if(mp->range.unsigned_bounds) {
            snprintf(lower, sizeof(lower), "UINT64_C(%" PRIu64 ")", mp->range.unsigned_lower_bound);
            snprintf(upper, sizeof(upper), "UINT64_C(%" PRIu64 ")", mp->range.unsigned_upper_bound);
        }
        if(mode == 1 && !mp->range.tail_count && !mp->range.extension_addition_count) {
            if(format(b, "struct %s { using value_type = %s; static constexpr value_type lower_bound = %s; static constexpr value_type upper_bound = %s; static constexpr bool extensible = %s; };\n", mp->value_name, mp->type, lower, upper, mp->range.is_extensible ? "true" : "false")) return -1;
            continue;
        }
        if(mode == 1) {
            size_t n; char lo[96], hi[96];
            if(format(b, "struct %s { using value_type = %s; static constexpr value_type lower_bound = %s; static constexpr value_type upper_bound = %s; static constexpr bool extensible = %s;\n", mp->value_name, mp->type, lower, upper, mp->range.is_extensible ? "true" : "false")) return -1;
            if(mp->range.tail_count) {
                if(format(b, "    static constexpr ::nrforge::aper::IntegerInterval root_intervals[%zu] = {", mp->range.tail_count + 1)) return -1;
                bound_literal(hi, sizeof(hi), mp->range.upper_bound, 1);
                if(format(b, "{%s, %s}", lower, hi)) return -1;
                for(n = 0; n < mp->range.tail_count; ++n) {
                    bound_literal(lo, sizeof(lo), mp->range.tail[n].lower_bound, 1);
                    bound_literal(hi, sizeof(hi), mp->range.tail[n].upper_bound, 1);
                    if(format(b, ", {%s, %s}", lo, hi)) return -1;
                }
                if(!mp->range.extension_addition_count) { if(append(b, "}; };\n")) return -1; continue; }
                if(append(b, "};\n")) return -1;
            }
            if(mp->range.extension_addition_count) {
                if(format(b, "    static constexpr ::nrforge::aper::IntegerInterval known_extension_intervals[%zu] = {", mp->range.extension_addition_count)) return -1;
                for(n = 0; n < mp->range.extension_addition_count; ++n) {
                    bound_literal(lo, sizeof(lo), mp->range.extension_additions[n].lower_bound, 1);
                    bound_literal(hi, sizeof(hi), mp->range.extension_additions[n].upper_bound, 1);
                    if(format(b, "%s{%s, %s}", n ? ", " : "", lo, hi)) return -1;
                }
                if(append(b, "};\n")) return -1;
            }
            if(append(b, "};\n")) return -1;
            continue;
        }
        suffix = mp->range.is_extensible ? "extensible_int" : mp->value_signed ? "bounded_int" : "bounded_uint";
        put = strrchr(mp->value_put, ':') + 1; get = strrchr(mp->value_get, ':') + 1;
        if(mp->range.tail_count) {
            if(format(b, "namespace compound_codec {\ninline ::nrforge::aper::Result<void> %s(::nrforge::aper::FieldWriter& f, const %s& v) { return f.write_integer_set(v, %s::root_intervals, %s::extensible); }\ninline ::nrforge::aper::Result<%s> %s(::nrforge::aper::FieldReader& f) { return f.read_integer_set(%s::root_intervals, %s::extensible); }\n}\n", put, mp->type, mp->value_mapping, mp->value_mapping, mp->type, get, mp->value_mapping, mp->value_mapping)) return -1;
            continue;
        }
        if(format(b, "namespace compound_codec {\ninline ::nrforge::aper::Result<void> %s(::nrforge::aper::FieldWriter& f, const %s& v) { return f.write_%s(v, %s::lower_bound, %s::upper_bound); }\ninline ::nrforge::aper::Result<%s> %s(::nrforge::aper::FieldReader& f) { return f.read_%s(%s::lower_bound, %s::upper_bound); }\n}\n", put, mp->type, suffix, mp->value_mapping, mp->value_mapping, mp->type, get, suffix, mp->value_mapping, mp->value_mapping)) return -1;
    }
    return 0;
}
static int
render(const asn1typed_module_t *m, const char *ns, char **out, char *diagnostic, size_t size, int mode, int extensions, int collections, int octets, int bits, int values, const struct asn1typed_cpp_ioc_entry *ioc) {
	struct plan p = {0};
	struct compound_buf b = {NULL, 0};
	char why[512] = "invalid compound renderer arguments";
	size_t i;
	int result = -1;
	p.extensions = extensions; p.collections = collections; p.ioc = ioc; p.octets = octets || !!ioc; p.bits = bits || !!ioc; p.values = values == 2 ? 2 : values ? 1 : ioc ? 2 : 0; p.characters = values == 8 || !!ioc; p.nulls = values == 4 || !!ioc; p.extensible_sizes = bits == 2 || !!ioc;
	if(out) *out = NULL;
	if(diagnostic && size) diagnostic[0] = 0;
	if(!out || preflight(m, ns, &p, why, sizeof(why))) goto done;
	if((collections || p.octets || p.bits) && append(&b, "#include <vector>\n")) goto oom;
    for(i = 0; i < m->type_count; ++i) {
        if(m->types[i].kind == ASN1TYPED_TYPE_PRIMITIVE && character_kind(m->types[i].primitive_kind)) {
            if(append(&b, "#include <string>\n")) goto oom;
            break;
        }
    }
	if(p.has_extension_sequence && append(&b, "#include <sequence_extensions.hpp>\n")) goto oom;
	if(extensions && mode == 2 && append(&b, "#include <new>\n#include <limits>\n#include <stdexcept>\n#include <utility>\n")) goto oom;
    if(append(&b, "#include <array>\n#include <cstddef>\n#include <cstdint>\n#include <optional>\n#include <span>\n#include <variant>\n") || format(&b, "namespace %s {\n", ns) ||
		(mode == 1 && emit_boolean_mapping(&b)) || (mode == 2 && emit_boolean_helpers(&b)) || append(&b, "} // namespace\n")) goto oom;
    if(p.has_nulls) {
        if(format(&b, "namespace %s {\n", ns)) goto oom;
        if(mode == 1 && append(&b, "struct COMPOUND_NULL { using value_type = ::std::monostate; static constexpr unsigned bit_count = 0; static constexpr bool align_before_payload_to_octet = false; };\n")) goto oom;
        if(mode == 2 && append(&b, "namespace compound_codec {\ninline ::nrforge::aper::Result<void> put_null(::nrforge::aper::FieldWriter& f, ::std::monostate v) { (void)f; (void)v; return ::nrforge::aper::Result<void>::success(); }\ninline ::nrforge::aper::Result<::std::monostate> get_null(::nrforge::aper::FieldReader& f) { (void)f; return ::nrforge::aper::Result<::std::monostate>::success({}); }\n}\n")) goto oom;
        if(append(&b, "}\n")) goto oom;
    }
    if(p.has_bits) {
        if(format(&b, "namespace %s {\n", ns)) goto oom;
        if(mode == 1 && append(&b, "struct COMPOUND_BITS { using value_type = ::nrforge::aper::BitString; static constexpr bool unconstrained = true; static constexpr bool fragmented_supported = false; };\n")) goto oom;
        if(mode == 2 && append(&b, "namespace compound_codec {\ninline ::nrforge::aper::Result<void> put_bits(::nrforge::aper::FieldWriter& f, const ::nrforge::aper::BitString& v) { return f.write_bit_string(v, 0, 0, true); }\ninline ::nrforge::aper::Result<::nrforge::aper::BitString> get_bits(::nrforge::aper::FieldReader& f) { return f.read_bit_string_owned(0, 0, true); }\n}\n")) goto oom;
        if(append(&b, "}\n")) goto oom;
    }
	if(p.has_octets) {
        if(format(&b, "namespace %s {\n", ns)) goto oom;
        if(mode == 1 && append(&b, "struct COMPOUND_OCTETS { using value_type = ::std::vector<::std::byte>; static constexpr bool unconstrained = true; static constexpr bool fragmented_supported = false; };\n")) goto oom;
        if(mode == 2 && append(&b, "namespace compound_codec {\ninline ::nrforge::aper::Result<void> put_octets(::nrforge::aper::FieldWriter& f, const ::std::vector<::std::byte>& v) { return f.write_octet_string(v, 0, 0, true); }\ninline ::nrforge::aper::Result<::std::vector<::std::byte>> get_octets(::nrforge::aper::FieldReader& f) { return f.read_octet_string_owned(0, 0, true); }\n}\n")) goto oom;
        if(append(&b, "}\n")) goto oom;
    }
	for(i = 0; i < m->type_count; ++i) {
		const asn1typed_type_t *t = &m->types[i];
		if(primitive(t)) {
			char *text = NULL;
			if(single_view(m, i, ns, mode, &text, why, sizeof(why), p.values)) { free(text); goto done; }
			if(append(&b, text)) { free(text); goto oom; } free(text);
		} else {
			if(format(&b, "namespace %s {\n", ns) || emit_member_sizes(&b, &p.types[i], mode) || emit_member_values(&b, &p.types[i], mode) || (ioc && ioc[i].registry ? asn1typed_render_cpp_ioc_emit(&b, &p.types[i], &ioc[i], mode) : mode == 0 ? emit_types(&b, t, &p.types[i]) : mode == 1 ? emit_mapping(&b, t, &p.types[i], &p) : emit_codec(&b, t, &p.types[i], &p)) || append(&b, "} // namespace\n")) goto oom;
		}
	}
	*out = b.text; b.text = NULL; result = 0; goto done;
oom:
	snprintf(why, sizeof(why), "out of memory emitting compound output");
done:
	free(b.text); if(p.types || p.boolean_mapping || p.boolean_put || p.boolean_get || p.null_mapping || p.null_put || p.null_get) clear(&p, m);
	if(result && diagnostic && size) snprintf(diagnostic, size, "%s", why[0] ? why : "compound preflight allocation failure");
	return result;
}
int
asn1typed_render_cpp_owned_compound_types(const asn1typed_module_t *m, const char *ns,
		char **out, char *diagnostic, size_t size) { return render(m, ns, out, diagnostic, size, 0, 0, 0, 0, 0, 0, NULL); }
int
asn1typed_render_cpp_owned_compound_mapping(const asn1typed_module_t *m, const char *ns,
		char **out, char *diagnostic, size_t size) { return render(m, ns, out, diagnostic, size, 1, 0, 0, 0, 0, 0, NULL); }
int
asn1typed_render_cpp_owned_compound_codec(const asn1typed_module_t *m, const char *ns,
		char **out, char *diagnostic, size_t size) { return render(m, ns, out, diagnostic, size, 2, 0, 0, 0, 0, 0, NULL); }

int
asn1typed_render_cpp_owned_sequence_extension_types(const asn1typed_module_t *m, const char *ns,
		char **out, char *diagnostic, size_t size) { return render(m, ns, out, diagnostic, size, 0, 1, 0, 0, 0, 0, NULL); }
int
asn1typed_render_cpp_owned_sequence_extension_mapping(const asn1typed_module_t *m, const char *ns,
		char **out, char *diagnostic, size_t size) { return render(m, ns, out, diagnostic, size, 1, 1, 0, 0, 0, 0, NULL); }
int
asn1typed_render_cpp_owned_sequence_extension_codec(const asn1typed_module_t *m, const char *ns,
		char **out, char *diagnostic, size_t size) { return render(m, ns, out, diagnostic, size, 2, 1, 0, 0, 0, 0, NULL); }

int
asn1typed_render_cpp_owned_collection_types(const asn1typed_module_t *m, const char *ns,
		char **out, char *diagnostic, size_t size) { return render(m, ns, out, diagnostic, size, 0, 1, 1, 0, 0, 0, NULL); }
int
asn1typed_render_cpp_owned_collection_mapping(const asn1typed_module_t *m, const char *ns,
		char **out, char *diagnostic, size_t size) { return render(m, ns, out, diagnostic, size, 1, 1, 1, 0, 0, 0, NULL); }
int
asn1typed_render_cpp_owned_collection_codec(const asn1typed_module_t *m, const char *ns,
		char **out, char *diagnostic, size_t size) { return render(m, ns, out, diagnostic, size, 2, 1, 1, 0, 0, 0, NULL); }

int
asn1typed_render_cpp_compound_ioc(const asn1typed_module_t *m, const char *ns,
        const struct asn1typed_cpp_ioc_entry *ioc, int mode,
        char **out, char *diagnostic, size_t size) {
    return render(m, ns, out, diagnostic, size, mode, 1, 1, 1, 1, 2, ioc);
}
int asn1typed_render_cpp_compound_ioc_check_names(const asn1typed_module_t *m, const char *ns,
        const struct asn1typed_cpp_ioc_entry *ioc, const char *const *names, size_t count, char *why, size_t size) {
    struct plan p = {0};
    int result;
    p.extensions = p.collections = 1; p.ioc = ioc; p.octets = !!ioc; p.bits = !!ioc; p.values = ioc ? 2 : 0; p.characters = p.nulls = p.extensible_sizes = !!ioc; p.extra_names = names; p.extra_count = count;
    result = preflight(m, ns, &p, why, size);
    if(p.types || p.boolean_mapping || p.boolean_put || p.boolean_get || p.null_mapping || p.null_put || p.null_get) clear(&p, m);
    return result;
}

int
asn1typed_render_cpp_owned_octet_types(const asn1typed_module_t *m, const char *ns,
        char **out, char *diagnostic, size_t size) { return render(m, ns, out, diagnostic, size, 0, 1, 1, 1, 0, 0, NULL); }

int
asn1typed_render_cpp_owned_octet_mapping(const asn1typed_module_t *m, const char *ns,
        char **out, char *diagnostic, size_t size) { return render(m, ns, out, diagnostic, size, 1, 1, 1, 1, 0, 0, NULL); }

int
asn1typed_render_cpp_owned_octet_codec(const asn1typed_module_t *m, const char *ns,
        char **out, char *diagnostic, size_t size) { return render(m, ns, out, diagnostic, size, 2, 1, 1, 1, 0, 0, NULL); }

int
asn1typed_render_cpp_owned_bit_types(const asn1typed_module_t *m, const char *ns, char **out, char *diagnostic, size_t size) { return render(m, ns, out, diagnostic, size, 0, 1, 1, 1, 1, 0, NULL); }
int
asn1typed_render_cpp_owned_bit_mapping(const asn1typed_module_t *m, const char *ns, char **out, char *diagnostic, size_t size) { return render(m, ns, out, diagnostic, size, 1, 1, 1, 1, 1, 0, NULL); }
int
asn1typed_render_cpp_owned_bit_codec(const asn1typed_module_t *m, const char *ns, char **out, char *diagnostic, size_t size) { return render(m, ns, out, diagnostic, size, 2, 1, 1, 1, 1, 0, NULL); }

int
asn1typed_render_cpp_owned_value_types(const asn1typed_module_t *m, const char *ns, char **out, char *diagnostic, size_t size) { return render(m, ns, out, diagnostic, size, 0, 1, 1, 1, 1, 1, NULL); }
int
asn1typed_render_cpp_owned_value_mapping(const asn1typed_module_t *m, const char *ns, char **out, char *diagnostic, size_t size) { return render(m, ns, out, diagnostic, size, 1, 1, 1, 1, 1, 1, NULL); }
int
asn1typed_render_cpp_owned_value_codec(const asn1typed_module_t *m, const char *ns, char **out, char *diagnostic, size_t size) { return render(m, ns, out, diagnostic, size, 2, 1, 1, 1, 1, 1, NULL); }

static int
render_shapes(const asn1typed_module_t *m, const char *ns, char **out, char *why, size_t size, int mode) {
    struct asn1typed_cpp_inline_enum_view view = {0};
    int rc;
    if(out) *out = NULL;
    if(!out) { if(why && size) snprintf(why, size, "invalid shape renderer output argument"); return -1; }
    if(asn1typed_cpp_inline_enum_view_init(&view, m, why, size)) return -1;
    rc = render(view.module, ns, out, why, size, mode, 1, 1, 1, 1, 1, NULL);
    asn1typed_cpp_inline_enum_view_clear(&view);
    return rc;
}
int
asn1typed_render_cpp_owned_shape_types(const asn1typed_module_t *m, const char *ns, char **out, char *why, size_t size) { return render_shapes(m, ns, out, why, size, 0); }
int
asn1typed_render_cpp_owned_shape_mapping(const asn1typed_module_t *m, const char *ns, char **out, char *why, size_t size) { return render_shapes(m, ns, out, why, size, 1); }
int
asn1typed_render_cpp_owned_shape_codec(const asn1typed_module_t *m, const char *ns, char **out, char *why, size_t size) { return render_shapes(m, ns, out, why, size, 2); }

int asn1typed_render_cpp_owned_character_types(const asn1typed_module_t *m, const char *ns, char **out, char *diagnostic, size_t size) { return render(m, ns, out, diagnostic, size, 0, 1, 1, 1, 1, 8, NULL); }
int asn1typed_render_cpp_owned_character_mapping(const asn1typed_module_t *m, const char *ns, char **out, char *diagnostic, size_t size) { return render(m, ns, out, diagnostic, size, 1, 1, 1, 1, 1, 8, NULL); }
int asn1typed_render_cpp_owned_character_codec(const asn1typed_module_t *m, const char *ns, char **out, char *diagnostic, size_t size) { return render(m, ns, out, diagnostic, size, 2, 1, 1, 1, 1, 8, NULL); }
/* NULL is opt-in for ordinary output; physical IOC output enables it above. */
static int render_null(const asn1typed_module_t *m, const char *ns, char **out, char *diag, size_t size, int mode) {
    struct asn1typed_cpp_inline_enum_view view = {0};
    char why[512]; int rc;
    if(out) *out = NULL;
    if(asn1typed_cpp_inline_enum_view_init(&view, m, why, sizeof(why))) { if(diag && size) snprintf(diag, size, "%s", why); return -1; }
    rc = render(view.module, ns, out, diag, size, mode, 1, 1, 1, 1, 4, NULL);
    asn1typed_cpp_inline_enum_view_clear(&view); return rc;
}
int asn1typed_render_cpp_owned_null_types(const asn1typed_module_t *m, const char *ns, char **out, char *diag, size_t size) { return render_null(m, ns, out, diag, size, 0); }
int asn1typed_render_cpp_owned_null_mapping(const asn1typed_module_t *m, const char *ns, char **out, char *diag, size_t size) { return render_null(m, ns, out, diag, size, 1); }
int asn1typed_render_cpp_owned_null_codec(const asn1typed_module_t *m, const char *ns, char **out, char *diag, size_t size) { return render_null(m, ns, out, diag, size, 2); }
/* Batch SIZE capability: historical ordinary entry points remain bounded. */
static int render_sizes(const asn1typed_module_t *m, const char *ns, char **out, char *why, size_t size, int mode) {
    struct asn1typed_cpp_inline_enum_view view = {0}; int rc;
    if(out) *out = NULL;
    if(!out) { if(why && size) snprintf(why,size,"invalid SIZE renderer output argument"); return -1; }
    if(asn1typed_cpp_inline_enum_view_init(&view,m,why,size)) return -1;
    rc=render(view.module,ns,out,why,size,mode,1,1,1,2,1,NULL);
    asn1typed_cpp_inline_enum_view_clear(&view); return rc;
}
int asn1typed_render_cpp_owned_size_types(const asn1typed_module_t *m,const char *ns,char **out,char *why,size_t size) { return render_sizes(m,ns,out,why,size,0); }
int asn1typed_render_cpp_owned_size_mapping(const asn1typed_module_t *m,const char *ns,char **out,char *why,size_t size) { return render_sizes(m,ns,out,why,size,1); }
int asn1typed_render_cpp_owned_size_codec(const asn1typed_module_t *m,const char *ns,char **out,char *why,size_t size) { return render_sizes(m,ns,out,why,size,2); }
static int
render_domains(const asn1typed_module_t *m, const char *ns, char **out, char *why, size_t size, int mode) {
    struct asn1typed_cpp_inline_enum_view view = {0};
    int rc;
    if(out) *out = NULL;
    if(!out) { if(why && size) snprintf(why, size, "invalid domain renderer output argument"); return -1; }
    if(asn1typed_cpp_inline_enum_view_init(&view, m, why, size)) return -1;
    rc = render(view.module, ns, out, why, size, mode, 1, 1, 1, 1, 2, NULL);
    asn1typed_cpp_inline_enum_view_clear(&view);
    return rc;
}

int
asn1typed_render_cpp_owned_domain_types(const asn1typed_module_t *m, const char *ns, char **out, char *why, size_t size) { return render_domains(m, ns, out, why, size, 0); }

int
asn1typed_render_cpp_owned_domain_mapping(const asn1typed_module_t *m, const char *ns, char **out, char *why, size_t size) { return render_domains(m, ns, out, why, size, 1); }

int
asn1typed_render_cpp_owned_domain_codec(const asn1typed_module_t *m, const char *ns, char **out, char *why, size_t size) { return render_domains(m, ns, out, why, size, 2); }
