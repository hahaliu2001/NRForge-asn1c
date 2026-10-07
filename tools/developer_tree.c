#include <stddef.h>
#include <asn1fix.h>
#include <asn1p_list.h>
#include "developer_tree.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char failed_path[4096];

static char *trim(char *s) {
    char *end;
    while(isspace((unsigned char)*s)) s++;
    end = s + strlen(s);
    while(end > s && isspace((unsigned char)end[-1])) *--end = 0;
    return s;
}

asn1p_t *dev_tree_load(const dev_tree_options_t *opt, const char **failed_file) {
    FILE *list;
    char *line = NULL;
    size_t cap = 0;
    ssize_t len;
    asn1p_t *combined = NULL;
    if(failed_file) *failed_file = opt ? opt->module_list : NULL;
    if(!opt || !opt->module_list || !(list = fopen(opt->module_list, "r"))) return NULL;
    combined = asn1p_new();
    if(!combined) { fclose(list); return NULL; }
    while((len = getline(&line, &cap, list)) >= 0) {
        char *name = trim(line), *path;
        asn1p_t *part;
        asn1p_module_t *mod;
        size_t n;
        (void)len;
        if(!*name || *name == '#') continue;
        if(opt->asn1_root && *opt->asn1_root && name[0] != '/') {
            n = strlen(opt->asn1_root) + strlen(name) + 2;
            path = malloc(n);
            if(!path) goto fail;
            snprintf(path, n, "%s/%s", opt->asn1_root, name);
        } else path = strdup(name);
        if(!path) goto fail;
        if(failed_file) {
            snprintf(failed_path, sizeof(failed_path), "%s", path);
            *failed_file = failed_path;
        }
        part = asn1p_parse_file(path, A1P_NOFLAGS);
        if(!part) { free(path); goto fail; }
        free(path);
        while((mod = TQ_REMOVE(&part->modules, mod_next))) {
            mod->asn1p = combined;
            TQ_ADD(&combined->modules, mod, mod_next);
        }
        asn1p_delete(part);
    }
    free(line);
    if(ferror(list)) goto fail_closed;
    fclose(list);
    if(!TQ_FIRST(&combined->modules)) { asn1p_delete(combined); return NULL; }
    if(failed_file) *failed_file = NULL;
    return combined;
fail:
    free(line);
fail_closed:
    fclose(list);
    if(combined) asn1p_delete(combined);
    return NULL;
}

int dev_tree_fix(asn1p_t *tree) { return asn1f_process(tree, A1F_NOFLAGS, NULL); }
