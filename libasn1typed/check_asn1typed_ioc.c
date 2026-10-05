#include "asn1typed_extract.h"
#include <asn1fix.h>

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef T4_FIXTURE
#error T4_FIXTURE must name the cross-module fixture
#endif

void check_asn1typed_ioc(void);
void check_asn1typed_multimodule(void);

static asn1p_t *
fixed_fixture(void) {
	asn1p_t *tree = asn1p_parse_file(T3_FIXTURE, A1P_NOFLAGS);
	assert(tree);
	assert(asn1f_process(tree, A1F_NOFLAGS, NULL) >= 0);
	return tree;
}

static asn1p_expr_t *
declaration(asn1p_t *tree, const char *name) {
	asn1p_module_t *module = TQ_FIRST(&tree->modules);
	asn1p_expr_t *expr;
	TQ_FOR(expr, &module->members, next)
		if(expr->Identifier && !strcmp(expr->Identifier, name)) return expr;
	assert(!"missing fixture declaration");
	return NULL;
}

static asn1typed_type_t *
type_named(asn1typed_module_t *ir, const char *name) {
	size_t i;
	for(i = 0; i < ir->type_count; ++i)
		if(!strcmp(ir->types[i].identity.source_name, name)) return &ir->types[i];
	assert(!"missing extracted type");
	return NULL;
}

static void
reject_message(asn1p_t *tree, const char *name, const char *label, const char *expected) {
	asn1typed_module_t ir;
	char error[256];
	int rc = asn1typed_extract_message(tree, "SyntheticIOC", name,
		&ir, error, sizeof(error));
	if(rc != -1) fprintf(stderr, "T3 negative %s unexpectedly succeeded\n", label);
	assert(rc == -1);
	assert(strstr(error, expected));
	assert(!ir.source_name && !ir.location.file && !ir.types);
	assert(!ir.type_count && !ir.type_capacity);
	asn1typed_module_clear(&ir);
	/* Failed extraction is safe to clear repeatedly. */
	asn1typed_module_clear(&ir);
	printf("T3 negative %s: PASS (%s; IR cleared)\n", label, error);
}

static void
reject(asn1p_t *tree, const char *label, const char *expected) {
	reject_message(tree, "Registration", label, expected);
}

static void
reject_missing_correlated_relation(asn1p_t *tree, const char *field_name,
		const char *label) {
	asn1p_expr_t *decl = declaration(tree, "CorrelatedProtocolIE-Field"), *member;
	const asn1p_constraint_t *ct = NULL;
	asn1p_constraint_t *saved;
	TQ_FOR(member, &decl->specializations.pspec[0].my_clone->members, next) {
		if(!strcmp(member->Identifier, field_name)) {
			ct = asn1p_get_component_relation_constraint(member->constraints);
			break;
		}
	}
	assert(ct && ct->el_count == 2 && ct->elements[1]);
	saved = ct->elements[1];
	ct->elements[1] = NULL;
	reject_message(tree, "ExtensibleRegistration", label, "object-set association");
	ct->elements[1] = saved;
}

void
check_asn1typed_ioc(void) {
	asn1p_t *tree = fixed_fixture();
	asn1typed_module_t ir;
	asn1typed_type_t *message, *type;
	asn1p_expr_t *set, *container;
	asn1p_ioc_row_t *row;
	asn1p_ref_t *choice_ref = NULL;
	asn1p_expr_t *saved_choice_target = NULL;
	char saved_choice_name[11];
	char error[256];
	size_t i;
	const char *names[] = { "NodeID", "NodeName", "Mode", "Items" };
	const char *symbols[] = { "id-NodeID", "id-NodeName", "id-Mode", "id-Items" };
	const char *types[] = { "NodeNumber", "LabelText", "OperatingMode", "ItemCollection" };
	const asn1typed_presence_e presence[] = {
		ASN1TYPED_PRESENCE_MANDATORY, ASN1TYPED_PRESENCE_OPTIONAL,
		ASN1TYPED_PRESENCE_CONDITIONAL, ASN1TYPED_PRESENCE_MANDATORY
	};
	const asn1typed_criticality_e criticality[] = {
		ASN1TYPED_CRITICALITY_REJECT, ASN1TYPED_CRITICALITY_IGNORE,
		ASN1TYPED_CRITICALITY_NOTIFY, ASN1TYPED_CRITICALITY_REJECT
	};
	set = declaration(tree, "RegistrationIEs");
	assert(set->ioc_table && set->ioc_table->rows == 4);
	for(i = 0; i < set->ioc_table->rows; ++i) {
		struct asn1p_ioc_cell_s *cell =
			asn1p_ioc_row_cell_fetch(set->ioc_table->row[i], "&criticality");
		asn1p_expr_t *setting = cell ? cell->value : NULL;
		asn1p_ref_t *ref = setting && setting->value &&
			setting->value->type == ATV_REFERENCED ?
			setting->value->value.reference : NULL;
		assert(setting && setting->Identifier && setting->meta_type == AMT_VALUE);
		assert(ref && ref->components && ref->comp_count > 0 && ref->comp_count <= 2);
		assert(ref->components[ref->comp_count - 1].name);
		assert(!strcmp(ref->components[ref->comp_count - 1].name,
			setting->Identifier));
	}
	{
		asn1p_expr_t *extensible = declaration(tree, "ExtensibleRegistration");
		asn1p_expr_t *payload = TQ_FIRST(&extensible->members);
		asn1p_expr_t *marker = payload ? TQ_NEXT(payload, next) : NULL;
		assert(payload && payload->expr_type == A1TC_REFERENCE);
		assert(marker && marker->expr_type == A1TC_EXTENSIBLE);
		assert(!TQ_NEXT(marker, next));
		if(asn1typed_extract_message(tree, "SyntheticIOC", "ExtensibleRegistration",
				&ir, error, sizeof(error))) {
			fprintf(stderr, "T3 extension-marker association failed: %s\n", error);
			assert(0);
		}
		assert(ir.types[0].field_count == 4);
		assert(!strcmp(ir.types[0].fields[0].source_name, "NodeID"));
		asn1typed_module_clear(&ir);
		puts("T3 trailing sequence extension marker association: PASS");
	}
	if(asn1typed_extract_message(tree, "SyntheticIOC", "Registration",
			&ir, error, sizeof(error))) {
		fprintf(stderr, "T3 positive extraction failed: %s\n", error);
		assert(0);
	}
	/* Every assertion below reads owned data after the parser is destroyed. */
	asn1p_delete(tree);
	assert(!strcmp(ir.source_name, "SyntheticIOC"));
	assert(ir.type_count == 6);
	/* Worklist order also establishes the dependency-cleanup test below. */
	for(i = 0; i < 4; ++i)
		assert(!strcmp(ir.types[i + 1].identity.source_name, types[i]));
	assert(!strcmp(ir.types[5].identity.source_name, "Item"));
	message = &ir.types[0];
	assert(!strcmp(message->identity.source_name, "Registration"));
	assert(!strcmp(message->identity.module, "SyntheticIOC"));
	assert(message->kind == ASN1TYPED_TYPE_SEQUENCE && message->field_count == 4);
	assert(message->location.file && message->location.line > 0);
	for(i = 0; i < 4; ++i) {
		asn1typed_field_t *field = &message->fields[i];
		assert(!strcmp(field->source_name, names[i]));
		assert(strcmp(field->source_name, "protocolIEs"));
		assert(field->type.kind == ASN1TYPED_REF_NAMED);
		assert(!strcmp(field->type.module, "SyntheticIOC"));
		assert(!strcmp(field->type.source_name, types[i]));
		assert(field->presence == presence[i]);
		assert(field->location.file && field->location.line > 0);
		assert(!strcmp(field->ioc.symbolic_id, symbols[i]));
		assert(field->ioc.criticality == criticality[i]);
		assert(field->ioc.has_numeric_id && field->ioc.numeric_id == (intmax_t)(11 * (i + 1)));
		printf("T3 Registration.%s -> SyntheticIOC.%s, presence=%d, id=%s/%jd, criticality=%d\n",
			field->source_name, field->type.source_name, (int)field->presence,
			field->ioc.symbolic_id, field->ioc.numeric_id, (int)field->ioc.criticality);
	}
	type = type_named(&ir, "NodeNumber");
	assert(type->kind == ASN1TYPED_TYPE_PRIMITIVE &&
		type->primitive_kind == ASN1TYPED_PRIMITIVE_INTEGER);
	type = type_named(&ir, "LabelText");
	assert(type->kind == ASN1TYPED_TYPE_PRIMITIVE &&
		type->primitive_kind == ASN1TYPED_PRIMITIVE_UTF8_STRING);
	type = type_named(&ir, "OperatingMode");
	assert(type->kind == ASN1TYPED_TYPE_ENUMERATED && type->enum_item_count == 2);
	assert(!strcmp(type->enum_items[0].source_name, "active"));
	assert(!strcmp(type->enum_items[1].source_name, "standby"));
	type = type_named(&ir, "ItemCollection");
	assert(type->kind == ASN1TYPED_TYPE_SEQUENCE_OF);
	assert(type->element_type.kind == ASN1TYPED_REF_NAMED);
	assert(!strcmp(type->element_type.module, "SyntheticIOC"));
	assert(!strcmp(type->element_type.source_name, "Item"));
	type = type_named(&ir, "Item");
	assert(type->kind == ASN1TYPED_TYPE_SEQUENCE && type->field_count == 2);
	assert(type->fields[0].type.kind == ASN1TYPED_REF_PRIMITIVE);
	assert(type->fields[0].type.primitive_kind == ASN1TYPED_PRIMITIVE_INTEGER);
	assert(!type->fields[0].ioc.symbolic_id);
	assert(type->fields[1].presence == ASN1TYPED_PRESENCE_OPTIONAL);
	assert(!strcmp(type->fields[1].type.source_name, "LabelText"));
	asn1typed_module_clear(&ir);
	puts("T3 owned IOC IR after parser-tree destruction: PASS");
	/* The only route to ChoiceOnlyDependency is through NodeChoice's two
	 * alternatives. Both alternatives deliberately name the same dependency. */
	tree = fixed_fixture();
	set = declaration(tree, "RegistrationIEs");
	{
		struct asn1p_ioc_cell_s *value =
			asn1p_ioc_row_cell_fetch(set->ioc_table->row[0], "&Value");
		asn1p_expr_t *choice = declaration(tree, "NodeChoice");
		asn1p_expr_t *setting = value ? value->value : NULL;
		asn1p_ref_t *ref = setting ? setting->reference : NULL;
		assert(ref && ref->comp_count == 1 && ref->components[0].name);
		assert(strlen(choice->Identifier) == 10);
		memcpy(saved_choice_name, ref->components[0].name,
			sizeof(saved_choice_name));
		choice_ref = ref;
		saved_choice_target = ref->ref_expr;
		memcpy(ref->components[0].name, choice->Identifier,
			sizeof(saved_choice_name));
		ref->ref_expr = choice;
	}
	if(asn1typed_extract_message(tree, "SyntheticIOC", "Registration",
			&ir, error, sizeof(error))) {
		fprintf(stderr, "T3 CHOICE dependency extraction failed: %s\n", error);
		assert(0);
	}
	memcpy(choice_ref->components[0].name, saved_choice_name,
		sizeof(saved_choice_name));
	choice_ref->ref_expr = saved_choice_target;
	asn1p_delete(tree);
	assert(ir.type_count == 7);
	assert(!strcmp(ir.types[0].identity.source_name, "Registration"));
	type = type_named(&ir, "NodeChoice");
	assert(type->kind == ASN1TYPED_TYPE_CHOICE && type->alternative_count == 3);
	assert(!strcmp(type->alternatives[0].type_ref.source_name,
		"ChoiceOnlyDependency"));
	assert(!strcmp(type->alternatives[1].type_ref.source_name,
		"ChoiceOnlyDependency"));
	assert(type->alternatives[2].type_ref.kind == ASN1TYPED_REF_PRIMITIVE);
	assert(type->alternatives[2].type_ref.primitive_kind ==
		ASN1TYPED_PRIMITIVE_BOOLEAN);
	type = type_named(&ir, "ChoiceOnlyDependency");
	assert(type->kind == ASN1TYPED_TYPE_SEQUENCE);
	{
		size_t count = 0;
		for(i = 0; i < ir.type_count; ++i)
			if(!strcmp(ir.types[i].identity.source_name,
					"ChoiceOnlyDependency")) ++count;
		assert(count == 1);
	}
	asn1typed_module_clear(&ir);
	puts("T3 CHOICE-only dependency closure and deduplication: PASS");
	{
		struct asn1p_ioc_cell_s *presence_cell;
		asn1p_expr_t *setting;
		asn1p_value_t direct, *saved_value;
		char *saved_identifier;
		char *identities[] = { "mandatory", "optional", "conditional" };
		const asn1typed_presence_e expected[] = {
			ASN1TYPED_PRESENCE_MANDATORY, ASN1TYPED_PRESENCE_OPTIONAL,
			ASN1TYPED_PRESENCE_CONDITIONAL
		};
		tree = fixed_fixture();
		set = declaration(tree, "RegistrationIEs");
		presence_cell = asn1p_ioc_row_cell_fetch(set->ioc_table->row[0], "&presence");
		assert(presence_cell && presence_cell->value->meta_type == AMT_VALUE);
		setting = presence_cell->value;
		direct = *setting->value;
		direct.type = ATV_INTEGER;
		saved_value = setting->value;
		saved_identifier = setting->Identifier;
		setting->value = &direct;
		{
			size_t j;
			for(j = 0; j < 3; ++j) {
				setting->Identifier = identities[j];
				direct.value.v_integer = (asn1c_integer_t)(7001 + j * 97);
				assert(asn1typed_extract_message(tree, "SyntheticIOC", "Registration",
						&ir, error, sizeof(error)) == 0);
				assert(ir.types[0].fields[0].presence == expected[j]);
				asn1typed_module_clear(&ir);
			}
		}
		setting->Identifier = "unknown-presence";
		reject(tree, "direct integer unknown presence", "unrecognized IOC presence");
		setting->Identifier = NULL;
		reject(tree, "direct integer presence missing Identifier", "unrecognized IOC presence");
		setting->Identifier = "mandatory";
		direct.type = ATV_TRUE;
		reject(tree, "unsupported direct presence value shape", "unrecognized IOC presence");
		setting->value = saved_value;
		setting->Identifier = saved_identifier;
		asn1p_delete(tree);
		puts("T3 resolved ATV_INTEGER presence identities and fail-closed shapes: PASS");
	}
	{
		struct asn1p_ioc_cell_s *criticality_cell;
		asn1p_expr_t *setting;
		asn1p_value_t direct, *saved_value;
		char *saved_identifier;
		char *identities[] = { "reject", "ignore", "notify" };
		const asn1typed_criticality_e expected[] = {
			ASN1TYPED_CRITICALITY_REJECT, ASN1TYPED_CRITICALITY_IGNORE,
			ASN1TYPED_CRITICALITY_NOTIFY
		};
		tree = fixed_fixture();
		set = declaration(tree, "RegistrationIEs");
		criticality_cell = asn1p_ioc_row_cell_fetch(set->ioc_table->row[0],
			"&criticality");
		assert(criticality_cell && criticality_cell->value->meta_type == AMT_VALUE);
		setting = criticality_cell->value;
		direct = *setting->value;
		direct.type = ATV_INTEGER;
		saved_value = setting->value;
		saved_identifier = setting->Identifier;
		setting->value = &direct;
		for(i = 0; i < 3; ++i) {
			setting->Identifier = identities[i];
			/* Deliberately unrelated ordinals prove identity drives selection. */
			direct.value.v_integer = (asn1c_integer_t)(7001 + i * 97);
			assert(asn1typed_extract_message(tree, "SyntheticIOC", "Registration",
					&ir, error, sizeof(error)) == 0);
			assert(ir.types[0].fields[0].ioc.criticality == expected[i]);
			asn1typed_module_clear(&ir);
		}
		setting->value = saved_value;
		setting->Identifier = saved_identifier;
		asn1p_delete(tree);
		puts("T3 direct ATV_INTEGER criticality identities: PASS");
	}
	{
		struct asn1p_ioc_cell_s *criticality_cell;
		asn1p_expr_t *setting;
		asn1p_value_t direct;
		asn1p_value_t *saved_value;
		char *saved_identifier;
		tree = fixed_fixture();
		set = declaration(tree, "RegistrationIEs");
		criticality_cell = asn1p_ioc_row_cell_fetch(set->ioc_table->row[0],
			"&criticality");
		setting = criticality_cell->value;
		direct = *setting->value;
		direct.type = ATV_INTEGER;
		direct.value.v_integer = 811;
		saved_value = setting->value;
		saved_identifier = setting->Identifier;
		setting->value = &direct;
		setting->Identifier = "";
		reject(tree, "direct integer criticality empty Identifier",
			"unrecognized IOC criticality");
		setting->Identifier = NULL;
		reject(tree, "direct integer criticality missing Identifier",
			"unrecognized IOC criticality");
		setting->Identifier = "unknown-criticality";
		reject(tree, "direct integer unknown criticality",
			"unrecognized IOC criticality");
		setting->Identifier = "reject";
		direct.type = ATV_TRUE;
		reject(tree, "unsupported criticality value shape",
			"unrecognized IOC criticality");
		setting->value = saved_value;
		setting->Identifier = saved_identifier;
		asn1p_delete(tree);
	}
	{
		struct asn1p_ioc_cell_s *criticality_cell;
		asn1p_expr_t *setting;
		asn1p_ref_t *ref;
		char *saved_component;
		tree = fixed_fixture();
		set = declaration(tree, "RegistrationIEs");
		criticality_cell = asn1p_ioc_row_cell_fetch(set->ioc_table->row[0],
			"&criticality");
		setting = criticality_cell->value;
		ref = setting->value->value.reference;
		assert(ref && ref->components && ref->comp_count);
		saved_component = ref->components[ref->comp_count - 1].name;
		ref->components[ref->comp_count - 1].name = "ignore";
		reject(tree, "criticality Identifier/reference mismatch",
			"unrecognized IOC criticality");
		ref->components[ref->comp_count - 1].name = saved_component;
		asn1p_delete(tree);
		puts("T3 criticality reference mismatch rejected: PASS");
	}
	{
		struct asn1p_ioc_cell_s *presence_cell;
		asn1p_expr_t *setting;
		asn1p_ref_t *ref;
		char *saved_component;
		tree = fixed_fixture();
		set = declaration(tree, "RegistrationIEs");
		presence_cell = asn1p_ioc_row_cell_fetch(set->ioc_table->row[0], "&presence");
		setting = presence_cell->value;
		ref = setting->value->value.reference;
		assert(ref && ref->components && ref->comp_count);
		saved_component = ref->components[ref->comp_count - 1].name;
		ref->components[ref->comp_count - 1].name = "optional";
		reject(tree, "presence Identifier/reference mismatch", "unrecognized IOC presence");
		ref->components[ref->comp_count - 1].name = saved_component;
		asn1p_delete(tree);
	}
	{
		struct asn1p_ioc_cell_s *id_cell;
		asn1p_expr_t *setting;
		asn1p_value_t direct;
		tree = fixed_fixture();
		set = declaration(tree, "RegistrationIEs");
		id_cell = asn1p_ioc_row_cell_fetch(set->ioc_table->row[0], "&id");
		assert(id_cell && id_cell->value->meta_type == AMT_VALUE);
		setting = id_cell->value;
		direct = *setting->value;
		direct.type = ATV_INTEGER;
		direct.value.v_integer = 7001;
		{
			asn1p_value_t *saved = setting->value;
			setting->value = &direct;
			assert(asn1typed_extract_message(tree, "SyntheticIOC", "Registration",
					&ir, error, sizeof(error)) == 0);
			setting->value = saved;
		}
		asn1p_delete(tree);
		assert(!strcmp(ir.types[0].fields[0].ioc.symbolic_id, "id-NodeID"));
		assert(ir.types[0].fields[0].ioc.has_numeric_id);
		assert(ir.types[0].fields[0].ioc.numeric_id == 7001);
		asn1typed_module_clear(&ir);
		tree = fixed_fixture();
		set = declaration(tree, "RegistrationIEs");
		id_cell = asn1p_ioc_row_cell_fetch(set->ioc_table->row[0], "&id");
		setting = id_cell->value;
		direct = *setting->value;
		direct.type = ATV_INTEGER;
		direct.value.v_integer = (asn1c_integer_t)INTMAX_MAX + 1;
		{
			asn1p_value_t *saved = setting->value;
			setting->value = &direct;
			assert(asn1typed_extract_message(tree, "SyntheticIOC", "Registration",
					&ir, error, sizeof(error)) == 0);
			setting->value = saved;
		}
		asn1p_delete(tree);
		assert(!strcmp(ir.types[0].fields[0].ioc.symbolic_id, "id-NodeID"));
		assert(!ir.types[0].fields[0].ioc.has_numeric_id);
		asn1typed_module_clear(&ir);
		puts("T3 direct ATV_INTEGER id identity, numeric value, and overflow policy: PASS");
	}
	{
		struct asn1p_ioc_cell_s *id_cell;
		asn1p_expr_t *setting;
		asn1p_value_t direct;
		tree = fixed_fixture();
		set = declaration(tree, "RegistrationIEs");
		id_cell = asn1p_ioc_row_cell_fetch(set->ioc_table->row[0], "&id");
		setting = id_cell->value;
		direct = *setting->value;
		direct.type = ATV_INTEGER;
		direct.value.v_integer = 27;
		{
			asn1p_value_t *saved_value = setting->value;
			char *saved_identifier = setting->Identifier;
			setting->value = &direct;
			setting->Identifier = NULL;
			reject(tree, "direct integer missing Identifier",
				"missing or invalid symbolic IOC id");
			setting->Identifier = "";
			reject(tree, "direct integer empty Identifier",
				"missing or invalid symbolic IOC id");
			setting->Identifier = saved_identifier;
			setting->value = saved_value;
		}
		asn1p_delete(tree);
	}
	{
		struct asn1p_ioc_cell_s *id_cell;
		asn1p_expr_t *setting;
		asn1p_ref_t *ref;
		char *saved_component, *saved_identifier;
		tree = fixed_fixture();
		set = declaration(tree, "RegistrationIEs");
		id_cell = asn1p_ioc_row_cell_fetch(set->ioc_table->row[0], "&id");
		setting = id_cell->value;
		ref = setting->value->value.reference;
		assert(ref && ref->comp_count == 1);
		saved_component = ref->components[0].name;
		saved_identifier = setting->Identifier;
		ref->components[0].name = "id-B";
		setting->Identifier = "id-A";
		reject(tree, "id Identifier/reference mismatch",
			"missing or invalid symbolic IOC id");
		ref->components[0].name = saved_component;
		setting->Identifier = saved_identifier;
		ref->components[0].name = "id-Missing";
		setting->Identifier = "id-Missing";
		reject(tree, "unresolved id reference",
			"missing or invalid symbolic IOC id");
		ref->components[0].name = saved_component;
		setting->Identifier = saved_identifier;
		ref->components[0].name = "NodeNumber";
		setting->Identifier = "NodeNumber";
		reject(tree, "id reference resolves to non-value",
			"missing or invalid symbolic IOC id");
		ref->components[0].name = saved_component;
		setting->Identifier = saved_identifier;
		asn1p_delete(tree);
	}

	/* Mutate the real fixed fixture for focused malformed-semantic tests.
	 * Restore borrowed slots before freeing the parser tree. */
	tree = fixed_fixture();
	set = declaration(tree, "RegistrationIEs");
	container = TQ_FIRST(&declaration(tree, "Registration")->members);
	{
		asn1p_expr_t *unrelated = declaration(tree, "UnrelatedContainer");
		assert(unrelated->lhs_params && unrelated->expr_type == ASN_CONSTR_SEQUENCE_OF);
		assert(TQ_FIRST(&unrelated->members)->expr_type == ASN_BASIC_INTEGER);
		assert(TQ_FIRST(&declaration(tree, "UnrelatedMessage")->members)->rhs_pspecs);
		reject_message(tree, "UnrelatedMessage", "unused object-set parameter",
			"object-set association");
	}
	{
		/* Mutate the fixed specialization actually used by the container. */
		asn1p_expr_t *decl = declaration(tree, "ProtocolIE-Field"), *member;
		const asn1p_constraint_t *ct = NULL;
		asn1p_ref_t *ref;
		char *saved_path;
		asn1p_value_t replacement, *saved_setting;
		asn1p_ref_t replacement_ref;
		assert(decl->specializations.pspecs_count == 1);
		TQ_FOR(member, &decl->specializations.pspec[0].my_clone->members, next) {
			if(!strcmp(member->Identifier, "value")) {
				ct = asn1p_get_component_relation_constraint(member->constraints);
				break;
			}
		}
		assert(ct && ct->el_count == 2);
		ref = ct->elements[1]->value->value.reference;
		saved_path = ref->components[0].name;
		ref->components[0].name = "@criticality";
		reject(tree, "wrong identifier component relation", "object-set association");
		ref->components[0].name = saved_path;
		assert(ct->elements[0]->value->type == ATV_VALUESET);
		ref = ct->elements[0]->value->value.constraint->containedSubtype->value.v_type->reference;
		/* Specialization shares the set reference with the outer actual.
		 * Replace only this constraint slot so the outer set stays intact. */
		replacement_ref = *ref;
		replacement_ref.ref_expr = declaration(tree, "OtherIEs");
		memset(&replacement, 0, sizeof(replacement));
		replacement.type = ATV_REFERENCED;
		replacement.value.reference = &replacement_ref;
		saved_setting = ct->elements[0]->value;
		ct->elements[0]->value = &replacement;
		reject(tree, "different open-type object set", "object-set association");
		ct->elements[0]->value = saved_setting;
	}
	{
		asn1p_expr_t *decl = declaration(tree, "CorrelatedProtocolIE-Field"), *member;
		const asn1p_constraint_t *ct = NULL;
		asn1p_ref_t *ref;
		char *saved_path;
		TQ_FOR(member, &decl->specializations.pspec[0].my_clone->members, next) {
			if(!strcmp(member->Identifier, "criticality")) {
				ct = asn1p_get_component_relation_constraint(member->constraints);
				break;
			}
		}
		assert(ct && ct->el_count == 2);
		ref = ct->elements[1]->value->value.reference;
		saved_path = ref->components[0].name;
		ref->components[0].name = "@value";
		reject_message(tree, "ExtensibleRegistration", "wrong criticality id relation",
			"object-set association");
		ref->components[0].name = saved_path;
	}
	reject_missing_correlated_relation(tree, "criticality",
		"missing criticality component relation");
	reject_missing_correlated_relation(tree, "value",
		"missing Value component relation");
	{
		asn1p_expr_t *saved = container->rhs_pspecs;
		container->rhs_pspecs = NULL;
		reject(tree, "missing association", "object-set association");
		container->rhs_pspecs = saved;
	}
	{
		asn1p_expr_t *parameter = TQ_FIRST(&container->rhs_pspecs->members);
		asn1p_ref_t *ref = parameter->constraints->containedSubtype->value.v_type->reference;
		asn1p_expr_t *saved = ref->ref_expr;
		ref->ref_expr = declaration(tree, "NodeNumber");
		reject(tree, "invalid association", "object-set association");
		ref->ref_expr = saved;
	}
	{
		asn1p_ioc_table_t *saved = set->ioc_table;
		set->ioc_table = NULL;
		reject(tree, "missing table", "IOC table");
		set->ioc_table = saved;
	}
	row = set->ioc_table->row[2];
	{
		/* Keep every required column present and append a duplicate. */
		struct asn1p_ioc_cell_s *value = asn1p_ioc_row_cell_fetch(row, "&Value");
		struct asn1p_ioc_cell_s columns[5], *saved = row->column;
		assert(row->columns == 4);
		memcpy(columns, row->column, 4 * sizeof(columns[0]));
		columns[4] = *value;
		row->column = columns;
		row->columns = 5;
		reject(tree, "duplicate required Value column", "missing or duplicated");
		row->column = saved;
		row->columns = 4;
	}
	{
		/* Rows all succeed. NodeNumber, LabelText, OperatingMode and
		 * ItemCollection are populated before the worklist reaches Item. */
		asn1p_expr_t *count = TQ_FIRST(&declaration(tree, "Item")->members);
		asn1p_expr_type_e saved = count->expr_type;
		count->expr_type = ASN_BASIC_OCTET_STRING;
		reject(tree, "failure after dependency population", "unsupported field type");
		count->expr_type = saved;
	}
	{
		const char *required[] = { "&id", "&Value", "&presence", "&criticality" };
		for(i = 0; i < 4; ++i) {
			struct asn1p_ioc_cell_s *cell = asn1p_ioc_row_cell_fetch(row, required[i]);
			asn1p_expr_t *saved = cell->value;
			cell->value = NULL;
			reject(tree, required[i], "required id/Value/presence/criticality");
			cell->value = saved;
		}
	}
	{
		struct asn1p_ioc_cell_s *cell = asn1p_ioc_row_cell_fetch(row, "&presence");
		asn1p_ref_t *ref = cell->value->value->value.reference;
		char *saved = ref->components[0].name;
		ref->components[0].name = "unknown-presence";
		reject(tree, "unknown presence", "unrecognized IOC presence");
		ref->components[0].name = saved;
	}
	{
		asn1p_expr_t *decl = declaration(tree, "OperatingMode");
		asn1p_expr_type_e saved = decl->expr_type;
		decl->expr_type = ASN_BASIC_OCTET_STRING;
		reject(tree, "unsupported Value", "unsupported IOC Value");
		decl->expr_type = saved;
	}
	{
		struct asn1p_ioc_cell_s *cell = asn1p_ioc_row_cell_fetch(row, "&Value");
		asn1p_ref_t *ref = cell->value->reference;
		asn1p_expr_t *saved_target = ref->ref_expr;
		char *saved_name = ref->components[0].name;
		ref->ref_expr = NULL;
		ref->components[0].name = "AbsentValueType";
		reject(tree, "unresolved Value", "unresolved or unsupported IOC Value");
		ref->ref_expr = saved_target;
		ref->components[0].name = saved_name;
	}
	{
		asn1p_ioc_row_t *saved = set->ioc_table->row[2];
		set->ioc_table->row[2] = NULL;
		reject(tree, "malformed row", "malformed IOC row");
		set->ioc_table->row[2] = saved;
	}
	/* Same fixed fixture: select three rows, change their order, and reorder
	 * columns. Extraction must follow rows and match columns by identity. */
	{
		asn1p_ioc_row_t *first = set->ioc_table->row[0];
		struct asn1p_ioc_cell_s first_cell = row->column[0];
		set->ioc_table->row[0] = row;
		set->ioc_table->row[2] = first;
		set->ioc_table->rows = 3;
		row->column[0] = row->column[3];
		row->column[3] = first_cell;
		assert(asn1typed_extract_message(tree, "SyntheticIOC", "Registration",
			&ir, error, sizeof(error)) == 0);
		row->column[3] = row->column[0];
		row->column[0] = first_cell;
		set->ioc_table->rows = 4;
		set->ioc_table->row[0] = first;
		set->ioc_table->row[2] = row;
	}
	asn1p_delete(tree);
	assert(ir.types[0].field_count == 3);
	assert(!strcmp(ir.types[0].fields[0].source_name, "Mode"));
	assert(!strcmp(ir.types[0].fields[0].type.source_name, "OperatingMode"));
	assert(ir.types[0].fields[0].presence == ASN1TYPED_PRESENCE_CONDITIONAL);
	assert(!strcmp(ir.types[0].fields[1].source_name, "NodeName"));
	assert(!strcmp(ir.types[0].fields[2].source_name, "NodeID"));
	asn1typed_module_clear(&ir);
	puts("T3 variable row count/order and reordered columns: PASS");
}

static asn1typed_type_t *
type_identity(asn1typed_module_t *ir, const char *module, const char *name) {
	size_t i;
	for(i = 0; i < ir->type_count; ++i)
		if(!strcmp(ir->types[i].identity.module, module) &&
			!strcmp(ir->types[i].identity.source_name, name)) return &ir->types[i];
	return NULL;
}

static void
rename_module_b_type_for_identity_test(asn1p_t *tree) {
	asn1p_module_t *module;
	asn1p_expr_t *decl;
	TQ_FOR(module, &tree->modules, mod_next)
		if(!strcmp(module->ModuleName, "ModuleB")) break;
	assert(module);
	TQ_FOR(decl, &module->members, next)
		if(decl->Identifier && !strcmp(decl->Identifier, "TypeY")) break;
	assert(decl);
	/* The fixer rejects duplicate declarations across modules in this compact
	 * fixture; rename after fixing to exercise owned identity/materialization. */
	decl->Identifier = strdup("TypeX");
	assert(decl->Identifier);
}

void
check_asn1typed_multimodule(void) {
	asn1p_t *tree = asn1p_parse_file(T4_FIXTURE, A1P_NOFLAGS);
	asn1typed_module_t ir;
	asn1typed_type_t *root, *a_type_x, *b_type, *b_type_x;
	char error[256];
	assert(tree && asn1f_process(tree, A1F_NOFLAGS, NULL) >= 0);
	rename_module_b_type_for_identity_test(tree);
	assert(asn1typed_extract_message(tree, "ModuleA", "Registration", &ir,
		error, sizeof(error)) == 0);
	asn1p_delete(tree);
	assert(!strcmp(ir.source_name, "ModuleA"));
	root = type_identity(&ir, "ModuleA", "Registration");
	a_type_x = type_identity(&ir, "ModuleA", "TypeX");
	b_type = type_identity(&ir, "ModuleB", "TypeB");
	b_type_x = type_identity(&ir, "ModuleB", "TypeX");
	assert(root && a_type_x && b_type && b_type_x);
	assert(a_type_x != b_type_x && !strcmp(a_type_x->identity.source_name,
		b_type_x->identity.source_name));
	assert(!strcmp(root->fields[0].type.module, "ModuleA") &&
		!strcmp(root->fields[0].type.source_name, "TypeX"));
	assert(!strcmp(root->fields[1].type.module, "ModuleB") &&
		!strcmp(root->fields[1].type.source_name, "TypeB"));
	assert(b_type->field_count == 1 &&
		!strcmp(b_type->fields[0].type.module, "ModuleB") &&
		!strcmp(b_type->fields[0].type.source_name, "TypeX"));
	assert(a_type_x->kind == ASN1TYPED_TYPE_PRIMITIVE &&
		a_type_x->primitive_kind == ASN1TYPED_PRIMITIVE_INTEGER);
	assert(b_type_x->kind == ASN1TYPED_TYPE_PRIMITIVE &&
		b_type_x->primitive_kind == ASN1TYPED_PRIMITIVE_BOOLEAN);
	assert(ir.type_count == 8);
	asn1typed_module_clear(&ir);
	puts("T4 cross-module identity, context, transitive closure, and parser ownership: PASS");

	/* A failed external lookup after the first dependency was materialized
	 * must discard the whole partially built closure. */
	tree = asn1p_parse_file(T4_FIXTURE, A1P_NOFLAGS);
	assert(tree && asn1f_process(tree, A1F_NOFLAGS, NULL) >= 0);
	rename_module_b_type_for_identity_test(tree);
	{
		asn1p_module_t *module;
		asn1p_expr_t *decl;
		TQ_FOR(module, &tree->modules, mod_next)
			if(!strcmp(module->ModuleName, "ModuleB")) break;
		assert(module);
		TQ_FOR(decl, &module->members, next)
			if(decl->Identifier && !strcmp(decl->Identifier, "TypeB")) break;
		assert(decl);
		assert(TQ_REMOVE(&module->members, next) == decl);
		assert(asn1typed_extract_message(tree, "ModuleA", "Registration", &ir,
			error, sizeof(error)) == -1);
		TQ_ADD(&module->members, decl, next);
	}
	assert(error[0] && strstr(error, "ModuleB.TypeB"));
	assert(!ir.source_name && !ir.types && !ir.type_count);
	asn1typed_module_clear(&ir);
	asn1typed_module_clear(&ir);
	asn1p_delete(tree);
	puts("T4 cross-module missing declaration failure cleanup: PASS");

	tree = asn1p_parse_file(T4_FIXTURE, A1P_NOFLAGS);
	assert(tree && asn1f_process(tree, A1F_NOFLAGS, NULL) >= 0);
	rename_module_b_type_for_identity_test(tree);
	{
		asn1p_module_t *module;
		asn1p_expr_t *decl;
		asn1p_module_t *saved_module;
		asn1p_module_t missing_module;
		TQ_FOR(module, &tree->modules, mod_next)
			if(!strcmp(module->ModuleName, "ModuleB")) break;
		assert(module);
		TQ_FOR(decl, &module->members, next)
			if(decl->Identifier && !strcmp(decl->Identifier, "TypeB")) break;
		assert(decl);
		memset(&missing_module, 0, sizeof(missing_module));
		missing_module.ModuleName = "MissingModule";
		saved_module = decl->module;
		decl->module = &missing_module;
		assert(asn1typed_extract_message(tree, "ModuleA", "Registration", &ir,
			error, sizeof(error)) == -1);
		decl->module = saved_module;
	}
	assert(error[0] && strstr(error, "module 'MissingModule' not found"));
	assert(!ir.source_name && !ir.types && !ir.type_count);
	asn1typed_module_clear(&ir);
	asn1typed_module_clear(&ir);
	asn1p_delete(tree);
	puts("T4 cross-module missing module failure cleanup: PASS");
}
