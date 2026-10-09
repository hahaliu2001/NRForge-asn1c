#include "asn1typed_extract.h"
#include "asn1typed_render_cpp.h"
#include <asn1fix.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef REVERSED_FIXTURE
#define REVERSED_FIXTURE "fixtures/cpp-aper-reversed-tags.asn1"
#endif
#ifndef COLLISION_FIXTURE
#define COLLISION_FIXTURE "fixtures/cpp-aper-wrapper-collision.asn1"
#endif
#ifndef ORDINAL_ZERO_FIXTURE
#define ORDINAL_ZERO_FIXTURE "fixtures/cpp-aper-ordinal-member-zero.asn1"
#endif
#ifndef ORDINAL_ONE_FIXTURE
#define ORDINAL_ONE_FIXTURE "fixtures/cpp-aper-ordinal-member-one.asn1"
#endif

void check_asn1typed_choice_wire_evidence(void);

int main(void) {
	asn1p_t *tree = asn1p_parse_file(S1_FIXTURE, A1P_NOFLAGS);
	asn1typed_module_t ir = {0}, wide_ir = {0}; char diag[256], *a = NULL, *b = NULL, *wide = NULL, *mapping = NULL, *mapping2 = NULL;
	static const char *const bad_ns[] = {
		"", "::a", "a::", "a:::b", "1x", "class", "__x", "NULL", "a::NULL",
		"std", "std::detail", "a::std", "x::std::y",
		"INT64_MAX", "a::INT64_MAX", "SIZE_MAX", "a::SIZE_MAX",
		"INT8_MIN", "UINT32_MAX", "INTMAX_MAX", "PTRDIFF_MIN", "WCHAR_MAX", "SIG_ATOMIC_MAX"
	};
	size_t i;
	assert(tree && asn1f_process(tree, A1F_NOFLAGS, NULL) >= 0);
	assert(asn1typed_extract_module(tree, "CppAperSlice", &ir, diag, sizeof(diag)) == 0);
	asn1p_delete(tree); tree = NULL;
	assert(ir.type_count == 3);
	assert(!strcmp(ir.types[0].identity.source_name, "Count"));
	assert(ir.types[0].value_range.has_value_range && !ir.types[0].value_range.is_extensible);
	assert(ir.types[0].value_range.lower_bound == 0 && ir.types[0].value_range.upper_bound == 65535);
	assert(ir.types[1].kind == ASN1TYPED_TYPE_CHOICE && ir.types[1].alternative_count == 2);
	assert(ir.types[2].kind == ASN1TYPED_TYPE_SEQUENCE && ir.types[2].field_count == 3);
	assert(ir.types[2].fields[0].presence == ASN1TYPED_PRESENCE_MANDATORY);
	assert(ir.types[2].fields[1].presence == ASN1TYPED_PRESENCE_OPTIONAL);
	assert(ir.types[2].fields[2].presence == ASN1TYPED_PRESENCE_MANDATORY);
	assert(asn1typed_render_cpp_owned_slice(&ir, "nrforge::synthetic::cpp_aper_slice", &a, diag, sizeof(diag)) == 0);
	assert(asn1typed_render_cpp_owned_slice(&ir, "nrforge::synthetic::cpp_aper_slice", &b, diag, sizeof(diag)) == 0);
	assert(!strcmp(a,b));
	free(b); b = NULL;
	assert(asn1typed_render_cpp_owned_aper_mapping(&ir, "nrforge::synthetic::cpp_aper_slice", &mapping, diag, sizeof(diag)) == 0);
	assert(asn1typed_render_cpp_owned_aper_mapping(&ir, "nrforge::synthetic::cpp_aper_slice", &mapping2, diag, sizeof(diag)) == 0);
	assert(!strcmp(mapping, mapping2)); free(mapping2); mapping2 = NULL;
	{
		char *rejected = NULL;
		size_t saved_index = ir.types[1].alternatives[0].per_root_index;
		ir.types[1].alternatives[0].per_root_index = 1;
		assert(asn1typed_render_cpp_owned_aper_mapping(&ir, "nrforge::synthetic::cpp_aper_slice", &rejected, diag, sizeof(diag)) == -1);
		assert(rejected == NULL && strstr(diag, "duplicate PER root indexes"));
		ir.types[1].alternatives[0].per_root_index = saved_index;
		asn1typed_choice_alternative_set_wire_unavailable(&ir.types[1], 0);
		rejected = NULL;
		assert(asn1typed_render_cpp_owned_aper_mapping(&ir, "nrforge::synthetic::cpp_aper_slice", &rejected, diag, sizeof(diag)) == -1);
		assert(rejected == NULL && strstr(diag, "alternative flag has unavailable or unsupported wire evidence"));
		assert(asn1typed_choice_alternative_set_wire_evidence(&ir.types[1], 0, ASN1TYPED_TAG_CLASS_CONTEXT_SPECIFIC, 0) == 0);
		assert(asn1typed_choice_alternative_set_wire_evidence(&ir.types[1], 1, ASN1TYPED_TAG_CLASS_CONTEXT_SPECIFIC, 1) == 0);
		assert(asn1typed_choice_wire_evidence_finalize(&ir.types[1], diag, sizeof(diag)) == ASN1TYPED_WIRE_FINALIZE_OK);
		ir.types[1].is_extensible = 1; rejected = NULL;
		assert(asn1typed_render_cpp_owned_aper_mapping(&ir, "nrforge::synthetic::cpp_aper_slice", &rejected, diag, sizeof(diag)) == -1);
		assert(rejected == NULL && strstr(diag, "extension"));
		ir.types[1].is_extensible = 0;
		ir.types[0].value_range.upper_bound = 255; rejected = NULL;
		assert(asn1typed_render_cpp_owned_aper_mapping(&ir, "nrforge::synthetic::cpp_aper_slice", &rejected, diag, sizeof(diag)) == -1);
		assert(rejected == NULL && strstr(diag, "0..65535"));
		ir.types[0].value_range.upper_bound = 65535;
		ir.types[2].fields[1].presence = ASN1TYPED_PRESENCE_CONDITIONAL; rejected = NULL;
		assert(asn1typed_render_cpp_owned_aper_mapping(&ir, "nrforge::synthetic::cpp_aper_slice", &rejected, diag, sizeof(diag)) == -1);
		assert(rejected == NULL && strstr(diag, "presence"));
		ir.types[2].fields[1].presence = ASN1TYPED_PRESENCE_OPTIONAL;
		ir.types[2].fields[0].type.actual_count = 1; rejected = NULL;
		assert(asn1typed_render_cpp_owned_aper_mapping(&ir, "nrforge::synthetic::cpp_aper_slice", &rejected, diag, sizeof(diag)) == -1);
		assert(rejected == NULL && strstr(diag, "parameterized"));
		ir.types[2].fields[0].type.actual_count = 0;
		rejected = NULL;
		assert(asn1typed_render_cpp_owned_aper_mapping(&ir, "class", &rejected, diag, sizeof(diag)) == -1);
		assert(rejected == NULL && strstr(diag, "namespace"));
	}
	for(i = 0; i < sizeof(bad_ns)/sizeof(bad_ns[0]); ++i) {
		assert(asn1typed_render_cpp_owned_slice(&ir, bad_ns[i], &b, diag, sizeof(diag)) == -1);
		assert(b == NULL && strstr(diag, "namespace"));
	}
	/* A collision after normalization is rejected. */
	free(ir.types[2].identity.source_name);
	ir.types[2].identity.source_name = strdup("count");
	b = NULL;
	assert(asn1typed_render_cpp_owned_slice(&ir, "nrforge::synthetic::cpp_aper_slice", &b, diag, sizeof(diag)) == -1 && b == NULL && diag[0]);
	free(ir.types[2].identity.source_name);
	ir.types[2].identity.source_name = strdup("Packet");
	/* Parameterized refs and bound-instance storage fail explicitly. */
	ir.types[2].fields[0].type.actual_count = 1;
	b = NULL;
	assert(asn1typed_render_cpp_owned_slice(&ir, "nrforge::synthetic::cpp_aper_slice", &b, diag, sizeof(diag)) == -1);
	assert(b == NULL && strstr(diag, "parameterized"));
	ir.types[2].fields[0].type.actual_count = 0;
	{
		asn1typed_type_actual_t actual = {0};
		ir.types[2].fields[0].type.actuals = &actual; b = NULL;
		assert(asn1typed_render_cpp_owned_slice(&ir, "nrforge::synthetic::cpp_aper_slice", &b, diag, sizeof(diag)) == -1);
		assert(b == NULL && strstr(diag, "parameterized"));
		ir.types[2].fields[0].type.actuals = NULL;
	}
	ir.bound_instance_count = 1; b = NULL;
	assert(asn1typed_render_cpp_owned_slice(&ir, "nrforge::synthetic::cpp_aper_slice", &b, diag, sizeof(diag)) == -1);
	assert(b == NULL && strstr(diag, "bound instances"));
	ir.bound_instance_count = 0;
	{
		asn1typed_bound_instance_t instance = {0};
		ir.bound_instances = &instance; b = NULL;
		assert(asn1typed_render_cpp_owned_slice(&ir, "nrforge::synthetic::cpp_aper_slice", &b, diag, sizeof(diag)) == -1);
		assert(b == NULL && strstr(diag, "bound instances"));
		ir.bound_instances = NULL;
	}
	/* Unsupported extension semantics fail closed. */
	ir.types[1].is_extensible = 1;
	b = NULL;
	assert(asn1typed_render_cpp_owned_slice(&ir, "nrforge::synthetic::cpp_aper_slice", &b, diag, sizeof(diag)) == -1);
	assert(strstr(diag, "extension") != NULL);
	ir.types[1].is_extensible = 0;
	/* Representative inconsistent IR states fail closed. */
	ir.types[0].size_constraint.is_extensible = 1; b = NULL;
	assert(asn1typed_render_cpp_owned_slice(&ir, "nrforge::synthetic::cpp_aper_slice", &b, diag, sizeof(diag)) == -1 && b == NULL);
	ir.types[0].size_constraint.is_extensible = 0;
	ir.types[2].fields[0].ioc.has_numeric_id = 1; b = NULL;
	assert(asn1typed_render_cpp_owned_slice(&ir, "nrforge::synthetic::cpp_aper_slice", &b, diag, sizeof(diag)) == -1 && b == NULL);
	assert(strstr(diag, "SEQUENCE field semantics") && !strstr(diag, "reference"));
	ir.types[2].fields[0].ioc.has_numeric_id = 0;
	ir.types[0].ioc_container.kind = ASN1TYPED_REF_PRIMITIVE; b = NULL;
	assert(asn1typed_render_cpp_owned_slice(&ir, "nrforge::synthetic::cpp_aper_slice", &b, diag, sizeof(diag)) == -1 && b == NULL);
	ir.types[0].ioc_container.kind = ASN1TYPED_REF_NAMED;
	ir.types[2].fields[0].inline_enumerated = &ir.types[1]; b = NULL;
	assert(asn1typed_render_cpp_owned_slice(&ir, "nrforge::synthetic::cpp_aper_slice", &b, diag, sizeof(diag)) == -1 && b == NULL);
	ir.types[2].fields[0].inline_enumerated = NULL;
	/* Cross-category type storage is rejected before any output is published. */
	ir.types[0].field_count = 1; b = NULL;
	assert(asn1typed_render_cpp_owned_slice(&ir, "nrforge::synthetic::cpp_aper_slice", &b, diag, sizeof(diag)) == -1 && b == NULL);
	ir.types[0].field_count = 0;
	ir.types[1].primitive_kind = ASN1TYPED_PRIMITIVE_BOOLEAN; b = NULL;
	assert(asn1typed_render_cpp_owned_slice(&ir, "nrforge::synthetic::cpp_aper_slice", &b, diag, sizeof(diag)) == -1 && b == NULL);
	ir.types[1].primitive_kind = ASN1TYPED_PRIMITIVE_INVALID;
	ir.types[1].enum_item_count = 1; b = NULL;
	assert(asn1typed_render_cpp_owned_slice(&ir, "nrforge::synthetic::cpp_aper_slice", &b, diag, sizeof(diag)) == -1 && b == NULL);
	ir.types[1].enum_item_count = 0;

	tree = asn1p_parse_file(S1_INT64_FIXTURE, A1P_NOFLAGS);
	assert(tree && asn1f_process(tree, A1F_NOFLAGS, NULL) >= 0);
	assert(asn1typed_extract_module(tree, "CppAperInt64", &wide_ir, diag, sizeof(diag)) == 0);
	asn1p_delete(tree);
	assert(wide_ir.type_count == 1 && wide_ir.types[0].value_range.has_value_range);
	assert(wide_ir.types[0].value_range.lower_bound == INT64_MIN);
	assert(wide_ir.types[0].value_range.upper_bound == INT64_MAX);
	assert(asn1typed_render_cpp_owned_slice(&wide_ir, "nrforge::synthetic::cpp_aper_int64", &wide, diag, sizeof(diag)) == 0);
	{
		char *rejected = NULL;
		assert(asn1typed_render_cpp_owned_aper_mapping(&wide_ir, "nrforge::synthetic::cpp_aper_int64", &rejected, diag, sizeof(diag)) == -1);
		assert(rejected == NULL && strstr(diag, "0..65535"));
	}
	check_asn1typed_choice_wire_evidence();
	{
		asn1typed_module_t collision = {0}; char *types = NULL, *wire = NULL;
		tree = asn1p_parse_file(COLLISION_FIXTURE, A1P_NOFLAGS);
		assert(tree && asn1f_process(tree, A1F_NOFLAGS, NULL) >= 0);
		assert(asn1typed_extract_module(tree, "CppAperWrapperCollision", &collision, diag, sizeof(diag)) == 0);
		asn1p_delete(tree); tree = NULL;
		assert(asn1typed_render_cpp_owned_slice(&collision, "nrforge::synthetic::collision", &types, diag, sizeof(diag)) == 0);
		assert(strstr(types, "struct Sel_aper"));
		assert(asn1typed_render_cpp_owned_aper_mapping(&collision, "nrforge::synthetic::collision", &wire, diag, sizeof(diag)) == -1);
		assert(wire == NULL && strstr(diag, "CHOICE wrapper"));
		free(types); asn1typed_module_clear(&collision);
	}
	{
		const char *const paths[] = {ORDINAL_ZERO_FIXTURE, ORDINAL_ONE_FIXTURE};
		const char *const modules[] = {"CppAperOrdinalMemberZero", "CppAperOrdinalMemberOne"};
		const char *const wrapper_names[] = {"Selection_storage_ordinal_1", "Selection_storage_ordinal_0"};
		for(i = 0; i < 2; ++i) {
			asn1typed_module_t reserved = {0}; char *types = NULL, *wire = NULL;
			tree = asn1p_parse_file(paths[i], A1P_NOFLAGS);
			assert(tree && asn1f_process(tree, A1F_NOFLAGS, NULL) >= 0);
			assert(asn1typed_extract_module(tree, modules[i], &reserved, diag, sizeof(diag)) == 0);
			asn1p_delete(tree); tree = NULL;
			assert(asn1typed_render_cpp_owned_slice(&reserved, "nrforge::synthetic::ordinal_reserved", &types, diag, sizeof(diag)) == 0);
			assert(strstr(types, wrapper_names[i]));
			assert(asn1typed_render_cpp_owned_aper_mapping(&reserved, "nrforge::synthetic::ordinal_reserved", &wire, diag, sizeof(diag)) == -1);
			assert(wire == NULL && strstr(diag, "reserved storage_ordinal_ member alias namespace"));
			free(types);
			asn1typed_module_clear(&reserved);
		}
	}
	{
		asn1typed_module_t reversed = {0}; char *types = NULL, *wire = NULL;
		tree = asn1p_parse_file(REVERSED_FIXTURE, A1P_NOFLAGS);
		assert(tree && asn1f_process(tree, A1F_NOFLAGS, NULL) >= 0);
		assert(asn1typed_extract_module(tree, "CppAperReversedTags", &reversed, diag, sizeof(diag)) == 0);
		asn1p_delete(tree); tree = NULL;
		assert(reversed.types[1].alternatives[0].per_root_index == 1);
		assert(reversed.types[1].alternatives[1].per_root_index == 0);
		assert(asn1typed_render_cpp_owned_slice(&reversed, "nrforge::synthetic::reversed", &types, diag, sizeof(diag)) == 0);
		assert(asn1typed_render_cpp_owned_aper_mapping(&reversed, "nrforge::synthetic::reversed", &wire, diag, sizeof(diag)) == 0);
		assert(strstr(types, "std::variant<Selection_count, Selection_flag>"));
		assert(strstr(wire, "storage_alternative_to_per_root_index[2] = {1, 0}"));
		fputs(types, stdout); fputs(wire, stdout);
		free(types); free(wire); asn1typed_module_clear(&reversed);
	}
	fputs(a, stdout); fputs(mapping, stdout); fputs(wide, stdout); free(a); free(b); free(wide); free(mapping); free(mapping2);
	asn1typed_module_clear(&ir); asn1typed_module_clear(&wide_ir);
	return 0;
}
