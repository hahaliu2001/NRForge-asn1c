#include "asn1typed_extract.h"
#include <asn1fix.h>

#include <assert.h>
#include <stdio.h>
#include <string.h>

#ifndef AUTO_FIXTURE
#ifdef S1_FIXTURE
#define AUTO_FIXTURE S1_FIXTURE
#else
#define AUTO_FIXTURE "fixtures/cpp-aper-s1.asn1"
#endif
#endif
#ifndef TAG_FIXTURE
#define TAG_FIXTURE "fixtures/choice-wire-evidence.asn1"
#endif

void check_asn1typed_choice_wire_evidence(void);

static asn1typed_type_t *
find_type(asn1typed_module_t *module, const char *name) {
	size_t i;
	for(i = 0; i < module->type_count; ++i)
		if(!strcmp(module->types[i].identity.source_name, name))
			return &module->types[i];
	return NULL;
}

static void
check_auto_tags_after_tree_destroy(void) {
	asn1typed_module_t ir = {0};
	asn1p_t *tree = asn1p_parse_file(AUTO_FIXTURE, A1P_NOFLAGS);
	asn1typed_type_t *choice;
	char error[256] = {0};
	assert(tree && asn1f_process(tree, A1F_NOFLAGS, NULL) >= 0);
	assert(asn1typed_extract_module(tree, "CppAperSlice", &ir,
		error, sizeof(error)) == 0);
	asn1p_delete(tree);
	assert(ir.tag_default == ASN1TYPED_TAG_DEFAULT_AUTOMATIC);
	choice = find_type(&ir, "Selection");
	assert(choice && choice->alternative_count == 2);
	assert(!strcmp(choice->alternatives[0].source_name, "flag"));
	assert(choice->alternatives[0].wire_evidence == ASN1TYPED_WIRE_EVIDENCE_RESOLVED);
	assert(choice->alternatives[0].effective_tag_class == ASN1TYPED_TAG_CLASS_CONTEXT_SPECIFIC);
	assert(choice->alternatives[0].effective_tag_number == 0);
	assert(choice->alternatives[0].has_per_root_index && choice->alternatives[0].per_root_index == 0);
	assert(!strcmp(choice->alternatives[1].source_name, "count"));
	assert(choice->alternatives[1].wire_evidence == ASN1TYPED_WIRE_EVIDENCE_RESOLVED);
	assert(choice->alternatives[1].effective_tag_class == ASN1TYPED_TAG_CLASS_CONTEXT_SPECIFIC);
	assert(choice->alternatives[1].effective_tag_number == 1);
	assert(choice->alternatives[1].has_per_root_index && choice->alternatives[1].per_root_index == 1);
	assert(asn1typed_choice_wire_evidence_validate(choice, error, sizeof(error)) == 0);
	asn1typed_module_clear(&ir);
}

static void
check_explicit_tags_and_classes_after_tree_destroy(void) {
	asn1typed_module_t ir = {0};
	asn1p_t *tree = asn1p_parse_file(TAG_FIXTURE, A1P_NOFLAGS);
	asn1typed_type_t *choice;
	char error[256] = {0};
	assert(tree && asn1f_process(tree, A1F_NOFLAGS, NULL) >= 0);
	assert(asn1typed_extract_module(tree, "ChoiceWireEvidence", &ir,
		error, sizeof(error)) == 0);
	asn1p_delete(tree);
	assert(ir.tag_default == ASN1TYPED_TAG_DEFAULT_EXPLICIT);
	choice = find_type(&ir, "TagOrder");
	assert(choice && choice->alternative_count == 3);
	assert(!strcmp(choice->alternatives[0].source_name, "privateFirst"));
	assert(choice->alternatives[0].type_ref.kind == ASN1TYPED_REF_NAMED);
	assert(choice->alternatives[0].effective_tag_class == ASN1TYPED_TAG_CLASS_PRIVATE);
	assert(choice->alternatives[0].effective_tag_number == 1);
	assert(choice->alternatives[0].per_root_index == 2);
	assert(!strcmp(choice->alternatives[1].source_name, "contextSecond"));
	assert(choice->alternatives[1].effective_tag_class == ASN1TYPED_TAG_CLASS_CONTEXT_SPECIFIC);
	assert(choice->alternatives[1].effective_tag_number == 0);
	assert(choice->alternatives[1].per_root_index == 1);
	assert(!strcmp(choice->alternatives[2].source_name, "applicationLast"));
	assert(choice->alternatives[2].effective_tag_class == ASN1TYPED_TAG_CLASS_APPLICATION);
	assert(choice->alternatives[2].effective_tag_number == 7);
	assert(choice->alternatives[2].per_root_index == 0);
	assert(asn1typed_choice_wire_evidence_validate(choice, error, sizeof(error)) == 0);
	choice = find_type(&ir, "NumericOrder");
	assert(choice && choice->alternative_count == 3);
	assert(!strcmp(choice->alternatives[0].source_name, "nine"));
	assert(choice->alternatives[0].per_root_index == 2);
	assert(!strcmp(choice->alternatives[1].source_name, "two"));
	assert(choice->alternatives[1].per_root_index == 0);
	assert(!strcmp(choice->alternatives[2].source_name, "five"));
	assert(choice->alternatives[2].per_root_index == 1);
	assert(asn1typed_choice_wire_evidence_validate(choice, error, sizeof(error)) == 0);
	asn1typed_module_clear(&ir);
}

static void
check_partial_extraction_keeps_no_indexes(void) {
	static const char schema[] =
		"PartialWire DEFINITIONS EXPLICIT TAGS ::= BEGIN\n"
		"Partial ::= CHOICE { tagged [2] BOOLEAN, untagged BOOLEAN }\n"
		"END\n";
	asn1typed_module_t ir = {0};
	asn1p_t *tree = asn1p_parse_buffer(schema, -1, "partial.asn1", 1,
		A1P_NOFLAGS);
	asn1typed_type_t *choice;
	char error[256] = {0};
	assert(tree && asn1f_process(tree, A1F_NOFLAGS, NULL) >= 0);
	assert(asn1typed_extract_module(tree, "PartialWire", &ir,
		error, sizeof(error)) == 0);
	asn1p_delete(tree);
	assert(error[0] == '\0');
	choice = find_type(&ir, "Partial");
	assert(choice && choice->alternative_count == 2);
	assert(choice->alternatives[0].wire_evidence == ASN1TYPED_WIRE_EVIDENCE_RESOLVED);
	assert(choice->alternatives[1].wire_evidence == ASN1TYPED_WIRE_EVIDENCE_UNAVAILABLE);
	assert(!choice->has_valid_per_root_mapping);
	assert(!choice->alternatives[0].has_per_root_index);
	assert(!choice->alternatives[1].has_per_root_index);
	assert(asn1typed_choice_wire_evidence_validate(choice, error, sizeof(error)) == -1);
	assert(strstr(error, "unavailable or unsupported") != NULL);
	assert(asn1typed_choice_wire_evidence_finalize(choice, error, sizeof(error)) ==
		ASN1TYPED_WIRE_FINALIZE_UNAVAILABLE);
	assert(strstr(error, "unavailable or unsupported") != NULL);
	assert(!choice->has_valid_per_root_mapping);
	assert(!choice->alternatives[0].has_per_root_index);
	assert(!choice->alternatives[1].has_per_root_index);
	asn1typed_module_clear(&ir);
}

static void
check_extension_choice_rejected_by_extractor(void) {
	static const char schema[] =
		"ExtendedWire DEFINITIONS AUTOMATIC TAGS ::= BEGIN\n"
		"Extended ::= CHOICE { first BOOLEAN, ..., later INTEGER }\n"
		"END\n";
	asn1typed_module_t ir = {0};
	asn1p_t *tree = asn1p_parse_buffer(schema, -1, "extended.asn1", 1,
		A1P_NOFLAGS);
	char error[256] = {0};
	assert(tree && asn1f_process(tree, A1F_NOFLAGS, NULL) >= 0);
	assert(asn1typed_extract_module(tree, "ExtendedWire", &ir,
		error, sizeof(error)) == -1);
	assert(strstr(error, "CHOICE extension marker/additions are unsupported") != NULL);
	assert(!ir.types && ir.type_count == 0);
	asn1p_delete(tree);
	asn1typed_module_clear(&ir);
}

static void
check_invalid_manual_evidence(void) {
	asn1typed_module_t ir = {0};
	asn1typed_type_t *choice;
	asn1typed_type_ref_t boolean_ref = {0};
	char error[256] = {0};
	assert(asn1typed_module_init(&ir, "Manual", "manual.asn1", 1) == 0);
	assert(asn1typed_module_add_type(&ir, "Choice", ASN1TYPED_TYPE_CHOICE,
		"manual.asn1", 2, &choice) == 0);
	assert(asn1typed_type_ref_init_primitive(&boolean_ref,
		ASN1TYPED_PRIMITIVE_BOOLEAN) == 0);
	assert(asn1typed_type_add_choice_alternative(choice, "a", &boolean_ref,
		NULL, NULL, "manual.asn1", 3) == 0);
	assert(asn1typed_type_add_choice_alternative(choice, "b", &boolean_ref,
		NULL, NULL, "manual.asn1", 4) == 0);
	assert(asn1typed_choice_wire_evidence_finalize(choice, error, sizeof(error)) ==
		ASN1TYPED_WIRE_FINALIZE_UNAVAILABLE);
	assert(strstr(error, "unavailable") != NULL);
	assert(asn1typed_choice_alternative_set_wire_evidence(choice, 0,
		ASN1TYPED_TAG_CLASS_CONTEXT_SPECIFIC, 0) == 0);
	assert(asn1typed_choice_alternative_set_wire_evidence(choice, 1,
		ASN1TYPED_TAG_CLASS_CONTEXT_SPECIFIC, 0) == 0);
	assert(asn1typed_choice_wire_evidence_finalize(choice, error, sizeof(error)) ==
		ASN1TYPED_WIRE_FINALIZE_UNAVAILABLE);
	assert(strstr(error, "duplicate effective tags") != NULL);
	assert(!choice->has_valid_per_root_mapping);
	assert(!choice->alternatives[0].has_per_root_index &&
		!choice->alternatives[1].has_per_root_index);
	assert(asn1typed_choice_alternative_set_wire_evidence(choice, 1,
		ASN1TYPED_TAG_CLASS_PRIVATE, 0) == 0);
	assert(asn1typed_choice_wire_evidence_finalize(choice, error, sizeof(error)) ==
		ASN1TYPED_WIRE_FINALIZE_OK);
	assert(choice->has_valid_per_root_mapping);
	assert(choice->alternatives[0].has_per_root_index &&
		choice->alternatives[0].per_root_index == 0);
	assert(choice->alternatives[1].has_per_root_index &&
		choice->alternatives[1].per_root_index == 1);
	choice->alternatives[0].per_root_index = 1;
	assert(asn1typed_choice_wire_evidence_validate(choice, error, sizeof(error)) == -1);
	assert(strstr(error, "duplicate PER root indexes") != NULL);
	choice->alternatives[0].per_root_index = 0;
	assert(asn1typed_choice_wire_evidence_validate(choice, error, sizeof(error)) == 0);
	assert(asn1typed_choice_wire_evidence_finalize(choice, error, sizeof(error)) ==
		ASN1TYPED_WIRE_FINALIZE_OK);
	assert(asn1typed_choice_alternative_set_wire_evidence(choice, 0,
		ASN1TYPED_TAG_CLASS_PRIVATE, 3) == 0);
	assert(!choice->has_valid_per_root_mapping);
	assert(!choice->alternatives[0].has_per_root_index &&
		!choice->alternatives[1].has_per_root_index);
	assert(asn1typed_choice_wire_evidence_finalize(choice, error, sizeof(error)) ==
		ASN1TYPED_WIRE_FINALIZE_OK);
	assert(choice->alternatives[0].per_root_index == 1 &&
		choice->alternatives[1].per_root_index == 0);
	assert(asn1typed_type_add_choice_alternative(choice, "c", &boolean_ref,
		NULL, NULL, "manual.asn1", 5) == 0);
	assert(!choice->has_valid_per_root_mapping);
	assert(!choice->alternatives[0].has_per_root_index &&
		!choice->alternatives[1].has_per_root_index &&
		!choice->alternatives[2].has_per_root_index);
	assert(asn1typed_choice_wire_evidence_finalize(choice, error, sizeof(error)) ==
		ASN1TYPED_WIRE_FINALIZE_UNAVAILABLE);
	assert(strstr(error, "unavailable or unsupported") != NULL);
	assert(!choice->has_valid_per_root_mapping);
	asn1typed_choice_alternative_set_wire_unsupported(choice, 2);
	assert(!choice->has_valid_per_root_mapping);
	assert(!choice->alternatives[0].has_per_root_index &&
		!choice->alternatives[1].has_per_root_index &&
		!choice->alternatives[2].has_per_root_index);
	assert(asn1typed_choice_wire_evidence_finalize(choice, error, sizeof(error)) ==
		ASN1TYPED_WIRE_FINALIZE_UNAVAILABLE);
	assert(!choice->has_valid_per_root_mapping);
	choice->is_extensible = 1;
	assert(asn1typed_choice_wire_evidence_finalize(choice, error, sizeof(error)) ==
		ASN1TYPED_WIRE_FINALIZE_UNAVAILABLE);
	assert(strstr(error, "extensible CHOICE") != NULL);
	assert(!choice->has_valid_per_root_mapping);
	asn1typed_type_ref_clear(&boolean_ref);
	asn1typed_module_clear(&ir);
}

void
check_asn1typed_choice_wire_evidence(void) {
	check_auto_tags_after_tree_destroy();
	check_explicit_tags_and_classes_after_tree_destroy();
	check_partial_extraction_keeps_no_indexes();
	check_extension_choice_rejected_by_extractor();
	check_invalid_manual_evidence();
}
