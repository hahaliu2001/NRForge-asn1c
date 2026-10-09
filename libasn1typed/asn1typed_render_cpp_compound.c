#include "asn1typed_render_cpp.h"
#include "asn1typed_render_cpp_internal.h"
#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
struct compound_buf { char *text; size_t length; };
struct member_plan { char *name, *wrapper, *qualified_wrapper; const char *type, *mapping, *put, *get; };
struct type_plan {
	char *type, *qualified_type, *mapping, *qualified_mapping, *constraint;
	char *encode, *decode, *put, *get, *qualified_put, *qualified_get, *extension_member;
	struct member_plan *members;
	size_t member_count;
};
struct plan { struct type_plan *types; char *boolean_mapping, *boolean_put, *boolean_get; int extensions, has_extension_sequence; };
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
	return !s->has_size_constraint && !s->is_extensible && !s->lower_bound && !s->upper_bound;
}
static int
empty_range(const asn1typed_integer_value_range_t *r) {
	return !r->has_value_range && !r->is_extensible && !r->lower_bound && !r->upper_bound && !r->tail && !r->tail_count;
}
static int
storage(size_t count, size_t capacity, const void *pointer) {
	return count <= capacity && !!capacity == !!pointer;
}
static int
shape(const asn1typed_type_t *t, int extensions) {
	if(!empty_size(&t->size_constraint) || !empty_ref(&t->element_type) || !empty_ref(&t->ioc_container) ||
		t->has_ioc_table || t->ioc_object_set_is_extensible) return 0;
	if(t->kind != ASN1TYPED_TYPE_ENUMERATED && (t->enum_items || t->enum_item_count || t->enum_item_capacity || t->has_valid_per_enumeration_mapping)) return 0;
	if(t->kind != ASN1TYPED_TYPE_SEQUENCE && (t->fields || t->field_count || t->field_capacity)) return 0;
	if(t->kind != ASN1TYPED_TYPE_CHOICE && (t->alternatives || t->alternative_count || t->alternative_capacity || t->has_valid_per_root_mapping)) return 0;
	if(t->kind != ASN1TYPED_TYPE_PRIMITIVE && t->primitive_kind != ASN1TYPED_PRIMITIVE_INVALID) return 0;
	if(t->kind != ASN1TYPED_TYPE_ENUMERATED && t->is_extensible && !(extensions && t->kind == ASN1TYPED_TYPE_SEQUENCE)) return 0;
	if(!(t->kind == ASN1TYPED_TYPE_PRIMITIVE && t->primitive_kind == ASN1TYPED_PRIMITIVE_INTEGER) && !empty_range(&t->value_range)) return 0;
	if(t->kind == ASN1TYPED_TYPE_PRIMITIVE) return t->primitive_kind == ASN1TYPED_PRIMITIVE_BOOLEAN || t->primitive_kind == ASN1TYPED_PRIMITIVE_INTEGER;
	if(t->kind == ASN1TYPED_TYPE_ENUMERATED) return storage(t->enum_item_count, t->enum_item_capacity, t->enum_items);
	if(t->kind == ASN1TYPED_TYPE_CHOICE) return t->alternative_count >= 1 && t->alternative_count <= 255 && storage(t->alternative_count, t->alternative_capacity, t->alternatives);
	if(t->kind == ASN1TYPED_TYPE_SEQUENCE) return storage(t->field_count, t->field_capacity, t->fields);
	return 0;
}
static int
field_semantics(const asn1typed_field_t *f) {
	const asn1typed_class_field_relation_t *r = &f->class_field_relation;
	return f->type_semantics == ASN1TYPED_FIELD_FIXED_TYPE && !f->inline_enumerated &&
		!f->has_class_field_relation && !r->class_module && !r->class_source_name && !r->class_field_source_name &&
		!r->actual_index && !r->has_selector && !r->selector_source_name &&
		!f->ioc.symbolic_id && !f->ioc.has_numeric_id && !f->ioc.numeric_id && f->ioc.criticality == ASN1TYPED_CRITICALITY_REJECT &&
		(f->presence == ASN1TYPED_PRESENCE_MANDATORY || f->presence == ASN1TYPED_PRESENCE_OPTIONAL) &&
		empty_size(&f->size_constraint) && empty_range(&f->value_range);
}
static void
clear(struct plan *p, const asn1typed_module_t *m) {
	size_t i, j;
	if(p->types) for(i = 0; i < m->type_count; ++i) {
		struct type_plan *t = &p->types[i];
		free(t->type); free(t->qualified_type); free(t->mapping); free(t->qualified_mapping); free(t->constraint);
		free(t->encode); free(t->decode); free(t->put); free(t->get); free(t->qualified_put); free(t->qualified_get); free(t->extension_member);
		if(t->members) for(j = 0; j < t->member_count; ++j) {
			free(t->members[j].name); free(t->members[j].wrapper); free(t->members[j].qualified_wrapper);
		}
		free(t->members);
	}
	free(p->types); free(p->boolean_mapping); free(p->boolean_put); free(p->boolean_get);
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
resolve(const asn1typed_module_t *m, const struct plan *p, size_t owner,
		const asn1typed_type_ref_t *r, struct member_plan *member) {
	size_t i;
	if(r->actuals || r->actual_count) return -1;
	if(r->kind == ASN1TYPED_REF_PRIMITIVE) {
		if(r->primitive_kind != ASN1TYPED_PRIMITIVE_BOOLEAN || r->module || r->source_name) return -1;
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
typedef int (*renderer)(const asn1typed_module_t *, const char *, char **, char *, size_t);
static renderer
delegate(const asn1typed_type_t *t, int mode) {
	if(t->kind == ASN1TYPED_TYPE_ENUMERATED) return mode == 0 ? asn1typed_render_cpp_owned_enum_types : mode == 1 ? asn1typed_render_cpp_owned_enum_mapping : asn1typed_render_cpp_owned_enum_codec;
	return mode == 0 ? asn1typed_render_cpp_owned_uint_types : mode == 1 ? asn1typed_render_cpp_owned_uint_mapping : asn1typed_render_cpp_owned_uint_codec;
}
static int
primitive(const asn1typed_type_t *t) {
	return t->kind == ASN1TYPED_TYPE_ENUMERATED || (t->kind == ASN1TYPED_TYPE_PRIMITIVE && t->primitive_kind == ASN1TYPED_PRIMITIVE_INTEGER);
}
static int
single_view(const asn1typed_module_t *m, size_t i, const char *ns, int mode, char **text, char *why, size_t size) {
	asn1typed_module_t view = *m;
	view.types = &m->types[i]; view.type_count = 1; view.type_capacity = 1;
	return delegate(&m->types[i], mode)(&view, ns, text, why, size);
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
		m->type_count > SIZE_MAX / sizeof(*p->types) || m->bound_instances || m->bound_instance_count || m->bound_instance_capacity)
		FAIL("compound module has invalid storage or unsupported bound instances");
	if(!asn1typed_render_cpp_safe_namespace(ns)) FAIL("invalid or unsafe compound namespace");
	if(p->extensions && (!strcmp(ns, "nrforge::aper") || !strncmp(ns, "nrforge::aper::", sizeof("nrforge::aper::") - 1)))
		FAIL("sequence extension namespace overlaps reserved runtime namespace nrforge::aper");
	p->types = (struct type_plan *)calloc(m->type_count, sizeof(*p->types));
	p->boolean_mapping = qualified(ns, "", "COMPOUND_BOOLEAN");
	p->boolean_put = qualified(ns, "compound_codec::", "put_boolean");
	p->boolean_get = qualified(ns, "compound_codec::", "get_boolean");
	if(!p->types || !p->boolean_mapping || !p->boolean_put || !p->boolean_get) FAIL("out of memory planning compound output");
	for(i = 0; i < sizeof(reserved) / sizeof(reserved[0]); ++i)
		if(register_symbol(reserved[i], &symbols, &symbol_count, why, size)) goto fail;
	for(i = 0; i < m->type_count; ++i) {
		const asn1typed_type_t *t = &m->types[i];
		struct type_plan *tp = &p->types[i];
		char *base;
		const char *scope = t->kind == ASN1TYPED_TYPE_ENUMERATED ? "enum_codec::" :
			(t->kind == ASN1TYPED_TYPE_PRIMITIVE && t->primitive_kind == ASN1TYPED_PRIMITIVE_INTEGER) ? "uint_codec::" : "compound_codec::";
		if(p->extensions && !(t->kind == ASN1TYPED_TYPE_SEQUENCE && t->is_extensible) &&
			(t->sequence_extension_evidence != ASN1TYPED_WIRE_EVIDENCE_UNAVAILABLE || t->sequence_root_field_count || t->sequence_known_addition_count || t->has_valid_sequence_extension_structure)) FAIL("unexpected SEQUENCE extension structure metadata");
		if(!shape(t, p->extensions)) FAIL("unsupported compound type shape, storage or metadata");
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
			if(single_view(m, i, ns, 0, &text, why, size)) { free(text); goto fail; }
			free(text); continue;
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
		tp->member_count = t->kind == ASN1TYPED_TYPE_CHOICE ? t->alternative_count : t->field_count;
		if(tp->member_count > SIZE_MAX / sizeof(*tp->members)) FAIL("compound member allocation size overflow");
		if(tp->member_count) tp->members = (struct member_plan *)calloc(tp->member_count, sizeof(*tp->members));
		if(tp->member_count && !tp->members) FAIL("out of memory planning compound members");
		for(j = 0; j < tp->member_count; ++j) {
			struct member_plan *mp = &tp->members[j];
			const asn1typed_type_ref_t *ref;
			const char *source;
			if(t->kind == ASN1TYPED_TYPE_CHOICE) {
				const asn1typed_choice_alternative_t *a = &t->alternatives[j];
				if(!empty_size(&a->size_constraint) || !empty_range(&a->value_range)) FAIL("unsupported CHOICE alternative constraint metadata");
				ref = &a->type_ref; source = a->source_name;
			} else {
				const asn1typed_field_t *f = &t->fields[j];
				if(!field_semantics(f)) FAIL("unsupported SEQUENCE field semantics or metadata");
				ref = &f->type; source = f->source_name;
			}
			if(!source || !source[0]) FAIL("compound member has missing source name");
			mp->name = asn1typed_render_cpp_final_name(source, ASN1TYPED_NAME_FIELD);
			if(!mp->name || asn1typed_render_cpp_header_macro(mp->name)) FAIL("compound member spelling is unsafe, a standard header macro, or unavailable");
			for(k = 0; k < j; ++k) if(!strcmp(mp->name, tp->members[k].name)) FAIL("compound member final name collision");
			if(resolve(m, p, i, ref, mp)) FAIL("unsupported, missing, external, forward or recursive compound reference");
			if(t->kind == ASN1TYPED_TYPE_CHOICE) {
				char *prefix = join("", tp->type, "_");
				if(!prefix) FAIL("out of memory naming CHOICE wrapper");
				mp->wrapper = join(prefix, mp->name, ""); free(prefix);
				mp->qualified_wrapper = mp->wrapper ? qualified(ns, "", mp->wrapper) : NULL;
				if(!mp->wrapper || !mp->qualified_wrapper) FAIL("out of memory qualifying CHOICE wrapper");
				if(register_symbol(mp->wrapper, &symbols, &symbol_count, why, size)) goto fail;
			}
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
	if(t->kind == ASN1TYPED_TYPE_PRIMITIVE) return format(b, "using %s = bool;\n", p->type);
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
	if(t->kind == ASN1TYPED_TYPE_PRIMITIVE)
		return format(b, "struct %s : %s { using value_type = %s; };\n", p->mapping, all->boolean_mapping, p->qualified_type);
	if(format(b, "struct %s {\n    using value_type = %s;\n    static constexpr bool extensible = %s;\n", p->mapping, p->qualified_type, p->extension_member ? "true" : "false")) return -1;
	if(t->kind == ASN1TYPED_TYPE_CHOICE) {
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
		if(format(b, "    return %s(f, v);\n", all->boolean_put)) return -1;
	} else if(t->kind == ASN1TYPED_TYPE_CHOICE) {
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
		if(format(b, "    return %s(f);\n", all->boolean_get)) return -1;
	} else if(t->kind == ASN1TYPED_TYPE_CHOICE) {
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
static int
render(const asn1typed_module_t *m, const char *ns, char **out, char *diagnostic, size_t size, int mode, int extensions) {
	struct plan p = {NULL, NULL, NULL, NULL, 0, 0};
	struct compound_buf b = {NULL, 0};
	char why[512] = "invalid compound renderer arguments";
	size_t i;
	int result = -1;
	p.extensions = extensions;
	if(out) *out = NULL;
	if(diagnostic && size) diagnostic[0] = 0;
	if(!out || preflight(m, ns, &p, why, sizeof(why))) goto done;
	if(p.has_extension_sequence && append(&b, "#include <sequence_extensions.hpp>\n")) goto oom;
	if(extensions && mode == 2 && append(&b, "#include <new>\n#include <limits>\n#include <stdexcept>\n#include <utility>\n")) goto oom;
	if(append(&b, "#include <array>\n#include <cstddef>\n#include <cstdint>\n#include <optional>\n#include <span>\n#include <variant>\n") || format(&b, "namespace %s {\n", ns) ||
		(mode == 1 && emit_boolean_mapping(&b)) || (mode == 2 && emit_boolean_helpers(&b)) || append(&b, "} // namespace\n")) goto oom;
	for(i = 0; i < m->type_count; ++i) {
		const asn1typed_type_t *t = &m->types[i];
		if(primitive(t)) {
			char *text = NULL;
			if(single_view(m, i, ns, mode, &text, why, sizeof(why))) { free(text); goto done; }
			if(append(&b, text)) { free(text); goto oom; } free(text);
		} else {
			if(format(&b, "namespace %s {\n", ns) || (mode == 0 ? emit_types(&b, t, &p.types[i]) : mode == 1 ? emit_mapping(&b, t, &p.types[i], &p) : emit_codec(&b, t, &p.types[i], &p)) || append(&b, "} // namespace\n")) goto oom;
		}
	}
	*out = b.text; b.text = NULL; result = 0; goto done;
oom:
	snprintf(why, sizeof(why), "out of memory emitting compound output");
done:
	free(b.text); if(p.types || p.boolean_mapping || p.boolean_put || p.boolean_get) clear(&p, m);
	if(result && diagnostic && size) snprintf(diagnostic, size, "%s", why[0] ? why : "compound preflight allocation failure");
	return result;
}
int
asn1typed_render_cpp_owned_compound_types(const asn1typed_module_t *m, const char *ns,
		char **out, char *diagnostic, size_t size) { return render(m, ns, out, diagnostic, size, 0, 0); }
int
asn1typed_render_cpp_owned_compound_mapping(const asn1typed_module_t *m, const char *ns,
		char **out, char *diagnostic, size_t size) { return render(m, ns, out, diagnostic, size, 1, 0); }
int
asn1typed_render_cpp_owned_compound_codec(const asn1typed_module_t *m, const char *ns,
		char **out, char *diagnostic, size_t size) { return render(m, ns, out, diagnostic, size, 2, 0); }

int
asn1typed_render_cpp_owned_sequence_extension_types(const asn1typed_module_t *m, const char *ns,
		char **out, char *diagnostic, size_t size) { return render(m, ns, out, diagnostic, size, 0, 1); }
int
asn1typed_render_cpp_owned_sequence_extension_mapping(const asn1typed_module_t *m, const char *ns,
		char **out, char *diagnostic, size_t size) { return render(m, ns, out, diagnostic, size, 1, 1); }
int
asn1typed_render_cpp_owned_sequence_extension_codec(const asn1typed_module_t *m, const char *ns,
		char **out, char *diagnostic, size_t size) { return render(m, ns, out, diagnostic, size, 2, 1); }
