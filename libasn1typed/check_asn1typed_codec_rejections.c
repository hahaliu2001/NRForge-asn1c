#include "asn1typed_extract.h"
#include "asn1typed_render_cpp.h"
#include <asn1fix.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef S1_FIXTURE
#define S1_FIXTURE "fixtures/cpp-aper-s1.asn1"
#endif
#ifndef COLLISION_FIXTURE
#define COLLISION_FIXTURE "fixtures/cpp-aper-wrapper-collision.asn1"
#endif
#ifndef ERRNO_FIELD_FIXTURE
#define ERRNO_FIELD_FIXTURE "fixtures/s4-errno-field.asn1"
#endif
#ifndef ERRNO_ALTERNATIVE_FIXTURE
#define ERRNO_ALTERNATIVE_FIXTURE "fixtures/s4-errno-alternative.asn1"
#endif

static void
reject(const asn1typed_module_t *m, const char *ns, const char *message) {
	char diagnostic[512];
	char *out = (char *)1;
	assert(asn1typed_render_cpp_owned_aper_codec(m, ns, &out, diagnostic, sizeof(diagnostic)) == -1);
	assert(out == NULL && diagnostic[0] && strstr(diagnostic, message));
}

static char *
copy_text(const char *text) {
	char *copy = malloc(strlen(text) + 1);
	assert(copy);
	strcpy(copy, text);
	return copy;
}

static void
check_errno_spelling(const char *fixture, const char *module_name,
		const char *owned_spelling) {
	asn1p_t *tree = asn1p_parse_file(fixture, A1P_NOFLAGS);
	asn1typed_module_t module = {0};
	char diagnostic[512];
	char *types = NULL, *codec = (char *)1;
	assert(tree && asn1f_process(tree, A1F_NOFLAGS, NULL) >= 0);
	assert(asn1typed_extract_module(tree, module_name, &module, diagnostic, sizeof(diagnostic)) == 0);
	asn1p_delete(tree);
	assert(asn1typed_render_cpp_owned_slice(&module, "nrforge::errno_case", &types, diagnostic, sizeof(diagnostic)) == 0);
	assert(strstr(types, owned_spelling));
	assert(asn1typed_render_cpp_owned_aper_codec(&module, "nrforge::errno_case", &codec, diagnostic, sizeof(diagnostic)) == -1);
	assert(codec == NULL && strstr(diagnostic, "final C++ spelling conflicts with a standard header macro"));
	free(types);
	asn1typed_module_clear(&module);
}

int
main(void) {
	asn1p_t *tree = asn1p_parse_file(S1_FIXTURE, A1P_NOFLAGS);
	asn1typed_module_t module = {0};
	char diagnostic[512];
	char *out = NULL;
	assert(tree && asn1f_process(tree, A1F_NOFLAGS, NULL) >= 0);
	assert(asn1typed_extract_module(tree, "CppAperSlice", &module, diagnostic, sizeof(diagnostic)) == 0);
	asn1p_delete(tree);
	reject(NULL, "nrforge::test", "invalid arguments");
	reject(&module, NULL, "invalid arguments");
	assert(asn1typed_render_cpp_owned_aper_codec(&module, "nrforge::test", NULL, diagnostic, sizeof(diagnostic)) == -1);
	assert(diagnostic[0] && strstr(diagnostic, "invalid arguments"));
	reject(&module, "class", "invalid or reserved output namespace");
	{
		intmax_t upper = module.types[0].value_range.upper_bound;
		module.types[0].value_range.upper_bound = 255;
		reject(&module, "nrforge::test", "0..65535");
		module.types[0].value_range.upper_bound = upper;
	}
	{
		char *original = module.types[2].identity.source_name;
		module.types[2].identity.source_name = copy_text("count");
		reject(&module, "nrforge::test", "collision");
		free(module.types[2].identity.source_name);
		module.types[2].identity.source_name = original;
	}
	{
		char *original = module.types[2].fields[2].source_name;
		module.types[2].fields[2].source_name = copy_text("count");
		reject(&module, "nrforge::test", "SEQUENCE field spelling collision");
		free(module.types[2].fields[2].source_name);
		module.types[2].fields[2].source_name = original;
	}
	{
		char *original = module.types[2].fields[1].source_name;
		char *types = NULL;
		module.types[2].fields[1].source_name = copy_text("errno");
		assert(asn1typed_render_cpp_owned_slice(&module, "nrforge::test", &types, diagnostic, sizeof(diagnostic)) == 0);
		assert(strstr(types, "std::optional<bool> errno;"));
		free(types);
		reject(&module, "nrforge::test", "final C++ spelling conflicts with a standard header macro");
		free(module.types[2].fields[1].source_name);
		module.types[2].fields[1].source_name = original;
	}
	{
		char *original = module.types[1].alternatives[0].source_name;
		char *types = NULL;
		module.types[1].alternatives[0].source_name = copy_text("errno");
		assert(asn1typed_render_cpp_owned_slice(&module, "nrforge::test", &types, diagnostic, sizeof(diagnostic)) == 0);
		assert(strstr(types, "struct Selection_errno"));
		free(types);
		reject(&module, "nrforge::test", "final C++ spelling conflicts with a standard header macro");
		free(module.types[1].alternatives[0].source_name);
		module.types[1].alternatives[0].source_name = original;
	}
	{
		asn1typed_type_t *choice = &module.types[1];
		size_t saved = choice->alternatives[0].per_root_index;
		choice->alternatives[0].per_root_index = choice->alternatives[1].per_root_index;
		reject(&module, "nrforge::test", "duplicate PER root indexes");
		choice->alternatives[0].per_root_index = saved;
	}
	{
		asn1typed_type_t *choice = &module.types[1];
		asn1typed_choice_alternative_set_wire_unavailable(choice, 0);
		reject(&module, "nrforge::test", "alternative flag has unavailable or unsupported wire evidence");
		assert(asn1typed_choice_alternative_set_wire_evidence(choice, 0,
			ASN1TYPED_TAG_CLASS_CONTEXT_SPECIFIC, 0) == 0);
		assert(asn1typed_choice_alternative_set_wire_evidence(choice, 1,
			ASN1TYPED_TAG_CLASS_CONTEXT_SPECIFIC, 1) == 0);
		assert(asn1typed_choice_wire_evidence_finalize(choice, diagnostic, sizeof(diagnostic)) == ASN1TYPED_WIRE_FINALIZE_OK);
	}
	assert(asn1typed_render_cpp_owned_aper_codec(&module, "nrforge::test", &out, diagnostic, sizeof(diagnostic)) == 0);
	assert(out && out[0]); free(out);
	asn1typed_module_clear(&module);
	{
		asn1typed_module_t collision = {0};
		tree = asn1p_parse_file(COLLISION_FIXTURE, A1P_NOFLAGS);
		assert(tree && asn1f_process(tree, A1F_NOFLAGS, NULL) >= 0);
		assert(asn1typed_extract_module(tree, "CppAperWrapperCollision", &collision, diagnostic, sizeof(diagnostic)) == 0);
		asn1p_delete(tree);
		out = (char *)1;
		assert(asn1typed_render_cpp_owned_aper_codec(&collision, "nrforge::collision", &out, diagnostic, sizeof(diagnostic)) == -1);
		assert(out == NULL && strstr(diagnostic, "CHOICE wrapper"));
		asn1typed_module_clear(&collision);
	}
	check_errno_spelling(ERRNO_FIELD_FIXTURE, "S4ErrnoField", "bool errno{};");
	check_errno_spelling(ERRNO_ALTERNATIVE_FIXTURE, "S4ErrnoAlternative", "struct Selection_errno");
	puts("PASS S4 codec rejection, evidence propagation, and NULL argument checks");
	return 0;
}
