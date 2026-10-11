/* Generate from owned ordinary graphs after destroying all parser/fixer state. */
#include "developer_tree.h"
#include <asn1typed_extract.h>
#include <asn1typed_render_uper.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
struct row {
    char module[128], root[128], error[512];
    int rc;
    asn1typed_module_t graph;
};
static void string(const char *s) {
    putchar('"');
    for (const unsigned char *p = (const unsigned char *)s; *p; ++p) {
        if (*p == '"' || *p == '\\')
            printf("\\%c", *p);
        else if (*p < 32)
            printf("\\u%04x", *p);
        else
            putchar(*p);
    }
    putchar('"');
}
int main(int argc, char **argv) {
    if (argc != 5) {
        fprintf(stderr, "probe MODULE_LIST ASN1_ROOT ROOT_LIST OUTPUT_DIRECTORY\n");
        return 2;
    }
    struct row *rows = calloc(1024, sizeof(*rows));
    if (!rows)
        return 3;
    FILE *file = fopen(argv[3], "r");
    if (!file) {
        free(rows);
        return 2;
    }
    size_t count = 0;
    char line[512], extra;
    while (fgets(line, sizeof(line), file)) {
        if (count == 1024 || !strchr(line, '\n') ||
            sscanf(line, "%127s %127s %c", rows[count].module, rows[count].root, &extra) != 2) {
            fclose(file);
            free(rows);
            return 2;
        }
        ++count;
    }
    int failed = ferror(file);
    fclose(file);
    if (failed || !count) {
        free(rows);
        return 2;
    }
    dev_tree_options_t options = {.asn1_root = argv[2], .module_list = argv[1]};
    const char *why = NULL;
    asn1p_t *tree = dev_tree_load(&options, &why);
    if (!tree) {
        free(rows);
        return 3;
    }
    if (dev_tree_fix(tree)) {
        asn1p_delete(tree);
        free(rows);
        return 3;
    }
    for (size_t i = 0; i < count; ++i)
        rows[i].rc =
            asn1typed_extract_root_graph(tree, rows[i].module, rows[i].root, NULL, &rows[i].graph,
                                         rows[i].error, sizeof(rows[i].error));
    asn1p_delete(tree);
    printf("{\"owned_after_tree_destruction\":true,\"roots\":[");
    for (size_t i = 0; i < count; ++i) {
        if (i)
            putchar(',');
        char *header = NULL;
        char ns[64];
        snprintf(ns, sizeof(ns), "nrforge_generated::r%03zu", i);
        int rendered = -1;
        if (!rows[i].rc)
            rendered = asn1typed_render_cpp_owned_uper(&rows[i].graph, ns, &header, rows[i].error,
                                                       sizeof(rows[i].error));
        if (rendered && header) {
            free(header);
            return 4;
        }
        if (!rendered) {
            char path[4096];
            int n = snprintf(path, sizeof(path), "%s/r%03zu.hpp", argv[4], i);
            if (n < 0 || (size_t)n >= sizeof(path)) {
                free(header);
                return 4;
            }
            file = fopen(path, "wb");
            if (!file) {
                free(header);
                return 4;
            }
            failed = fputs(header, file) == EOF;
            failed |= fclose(file) != 0;
            free(header);
            if (failed)
                return 4;
        }
        printf("{\"module\":");
        string(rows[i].module);
        printf(",\"root\":");
        string(rows[i].root);
        printf(",\"extract\":%d,\"render\":%d,\"type_count\":%zu,\"error\":", rows[i].rc, rendered,
               rows[i].graph.type_count);
        string(rows[i].error);
        printf(",\"types\":[");
        for (size_t j = 0; j < rows[i].graph.type_count; ++j) {
            if (j)
                putchar(',');
            printf("{\"index\":%zu,\"module\":", j);
            string(rows[i].graph.types[j].identity.module);
            printf(",\"name\":");
            string(rows[i].graph.types[j].identity.source_name);
            putchar('}');
        }
        printf("]}");
        asn1typed_module_clear(&rows[i].graph);
    }
    printf("]}\n");
    free(rows);
    return ferror(stdout) ? 5 : 0;
}
