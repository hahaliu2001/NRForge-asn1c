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

int
asn1typed_render_cpp(const asn1typed_module_t *module, char **out,
		char *diagnostic, size_t diagnostic_size) {
	struct text_buffer body = {0}, result = {0};
	struct identifier *types = NULL, *members = NULL;
	size_t i, j, k, member_count = 0;
	int integer = 0, string = 0;
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
		if(!types) { error = "out of memory"; goto fail; }
	}
	for(i = 0; i < module->type_count; ++i) {
		const asn1typed_type_t *type = &module->types[i];
		if(!type->identity.module || strcmp(type->identity.module, module->source_name)) {
			error = "type identity does not belong to module"; goto fail;
		}
		error = add_name(types, i, type->identity.source_name, ASN1TYPED_NAME_TYPE);
		if(error) goto fail;
		if(i) EMIT(body, "\n");
		if(type->kind == ASN1TYPED_TYPE_PRIMITIVE) {
			spelling = primitive(type->primitive_kind, &integer, &string);
			if(!spelling) { error = "unsupported primitive kind"; goto fail; }
			EMIT(body, "using "); EMIT(body, types[i].name);
			EMIT(body, " = "); EMIT(body, spelling); EMIT(body, ";\n");
			continue;
		}
		if(type->kind != ASN1TYPED_TYPE_SEQUENCE && type->kind != ASN1TYPED_TYPE_ENUMERATED) {
			error = "unsupported type kind (including SequenceOf)"; goto fail;
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
				if(field->presence != ASN1TYPED_PRESENCE_MANDATORY) {
					error = "unsupported field presence (Optional/Conditional)"; goto fail;
				}
				error = add_name(members, j,
					field->ioc.symbolic_id ? field->ioc.symbolic_id : field->source_name,
					field->ioc.symbolic_id ? ASN1TYPED_NAME_IOC_FIELD : ASN1TYPED_NAME_FIELD);
				if(error) goto fail;
				spelling = NULL;
				if(ref->kind == ASN1TYPED_REF_PRIMITIVE) {
					spelling = primitive(ref->primitive_kind, &integer, &string);
					if(!spelling) { error = "unsupported primitive kind"; goto fail; }
				} else if(ref->kind == ASN1TYPED_REF_NAMED && ref->module && ref->source_name) {
					for(k = 0; k < i; ++k)
						if(!strcmp(ref->module, module->source_name) &&
							!strcmp(ref->source_name, types[k].source)) { spelling = types[k].name; break; }
				}
				if(!spelling) { error = "unresolved reference: dependency-ready local IR order required"; goto fail; }
				EMIT(body, "    "); EMIT(body, spelling); EMIT(body, " ");
				EMIT(body, members[j].name); EMIT(body, ";\n");
			}
		}
		EMIT(body, "};\n");
		clear_names(members, member_count); members = NULL; member_count = 0;
	}
	if(integer) EMIT(result, "#include <cstdint>\n");
	if(string) EMIT(result, "#include <string>\n");
	if(integer || string) EMIT(result, "\n");
	EMIT(result, body.data ? body.data : "");
	free(body.data);
	clear_names(types, module->type_count);
	*out = result.data;
	return 0;
fail:
	if(diagnostic && diagnostic_size) snprintf(diagnostic, diagnostic_size, "%s", error);
	free(body.data); free(result.data);
	clear_names(members, member_count);
	clear_names(types, module ? module->type_count : 0);
	return -1;
#undef EMIT
}
