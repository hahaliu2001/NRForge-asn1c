#include "asn1typed_extract.h"
#include "asn1typed_render_cpp.h"
#include <asn1fix.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define REQUIRE(x) do { if(!(x)) { fprintf(stderr,"line %d: %s; %s\n",__LINE__,#x,why); abort(); } } while(0)
typedef int (*renderer)(const asn1typed_module_t *,const char *,char **,char *,size_t);
static renderer renderers[]={asn1typed_render_cpp_owned_character_types,asn1typed_render_cpp_owned_character_mapping,asn1typed_render_cpp_owned_character_codec};
int main(int argc,char **argv) {
 asn1typed_module_t module={0}; asn1p_t *tree; char why[512]; size_t i;
 const char *files[]={"types.hpp","mapping.hpp","codec.hpp"};
 REQUIRE(argc==3); tree=asn1p_parse_file(argv[1],A1P_NOFLAGS); REQUIRE(tree && asn1f_process(tree,A1F_NOFLAGS,NULL)>=0);
 REQUIRE(asn1typed_extract_module(tree,"CharactersGeneration",&module,why,sizeof(why))==0); asn1p_delete(tree);
 for(i=0;i<3;++i) { char *a=NULL,*b=NULL,path[4096]; FILE *output;
  REQUIRE(renderers[i](&module,"characters",&a,why,sizeof(why))==0 && a);
  REQUIRE(renderers[i](&module,"characters",&b,why,sizeof(why))==0 && !strcmp(a,b)); free(b);
  REQUIRE(snprintf(path,sizeof(path),"%s/%s",argv[2],files[i])>0); output=fopen(path,"w"); REQUIRE(output && fputs(a,output)>=0 && fclose(output)==0); free(a);
  a=NULL; REQUIRE(renderers[i](&module,"std",&a,why,sizeof(why))==-1 && !a && why[0]);
 }
 { char *out=NULL; REQUIRE(asn1typed_render_cpp_owned_shape_types(&module,"characters",&out,why,sizeof(why))==-1 && !out); }
 { asn1typed_size_constraint_t old=module.types[0].size_constraint; module.types[0].size_constraint.upper_bound=65536;
  for(i=0;i<3;++i) { char *out=NULL; REQUIRE(renderers[i](&module,"characters",&out,why,sizeof(why))==-1 && !out && why[0]); }
  module.types[0].size_constraint=old;
 }
 { asn1typed_size_constraint_t old=module.types[0].size_constraint;
  module.types[0].size_constraint.is_extensible=1; module.types[0].size_constraint.has_extension_addition=1;
  module.types[0].size_constraint.extension_lower_bound=151; module.types[0].size_constraint.extension_upper_bound=160;
  for(i=0;i<3;++i) { char *out=NULL; REQUIRE(renderers[i](&module,"characters",&out,why,sizeof(why))==-1 && !out && strstr(why,"metadata")); }
  module.types[0].size_constraint=old;
 }
 asn1typed_module_clear(&module); return 0;
}
