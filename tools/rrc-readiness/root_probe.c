/* RRC-P2: serialize owned ordinary graphs only after deleting the fixed tree. */
#include <asn1typed_extract.h>
#include "developer_tree.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define MAX_ROOTS 1024
struct row { char module[128], root[128], error[2048]; int rc; asn1typed_module_t graph; };
static void string(const char *s) {
    const unsigned char *p = (const unsigned char *)(s ? s : "");
    putchar('"');
    for(; *p; ++p) {
        if(*p == '"' || *p == '\\') { putchar('\\'); putchar(*p); }
        else if(*p < 32) printf("\\u%04x", (unsigned)*p);
        else putchar(*p);
    }
    putchar('"');
}
static void ref(const asn1typed_type_ref_t *r) {
    printf("{\"kind\":%d,\"module\":", (int)r->kind); string(r->module);
    printf(",\"name\":"); string(r->source_name);
    printf(",\"primitive\":%d,\"actual_count\":%zu}", (int)r->primitive_kind, r->actual_count);
}
static void inventory(const asn1typed_module_t *g) {
    size_t i, j;
    putchar('[');
    for(i = 0; i < g->type_count; ++i) {
        const asn1typed_type_t *t = &g->types[i];
        if(i) putchar(',');
        printf("{\"module\":"); string(t->identity.module);
        printf(",\"name\":"); string(t->identity.source_name);
        printf(",\"kind\":%d,\"enum_items\":%zu,\"extensible\":%d,\"fields\":[", (int)t->kind, t->enum_item_count, t->is_extensible);
        for(j = 0; j < t->field_count; ++j) {
            const asn1typed_field_t *f = &t->fields[j];
            if(j) putchar(',');
            printf("{\"name\":"); string(f->source_name);
            printf(",\"presence\":%d,\"inline_enum\":%s,\"ref\":", (int)f->presence, f->inline_enumerated ? "true" : "false");
            ref(&f->type); putchar('}');
        }
        printf("],\"alternatives\":[");
        for(j = 0; j < t->alternative_count; ++j) {
            const asn1typed_choice_alternative_t *a = &t->alternatives[j];
            if(j) putchar(',');
            printf("{\"name\":"); string(a->source_name);
            printf(",\"inline_enum\":%s,\"ref\":", a->inline_enumerated ? "true" : "false"); ref(&a->type_ref); putchar('}');
        }
        printf("],\"element\":"); ref(&t->element_type); putchar('}');
    }
    putchar(']');
}
int main(int argc, char **argv) {
    dev_tree_options_t options;
    asn1p_t *tree;
    struct row *rows;
    const char *failed = NULL;
    FILE *file;
    char line[512], extra;
    size_t count = 0, i;
    if(argc != 4) { fprintf(stderr, "Usage: root_probe MODULE_LIST ASN1_ROOT ROOT_LIST\n"); return 2; }
    rows = calloc(MAX_ROOTS, sizeof(*rows));
    if(!rows) return 3;
    file = fopen(argv[3], "r");
    if(!file) { free(rows); return 2; }
    while(fgets(line, sizeof(line), file)) {
        if(count == MAX_ROOTS || !strchr(line, '\n') ||
                sscanf(line, "%127s %127s %c", rows[count].module, rows[count].root, &extra) != 2) {
            fclose(file); free(rows); return 2;
        }
        ++count;
    }
    if(ferror(file) || !count) { fclose(file); free(rows); return 2; }
    fclose(file);
    options.module_list = argv[1]; options.asn1_root = argv[2];
    tree = dev_tree_load(&options, &failed);
    if(!tree) { fprintf(stderr, "parse failed: %s\n", failed ? failed : "unknown"); free(rows); return 3; }
    if(dev_tree_fix(tree)) { asn1p_delete(tree); free(rows); return 4; }
    for(i = 0; i < count; ++i)
        rows[i].rc = asn1typed_extract_root_graph(tree, rows[i].module, rows[i].root, NULL,
            &rows[i].graph, rows[i].error, sizeof(rows[i].error));
    asn1p_delete(tree);
    printf("{\"parse_fix\":\"PASS\",\"owned_after_tree_destruction\":true,\"roots\":[");
    for(i = 0; i < count; ++i) {
        if(i) putchar(',');
        printf("{\"module\":"); string(rows[i].module);
        printf(",\"root\":"); string(rows[i].root);
        printf(",\"rc\":%d,\"error\":", rows[i].rc); string(rows[i].error);
        printf(",\"type_count\":%zu,\"types\":", rows[i].graph.type_count);
        inventory(&rows[i].graph); putchar('}');
        asn1typed_module_clear(&rows[i].graph);
    }
    printf("]}\n");
    free(rows);
    return ferror(stdout) ? 5 : 0;
}
