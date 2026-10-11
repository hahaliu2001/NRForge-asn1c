#include "developer_tree.h"
#include "asn1typed_extract.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"real-alias:%d: %s (%s)\n",__LINE__,#x,error); exit(1); } } while(0)
static char error[2048];
static asn1typed_type_t *find(asn1typed_module_t *g,const char *name) {
    for(size_t i=0;i<g->type_count;++i)
        if(!strcmp(g->types[i].identity.module,"NR-RRC-Definitions") && !strcmp(g->types[i].identity.source_name,name)) return &g->types[i];
    return NULL;
}
int main(int argc,char **argv) {
    CHECK(argc==3); dev_tree_options_t opts={.module_list=argv[1],.asn1_root=argv[2]};
    const char *why=NULL; asn1p_t *t=dev_tree_load(&opts,&why); CHECK(t && !dev_tree_fix(t));
    const char *roots[]={"VisitedCellInfoList-r16","MobilityHistoryReport-r16","UEInformationResponse-r16","UL-DCCH-Message"};
    asn1typed_module_t graphs[4]={{0}};
    for(size_t i=0;i<4;++i) CHECK(!asn1typed_extract_root_graph(t,"NR-RRC-Definitions",roots[i],NULL,&graphs[i],error,sizeof(error)));
    asn1p_delete(t);
    for(size_t i=0;i<4;++i) {
        asn1typed_type_t *list=find(&graphs[i],i ? "MobilityHistoryReport-r16" : "VisitedCellInfoList-r16");
        CHECK(list && list->kind==ASN1TYPED_TYPE_SEQUENCE_OF);
        CHECK(list->size_constraint.has_size_constraint && list->size_constraint.lower_bound==1 && list->size_constraint.upper_bound==16);
        CHECK(!list->size_constraint.is_extensible && !list->size_constraint.has_extension_addition);
        CHECK(list->element_type.kind==ASN1TYPED_REF_NAMED && !strcmp(list->element_type.module,"NR-RRC-Definitions") && !strcmp(list->element_type.source_name,"VisitedCellInfo-r16"));
        CHECK(list->location.line>0 && list->location.file);
        asn1typed_type_t *element=find(&graphs[i],"VisitedCellInfo-r16"); CHECK(element && element->field_count);
        CHECK(element->fields[0].location.line==graphs[0].types[1].fields[0].location.line);
    }
    for(size_t i=0;i<4;++i) asn1typed_module_clear(&graphs[i]);
    puts("real alias: direct terminal, named alias and two real root closures own SIZE1..16/VisitedCellInfo identity after AST destruction PASS");
    return 0;
}
