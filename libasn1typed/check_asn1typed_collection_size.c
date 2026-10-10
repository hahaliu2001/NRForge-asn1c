#include "asn1typed_extract.h"
#include <asn1fix.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define REQUIRE(x) do { if(!(x)) { fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #x); abort(); } } while(0)
#ifndef COLLECTION_SIZE_FIXTURE
#define COLLECTION_SIZE_FIXTURE "fixtures/collection-size-evidence-n8.asn1"
#endif

static const asn1typed_type_t *find(const asn1typed_module_t *module, const char *name) {
    size_t i;
    for(i = 0; i < module->type_count; ++i)
        if(!strcmp(module->types[i].identity.source_name, name)) return &module->types[i];
    REQUIRE(0);
    return NULL;
}
static void size_evidence(const asn1typed_module_t *module, const char *name,
                          intmax_t lower, intmax_t upper, int extensible) {
    const asn1typed_type_t *type = find(module, name);
    REQUIRE(type->kind == ASN1TYPED_TYPE_SEQUENCE_OF);
    REQUIRE(type->size_constraint.has_size_constraint == 1);
    REQUIRE(type->size_constraint.lower_bound == lower);
    REQUIRE(type->size_constraint.upper_bound == upper);
    REQUIRE(type->size_constraint.is_extensible == extensible);
}
int main(void) {
    asn1typed_module_t module = {0}, rejected = {0};
    asn1p_t *tree = asn1p_parse_file(COLLECTION_SIZE_FIXTURE, A1P_NOFLAGS);
    const asn1typed_type_t *type;
    char diagnostic[512] = {0};
    REQUIRE(tree && asn1f_process(tree, A1F_NOFLAGS, NULL) >= 0);
    REQUIRE(asn1typed_extract_module(tree, "CollectionSizeEvidence", &module,
                                    diagnostic, sizeof(diagnostic)) == 0);
    REQUIRE(asn1typed_extract_module(tree, "CollectionSizeUnion", &rejected,
                                    diagnostic, sizeof(diagnostic)) == -1);
    REQUIRE(diagnostic[0] && strstr(diagnostic, "constraint"));
    REQUIRE(rejected.type_count == 0 && rejected.types == NULL);
    asn1p_delete(tree);
    size_evidence(&module, "RangeList", 0, 65535, 0);
    size_evidence(&module, "FixedList", 2, 2, 0);
    size_evidence(&module, "EmptyList", 0, 0, 0);
    size_evidence(&module, "PositiveList", 1, 3, 0);
    size_evidence(&module, "ExtensibleList", 1, 3, 1);
    size_evidence(&module, "ExactExtensibleList", 2, 2, 1);
    type = find(&module, "UnboundedList");
    REQUIRE(type->kind == ASN1TYPED_TYPE_SEQUENCE_OF);
    REQUIRE(type->size_constraint.has_size_constraint == 0);
    REQUIRE(type->size_constraint.is_extensible == 0);
    type = find(&module, "FixedList");
    REQUIRE(type->element_type.kind == ASN1TYPED_REF_PRIMITIVE);
    REQUIRE(type->element_type.primitive_kind == ASN1TYPED_PRIMITIVE_BOOLEAN);
    type = find(&module, "RangeList");
    REQUIRE(type->element_type.kind == ASN1TYPED_REF_NAMED);
    REQUIRE(!strcmp(type->element_type.module, "CollectionSizeEvidence"));
    REQUIRE(!strcmp(type->element_type.source_name, "Flag"));
    asn1typed_module_clear(&module);
    asn1typed_module_clear(&rejected);
    puts("PASS owned SEQUENCE OF exact/range/missing/extensible SIZE evidence");
    return 0;
}
