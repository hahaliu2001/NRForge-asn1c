#include "asn1typed_name.h"
#include "asn1typed.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void
check(asn1typed_name_style_e style, const char *source, const char *expected) {
	char *first = NULL, *second = NULL;
	assert(asn1typed_name_make(source, style, &first) == ASN1TYPED_NAME_OK);
	assert(asn1typed_name_make(source, style, &second) == ASN1TYPED_NAME_OK);
	assert(first != second);
	assert(!strcmp(first, expected));
	assert(!strcmp(first, second));
	printf("style %d: %s -> %s (repeat identical)\n", (int)style, source, first);
	free(first);
	free(second);
}

static void
check_collision(const char *a, const char *b, asn1typed_name_style_e style) {
	asn1typed_name_scope_t scope = {0};
	char *first = NULL, *second = NULL;
	assert(asn1typed_name_make(a, style, &first) == ASN1TYPED_NAME_OK);
	assert(asn1typed_name_make(b, style, &second) == ASN1TYPED_NAME_OK);
	assert(!strcmp(first, second));
	assert(asn1typed_name_scope_add(&scope, a, first) == ASN1TYPED_NAME_OK);
	assert(asn1typed_name_scope_add(&scope, a, first) == ASN1TYPED_NAME_OK);
	assert(asn1typed_name_scope_add(&scope, b, second) == ASN1TYPED_NAME_COLLISION);
	/* Failure preserves the existing registration. */
	assert(asn1typed_name_scope_add(&scope, a, first) == ASN1TYPED_NAME_OK);
	asn1typed_name_scope_clear(&scope);
	/* Collision detection does not depend on insertion order. */
	assert(asn1typed_name_scope_add(&scope, b, second) == ASN1TYPED_NAME_OK);
	assert(asn1typed_name_scope_add(&scope, a, first) == ASN1TYPED_NAME_COLLISION);
	printf("collision: %s / %s -> %s (both orders detected)\n", a, b, first);
	free(first);
	free(second);
	asn1typed_name_scope_clear(&scope);
	asn1typed_name_scope_clear(&scope);
}

int
main(void) {
	static const struct {
		const char *source, *type, *field;
	} cases[] = {
		{"NGSetupRequest", "NgSetupRequest", "ng_setup_request"},
		{"GlobalRANNodeID", "GlobalRanNodeId", "global_ran_node_id"},
		{"SupportedTAItem", "SupportedTaItem", "supported_ta_item"},
		{"PagingDRX", "PagingDrx", "paging_drx"},
		{"DefaultPagingDRX", "DefaultPagingDrx", "default_paging_drx"},
		{"SupportedTAList", "SupportedTaList", "supported_ta_list"},
		{"HTTPServerURL", "HttpServerUrl", "http_server_url"},
		{"XMLParserID", "XmlParserId", "xml_parser_id"},
		{"MyABCValue", "MyAbcValue", "my_abc_value"},
		{"alpha-beta", "AlphaBeta", "alpha_beta"},
		{"--alpha---BETA--", "AlphaBeta", "alpha_beta"},
		{"HTTP2ServerURL", "Http2ServerUrl", "http_2_server_url"},
		{"v128", "V128", "v_128"},
		{"Item12ABC34Value", "Item12Abc34Value", "item_12_abc_34_value"},
		{"A", "A", "a"},
		{"ABC", "Abc", "abc"},
		{"aB", "AB", "a_b"},
		{"class", "Class", "class"},
		{"def", "Def", "def"}
	};
	static const char *invalid[] = {
		NULL, "", "-", "---", "123", "2Name", "--2Name", "a_b",
		"a b", "a/b", "a.b", "a\tB", "caf\303\251"
	};
	asn1typed_field_t field = {0};
	asn1typed_name_scope_t scope = {0};
	char *out = NULL;
	char source[] = "Source", name[] = "source";
	size_t i;
	int style;
	for(i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
		check(ASN1TYPED_NAME_TYPE, cases[i].source, cases[i].type);
		check(ASN1TYPED_NAME_FIELD, cases[i].source, cases[i].field);
	}
	check(ASN1TYPED_NAME_MODULE, "NGAP-PDU-Contents", "ngap_pdu_contents");
	check(ASN1TYPED_NAME_MODULE, "NGAP-CommonDataTypes", "ngap_common_data_types");
	check(ASN1TYPED_NAME_IOC_FIELD, "id-DefaultPagingDRX", "default_paging_drx");
	check(ASN1TYPED_NAME_IOC_FIELD, "id-UERetentionInformation", "ue_retention_information");
	check(ASN1TYPED_NAME_IOC_FIELD, "NodeName", "node_name");
	check(ASN1TYPED_NAME_IOC_FIELD, "id-id-NodeName", "id_node_name");
	check(ASN1TYPED_NAME_IOC_FIELD, "ID-NodeName", "id_node_name");
	check(ASN1TYPED_NAME_IOC_FIELD, "Node-id-Name", "node_id_name");
	check(ASN1TYPED_NAME_IOC_FIELD, "id", "id");
	check(ASN1TYPED_NAME_FIELD, "id-NodeName", "id_node_name");
	assert(asn1typed_name_ioc_identity(NULL) == NULL);
	assert(!strcmp(asn1typed_name_ioc_identity("id-id-NodeName"), "id-NodeName"));
	for(style = ASN1TYPED_NAME_TYPE; style <= ASN1TYPED_NAME_MODULE; ++style)
		for(i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i) {
			out = name; /* Failure must clear the output without freeing it. */
			assert(asn1typed_name_make(invalid[i], (asn1typed_name_style_e)style,
				&out) == ASN1TYPED_NAME_INVALID);
			assert(out == NULL);
		}
	assert(asn1typed_name_make("id-", ASN1TYPED_NAME_IOC_FIELD, &out)
		== ASN1TYPED_NAME_INVALID && out == NULL);
	assert(asn1typed_name_make("id---", ASN1TYPED_NAME_IOC_FIELD, &out)
		== ASN1TYPED_NAME_INVALID && out == NULL);
	assert(asn1typed_name_make("Valid", (asn1typed_name_style_e)99, &out)
		== ASN1TYPED_NAME_INVALID && out == NULL);
	assert(asn1typed_name_make("Valid", ASN1TYPED_NAME_TYPE, NULL)
		== ASN1TYPED_NAME_INVALID);
	puts("invalid inputs and API arguments rejected");

	/* Only raw metadata is populated: no type, row, or message context. */
	assert(asn1typed_field_set_ioc(&field, "id-NodeName",
		ASN1TYPED_CRITICALITY_IGNORE, 0, 0) == 0);
	check(ASN1TYPED_NAME_IOC_FIELD, field.ioc.symbolic_id, "node_name");
	assert(!strcmp(field.ioc.symbolic_id, "id-NodeName"));
	free(field.ioc.symbolic_id);

	check_collision("HTTPServer", "Http-Server", ASN1TYPED_NAME_TYPE);
	check_collision("HTTPServer", "Http-Server", ASN1TYPED_NAME_FIELD);
	check_collision("id-NodeName", "NodeName", ASN1TYPED_NAME_IOC_FIELD);
	check_collision("Example-Module", "ExampleModule", ASN1TYPED_NAME_MODULE);
	assert(asn1typed_name_scope_add(&scope, source, name) == ASN1TYPED_NAME_OK);
	memset(source, 'x', sizeof(source) - 1);
	memset(name, 'x', sizeof(name) - 1);
	assert(asn1typed_name_scope_add(&scope, "Other", "source") == ASN1TYPED_NAME_COLLISION);
	assert(asn1typed_name_scope_add(&scope, "Other", "other") == ASN1TYPED_NAME_OK);
	assert(asn1typed_name_scope_add(&scope, "", "valid") == ASN1TYPED_NAME_INVALID);
	assert(asn1typed_name_scope_add(&scope, "Key", "not-valid") == ASN1TYPED_NAME_INVALID);
	assert(asn1typed_name_scope_add(NULL, "Key", "valid") == ASN1TYPED_NAME_INVALID);
	assert(asn1typed_name_scope_add(&scope, NULL, "valid") == ASN1TYPED_NAME_INVALID);
	assert(asn1typed_name_scope_add(&scope, "Key", NULL) == ASN1TYPED_NAME_INVALID);
	asn1typed_name_scope_clear(&scope);
	assert(scope.entries == NULL);
	asn1typed_name_scope_clear(NULL);
	puts("scope ownership and independent names verified");
	return 0;
}
