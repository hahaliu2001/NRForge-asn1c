#include "asn1typed.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

static void
check_class_field_semantics(void) {
	asn1typed_module_t module;
	asn1typed_type_t *sequence;
	asn1typed_type_ref_t fixed_ref = {0};
	asn1typed_class_field_relation_t relation = {0};
	asn1typed_field_t *fixed, *selected, *copy;
	char class_module[] = "TestModule";
	char class_name[] = "TestClass";
	char class_field[] = "criticality";
	char selector[] = "id";
	char *saved;

	assert(asn1typed_module_init(&module, "Fixture", "fixture.asn", 1) == 0);
	assert(asn1typed_module_add_type(&module, "Body", ASN1TYPED_TYPE_SEQUENCE,
		"fixture.asn", 2, &sequence) == 0);
	assert(asn1typed_type_ref_init(&fixed_ref, "TestModule", "Criticality") == 0);
	relation.class_module = class_module;
	relation.class_source_name = class_name;
	relation.class_field_source_name = class_field;
	relation.actual_index = 0;
	relation.has_selector = 1;
	relation.selector_source_name = selector;
	assert(asn1typed_type_add_class_field(sequence, "criticality",
		ASN1TYPED_FIELD_FIXED_TYPE, &fixed_ref, &relation,
		ASN1TYPED_PRESENCE_MANDATORY, "fixture.asn", 3) == 0);
	/* All constructor inputs are copied before the caller reuses them. */
	memset(class_module, 'x', sizeof(class_module) - 1);
	memset(class_name, 'x', sizeof(class_name) - 1);
	memset(class_field, 'x', sizeof(class_field) - 1);
	memset(selector, 'x', sizeof(selector) - 1);
	asn1typed_type_ref_clear(&fixed_ref);

	assert(asn1typed_type_add_class_field(sequence, "value",
		ASN1TYPED_FIELD_CLASS_FIELD_SELECTED_TYPE, NULL,
		&(asn1typed_class_field_relation_t){
			"TestModule", "TestClass", "Value", 0, 1, "id"
		}, ASN1TYPED_PRESENCE_MANDATORY, "fixture.asn", 4) == 0);
	assert(sequence->field_count == 2);
	fixed = &sequence->fields[0];
	selected = &sequence->fields[1];
	assert(fixed->type_semantics == ASN1TYPED_FIELD_FIXED_TYPE);
	assert(fixed->type.kind == ASN1TYPED_REF_NAMED);
	assert(!strcmp(fixed->type.module, "TestModule"));
	assert(!strcmp(fixed->type.source_name, "Criticality"));
	assert(fixed->has_class_field_relation);
	assert(!strcmp(fixed->class_field_relation.class_module, "TestModule"));
	assert(!strcmp(fixed->class_field_relation.class_source_name, "TestClass"));
	assert(!strcmp(fixed->class_field_relation.class_field_source_name,
		"criticality"));
	assert(fixed->class_field_relation.actual_index == 0);
	assert(fixed->class_field_relation.has_selector);
	assert(!strcmp(fixed->class_field_relation.selector_source_name, "id"));
	assert(selected->type_semantics == ASN1TYPED_FIELD_CLASS_FIELD_SELECTED_TYPE);
	assert(!selected->type.module && !selected->type.source_name &&
		!selected->type.actuals && !selected->type.actual_count);
	assert(selected->has_class_field_relation);
	assert(!strcmp(selected->class_field_relation.class_field_source_name, "Value"));
	assert(selected->class_field_relation.actual_index == 0);
	assert(selected->class_field_relation.has_selector);
	assert(!strcmp(selected->class_field_relation.selector_source_name, "id"));
	assert(fixed->location.line == 3);
	assert(asn1typed_field_set_ioc(fixed, "id-criticality",
		ASN1TYPED_CRITICALITY_REJECT, 1, 7) == 0);
	fixed->size_constraint.has_size_constraint = 1;
	fixed->size_constraint.lower_bound = 22;
	fixed->size_constraint.upper_bound = 32;

	/* Field copy owns an independent relation, location, type ref and IOC data. */
	assert(asn1typed_type_add_field_copy(sequence, fixed) == 0);
	assert(sequence->field_count == 3);
	fixed = &sequence->fields[0];
	selected = &sequence->fields[1];
	copy = &sequence->fields[2];
	assert(copy->type_semantics == fixed->type_semantics);
	assert(copy->presence == fixed->presence);
	assert(copy->location.line == fixed->location.line);
	assert(!strcmp(copy->location.file, fixed->location.file));
	assert(!strcmp(copy->type.source_name, fixed->type.source_name));
	assert(copy->type.source_name != fixed->type.source_name);
	assert(copy->class_field_relation.class_field_source_name !=
		fixed->class_field_relation.class_field_source_name);
	assert(copy->class_field_relation.selector_source_name !=
		fixed->class_field_relation.selector_source_name);
	assert(!strcmp(copy->ioc.symbolic_id, fixed->ioc.symbolic_id));
	assert(copy->ioc.symbolic_id != fixed->ioc.symbolic_id);
	assert(copy->size_constraint.has_size_constraint);
	assert(copy->size_constraint.lower_bound == 22);
	assert(copy->size_constraint.upper_bound == 32);
	assert(!copy->size_constraint.is_extensible);
	saved = fixed->class_field_relation.class_field_source_name;
	fixed->class_field_relation.class_field_source_name[0] = 'X';
	assert(!strcmp(copy->class_field_relation.class_field_source_name,
		"criticality"));
	fixed->class_field_relation.class_field_source_name[0] = saved[0];
	assert(asn1typed_type_add_field_copy(sequence, selected) == 0);
	assert(sequence->field_count == 4);
	selected = &sequence->fields[1];
	assert(sequence->fields[3].type_semantics ==
		ASN1TYPED_FIELD_CLASS_FIELD_SELECTED_TYPE);
	assert(!sequence->fields[3].type.module &&
		!sequence->fields[3].type.source_name);
	assert(sequence->fields[3].has_class_field_relation);
	assert(sequence->fields[3].class_field_relation.selector_source_name !=
		selected->class_field_relation.selector_source_name);
	/* id is an ordinary fixed-type field with no per-field relation. */
	assert(asn1typed_type_add_primitive_field(sequence, "id",
		ASN1TYPED_PRIMITIVE_INTEGER, ASN1TYPED_PRESENCE_MANDATORY,
		"fixture.asn", 5) == 0);
	assert(sequence->fields[4].type_semantics == ASN1TYPED_FIELD_FIXED_TYPE);
	assert(!sequence->fields[4].has_class_field_relation);
	assert(sequence->fields[4].type.kind == ASN1TYPED_REF_PRIMITIVE);
	assert(!sequence->fields[4].size_constraint.has_size_constraint);
	/* Ordinary field APIs retain fixed-type semantics and no relation. */
	assert(asn1typed_type_add_primitive_field(sequence, "ordinary",
		ASN1TYPED_PRIMITIVE_INTEGER, ASN1TYPED_PRESENCE_MANDATORY,
		"fixture.asn", 6) == 0);
	assert(sequence->fields[5].type_semantics == ASN1TYPED_FIELD_FIXED_TYPE);
	assert(!sequence->fields[5].has_class_field_relation);
	{
		asn1typed_type_ref_t key_ref = {0};
		asn1typed_class_field_relation_t key_relation = {
			"TestModule", "TestClass", "key", 0, 0, NULL
		};
	assert(asn1typed_type_ref_init_primitive(&key_ref,
			ASN1TYPED_PRIMITIVE_INTEGER) == 0);
	assert(asn1typed_type_add_class_field(sequence, "key",
			ASN1TYPED_FIELD_FIXED_TYPE, &key_ref, &key_relation,
			ASN1TYPED_PRESENCE_MANDATORY, "fixture.asn", 7) == 0);
	assert(!sequence->fields[6].class_field_relation.has_selector);
	assert(sequence->fields[6].class_field_relation.selector_source_name == NULL);
	asn1typed_type_ref_clear(&key_ref);
	}

	/* Malformed relation and semantic combinations fail before publication. */
	{
		asn1typed_class_field_relation_t bad = {
			NULL, "TestClass", "Value", 0, 0, NULL
		};
		assert(asn1typed_type_add_class_field(sequence, "bad-class",
			ASN1TYPED_FIELD_CLASS_FIELD_SELECTED_TYPE, NULL, &bad,
			ASN1TYPED_PRESENCE_MANDATORY, "fixture.asn", 6) == -1);
		bad.class_module = "TestModule";
		bad.class_source_name = NULL;
		assert(asn1typed_type_add_class_field(sequence, "bad-class-name",
			ASN1TYPED_FIELD_CLASS_FIELD_SELECTED_TYPE, NULL, &bad,
			ASN1TYPED_PRESENCE_MANDATORY, "fixture.asn", 6) == -1);
		bad.class_source_name = "TestClass";
		bad.class_field_source_name = NULL;
		assert(asn1typed_type_add_class_field(sequence, "bad-class-field",
			ASN1TYPED_FIELD_CLASS_FIELD_SELECTED_TYPE, NULL, &bad,
			ASN1TYPED_PRESENCE_MANDATORY, "fixture.asn", 6) == -1);
		bad.class_field_source_name = "Value";
		bad.has_selector = 1;
		bad.selector_source_name = NULL;
		assert(asn1typed_type_add_class_field(sequence, "bad-selector",
			ASN1TYPED_FIELD_CLASS_FIELD_SELECTED_TYPE, NULL, &bad,
			ASN1TYPED_PRESENCE_MANDATORY, "fixture.asn", 6) == -1);
		bad.has_selector = 0;
		bad.selector_source_name = NULL;
		assert(asn1typed_type_add_class_field(sequence, "bad-selected-ref",
			ASN1TYPED_FIELD_CLASS_FIELD_SELECTED_TYPE, &sequence->fields[5].type,
			&bad, ASN1TYPED_PRESENCE_MANDATORY, "fixture.asn", 6) == -1);
		assert(asn1typed_type_add_class_field(sequence, "missing-fixed-ref",
			ASN1TYPED_FIELD_FIXED_TYPE, NULL, &bad,
			ASN1TYPED_PRESENCE_MANDATORY, "fixture.asn", 6) == -1);
		bad.has_selector = 2;
		bad.selector_source_name = NULL;
		assert(asn1typed_type_add_class_field(sequence, "invalid-selector-state",
			ASN1TYPED_FIELD_CLASS_FIELD_SELECTED_TYPE, NULL, &bad,
			ASN1TYPED_PRESENCE_MANDATORY, "fixture.asn", 6) == -1);
		assert(sequence->field_count == 7);
	}
	asn1typed_module_clear(&module);
	asn1typed_module_clear(&module);
}

static void
check_choice_alternative_size_api(void) {
	asn1typed_module_t module;
	asn1typed_type_t *choice;
	asn1typed_type_ref_t bit_string = {0};
	asn1typed_type_ref_t integer = {0};
	asn1typed_size_constraint_t size = {1, 22, 32, 0};
	asn1typed_size_constraint_t invalid = {0, 22, 32, 0};
	asn1typed_integer_value_range_t value_range = {1, 1, 10, 1, NULL, 0, NULL, 0, 0, 0, 0};
	asn1typed_integer_value_range_t invalid_range = {0};
	asn1typed_integer_interval_t tail[] = {{20, 30}};
	asn1typed_integer_interval_t additions[] = {{40, 40}, {45, 45}};
	assert(asn1typed_module_init(&module, "ChoiceFixture", "choice.asn", 1) == 0);
	assert(asn1typed_module_add_type(&module, "C", ASN1TYPED_TYPE_CHOICE,
		"choice.asn", 2, &choice) == 0);
	assert(asn1typed_type_ref_init_primitive(&bit_string,
		ASN1TYPED_PRIMITIVE_BIT_STRING) == 0);
	assert(asn1typed_type_ref_init_primitive(&integer,
		ASN1TYPED_PRIMITIVE_INTEGER) == 0);
	assert(asn1typed_type_add_choice_alternative(choice, "bad", &bit_string,
		&invalid, NULL, "choice.asn", 3) == -1);
	assert(choice->alternative_count == 0);
	assert(asn1typed_type_add_choice_alternative(choice, "sized", &bit_string,
		&size, NULL, "choice.asn", 4) == 0);
	assert(asn1typed_type_add_choice_alternative(choice, "wrong-primitive",
		&bit_string, NULL, &value_range, "choice.asn", 5) == -1);
	assert(asn1typed_type_add_choice_alternative(choice, "empty-range",
		&integer, NULL, &invalid_range, "choice.asn", 6) == -1);
	assert(asn1typed_type_add_choice_alternative(choice, "both-constraints",
		&integer, &size, &value_range, "choice.asn", 7) == -1);
	size.lower_bound = 1;
	assert(choice->alternative_count == 1);
	assert(choice->alternatives[0].size_constraint.lower_bound == 22);
	assert(choice->alternatives[0].size_constraint.upper_bound == 32);
	assert(!choice->alternatives[0].size_constraint.is_extensible);
	assert(choice->alternatives[0].location.line == 4);
	{
		asn1typed_type_t *integer_choice;
		assert(asn1typed_module_add_type(&module, "IntegerC",
			ASN1TYPED_TYPE_CHOICE, "choice.asn", 8, &integer_choice) == 0);
		value_range.tail = tail;
		value_range.tail_count = 1;
		value_range.extension_additions = additions; value_range.extension_addition_count = 2;
		assert(asn1typed_type_add_choice_alternative(integer_choice, "ranged",
			&integer, NULL, &value_range, "choice.asn", 9) == 0);
		additions[0].upper_bound = 41;
		assert(integer_choice->alternatives[0].value_range.extension_additions != additions);
		assert(integer_choice->alternatives[0].value_range.extension_additions[0].upper_bound == 40);
		tail[0].upper_bound = 31;
		value_range.upper_bound = 11;
		assert(integer_choice->alternatives[0].value_range.upper_bound == 10);
		assert(integer_choice->alternatives[0].value_range.tail != tail);
		assert(integer_choice->alternatives[0].value_range.tail[0].upper_bound == 30);
		value_range.tail = NULL;
		value_range.tail_count = 0;
	}
	asn1typed_type_ref_clear(&bit_string);
	asn1typed_type_ref_clear(&integer);
	asn1typed_module_clear(&module);
	asn1typed_module_clear(&module);
}

int
main(void) {
	char module_name[] = "Example-Module";
	char type_name[] = "Person";
	char field_name[] = "name";
	asn1typed_module_t module;
	asn1typed_type_t *person, *list, *paging;
	const char *enum_names[] = { "v32", "v64", "v128", "v256" };
	size_t i;

	assert(asn1typed_module_init(&module, module_name, "sample.asn", 1) == 0);
	check_class_field_semantics();
	check_choice_alternative_size_api();
	memset(module_name, 'x', sizeof(module_name) - 1);
	assert(strcmp(module.source_name, "Example-Module") == 0);
	assert(strcmp(module.location.file, "sample.asn") == 0);

	assert(asn1typed_module_add_type(&module, type_name,
		ASN1TYPED_TYPE_SEQUENCE, "sample.asn", 3, &person) == 0);
	memset(type_name, 'x', sizeof(type_name) - 1);
	assert(strcmp(person->identity.module, "Example-Module") == 0);
	assert(strcmp(person->identity.source_name, "Person") == 0);
	assert(person->kind == ASN1TYPED_TYPE_SEQUENCE);

	assert(asn1typed_type_add_field(person, field_name, "Universal",
		"UTF8String", ASN1TYPED_PRESENCE_MANDATORY, "sample.asn", 4) == 0);
	memset(field_name, 'x', sizeof(field_name) - 1);
	assert(asn1typed_type_add_field(person, "age", "Universal", "INTEGER",
		ASN1TYPED_PRESENCE_OPTIONAL, "sample.asn", 5) == 0);
	assert(asn1typed_type_add_field(person, "note", "Universal", "UTF8String",
		ASN1TYPED_PRESENCE_CONDITIONAL, "sample.asn", 6) == 0);
	assert(person->field_count == 3);
	assert(strcmp(person->fields[0].source_name, "name") == 0);
	assert(strcmp(person->fields[0].type.source_name, "UTF8String") == 0);
	assert(person->fields[0].presence == ASN1TYPED_PRESENCE_MANDATORY);
	assert(strcmp(person->fields[1].source_name, "age") == 0);
	assert(strcmp(person->fields[1].type.source_name, "INTEGER") == 0);
	assert(person->fields[1].presence == ASN1TYPED_PRESENCE_OPTIONAL);
	assert(person->fields[2].presence == ASN1TYPED_PRESENCE_CONDITIONAL);
	assert(strcmp(person->fields[0].location.file, "sample.asn") == 0);
	assert(person->fields[0].location.line == 4);

	assert(asn1typed_module_add_type(&module, "SupportedTAList",
		ASN1TYPED_TYPE_SEQUENCE_OF, "sample.asn", 10, &list) == 0);
	assert(asn1typed_type_set_element_type(list, "NGAP-PDU-Contents",
		"SupportedTAItem") == 0);
	assert(list->kind == ASN1TYPED_TYPE_SEQUENCE_OF);
	assert(strcmp(list->element_type.module, "NGAP-PDU-Contents") == 0);
	assert(strcmp(list->element_type.source_name, "SupportedTAItem") == 0);

	assert(asn1typed_module_add_type(&module, "PagingDRX",
		ASN1TYPED_TYPE_ENUMERATED, "sample.asn", 20, &paging) == 0);
	for(i = 0; i < sizeof(enum_names) / sizeof(enum_names[0]); ++i)
		assert(asn1typed_type_add_enum_item(paging, enum_names[i],
			"sample.asn", (unsigned)(21 + i)) == 0);
	assert(paging->kind == ASN1TYPED_TYPE_ENUMERATED);
	assert(paging->enum_item_count == 4);
	for(i = 0; i < paging->enum_item_count; ++i)
		assert(strcmp(paging->enum_items[i].source_name, enum_names[i]) == 0);

	asn1typed_module_clear(&module);
	assert(module.source_name == NULL && module.types == NULL);
	return 0;
}
