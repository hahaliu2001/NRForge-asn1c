#include "asn1typed_render_cpp.h"
#include "asn1typed_render_cpp_internal.h"
#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct enum_buf { char *text; size_t length; };
struct enum_type_plan {
	char *type, *qualified_type, *mapping, *encode, *decode, *put, *get;
	char **items;
	size_t roots, additions, default_item;
};
static int
append(struct enum_buf *b, const char *text) {
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
format(struct enum_buf *b, const char *fmt, ...) {
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
supported_shape(const asn1typed_type_t *t) {
	return t->kind == ASN1TYPED_TYPE_ENUMERATED &&
		t->primitive_kind == ASN1TYPED_PRIMITIVE_INVALID &&
		!t->fields && !t->field_count && !t->field_capacity &&
		!t->alternatives && !t->alternative_count && !t->alternative_capacity &&
		!t->has_valid_per_root_mapping && !t->choice_root_only_extension_owned && empty_ref(&t->element_type) &&
		empty_ref(&t->ioc_container) && !t->has_ioc_table && !t->ioc_object_set_is_extensible &&
		!t->size_constraint.has_size_constraint && !t->size_constraint.is_extensible &&
		!t->size_constraint.lower_bound && !t->size_constraint.upper_bound && !t->size_constraint.has_extension_addition && !t->size_constraint.extension_lower_bound && !t->size_constraint.extension_upper_bound &&
		!t->value_range.has_value_range && !t->value_range.is_extensible &&
		!t->value_range.lower_bound && !t->value_range.upper_bound &&
		!t->value_range.tail && !t->value_range.tail_count && !t->value_range.extension_additions && !t->value_range.extension_addition_count;
}
static void
clear_plan(struct enum_type_plan *p, const asn1typed_module_t *m) {
	size_t i, j;
	if(!p) return;
	for(i = 0; i < m->type_count; ++i) {
		free(p[i].type); free(p[i].qualified_type); free(p[i].mapping); free(p[i].encode); free(p[i].decode);
		free(p[i].put); free(p[i].get);
		if(p[i].items) for(j = 0; j < m->types[i].enum_item_count; ++j) free(p[i].items[j]);
		free(p[i].items);
	}
	free(p);
}
static int
safe_symbol(const char *s) {
	return s && !asn1typed_render_cpp_header_macro(s);
}
static int
preflight(const asn1typed_module_t *m, const char *ns,
		struct enum_type_plan **out, char *why, size_t size) {
	struct enum_type_plan *p;
	size_t i, j, k;
#define FAIL(message) do { if(why && size) snprintf(why, size, "%s", message); goto fail; } while(0)
	*out = NULL;
	if(!m || !m->source_name || !m->source_name[0] || !m->types || !m->type_count ||
		m->type_count > m->type_capacity || m->type_count > SIZE_MAX / sizeof(*p) ||
		m->bound_instance_count || m->bound_instances || m->bound_instance_capacity) {
		if(why && size) snprintf(why, size, "enum-only module has invalid storage or unsupported bound instances");
		return -1;
	}
	if(!asn1typed_render_cpp_safe_namespace(ns)) {
		if(why && size) snprintf(why, size, "invalid or unsafe namespace for enum-only generation");
		return -1;
	}
	p = (struct enum_type_plan *)calloc(m->type_count, sizeof(*p));
	if(!p) { if(why && size) snprintf(why, size, "out of memory planning enum generation"); return -1; }
	for(i = 0; i < m->type_count; ++i) {
		const asn1typed_type_t *t = &m->types[i];
		char *base;
		if(!supported_shape(t)) FAIL("unsupported enum-only type metadata or kind");
		if(!t->identity.module || strcmp(t->identity.module, m->source_name) ||
			!t->identity.source_name || !t->identity.source_name[0]) FAIL("invalid enum type identity");
		if(asn1typed_enumerated_evidence_validate(t, why, size)) goto fail;
#if SIZE_MAX > UINT64_MAX
		if(t->enum_item_count > UINT64_MAX) FAIL("enum item count exceeds uint64 wire index domain");
#endif
		p[i].type = asn1typed_render_cpp_final_name(t->identity.source_name, ASN1TYPED_NAME_TYPE);
		base = asn1typed_render_cpp_final_name(t->identity.source_name, ASN1TYPED_NAME_FIELD);
		if(!p[i].type || !base) { free(base); FAIL("invalid enum spelling or out of memory naming enum"); }
		{
			char *prefix = join("::", ns, "::");
			if(!prefix) { free(base); FAIL("out of memory qualifying enum type"); }
			p[i].qualified_type = join(prefix, p[i].type, "");
			free(prefix);
			if(!p[i].qualified_type) { free(base); FAIL("out of memory qualifying enum type"); }
		}
		p[i].mapping = join("", p[i].type, "_aper");
		p[i].encode = join("encode_", base, ""); p[i].decode = join("decode_", base, "");
		p[i].put = join("put_", p[i].type, ""); p[i].get = join("get_", p[i].type, "");
		free(base);
		if(!p[i].mapping || !p[i].encode || !p[i].decode || !p[i].put || !p[i].get)
			FAIL("out of memory constructing enum helper names");
		if(!safe_symbol(p[i].type) || !safe_symbol(p[i].mapping) || !safe_symbol(p[i].encode) ||
			!safe_symbol(p[i].decode) || !safe_symbol(p[i].put) || !safe_symbol(p[i].get))
			FAIL("enum final spelling conflicts with a standard header macro");
		if(!strcmp(p[i].type, "Known") || (t->is_extensible && !strcmp(p[i].type, "UnknownExtension")))
			FAIL("enum type name collides with nested generated declaration");
		p[i].items = (char **)calloc(t->enum_item_count, sizeof(*p[i].items));
		if(!p[i].items) FAIL("out of memory naming enum items");
		for(j = 0; j < t->enum_item_count; ++j) {
			const asn1typed_enum_item_t *item = &t->enum_items[j];
			if(item->assigned_number < INT64_MIN || item->assigned_number > INT64_MAX)
				FAIL("enum assigned number is outside int64 storage domain");
			p[i].items[j] = asn1typed_render_cpp_final_name(item->source_name, ASN1TYPED_NAME_FIELD);
			if(!safe_symbol(p[i].items[j])) FAIL("enum item spelling is unsafe, a header macro, or unavailable");
			if(!strcmp(p[i].items[j], "Known")) FAIL("enum item collides with nested Known name");
			for(k = 0; k < j; ++k) if(!strcmp(p[i].items[k], p[i].items[j]))
				FAIL("enum item final name collision");
			if(item->is_extension_addition) ++p[i].additions;
			else { ++p[i].roots; if(!item->per_enumeration_index) p[i].default_item = j; }
		}
		if(!p[i].roots || p[i].roots > 255) FAIL("enum root count is outside supported 1..255 domain");
	}
	/* Compare declarations in their actual namespace scopes. */
	for(i = 0; i < m->type_count; ++i) {
		const char *current[] = {p[i].type, p[i].mapping, p[i].encode, p[i].decode};
		for(j = 0; j < 4; ++j) {
			if(!strcmp(current[j], "enum_codec")) FAIL("enum declaration collides with helper namespace");
			for(k = 0; k < m->type_count; ++k) {
				const char *other[] = {p[k].type, p[k].mapping, p[k].encode, p[k].decode};
				size_t q;
				for(q = 0; q < 4; ++q) if((i != k || j != q) && !strcmp(current[j], other[q]))
					FAIL("enum type, mapping or API final name collision");
				if(i != k && (!strcmp(p[i].put, p[k].put) || !strcmp(p[i].get, p[k].get)))
					FAIL("enum field helper final name collision");
			}
		}
	}
	*out = p;
#undef FAIL
	return 0;
fail:
	clear_plan(p, m);
#undef FAIL
	return -1;
}
static int
number(struct enum_buf *b, intmax_t n) {
	if(n == INT64_MIN) return append(b, "(-INT64_C(9223372036854775807) - INT64_C(1))");
	if(n < 0) return format(b, "(-INT64_C(%" PRIdMAX "))", -n);
	return format(b, "INT64_C(%" PRIdMAX ")", n);
}
static int
emit_types(struct enum_buf *b, const asn1typed_module_t *m, struct enum_type_plan *p) {
	size_t i, j;
	for(i = 0; i < m->type_count; ++i) {
		const asn1typed_type_t *t = &m->types[i];
		if(format(b, "struct %s {\n    enum class Known : ::std::int64_t {\n", p[i].type)) return -1;
		for(j = 0; j < t->enum_item_count; ++j)
			if(format(b, "        %s = ", p[i].items[j]) || number(b, t->enum_items[j].assigned_number) || append(b, ",\n")) return -1;
		if(append(b, "    };\n")) return -1;
		if(t->is_extensible) {
			if(format(b, "    struct UnknownExtension { ::std::uint64_t index; };\n    ::std::variant<Known, UnknownExtension> value{Known::%s};\n};\n", p[i].items[p[i].default_item])) return -1;
		} else if(format(b, "    Known value{Known::%s};\n};\n", p[i].items[p[i].default_item])) return -1;
	}
	return 0;
}
static int
emit_mapping(struct enum_buf *b, const asn1typed_module_t *m, struct enum_type_plan *p) {
	size_t i, j, k;
	for(i = 0; i < m->type_count; ++i) {
		const asn1typed_type_t *t = &m->types[i];
		if(format(b, "struct %s {\n    static constexpr ::std::size_t root_count = %zu;\n    static constexpr ::std::size_t known_addition_count = %zu;\n    static constexpr bool extensible = %s;\n    struct Entry { %s::Known value; ::std::int64_t assigned_number; bool is_extension; ::std::size_t per_index; };\n    static constexpr ::std::array<Entry, %zu> entries{{\n", p[i].mapping, p[i].roots, p[i].additions, t->is_extensible ? "true" : "false", p[i].qualified_type, t->enum_item_count)) return -1;
		for(j = 0; j < t->enum_item_count; ++j) {
			if(format(b, "        {%s::Known::%s, ", p[i].qualified_type, p[i].items[j]) || number(b, t->enum_items[j].assigned_number) || format(b, ", %s, %zu},\n", t->enum_items[j].is_extension_addition ? "true" : "false", t->enum_items[j].per_enumeration_index)) return -1;
		}
		if(format(b, "    }};\n    static constexpr ::std::array<::std::size_t, %zu> source_ordinal_to_per_index{{", t->enum_item_count)) return -1;
		for(j = 0; j < t->enum_item_count; ++j) if(format(b, "%s%zu", j ? ", " : "", t->enum_items[j].per_enumeration_index)) return -1;
		if(append(b, "}};\n")) return -1;
		for(k = 0; k < 2; ++k) {
			size_t count = k ? p[i].additions : p[i].roots;
			if(format(b, "    static constexpr ::std::array<::std::size_t, %zu> %s_index_to_source_ordinal{{", count, k ? "addition" : "root")) return -1;
			for(j = 0; j < count; ++j) {
				size_t q;
				for(q = 0; q < t->enum_item_count; ++q) if((size_t)t->enum_items[q].is_extension_addition == k && t->enum_items[q].per_enumeration_index == j) break;
				if(q == t->enum_item_count || format(b, "%s%zu", j ? ", " : "", q)) return -1;
			}
			if(append(b, "}};\n")) return -1;
		}
		if(append(b, "    static_assert(root_count >= 1 && root_count <= 255);\n    static_assert(known_addition_count <= UINT64_MAX);\n};\n")) return -1;
	}
	return 0;
}
static int
emit_codec(struct enum_buf *b, const asn1typed_module_t *m, struct enum_type_plan *p) {
	size_t i;
	if(append(b, "namespace enum_codec {\n")) return -1;
	for(i = 0; i < m->type_count; ++i) {
		const asn1typed_type_t *t = &m->types[i];
		if(format(b, "inline ::nrforge::aper::Result<void> %s(::nrforge::aper::FieldWriter& f, const %s& v) {\n", p[i].put, p[i].type)) return -1;
		if(t->is_extensible) {
			if(format(b, "    if(const auto* known = ::std::get_if<%s::Known>(&v.value)) {\n        for(const auto& e : %s::entries) if(e.value == *known) return f.write_enumerated({e.is_extension, static_cast<::std::uint64_t>(e.per_index)}, static_cast<unsigned>(%s::root_count), true);\n    } else if(const auto* unknown = ::std::get_if<%s::UnknownExtension>(&v.value)) {\n        if(unknown->index >= %s::known_addition_count) return f.write_enumerated({true, unknown->index}, static_cast<unsigned>(%s::root_count), true);\n    }\n", p[i].type, p[i].mapping, p[i].mapping, p[i].type, p[i].mapping, p[i].mapping)) return -1;
		} else if(format(b, "    for(const auto& e : %s::entries) if(e.value == v.value) return f.write_enumerated({false, static_cast<::std::uint64_t>(e.per_index)}, static_cast<unsigned>(%s::root_count), false);\n", p[i].mapping, p[i].mapping)) return -1;
		if(format(b, "    return f.write_enumerated({false, static_cast<::std::uint64_t>(%s::root_count)}, static_cast<unsigned>(%s::root_count), %s::extensible);\n}\ninline ::nrforge::aper::Result<%s> %s(::nrforge::aper::FieldReader& f) {\n    auto index = f.read_enumerated(static_cast<unsigned>(%s::root_count), %s::extensible);\n    if(!index) return ::nrforge::aper::Result<%s>::failure(index.error());\n    %s v{};\n", p[i].mapping, p[i].mapping, p[i].mapping, p[i].type, p[i].get, p[i].mapping, p[i].mapping, p[i].type, p[i].type)) return -1;
		if(t->is_extensible && format(b, "    if(index.value().is_extension) {\n        if(index.value().index < %s::known_addition_count) v.value = %s::entries[%s::addition_index_to_source_ordinal[static_cast<::std::size_t>(index.value().index)]].value;\n        else v.value = %s::UnknownExtension{index.value().index};\n    } else ", p[i].mapping, p[i].mapping, p[i].mapping, p[i].type)) return -1;
		if(format(b, "v.value = %s::entries[%s::root_index_to_source_ordinal[static_cast<::std::size_t>(index.value().index)]].value;\n    return ::nrforge::aper::Result<%s>::success(v);\n}\n", p[i].mapping, p[i].mapping, p[i].type)) return -1;
	}
	if(append(b, "} // namespace enum_codec\n")) return -1;
	for(i = 0; i < m->type_count; ++i) {
		if(format(b, "inline ::nrforge::aper::Result<::nrforge::aper::CompleteEncoding> %s(const %s& v, const ::nrforge::aper::Limits& limits = {}) {\n    return ::nrforge::aper::encode_complete(v, limits, [&](::nrforge::aper::FieldWriter& f) { return enum_codec::%s(f, v); });\n}\ninline ::nrforge::aper::Result<%s> %s(::std::span<const ::std::byte> input, const ::nrforge::aper::Limits& limits = {}) {\n    return ::nrforge::aper::decode_complete<%s>(input, limits, [](::nrforge::aper::FieldReader& f) { return enum_codec::%s(f); });\n}\n", p[i].encode, p[i].type, p[i].put, p[i].type, p[i].decode, p[i].type, p[i].get)) return -1;
	}
	return 0;
}
static int
render(const asn1typed_module_t *m, const char *ns, char **out,
		char *diagnostic, size_t size, int mode) {
	struct enum_type_plan *p = NULL;
	struct enum_buf b = {NULL, 0};
	char why[512] = "invalid enum renderer arguments";
	int result = -1;
	if(out) *out = NULL;
	if(diagnostic && size) diagnostic[0] = 0;
	if(!out) goto done;
	if(preflight(m, ns, &p, why, sizeof(why))) goto done;
	/* Includes must be outside the requested namespace. */
	if(append(&b, mode == 0 ? "#include <cstdint>\n#include <variant>\n" : mode == 1 ? "#include <array>\n#include <cstddef>\n#include <cstdint>\n" : "#include <span>\n#include <cstddef>\n")) goto oom;
	if(format(&b, "namespace %s {\n", ns)) goto oom;
	if((mode == 0 ? emit_types(&b, m, p) : mode == 1 ? emit_mapping(&b, m, p) : emit_codec(&b, m, p)) || append(&b, "} // namespace\n")) goto oom;
	*out = b.text; b.text = NULL; result = 0; goto done;
oom:
	snprintf(why, sizeof(why), "out of memory emitting enum output");
done:
	free(b.text);
	if(p) clear_plan(p, m);
	if(result && diagnostic && size) snprintf(diagnostic, size, "%s", why);
	return result;
}
int
asn1typed_render_cpp_owned_enum_types(const asn1typed_module_t *m, const char *ns,
		char **out, char *diagnostic, size_t size) { return render(m, ns, out, diagnostic, size, 0); }
int
asn1typed_render_cpp_owned_enum_mapping(const asn1typed_module_t *m, const char *ns,
		char **out, char *diagnostic, size_t size) { return render(m, ns, out, diagnostic, size, 1); }
int
asn1typed_render_cpp_owned_enum_codec(const asn1typed_module_t *m, const char *ns,
		char **out, char *diagnostic, size_t size) { return render(m, ns, out, diagnostic, size, 2); }
