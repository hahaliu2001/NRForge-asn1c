/* Read-only batch readiness evidence; successful generation is not qualification. */
#include <asn1typed_extract.h>
#include <asn1typed_render_cpp.h>
#include "developer_tree.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_MESSAGES 128
struct result {
	char name[128], extraction_error[1024], envelope_error[1024];
	asn1typed_module_t body;
	asn1typed_target_envelope_t envelope;
	int extraction_rc, envelope_rc;
};
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
static void range(const asn1typed_integer_value_range_t *r) {
	printf("{\"present\":%d,\"lower\":%jd,\"upper\":%jd,\"extensible\":%d,\"tail_count\":%zu}",
		r->has_value_range, r->lower_bound, r->upper_bound, r->is_extensible, r->tail_count);
}
static void size(const asn1typed_size_constraint_t *s) {
	printf("{\"present\":%d,\"lower\":%jd,\"upper\":%jd,\"extensible\":%d}",
		s->has_size_constraint, s->lower_bound, s->upper_bound, s->is_extensible);
}
static void inventory(const asn1typed_module_t *m) {
	size_t i, j;
	printf("[");
	for(i = 0; i < m->type_count; ++i) {
		const asn1typed_type_t *t = &m->types[i];
		if(i) putchar(',');
		printf("{\"module\":"); string(t->identity.module);
		printf(",\"name\":"); string(t->identity.source_name);
		printf(",\"kind\":%u,\"primitive_kind\":%u,\"extensible\":%d,\"range\":",
			(unsigned)t->kind, (unsigned)t->primitive_kind, t->is_extensible);
		range(&t->value_range); printf(",\"size\":"); size(&t->size_constraint);
		printf(",\"fields\":[");
		for(j = 0; j < t->field_count; ++j) {
			const asn1typed_field_t *f = &t->fields[j];
			if(j) putchar(',');
			printf("{\"name\":"); string(f->source_name);
			printf(",\"semantics\":%u,\"presence\":%u,\"inline_enum\":%s,\"range\":",
				(unsigned)f->type_semantics, (unsigned)f->presence, f->inline_enumerated ? "true" : "false");
			range(&f->value_range); printf(",\"size\":"); size(&f->size_constraint); putchar('}');
		}
		printf("],\"alternatives\":[");
		for(j = 0; j < t->alternative_count; ++j) {
			if(j) putchar(',');
			printf("{\"name\":"); string(t->alternatives[j].source_name);
			printf(",\"range\":"); range(&t->alternatives[j].value_range);
			printf(",\"size\":"); size(&t->alternatives[j].size_constraint); putchar('}');
		}
		printf("]}");
	}
	putchar(']');
}
int main(int argc, char **argv) {
	struct result *rows;
	dev_tree_options_t options;
	asn1p_t *tree;
	const char *failed = NULL;
	FILE *file;
	char line[256];
	size_t n = 0, i, q;
	int (*render[])(const asn1typed_module_t *, const char *, char **, char *, size_t) = {
		asn1typed_render_cpp_owned_ioc_types, asn1typed_render_cpp_owned_ioc_mapping, asn1typed_render_cpp_owned_ioc_codec
	};
	if(argc != 4) { fprintf(stderr, "Usage: probe MODULE_LIST ASN1_ROOT MESSAGE_LIST\n"); return 2; }
	rows = calloc(MAX_MESSAGES, sizeof(*rows)); if(!rows) return 3;
	file = fopen(argv[3], "r"); if(!file) { free(rows); return 2; }
	while(fgets(line, sizeof(line), file)) {
		size_t len = strcspn(line, "\r\n");
		if(!len || line[0] == '#') continue;
		if(n == MAX_MESSAGES || len >= sizeof(rows[n].name) || (line[len] == '\0' && !feof(file))) {
			fclose(file); free(rows); return 2;
		}
		memcpy(rows[n].name, line, len); rows[n].name[len] = '\0'; ++n;
	}
	if(ferror(file) || !n) { fclose(file); free(rows); return 2; }
	fclose(file);
	options.module_list = argv[1]; options.asn1_root = argv[2];
	tree = dev_tree_load(&options, &failed);
	if(!tree) { fprintf(stderr, "parse failed: %s\n", failed ? failed : "unknown"); free(rows); return 3; }
	if(dev_tree_fix(tree) < 0) { asn1p_delete(tree); free(rows); return 4; }
	for(i = 0; i < n; ++i) {
		rows[i].extraction_rc = asn1typed_extract_physical_message(tree, "NGAP-PDU-Contents", rows[i].name,
			&rows[i].body, rows[i].extraction_error, sizeof(rows[i].extraction_error));
		rows[i].envelope_rc = asn1typed_extract_target_envelope(tree, "NGAP-PDU-Descriptions", "NGAP-PDU",
			"NGAP-PDU-Contents", rows[i].name, &rows[i].envelope, rows[i].envelope_error, sizeof(rows[i].envelope_error));
	}
	asn1p_delete(tree);
	printf("{\"parse\":\"PASS\",\"fix\":\"PASS\",\"parser_deleted_before_generation\":true,\"messages\":[");
	for(i = 0; i < n; ++i) {
		struct result *r = &rows[i];
		if(i) putchar(',');
		printf("{\"message\":"); string(r->name);
		printf(",\"physical_extraction_rc\":%d,\"extraction_diagnostic\":", r->extraction_rc); string(r->extraction_error);
		printf(",\"owned_types\":%zu,\"bound_instances\":%zu,\"registries\":%zu,\"envelope_extraction_rc\":%d,\"envelope_diagnostic\":",
			r->body.type_count, r->body.bound_instance_count, r->body.ioc_registry_count, r->envelope_rc); string(r->envelope_error);
		printf(",\"generation\":[");
		if(!r->extraction_rc) for(q = 0; q < 3; ++q) {
			char *out = NULL, diag[1024] = {0};
			int rc = render[q](&r->body, "n12::coverage", &out, diag, sizeof(diag));
			if(q) putchar(',');
			printf("{\"family\":%zu,\"rc\":%d,\"diagnostic\":", q, rc); string(diag); putchar('}'); free(out);
		}
		printf("],\"inventory\":"); inventory(&r->body); putchar('}');
		asn1typed_module_clear(&r->body); asn1typed_target_envelope_clear(&r->envelope);
	}
	printf("]}\n"); free(rows); return 0;
}
