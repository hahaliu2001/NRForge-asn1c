#include "asn1typed_render_cpp.h"
#include "asn1typed_render_cpp_internal.h"
#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct uint_buf { char *text; size_t length; };
struct uint_type_plan {
	char *type, *qualified_type, *constraint, *mapping, *encode, *decode, *put, *get;
	unsigned bits;
	uint64_t upper;
};
static int
append(struct uint_buf *b, const char *text) {
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
format(struct uint_buf *b, const char *fmt, ...) {
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
supported_metadata(const asn1typed_type_t *t) {
	return t->kind == ASN1TYPED_TYPE_PRIMITIVE && t->primitive_kind == ASN1TYPED_PRIMITIVE_INTEGER &&
		!t->fields && !t->field_count && !t->field_capacity &&
		!t->alternatives && !t->alternative_count && !t->alternative_capacity &&
		!t->enum_items && !t->enum_item_count && !t->enum_item_capacity &&
		!t->has_valid_per_root_mapping && !t->has_valid_per_enumeration_mapping &&
		empty_ref(&t->element_type) && empty_ref(&t->ioc_container) &&
		!t->has_ioc_table && !t->ioc_object_set_is_extensible && !t->is_extensible &&
		!t->size_constraint.has_size_constraint && !t->size_constraint.is_extensible &&
		!t->size_constraint.lower_bound && !t->size_constraint.upper_bound;
}
static void
clear_plan(struct uint_type_plan *p, const asn1typed_module_t *m) {
	size_t i;
	if(!p) return;
	for(i = 0; i < m->type_count; ++i) {
		free(p[i].type); free(p[i].qualified_type); free(p[i].constraint); free(p[i].mapping);
		free(p[i].encode); free(p[i].decode); free(p[i].put); free(p[i].get);
	}
	free(p);
}
static int
preflight(const asn1typed_module_t *m, const char *ns,
		struct uint_type_plan **out, char *why, size_t size) {
	struct uint_type_plan *p;
	size_t i, j, k, q;
#define FAIL(message) do { if(why && size) snprintf(why, size, "%s", message); goto fail; } while(0)
	*out = NULL;
	if(!m || !m->source_name || !m->source_name[0] || !m->types || !m->type_count ||
		m->type_count > m->type_capacity || m->type_count > SIZE_MAX / sizeof(*p) ||
		m->bound_instance_count || m->bound_instances || m->bound_instance_capacity) {
		if(why && size) snprintf(why, size, "uint-only module has invalid storage or unsupported bound instances");
		return -1;
	}
	if(!asn1typed_render_cpp_safe_namespace(ns)) {
		if(why && size) snprintf(why, size, "invalid or unsafe namespace for uint-only generation");
		return -1;
	}
	p = (struct uint_type_plan *)calloc(m->type_count, sizeof(*p));
	if(!p) { if(why && size) snprintf(why, size, "out of memory planning uint generation"); return -1; }
	for(i = 0; i < m->type_count; ++i) {
		const asn1typed_type_t *t = &m->types[i];
		const asn1typed_integer_value_range_t *range = &t->value_range;
		char *base, *prefix;
		if(!supported_metadata(t)) FAIL("unsupported uint-only type metadata or kind");
		if(!t->identity.module || strcmp(t->identity.module, m->source_name) ||
			!t->identity.source_name || !t->identity.source_name[0]) FAIL("invalid uint type identity");
		if(range->has_value_range != 1 || range->lower_bound != 0 || range->is_extensible ||
			range->tail || range->tail_count) FAIL("unsupported uint constraint: requires one non-extensible zero-based root range");
		if(range->upper_bound == 255) p[i].bits = 8;
		else if(range->upper_bound == 65535) p[i].bits = 16;
		else if(range->upper_bound == INT64_C(4294967295)) p[i].bits = 32;
		else if(range->upper_bound == INT64_C(1099511627775)) p[i].bits = 40;
		else FAIL("unsupported uint constraint domain: requires exactly 8, 16, 32 or 40 root bits");
		p[i].upper = (uint64_t)range->upper_bound;
		p[i].type = asn1typed_render_cpp_final_name(t->identity.source_name, ASN1TYPED_NAME_TYPE);
		base = asn1typed_render_cpp_final_name(t->identity.source_name, ASN1TYPED_NAME_FIELD);
		if(!p[i].type || !base) { free(base); FAIL("invalid uint spelling or out of memory naming uint"); }
		prefix = join("::", ns, "::");
		if(!prefix) { free(base); FAIL("out of memory qualifying uint type"); }
		p[i].qualified_type = join(prefix, p[i].type, ""); free(prefix);
		p[i].constraint = join("", p[i].type, "_constraint");
		p[i].mapping = join("", p[i].type, "_aper");
		p[i].encode = join("encode_", base, ""); p[i].decode = join("decode_", base, "");
		p[i].put = join("put_", p[i].type, ""); p[i].get = join("get_", p[i].type, ""); free(base);
		if(!p[i].qualified_type || !p[i].constraint || !p[i].mapping || !p[i].encode ||
			!p[i].decode || !p[i].put || !p[i].get) FAIL("out of memory constructing uint helper names");
	}
	for(i = 0; i < m->type_count; ++i) {
		const char *current[] = {p[i].type, p[i].constraint, p[i].mapping, p[i].encode, p[i].decode};
		for(j = 0; j < 5; ++j) {
			if(asn1typed_render_cpp_header_macro(current[j])) FAIL("uint final spelling conflicts with a standard header macro");
			if(!strcmp(current[j], "uint_codec")) FAIL("uint declaration collides with helper namespace");
			for(k = 0; k < m->type_count; ++k) {
				const char *other[] = {p[k].type, p[k].constraint, p[k].mapping, p[k].encode, p[k].decode};
				for(q = 0; q < 5; ++q) if((i != k || j != q) && !strcmp(current[j], other[q]))
					FAIL("uint type, constraint, mapping or API final name collision");
				if(i != k && (!strcmp(p[i].put, p[k].put) || !strcmp(p[i].get, p[k].get)))
					FAIL("uint field helper final name collision");
			}
		}
		if(asn1typed_render_cpp_header_macro(p[i].put) || asn1typed_render_cpp_header_macro(p[i].get))
			FAIL("uint field helper spelling conflicts with a standard header macro");
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
emit(struct uint_buf *b, const asn1typed_module_t *m, struct uint_type_plan *p, int mode) {
	size_t i;
	if(mode == 2 && append(b, "namespace uint_codec {\n")) return -1;
	for(i = 0; i < m->type_count; ++i) {
		if(mode == 0) {
			if(format(b, "using %s = ::std::uint64_t;\nstruct %s {\n    static constexpr ::std::uint64_t lower_bound = UINT64_C(0);\n    static constexpr ::std::uint64_t upper_bound = UINT64_C(%" PRIu64 ");\n};\n", p[i].type, p[i].constraint, p[i].upper)) return -1;
		} else if(mode == 1) {
			unsigned bits = p[i].bits;
			if(format(b, "struct %s {\n    using value_type = %s;\n    static constexpr unsigned root_bits = %u;\n    static constexpr ::std::uint64_t lower_bound = UINT64_C(0);\n    static constexpr ::std::uint64_t upper_bound = UINT64_C(%" PRIu64 ");\n    static constexpr bool extensible = false;\n    static constexpr bool align_before_payload_to_octet = true;\n    static constexpr unsigned length_prefix_bits = %u;\n    static constexpr unsigned minimum_payload_octets = %u;\n    static constexpr unsigned maximum_payload_octets = %u;\n    static constexpr bool minimal_payload = %s;\n    static constexpr bool most_significant_octet_first = true;\n};\n", p[i].mapping, p[i].qualified_type, bits, p[i].upper, bits == 32 ? 2u : bits == 40 ? 3u : 0u, bits == 16 ? 2u : 1u, bits / 8, bits > 16 ? "true" : "false")) return -1;
		} else {
			if(format(b, "inline ::nrforge::aper::Result<void> %s(::nrforge::aper::FieldWriter& f, const %s& v) {\n    return f.write_constrained_uint(v, %s::root_bits);\n}\ninline ::nrforge::aper::Result<%s> %s(::nrforge::aper::FieldReader& f) {\n    return f.read_constrained_uint(%s::root_bits);\n}\n", p[i].put, p[i].type, p[i].mapping, p[i].type, p[i].get, p[i].mapping)) return -1;
		}
	}
	if(mode == 2) {
		if(append(b, "} // namespace uint_codec\n")) return -1;
		for(i = 0; i < m->type_count; ++i)
			if(format(b, "inline ::nrforge::aper::Result<::nrforge::aper::CompleteEncoding> %s(const %s& v, const ::nrforge::aper::Limits& limits = {}) {\n    return ::nrforge::aper::encode_complete(v, limits, [&](::nrforge::aper::FieldWriter& f) { return uint_codec::%s(f, v); });\n}\ninline ::nrforge::aper::Result<%s> %s(::std::span<const ::std::byte> input, const ::nrforge::aper::Limits& limits = {}) {\n    return ::nrforge::aper::decode_complete<%s>(input, limits, [](::nrforge::aper::FieldReader& f) { return uint_codec::%s(f); });\n}\n", p[i].encode, p[i].type, p[i].put, p[i].type, p[i].decode, p[i].type, p[i].get)) return -1;
	}
	return 0;
}
static int
render(const asn1typed_module_t *m, const char *ns, char **out,
		char *diagnostic, size_t size, int mode) {
	struct uint_type_plan *p = NULL;
	struct uint_buf b = {NULL, 0};
	char why[512] = "invalid uint renderer arguments";
	int result = -1;
	if(out) *out = NULL;
	if(diagnostic && size) diagnostic[0] = 0;
	if(!out) goto done;
	if(preflight(m, ns, &p, why, sizeof(why))) goto done;
	if(append(&b, mode == 2 ? "#include <span>\n#include <cstddef>\n" : "#include <cstdint>\n") ||
		format(&b, "namespace %s {\n", ns) || emit(&b, m, p, mode) || append(&b, "} // namespace\n")) {
		snprintf(why, sizeof(why), "out of memory emitting uint output"); goto done;
	}
	*out = b.text; b.text = NULL; result = 0;
done:
	free(b.text); if(p) clear_plan(p, m);
	if(result && diagnostic && size) snprintf(diagnostic, size, "%s", why);
	return result;
}
int
asn1typed_render_cpp_owned_uint_types(const asn1typed_module_t *m, const char *ns,
		char **out, char *diagnostic, size_t size) { return render(m, ns, out, diagnostic, size, 0); }
int
asn1typed_render_cpp_owned_uint_mapping(const asn1typed_module_t *m, const char *ns,
		char **out, char *diagnostic, size_t size) { return render(m, ns, out, diagnostic, size, 1); }
int
asn1typed_render_cpp_owned_uint_codec(const asn1typed_module_t *m, const char *ns,
		char **out, char *diagnostic, size_t size) { return render(m, ns, out, diagnostic, size, 2); }
