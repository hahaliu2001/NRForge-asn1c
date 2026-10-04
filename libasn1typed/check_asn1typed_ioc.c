#include "asn1typed_extract.h"
#include <asn1fix.h>

#include <assert.h>
#include <stdio.h>
#include <string.h>

void check_asn1typed_ioc(void);

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
	printf("T3 negative %s: PASS (%s; IR cleared)\n", label, error);
}

static void
reject(asn1p_t *tree, const char *label, const char *expected) {
	reject_message(tree, "Registration", label, expected);
}

void
check_asn1typed_ioc(void) {
	asn1p_t *tree = fixed_fixture();
	asn1typed_module_t ir;
	asn1typed_type_t *message, *type;
	asn1p_expr_t *set, *container;
	asn1p_ioc_row_t *row;
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
