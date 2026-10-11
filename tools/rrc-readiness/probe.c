/* RRC-P1: read-only fixed-AST inventory and existing API profile probes.
 * No renderer or wire codec is invoked. JSON is analysis input, not typed IR. */
#include <asn1typed_extract.h>
#include "developer_tree.h"
#include <stdio.h>
#include <string.h>

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
static void constraint(const asn1p_constraint_t *c) {
    unsigned i;
    if(!c) { printf("null"); return; }
    printf("{\"kind\":"); string(asn1p_constraint_type2str(c->type));
    printf(",\"children\":[");
    for(i = 0; i < c->el_count; ++i) {
        if(i) putchar(',');
        constraint(c->elements[i]);
    }
    printf("]}");
}
static void expression(const asn1p_expr_t *e) {
    asn1p_expr_t *child;
    int first = 1;
    const char *kind = ASN_EXPR_TYPE2STR(e->expr_type);
    if(e->expr_type == A1TC_REFERENCE) kind = "REFERENCE";
    if(e->expr_type == A1TC_EXTENSIBLE) kind = "EXTENSION_MARKER";
    printf("{\"name\":"); string(e->Identifier);
    printf(",\"kind\":"); string(kind);
    printf(",\"meta\":%d,\"line\":%d,\"default\":%s,\"optional\":%s,\"template\":%s,\"specializations\":%d,\"reference\":",
        (int)e->meta_type, e->_lineno,
        (e->marker.flags & EM_DEFAULT) == EM_DEFAULT ? "true" : "false",
        (e->marker.flags & EM_DEFAULT) != EM_DEFAULT && (e->marker.flags & EM_OPTIONAL) == EM_OPTIONAL ? "true" : "false",
        e->lhs_params ? "true" : "false", e->specializations.pspecs_count);
    string(e->reference ? asn1p_ref_string(e->reference) : NULL);
    printf(",\"constraint\":"); constraint(e->constraints);
    printf(",\"actuals\":");
    if(e->rhs_pspecs) expression(e->rhs_pspecs); else printf("null");
    printf(",\"members\":[");
    TQ_FOR(child, &e->members, next) {
        if(!first) putchar(',');
        first = 0; expression(child);
    }
    printf("]}");
}
static void gate(asn1p_t *tree, const char *module, const char *root) {
    asn1typed_module_t ir = {0};
    char error[1024] = {0};
    int rc = root ? asn1typed_extract_physical_message(tree, module, root, &ir, error, sizeof(error))
                  : asn1typed_extract_module(tree, module, &ir, error, sizeof(error));
    printf("{\"module\":"); string(module);
    printf(",\"root\":"); string(root);
    printf(",\"api\":"); string(root ? "physical_single_container_IOC" : "whole_module");
    printf(",\"rc\":%d,\"types\":%zu,\"error\":", rc, ir.type_count); string(error); putchar('}');
    asn1typed_module_clear(&ir);
}
int main(int argc, char **argv) {
    static const char *const roots[][2] = {
        {"NR-RRC-Definitions", "BCCH-BCH-Message"},
        {"NR-RRC-Definitions", "BCCH-DL-SCH-Message"},
        {"NR-RRC-Definitions", "DL-CCCH-Message"},
        {"NR-RRC-Definitions", "DL-DCCH-Message"},
        {"NR-RRC-Definitions", "MCCH-Message-r17"},
        {"NR-RRC-Definitions", "MulticastMCCH-Message-r18"},
        {"NR-RRC-Definitions", "PCCH-Message"},
        {"NR-RRC-Definitions", "UL-CCCH-Message"},
        {"NR-RRC-Definitions", "UL-CCCH1-Message"},
        {"NR-RRC-Definitions", "UL-DCCH-Message"},
        {"PC5-RRC-Definitions", "SBCCH-SL-BCH-Message"},
        {"PC5-RRC-Definitions", "SCCH-Message"},
        {"NR-RRC-Definitions", "MIB"},
        {"NR-RRC-Definitions", "SIB1"},
        {"NR-RRC-Definitions", "SIB2"},
        {"NR-InterNodeDefinitions", "MeasurementTimingConfiguration"}
    };
    dev_tree_options_t options;
    const char *failed = NULL;
    asn1p_t *tree;
    asn1p_module_t *module;
    size_t i;
    int first = 1;
    if(argc != 3) { fprintf(stderr, "Usage: probe MODULE_LIST ASN1_ROOT\n"); return 2; }
    options.module_list = argv[1]; options.asn1_root = argv[2];
    tree = dev_tree_load(&options, &failed);
    if(!tree) { fprintf(stderr, "parse failed: %s\n", failed ? failed : "unknown"); return 3; }
    if(dev_tree_fix(tree)) { asn1p_delete(tree); return 4; }
    printf("{\"parse_fix\":\"PASS\",\"modules\":[");
    TQ_FOR(module, &tree->modules, mod_next) {
        asn1p_expr_t *e;
        int member_first = 1;
        if(!first) putchar(',');
        first = 0;
        printf("{\"name\":"); string(module->ModuleName);
        printf(",\"imports\":[");
        {
            asn1p_xports_t *xp;
            int import_first = 1;
            TQ_FOR(xp, &module->imports, xp_next) {
                TQ_FOR(e, &xp->xp_members, next) {
                    if(!import_first) putchar(',');
                    import_first = 0;
                    printf("{\"name\":"); string(e->Identifier);
                    printf(",\"module\":"); string(xp->fromModuleName); putchar('}');
                }
            }
        }
        printf("],\"declarations\":[");
        TQ_FOR(e, &module->members, next) {
            if(!member_first) putchar(',');
            member_first = 0; expression(e);
        }
        printf("]}");
    }
    printf("],\"gates\":["); first = 1;
    TQ_FOR(module, &tree->modules, mod_next) {
        if(!first) putchar(',');
        first = 0; gate(tree, module->ModuleName, NULL);
    }
    for(i = 0; i < sizeof(roots)/sizeof(roots[0]); ++i) {
        putchar(','); gate(tree, roots[i][0], roots[i][1]);
    }
    printf("]}\n");
    asn1p_delete(tree);
    return ferror(stdout) ? 5 : 0;
}
