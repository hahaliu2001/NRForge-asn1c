/* Read-only fixed-tree constraint/tag evidence; never changes producer state. */
#include "developer_tree.h"
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

static void string(const char *s) {
    putchar('"');
    for (const unsigned char *p = (const unsigned char *)(s ? s : ""); *p; ++p) {
        if (*p == '"' || *p == '\\') printf("\\%c", *p);
        else if (*p < 32) printf("\\u%04x", (unsigned)*p);
        else putchar(*p);
    }
    putchar('"');
}
static void value(const asn1p_value_t *v) {
    if (!v) { printf("null"); return; }
    printf("{\"type\":%d", (int)v->type);
    if (v->type == ATV_INTEGER) printf(",\"integer\":%jd", (intmax_t)v->value.v_integer);
    if (v->type == ATV_REFERENCED) {
        printf(",\"reference\":"); string(asn1p_ref_string(v->value.reference));
    }
    putchar('}');
}
static void constraint(const asn1p_constraint_t *c) {
    if (!c) { printf("null"); return; }
    printf("{\"kind\":"); string(asn1p_constraint_type2str(c->type));
    printf(",\"value\":"); value(c->value);
    printf(",\"lower\":"); value(c->range_start);
    printf(",\"upper\":"); value(c->range_stop);
    printf(",\"contained\":"); value(c->containedSubtype);
    printf(",\"children\":[");
    for (unsigned i = 0; i < c->el_count; ++i) {
        if (i) putchar(',');
        constraint(c->elements[i]);
    }
    printf("]}");
}
static void expression(const asn1p_expr_t *e) {
    asn1p_expr_t *child;
    int first = 1;
    printf("{\"module\":"); string(e->module ? e->module->ModuleName : NULL);
    printf(",\"name\":"); string(e->Identifier);
    printf(",\"line\":%d,\"kind\":", e->_lineno); string(ASN_EXPR_TYPE2STR(e->expr_type));
    printf(",\"tag_class\":%d,\"tag_number\":%jd,\"auto_tags_OK\":%d,\"reference\":",
           (int)e->tag.tag_class, (intmax_t)e->tag.tag_value, e->auto_tags_OK);
    string(e->reference ? asn1p_ref_string(e->reference) : NULL);
    const asn1p_expr_t *target = e->reference ? e->reference->ref_expr : NULL;
    printf(",\"reference_target_module\":");
    string(target && target->module ? target->module->ModuleName : NULL);
    printf(",\"reference_target_name\":"); string(target ? target->Identifier : NULL);
    printf(",\"declared\":"); constraint(e->constraints);
    printf(",\"combined\":"); constraint(e->combined_constraints);
    printf(",\"children\":[");
    TQ_FOR(child, &e->members, next) {
        if (!first) putchar(',');
        first = 0; expression(child);
    }
    printf("]}");
}
int main(int argc, char **argv) {
    if (argc != 3) return 2;
    dev_tree_options_t options = {.module_list = argv[1], .asn1_root = argv[2]};
    const char *why = NULL;
    asn1p_t *tree = dev_tree_load(&options, &why);
    if (!tree) return 3;
    if (dev_tree_fix(tree)) { asn1p_delete(tree); return 3; }
    const char *names[] = {"SRB-Identity-v1700", "MCCH-RepetitionPeriodAndOffset-r17",
        "SRS-PosRRC-Inactive-r17", "MobilityHistoryReport-r16", "VisitedCellInfoList-r16", "SetupRelease"};
    asn1p_module_t *m; asn1p_expr_t *d;
    int first = 1;
    printf("{\"read_only\":true,\"declarations\":[");
    TQ_FOR(m, &tree->modules, mod_next) TQ_FOR(d, &m->members, next) {
        if (strcmp(m->ModuleName, "NR-RRC-Definitions")) continue;
        int selected = 0;
        for (size_t i = 0; i < sizeof(names)/sizeof(names[0]); ++i)
            if (!strcmp(d->Identifier, names[i])) selected = 1;
        if (!selected) continue;
        if (!first) putchar(',');
        first = 0;
        printf("{\"module\":"); string(m->ModuleName);
        printf(",\"declaration\":"); expression(d);
        printf(",\"specializations\":[");
        for (int i = 0; i < d->specializations.pspecs_count; ++i) {
            if (i) putchar(',');
            printf("{\"actuals\":"); expression(d->specializations.pspec[i].rhs_pspecs);
            printf(",\"clone\":"); expression(d->specializations.pspec[i].my_clone);
            putchar('}');
        }
        printf("]}");
    }
    printf("]}\n"); asn1p_delete(tree); return ferror(stdout) ? 4 : 0;
}
