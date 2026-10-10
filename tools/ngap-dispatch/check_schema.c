/* Exercise the actual controller comparator without Parser/Fixer cost. */
int dispatch_generator_main(int argc,char **argv);
#define main dispatch_generator_main
#include "generate.c"
#undef main
static void require(int condition,int line) {
    if(!condition) { fprintf(stderr,"check_dispatch_schema:%d\n",line); abort(); }
}
#define REQUIRE(condition) require((condition),__LINE__)

int main(void) {
    asn1typed_type_ref_t ref={0};
    asn1typed_target_envelope_t a={0},b={0};
    asn1typed_envelope_root_t roots[4]={{0}},other[4]={{0}};
    size_t i;
    ref.module="Fixture"; ref.source_name="Identity";
    a.header.pdu=a.header.procedure_class=a.header.object_set=ref;
    a.header.procedure_type=a.header.criticality_type=ref;
    a.root_count=4; a.roots=roots;
    for(i=0;i<4;++i) {
        roots[i].source_name="root";
        roots[i].sequence=roots[i].procedure_class=roots[i].object_set=ref;
        roots[i].role=(asn1typed_envelope_role_e)i;
        roots[i].per_root_index=i;
        if(i<3) roots[i].procedure_type=roots[i].criticality_type=ref;
        else roots[i].unsupported_payload=1;
    }
    b=a; memcpy(other,roots,sizeof(roots)); b.roots=other;
    a.target_body=ref; b.target_body=ref; b.target_body.source_name="AnotherTarget";
    REQUIRE(same_schema(&a,&b));
    other[3].unsupported_payload=0; REQUIRE(!same_schema(&a,&b)); other[3]=roots[3];
    other[3].procedure_type=ref; REQUIRE(!same_schema(&a,&b)); other[3]=roots[3];
    other[3].criticality_type=ref; REQUIRE(!same_schema(&a,&b)); other[3]=roots[3];
    other[3].object_set.source_name="WrongSet"; REQUIRE(!same_schema(&a,&b));
    puts("PASS dispatch schema comparison across targets with empty unsupported-root scalar references");
    return 0;
}
