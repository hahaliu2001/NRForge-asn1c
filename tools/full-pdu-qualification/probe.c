#include <asn1typed_extract.h>
#include <asn1typed_render_cpp.h>
#include "developer_tree.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* One untouched six-module Parser/Fixer pass, owned extraction before deletion,
 * followed by deterministic generation of all six headers. */
struct row {
    char message[128];
    asn1typed_module_t body;
    asn1typed_target_envelope_t envelope;
};
static int publish(const char *directory, size_t ordinal, const char *family,
        const char *text) {
    char path[4096];
    FILE *file;
    int n = snprintf(path, sizeof(path), "%s/%03zu_%s.hpp", directory, ordinal, family);
    if(n < 0 || (size_t)n >= sizeof(path) || !(file = fopen(path, "wb"))) return -1;
    int written = fputs(text, file) != EOF;
    int closed = fclose(file) == 0;
    return written && closed ? 0 : -1;
}
int main(int argc, char **argv) {
    struct row *rows = NULL;
    dev_tree_options_t options;
    asn1p_t *tree = NULL;
    const char *failed = NULL;
    char line[256], why[1024] = {0};
    size_t count = 0, i = 0, family;
    int result = 1;
    FILE *list = NULL;
    int (*body_renderers[])(const asn1typed_module_t *, const char *, char **, char *, size_t) = {
        asn1typed_render_cpp_owned_ioc_types, asn1typed_render_cpp_owned_ioc_mapping,
        asn1typed_render_cpp_owned_ioc_codec
    };
    int (*envelope_renderers[])(const asn1typed_module_t *, const asn1typed_target_envelope_t *, const char *, char **, char *, size_t) = {
        asn1typed_render_cpp_target_envelope_types, asn1typed_render_cpp_target_envelope_mapping,
        asn1typed_render_cpp_target_envelope_codec
    };
    static const char *const families[] = {"body_types", "body_mapping", "body_codec",
        "envelope_types", "envelope_mapping", "envelope_codec"};
    if(argc != 5) {
        fprintf(stderr, "Usage: probe MODULE_LIST ASN1_ROOT MESSAGE_LIST HEADER_DIR\n");
        return 2;
    }
    rows = calloc(512, sizeof(*rows));
    if(!rows || !(list = fopen(argv[3], "r"))) goto done;
    while(fgets(line, sizeof(line), list)) {
        size_t length = strcspn(line, "\r\n");
        if(!length || line[0] == '#') continue;
        if(count == 512 || length >= sizeof(rows[count].message)
            || (line[length] == '\0' && !feof(list))) goto done;
        for(size_t k = 0; k < length; ++k)
            if(!((line[k] >= 'A' && line[k] <= 'Z')
                || (line[k] >= 'a' && line[k] <= 'z')
                || (line[k] >= '0' && line[k] <= '9') || line[k] == '-')) goto done;
        for(size_t k = 0; k < count; ++k)
            if(strlen(rows[k].message) == length && !memcmp(rows[k].message, line, length)) goto done;
        memcpy(rows[count].message, line, length);
        rows[count++].message[length] = '\0';
    }
    if(ferror(list) || !count) goto done;
    fclose(list); list = NULL;
    options.module_list = argv[1]; options.asn1_root = argv[2];
    tree = dev_tree_load(&options, &failed);
    if(!tree) { fprintf(stderr, "parse failed: %s\n", failed ? failed : "unknown"); goto done; }
    if(dev_tree_fix(tree) < 0) goto done;
    for(i = 0; i < count; ++i) {
        if(asn1typed_extract_physical_message(tree, "NGAP-PDU-Contents", rows[i].message,
                &rows[i].body, why, sizeof(why))
            || asn1typed_extract_target_envelope(tree, "NGAP-PDU-Descriptions", "NGAP-PDU",
                "NGAP-PDU-Contents", rows[i].message, &rows[i].envelope, why, sizeof(why))) goto done;
    }
    asn1p_delete(tree); tree = NULL;
    printf("{\"parse\":\"PASS\",\"fix\":\"PASS\",\"parser_deleted\":true,\"messages\":[");
    for(i = 0; i < count; ++i) {
        const asn1typed_target_envelope_t *d = &rows[i].envelope;
        for(family = 0; family < 6; ++family) {
            char *first = NULL, *second = NULL;
            int a, b;
            if(family < 3) {
                a = body_renderers[family](&rows[i].body, "fullpdu", &first, why, sizeof(why));
                b = body_renderers[family](&rows[i].body, "fullpdu", &second, why, sizeof(why));
            } else {
                a = envelope_renderers[family - 3](&rows[i].body, d, "fullpdu", &first, why, sizeof(why));
                b = envelope_renderers[family - 3](&rows[i].body, d, "fullpdu", &second, why, sizeof(why));
            }
            int valid = !a && !b && first && second && !strcmp(first, second)
                && !publish(argv[4], i, families[family], first);
            free(first); free(second);
            if(!valid) goto done;
        }
        printf("%s{\"message\":\"%s\",\"code\":%jd,\"role\":%u,\"criticality\":%u,\"deterministic\":true}",
            i ? "," : "", rows[i].message, d->rows[d->target_row_index].numeric_code,
            (unsigned)d->roots[d->target_root_ordinal].role, d->rows[d->target_row_index].expected_criticality);
    }
    printf("]}\n"); result = 0;
done:
    if(result) fprintf(stderr, "qualification generation failed at row %zu: %s\n", i, why);
    if(list) fclose(list);
    if(tree) asn1p_delete(tree);
    if(rows) for(i = 0; i < count; ++i) {
        asn1typed_module_clear(&rows[i].body);
        asn1typed_target_envelope_clear(&rows[i].envelope);
    }
    free(rows); return result;
}
