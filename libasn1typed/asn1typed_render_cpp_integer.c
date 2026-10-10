#include "asn1typed_render_cpp.h"
#include "asn1typed_render_cpp_internal.h"
#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct integer_buf { char *text; size_t length; };
struct integer_type_plan {
	char *type, *qualified_type, *constraint, *mapping, *encode, *decode, *put, *get;
	/* Cardinality offset width, not the fixed width of an APER payload. */
	unsigned bits;
	uint64_t distance;
	int64_t lower, upper;
	int is_signed, extensible;
	const asn1typed_integer_value_range_t *range;
	char lower_literal[96], upper_literal[96];
};
static int
append(struct integer_buf *b, const char *text) {
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
format(struct integer_buf *b, const char *fmt, ...) {
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
		!t->has_valid_per_root_mapping && !t->choice_root_only_extension_owned && !t->has_valid_per_enumeration_mapping &&
		t->sequence_extension_evidence == ASN1TYPED_WIRE_EVIDENCE_UNAVAILABLE &&
		!t->sequence_root_field_count && !t->sequence_known_addition_count &&
		!t->has_valid_sequence_extension_structure &&
		empty_ref(&t->element_type) && empty_ref(&t->ioc_container) &&
		!t->has_ioc_table && !t->ioc_object_set_is_extensible && !t->is_extensible &&
		!t->size_constraint.has_size_constraint && !t->size_constraint.is_extensible &&
		!t->size_constraint.lower_bound && !t->size_constraint.upper_bound && !t->size_constraint.has_extension_addition && !t->size_constraint.extension_lower_bound && !t->size_constraint.extension_upper_bound;
}
static void
clear_plan(struct integer_type_plan *p, const asn1typed_module_t *m) {
	size_t i;
	if(!p) return;
	for(i = 0; i < m->type_count; ++i) {
		free(p[i].type); free(p[i].qualified_type); free(p[i].constraint); free(p[i].mapping);
		free(p[i].encode); free(p[i].decode); free(p[i].put); free(p[i].get);
	}
	free(p);
}
static void
integer_literal(char *out, size_t size, int64_t value, int signed_type) {
	if(!signed_type) snprintf(out, size, "UINT64_C(%" PRIu64 ")", (uint64_t)value);
	else if(value == INT64_MIN) snprintf(out, size, "(-INT64_C(9223372036854775807) - INT64_C(1))");
	else if(value < 0) snprintf(out, size, "(-INT64_C(%" PRIu64 "))", (uint64_t)(-value));
	else snprintf(out, size, "INT64_C(%" PRId64 ")", value);
}
static int
preflight(const asn1typed_module_t *m, const char *ns,
		struct integer_type_plan **out, char *why, size_t size, int extended) {
	struct integer_type_plan *p;
	size_t i, j, k, q;
#define FAIL(message) do { if(why && size) snprintf(why, size, "%s", message); goto fail; } while(0)
	*out = NULL;
	if(!m || !m->source_name || !m->source_name[0] || !m->types || !m->type_count ||
		m->type_count > m->type_capacity || m->type_count > SIZE_MAX / sizeof(*p) ||
		m->bound_instance_count || m->bound_instances || m->bound_instance_capacity ||
		m->ioc_registries || m->ioc_registry_count || m->ioc_registry_capacity) {
		if(why && size) snprintf(why, size, "integer-only module has invalid storage or unsupported bound instances");
		return -1;
	}
	if(!asn1typed_render_cpp_safe_namespace(ns) ||
		!strcmp(ns, "nrforge::aper") || !strncmp(ns, "nrforge::aper::", 15)) {
		if(why && size) snprintf(why, size, "invalid, unsafe or reserved runtime namespace for integer-only generation");
		return -1;
	}
	p = (struct integer_type_plan *)calloc(m->type_count, sizeof(*p));
	if(!p) { if(why && size) snprintf(why, size, "out of memory planning integer generation"); return -1; }
	for(i = 0; i < m->type_count; ++i) {
		const asn1typed_type_t *t = &m->types[i];
		const asn1typed_integer_value_range_t *range = &t->value_range;
		char *base, *prefix;
		if(!supported_metadata(t)) FAIL("unsupported integer-only type metadata or kind");
		if(!t->identity.module || strcmp(t->identity.module, m->source_name) ||
			!t->identity.source_name || !t->identity.source_name[0]) FAIL("invalid integer type identity");
		if(range->has_value_range != 1 || (!extended && (range->is_extensible || range->tail || range->tail_count)) ||
            (range->is_extensible != 0 && range->is_extensible != 1) ||
            (!!range->tail != !!range->tail_count) || range->tail_count > SIZE_MAX / sizeof(*range->tail) ||
            range->lower_bound > range->upper_bound)
			FAIL("unsupported integer constraint: requires one finite non-extensible interval");
		if(range->lower_bound < INT64_MIN || range->upper_bound > INT64_MAX)
			FAIL("integer interval exceeds int64 evidence domain");
		p[i].lower = (int64_t)range->lower_bound;
		p[i].upper = (int64_t)range->upper_bound;
        for(j = 0; j < range->tail_count; ++j) {
            const asn1typed_integer_interval_t *a = &range->tail[j];
            if(a->lower_bound < INT64_MIN || a->upper_bound > INT64_MAX || a->lower_bound > a->upper_bound ||
                p[i].upper == INT64_MAX || a->lower_bound <= p[i].upper + INT64_C(1))
                FAIL("INTEGER root set is not finite canonical disjoint intervals");
            p[i].upper = (int64_t)a->upper_bound;
        }
        if((!!range->extension_additions != !!range->extension_addition_count) ||
            (range->extension_addition_count && !range->is_extensible) ||
            range->extension_addition_count > SIZE_MAX / sizeof(*range->extension_additions)) FAIL("invalid INTEGER known extension evidence");
        for(j = 0; j < range->extension_addition_count; ++j) {
            const asn1typed_integer_interval_t *a = &range->extension_additions[j];
            if(a->lower_bound < INT64_MIN || a->upper_bound > INT64_MAX || a->lower_bound > a->upper_bound ||
                (j && (range->extension_additions[j-1].upper_bound == INTMAX_MAX || a->lower_bound <= range->extension_additions[j-1].upper_bound + 1)))
                FAIL("INTEGER known additions are not canonical finite intervals");
        }
        p[i].range = range; p[i].extensible = range->is_extensible;
        p[i].is_signed = p[i].lower < 0 || p[i].extensible || range->tail_count;
		p[i].distance = (uint64_t)p[i].upper - (uint64_t)p[i].lower;
		{ uint64_t remaining = p[i].distance; while(remaining) { ++p[i].bits; remaining >>= 1; } }
		integer_literal(p[i].lower_literal, sizeof(p[i].lower_literal), p[i].lower, p[i].is_signed);
		integer_literal(p[i].upper_literal, sizeof(p[i].upper_literal), p[i].upper, p[i].is_signed);
		p[i].type = asn1typed_render_cpp_final_name(t->identity.source_name, ASN1TYPED_NAME_TYPE);
		base = asn1typed_render_cpp_final_name(t->identity.source_name, ASN1TYPED_NAME_FIELD);
		if(!p[i].type || !base) { free(base); FAIL("invalid integer spelling or out of memory naming integer"); }
		prefix = join("::", ns, "::");
		if(!prefix) { free(base); FAIL("out of memory qualifying integer type"); }
		p[i].qualified_type = join(prefix, p[i].type, ""); free(prefix);
		p[i].constraint = join("", p[i].type, "_constraint");
		p[i].mapping = join("", p[i].type, "_aper");
		p[i].encode = join("encode_", base, ""); p[i].decode = join("decode_", base, "");
		p[i].put = join("put_", p[i].type, ""); p[i].get = join("get_", p[i].type, ""); free(base);
		if(!p[i].qualified_type || !p[i].constraint || !p[i].mapping || !p[i].encode ||
			!p[i].decode || !p[i].put || !p[i].get) FAIL("out of memory constructing integer helper names");
	}
	for(i = 0; i < m->type_count; ++i) {
		const char *current[] = {p[i].type, p[i].constraint, p[i].mapping, p[i].encode, p[i].decode};
		for(j = 0; j < 5; ++j) {
			if(asn1typed_render_cpp_header_macro(current[j])) FAIL("integer final spelling conflicts with a standard header macro");
			if(!strcmp(current[j], "integer_codec")) FAIL("integer declaration collides with helper namespace");
			for(k = 0; k < m->type_count; ++k) {
				const char *other[] = {p[k].type, p[k].constraint, p[k].mapping, p[k].encode, p[k].decode};
				for(q = 0; q < 5; ++q) if((i != k || j != q) && !strcmp(current[j], other[q]))
					FAIL("integer type, constraint, mapping or API final name collision");
				if(i != k && (!strcmp(p[i].put, p[k].put) || !strcmp(p[i].get, p[k].get)))
					FAIL("integer field helper final name collision");
			}
		}
		if(asn1typed_render_cpp_header_macro(p[i].put) || asn1typed_render_cpp_header_macro(p[i].get))
			FAIL("integer field helper spelling conflicts with a standard header macro");
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
emit(struct integer_buf *b, const asn1typed_module_t *m,
		struct integer_type_plan *p, int mode) {
	size_t i;
	if(mode == 2 && append(b, "namespace integer_codec {\n")) return -1;
	for(i = 0; i < m->type_count; ++i) {
		const char *value_type = p[i].is_signed ? "::std::int64_t" : "::std::uint64_t";
		if(mode == 0) {
			if(format(b, "using %s = %s;\nstruct %s {\n    static constexpr %s lower_bound = %s;\n    static constexpr %s upper_bound = %s;\n};\n",
				p[i].type, value_type, p[i].constraint, value_type, p[i].lower_literal,
				value_type, p[i].upper_literal)) return -1;
		} else if(mode == 1) {
			if(format(b, "struct %s {\n    using value_type = %s;\n    static constexpr value_type lower_bound = %s;\n    static constexpr value_type upper_bound = %s;\n    // Offset/cardinality bit width; large-range wire payload is variable length.\n    static constexpr unsigned root_bits = %u;\n    static constexpr ::std::uint64_t cardinality_minus_one = UINT64_C(%" PRIu64 ");\n    static constexpr bool cardinality_is_full = %s;\n    static constexpr bool extensible = %s;\n",
				p[i].mapping, p[i].qualified_type, p[i].lower_literal, p[i].upper_literal,
				p[i].bits, p[i].distance, p[i].distance == UINT64_MAX ? "true" : "false", p[i].extensible ? "true" : "false")) return -1;
            if(p[i].extensible || p[i].range->tail_count) {
                size_t j;
                char lo[96], hi[96];
                if(format(b, "    // PER-visible hull differs from retained permitted root membership.\n    static constexpr ::nrforge::aper::IntegerInterval root_intervals[%zu] = {", p[i].range->tail_count + 1)) return -1;
                integer_literal(hi, sizeof(hi), (int64_t)p[i].range->upper_bound, 1);
                if(format(b, "{%s, %s}", p[i].lower_literal, hi)) return -1;
                for(j = 0; j < p[i].range->tail_count; ++j) {
                    integer_literal(lo, sizeof(lo), (int64_t)p[i].range->tail[j].lower_bound, 1);
                    integer_literal(hi, sizeof(hi), (int64_t)p[i].range->tail[j].upper_bound, 1);
                    if(format(b, ", {%s, %s}", lo, hi)) return -1;
                }
                if(append(b, "};\n")) return -1;
            }
            if(p[i].range->extension_addition_count) {
                size_t j; char lo[96], hi[96];
                if(format(b, "    // Known additions do not close the open extension domain.\n    static constexpr ::nrforge::aper::IntegerInterval known_extension_intervals[%zu] = {", p[i].range->extension_addition_count)) return -1;
                for(j = 0; j < p[i].range->extension_addition_count; ++j) {
                    integer_literal(lo, sizeof(lo), (int64_t)p[i].range->extension_additions[j].lower_bound, 1);
                    integer_literal(hi, sizeof(hi), (int64_t)p[i].range->extension_additions[j].upper_bound, 1);
                    if(format(b, "%s{%s, %s}", j ? ", " : "", lo, hi)) return -1;
                }
                if(append(b, "};\n")) return -1;
            }
            if(append(b, "};\n")) return -1;
		} else {
            if(p[i].extensible || p[i].range->tail_count) {
                if(format(b, "inline ::nrforge::aper::Result<void> %s(::nrforge::aper::FieldWriter& f, const %s& v) {\n    return f.write_integer_set(v, %s::root_intervals, %s::extensible);\n}\ninline ::nrforge::aper::Result<%s> %s(::nrforge::aper::FieldReader& f) {\n    return f.read_integer_set(%s::root_intervals, %s::extensible);\n}\n", p[i].put, p[i].qualified_type, p[i].mapping, p[i].mapping, p[i].qualified_type, p[i].get, p[i].mapping, p[i].mapping)) return -1;
                continue;
            }
			const char *suffix = p[i].is_signed ? "int" : "uint";
			if(format(b, "inline ::nrforge::aper::Result<void> %s(::nrforge::aper::FieldWriter& f, const %s& v) {\n    return f.write_bounded_%s(v, %s::lower_bound, %s::upper_bound);\n}\ninline ::nrforge::aper::Result<%s> %s(::nrforge::aper::FieldReader& f) {\n    return f.read_bounded_%s(%s::lower_bound, %s::upper_bound);\n}\n",
				p[i].put, p[i].qualified_type, suffix, p[i].mapping, p[i].mapping,
				p[i].qualified_type, p[i].get, suffix, p[i].mapping, p[i].mapping)) return -1;
		}
	}
	if(mode == 2) {
		if(append(b, "} // namespace integer_codec\n")) return -1;
		for(i = 0; i < m->type_count; ++i)
			if(format(b, "inline ::nrforge::aper::Result<::nrforge::aper::CompleteEncoding> %s(const %s& v, const ::nrforge::aper::Limits& limits = {}) {\n    return ::nrforge::aper::encode_complete(v, limits, [&](::nrforge::aper::FieldWriter& f) { return integer_codec::%s(f, v); });\n}\ninline ::nrforge::aper::Result<%s> %s(::std::span<const ::std::byte> input, const ::nrforge::aper::Limits& limits = {}) {\n    return ::nrforge::aper::decode_complete<%s>(input, limits, [](::nrforge::aper::FieldReader& f) { return integer_codec::%s(f); });\n}\n",
				p[i].encode, p[i].qualified_type, p[i].put, p[i].qualified_type,
				p[i].decode, p[i].qualified_type, p[i].get)) return -1;
	}
	return 0;
}
static int
render(const asn1typed_module_t *m, const char *ns, char **out,
		char *diagnostic, size_t size, int mode, int extended) {
	struct integer_type_plan *p = NULL;
	struct integer_buf b = {NULL, 0};
	char why[512] = "invalid integer renderer arguments";
	int result = -1;
	if(out) *out = NULL;
	if(diagnostic && size) diagnostic[0] = 0;
	if(!out) goto done;
	if(preflight(m, ns, &p, why, sizeof(why), extended)) goto done;
	if(append(&b, mode == 2 ? "#include <span>\n#include <cstddef>\n" : "#include <cstdint>\n") ||
		format(&b, "namespace %s {\n", ns) || emit(&b, m, p, mode) || append(&b, "} // namespace\n")) {
		snprintf(why, sizeof(why), "out of memory emitting integer output"); goto done;
	}
	*out = b.text; b.text = NULL; result = 0;
done:
	free(b.text); if(p) clear_plan(p, m);
	if(result && diagnostic && size) snprintf(diagnostic, size, "%s", why);
	return result;
}
int
asn1typed_render_cpp_owned_integer_types(const asn1typed_module_t *m, const char *ns,
		char **out, char *diagnostic, size_t size) { return render(m, ns, out, diagnostic, size, 0, 0); }
int
asn1typed_render_cpp_owned_integer_mapping(const asn1typed_module_t *m, const char *ns,
		char **out, char *diagnostic, size_t size) { return render(m, ns, out, diagnostic, size, 1, 0); }
int
asn1typed_render_cpp_owned_integer_codec(const asn1typed_module_t *m, const char *ns,
		char **out, char *diagnostic, size_t size) { return render(m, ns, out, diagnostic, size, 2, 0); }

int
asn1typed_render_cpp_owned_integer_set_types(const asn1typed_module_t *m, const char *ns,
        char **out, char *diagnostic, size_t size) { return render(m, ns, out, diagnostic, size, 0, 1); }

int
asn1typed_render_cpp_owned_integer_set_mapping(const asn1typed_module_t *m, const char *ns,
        char **out, char *diagnostic, size_t size) { return render(m, ns, out, diagnostic, size, 1, 1); }

int
asn1typed_render_cpp_owned_integer_set_codec(const asn1typed_module_t *m, const char *ns,
        char **out, char *diagnostic, size_t size) { return render(m, ns, out, diagnostic, size, 2, 1); }
