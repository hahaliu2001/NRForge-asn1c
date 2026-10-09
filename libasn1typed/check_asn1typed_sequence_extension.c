#include "asn1typed_extract.h"
#include <asn1fix.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define REQUIRE(x) do {if(!(x)){fprintf(stderr,"%s:%d: %s\n",__FILE__,__LINE__,#x);abort();}}while(0)
#ifndef SEQ_EXT_FIXTURE
#define SEQ_EXT_FIXTURE "fixtures/sequence-extension-evidence-n7.asn1"
#endif
#ifndef SEQ_ADD_FIXTURE
#define SEQ_ADD_FIXTURE "fixtures/sequence-extension-additions-n7.asn1"
#endif
#ifndef SEQ_REPEAT_FIXTURE
#define SEQ_REPEAT_FIXTURE "fixtures/sequence-extension-repeated-n7.asn1"
#endif
#ifndef IOC_FIXTURE
#define IOC_FIXTURE "fixtures/ioc-t3.asn1"
#endif
static char diagnostic[512];
static asn1typed_type_t *find(asn1typed_module_t*m,const char*name){
	size_t i;
	for(i=0;i<m->type_count;i++)if(!strcmp(m->types[i].identity.source_name,name))return &m->types[i];
	return NULL;
}
static void resolved(asn1typed_type_t*t,size_t roots){
	REQUIRE(t&&t->has_valid_sequence_extension_structure&&t->sequence_extension_evidence==ASN1TYPED_WIRE_EVIDENCE_RESOLVED&&t->sequence_root_field_count==roots&&t->sequence_known_addition_count==0);
	REQUIRE(asn1typed_sequence_extension_structure_validate(t,diagnostic,sizeof(diagnostic))==0);
}
static void real_fixture(void){
	asn1typed_module_t m= {0};
	asn1p_t*tree=asn1p_parse_file(SEQ_EXT_FIXTURE,A1P_NOFLAGS);
	asn1typed_type_t*t,*plain,*empty;
	REQUIRE(tree&&asn1f_process(tree,A1F_NOFLAGS,NULL)>=0);
	REQUIRE(asn1typed_extract_module(tree,"SequenceExtensionEvidence",&m,diagnostic,sizeof(diagnostic))==0);
	asn1p_delete(tree);
	t=find(&m,"RootOnly");
	resolved(t,2);
	empty=find(&m,"EmptyRoot");
	resolved(empty,0);
	plain=find(&m,"Plain");
	REQUIRE(plain&&!plain->has_valid_sequence_extension_structure&&plain->sequence_extension_evidence==ASN1TYPED_WIRE_EVIDENCE_UNAVAILABLE);
	REQUIRE(asn1typed_sequence_extension_structure_validate(plain,diagnostic,sizeof(diagnostic))==-1);
	REQUIRE(asn1typed_sequence_set_extension_structure(t,1,0)==-1);
	resolved(t,2);
	REQUIRE(asn1typed_sequence_set_extension_structure(t,2,1)==-1);
	resolved(t,2);
	{
		size_t count=t->sequence_root_field_count;
		t->sequence_root_field_count=0;
		REQUIRE(asn1typed_sequence_extension_structure_validate(t,diagnostic,sizeof(diagnostic))==-1);
		t->sequence_root_field_count=count;
		resolved(t,2);
	}
	t->has_valid_sequence_extension_structure=0;
	REQUIRE(asn1typed_sequence_extension_structure_validate(t,diagnostic,sizeof(diagnostic))==-1);
	t->has_valid_sequence_extension_structure=1;
	resolved(t,2);
	{
		size_t capacity=t->field_capacity;
		t->field_capacity=1;
		REQUIRE(asn1typed_sequence_extension_structure_finalize(t,diagnostic,sizeof(diagnostic))==ASN1TYPED_WIRE_FINALIZE_ERROR);
		REQUIRE(t->has_valid_sequence_extension_structure&&t->sequence_root_field_count==2);
		REQUIRE(asn1typed_sequence_set_extension_structure(t,2,0)==-1);
		REQUIRE(t->has_valid_sequence_extension_structure);
		t->field_capacity=capacity;
		resolved(t,2);
	}
	REQUIRE(asn1typed_type_add_field(t,"another","SequenceExtensionEvidence","Flag",ASN1TYPED_PRESENCE_MANDATORY,__FILE__,__LINE__)==0);
	REQUIRE(!t->has_valid_sequence_extension_structure&&t->sequence_extension_evidence==ASN1TYPED_WIRE_EVIDENCE_UNAVAILABLE&&t->sequence_root_field_count==0&&t->sequence_known_addition_count==0);
	REQUIRE(asn1typed_sequence_extension_structure_finalize(t,diagnostic,sizeof(diagnostic))==ASN1TYPED_WIRE_FINALIZE_UNAVAILABLE);
	REQUIRE(asn1typed_sequence_set_extension_structure(t,3,0)==0);
	REQUIRE(asn1typed_sequence_extension_structure_finalize(t,diagnostic,sizeof(diagnostic))==ASN1TYPED_WIRE_FINALIZE_OK);
	resolved(t,3);
	{
		char *name=plain->fields[0].source_name;
		plain->fields[0].source_name="copiedFlag";
		REQUIRE(asn1typed_type_add_field_copy(t,&plain->fields[0])==0);
		plain->fields[0].source_name=name;
	}
	REQUIRE(!t->has_valid_sequence_extension_structure);
	REQUIRE(asn1typed_sequence_set_extension_structure(t,4,0)==0);
	REQUIRE(asn1typed_sequence_extension_structure_finalize(t,diagnostic,sizeof(diagnostic))==ASN1TYPED_WIRE_FINALIZE_OK);
	resolved(t,4);
	REQUIRE(asn1typed_sequence_set_extension_unsupported(t)==0);
	REQUIRE(t->sequence_extension_evidence==ASN1TYPED_WIRE_EVIDENCE_UNSUPPORTED&&t->sequence_root_field_count==0&&!t->has_valid_sequence_extension_structure);
	REQUIRE(asn1typed_sequence_extension_structure_finalize(t,diagnostic,sizeof(diagnostic))==ASN1TYPED_WIRE_FINALIZE_UNAVAILABLE);
	REQUIRE(asn1typed_sequence_set_extension_unavailable(t)==0);
	REQUIRE(t->sequence_extension_evidence==ASN1TYPED_WIRE_EVIDENCE_UNAVAILABLE);
	REQUIRE(asn1typed_sequence_set_extension_structure(t,4,0)==0);
	REQUIRE(asn1typed_sequence_extension_structure_finalize(t,diagnostic,sizeof(diagnostic))==ASN1TYPED_WIRE_FINALIZE_OK);
	t->sequence_known_addition_count=1;
	REQUIRE(asn1typed_sequence_extension_structure_validate(t,diagnostic,sizeof(diagnostic))==-1);
	REQUIRE(asn1typed_sequence_extension_structure_finalize(t,diagnostic,sizeof(diagnostic))==ASN1TYPED_WIRE_FINALIZE_UNAVAILABLE);
	REQUIRE(!t->has_valid_sequence_extension_structure);
	REQUIRE(asn1typed_sequence_set_extension_structure(plain,1,0)==-1);
	REQUIRE(asn1typed_sequence_set_extension_structure(find(&m,"Flag"),0,0)==-1);
	REQUIRE(asn1typed_sequence_set_extension_unavailable(find(&m,"Flag"))==-1);
	REQUIRE(asn1typed_sequence_extension_structure_finalize(NULL,diagnostic,sizeof(diagnostic))==ASN1TYPED_WIRE_FINALIZE_ERROR);
	asn1typed_module_clear(&m);
}
static void old_rejection(const char *file, const char *module, int duplicate_marker) {
	asn1typed_module_t m = {
		0
	};
	asn1p_t *tree = asn1p_parse_file(file, A1P_NOFLAGS);
	REQUIRE(tree);
	REQUIRE(asn1f_process(tree, A1F_NOFLAGS, NULL) >= 0);
	if(duplicate_marker) {
		REQUIRE(asn1typed_extract_module(tree, module, &m, diagnostic, sizeof(diagnostic)) == 0);
		resolved(find(&m, "Repeated"), 1);
		asn1typed_module_clear(&m);
		asn1p_module_t *source = TQ_FIRST(&tree->modules);
		asn1p_expr_t *decl = TQ_FIRST(&source->members);
		asn1p_expr_t *member, *marker = NULL;
		for(member = TQ_FIRST(&decl->members); member; member = TQ_NEXT(member, next)) {
			if(member->expr_type == A1TC_EXTENSIBLE) marker = member;
		}
		REQUIRE(marker);
		/* Fixer normalizes the legal closing marker. Exercise the extractor's
		 * existing rejection with two physical markers in its fixed AST input. */
		member = asn1p_expr_clone(marker, 0);
		REQUIRE(member);
		asn1p_expr_add(decl, member);
	}
	REQUIRE(asn1typed_extract_module(tree, module, &m, diagnostic, sizeof(diagnostic)) == -1);
	REQUIRE(diagnostic[0]);
	REQUIRE(m.type_count == 0);
	asn1p_delete(tree);
	asn1typed_module_clear(&m);
}
static void flattened(void){
	asn1typed_module_t m= {0};
	asn1p_t*tree=asn1p_parse_file(IOC_FIXTURE,A1P_NOFLAGS);
	REQUIRE(tree&&asn1f_process(tree,A1F_NOFLAGS,NULL)>=0);
	REQUIRE(asn1typed_extract_message(tree,"SyntheticIOC","ExtensibleRegistration",&m,diagnostic,sizeof(diagnostic))==0);
	asn1p_delete(tree);
	REQUIRE(m.type_count&&m.types[0].is_extensible&&!m.types[0].has_valid_sequence_extension_structure);
	REQUIRE(m.types[0].sequence_extension_evidence!=ASN1TYPED_WIRE_EVIDENCE_RESOLVED);
	REQUIRE(asn1typed_sequence_extension_structure_validate(&m.types[0],diagnostic,sizeof(diagnostic))==-1);
	asn1typed_module_clear(&m);
}
static void bound_move(void){
	asn1typed_module_t m= {0};
	asn1typed_type_t body= {0};
	asn1typed_type_ref_t identity={
		0
	},flag= {0};
	asn1typed_type_actual_t actual= {0};
	asn1typed_bound_instance_t*instance=NULL;
	REQUIRE(asn1typed_module_init(&m,"Move",__FILE__,__LINE__)==0);
	identity.kind=ASN1TYPED_REF_NAMED;
	identity.module="Move";
	identity.source_name="Container";
	actual.kind=ASN1TYPED_ACTUAL_OBJECT_SET_REFERENCE;
	actual.module="Move";
	actual.source_name="Set";
	identity.actuals=&actual;
	identity.actual_count=1;
	REQUIRE(asn1typed_module_add_bound_instance(&m,&identity,&instance)==0);
	body.kind=ASN1TYPED_TYPE_SEQUENCE;
	body.is_extensible=1;
	flag.kind=ASN1TYPED_REF_PRIMITIVE;
	flag.primitive_kind=ASN1TYPED_PRIMITIVE_BOOLEAN;
	REQUIRE(asn1typed_type_add_field_ref(&body,"flag",&flag,ASN1TYPED_PRESENCE_MANDATORY,__FILE__,__LINE__)==0);
	REQUIRE(asn1typed_sequence_set_extension_structure(&body,1,0)==0);
	REQUIRE(asn1typed_sequence_extension_structure_finalize(&body,diagnostic,sizeof(diagnostic))==ASN1TYPED_WIRE_FINALIZE_OK);
	REQUIRE(asn1typed_bound_instance_set_body(&m,0,&body)==0);
	REQUIRE(!body.fields&&!body.field_count&&!body.has_valid_sequence_extension_structure);
	asn1typed_type_clear(&body);
	resolved(&instance->body,1);
	REQUIRE(!strcmp(instance->body.fields[0].source_name,"flag"));
	asn1typed_module_clear(&m);
}
#ifndef BOUND_SEQUENCE_FIXTURE
#define BOUND_SEQUENCE_FIXTURE "fixtures/sequence-extension-bound-n7.asn1"
#endif
static void real_bound(void) {
	asn1typed_module_t m = {
		0
	};
	asn1p_t *tree = asn1p_parse_file(BOUND_SEQUENCE_FIXTURE, A1P_NOFLAGS);
	size_t i;
	int found = 0;
	REQUIRE(tree && asn1f_process(tree, A1F_NOFLAGS, NULL) >= 0);
	REQUIRE(asn1typed_extract_message(tree, "SequenceExtensionBound", "Message", &m, diagnostic, sizeof(diagnostic)) == 0);
	asn1p_delete(tree);
	for(i = 0; i < m.bound_instance_count; ++i) {
		asn1typed_bound_instance_t *instance = &m.bound_instances[i];
		if(!strcmp(instance->identity.source_name, "RootField")) {
			resolved(&instance->body, 3);
			REQUIRE(!strcmp(instance->body.fields[0].source_name, "id"));
			found = 1;
		}
	}
	REQUIRE(found);
	asn1typed_module_clear(&m);
}
int main(void){
	real_fixture();
	old_rejection(SEQ_ADD_FIXTURE,"SequenceExtensionAdditions",0);
	old_rejection(SEQ_REPEAT_FIXTURE,"SequenceExtensionRepeated",1);
	flattened();
	bound_move();
	real_bound();
	return 0;
}
