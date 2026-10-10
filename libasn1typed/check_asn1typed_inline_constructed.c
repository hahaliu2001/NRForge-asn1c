#include "asn1typed_extract.h"
#include "asn1typed_render_cpp.h"
#include <asn1fix.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define REQUIRE(x) do { if(!(x)) { fprintf(stderr, "%d: %s (%s)\n", __LINE__, #x, diagnostic); abort(); } } while(0)
int main(int argc, char **argv) {
    asn1p_t *tree;
    asn1typed_module_t module = {0};
    char diagnostic[512] = {0};
    const char *files[] = {"types.hpp", "mapping.hpp", "codec.hpp"};
    int (*renderers[])(const asn1typed_module_t *, const char *, char **, char *, size_t) = {
        asn1typed_render_cpp_owned_collection_types, asn1typed_render_cpp_owned_collection_mapping, asn1typed_render_cpp_owned_collection_codec
    };
    size_t i;
    REQUIRE(argc == 3 || argc == 4);
    tree = asn1p_parse_file(argv[1], A1P_NOFLAGS);
    REQUIRE(tree && asn1f_process(tree, A1F_NOFLAGS, NULL) >= 0);
    if(argc == 4) {
        REQUIRE(asn1typed_extract_physical_message(tree, "InlineCollection", "Message", &module, diagnostic, sizeof(diagnostic)) == 0);
        renderers[0] = asn1typed_render_cpp_owned_ioc_types;
        renderers[1] = asn1typed_render_cpp_owned_ioc_mapping;
        renderers[2] = asn1typed_render_cpp_owned_ioc_codec;
    } else REQUIRE(asn1typed_extract_module(tree, "InlineConstructed", &module, diagnostic, sizeof(diagnostic)) == 0);
    asn1p_delete(tree);
    if(argc == 3) {
    REQUIRE(module.type_count == 10);
    REQUIRE(strcmp(module.types[0].fields[0].type.source_name, module.types[0].fields[1].type.source_name));
    /* Family collection entry points require dependency-before-use order.
     * The physical IOC entry point performs this ordering internally. */
    for(i = 0; i < module.type_count / 2; ++i) {
        asn1typed_type_t swap = module.types[i];
        module.types[i] = module.types[module.type_count - i - 1];
        module.types[module.type_count - i - 1] = swap;
    }
    }
    for(i = 0; i < 3; ++i) {
        char *output = NULL, path[4096];
        FILE *file;
        REQUIRE(renderers[i](&module, argc == 4 ? "inline_collection" : "inline_test", &output, diagnostic, sizeof(diagnostic)) == 0);
        REQUIRE(snprintf(path, sizeof(path), "%s/%s", argv[2], files[i]) > 0);
        file = fopen(path, "wb");
        REQUIRE(file && fwrite(output, 1, strlen(output), file) == strlen(output));
        REQUIRE(fclose(file) == 0);
        free(output);
    }
    {
        const char *bad = "BadInline DEFINITIONS AUTOMATIC TAGS ::= BEGIN\n"
            "Root ::= SEQUENCE { child SEQUENCE { bit BOOLEAN } (WITH COMPONENTS { bit PRESENT }) }\nEND\n";
        asn1typed_module_t rejected = {0};
        tree = asn1p_parse_buffer(bad, -1, "bad-inline.asn", 1, A1P_NOFLAGS);
        REQUIRE(tree && asn1f_process(tree, A1F_NOFLAGS, NULL) >= 0);
        REQUIRE(asn1typed_extract_module(tree, "BadInline", &rejected, diagnostic, sizeof(diagnostic)) == -1);
        REQUIRE(diagnostic[0] && !rejected.types && !rejected.type_count);
        asn1p_delete(tree);
        asn1typed_module_clear(&rejected);
    }
    asn1typed_module_clear(&module);
    return 0;
}
