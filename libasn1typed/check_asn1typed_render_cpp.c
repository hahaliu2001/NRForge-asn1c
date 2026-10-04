#include "asn1typed_render_cpp.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static asn1typed_type_t *
add_type(asn1typed_module_t *m, const char *name, asn1typed_type_kind_e kind) {
	asn1typed_type_t *t = NULL;
	assert(asn1typed_module_add_type(m, name, kind, "test", 1, &t) == 0);
	return t;
}

static void
alias(asn1typed_module_t *m, const char *name, asn1typed_primitive_kind_e kind) {
	assert(asn1typed_type_set_primitive(add_type(m, name, ASN1TYPED_TYPE_PRIMITIVE), kind) == 0);
}

static void
field(asn1typed_type_t *t, const char *name, const char *ref) {
	assert(asn1typed_type_add_field(t, name, "Example", ref,
		ASN1TYPED_PRESENCE_MANDATORY, "test", 1) == 0);
}

static void
item(asn1typed_type_t *t, const char *name) {
	assert(asn1typed_type_add_enum_item(t, name, "test", 1) == 0);
}

static void
failure(asn1typed_module_t *m, const char *label, const char *expected) {
	char sentinel, *text = &sentinel, diagnostic[256] = "";
	assert(asn1typed_render_cpp(m, &text, diagnostic, sizeof(diagnostic)) == -1);
	assert(text == NULL);
	assert(diagnostic[0] && strstr(diagnostic, expected));
	fprintf(stderr, "PASS %s: %s; output NULL\n", label, diagnostic);
}

int
main(void) {
	static const char expected[] =
		"#include <cstdint>\n"
		"#include <string>\n\n"
		"using PersonName = std::string;\n\n"
		"using Age = std::int64_t;\n\n"
		"using Enabled = bool;\n\n"
		"using PrintableName = std::string;\n\n"
		"using AB = bool;\n\n"
		"enum class PagingDrx {\n"
		"    active,\n    standby,\n    v_32,\n    v_64,\n    v_128,\n    v_256,\n"
		"    class_,\n    struct_,\n    namespace_,\n    template_,\n"
		"    operator_,\n    new_,\n    delete_,\n};\n\n"
		"struct Person {\n"
		"    PersonName person_name;\n    Age age;\n    Enabled enabled;\n"
		"    PagingDrx default_paging_drx;\n    PrintableName label;\n"
		"    AB flag;\n    bool class_;\n};\n\n"
		"struct Household {\n    Person person;\n};\n";
	static const char *const items[] = {
		"active", "standby", "v32", "v64", "v128", "v256", "class",
		"struct", "namespace", "template", "operator", "new", "delete"
	};
	asn1typed_module_t m = {0}, bad = {0};
	asn1typed_type_t *t;
	char *text = NULL, *again = NULL, *saved;
	char diagnostic[256];
	size_t i;
	assert(asn1typed_module_init(&m, "Example", "test", 1) == 0);
	alias(&m, "PersonName", ASN1TYPED_PRIMITIVE_UTF8_STRING);
	alias(&m, "Age", ASN1TYPED_PRIMITIVE_INTEGER);
	alias(&m, "Enabled", ASN1TYPED_PRIMITIVE_BOOLEAN);
	alias(&m, "PrintableName", ASN1TYPED_PRIMITIVE_PRINTABLE_STRING);
	/* TYPE normalization is not idempotent: aB -> AB -> Ab. */
	alias(&m, "aB", ASN1TYPED_PRIMITIVE_BOOLEAN);
	t = add_type(&m, "PagingDRX", ASN1TYPED_TYPE_ENUMERATED);
	for(i = 0; i < sizeof(items) / sizeof(items[0]); ++i) item(t, items[i]);
	t = add_type(&m, "Person", ASN1TYPED_TYPE_SEQUENCE);
	field(t, "personName", "PersonName");
	field(t, "age", "Age");
	field(t, "enabled", "Enabled");
	field(t, "DefaultPagingDRX", "PagingDRX");
	field(t, "label", "PrintableName");
	field(t, "flag", "aB");
	assert(asn1typed_type_add_primitive_field(t, "class", ASN1TYPED_PRIMITIVE_BOOLEAN,
		ASN1TYPED_PRESENCE_MANDATORY, "test", 1) == 0);
	t = add_type(&m, "Household", ASN1TYPED_TYPE_SEQUENCE);
	field(t, "person", "Person");
	assert(asn1typed_render_cpp(&m, &text, diagnostic, sizeof(diagnostic)) == 0);
	assert(diagnostic[0] == '\0' && text && !strcmp(text, expected));
	assert(asn1typed_render_cpp(&m, &again, diagnostic, sizeof(diagnostic)) == 0);
	assert(text != again && !strcmp(text, again));
	/* stdout is exclusively the actual generated text consumed by the C++ test. */
	fputs(text, stdout);
	free(text); free(again);
	fprintf(stderr, "PASS complete expected output and deterministic repeat\n");

	t = &m.types[6];
	t->fields[0].presence = ASN1TYPED_PRESENCE_OPTIONAL;
	failure(&m, "Optional", "unsupported field presence");
	t->fields[0].presence = ASN1TYPED_PRESENCE_CONDITIONAL;
	failure(&m, "Conditional", "unsupported field presence");
	t->fields[0].presence = ASN1TYPED_PRESENCE_MANDATORY;
	m.types[7].kind = ASN1TYPED_TYPE_SEQUENCE_OF;
	failure(&m, "SequenceOf", "unsupported type kind");
	m.types[7].kind = ASN1TYPED_TYPE_SEQUENCE;
	m.types[0].primitive_kind = ASN1TYPED_PRIMITIVE_INVALID;
	failure(&m, "invalid primitive alias", "unsupported primitive");
	m.types[0].primitive_kind = ASN1TYPED_PRIMITIVE_UTF8_STRING;
	t->fields[6].type.primitive_kind = ASN1TYPED_PRIMITIVE_INVALID;
	failure(&m, "invalid inline primitive", "unsupported primitive");
	t->fields[6].type.primitive_kind = ASN1TYPED_PRIMITIVE_BOOLEAN;
	saved = t->fields[1].source_name;
	t->fields[1].source_name = "person-name";
	failure(&m, "normalized member collision", "collision after keyword escaping");
	t->fields[1].source_name = "bad_name";
	failure(&m, "invalid source identity", "naming failed");
	t->fields[1].source_name = t->fields[0].source_name;
	failure(&m, "duplicate source identity", "duplicate or inconsistent");
	t->fields[1].source_name = saved;
	saved = t->fields[0].type.source_name;
	t->fields[0].type.source_name = "Household";
	failure(&m, "forward dependency", "dependency-ready");
	t->fields[0].type.source_name = "Person";
	failure(&m, "recursive dependency", "dependency-ready");
	t->fields[0].type.source_name = "Missing";
	failure(&m, "missing dependency", "dependency-ready");
	t->fields[0].type.source_name = saved;
	saved = t->fields[0].type.module;
	t->fields[0].type.module = "Other";
	failure(&m, "external dependency", "dependency-ready");
	t->fields[0].type.module = saved;
	m.types[7].kind = (asn1typed_type_kind_e)99;
	failure(&m, "unknown type kind", "unsupported type kind");
	m.types[7].kind = ASN1TYPED_TYPE_SEQUENCE;
	saved = m.types[7].identity.source_name;
	m.types[7].identity.source_name = "person";
	failure(&m, "type collision", "collision after keyword escaping");
	m.types[7].identity.source_name = saved;
	asn1typed_module_clear(&m);

	assert(asn1typed_module_init(&bad, "Example", "test", 1) == 0);
	t = add_type(&bad, "Mode", ASN1TYPED_TYPE_ENUMERATED);
	item(t, "class"); item(t, "Class");
	/* Both accepted identities really traverse keyword escaping to class_.
	 * T4 cannot produce a distinct unescaped class_: underscores are invalid
	 * input and trailing hyphens are discarded, so escaping adds no new
	 * collisions today. Still check the final, escaped namespace. */
	failure(&bad, "enum post-escape collision", "collision after keyword escaping");
	asn1typed_module_clear(&bad);

	assert(asn1typed_module_init(&bad, "Example", "test", 1) == 0);
	alias(&bad, "Flag", ASN1TYPED_PRIMITIVE_BOOLEAN);
	assert(asn1typed_render_cpp(&bad, &text, diagnostic, sizeof(diagnostic)) == 0);
	assert(!strcmp(text, "using Flag = bool;\n"));
	free(text);
	asn1typed_module_clear(&bad);
	assert(asn1typed_module_init(&bad, "Example", "test", 1) == 0);
	assert(asn1typed_render_cpp(&bad, &text, diagnostic, sizeof(diagnostic)) == 0);
	assert(text && !text[0]);
	free(text);
	asn1typed_module_clear(&bad);
	fprintf(stderr, "PASS minimal includes and empty module\n");
	return 0;
}
