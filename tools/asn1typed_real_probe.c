#include <stddef.h>
#include <asn1fix.h>
#include <asn1typed.h>
#include <asn1typed_extract.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "developer_tree.h"

static void help(FILE *f) {
    fprintf(f, "Usage: asn1typed_real_probe --module-list FILE [--asn1-root DIR] --root-module MODULE --message NAME\n"
        "Parse ordered ASN.1 modules, fix their combined tree, and extract one message.\n"
        "Example: asn1typed_real_probe --asn1-root asn1 --module-list modules.txt --root-module Example --message Request\n"
        "Exit: 0 success; 2 invalid CLI; 3 module-list/load/parse failure; 4 fix failure; 5 extraction failure.\n");
}
int main(int argc, char **argv) {
    const char *list = NULL, *root = NULL, *module = NULL, *message = NULL, *failed = NULL;
    dev_tree_options_t options;
    asn1p_t *tree = NULL;
    asn1typed_module_t out;
    char error[1024];
    int i, rc = 0;
    memset(&out, 0, sizeof(out));
    for(i=1;i<argc;i++) {
        if(!strcmp(argv[i], "--help")) { help(stdout); return 0; }
        if(i+1>=argc) { help(stderr); return 2; }
        if(!strcmp(argv[i], "--module-list")) list=argv[++i];
        else if(!strcmp(argv[i], "--asn1-root")) root=argv[++i];
        else if(!strcmp(argv[i], "--root-module")) module=argv[++i];
        else if(!strcmp(argv[i], "--message")) message=argv[++i];
        else { help(stderr); return 2; }
    }
    if(!list || !module || !message) { help(stderr); return 2; }
    options.asn1_root=root; options.module_list=list;
    tree=dev_tree_load(&options, &failed);
    if(!tree) { puts("PARSE FAIL"); if(failed) printf("FILE %s\n", failed); return 3; }
    puts("PARSE PASS");
    if(dev_tree_fix(tree)<0) { puts("FIX FAIL"); rc=4; goto done; }
    puts("FIX PASS");
    if(asn1typed_extract_message(tree,module,message,&out,error,sizeof(error))) {
        puts("EXTRACT FAIL"); printf("DIAGNOSTIC %s\n", error); rc=5; goto done;
    }
    puts("EXTRACT PASS"); printf("ROOT %s.%s\nOWNED_TYPES %zu\nBOUND_INSTANCES %zu\n",module,message,out.type_count,out.bound_instance_count);
done:
    asn1typed_module_clear(&out);
    asn1p_delete(tree);
    return rc;
}
