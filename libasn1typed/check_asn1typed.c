#include "asn1typed.h"

#include <assert.h>
#include <string.h>

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
