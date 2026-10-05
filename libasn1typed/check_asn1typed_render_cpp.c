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

/* Separate production IR keeps the accepted T5 golden text unchanged. Both
 * successful renderings go to stdout and are compiled by the shell harness. */
static void
identifier_safety(void) {
	static const char *const sources[] = {
		"errno", "stdin", "stdout", "stderr"
	};
	static const char *const type_sources[] = {
		"B-U-F-S-I-Z", "E-O-F", "N-U-L-L", "E-D-O-M", "E-I-L-S-E-Q",
		"E-R-A-N-G-E", "W-E-O-F", "F-I-L-E", "E-I-N-V-A-L", "E-2-B-I-G"
	};
	static const char *const type_names[] = {
		"BUFSIZ", "EOF", "NULL", "EDOM", "EILSEQ", "ERANGE", "WEOF", "FILE",
		"EINVAL", "E2BIG"
	};
	static char reserved[][9] = {"__hidden", "_Upper", "_lower", "a__b"};
	asn1typed_module_t m = {0};
	asn1typed_type_t *t;
	char *text = NULL, *saved, diagnostic[256], expected[128];
	size_t i;
	assert(asn1typed_module_init(&m, "Example", "test", 1) == 0);
	alias(&m, "SafetyText", ASN1TYPED_PRIMITIVE_UTF8_STRING);
	for(i = 0; i < sizeof(type_sources) / sizeof(type_sources[0]); ++i)
		alias(&m, type_sources[i], ASN1TYPED_PRIMITIVE_INTEGER);
	t = add_type(&m, "SafeItems", ASN1TYPED_TYPE_ENUMERATED);
	for(i = 0; i < sizeof(sources) / sizeof(sources[0]); ++i) item(t, sources[i]);
	t = add_type(&m, "SafeFields", ASN1TYPED_TYPE_SEQUENCE);
	for(i = 0; i < sizeof(sources) / sizeof(sources[0]); ++i)
		field(t, sources[i], "SafetyText");
	field(t, "unrelatedSource", "E-O-F");
	assert(asn1typed_field_set_ioc(&t->fields[4], "id-NodeName",
		ASN1TYPED_CRITICALITY_IGNORE, 0, 0) == 0);
	assert(asn1typed_render_cpp(&m, &text, diagnostic, sizeof(diagnostic)) == 0);
	assert(text && !diagnostic[0]);
	for(i = 0; i < sizeof(type_names) / sizeof(type_names[0]); ++i) {
		snprintf(expected, sizeof(expected), "using cpp_%s = std::int64_t;", type_names[i]);
		assert(strstr(text, expected));
	}
	for(i = 0; i < sizeof(sources) / sizeof(sources[0]); ++i) {
		snprintf(expected, sizeof(expected), "    cpp_%s,", sources[i]);
		assert(strstr(text, expected));
		snprintf(expected, sizeof(expected), "    SafetyText cpp_%s;", sources[i]);
		assert(strstr(text, expected));
		assert(!strcmp(t->fields[i].source_name, sources[i]));
	}
	assert(strstr(text, "    cpp_EOF node_name;"));
	assert(!strstr(text, "unrelated_source"));
	assert(!strcmp(t->fields[4].ioc.symbolic_id, "id-NodeName"));
	fputs(text, stdout);
	free(text);
	fprintf(stderr, "PASS header safety, escaped reference and raw IOC symbolic identity\n");

	saved = t->fields[1].source_name;
	t->fields[1].source_name = "cpp-errno";
	failure(&m, "field post-safety collision", "collision after safety transformation");
	t->fields[1].source_name = saved;
	/* Same raw source key, different T4 styles would yield different names.
	 * The renderer rejects this misuse without changing the T4 scope API. */
	saved = t->fields[0].source_name;
	t->fields[0].source_name = t->fields[4].ioc.symbolic_id;
	failure(&m, "same source key across FIELD/IOC_FIELD", "duplicate or inconsistent");
	for(i = 0; i < sizeof(reserved) / sizeof(reserved[0]); ++i) {
		t->fields[0].source_name = reserved[i];
		/* Current T4 rejects these before the defensive C++ reserved guard. */
		failure(&m, reserved[i], "naming failed");
	}
	t->fields[0].source_name = saved;
	t = &m.types[1 + sizeof(type_sources) / sizeof(type_sources[0])];
	saved = t->enum_items[1].source_name;
	t->enum_items[1].source_name = "cpp-errno";
	failure(&m, "enum post-safety collision", "collision after safety transformation");
	t->enum_items[1].source_name = saved;
	asn1typed_module_clear(&m);
}

static void
optional_sequence_of(void) {
	static const char expected[] =
		"#include <cstdint>\n"
		"#include <optional>\n"
		"#include <string>\n"
		"#include <vector>\n\n"
		"using ItemName = std::string;\n\n"
		"struct Item {\n    ItemName name;\n};\n\n"
		"using ItemList = std::vector<Item>;\n\n"
		"using NumberList = std::vector<std::int64_t>;\n\n"
		"struct Basket {\n"
		"    ItemList items;\n"
		"    std::optional<ItemList> backup_items;\n"
		"    std::optional<std::int64_t> count;\n"
		"    NumberList numbers;\n};\n";
	asn1typed_module_t m = {0};
	asn1typed_type_t *t;
	asn1typed_type_ref_t *ref;
	char *text = NULL, *again = NULL, *saved, diagnostic[256];
	assert(asn1typed_module_init(&m, "Example", "test", 1) == 0);
	alias(&m, "ItemName", ASN1TYPED_PRIMITIVE_UTF8_STRING);
	t = add_type(&m, "Item", ASN1TYPED_TYPE_SEQUENCE);
	field(t, "name", "ItemName");
	t = add_type(&m, "ItemList", ASN1TYPED_TYPE_SEQUENCE_OF);
	assert(asn1typed_type_set_element_type(t, "Example", "Item") == 0);
	t = add_type(&m, "NumberList", ASN1TYPED_TYPE_SEQUENCE_OF);
	assert(asn1typed_type_set_element_primitive(t, ASN1TYPED_PRIMITIVE_INTEGER) == 0);
	t = add_type(&m, "Basket", ASN1TYPED_TYPE_SEQUENCE);
	field(t, "items", "ItemList");
	assert(asn1typed_type_add_field(t, "backupItems", "Example", "ItemList",
		ASN1TYPED_PRESENCE_OPTIONAL, "test", 1) == 0);
	assert(asn1typed_type_add_primitive_field(t, "count", ASN1TYPED_PRIMITIVE_INTEGER,
		ASN1TYPED_PRESENCE_OPTIONAL, "test", 1) == 0);
	field(t, "numbers", "NumberList");
	assert(asn1typed_render_cpp(&m, &text, diagnostic, sizeof(diagnostic)) == 0);
	assert(text && !diagnostic[0] && !strcmp(text, expected));
	assert(asn1typed_render_cpp(&m, &again, diagnostic, sizeof(diagnostic)) == 0);
	assert(again && text != again && !strcmp(text, again));
	fputs(text, stdout);
	free(text); free(again);
	fprintf(stderr, "PASS T6 complete expected output and deterministic repeat\n");

	ref = &m.types[2].element_type;
	saved = ref->source_name;
	ref->source_name = "Basket";
	failure(&m, "SequenceOf mutual cycle", "cyclic");
	ref->source_name = "Missing";
	failure(&m, "SequenceOf missing element", "missing local");
	ref->source_name = "ItemList";
	failure(&m, "SequenceOf self cycle", "cyclic");
	ref->source_name = NULL;
	failure(&m, "SequenceOf invalid element identity", "invalid named reference");
	ref->source_name = saved;
	saved = ref->module;
	ref->module = "Other";
	failure(&m, "SequenceOf external element", "external module");
	ref->module = saved;
	ref->kind = (asn1typed_ref_kind_e)99;
	failure(&m, "SequenceOf invalid reference kind", "invalid reference kind");
	ref->kind = ASN1TYPED_REF_NAMED;
	m.types[3].element_type.primitive_kind = ASN1TYPED_PRIMITIVE_INVALID;
	failure(&m, "SequenceOf unsupported primitive element", "unsupported primitive");
	m.types[3].element_type.primitive_kind = ASN1TYPED_PRIMITIVE_INTEGER;

	/* Optional edges participate in cycles exactly like mandatory edges. */
	m.types[1].fields[0].presence = ASN1TYPED_PRESENCE_OPTIONAL;
	ref = &m.types[1].fields[0].type;
	saved = ref->source_name;
	ref->source_name = "ItemList";
	failure(&m, "Optional SequenceOf mutual cycle", "cyclic");
	ref->source_name = "Missing";
	failure(&m, "Optional missing dependency", "missing local");
	ref->source_name = saved;
	saved = ref->module;
	ref->module = "Other";
	failure(&m, "Optional external dependency", "external module");
	ref->module = saved;
	asn1typed_module_clear(&m);
}

/* Exact minimal-include outputs also exercise named primitive identity and
 * inline BOOLEAN. A test-only namespace avoids repeating T5's Age globally. */
static void
optional_alias_and_includes(void) {
	asn1typed_module_t m = {0};
	asn1typed_type_t *t;
	char *text = NULL, diagnostic[256];
	assert(asn1typed_module_init(&m, "Example", "test", 1) == 0);
	alias(&m, "Age", ASN1TYPED_PRIMITIVE_INTEGER);
	t = add_type(&m, "OptionalAge", ASN1TYPED_TYPE_SEQUENCE);
	assert(asn1typed_type_add_field(t, "age", "Example", "Age",
		ASN1TYPED_PRESENCE_OPTIONAL, "test", 1) == 0);
	assert(asn1typed_render_cpp(&m, &text, diagnostic, sizeof(diagnostic)) == 0);
	assert(text && !diagnostic[0] && !strcmp(text,
		"#include <cstdint>\n#include <optional>\n\n"
		"using Age = std::int64_t;\n\n"
		"struct OptionalAge {\n    std::optional<Age> age;\n};\n"));
	/* Headers already appeared in the actual T6 output before this wrapper. */
	fputs("namespace alias_test {\n", stdout);
	fputs(text, stdout);
	fputs("}\n", stdout);
	free(text);
	asn1typed_module_clear(&m);

	assert(asn1typed_module_init(&m, "Example", "test", 1) == 0);
	t = add_type(&m, "OptionalFlag", ASN1TYPED_TYPE_SEQUENCE);
	assert(asn1typed_type_add_primitive_field(t, "enabled", ASN1TYPED_PRIMITIVE_BOOLEAN,
		ASN1TYPED_PRESENCE_OPTIONAL, "test", 1) == 0);
	assert(asn1typed_render_cpp(&m, &text, diagnostic, sizeof(diagnostic)) == 0);
	assert(text && !diagnostic[0] && !strcmp(text,
		"#include <optional>\n\n"
		"struct OptionalFlag {\n    std::optional<bool> enabled;\n};\n"));
	fputs(text, stdout);
	free(text);
	asn1typed_module_clear(&m);

	assert(asn1typed_module_init(&m, "Example", "test", 1) == 0);
	t = add_type(&m, "Flags", ASN1TYPED_TYPE_SEQUENCE_OF);
	assert(asn1typed_type_set_element_primitive(t, ASN1TYPED_PRIMITIVE_BOOLEAN) == 0);
	assert(asn1typed_render_cpp(&m, &text, diagnostic, sizeof(diagnostic)) == 0);
	assert(text && !diagnostic[0] && !strcmp(text,
		"#include <vector>\n\nusing Flags = std::vector<bool>;\n"));
	fputs(text, stdout);
	free(text);
	asn1typed_module_clear(&m);
	fprintf(stderr, "PASS Optional named alias, inline primitive and minimal T6 includes\n");
}

/* Capture used IR storage and every owned string, including pointer identities.
 * Comparing snapshots proves both byte content and array order are unchanged. */
struct ir_snapshot { unsigned char bytes[32768]; size_t size; };

static void
snapshot_bytes(struct ir_snapshot *s, const void *p, size_t n) {
	assert(n <= sizeof(s->bytes) - s->size);
	if(n) memcpy(s->bytes + s->size, p, n);
	s->size += n;
}

static void
snapshot_string(struct ir_snapshot *s, const char *p) {
	if(p) snapshot_bytes(s, p, strlen(p) + 1);
}

static void
snapshot_ref(struct ir_snapshot *s, const asn1typed_type_ref_t *ref) {
	snapshot_string(s, ref->module);
	snapshot_string(s, ref->source_name);
}

static void
snapshot_ir(struct ir_snapshot *s, const asn1typed_module_t *m) {
	size_t i, j;
	s->size = 0;
	snapshot_bytes(s, m, sizeof(*m));
	snapshot_string(s, m->source_name);
	snapshot_string(s, m->location.file);
	snapshot_bytes(s, m->types, m->type_count * sizeof(*m->types));
	for(i = 0; i < m->type_count; ++i) {
		const asn1typed_type_t *t = &m->types[i];
		snapshot_string(s, t->identity.module);
		snapshot_string(s, t->identity.source_name);
		snapshot_string(s, t->location.file);
		snapshot_ref(s, &t->element_type);
		snapshot_bytes(s, t->fields, t->field_count * sizeof(*t->fields));
		for(j = 0; j < t->field_count; ++j) {
			snapshot_string(s, t->fields[j].source_name);
			snapshot_ref(s, &t->fields[j].type);
			snapshot_string(s, t->fields[j].location.file);
			snapshot_string(s, t->fields[j].ioc.symbolic_id);
		}
		snapshot_bytes(s, t->enum_items, t->enum_item_count * sizeof(*t->enum_items));
		for(j = 0; j < t->enum_item_count; ++j) {
			snapshot_string(s, t->enum_items[j].source_name);
			snapshot_string(s, t->enum_items[j].location.file);
		}
	}
}

static void
assert_snapshot(const struct ir_snapshot *before, const asn1typed_module_t *m) {
	struct ir_snapshot after;
	snapshot_ir(&after, m);
	assert(before->size == after.size);
	assert(!memcmp(before->bytes, after.bytes, after.size));
}

static void
ordered_success(asn1typed_module_t *m, const char *expected,
		const char *wrapper, const char *label) {
	struct ir_snapshot before;
	char *text = NULL, *again = NULL, diagnostic[256];
	snapshot_ir(&before, m);
	assert(asn1typed_render_cpp(m, &text, diagnostic, sizeof(diagnostic)) == 0);
	assert(text && !diagnostic[0] && !strcmp(text, expected));
	assert_snapshot(&before, m);
	assert(asn1typed_render_cpp(m, &again, diagnostic, sizeof(diagnostic)) == 0);
	assert(again && !diagnostic[0] && !strcmp(text, again));
	assert_snapshot(&before, m);
	/* All needed headers have already appeared in the T5/T6 output. */
	fputs(wrapper, stdout); fputs(text, stdout); fputs("}\n", stdout);
	free(text); free(again);
	fprintf(stderr, "PASS %s; exact order, repeat and unchanged IR\n", label);
}

static void
conditional_storage(void) {
	asn1typed_module_t m = {0};
	asn1typed_type_t *t;
	assert(asn1typed_module_init(&m, "Example", "test", 1) == 0);
	t = add_type(&m, "ConditionalStorage", ASN1TYPED_TYPE_SEQUENCE);
	assert(asn1typed_type_add_field(t, "mode", "Example", "OperatingMode",
		ASN1TYPED_PRESENCE_CONDITIONAL, "test", 1) == 0);
	assert(asn1typed_type_add_primitive_field(t, "count", ASN1TYPED_PRIMITIVE_INTEGER,
		ASN1TYPED_PRESENCE_CONDITIONAL, "test", 1) == 0);
	t = add_type(&m, "OperatingMode", ASN1TYPED_TYPE_ENUMERATED);
	item(t, "active"); item(t, "standby");
	ordered_success(&m,
		"#include <cstdint>\n#include <optional>\n\n"
		"enum class OperatingMode {\n    active,\n    standby,\n};\n\n"
		"struct ConditionalStorage {\n    std::optional<OperatingMode> mode;\n"
		"    std::optional<std::int64_t> count;\n};\n",
		"namespace conditional_storage {\n", "Conditional named enum and inline INTEGER");
	asn1typed_module_clear(&m);
}

static void
declaration_planning(void) {
	asn1typed_module_t m = {0};
	asn1typed_type_t *t;
	struct ir_snapshot before;
	char *saved;
	assert(asn1typed_module_init(&m, "Example", "test", 1) == 0);
	t = add_type(&m, "Person", ASN1TYPED_TYPE_SEQUENCE);
	field(t, "name", "PersonName");
	field(t, "otherName", "PersonName"); /* distinct-edge deduplication */
	alias(&m, "PersonName", ASN1TYPED_PRIMITIVE_UTF8_STRING);
	alias(&m, "Independent", ASN1TYPED_PRIMITIVE_BOOLEAN);
	ordered_success(&m,
		"#include <string>\n\nusing PersonName = std::string;\n\n"
		"struct Person {\n    PersonName name;\n    PersonName other_name;\n};\n\n"
		"using Independent = bool;\n", "namespace forward_mandatory {\n",
		"mandatory forward, duplicate edges and newly ready priority");
	asn1typed_module_clear(&m);

	assert(asn1typed_module_init(&m, "Example", "test", 1) == 0);
	t = add_type(&m, "Container", ASN1TYPED_TYPE_SEQUENCE);
	assert(asn1typed_type_add_field(t, "age", "Example", "Age",
		ASN1TYPED_PRESENCE_OPTIONAL, "test", 1) == 0);
	alias(&m, "Age", ASN1TYPED_PRIMITIVE_INTEGER);
	ordered_success(&m,
		"#include <cstdint>\n#include <optional>\n\nusing Age = std::int64_t;\n\n"
		"struct Container {\n    std::optional<Age> age;\n};\n",
		"namespace forward_optional {\n", "Optional forward");
	asn1typed_module_clear(&m);

	assert(asn1typed_module_init(&m, "Example", "test", 1) == 0);
	t = add_type(&m, "ItemList", ASN1TYPED_TYPE_SEQUENCE_OF);
	assert(asn1typed_type_set_element_type(t, "Example", "Item") == 0);
	add_type(&m, "Item", ASN1TYPED_TYPE_SEQUENCE);
	ordered_success(&m,
		"#include <vector>\n\nstruct Item {\n};\n\nusing ItemList = std::vector<Item>;\n",
		"namespace forward_vector {\n", "SequenceOf forward");
	asn1typed_module_clear(&m);

	/* Exact T3 type graph and extraction order; supported presence for Mode
	 * permits an emission proof without exposing a production planner API. */
	assert(asn1typed_module_init(&m, "Example", "test", 1) == 0);
	t = add_type(&m, "Registration", ASN1TYPED_TYPE_SEQUENCE);
	field(t, "NodeID", "NodeNumber");
	assert(asn1typed_type_add_field(t, "NodeName", "Example", "LabelText",
		ASN1TYPED_PRESENCE_OPTIONAL, "test", 1) == 0);
	field(t, "Mode", "OperatingMode");
	field(t, "Items", "ItemCollection");
	assert(asn1typed_field_set_ioc(&t->fields[0], "id-NodeID",
		ASN1TYPED_CRITICALITY_REJECT, 1, 11) == 0);
	alias(&m, "NodeNumber", ASN1TYPED_PRIMITIVE_INTEGER);
	alias(&m, "LabelText", ASN1TYPED_PRIMITIVE_UTF8_STRING);
	t = add_type(&m, "OperatingMode", ASN1TYPED_TYPE_ENUMERATED);
	item(t, "active"); item(t, "standby");
	t = add_type(&m, "ItemCollection", ASN1TYPED_TYPE_SEQUENCE_OF);
	assert(asn1typed_type_set_element_type(t, "Example", "Item") == 0);
	t = add_type(&m, "Item", ASN1TYPED_TYPE_SEQUENCE);
	assert(asn1typed_type_add_primitive_field(t, "count", ASN1TYPED_PRIMITIVE_INTEGER,
		ASN1TYPED_PRESENCE_MANDATORY, "test", 1) == 0);
	assert(asn1typed_type_add_field(t, "label", "Example", "LabelText",
		ASN1TYPED_PRESENCE_OPTIONAL, "test", 1) == 0);
	ordered_success(&m,
		"#include <cstdint>\n#include <optional>\n#include <string>\n#include <vector>\n\n"
		"using NodeNumber = std::int64_t;\n\nusing LabelText = std::string;\n\n"
		"enum class OperatingMode {\n    active,\n    standby,\n};\n\n"
		"struct Item {\n    std::int64_t count;\n    std::optional<LabelText> label;\n};\n\n"
		"using ItemCollection = std::vector<Item>;\n\n"
		"struct Registration {\n    NodeNumber node_id;\n    std::optional<LabelText> node_name;\n"
		"    OperatingMode mode;\n    ItemCollection items;\n};\n",
		"namespace t3_order {\n", "T3 equivalent graph ordering");
	m.types[0].fields[2].presence = (asn1typed_presence_e)99;
	snapshot_ir(&before, &m);
	failure(&m, "invalid presence after planning", "unsupported field presence");
	assert_snapshot(&before, &m);
	asn1typed_module_clear(&m);

	assert(asn1typed_module_init(&m, "Example", "test", 1) == 0);
	t = add_type(&m, "Blocked", ASN1TYPED_TYPE_SEQUENCE); field(t, "a", "A");
	t = add_type(&m, "A", ASN1TYPED_TYPE_SEQUENCE); field(t, "b", "B");
	t = add_type(&m, "B", ASN1TYPED_TYPE_SEQUENCE); field(t, "c", "C");
	t = add_type(&m, "C", ASN1TYPED_TYPE_SEQUENCE); field(t, "a", "A");
	snapshot_ir(&before, &m);
	failure(&m, "three-node cycle with blocked user", "cyclic or cyclically blocked");
	assert_snapshot(&before, &m);
	asn1typed_module_clear(&m);

	/* Generated AB must not be used as an alternative raw identity for aB. */
	assert(asn1typed_module_init(&m, "Example", "test", 1) == 0);
	t = add_type(&m, "User", ASN1TYPED_TYPE_SEQUENCE); field(t, "value", "aB");
	alias(&m, "aB", ASN1TYPED_PRIMITIVE_BOOLEAN);
	ordered_success(&m, "using AB = bool;\n\nstruct User {\n    AB value;\n};\n",
		"namespace forward_identity {\n", "forward raw identity spelling reused");
	saved = m.types[0].fields[0].type.source_name;
	m.types[0].fields[0].type.source_name = "AB";
	failure(&m, "generated spelling is not source identity", "missing local");
	m.types[0].fields[0].type.source_name = saved;
	/* Full-table collision must precede reference resolution. */
	alias(&m, "A-B", ASN1TYPED_PRIMITIVE_BOOLEAN);
	m.types[0].fields[0].type.source_name = "Missing";
	failure(&m, "complete table detects later collision first", "collision after safety");
	m.types[0].fields[0].type.source_name = saved;
	asn1typed_module_clear(&m);
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
	t->fields[0].presence = (asn1typed_presence_e)99;
	failure(&m, "invalid presence", "unsupported field presence");
	t->fields[0].presence = ASN1TYPED_PRESENCE_MANDATORY;
	m.types[7].kind = ASN1TYPED_TYPE_SEQUENCE_OF;
	failure(&m, "SequenceOf absent element identity", "invalid named reference");
	m.types[7].kind = ASN1TYPED_TYPE_SEQUENCE;
	m.types[0].primitive_kind = ASN1TYPED_PRIMITIVE_INVALID;
	failure(&m, "invalid primitive alias", "unsupported primitive");
	m.types[0].primitive_kind = ASN1TYPED_PRIMITIVE_UTF8_STRING;
	t->fields[6].type.primitive_kind = ASN1TYPED_PRIMITIVE_INVALID;
	failure(&m, "invalid inline primitive", "unsupported primitive");
	t->fields[6].type.primitive_kind = ASN1TYPED_PRIMITIVE_BOOLEAN;
	saved = t->fields[1].source_name;
	t->fields[1].source_name = "person-name";
	failure(&m, "normalized member collision", "collision after safety transformation");
	t->fields[1].source_name = "bad_name";
	failure(&m, "invalid source identity", "naming failed");
	t->fields[1].source_name = t->fields[0].source_name;
	failure(&m, "duplicate source identity", "duplicate or inconsistent");
	t->fields[1].source_name = saved;
	saved = t->fields[0].type.source_name;
	t->fields[0].type.source_name = "Household";
	failure(&m, "Person Household mutual cycle", "cyclic");
	t->fields[0].type.source_name = "Person";
	failure(&m, "Sequence self cycle", "cyclic");
	t->fields[0].type.source_name = "Missing";
	failure(&m, "missing dependency", "missing local");
	t->fields[0].type.source_name = saved;
	saved = t->fields[0].type.module;
	t->fields[0].type.module = "Other";
	failure(&m, "external dependency", "external module");
	t->fields[0].type.module = saved;
	m.types[7].kind = (asn1typed_type_kind_e)99;
	failure(&m, "unknown type kind", "unsupported type kind");
	m.types[7].kind = ASN1TYPED_TYPE_SEQUENCE;
	saved = m.types[7].identity.source_name;
	m.types[7].identity.source_name = "person";
	failure(&m, "type collision", "collision after safety transformation");
	m.types[7].identity.source_name = m.types[6].identity.source_name;
	failure(&m, "exact duplicate type source identity", "duplicate or inconsistent");
	m.types[7].identity.source_name = saved;
	asn1typed_module_clear(&m);

	assert(asn1typed_module_init(&bad, "Example", "test", 1) == 0);
	t = add_type(&bad, "Mode", ASN1TYPED_TYPE_ENUMERATED);
	item(t, "class"); item(t, "Class");
	/* Both accepted identities really traverse keyword escaping to class_.
	 * T4 cannot produce a distinct unescaped class_: underscores are invalid
	 * input and trailing hyphens are discarded, so escaping adds no new
	 * collisions today. Still check the final, escaped namespace. */
	failure(&bad, "enum post-escape collision", "collision after safety transformation");
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
	optional_sequence_of();
	/* Compile identifier-safety declarations with the new headers present. */
	identifier_safety();
	optional_alias_and_includes();
	declaration_planning();
	conditional_storage();
	return 0;
}
