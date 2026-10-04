#include "asn1typed_extract.h"
#include <asn1fix.h>

#include <assert.h>
#include <stdio.h>
#include <string.h>

void check_asn1typed_ioc(void);

static asn1typed_type_t *
find_type(asn1typed_module_t *module, const char *name) {
	size_t i;
	for(i = 0; i < module->type_count; ++i)
		if(!strcmp(module->types[i].identity.source_name, name))
			return &module->types[i];
	return NULL;
}

static void
assert_ir_cleared(asn1typed_module_t *ir) {
	assert(ir->source_name == NULL);
	assert(ir->location.file == NULL);
	assert(ir->types == NULL);
	assert(ir->type_count == 0 && ir->type_capacity == 0);
	/* Also prove that failure output remains safe to clear by its caller. */
	asn1typed_module_clear(ir);
}

static void
expect_rejected(const char *source, const char *module_name) {
	asn1p_t *tree = asn1p_parse_buffer(source, -1, "negative-t2.asn", 1,
		A1P_NOFLAGS);
	asn1typed_module_t ir;
	char error[256];
	assert(tree != NULL);
	assert(asn1f_process(tree, A1F_NOFLAGS, NULL) >= 0);
	assert(asn1typed_extract_module(tree, module_name, &ir,
		error, sizeof(error)) != 0);
	assert(error[0] != '\0');
	assert_ir_cleared(&ir);
	asn1p_delete(tree);
}

int
main(void) {
	asn1p_t *tree = asn1p_parse_file(T2_FIXTURE, A1P_NOFLAGS);
	asn1typed_module_t ir;
	asn1typed_type_t *type;
	char error[256];
	const char unsupported[] = "Unsupported DEFINITIONS ::= BEGIN\n"
		"Bad ::= CHOICE { a INTEGER }\nEND\n";
	const char bad_builtin[] = "BadFields DEFINITIONS ::= BEGIN\n"
		"Bad ::= SEQUENCE { data OCTET STRING }\nEND\n";
	const char extension[] = "ExtTest DEFINITIONS ::= BEGIN\n"
		"Extensible ::= SEQUENCE { value INTEGER, ... }\nEND\n";
	const char default_value[] = "DefaultTest DEFINITIONS ::= BEGIN\n"
		"WithDefault ::= SEQUENCE { value INTEGER DEFAULT 1 }\nEND\n";
	const char inline_enum[] = "InlineEnum DEFINITIONS ::= BEGIN\n"
		"Bad ::= SEQUENCE { color ENUMERATED { red, green } }\nEND\n";
	const char unresolved[] = "Unresolved DEFINITIONS ::= BEGIN\n"
		"Target ::= INTEGER\nBad ::= SEQUENCE { value Target }\nEND\n";
	size_t i;
	const char *items[] = { "v32", "v64", "v128", "v256" };
	assert(tree != NULL);
	assert(asn1f_process(tree, A1F_NOFLAGS, NULL) >= 0);
	assert(asn1typed_extract_module(tree, "OrdinaryTypes", &ir,
		error, sizeof(error)) == 0);
	assert(ir.type_count == 8);
	assert(strcmp(ir.source_name, "OrdinaryTypes") == 0);
	assert(ir.location.file && ir.location.file[0]);
	assert(ir.location.line > 0);

	/* Destroy the parser/fixer tree before inspecting any extracted data. */
	asn1p_delete(tree);
	assert(strcmp(ir.types[0].identity.module, "OrdinaryTypes") == 0);
	type = find_type(&ir, "PersonName");
	assert(type && type->kind == ASN1TYPED_TYPE_PRIMITIVE);
	assert(type->primitive_kind == ASN1TYPED_PRIMITIVE_UTF8_STRING);
	type = find_type(&ir, "Age");
	assert(type && type->kind == ASN1TYPED_TYPE_PRIMITIVE);
	assert(type->primitive_kind == ASN1TYPED_PRIMITIVE_INTEGER);
	type = find_type(&ir, "Enabled");
	assert(type && type->kind == ASN1TYPED_TYPE_PRIMITIVE);
	assert(type->primitive_kind == ASN1TYPED_PRIMITIVE_BOOLEAN);
	type = find_type(&ir, "DisplayName");
	assert(type && type->kind == ASN1TYPED_TYPE_PRIMITIVE);
	assert(strcmp(type->identity.module, "OrdinaryTypes") == 0);
	assert(strcmp(type->identity.source_name, "DisplayName") == 0);
	assert(type->primitive_kind == ASN1TYPED_PRIMITIVE_PRINTABLE_STRING);

	type = find_type(&ir, "Person");
	assert(type && type->kind == ASN1TYPED_TYPE_SEQUENCE);
	assert(type->field_count == 4);
	assert(strcmp(type->fields[0].source_name, "name") == 0);
	assert(type->fields[0].type.kind == ASN1TYPED_REF_NAMED);
	assert(strcmp(type->fields[0].type.module, "OrdinaryTypes") == 0);
	assert(strcmp(type->fields[0].type.source_name, "PersonName") == 0);
	assert(type->fields[0].presence == ASN1TYPED_PRESENCE_MANDATORY);
	assert(strcmp(type->fields[1].source_name, "age") == 0);
	assert(strcmp(type->fields[1].type.source_name, "Age") == 0);
	assert(type->fields[1].presence == ASN1TYPED_PRESENCE_OPTIONAL);
	assert(strcmp(type->fields[2].source_name, "enabled") == 0);
	assert(strcmp(type->fields[2].type.source_name, "Enabled") == 0);
	assert(type->fields[2].presence == ASN1TYPED_PRESENCE_MANDATORY);
	assert(strcmp(type->fields[3].source_name, "rawCount") == 0);
	assert(type->fields[3].type.kind == ASN1TYPED_REF_PRIMITIVE);
	assert(type->fields[3].type.primitive_kind == ASN1TYPED_PRIMITIVE_INTEGER);
	assert(type->fields[3].presence == ASN1TYPED_PRESENCE_MANDATORY);
	for(i = 0; i < type->field_count; ++i) {
		assert(type->fields[i].location.file && type->fields[i].location.file[0]);
		assert(type->fields[i].location.line > 0);
	}

	type = find_type(&ir, "PersonList");
	assert(type && type->kind == ASN1TYPED_TYPE_SEQUENCE_OF);
	assert(type->element_type.kind == ASN1TYPED_REF_NAMED);
	assert(strcmp(type->element_type.module, "OrdinaryTypes") == 0);
	assert(strcmp(type->element_type.source_name, "Person") == 0);
	type = find_type(&ir, "BoundedPersonList");
	assert(type && type->kind == ASN1TYPED_TYPE_SEQUENCE_OF);
	assert(strcmp(type->element_type.source_name, "Person") == 0);

	type = find_type(&ir, "PagingDRX");
	assert(type && type->kind == ASN1TYPED_TYPE_ENUMERATED);
	assert(type->enum_item_count == sizeof(items) / sizeof(items[0]));
	for(i = 0; i < type->enum_item_count; ++i) {
		assert(strcmp(type->enum_items[i].source_name, items[i]) == 0);
		assert(type->enum_items[i].location.line > 0);
	}

	asn1typed_module_clear(&ir);
	tree = asn1p_parse_buffer(unsupported, -1, "unsupported.asn", 1,
		A1P_NOFLAGS);
	assert(tree != NULL);
	assert(asn1f_process(tree, A1F_NOFLAGS, NULL) >= 0);
	assert(asn1typed_extract_module(tree, "Unsupported", &ir,
		error, sizeof(error)) != 0);
	assert(error[0] != '\0');
	assert(ir.source_name == NULL && ir.types == NULL);
	asn1p_delete(tree);

	expect_rejected(bad_builtin, "BadFields");
	expect_rejected(extension, "ExtTest");
	expect_rejected(default_value, "DefaultTest");
	expect_rejected(inline_enum, "InlineEnum");

	/* Simulate a fixed-tree reference whose semantic target is unavailable. */
	tree = asn1p_parse_buffer(unresolved, -1, "unresolved-t2.asn", 1,
		A1P_NOFLAGS);
	assert(tree != NULL);
	assert(asn1f_process(tree, A1F_NOFLAGS, NULL) >= 0);
	{
		asn1p_module_t *module = TQ_FIRST(&tree->modules);
		asn1p_expr_t *decl;
		asn1p_expr_t *field;
		assert(module != NULL);
		for(decl = TQ_FIRST(&module->members); decl;
			decl = TQ_NEXT(decl, next))
			if(decl->Identifier && !strcmp(decl->Identifier, "Bad")) break;
		assert(decl != NULL);
		field = TQ_FIRST(&decl->members);
		assert(field && field->reference);
		field->reference->ref_expr = NULL;
	}
	assert(asn1typed_extract_module(tree, "Unresolved", &ir,
		error, sizeof(error)) != 0);
	assert(error[0] != '\0');
	assert_ir_cleared(&ir);
	asn1p_delete(tree);
	puts("T2 ordinary ASN.1 extraction: PASS (IR valid after parser tree destruction)");
	check_asn1typed_ioc();
	return 0;
}
