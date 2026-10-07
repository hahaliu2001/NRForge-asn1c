#ifndef ASN1TYPED_DEVELOPER_TREE_H
#define ASN1TYPED_DEVELOPER_TREE_H
#include <stddef.h>
#include <asn1parser.h>

typedef struct {
    const char *asn1_root;
    const char *module_list;
} dev_tree_options_t;

/* Returns NULL and sets failed_file on module-list, parse, or allocation errors. */
asn1p_t *dev_tree_load(const dev_tree_options_t *options, const char **failed_file);
int dev_tree_fix(asn1p_t *tree);
#endif
