/* Freeze P10's ten named instance contexts against current owned tag evidence. */
#include "developer_tree.h"
#include <asn1typed_extract.h>
#include <stdio.h>
#include <string.h>
static const char *names[]={"LocationMeasurementInfo","SL-L2RemoteUE-Config-r17",
    "SL-DRX-ConfigUC-r17","SL-MeasConfig-r16","SL-CSI-RS-Config-r16",
    "SL-LatencyBoundIUC-Report-r17","SL-PagingInfo-RemoteUE-r17",
    "SL-RequestedPosSIB-List-r18","SL-RequestedSIB-List-r17","SL-SFN-DFN-Offset-r18"};
static const char *modules[]={"NR-RRC-Definitions","NR-RRC-Definitions",
    "NR-RRC-Definitions","NR-RRC-Definitions","PC5-RRC-Definitions",
    "PC5-RRC-Definitions","PC5-RRC-Definitions","PC5-RRC-Definitions",
    "PC5-RRC-Definitions","PC5-RRC-Definitions"};
int main(int argc,char **argv) {
    if(argc!=4) return 2;
    dev_tree_options_t options={.module_list=argv[1],.asn1_root=argv[2]}; const char *why=NULL;
    asn1p_t *tree=dev_tree_load(&options,&why);
    if(!tree || dev_tree_fix(tree)) return 3;
    FILE *roots=fopen(argv[3],"r"); if(!roots) return 2;
    char module[128],name[128],error[2048]; unsigned seen[10]={0};
    while(fscanf(roots,"%127s %127s",module,name)==2) {
        asn1typed_module_t graph={0};
        if(asn1typed_extract_root_graph(tree,module,name,NULL,&graph,error,sizeof(error))) continue;
        for(size_t i=0;i<graph.type_count;++i) {
            asn1typed_type_t *t=&graph.types[i];
            if(t->kind!=ASN1TYPED_TYPE_CHOICE || t->ordinary_instance.actual_count!=1) continue;
            for(size_t j=0;j<10;++j) {
                const asn1typed_type_actual_t *actual=&t->ordinary_instance.actuals[0];
                if(strcmp(actual->module,modules[j]) || strcmp(actual->source_name,names[j])) continue;
                if(t->alternative_count!=2 || strcmp(t->alternatives[0].source_name,"release") || strcmp(t->alternatives[1].source_name,"setup")) return 4;
                for(size_t k=0;k<2;++k) {
                    asn1typed_choice_alternative_t *a=&t->alternatives[k];
                    if(j<10) {
                        if(!a->has_per_root_index || a->per_root_index!=k ||
                           a->effective_tag_class!=ASN1TYPED_TAG_CLASS_CONTEXT_SPECIFIC || a->effective_tag_number!=(intmax_t)k) return 4;
                    } else if(a->has_per_root_index) return 4;
                }
                ++seen[j];
            }
        }
        asn1typed_module_clear(&graph);
    }
    int failed=ferror(roots); fclose(roots); asn1p_delete(tree); if(failed) return 3;
    printf("{\"instances\":[");
    for(size_t j=0;j<10;++j) {
        if(!seen[j]) return 4;
        printf("%s{\"module\":\"%s\",\"actual\":\"%s\",\"tagged\":%s,\"root_occurrences\":%u}",j?",":"",modules[j],names[j],j<10?"true":"false",seen[j]);
    }
    printf("]}\n"); return 0;
}
