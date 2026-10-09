#include "asn1typed_render_cpp.h"
#include "asn1typed_render_cpp_internal.h"
#include "asn1typed_name.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct codec_buf { char *data; size_t size; };

static int
append(struct codec_buf *b, const char *s) {
	size_t n = strlen(s);
	char *p;
	if(n > SIZE_MAX - b->size - 1) return -1;
	p = realloc(b->data, b->size + n + 1);
	if(!p) return -1;
	memcpy(p + b->size, s, n + 1);
	b->data = p;
	b->size += n;
	return 0;
}

static int
format(struct codec_buf *b, const char *fmt, ...) {
	va_list ap;
	int n;
	char *text;
	va_start(ap, fmt);
	n = vsnprintf(NULL, 0, fmt, ap);
	va_end(ap);
	if(n < 0 || !(text = malloc((size_t)n + 1))) return -1;
	va_start(ap, fmt);
	(void)vsnprintf(text, (size_t)n + 1, fmt, ap);
	va_end(ap);
	n = append(b, text);
	free(text);
	return n;
}

static char *
final_name(const char *source, asn1typed_name_style_e style, const char **error) {
	char *name = asn1typed_render_cpp_final_name(source, style);
	if(!name) { *error = "unsafe final C++ spelling"; return NULL; }
	if(asn1typed_render_cpp_header_macro(name)) {
		free(name);
		*error = "final C++ spelling conflicts with a standard header macro";
		return NULL;
	}
	return name;
}

static int
empty_size(const asn1typed_size_constraint_t *c) {
	return !c->has_size_constraint && !c->lower_bound && !c->upper_bound && !c->is_extensible;
}
static int
empty_range(const asn1typed_integer_value_range_t *r) {
	return !r->has_value_range && !r->lower_bound && !r->upper_bound &&
		!r->is_extensible && !r->tail && !r->tail_count;
}

struct codec_plan {
	char **types;
	char **helpers;
	char **getters;
	char **apis;
	char ***fields;
	char ***wrappers;
};

static void
clear_plan(const asn1typed_module_t *m, struct codec_plan *p) {
	size_t i, j;
	if(p->types) for(i = 0; i < m->type_count; ++i) free(p->types[i]);
	if(p->helpers) for(i = 0; i < m->type_count; ++i) free(p->helpers[i]);
	if(p->getters) for(i = 0; i < m->type_count; ++i) free(p->getters[i]);
	if(p->apis) for(i = 0; i < m->type_count; ++i) free(p->apis[i]);
	if(p->fields) for(i = 0; i < m->type_count; ++i) {
		if(p->fields[i]) for(j = 0; j < m->types[i].field_count; ++j) free(p->fields[i][j]);
		free(p->fields[i]);
	}
	if(p->wrappers) for(i = 0; i < m->type_count; ++i) {
		if(p->wrappers[i]) for(j = 0; j < m->types[i].alternative_count; ++j) free(p->wrappers[i][j]);
		free(p->wrappers[i]);
	}
	free(p->types); free(p->helpers); free(p->getters); free(p->apis); free(p->fields); free(p->wrappers);
	memset(p, 0, sizeof(*p));
}

static size_t
find_type(const asn1typed_module_t *m, const char *source) {
	size_t i;
	for(i = 0; i < m->type_count; ++i)
		if(m->types[i].identity.source_name && !strcmp(m->types[i].identity.source_name, source)) return i;
	return SIZE_MAX;
}

static int
resolve_ref(const asn1typed_module_t *m, const asn1typed_type_ref_t *r,
		const struct codec_plan *p, size_t *index, const char **cpp_type,
		const char **put, const char **get) {
	*index = SIZE_MAX;
	if(r->actual_count || r->actuals) return -1;
	if(r->kind == ASN1TYPED_REF_PRIMITIVE && r->primitive_kind == ASN1TYPED_PRIMITIVE_BOOLEAN) {
		*cpp_type = "bool"; *put = "put_boolean"; *get = "get_boolean"; return 0;
	}
	if(r->kind != ASN1TYPED_REF_NAMED || !r->module || strcmp(r->module, m->source_name) || !r->source_name) return -1;
	*index = find_type(m, r->source_name);
	if(*index == SIZE_MAX) return -1;
	*cpp_type = p->types[*index]; *put = p->helpers[*index];
	*get = p->getters[*index];
	return 0;
}

static int
prepare(const asn1typed_module_t *m, struct codec_plan *p, const char **error) {
	size_t i, j, k;
	p->types = calloc(m->type_count ? m->type_count : 1, sizeof(*p->types));
	p->helpers = calloc(m->type_count ? m->type_count : 1, sizeof(*p->helpers));
	p->getters = calloc(m->type_count ? m->type_count : 1, sizeof(*p->getters));
	p->apis = calloc(m->type_count ? m->type_count : 1, sizeof(*p->apis));
	p->fields = calloc(m->type_count ? m->type_count : 1, sizeof(*p->fields));
	p->wrappers = calloc(m->type_count ? m->type_count : 1, sizeof(*p->wrappers));
	if(!p->types || !p->helpers || !p->getters || !p->apis || !p->fields || !p->wrappers) { *error = "out of memory"; return -1; }
	for(i = 0; i < m->type_count; ++i) {
		const asn1typed_type_t *t = &m->types[i];
		if(!t->identity.source_name || !t->identity.module || strcmp(t->identity.module, m->source_name)) { *error = "invalid type identity"; return -1; }
		p->types[i] = final_name(t->identity.source_name, ASN1TYPED_NAME_TYPE, error);
		if(!p->types[i]) return -1;
		p->helpers[i] = malloc(strlen(p->types[i]) + 5);
		p->getters[i] = malloc(strlen(p->types[i]) + 5);
		if(!p->helpers[i] || !p->getters[i]) { *error = "out of memory"; return -1; }
		(void)sprintf(p->helpers[i], "put_%s", p->types[i]);
		(void)sprintf(p->getters[i], "get_%s", p->types[i]);
		{
			char *base = final_name(p->types[i], ASN1TYPED_NAME_FIELD, error);
			size_t n;
			if(!base) return -1;
			n = strlen(base) + sizeof("encode_");
			p->apis[i] = malloc(n);
			if(!p->apis[i]) { free(base); *error = "out of memory"; return -1; }
			(void)snprintf(p->apis[i], n, "encode_%s", base);
			free(base);
		}
		for(j = 0; j < i; ++j) if(!strcmp(p->types[i], p->types[j])) { *error = "type spelling collision"; return -1; }
		if(t->kind == ASN1TYPED_TYPE_SEQUENCE) {
			p->fields[i] = calloc(t->field_count ? t->field_count : 1, sizeof(**p->fields));
			if(!p->fields[i]) { *error = "out of memory"; return -1; }
			for(j = 0; j < t->field_count; ++j) {
				p->fields[i][j] = final_name(t->fields[j].source_name, ASN1TYPED_NAME_FIELD, error);
				if(!p->fields[i][j]) return -1;
				for(k = 0; k < j; ++k) if(!strcmp(p->fields[i][j], p->fields[i][k])) { *error = "SEQUENCE field spelling collision"; return -1; }
			}
		}
		if(t->kind == ASN1TYPED_TYPE_CHOICE) {
			p->wrappers[i] = calloc(t->alternative_count ? t->alternative_count : 1, sizeof(**p->wrappers));
			if(!p->wrappers[i]) { *error = "out of memory"; return -1; }
			for(j = 0; j < t->alternative_count; ++j) {
				char *alt = final_name(t->alternatives[j].source_name, ASN1TYPED_NAME_FIELD, error);
				size_t n;
				if(!alt) return -1;
				n = strlen(p->types[i]) + strlen(alt) + 2;
				p->wrappers[i][j] = malloc(n);
				if(!p->wrappers[i][j]) { free(alt); *error = "out of memory"; return -1; }
				(void)snprintf(p->wrappers[i][j], n, "%s_%s", p->types[i], alt);
				free(alt);
				for(k = 0; k < j; ++k) if(!strcmp(p->wrappers[i][j], p->wrappers[i][k])) { *error = "CHOICE wrapper spelling collision"; return -1; }
			}
		}
	}
	/* Register all codec symbols against public value/mapping/wrapper names. */
	for(i = 0; i < m->type_count; ++i) {
		for(j = 0; j < m->type_count; ++j) {
			char mapping[512];
			(void)snprintf(mapping, sizeof(mapping), "%s_aper", p->types[j]);
			if(!strcmp(p->helpers[i], p->types[j]) || !strcmp(p->helpers[i], mapping)) { *error = "codec helper collides with generated symbol"; return -1; }
			for(k = 0; k < m->types[j].alternative_count; ++k)
				if(p->wrappers[j] && !strcmp(p->helpers[i], p->wrappers[j][k])) { *error = "codec helper collides with CHOICE wrapper"; return -1; }
			if(!strcmp(p->getters[i], p->types[j]) || !strcmp(p->getters[i], mapping) ||
				!strcmp(p->apis[i], p->types[j]) || !strcmp(p->apis[i], mapping) ||
				!strcmp(p->helpers[i], p->apis[j]) || !strcmp(p->getters[i], p->apis[j]) ||
				(i != j && !strcmp(p->apis[i], p->apis[j]))) {
				*error = "codec symbol collides with generated type, mapping, or API name"; return -1;
			}
			for(k = 0; k < m->types[j].alternative_count; ++k) if(p->wrappers[j] &&
				(!strcmp(p->getters[i], p->wrappers[j][k]) || !strcmp(p->apis[i], p->wrappers[j][k]))) {
				*error = "codec symbol collides with CHOICE wrapper"; return -1;
			}
		}
		for(j = 0; j < i; ++j) if(!strcmp(p->helpers[i], p->helpers[j]) ||
			!strcmp(p->getters[i], p->getters[j]) || !strcmp(p->apis[i], p->apis[j]) ||
			!strcmp(p->helpers[i], p->getters[j]) || !strcmp(p->getters[i], p->helpers[j])) {
			*error = "codec helper or API name collision"; return -1;
		}
		if(!strcmp(p->helpers[i], "put_boolean") || !strcmp(p->getters[i], "get_boolean") ||
			!strcmp(p->apis[i], "put_boolean") || !strcmp(p->apis[i], "get_boolean")) {
			*error = "codec symbol collides with BOOLEAN helper"; return -1;
		}
	}
	return 0;
}

static int
emit_put_boolean(struct codec_buf *b) {
	return append(b, "inline ::nrforge::aper::Result<void> put_boolean(::nrforge::aper::FieldWriter& f, bool v) { return f.write_bit(v); }\ninline ::nrforge::aper::Result<bool> get_boolean(::nrforge::aper::FieldReader& f) { return f.read_bit(); }\n");
}

static int
emit_primitive(struct codec_buf *b, const asn1typed_type_t *t, const struct codec_plan *p, size_t i) {
	if(t->primitive_kind == ASN1TYPED_PRIMITIVE_BOOLEAN) {
		if(!empty_size(&t->size_constraint) || !empty_range(&t->value_range)) return -2;
		return format(b, "inline ::nrforge::aper::Result<void> %s(::nrforge::aper::FieldWriter& f, %s v) { return put_boolean(f, v); }\ninline ::nrforge::aper::Result<%s> %s(::nrforge::aper::FieldReader& f) { return get_boolean(f); }\n", p->helpers[i], p->types[i], p->types[i], p->getters[i]);
	}
	if(t->primitive_kind == ASN1TYPED_PRIMITIVE_INTEGER) {
		const asn1typed_integer_value_range_t *r = &t->value_range;
		if(!r->has_value_range || r->lower_bound != 0 || r->upper_bound != 65535 || r->is_extensible || r->tail || r->tail_count || !empty_size(&t->size_constraint)) return -2;
		return format(b, "inline ::nrforge::aper::Result<void> %s(::nrforge::aper::FieldWriter& f, %s v) { return f.write_aligned_u16_be(v); }\ninline ::nrforge::aper::Result<%s> %s(::nrforge::aper::FieldReader& f) { auto v = f.read_aligned_u16_be(); if(!v) return ::nrforge::aper::Result<%s>::failure(v.error()); return ::nrforge::aper::Result<%s>::success(v.value()); }\n", p->helpers[i], p->types[i], p->types[i], p->getters[i], p->types[i], p->types[i]);
	}
	return -2;
}

static int
emit_choice(struct codec_buf *b, const asn1typed_module_t *m, const struct codec_plan *p, size_t i) {
	const asn1typed_type_t *t = &m->types[i];
	size_t j;
	if(t->alternative_count != 2 || !t->alternatives || !empty_size(&t->size_constraint) || t->is_extensible) return -2;
	if(format(b, "inline ::nrforge::aper::Result<void> %s(::nrforge::aper::FieldWriter& f, const %s& v) { if(v.valueless_by_exception()) return ::nrforge::aper::Result<void>::failure({::nrforge::aper::ErrorCode::invalid_argument, f.cursor_bit()}); const auto ordinal = v.index(); if(ordinal >= 2) return ::nrforge::aper::Result<void>::failure({::nrforge::aper::ErrorCode::invalid_argument, f.cursor_bit()}); const auto index = %s_aper::storage_alternative_to_per_root_index[ordinal]; auto r = f.write_bit(index != 0); if(!r) return r; ", p->helpers[i], p->types[i], p->types[i])) return -1;
	for(j = 0; j < 2; ++j) {
		const asn1typed_choice_alternative_t *a = &t->alternatives[j];
		size_t ri; const char *type, *put, *get;
		if(!empty_size(&a->size_constraint) || !empty_range(&a->value_range) || resolve_ref(m, &a->type_ref, p, &ri, &type, &put, &get)) return -2;
		if(a->type_ref.kind == ASN1TYPED_REF_NAMED && ri >= i) return -2;
		if(format(b, "if(ordinal == %zu) return %s(f, std::get<%s>(v).value); ", j, put, p->wrappers[i][j])) return -1;
	}
	if(append(b, "return ::nrforge::aper::Result<void>::failure({::nrforge::aper::ErrorCode::invalid_argument, f.cursor_bit()}); }\n")) return -1;
	if(format(b, "inline ::nrforge::aper::Result<%s> %s(::nrforge::aper::FieldReader& f) { auto i = f.read_bit(); if(!i) return ::nrforge::aper::Result<%s>::failure(i.error()); const auto ordinal = %s_aper::per_root_index_to_storage_ordinal[i.value() ? 1 : 0]; ", p->types[i], p->getters[i], p->types[i], p->types[i])) return -1;
	for(j = 0; j < 2; ++j) {
		const asn1typed_choice_alternative_t *a = &t->alternatives[j];
		size_t ri; const char *type, *put, *get;
		if(resolve_ref(m, &a->type_ref, p, &ri, &type, &put, &get)) return -2;
		if(format(b, "if(ordinal == %zu) { auto v = %s(f); if(!v) return ::nrforge::aper::Result<%s>::failure(v.error()); return ::nrforge::aper::Result<%s>::success(%s{%s{std::move(v).value()}}); } ", j, get, p->types[i], p->types[i], p->types[i], p->wrappers[i][j])) return -1;
	}
	return format(b, "return ::nrforge::aper::Result<%s>::failure({::nrforge::aper::ErrorCode::invalid_argument, f.cursor_bit()}); }\n", p->types[i]);
}

static int
emit_sequence(struct codec_buf *b, const asn1typed_module_t *m, const struct codec_plan *p, size_t i) {
	const asn1typed_type_t *t = &m->types[i];
	size_t j;
	if(!empty_size(&t->size_constraint) || (t->field_count && !t->fields) || t->is_extensible) return -2;
	if(format(b, "inline ::nrforge::aper::Result<void> %s(::nrforge::aper::FieldWriter& f, const %s& v) { ", p->helpers[i], p->types[i])) return -1;
	if(t->field_count == 0 && append(b, "(void)f; (void)v; ")) return -1;
	for(j = 0; j < t->field_count; ++j) {
		const asn1typed_field_t *field = &t->fields[j];
		if(field->presence == ASN1TYPED_PRESENCE_OPTIONAL) {
			if(format(b, "auto p%zu = f.write_bit(v.%s.has_value()); if(!p%zu) return p%zu; ", j, p->fields[i][j], j, j)) return -1;
		} else if(field->presence != ASN1TYPED_PRESENCE_MANDATORY) return -2;
	}
	for(j = 0; j < t->field_count; ++j) {
		const asn1typed_field_t *field = &t->fields[j];
		size_t ri; const char *type, *put, *get;
		if(field->type_semantics != ASN1TYPED_FIELD_FIXED_TYPE || field->inline_enumerated || field->has_class_field_relation ||
			!empty_range(&field->value_range) || !empty_size(&field->size_constraint) || resolve_ref(m, &field->type, p, &ri, &type, &put, &get)) return -2;
		if(field->type.kind == ASN1TYPED_REF_NAMED && ri >= i) return -2;
		if(field->presence == ASN1TYPED_PRESENCE_OPTIONAL) {
			if(format(b, "if(v.%s) { auto r%zu = %s(f, *v.%s); if(!r%zu) return r%zu; } ", p->fields[i][j], j, put, p->fields[i][j], j, j)) return -1;
		} else if(format(b, "{ auto r%zu = %s(f, v.%s); if(!r%zu) return r%zu; } ", j, put, p->fields[i][j], j, j)) return -1;
	}
	if(append(b, "return ::nrforge::aper::Result<void>::success(); }\n")) return -1;
	if(format(b, "inline ::nrforge::aper::Result<%s> %s(::nrforge::aper::FieldReader& f) { %s v{}; ", p->types[i], p->getters[i], p->types[i])) return -1;
	if(t->field_count == 0 && append(b, "(void)f; ")) return -1;
	for(j = 0; j < t->field_count; ++j) if(t->fields[j].presence == ASN1TYPED_PRESENCE_OPTIONAL)
		if(format(b, "auto p%zu = f.read_bit(); if(!p%zu) return ::nrforge::aper::Result<%s>::failure(p%zu.error()); ", j, j, p->types[i], j)) return -1;
	for(j = 0; j < t->field_count; ++j) {
		const asn1typed_field_t *field = &t->fields[j];
		size_t ri; const char *type, *put, *get;
		if(resolve_ref(m, &field->type, p, &ri, &type, &put, &get)) return -2;
		if(field->presence == ASN1TYPED_PRESENCE_OPTIONAL) {
			if(format(b, "if(p%zu.value()) { auto x%zu = %s(f); if(!x%zu) return ::nrforge::aper::Result<%s>::failure(x%zu.error()); v.%s = std::move(x%zu).value(); } ", j, j, get, j, p->types[i], j, p->fields[i][j], j)) return -1;
		} else if(format(b, "{ auto x%zu = %s(f); if(!x%zu) return ::nrforge::aper::Result<%s>::failure(x%zu.error()); v.%s = std::move(x%zu).value(); } ", j, get, j, p->types[i], j, p->fields[i][j], j)) return -1;
	}
	return format(b, "return ::nrforge::aper::Result<%s>::success(std::move(v)); }\n", p->types[i]);
}

static int
emit_api(struct codec_buf *b, const struct codec_plan *p, size_t i) {
	int rc;
	rc = format(b, "inline ::nrforge::aper::Result<::nrforge::aper::CompleteEncoding> %s(const %s& v, const ::nrforge::aper::Limits& l = {}) { return ::nrforge::aper::encode_complete(v, l, [&](::nrforge::aper::FieldWriter& f) { return codec::%s(f, v); }); }\ninline ::nrforge::aper::Result<%s> decode_%s(std::span<const std::byte> s, const ::nrforge::aper::Limits& l = {}) { return ::nrforge::aper::decode_complete<%s>(s, l, [](::nrforge::aper::FieldReader& f) { return codec::%s(f); }); }\n", p->apis[i], p->types[i], p->helpers[i], p->types[i], p->apis[i] + 7, p->types[i], p->getters[i]);
	return rc;
}

int
asn1typed_render_cpp_owned_aper_codec(const asn1typed_module_t *m, const char *ns,
		char **out, char *diag, size_t dsz) {
	struct codec_buf b = {0}; struct codec_plan p = {0};
	char *mapping = NULL; char mapping_error[512]; const char *error = "invalid arguments";
	size_t i;
	int rc;
	if(out) *out = NULL;
	if(diag && dsz) diag[0] = '\0';
	if(!m || !ns || !out || !m->source_name || (m->type_count && !m->types)) goto fail;
	if(asn1typed_render_cpp_owned_aper_mapping(m, ns, &mapping, diag, dsz)) {
		if(diag && dsz && diag[0]) { (void)snprintf(mapping_error, sizeof(mapping_error), "%s", diag); error = mapping_error; }
		goto fail;
	}
	if(prepare(m, &p, &error)) goto fail;
	if(append(&b, "#include <cstddef>\n#include <cstdint>\n#include <span>\n#include <utility>\n#include <variant>\n#include <optional>\n\nnamespace ") || append(&b, ns) || append(&b, " {\nnamespace codec {\n")) goto oom;
	if(emit_put_boolean(&b)) goto oom;
	for(i = 0; i < m->type_count; ++i) {
		const asn1typed_type_t *t = &m->types[i];
		if(t->kind == ASN1TYPED_TYPE_PRIMITIVE) rc = emit_primitive(&b, t, &p, i);
		else if(t->kind == ASN1TYPED_TYPE_CHOICE) rc = emit_choice(&b, m, &p, i);
		else if(t->kind == ASN1TYPED_TYPE_SEQUENCE) rc = emit_sequence(&b, m, &p, i);
		else rc = -2;
		if(rc == -2) { error = "unsupported codec IR type or field semantics"; goto fail; }
		if(rc) goto oom;
	}
	if(append(&b, "} // namespace codec\n")) goto oom;
	for(i = 0; i < m->type_count; ++i) if(emit_api(&b, &p, i)) goto oom;
	if(append(&b, "} // namespace ") || append(&b, ns) || append(&b, "\n")) goto oom;
	free(mapping); clear_plan(m, &p); *out = b.data; return 0;
oom:
	error = "out of memory";
fail:
	free(mapping); free(b.data); clear_plan(m, &p);
	if(diag && dsz) (void)snprintf(diag, dsz, "%s", error);
	return -1;
}
