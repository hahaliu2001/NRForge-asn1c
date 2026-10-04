#include "asn1typed_name.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static int lower(unsigned char c) { return c >= 'a' && c <= 'z'; }
static int upper(unsigned char c) { return c >= 'A' && c <= 'Z'; }
static int digit(unsigned char c) { return c >= '0' && c <= '9'; }
static int letter(unsigned char c) { return lower(c) || upper(c); }

const char *
asn1typed_name_ioc_identity(const char *source) {
	return source && !strncmp(source, "id-", 3) ? source + 3 : source;
}

/* The sole tokenizer. Spans borrow the validated input only during make(). */
static const char *
next_token(const char **cursor, size_t *length) {
	const char *start, *p = *cursor;
	while(*p == '-') ++p;
	start = p;
	if(*p) ++p;
	while(*p && *p != '-') {
		if((lower(p[-1]) && upper(*p)) ||
			(upper(p[-1]) && upper(*p) && lower(p[1])) ||
			(digit(p[-1]) != digit(*p))) break;
		++p;
	}
	*length = (size_t)(p - start);
	*cursor = p;
	return start;
}

asn1typed_name_result_e
asn1typed_name_make(const char *source, asn1typed_name_style_e style,
		char **out) {
	const char *p, *token;
	char *result;
	size_t n, length, used = 0, i;
	if(!out) return ASN1TYPED_NAME_INVALID;
	*out = NULL;
	if(!source || style < ASN1TYPED_NAME_TYPE ||
		style > ASN1TYPED_NAME_MODULE) return ASN1TYPED_NAME_INVALID;
	if(style == ASN1TYPED_NAME_IOC_FIELD)
		source = asn1typed_name_ioc_identity(source);
	for(p = source; *p; ++p)
		if(!letter(*p) && !digit(*p) && *p != '-')
			return ASN1TYPED_NAME_INVALID;
	n = (size_t)(p - source);
	p = source;
	token = next_token(&p, &length);
	if(!length || !letter(*token)) return ASN1TYPED_NAME_INVALID;
	if(n > (SIZE_MAX - 1) / 2) return ASN1TYPED_NAME_NOMEM;
	result = (char *)malloc(2 * n + 1);
	if(!result) return ASN1TYPED_NAME_NOMEM;
	do {
		if(used && style != ASN1TYPED_NAME_TYPE) result[used++] = '_';
		for(i = 0; i < length; ++i) {
			unsigned char c = (unsigned char)token[i];
			if(upper(c)) c = (unsigned char)(c - 'A' + 'a');
			if(style == ASN1TYPED_NAME_TYPE && i == 0 && lower(c))
				c = (unsigned char)(c - 'a' + 'A');
			result[used++] = (char)c;
		}
		token = next_token(&p, &length);
	} while(length);
	result[used] = '\0';
	*out = result;
	return ASN1TYPED_NAME_OK;
}

struct asn1typed_name_entry_s {
	char *source;
	char *name;
	struct asn1typed_name_entry_s *next;
};

asn1typed_name_result_e
asn1typed_name_scope_add(asn1typed_name_scope_t *scope,
		const char *source_key, const char *name) {
	struct asn1typed_name_entry_s *entry;
	const char *p;
	size_t source_length, name_length;
	if(!scope || !source_key || !*source_key || !name || !letter(*name))
		return ASN1TYPED_NAME_INVALID;
	for(p = name; *p; ++p)
		if(!letter(*p) && !digit(*p) && *p != '_')
			return ASN1TYPED_NAME_INVALID;
	for(entry = scope->entries; entry; entry = entry->next) {
		if(!strcmp(entry->name, name))
			return !strcmp(entry->source, source_key) ? ASN1TYPED_NAME_OK
				: ASN1TYPED_NAME_COLLISION;
	}
	source_length = strlen(source_key);
	name_length = strlen(name);
	if(source_length == SIZE_MAX || name_length == SIZE_MAX)
		return ASN1TYPED_NAME_NOMEM;
	entry = (struct asn1typed_name_entry_s *)calloc(1, sizeof(*entry));
	if(!entry) return ASN1TYPED_NAME_NOMEM;
	entry->source = (char *)malloc(source_length + 1);
	entry->name = (char *)malloc(name_length + 1);
	if(!entry->source || !entry->name) {
		free(entry->source);
		free(entry->name);
		free(entry);
		return ASN1TYPED_NAME_NOMEM;
	}
	memcpy(entry->source, source_key, source_length + 1);
	memcpy(entry->name, name, name_length + 1);
	entry->next = scope->entries;
	scope->entries = entry;
	return ASN1TYPED_NAME_OK;
}

void
asn1typed_name_scope_clear(asn1typed_name_scope_t *scope) {
	struct asn1typed_name_entry_s *entry, *next;
	if(!scope) return;
	for(entry = scope->entries; entry; entry = next) {
		next = entry->next;
		free(entry->source);
		free(entry->name);
		free(entry);
	}
	scope->entries = NULL;
}
