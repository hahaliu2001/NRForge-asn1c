#include "asn1typed_extract.h"
#include "asn1typed_render_uper.h"
#include <asn1fix.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef NULL_FIXTURE
#define NULL_FIXTURE "tools/rrc-null-open/null.asn1"
#endif
static char error[2048];
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"null-open:%d: %s (%s)\n",__LINE__,#x,error); exit(1); } } while(0)
int main(void) {
    const char *names[]={"SeqNull","ChoiceNull","SeqBool","ChoiceBool","SeqNamed","ChoiceNamed"};
    asn1p_t *t=asn1p_parse_file(NULL_FIXTURE,A1P_NOFLAGS);
    CHECK(t && asn1f_process(t,A1F_NOFLAGS,NULL)>=0);
    asn1typed_module_t graphs[6]={{0}};
    for(size_t i=0;i<6;++i) CHECK(!asn1typed_extract_root_graph(t,"NullOpen",names[i],NULL,&graphs[i],error,sizeof(error)));
    asn1p_delete(t);
    for(size_t i=0;i<6;++i) {
        char *h=NULL; CHECK(!asn1typed_render_cpp_owned_uper(&graphs[i],"nullopen",&h,error,sizeof(error)));
        CHECK((strstr(h,"Writer& io){\n(void)io;\n")!=NULL)==(i<2));
        CHECK((strstr(h,"Reader& io){(void)io;return")!=NULL)==(i<2));
        free(h); asn1typed_module_clear(&graphs[i]);
    }
    puts("six owned NULL/BOOL/named-NULL SEQUENCE/CHOICE roots: conditional markers and AST lifetime PASS"); return 0;
}
