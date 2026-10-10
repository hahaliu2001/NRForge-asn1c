#include <asn1typed_extract.h>
#include <asn1typed_render_cpp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "developer_tree.h"

static void help(FILE *file) {
    fprintf(file, "Usage: asn1typed_ioc_probe --module-list FILE [--asn1-root DIR] --root-module MODULE --message NAME --output-prefix PATH [--namespace NAME] [--verify-determinism]\n"
        "Optional envelope: --envelope-module MODULE --envelope-type NAME --envelope-output-prefix PATH\n"
        "Parse/fix a complete source set, extract physical IOC evidence, delete the Parser tree, and generate types/mapping/codec headers.\n"
        "Writes PATH_types.hpp, PATH_mapping.hpp and PATH_codec.hpp; the parent directory must exist.\n"
        "This is generation readiness, not message or envelope wire qualification.\n"
        "Exit: 0 success; 2 CLI; 3 parse/load; 4 fix; 5 extraction; 6 generation; 7 output I/O.\n");
}
int main(int argc, char **argv) {
    const char *list = NULL, *root = NULL, *module = NULL, *message = NULL;
    const char *prefix = NULL, *ns = "ioc_probe", *failed = NULL;
    const char *envelope_module = NULL, *envelope_type = NULL, *envelope_prefix = NULL;
    const char *suffixes[] = {"types", "mapping", "codec"};
    int (*renderers[])(const asn1typed_module_t *, const char *, char **, char *, size_t) = {
        asn1typed_render_cpp_owned_ioc_types,
        asn1typed_render_cpp_owned_ioc_mapping,
        asn1typed_render_cpp_owned_ioc_codec
    };
    int (*envelope_renderers[])(const asn1typed_module_t *, const asn1typed_target_envelope_t *, const char *, char **, char *, size_t) = {
        asn1typed_render_cpp_target_envelope_types,
        asn1typed_render_cpp_target_envelope_mapping,
        asn1typed_render_cpp_target_envelope_codec
    };
    dev_tree_options_t options;
    asn1p_t *tree = NULL;
    asn1typed_module_t owned = {0};
    asn1typed_target_envelope_t envelope = {0};
    char error[1024], *outputs[6] = {NULL, NULL, NULL, NULL, NULL, NULL}, *path = NULL;
    size_t q, j, path_size, output_count = 3;
    int i, result = 0, verify_determinism = 0;
    for(i = 1; i < argc; ++i) {
        if(!strcmp(argv[i], "--help")) { help(stdout); return 0; }
        if(!strcmp(argv[i], "--verify-determinism")) { verify_determinism = 1; continue; }
        if(i + 1 >= argc) { help(stderr); return 2; }
        if(!strcmp(argv[i], "--module-list")) list = argv[++i];
        else if(!strcmp(argv[i], "--asn1-root")) root = argv[++i];
        else if(!strcmp(argv[i], "--root-module")) module = argv[++i];
        else if(!strcmp(argv[i], "--message")) message = argv[++i];
        else if(!strcmp(argv[i], "--output-prefix")) prefix = argv[++i];
        else if(!strcmp(argv[i], "--namespace")) ns = argv[++i];
        else if(!strcmp(argv[i], "--envelope-module")) envelope_module = argv[++i];
        else if(!strcmp(argv[i], "--envelope-type")) envelope_type = argv[++i];
        else if(!strcmp(argv[i], "--envelope-output-prefix")) envelope_prefix = argv[++i];
        else { help(stderr); return 2; }
    }
    if(!list || !list[0] || !module || !module[0] || !message || !message[0] || !prefix || !prefix[0]) { help(stderr); return 2; }
    if(envelope_module || envelope_type || envelope_prefix) {
        if(!envelope_module || !envelope_module[0] || !envelope_type || !envelope_type[0] || !envelope_prefix || !envelope_prefix[0]) { help(stderr); return 2; }
        if(!strcmp(prefix, envelope_prefix)) { help(stderr); return 2; }
        output_count = 6;
    }
    options.asn1_root = root; options.module_list = list;
    tree = dev_tree_load(&options, &failed);
    if(!tree) { puts("PARSE FAIL"); if(failed) printf("FILE %s\n", failed); return 3; }
    puts("PARSE PASS");
    if(dev_tree_fix(tree) < 0) { puts("FIX FAIL"); result = 4; goto done; }
    puts("FIX PASS");
    if(asn1typed_extract_physical_message(tree, module, message, &owned, error, sizeof(error))) {
        printf("EXTRACT FAIL\nDIAGNOSTIC %s\n", error); result = 5; goto done;
    }
    if(output_count == 6 && asn1typed_extract_target_envelope(tree, envelope_module, envelope_type, module, message, &envelope, error, sizeof(error))) {
        printf("ENVELOPE EXTRACT FAIL\nDIAGNOSTIC %s\n", error); result = 5; goto done;
    }
    asn1p_delete(tree); tree = NULL;
    printf("EXTRACT PASS\nPARSER_DELETED\nROOT %s.%s\nOWNED_TYPES %zu\nBOUND_INSTANCES %zu\nIOC_REGISTRIES %zu\n", module, message, owned.type_count, owned.bound_instance_count, owned.ioc_registry_count);
    for(q = 0; q < owned.ioc_registry_count; ++q) {
        const asn1typed_ioc_registry_t *r = &owned.ioc_registries[q];
        printf("REGISTRY %s.%s SET %s.%s SELECTED %s ROWS %zu EXTENSIBLE %d VALID %d\n", r->class_module, r->class_source_name, r->object_set_module, r->object_set_source_name, r->selected_class_field_source_name, r->row_count, r->object_set_is_extensible, r->has_valid_dispatch);
        for(j = 0; j < r->row_count; ++j)
            printf("ROW %jd EXPECTED_CRITICALITY %u HAS_PRESENCE %d PRESENCE %u\n", r->rows[j].numeric_id, (unsigned)r->rows[j].criticality, r->rows[j].has_presence, (unsigned)r->rows[j].presence);
    }
    /* Preflight all requested families before producing any file. I/O errors can
     * leave earlier output files; the diagnostic never claims atomic I/O. */
    if(output_count == 6) printf("ENVELOPE EXTRACT PASS\nENVELOPE ROOT %s.%s\nENVELOPE ROWS %zu\nENVELOPE TARGET_CODE %jd\n", envelope_module, envelope_type, envelope.row_count, envelope.rows[envelope.target_row_index].numeric_code);
    for(q = 0; q < output_count; ++q) {
        const char *family = q < 3 ? "" : "envelope_";
        if((q < 3 ? renderers[q](&owned, ns, &outputs[q], error, sizeof(error)) : envelope_renderers[q-3](&owned, &envelope, ns, &outputs[q], error, sizeof(error)))) {
            printf("GENERATION %s%s FAIL\nDIAGNOSTIC %s\n", family, suffixes[q%3], error); result = 6; goto done;
        }
        printf("GENERATION %s%s PASS\n", family, suffixes[q%3]);
        if(verify_determinism) {
            char *again = NULL;
            if((q < 3 ? renderers[q](&owned, ns, &again, error, sizeof(error)) : envelope_renderers[q-3](&owned, &envelope, ns, &again, error, sizeof(error)))) {
                printf("DETERMINISM %s%s FAIL\nDIAGNOSTIC %s\n", family, suffixes[q%3], error);
                free(again); result = 6; goto done;
            }
            if(strcmp(outputs[q], again)) {
                printf("DETERMINISM %s%s FAIL\nDIAGNOSTIC generated output changed for unchanged owned graph\n", family, suffixes[q%3]);
                free(again); result = 6; goto done;
            }
            free(again);
            printf("DETERMINISM %s%s PASS\n", family, suffixes[q%3]);
        }
    }
    if(strlen(prefix) > SIZE_MAX - 32) { result = 7; goto done; }
    path_size = strlen(prefix) + 32;
    if(output_count == 6) {
        if(strlen(envelope_prefix) > SIZE_MAX - 32) { result = 7; goto done; }
        if(strlen(envelope_prefix) + 32 > path_size) path_size = strlen(envelope_prefix) + 32;
    }
    path = (char *)malloc(path_size);
    if(!path) { result = 7; goto done; }
    for(q = 0; q < output_count; ++q) {
        FILE *file;
        int n, written, closed;
        n = snprintf(path, path_size, "%s_%s.hpp", q < 3 ? prefix : envelope_prefix, suffixes[q%3]);
        if(n < 0 || (size_t)n >= path_size) { result = 7; goto done; }
        file = fopen(path, "wb");
        if(!file) { fprintf(stderr, "OUTPUT FAIL %s\n", path); result = 7; goto done; }
        written = fwrite(outputs[q], 1, strlen(outputs[q]), file) == strlen(outputs[q]);
        closed = fclose(file) == 0;
        if(!written || !closed) { fprintf(stderr, "OUTPUT FAIL %s\n", path); result = 7; goto done; }
        printf("OUTPUT %s\n", path);
    }
done:
    if(result == 7 && !path) fprintf(stderr, "OUTPUT FAIL allocation or path size\n");
    free(path);
    for(q = 0; q < 6; ++q) free(outputs[q]);
    asn1typed_target_envelope_clear(&envelope);
    asn1typed_module_clear(&owned);
    if(tree) asn1p_delete(tree);
    return result;
}
