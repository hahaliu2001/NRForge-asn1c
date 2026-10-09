#include "asn1typed_extract.h"
#include "asn1typed_render_cpp.h"
#include <asn1fix.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void
write_output(const char *path, const char *text) {
	FILE *f = fopen(path, "wb");
	assert(f && fwrite(text, 1, strlen(text), f) == strlen(text));
	assert(fclose(f) == 0);
}

int
main(int argc, char **argv) {
	asn1p_t *tree;
	asn1typed_module_t module = {0};
	char diagnostic[512];
	char *types = NULL, *mapping = NULL, *codec = NULL, *codec_again = NULL;
	char path[4096];
	assert(argc == 5);
	tree = asn1p_parse_file(argv[1], A1P_NOFLAGS);
	assert(tree && asn1f_process(tree, A1F_NOFLAGS, NULL) >= 0);
	assert(asn1typed_extract_module(tree, argv[2], &module, diagnostic, sizeof(diagnostic)) == 0);
	/* The renderer must consume only owned data after Parser/Fixer destruction. */
	asn1p_delete(tree);
	assert(asn1typed_render_cpp_owned_slice(&module, argv[3], &types, diagnostic, sizeof(diagnostic)) == 0);
	assert(asn1typed_render_cpp_owned_aper_mapping(&module, argv[3], &mapping, diagnostic, sizeof(diagnostic)) == 0);
	assert(asn1typed_render_cpp_owned_aper_codec(&module, argv[3], &codec, diagnostic, sizeof(diagnostic)) == 0);
	assert(asn1typed_render_cpp_owned_aper_codec(&module, argv[3], &codec_again, diagnostic, sizeof(diagnostic)) == 0);
	assert(!strcmp(codec, codec_again));
	assert(snprintf(path, sizeof(path), "%s/types.hpp", argv[4]) > 0); write_output(path, types);
	assert(snprintf(path, sizeof(path), "%s/mapping.hpp", argv[4]) > 0); write_output(path, mapping);
	assert(snprintf(path, sizeof(path), "%s/codec.hpp", argv[4]) > 0); write_output(path, codec);
	free(types); free(mapping); free(codec); free(codec_again);
	asn1typed_module_clear(&module);
	return 0;
}
