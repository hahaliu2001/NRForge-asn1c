#include "asn1typed_extract.h"
#include <asn1fix.h>

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef T5_FIXTURE
#define T5_FIXTURE "fixtures/parameterized-reference-b7a.asn1"
#endif

void check_asn1typed_ioc(void);
void check_asn1typed_multimodule(void);

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

static asn1p_expr_t *fixture_declaration(asn1p_t *tree, const char *name);

static void
check_parameterized_reference_identity(void) {
	asn1p_t *tree = asn1p_parse_file(T5_FIXTURE, A1P_NOFLAGS);
	asn1p_expr_t *bad_sequence, *bad_sequence_of;
	asn1p_expr_t *root_field, *root_container, *root_message;
	asn1typed_module_t ir;
	asn1typed_type_t *a, *again, *b;
	char error[256];
	assert(tree != NULL);
	assert(asn1f_process(tree, A1F_NOFLAGS, NULL) >= 0);
	bad_sequence = fixture_declaration(tree, "BadSequence");
	bad_sequence_of = fixture_declaration(tree, "BadSequenceOf");
	root_field = fixture_declaration(tree, "B7A-Field");
	root_container = fixture_declaration(tree, "B7A-Container");
	root_message = fixture_declaration(tree, "RootMessage");
	assert(bad_sequence && bad_sequence_of && root_field && root_container && root_message);
	bad_sequence->meta_type = AMT_VALUE;
	bad_sequence_of->meta_type = AMT_VALUE;
	root_field->meta_type = AMT_VALUE;
	root_container->meta_type = AMT_VALUE;
	root_message->meta_type = AMT_VALUE;
	assert(asn1typed_extract_module(tree, "ParameterizedReferenceB7A", &ir,
		error, sizeof(error)) == 0);
	/* The owned binding must not depend on the parser/fixer tree. */
	asn1p_delete(tree);
	a = find_type(&ir, "UseA");
	again = find_type(&ir, "UseAAgain");
	b = find_type(&ir, "UseB");
	assert(a && again && b);
	assert(a->alternatives[0].type_ref.actual_count == 1);
	assert(again->alternatives[0].type_ref.actual_count == 1);
	assert(b->alternatives[0].type_ref.actual_count == 1);
	assert(!strcmp(a->alternatives[0].type_ref.module, "ParameterizedReferenceB7A"));
	assert(!strcmp(a->alternatives[0].type_ref.source_name, "Target"));
	assert(!strcmp(a->alternatives[0].type_ref.actuals[0].module,
		"ParameterizedReferenceB7A"));
	assert(!strcmp(a->alternatives[0].type_ref.actuals[0].source_name, "SetA"));
	assert(a->alternatives[0].type_ref.actuals[0].kind ==
		ASN1TYPED_ACTUAL_OBJECT_SET_REFERENCE);
	assert(asn1typed_type_ref_equal(&a->alternatives[0].type_ref,
		&again->alternatives[0].type_ref));
	assert(!asn1typed_type_ref_equal(&a->alternatives[0].type_ref,
		&b->alternatives[0].type_ref));
	assert(!strcmp(b->alternatives[0].type_ref.actuals[0].source_name, "SetB"));
	assert(a->alternatives[0].type_ref.actual_count == 1);
	assert(again->alternatives[0].type_ref.actual_count == 1);
	assert(b->alternatives[0].type_ref.actual_count == 1);
	assert(a->alternatives[0].type_ref.kind == ASN1TYPED_REF_NAMED);
	assert(a->alternatives[0].type_ref.actual_count != 0);
	assert(find_type(&ir, "OrdinaryUse")->alternatives[0].type_ref.actual_count == 0);
	asn1typed_module_clear(&ir);
}

static asn1p_expr_t *
fixture_declaration(asn1p_t *tree, const char *name) {
	asn1p_module_t *module;
	asn1p_expr_t *decl;
	TQ_FOR(module, &tree->modules, mod_next)
		TQ_FOR(decl, &module->members, next)
			if(decl->Identifier && !strcmp(decl->Identifier, name)) return decl;
	return NULL;
}

static asn1p_ref_t *
fixture_actual_ref(asn1p_expr_t *decl) {
	asn1p_expr_t *alternative = TQ_FIRST(&decl->members);
	asn1p_expr_t *parameter = alternative && alternative->rhs_pspecs ?
		TQ_FIRST(&alternative->rhs_pspecs->members) : NULL;
	asn1p_value_t *value;
	asn1p_constraint_t *constraint;
	asn1p_expr_t *expr;
	assert(parameter && parameter->constraints &&
		parameter->constraints->type == ACT_EL_TYPE);
	value = parameter->constraints->containedSubtype;
	assert(value);
	if(value->type == ATV_REFERENCED) return value->value.reference;
	if(value->type == ATV_TYPE) {
		expr = value->value.v_type;
		assert(expr && expr->expr_type == A1TC_REFERENCE && expr->reference);
		return expr->reference;
	}
	assert(value->type == ATV_VALUESET && (constraint = value->value.constraint) &&
		constraint->containedSubtype);
	value = constraint->containedSubtype;
	assert(value->type == ATV_REFERENCED && value->value.reference);
	return value->value.reference;
}

static void
expect_bad_parameterized_actual(int failure_kind) {
	asn1p_t *tree = asn1p_parse_file(T5_FIXTURE, A1P_NOFLAGS);
	asn1p_expr_t *decl;
	asn1p_expr_t *alternative, *parameter;
	asn1p_ref_t *ref;
	asn1typed_module_t ir;
	char error[256];
	assert(tree && asn1f_process(tree, A1F_NOFLAGS, NULL) >= 0);
	decl = fixture_declaration(tree, "UseA");
	assert(decl);
	alternative = TQ_FIRST(&decl->members);
	parameter = TQ_FIRST(&alternative->rhs_pspecs->members);
	if(failure_kind == 2) {
		/* A setting with the wrong representation must fail closed. */
		parameter->constraints->type = ACT_CA_SET;
	} else {
		ref = fixture_actual_ref(decl);
		free(ref->components[0].name);
		ref->components[0].name = strdup(failure_kind == 0 ? "MissingSet" : "Named");
		assert(ref->components[0].name);
		ref->ref_expr = NULL;
	}
	assert(asn1typed_extract_module(tree, "ParameterizedReferenceB7A", &ir,
		error, sizeof(error)) != 0);
	assert(error[0]);
	assert(ir.source_name == NULL && ir.types == NULL && ir.type_count == 0);
	asn1typed_module_clear(&ir);
	asn1typed_module_clear(&ir);
	asn1p_delete(tree);
}

static void
expect_rejected_parameterized_shape(const char *bad_name,
		const char *other_name, const char *expected_error) {
	asn1p_t *tree = asn1p_parse_file(T5_FIXTURE, A1P_NOFLAGS);
	asn1p_expr_t *bad, *other;
	asn1typed_module_t ir;
	char error[256];
	assert(tree && asn1f_process(tree, A1F_NOFLAGS, NULL) >= 0);
	bad = fixture_declaration(tree, bad_name);
	other = fixture_declaration(tree, other_name);
	assert(bad && other);
	other->meta_type = AMT_VALUE;
	assert(asn1typed_extract_module(tree, "ParameterizedReferenceB7A", &ir,
		error, sizeof(error)) != 0);
	assert(error[0] && strstr(error, expected_error));
	assert(ir.source_name == NULL && ir.types == NULL && ir.type_count == 0);
	asn1typed_module_clear(&ir);
	asn1typed_module_clear(&ir);
	asn1p_delete(tree);
}

static void
check_parameterized_closure_rejected(void) {
	asn1p_t *tree = asn1p_parse_file(T5_FIXTURE, A1P_NOFLAGS);
	asn1typed_module_t ir;
	char error[256];
	assert(tree && asn1f_process(tree, A1F_NOFLAGS, NULL) >= 0);
	assert(asn1typed_extract_message(tree, "ParameterizedReferenceB7A",
		"RootMessage", &ir, error, sizeof(error)) != 0);
	assert(strstr(error, "parameterized dependency") != NULL);
	assert(strstr(error, "ParameterizedReferenceB7A.Target") != NULL);
	assert(strstr(error, "ParameterizedReferenceB7A.SetA") != NULL);
	assert(strstr(error, "is not materialized") != NULL);
	assert(ir.source_name == NULL && ir.types == NULL && ir.type_count == 0);
	asn1typed_module_clear(&ir);
	asn1typed_module_clear(&ir);
	asn1p_delete(tree);
}

int
main(void) {
	asn1p_t *tree = asn1p_parse_file(T2_FIXTURE, A1P_NOFLAGS);
	asn1typed_module_t ir;
	asn1typed_type_t *type;
	char error[256];
	const char unsupported[] = "Unsupported DEFINITIONS ::= BEGIN\n"
		"Bad ::= CHOICE { a OCTET STRING }\nEND\n";
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
	const char malformed_choice[] = "BadChoice DEFINITIONS ::= BEGIN\n"
		"Label ::= UTF8String\nBad ::= CHOICE { first BOOLEAN, second Label }\nEND\n";
	size_t i;
	const char *items[] = { "v32", "v64", "v128", "v256" };
	check_parameterized_reference_identity();
	expect_bad_parameterized_actual(0); /* unresolved object set */
	expect_bad_parameterized_actual(1); /* resolves to a type, not a set */
	expect_bad_parameterized_actual(2); /* unsupported setting representation */
	expect_rejected_parameterized_shape("BadSequence", "BadSequenceOf",
		"parameterized SEQUENCE field reference");
	expect_rejected_parameterized_shape("BadSequenceOf", "BadSequence",
		"parameterized SEQUENCE OF element reference");
	check_parameterized_closure_rejected();
	assert(tree != NULL);
	assert(asn1f_process(tree, A1F_NOFLAGS, NULL) >= 0);
	assert(asn1typed_extract_module(tree, "OrdinaryTypes", &ir,
		error, sizeof(error)) == 0);
	assert(ir.type_count == 10);
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
	type = find_type(&ir, "SimpleChoice");
	assert(type && type->kind == ASN1TYPED_TYPE_CHOICE);
	assert(type->alternative_count == 2);
	assert(strcmp(type->alternatives[0].source_name, "label") == 0);
	assert(type->alternatives[0].type_ref.kind == ASN1TYPED_REF_NAMED);
	assert(strcmp(type->alternatives[0].type_ref.module, "OrdinaryTypes") == 0);
	assert(strcmp(type->alternatives[0].type_ref.source_name, "LabelText") == 0);
	assert(strcmp(type->alternatives[1].source_name, "flag") == 0);
	assert(type->alternatives[1].type_ref.kind == ASN1TYPED_REF_PRIMITIVE);
	assert(type->alternatives[1].type_ref.primitive_kind == ASN1TYPED_PRIMITIVE_BOOLEAN);
	assert(type->alternatives[0].location.file && type->alternatives[0].location.line > 0);
	assert(type->alternatives[1].location.file && type->alternatives[1].location.line > 0);

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
	/* A valid first alternative followed by a broken one exercises partial
	 * CHOICE construction cleanup. */
	tree = asn1p_parse_buffer(malformed_choice, -1, "bad-choice.asn", 1,
		A1P_NOFLAGS);
	assert(tree != NULL);
	assert(asn1f_process(tree, A1F_NOFLAGS, NULL) >= 0);
	{
		asn1p_module_t *module = TQ_FIRST(&tree->modules);
		asn1p_expr_t *decl;
		asn1p_expr_t *alternative;
		assert(module != NULL);
		for(decl = TQ_FIRST(&module->members); decl;
			decl = TQ_NEXT(decl, next))
			if(decl->Identifier && !strcmp(decl->Identifier, "Bad")) break;
		assert(decl != NULL);
		alternative = TQ_FIRST(&decl->members);
		assert(alternative != NULL);
		alternative->Identifier = NULL;
	}
	assert(asn1typed_extract_module(tree, "BadChoice", &ir,
		error, sizeof(error)) != 0);
	assert(strstr(error, "unnamed CHOICE alternative") != NULL);
	assert_ir_cleared(&ir);
	asn1p_delete(tree);
	/* Fail after one owned alternative when its named reference is unresolved. */
	tree = asn1p_parse_buffer(malformed_choice, -1, "bad-choice.asn", 1,
		A1P_NOFLAGS);
	assert(tree != NULL);
	assert(asn1f_process(tree, A1F_NOFLAGS, NULL) >= 0);
	{
		asn1p_module_t *module = TQ_FIRST(&tree->modules);
		asn1p_expr_t *decl;
		asn1p_expr_t *alternative;
		assert(module != NULL);
		for(decl = TQ_FIRST(&module->members); decl;
			decl = TQ_NEXT(decl, next))
			if(decl->Identifier && !strcmp(decl->Identifier, "Bad")) break;
		assert(decl != NULL);
		alternative = TQ_FIRST(&decl->members);
		assert(alternative != NULL);
		alternative = TQ_NEXT(alternative, next);
		assert(alternative && alternative->reference);
		alternative->reference->ref_expr = NULL;
	}
	assert(asn1typed_extract_module(tree, "BadChoice", &ir,
		error, sizeof(error)) != 0);
	assert(strstr(error, "unresolved CHOICE alternative") != NULL);
	assert_ir_cleared(&ir);
	asn1p_delete(tree);

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
	check_asn1typed_multimodule();
	return 0;
}
