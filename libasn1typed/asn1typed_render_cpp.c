#include "asn1typed_render_cpp.h"
#include "asn1typed_name.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct text_buffer {
	char *data;
	size_t size;
};

/* Small transactional buffer: nothing is published until the entire render
 * succeeds. Fixed fragments keep formatting independent of locale. */
static int
append(struct text_buffer *b, const char *s) {
	size_t n = strlen(s);
	char *p;
	if(n >= SIZE_MAX - b->size) return -1;
	p = realloc(b->data, b->size + n + 1);
	if(!p) return -1;
	memcpy(p + b->size, s, n + 1);
	b->data = p;
	b->size += n;
	return 0;
}

static int
keyword(const char *name) {
	/* C++20 keywords, including alternative operator tokens. Contextual
	 * identifiers (final, override, module, import) need no escaping here. */
	static const char *const words[] = {
		"alignas", "alignof", "and", "and_eq", "asm", "auto", "bitand",
		"bitor", "bool", "break", "case", "catch", "char", "char8_t",
		"char16_t", "char32_t", "class", "compl", "concept", "const",
		"consteval", "constexpr", "constinit", "const_cast", "continue",
		"co_await", "co_return", "co_yield", "decltype", "default", "delete",
		"do", "double", "dynamic_cast", "else", "enum", "explicit", "export",
		"extern", "false", "float", "for", "friend", "goto", "if", "inline",
		"int", "long", "mutable", "namespace", "new", "noexcept", "not",
		"not_eq", "nullptr", "operator", "or", "or_eq", "private",
		"protected", "public", "register", "reinterpret_cast", "requires",
		"return", "short", "signed", "sizeof", "static", "static_assert",
		"static_cast", "struct", "switch", "template", "this", "thread_local",
		"throw", "true", "try", "typedef", "typeid", "typename", "union",
		"unsigned", "using", "virtual", "void", "volatile", "wchar_t",
		"while", "xor", "xor_eq"
	};
	size_t i;
	for(i = 0; i < sizeof(words) / sizeof(words[0]); ++i)
		if(!strcmp(name, words[i])) return 1;
	return 0;
}

struct identifier {
	const char *source; /* borrowed, within a single module or member scope */
	char *name;
};

static void
clear_names(struct identifier *names, size_t count) {
	size_t i;
	if(!names) return;
	for(i = 0; i < count; ++i) free(names[i].name);
	free(names);
}

/* The only C++ safety pass, after T4 and before collision registration.
 * Reject all leading underscores (including global-scope reserved forms) and
 * double underscores anywhere. T4 currently cannot produce either form;
 * keep the guard here so that C++ safety does not depend on that accident.
 *
 * Development evidence: probes with g++ -std=c++20 and <cstdint>/<string>
 * found relevant standard-header names exposed by libstdc++'s transitive
 * includes. errno expands nontrivially; stream macros are self-referential
 * in that environment. Other implementations need not expose the same names
 * or use the same expansions.
 * T6 assessment: C++20 N4861 [optional.syn] and [vector.syn] declare
 * their interfaces within std (including std::pmr); their specified includes
 * <compare>/<initializer_list> add no relevant global names or macros.
 * A differential g++ -std=c++20 -dM -E probe adding <optional>/<vector>
 * to <cstdint>/<string> found no additional non-underscore macros.
 * Feature-test macros have reserved double underscores and cannot be emitted
 * by our identifier pipeline. No portable protection-list extension is needed.
 * https://timsong-cpp.github.io/cppwp/n4861/optional.syn
 * https://timsong-cpp.github.io/cppwp/n4861/vector.syn
 * Production policy: protect the explicit standard-header interface names
 * below, including the complete C++20 <cerrno> synopsis, unconditionally.
 * This bounds protection by standard interfaces relevant to the emitted
 * headers, not by a dump of the development machine's macros. Escape even
 * when no headers are needed so names never depend on other IR types.
 * TYPE can retain capitals from one-letter tokens (E-O-F -> EOF), but cannot
 * contain underscores. Include the reachable standard C object-like macros
 * and FILE (a global typedef), not implementation-specific extension names.
 * FIELD cannot contain capitals. Function-like macros do not expand in the
 * declaration/reference positions emitted here. This bounded policy covers
 * standard-header names. GNU/platform extensions, consumer/compiler macros
 * outside this set and extra includes may require a later isolation strategy.
 * Prefixing allows a collision with ordinary cpp-* source names; the caller
 * must check the FINAL spelling. No #undef is emitted.
 */
static const char *
cpp_identifier(char **slot) {
	static const char *const header_names[] = {
		"stdin", "stdout", "stderr", "BUFSIZ", "EOF", "NULL", "WEOF", "FILE",
		/* C++20 draft N4861 [cerrno.syn], all listed macros, including errno:
		 * https://timsong-cpp.github.io/cppwp/n4861/cerrno.syn
		 * Standard synopsis entries only; no GNU/platform errno additions. */
		"errno", "E2BIG", "EACCES", "EADDRINUSE", "EADDRNOTAVAIL",
		"EAFNOSUPPORT", "EAGAIN", "EALREADY", "EBADF", "EBADMSG", "EBUSY",
		"ECANCELED", "ECHILD", "ECONNABORTED", "ECONNREFUSED", "ECONNRESET",
		"EDEADLK", "EDESTADDRREQ", "EDOM", "EEXIST", "EFAULT", "EFBIG",
		"EHOSTUNREACH", "EIDRM", "EILSEQ", "EINPROGRESS", "EINTR", "EINVAL",
		"EIO", "EISCONN", "EISDIR", "ELOOP", "EMFILE", "EMLINK", "EMSGSIZE",
		"ENAMETOOLONG", "ENETDOWN", "ENETRESET", "ENETUNREACH", "ENFILE",
		"ENOBUFS", "ENODATA", "ENODEV", "ENOENT", "ENOEXEC", "ENOLCK",
		"ENOLINK", "ENOMEM", "ENOMSG", "ENOPROTOOPT", "ENOSPC", "ENOSR",
		"ENOSTR", "ENOSYS", "ENOTCONN", "ENOTDIR", "ENOTEMPTY",
		"ENOTRECOVERABLE", "ENOTSOCK", "ENOTSUP", "ENOTTY", "ENXIO",
		"EOPNOTSUPP", "EOVERFLOW", "EOWNERDEAD", "EPERM", "EPIPE", "EPROTO",
		"EPROTONOSUPPORT", "EPROTOTYPE", "ERANGE", "EROFS", "ESPIPE", "ESRCH",
		"ETIME", "ETIMEDOUT", "ETXTBSY", "EWOULDBLOCK", "EXDEV"
	};
	char *name = *slot, *escaped;
	size_t i, n = strlen(name);
	if(keyword(name)) {
		escaped = realloc(name, n + 2);
		if(!escaped) return "out of memory";
		*slot = name = escaped;
		name[n++] = '_';
		name[n] = '\0';
	}
	if(name[0] == '_' || strstr(name, "__"))
		return "implementation-reserved C++ identifier";
	for(i = 0; i < sizeof(header_names) / sizeof(header_names[0]); ++i) {
		if(strcmp(name, header_names[i])) continue;
		escaped = malloc(n + 5);
		if(!escaped) return "out of memory";
		memcpy(escaped, "cpp_", 4);
		memcpy(escaped + 4, name, n + 1);
		free(name);
		*slot = escaped;
		break;
	}
	return NULL;
}

/* Registration checks final spellings AND source keys. No generated name
 * ever goes back through T4; references reuse the declaration's spelling. */
static const char *
add_name(struct identifier *names, size_t index, const char *source,
		asn1typed_name_style_e style) {
	char *name = NULL;
	const char *error;
	size_t i;
	if(asn1typed_name_make(source, style, &name) != ASN1TYPED_NAME_OK)
		return "source identity naming failed";
	error = cpp_identifier(&name);
	if(error) { free(name); return error; }
	for(i = 0; i < index; ++i) {
		if(!strcmp(names[i].source, source)) {
			free(name);
			return "duplicate or inconsistent source identity";
		}
		if(!strcmp(names[i].name, name)) {
			free(name);
			return "C++ identifier collision after safety transformation";
		}
	}
	names[index].source = source;
	names[index].name = name;
	return NULL;
}

static const char *
primitive(asn1typed_primitive_kind_e kind, int *integer, int *string) {
	switch(kind) {
	case ASN1TYPED_PRIMITIVE_BOOLEAN: return "bool";
	case ASN1TYPED_PRIMITIVE_INTEGER: *integer = 1; return "std::int64_t";
	case ASN1TYPED_PRIMITIVE_UTF8_STRING:
	case ASN1TYPED_PRIMITIVE_PRINTABLE_STRING:
		*string = 1; return "std::string";
	default: return NULL;
	}
}

/* Resolve raw identities against the complete local table. Never flatten a
 * named alias or normalize a generated spelling again. */
static const char *
resolve_reference(const asn1typed_type_ref_t *ref, const char *module,
		const struct identifier *types, size_t count, int *integer,
		int *string, const char **spelling, size_t *dependency) {
	size_t k;
	*spelling = NULL;
	if(dependency) *dependency = SIZE_MAX;
	if(ref->kind == ASN1TYPED_REF_PRIMITIVE) {
		*spelling = primitive(ref->primitive_kind, integer, string);
		return *spelling ? NULL : "unsupported primitive kind";
	}
	if(ref->kind != ASN1TYPED_REF_NAMED)
		return "invalid reference kind";
	if(!ref->module || !ref->module[0] || !ref->source_name || !ref->source_name[0])
		return "invalid named reference identity";
	if(strcmp(ref->module, module)) return "external module reference unsupported";
	for(k = 0; k < count; ++k) {
		if(!strcmp(ref->source_name, types[k].source)) {
			*spelling = types[k].name;
			if(dependency) *dependency = k;
			return NULL;
		}
	}
	return "missing local declaration";
}

struct declaration_user {
	size_t index;
	struct declaration_user *next;
};

/* Private C++ declaration plan. Reverse edges use O(types + distinct edges)
 * storage; scanning from index zero selects the smallest CURRENTLY ready node.
 * SIZE_MAX marks selected nodes and the absence of a named dependency. */
static const char *
plan_declarations(const asn1typed_module_t *module,
		const struct identifier *types, size_t *order) {
	size_t n = module->type_count, i, j, step, dependency;
	size_t *remaining = NULL, *seen = NULL;
	struct declaration_user **users = NULL, *edge;
	const char *error = "out of memory", *spelling;
	int integer = 0, string = 0;
	if(!n) return NULL;
	remaining = calloc(n, sizeof(*remaining));
	seen = calloc(n, sizeof(*seen));
	users = calloc(n, sizeof(*users));
	if(!remaining || !seen || !users) goto done;
	for(i = 0; i < n; ++i) seen[i] = SIZE_MAX;
	for(i = 0; i < n; ++i) {
		const asn1typed_type_t *type = &module->types[i];
		size_t count = 0;
		switch(type->kind) {
		case ASN1TYPED_TYPE_PRIMITIVE:
		case ASN1TYPED_TYPE_ENUMERATED: break;
		case ASN1TYPED_TYPE_SEQUENCE:
			count = type->field_count;
			if(count && !type->fields) {
				error = "invalid member storage"; goto done;
			}
			break;
		case ASN1TYPED_TYPE_SEQUENCE_OF: count = 1; break;
		default: error = "unsupported type kind"; goto done;
		}
		for(j = 0; j < count; ++j) {
			const asn1typed_type_ref_t *ref = type->kind == ASN1TYPED_TYPE_SEQUENCE
				? &type->fields[j].type : &type->element_type;
			error = resolve_reference(ref, module->source_name, types, n,
				&integer, &string, &spelling, &dependency);
			if(error) goto done;
			if(dependency == SIZE_MAX || seen[dependency] == i) continue;
			seen[dependency] = i;
			edge = malloc(sizeof(*edge));
			if(!edge) { error = "out of memory"; goto done; }
			edge->index = i;
			edge->next = users[dependency];
			users[dependency] = edge;
			++remaining[i];
		}
	}
	for(step = 0; step < n; ++step) {
		for(i = 0; i < n && remaining[i] != 0; ++i) {}
		if(i == n) {
			error = "cyclic or cyclically blocked declaration dependencies";
			goto done;
		}
		order[step] = i;
		remaining[i] = SIZE_MAX;
		for(edge = users[i]; edge; edge = edge->next) --remaining[edge->index];
	}
	error = NULL;
done:
	if(users) for(i = 0; i < n; ++i) {
		while((edge = users[i]) != NULL) {
			users[i] = edge->next;
			free(edge);
		}
	}
	free(users); free(seen); free(remaining);
	return error;
}

int
asn1typed_render_cpp(const asn1typed_module_t *module, char **out,
		char *diagnostic, size_t diagnostic_size) {
	struct text_buffer body = {0}, result = {0};
	struct identifier *types = NULL, *members = NULL;
	size_t i, j, position, member_count = 0;
	size_t *order = NULL;
	int integer = 0, string = 0, optional = 0, vector = 0;
	const char *error = "invalid renderer argument", *spelling;
	char *module_name = NULL;
#define EMIT(buffer, value) do { if(append(&(buffer), (value))) { \
	error = "out of memory"; goto fail; } } while(0)
	if(out) *out = NULL;
	if(diagnostic && diagnostic_size) diagnostic[0] = '\0';
	if(!module || !out || (module->type_count && !module->types)) goto fail;
	if(asn1typed_name_make(module->source_name, ASN1TYPED_NAME_MODULE,
			&module_name) != ASN1TYPED_NAME_OK) {
		error = "module source identity naming failed"; goto fail;
	}
	free(module_name);
	if(module->type_count) {
		types = calloc(module->type_count, sizeof(*types));
		order = calloc(module->type_count, sizeof(*order));
		if(!types || !order) { error = "out of memory"; goto fail; }
	}
	for(i = 0; i < module->type_count; ++i) {
		const asn1typed_type_t *type = &module->types[i];
		if(!type->identity.module || strcmp(type->identity.module, module->source_name)) {
			error = "type identity does not belong to module"; goto fail;
		}
		error = add_name(types, i, type->identity.source_name, ASN1TYPED_NAME_TYPE);
		if(error) goto fail;
	}
	error = plan_declarations(module, types, order);
	if(error) goto fail;
	for(position = 0; position < module->type_count; ++position) {
		const asn1typed_type_t *type;
		i = order[position];
		type = &module->types[i];
		if(position) EMIT(body, "\n");
		if(type->kind == ASN1TYPED_TYPE_PRIMITIVE) {
			spelling = primitive(type->primitive_kind, &integer, &string);
			if(!spelling) { error = "unsupported primitive kind"; goto fail; }
			EMIT(body, "using "); EMIT(body, types[i].name);
			EMIT(body, " = "); EMIT(body, spelling); EMIT(body, ";\n");
			continue;
		}
		if(type->kind == ASN1TYPED_TYPE_SEQUENCE_OF) {
			error = resolve_reference(&type->element_type, module->source_name,
				types, module->type_count, &integer, &string, &spelling, NULL);
			if(error) goto fail;
			vector = 1;
			EMIT(body, "using "); EMIT(body, types[i].name);
			EMIT(body, " = std::vector<"); EMIT(body, spelling);
			EMIT(body, ">;\n");
			continue;
		}
		if(type->kind != ASN1TYPED_TYPE_SEQUENCE && type->kind != ASN1TYPED_TYPE_ENUMERATED) {
			error = "unsupported type kind"; goto fail;
		}
		member_count = type->kind == ASN1TYPED_TYPE_SEQUENCE ? type->field_count : type->enum_item_count;
		if(member_count && (type->kind == ASN1TYPED_TYPE_SEQUENCE ? !type->fields : !type->enum_items)) {
			error = "invalid member storage"; goto fail;
		}
		if(member_count) {
			members = calloc(member_count, sizeof(*members));
			if(!members) { error = "out of memory"; goto fail; }
		}
		EMIT(body, type->kind == ASN1TYPED_TYPE_SEQUENCE ? "struct " : "enum class ");
		EMIT(body, types[i].name); EMIT(body, " {\n");
		for(j = 0; j < member_count; ++j) {
			if(type->kind == ASN1TYPED_TYPE_ENUMERATED) {
				error = add_name(members, j, type->enum_items[j].source_name, ASN1TYPED_NAME_FIELD);
				if(error) goto fail;
				EMIT(body, "    "); EMIT(body, members[j].name); EMIT(body, ",\n");
			} else {
				const asn1typed_field_t *field = &type->fields[j];
				const asn1typed_type_ref_t *ref = &field->type;
				if(field->presence != ASN1TYPED_PRESENCE_MANDATORY &&
						field->presence != ASN1TYPED_PRESENCE_OPTIONAL) {
					error = "unsupported field presence (Conditional or invalid)"; goto fail;
				}
				error = add_name(members, j,
					field->ioc.symbolic_id ? field->ioc.symbolic_id : field->source_name,
					field->ioc.symbolic_id ? ASN1TYPED_NAME_IOC_FIELD : ASN1TYPED_NAME_FIELD);
				if(error) goto fail;
				error = resolve_reference(ref, module->source_name, types, module->type_count,
					&integer, &string, &spelling, NULL);
				if(error) goto fail;
				EMIT(body, "    ");
				if(field->presence == ASN1TYPED_PRESENCE_OPTIONAL) {
					optional = 1;
					EMIT(body, "std::optional<");
				}
				EMIT(body, spelling);
				if(field->presence == ASN1TYPED_PRESENCE_OPTIONAL) EMIT(body, ">");
				EMIT(body, " ");
				EMIT(body, members[j].name); EMIT(body, ";\n");
			}
		}
		EMIT(body, "};\n");
		clear_names(members, member_count); members = NULL; member_count = 0;
	}
	if(integer) EMIT(result, "#include <cstdint>\n");
	if(optional) EMIT(result, "#include <optional>\n");
	if(string) EMIT(result, "#include <string>\n");
	if(vector) EMIT(result, "#include <vector>\n");
	if(integer || optional || string || vector) EMIT(result, "\n");
	EMIT(result, body.data ? body.data : "");
	free(body.data);
	free(order);
	clear_names(types, module->type_count);
	*out = result.data;
	return 0;
fail:
	if(diagnostic && diagnostic_size) snprintf(diagnostic, diagnostic_size, "%s", error);
	free(body.data); free(result.data); free(order);
	clear_names(members, member_count);
	clear_names(types, module ? module->type_count : 0);
	return -1;
#undef EMIT
}
