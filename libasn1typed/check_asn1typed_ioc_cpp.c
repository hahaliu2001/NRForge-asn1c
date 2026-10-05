#include "asn1typed_extract.h"
#include "asn1typed_render_cpp.h"
#include <asn1fix.h>

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void
inspect_owned_ir(const asn1typed_module_t *ir) {
	static const char *const order[] = {
		"Registration", "NodeNumber", "LabelText", "OperatingMode", "ItemCollection", "Item"
	};
	static const char *const fields[] = { "NodeID", "NodeName", "Mode", "Items" };
	static const char *const symbols[] = { "id-NodeID", "id-NodeName", "id-Mode", "id-Items" };
	static const asn1typed_presence_e presence[] = {
		ASN1TYPED_PRESENCE_MANDATORY, ASN1TYPED_PRESENCE_OPTIONAL,
		ASN1TYPED_PRESENCE_CONDITIONAL, ASN1TYPED_PRESENCE_MANDATORY
	};
	static const asn1typed_criticality_e criticality[] = {
		ASN1TYPED_CRITICALITY_REJECT, ASN1TYPED_CRITICALITY_IGNORE,
		ASN1TYPED_CRITICALITY_NOTIFY, ASN1TYPED_CRITICALITY_REJECT
	};
	size_t i;
	assert(!strcmp(ir->source_name, "SyntheticIOC"));
	assert(ir->type_count == 6);
	for(i = 0; i < 6; ++i) {
		assert(!strcmp(ir->types[i].identity.source_name, order[i]));
		assert(!strcmp(ir->types[i].identity.module, "SyntheticIOC"));
	}
	assert(ir->types[0].field_count == 4);
	for(i = 0; i < 4; ++i) {
		const asn1typed_field_t *f = &ir->types[0].fields[i];
		assert(!strcmp(f->source_name, fields[i]));
		assert(f->presence == presence[i]);
		assert(f->type.kind == ASN1TYPED_REF_NAMED);
		assert(!strcmp(f->type.module, "SyntheticIOC"));
		assert(!strcmp(f->type.source_name, order[i + 1]));
		assert(!strcmp(f->ioc.symbolic_id, symbols[i]));
		assert(f->ioc.has_numeric_id && f->ioc.numeric_id == (intmax_t)(11 * (i + 1)));
		assert(f->ioc.criticality == criticality[i]);
	}
	assert(ir->types[0].fields[2].presence == ASN1TYPED_PRESENCE_CONDITIONAL);
}

int
main(void) {
	static const char expected[] =
		"#include <cstdint>\n"
		"#include <optional>\n"
		"#include <string>\n"
		"#include <vector>\n\n"
		"using NodeNumber = std::int64_t;\n\n"
		"using LabelText = std::string;\n\n"
		"enum class OperatingMode {\n"
		"    active,\n    standby,\n};\n\n"
		"struct Item {\n"
		"    std::int64_t count;\n"
		"    std::optional<LabelText> label;\n};\n\n"
		"using ItemCollection = std::vector<Item>;\n\n"
		"struct Registration {\n"
		"    NodeNumber node_id;\n"
		"    std::optional<LabelText> node_name;\n"
		"    std::optional<OperatingMode> mode;\n"
		"    ItemCollection items;\n};\n";
	asn1p_t *tree = asn1p_parse_file(T3_FIXTURE, A1P_NOFLAGS);
	asn1typed_module_t ir = {0};
	char diagnostic[256], *text = NULL, *again = NULL;
	assert(tree);
	assert(asn1f_process(tree, A1F_NOFLAGS, NULL) >= 0);
	assert(asn1typed_extract_message(tree, "SyntheticIOC", "Registration",
		&ir, diagnostic, sizeof(diagnostic)) == 0);
	/* Ownership boundary: no parser/fixer tree survives into inspection/rendering. */
	asn1p_delete(tree);
	tree = NULL;
	inspect_owned_ir(&ir);
	fprintf(stderr, "PASS owned IOC IR after tree destruction: source order, presence and metadata\n");
	assert(asn1typed_render_cpp(&ir, &text, diagnostic, sizeof(diagnostic)) == 0);
	assert(text && !diagnostic[0] && !strcmp(text, expected));
	inspect_owned_ir(&ir);
	assert(asn1typed_render_cpp(&ir, &again, diagnostic, sizeof(diagnostic)) == 0);
	assert(again && !diagnostic[0] && text != again && !strcmp(text, again));
	inspect_owned_ir(&ir);
	fprintf(stderr, "PASS complete production IOC C++ golden and deterministic repeat\n");
	/* stdout contains only actual production renderer output for compilation. */
	fputs(text, stdout);
	free(text); free(again);
	asn1typed_module_clear(&ir);
	return 0;
}
