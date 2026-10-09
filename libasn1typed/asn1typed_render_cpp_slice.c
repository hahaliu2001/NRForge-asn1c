#include "asn1typed_render_cpp.h"
#include "asn1typed_name.h"

#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct outbuf { char *p; size_t n; };
static int put(struct outbuf *b, const char *s) {
	size_t n = strlen(s); char *p;
	if(n > SIZE_MAX - b->n - 1) return -1;
	p = realloc(b->p, b->n + n + 1); if(!p) return -1;
	memcpy(p + b->n, s, n + 1); b->p = p; b->n += n; return 0;
}
static int fmt(struct outbuf *b, const char *f, ...) {
	va_list ap; int n; char *s;
	va_start(ap, f); n = vsnprintf(NULL, 0, f, ap); va_end(ap);
	if(n < 0 || !(s = malloc((size_t)n + 1))) return -1;
	va_start(ap, f); vsnprintf(s, (size_t)n + 1, f, ap); va_end(ap);
	if(put(b, s)) { free(s); return -1; } free(s); return 0;
}
static int cpp_keyword(const char *s) {
	static const char *const kw[] = {"alignas","alignof","and","and_eq","asm","auto","bitand","bitor","bool","break","case","catch","char","class","compl","concept","const","consteval","constexpr","constinit","const_cast","continue","co_await","co_return","co_yield","decltype","default","delete","do","double","dynamic_cast","else","enum","explicit","export","extern","false","float","for","friend","goto","if","inline","int","long","mutable","namespace","new","noexcept","not","not_eq","nullptr","operator","or","or_eq","private","protected","public","register","reinterpret_cast","requires","return","short","signed","sizeof","static","static_assert","static_cast","struct","switch","template","this","thread_local","throw","true","try","typedef","typeid","typename","union","unsigned","using","virtual","void","volatile","wchar_t","while","xor","xor_eq"};
	size_t i; for(i = 0; i < sizeof(kw)/sizeof(kw[0]); ++i) if(!strcmp(s, kw[i])) return 1;
	return 0;
}
static int cpp_header_macro(const char *s) {
	static const char *const names[] = {
		"stdin", "stdout", "stderr", "BUFSIZ", "EOF", "NULL", "WEOF", "FILE",
		"errno", "E2BIG", "EACCES", "EADDRINUSE", "EADDRNOTAVAIL", "EAFNOSUPPORT",
		"EAGAIN", "EALREADY", "EBADF", "EBADMSG", "EBUSY", "ECANCELED", "ECHILD",
		"ECONNABORTED", "ECONNREFUSED", "ECONNRESET", "EDEADLK", "EDESTADDRREQ", "EDOM",
		"EEXIST", "EFAULT", "EFBIG", "EHOSTUNREACH", "EIDRM", "EILSEQ", "EINPROGRESS",
		"EINTR", "EINVAL", "EIO", "EISCONN", "EISDIR", "ELOOP", "EMFILE", "EMLINK",
		"EMSGSIZE", "ENAMETOOLONG", "ENETDOWN", "ENETRESET", "ENETUNREACH", "ENFILE",
		"ENOBUFS", "ENODATA", "ENODEV", "ENOENT", "ENOEXEC", "ENOLCK", "ENOLINK",
		"ENOMEM", "ENOMSG", "ENOPROTOOPT", "ENOSPC", "ENOSR", "ENOSTR", "ENOSYS",
		"ENOTCONN", "ENOTDIR", "ENOTEMPTY", "ENOTRECOVERABLE", "ENOTSOCK", "ENOTSUP",
		"ENOTTY", "ENXIO", "EOPNOTSUPP", "EOVERFLOW", "EOWNERDEAD", "EPERM", "EPIPE",
		"EPROTO", "EPROTONOSUPPORT", "EPROTOTYPE", "ERANGE", "EROFS", "ESPIPE", "ESRCH",
		"ETIME", "ETIMEDOUT", "ETXTBSY", "EWOULDBLOCK", "EXDEV",
		/* Object-like macros from the standard <cstdint>/<stdint.h> synopsis.
		 * Function-like INT*_C/UINT*_C macros do not expand in identifiers. */
		"INT8_MIN", "INT8_MAX", "UINT8_MAX", "INT16_MIN", "INT16_MAX", "UINT16_MAX",
		"INT32_MIN", "INT32_MAX", "UINT32_MAX", "INT64_MIN", "INT64_MAX", "UINT64_MAX",
		"INT_LEAST8_MIN", "INT_LEAST8_MAX", "UINT_LEAST8_MAX",
		"INT_LEAST16_MIN", "INT_LEAST16_MAX", "UINT_LEAST16_MAX",
		"INT_LEAST32_MIN", "INT_LEAST32_MAX", "UINT_LEAST32_MAX",
		"INT_LEAST64_MIN", "INT_LEAST64_MAX", "UINT_LEAST64_MAX",
		"INT_FAST8_MIN", "INT_FAST8_MAX", "UINT_FAST8_MAX",
		"INT_FAST16_MIN", "INT_FAST16_MAX", "UINT_FAST16_MAX",
		"INT_FAST32_MIN", "INT_FAST32_MAX", "UINT_FAST32_MAX",
		"INT_FAST64_MIN", "INT_FAST64_MAX", "UINT_FAST64_MAX",
		"INTPTR_MIN", "INTPTR_MAX", "UINTPTR_MAX", "INTMAX_MIN", "INTMAX_MAX", "UINTMAX_MAX",
		"PTRDIFF_MIN", "PTRDIFF_MAX", "SIG_ATOMIC_MIN", "SIG_ATOMIC_MAX", "SIZE_MAX",
		"WCHAR_MIN", "WCHAR_MAX", "WINT_MIN", "WINT_MAX"
	};
	size_t i;
	for(i = 0; i < sizeof(names)/sizeof(names[0]); ++i) if(!strcmp(s, names[i])) return 1;
	return 0;
}
static char *name(const char *s, asn1typed_name_style_e style) {
	char *p = NULL;
	if(asn1typed_name_make(s, style, &p) != ASN1TYPED_NAME_OK) return NULL;
	size_t n = strlen(p);
	if(cpp_keyword(p)) {
		char *q = realloc(p, n + 2); if(!q) { free(p); return NULL; }
		p = q; p[n] = '_'; p[n + 1] = 0;
	}
	if(p[0] == '_' || strstr(p, "__")) { free(p); return NULL; }
	if(!strcmp(p, "EOF") || !strcmp(p, "NULL") || !strcmp(p, "FILE") ||
		!strcmp(p, "stdin") || !strcmp(p, "stdout") || !strcmp(p, "stderr")) {
		char *q = malloc(strlen(p) + 5); if(!q) { free(p); return NULL; }
		sprintf(q, "cpp_%s", p); free(p); p = q;
	}
	return p;
}
static int safe_namespace(const char *ns) {
	const char *p = ns, *start;
	if(!ns || !*ns) return 0;
	while(*p) {
		size_t n, i; char token[128];
		start = p;
		if(!((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z'))) return 0;
		while((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z') ||
			(*p >= '0' && *p <= '9') || *p == '_') ++p;
		n = (size_t)(p - start);
		/* Namespace segment support is intentionally limited to 127 bytes. */
		if(n >= sizeof(token) || start[0] == '_' || (n > 1 && start[0] == '_' && start[1] == '_')) return 0;
		memcpy(token, start, n); token[n] = 0;
		if(cpp_keyword(token) || cpp_header_macro(token) || !strcmp(token, "std")) return 0;
		for(i = 0; i < n; ++i) if(token[i] == '_' && i + 1 < n && token[i + 1] == '_') return 0;
		if(!*p) break;
		if(p[0] != ':' || p[1] != ':') return 0;
		p += 2; if(!*p) return 0;
	}
	return 1;
}
static const asn1typed_type_t *find_type(const asn1typed_module_t *m,
		const char *s) {
	size_t i; for(i = 0; i < m->type_count; ++i)
		if(!strcmp(m->types[i].identity.source_name, s)) return &m->types[i];
	return NULL;
}
static const char *ref_cpp(const asn1typed_module_t *m,
		const asn1typed_type_ref_t *r, char **names, size_t *type_index,
		const char **why) {
	const asn1typed_type_t *t; size_t i;
	if(why) *why = "invalid or unresolved type reference";
	if(type_index) *type_index = SIZE_MAX;
	if(r->actual_count != 0 || r->actuals != NULL) {
		if(why) *why = "parameterized references unsupported by owned slice";
		return NULL;
	}
	if(r->kind == ASN1TYPED_REF_PRIMITIVE) {
		if(r->primitive_kind == ASN1TYPED_PRIMITIVE_BOOLEAN) {
			if(why) *why = NULL;
			return "bool";
		}
		if(why) *why = "unsupported primitive reference kind";
		return NULL;
	}
	if(r->kind != ASN1TYPED_REF_NAMED || !r->module ||
			strcmp(r->module, m->source_name) || !r->source_name) return NULL;
	t = find_type(m, r->source_name);
	if(!t) return NULL;
	for(i = 0; i < m->type_count; ++i) if(t == &m->types[i]) break;
	if(i == m->type_count) return NULL;
	if(type_index) *type_index = i;
	if(why) *why = NULL;
	return names[i];
}

/* Keep generated helper spellings separate from declaration spellings and
 * retain each final C++ spelling once for cross-category collision checks. */
static int register_generated(char ***symbols, size_t *count,
		const char *candidate, char **types, size_t type_count) {
	size_t i; char **p; char *copy;
	for(i = 0; i < type_count; ++i) if(!strcmp(candidate, types[i])) return -1;
	for(i = 0; i < *count; ++i) if(!strcmp(candidate, (*symbols)[i])) return -1;
	copy = malloc(strlen(candidate) + 1); if(!copy) return -1;
	strcpy(copy, candidate);
	p = realloc(*symbols, (*count + 1) * sizeof(**symbols));
	if(!p) { free(copy); return -1; }
	*symbols = p; p[(*count)++] = copy; return 0;
}
static int ref_is_empty(const asn1typed_type_ref_t *r) {
	return r->kind == ASN1TYPED_REF_NAMED && !r->module && !r->source_name &&
		r->primitive_kind == ASN1TYPED_PRIMITIVE_INVALID && !r->actuals && !r->actual_count;
}
static int storage_consistent(const void *ptr, size_t count, size_t capacity) {
	return count <= capacity && ((capacity == 0) == (ptr == NULL));
}
static int storage_empty(const void *ptr, size_t count, size_t capacity) {
	return ptr == NULL && count == 0 && capacity == 0;
}
static int type_storage_valid(const asn1typed_type_t *t) {
	int primitive = t->kind == ASN1TYPED_TYPE_PRIMITIVE;
	int sequence = t->kind == ASN1TYPED_TYPE_SEQUENCE;
	int choice = t->kind == ASN1TYPED_TYPE_CHOICE;
	int enumerated = t->kind == ASN1TYPED_TYPE_ENUMERATED;
	int sequence_of = t->kind == ASN1TYPED_TYPE_SEQUENCE_OF;
	if(t->kind < ASN1TYPED_TYPE_PRIMITIVE || t->kind > ASN1TYPED_TYPE_CHOICE ||
		!storage_consistent(t->fields, t->field_count, t->field_capacity) ||
		!storage_consistent(t->alternatives, t->alternative_count, t->alternative_capacity) ||
		!storage_consistent(t->enum_items, t->enum_item_count, t->enum_item_capacity)) return 0;
	if((!sequence && !storage_empty(t->fields, t->field_count, t->field_capacity)) ||
		(!choice && !storage_empty(t->alternatives, t->alternative_count, t->alternative_capacity)) ||
		(!enumerated && !storage_empty(t->enum_items, t->enum_item_count, t->enum_item_capacity))) return 0;
	if((primitive && (t->primitive_kind <= ASN1TYPED_PRIMITIVE_INVALID || t->primitive_kind > ASN1TYPED_PRIMITIVE_BIT_STRING)) ||
		(!primitive && t->primitive_kind != ASN1TYPED_PRIMITIVE_INVALID)) return 0;
	if(sequence_of ? ref_is_empty(&t->element_type) : !ref_is_empty(&t->element_type)) return 0;
	return 1;
}
static int ioc_metadata_is_empty(const asn1typed_ioc_metadata_t *ioc) {
	return !ioc->symbolic_id && !ioc->has_numeric_id && ioc->numeric_id == 0 &&
		ioc->criticality == ASN1TYPED_CRITICALITY_REJECT;
}
static int size_constraint_is_empty(const asn1typed_size_constraint_t *c) {
	return !c->has_size_constraint && c->lower_bound == 0 && c->upper_bound == 0 && !c->is_extensible;
}
static int value_range_is_empty(const asn1typed_integer_value_range_t *r) {
	return !r->has_value_range && r->lower_bound == 0 && r->upper_bound == 0 &&
		!r->is_extensible && !r->tail && !r->tail_count;
}
static int fmt_i64(struct outbuf *b, intmax_t v) {
	if(v == INT64_MIN) return put(b, "(-INT64_C(9223372036854775807) - INT64_C(1))");
	if(v < 0) return fmt(b, "(-INT64_C(%" PRIuMAX "))", (uintmax_t)(-v));
	return fmt(b, "INT64_C(%" PRIuMAX ")", (uintmax_t)v);
}

int asn1typed_render_cpp_owned_slice(const asn1typed_module_t *m,
		const char *ns, char **out, char *diag, size_t dsz) {
	struct outbuf b = {0}; size_t i,j,k; const char *err = "invalid arguments";
	char **names = NULL, **generated = NULL, ***alts = NULL, ***wrappers = NULL;
	size_t generated_count = 0;
	if(out) *out = NULL;
	if(diag && dsz) diag[0] = 0;
	if(!m || !ns || !out || !m->source_name || (m->type_count && !m->types)) goto fail;
	if(!safe_namespace(ns)) { err = "invalid or reserved output namespace"; goto fail; }
	if(m->bound_instance_count || m->bound_instances) { err = "bound instances unsupported by owned slice"; goto fail; }
	if(put(&b, "#include <cstdint>\n#include <optional>\n#include <variant>\n\nnamespace ") ||
		put(&b, ns) || put(&b, " {\n\n")) { err = "out of memory"; goto fail; }
	names = calloc(m->type_count ? m->type_count : 1, sizeof(*names));
	if(!names) { err = "out of memory"; goto fail; }
	for(i = 0; i < m->type_count; ++i) {
		const asn1typed_type_t *t = &m->types[i];
		if(!type_storage_valid(t)) { err = "type kind has inconsistent or unrelated IR storage"; goto fail; }
		names[i] = name(t->identity.source_name, ASN1TYPED_NAME_TYPE);
		if(!names[i] || !t->identity.module || strcmp(t->identity.module, m->source_name)) {
			err = "invalid type identity or unsafe name"; goto fail;
		}
		for(j = 0; j < i; ++j) if(!strcmp(names[j], names[i])) {
			err = "type name collision after normalization"; goto fail;
		}
	}
	alts = calloc(m->type_count ? m->type_count : 1, sizeof(*alts));
	wrappers = calloc(m->type_count ? m->type_count : 1, sizeof(*wrappers));
	if(!alts || !wrappers) goto oom;
	for(i = 0; i < m->type_count; ++i) {
		const asn1typed_type_t *t = &m->types[i];
		if(t->size_constraint.is_extensible && !t->size_constraint.has_size_constraint) { err = "SIZE extensibility marker without SIZE constraint"; goto fail; }
		if(t->value_range.is_extensible && !t->value_range.has_value_range) { err = "INTEGER extensibility marker without value range"; goto fail; }
		if(t->kind != ASN1TYPED_TYPE_PRIMITIVE && !value_range_is_empty(&t->value_range)) { err = "unexpected INTEGER range metadata"; goto fail; }
		if(!t->has_ioc_table && !ref_is_empty(&t->ioc_container)) { err = "unexpected IOC container metadata"; goto fail; }
		if(t->kind == ASN1TYPED_TYPE_PRIMITIVE && t->primitive_kind == ASN1TYPED_PRIMITIVE_INTEGER) {
			char *s = NULL; size_t n = strlen(names[i]) + sizeof("_constraint");
			s = malloc(n); if(!s) goto oom; snprintf(s, n, "%s_constraint", names[i]);
			if(register_generated(&generated, &generated_count, s, names, m->type_count)) { free(s); err = "generated constraint name collision or allocation failure"; goto fail; }
			free(s);
		}
		if(t->kind == ASN1TYPED_TYPE_CHOICE) {
			if(!t->alternative_count || !t->alternatives) { err = "empty or malformed CHOICE"; goto fail; }
			for(j = 0; j < t->alternative_count; ++j) {
			char *alt = name(t->alternatives[j].source_name, ASN1TYPED_NAME_FIELD), *s;
			size_t n;
			if(!alt) { err = "unsafe CHOICE alternative name"; goto fail; }
			if(!alts[i]) { alts[i] = calloc(t->alternative_count, sizeof(**alts)); wrappers[i] = calloc(t->alternative_count, sizeof(**wrappers)); if(!alts[i] || !wrappers[i]) { free(alt); goto oom; } }
			alts[i][j] = alt;
			n = strlen(names[i]) + strlen(alt) + 2; s = malloc(n);
			if(!s) goto oom;
			snprintf(s, n, "%s_%s", names[i], alt); wrappers[i][j] = s;
			if(register_generated(&generated, &generated_count, s, names, m->type_count)) { err = "generated CHOICE wrapper name collision or allocation failure"; goto fail; }
			}
		}
	}
	for(i = 0; i < m->type_count; ++i) {
		const asn1typed_type_t *t = &m->types[i];
		if(t->is_extensible || t->has_ioc_table || t->ioc_object_set_is_extensible) {
			err = "extension or IOC semantics unsupported by owned slice"; goto fail;
		}
		if(t->kind == ASN1TYPED_TYPE_PRIMITIVE) {
			if(t->primitive_kind == ASN1TYPED_PRIMITIVE_BOOLEAN) {
				if(!value_range_is_empty(&t->value_range) || !size_constraint_is_empty(&t->size_constraint)) { err = "constraint on BOOLEAN unsupported"; goto fail; }
				if(fmt(&b, "using %s = bool;\n\n", names[i])) goto oom;
			} else if(t->primitive_kind == ASN1TYPED_PRIMITIVE_INTEGER) {
				const asn1typed_integer_value_range_t *r = &t->value_range;
				if(!r->has_value_range || r->is_extensible || r->tail || r->tail_count || r->lower_bound > r->upper_bound || !size_constraint_is_empty(&t->size_constraint)) { err = "INTEGER requires one complete non-extensible interval"; goto fail; }
				if(r->lower_bound < 0) {
					if(r->lower_bound < INT64_MIN || r->upper_bound > INT64_MAX) { err = "INTEGER domain exceeds int64_t"; goto fail; }
					if(fmt(&b, "using %s = std::int64_t;\nstruct %s_constraint {\n    static constexpr std::int64_t lower_bound = ", names[i], names[i]) || fmt_i64(&b, r->lower_bound) || put(&b, ";\n    static constexpr std::int64_t upper_bound = ") || fmt_i64(&b, r->upper_bound) || put(&b, ";\n};\n\n")) goto oom;
				} else {
					if((uintmax_t)r->upper_bound > UINT64_MAX) { err = "INTEGER domain exceeds uint64_t"; goto fail; }
					if(fmt(&b, "using %s = std::uint64_t;\nstruct %s_constraint {\n    static constexpr std::uint64_t lower_bound = UINT64_C(%" PRIuMAX ");\n    static constexpr std::uint64_t upper_bound = UINT64_C(%" PRIuMAX ");\n};\n\n", names[i], names[i], (uintmax_t)r->lower_bound, (uintmax_t)r->upper_bound)) goto oom;
				}
			} else { err = "primitive type unsupported by owned slice"; goto fail; }
		} else if(t->kind == ASN1TYPED_TYPE_CHOICE) {
			if(!size_constraint_is_empty(&t->size_constraint)) { err = "constraint on CHOICE unsupported"; goto fail; }
			if(!t->alternative_count || !t->alternatives) { err = "empty or malformed CHOICE"; goto fail; }
			for(j = 0; j < t->alternative_count; ++j) {
				const char *alt = alts[i][j], *sp, *ref_error = NULL;
				size_t ri;
				sp = ref_cpp(m, &t->alternatives[j].type_ref, names, &ri, &ref_error);
				if(!sp) { err = ref_error ? ref_error : "unsupported CHOICE alternative reference"; goto fail; }
				if(!size_constraint_is_empty(&t->alternatives[j].size_constraint) || !value_range_is_empty(&t->alternatives[j].value_range)) { err = "CHOICE alternative constraints unsupported"; goto fail; }
				if(!alt) { err = "unsafe CHOICE alternative name"; goto fail; }
				if(t->alternatives[j].type_ref.kind == ASN1TYPED_REF_NAMED && ri >= i) { err = "forward CHOICE alternative reference unsupported"; goto fail; }
				for(k = 0; k < j; ++k) {
					if(!strcmp(alts[i][k], alt)) { err = "CHOICE alternative name collision"; goto fail; }
				}
				if(fmt(&b, "struct %s { %s value{}; };\n", wrappers[i][j], sp)) goto oom;
			}
			if(fmt(&b, "using %s = std::variant<", names[i])) goto oom;
			for(j = 0; j < t->alternative_count; ++j) {
				if(j && put(&b, ", ")) goto oom;
				if(put(&b, wrappers[i][j])) goto oom;
			}
			if(put(&b, ">;\n\n")) goto oom;
		} else if(t->kind == ASN1TYPED_TYPE_SEQUENCE) {
			if(!size_constraint_is_empty(&t->size_constraint)) { err = "constraint on SEQUENCE unsupported"; goto fail; }
			if(t->field_count && !t->fields) { err = "malformed SEQUENCE fields"; goto fail; }
			if(fmt(&b, "struct %s {\n", names[i])) goto oom;
			for(j = 0; j < t->field_count; ++j) {
				const asn1typed_field_t *f = &t->fields[j]; char *field = name(f->source_name, ASN1TYPED_NAME_FIELD);
				size_t ri; const char *ref_error = NULL, *sp = ref_cpp(m, &f->type, names, &ri, &ref_error);
				if(f->type_semantics != ASN1TYPED_FIELD_FIXED_TYPE || f->inline_enumerated || f->has_class_field_relation || !ioc_metadata_is_empty(&f->ioc) || f->presence == ASN1TYPED_PRESENCE_CONDITIONAL || !field || !sp || !value_range_is_empty(&f->value_range) || !size_constraint_is_empty(&f->size_constraint) || (f->type.kind == ASN1TYPED_REF_NAMED && ri >= i)) {
					free(field); err = ref_error ? ref_error : "unsupported SEQUENCE field semantics"; goto fail;
				}
				for(k = 0; k < j; ++k) {
					char *prev = name(t->fields[k].source_name, ASN1TYPED_NAME_FIELD);
					if(!prev || !strcmp(prev, field)) { free(prev); free(field); err = "SEQUENCE field name collision"; goto fail; }
					free(prev);
				}
				if(f->presence == ASN1TYPED_PRESENCE_OPTIONAL) { if(fmt(&b, "    std::optional<%s> %s;\n", sp, field)) { free(field); goto oom; } }
				else if(f->presence == ASN1TYPED_PRESENCE_MANDATORY) { if(fmt(&b, "    %s %s{};\n", sp, field)) { free(field); goto oom; } }
				else { free(field); err = "invalid presence value"; goto fail; }
				free(field);
			}
			if(put(&b, "};\n\n")) goto oom;
		} else { err = "type kind unsupported by owned slice"; goto fail; }
	}
	/* Includes are emitted unconditionally by contract for stable output. */
	if(put(&b, "} // namespace ") || put(&b, ns) || put(&b, "\n")) goto oom;
	for(i = 0; i < m->type_count; ++i) free(names[i]);
	free(names);
	for(i = 0; i < m->type_count; ++i) {
		if(alts && alts[i]) for(j = 0; j < m->types[i].alternative_count; ++j) free(alts[i][j]);
		if(wrappers && wrappers[i]) for(j = 0; j < m->types[i].alternative_count; ++j) free(wrappers[i][j]);
		if(alts) free(alts[i]);
		if(wrappers) free(wrappers[i]);
	}
	free(alts); free(wrappers);
	for(i = 0; i < generated_count; ++i) free(generated[i]);
	free(generated);
	*out = b.p;
	return 0;
oom: err = "out of memory";
fail:
	if(diag && dsz) snprintf(diag, dsz, "%s", err);
	if(names) { for(i = 0; i < m->type_count; ++i) free(names[i]); free(names); }
	if(m && m->types) for(i = 0; i < m->type_count; ++i) {
		if(alts && alts[i]) for(j = 0; j < m->types[i].alternative_count; ++j) free(alts[i][j]);
		if(wrappers && wrappers[i]) for(j = 0; j < m->types[i].alternative_count; ++j) free(wrappers[i][j]);
		if(alts) free(alts[i]);
		if(wrappers) free(wrappers[i]);
	}
	free(alts); free(wrappers);
	if(generated) { for(i = 0; i < generated_count; ++i) free(generated[i]); free(generated); }
	free(b.p); return -1;
}
