#include "asn1typed.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static char *
asn1typed_strdup(const char *value) {
	size_t length;
	char *copy;
	if(!value) return NULL;
	length = strlen(value);
	if(length == SIZE_MAX) return NULL;
	copy = (char *)malloc(length + 1);
	if(copy) memcpy(copy, value, length + 1);
	return copy;
}

static int
asn1typed_reserve(void **data, size_t *capacity, size_t count,
		size_t item_size) {
	size_t next;
	void *grown;
	if(count <= *capacity) return 0;
	next = *capacity ? *capacity : 4;
	while(next < count) {
		if(next > SIZE_MAX / 2) { next = count; break; }
		next *= 2;
	}
	if(item_size && next > SIZE_MAX / item_size) return -1;
	grown = realloc(*data, next * item_size);
	if(!grown) return -1;
	*data = grown;
	*capacity = next;
	return 0;
}

int
asn1typed_source_location_init(asn1typed_source_location_t *location,
		const char *file, unsigned line) {
	char *copy;
	if(!location || !file) return -1;
	copy = asn1typed_strdup(file);
	if(!copy) return -1;
	location->file = copy;
	location->line = line;
	return 0;
}

void
asn1typed_source_location_clear(asn1typed_source_location_t *location) {
	if(!location) return;
	free(location->file);
	location->file = NULL;
	location->line = 0;
}

int
asn1typed_type_ref_init(asn1typed_type_ref_t *ref,
		const char *module, const char *source_name) {
	char *module_copy, *name_copy;
	if(!ref || !module || !source_name) return -1;
	module_copy = asn1typed_strdup(module);
	name_copy = asn1typed_strdup(source_name);
	if(!module_copy || !name_copy) {
		free(module_copy);
		free(name_copy);
		return -1;
	}
	ref->module = module_copy;
	ref->source_name = name_copy;
	ref->kind = ASN1TYPED_REF_NAMED;
	ref->primitive_kind = ASN1TYPED_PRIMITIVE_INVALID;
	return 0;
}

int
asn1typed_type_ref_init_primitive(asn1typed_type_ref_t *ref,
		asn1typed_primitive_kind_e primitive_kind) {
	if(!ref || primitive_kind <= ASN1TYPED_PRIMITIVE_INVALID ||
		primitive_kind > ASN1TYPED_PRIMITIVE_PRINTABLE_STRING) return -1;
	memset(ref, 0, sizeof(*ref));
	ref->kind = ASN1TYPED_REF_PRIMITIVE;
	ref->primitive_kind = primitive_kind;
	return 0;
}

void
asn1typed_type_ref_clear(asn1typed_type_ref_t *ref) {
	if(!ref) return;
	free(ref->module);
	free(ref->source_name);
	ref->module = ref->source_name = NULL;
	ref->kind = ASN1TYPED_REF_NAMED;
	ref->primitive_kind = ASN1TYPED_PRIMITIVE_INVALID;
}

void
asn1typed_type_clear(asn1typed_type_t *type) {
	size_t i;
	if(!type) return;
	free(type->identity.module);
	free(type->identity.source_name);
	asn1typed_source_location_clear(&type->location);
	for(i = 0; i < type->field_count; ++i) {
		free(type->fields[i].source_name);
		asn1typed_type_ref_clear(&type->fields[i].type);
		asn1typed_source_location_clear(&type->fields[i].location);
	}
	free(type->fields);
	asn1typed_type_ref_clear(&type->element_type);
	for(i = 0; i < type->enum_item_count; ++i) {
		free(type->enum_items[i].source_name);
		asn1typed_source_location_clear(&type->enum_items[i].location);
	}
	free(type->enum_items);
	memset(type, 0, sizeof(*type));
}

int
asn1typed_module_init(asn1typed_module_t *module,
		const char *source_name, const char *file, unsigned line) {
	char *name;
	if(!module || !source_name || !file) return -1;
	name = asn1typed_strdup(source_name);
	if(!name) return -1;
	memset(module, 0, sizeof(*module));
	module->source_name = name;
	if(asn1typed_source_location_init(&module->location, file, line)) {
		free(name);
		memset(module, 0, sizeof(*module));
		return -1;
	}
	return 0;
}

void
asn1typed_module_clear(asn1typed_module_t *module) {
	size_t i;
	if(!module) return;
	for(i = 0; i < module->type_count; ++i)
		asn1typed_type_clear(&module->types[i]);
	free(module->types);
	free(module->source_name);
	asn1typed_source_location_clear(&module->location);
	memset(module, 0, sizeof(*module));
}

int
asn1typed_module_add_type(asn1typed_module_t *module,
		const char *source_name, asn1typed_type_kind_e kind,
		const char *file, unsigned line, asn1typed_type_t **type_out) {
	asn1typed_type_t type;
	if(!module || !module->source_name || !source_name || !file ||
		kind < ASN1TYPED_TYPE_PRIMITIVE || kind > ASN1TYPED_TYPE_ENUMERATED)
		return -1;
	memset(&type, 0, sizeof(type));
	type.identity.module = asn1typed_strdup(module->source_name);
	type.identity.source_name = asn1typed_strdup(source_name);
	if(!type.identity.module || !type.identity.source_name) {
		asn1typed_type_clear(&type);
		return -1;
	}
	type.kind = kind;
	if(asn1typed_reserve((void **)&module->types, &module->type_capacity,
			module->type_count + 1, sizeof(*module->types)) ||
		asn1typed_source_location_init(&type.location, file, line)) {
		asn1typed_type_clear(&type);
		return -1;
	}
	module->types[module->type_count] = type;
	if(type_out) *type_out = &module->types[module->type_count];
	++module->type_count;
	return 0;
}

int
asn1typed_type_add_field(asn1typed_type_t *type,
		const char *source_name, const char *ref_module,
		const char *ref_source_name, asn1typed_presence_e presence,
		const char *file, unsigned line) {
	asn1typed_field_t field;
	if(!type || type->kind != ASN1TYPED_TYPE_SEQUENCE || !source_name ||
		!ref_module || !ref_source_name || !file ||
		presence < ASN1TYPED_PRESENCE_MANDATORY ||
		presence > ASN1TYPED_PRESENCE_CONDITIONAL) return -1;
	memset(&field, 0, sizeof(field));
	field.source_name = asn1typed_strdup(source_name);
	field.presence = presence;
	if(!field.source_name || asn1typed_type_ref_init(&field.type,
			ref_module, ref_source_name) ||
		asn1typed_source_location_init(&field.location, file, line) ||
		asn1typed_reserve((void **)&type->fields, &type->field_capacity,
			type->field_count + 1, sizeof(*type->fields))) {
		free(field.source_name);
		asn1typed_type_ref_clear(&field.type);
		asn1typed_source_location_clear(&field.location);
		return -1;
	}
	type->fields[type->field_count++] = field;
	return 0;
}

int
asn1typed_type_set_element_type(asn1typed_type_t *type,
		const char *ref_module, const char *ref_source_name) {
	asn1typed_type_ref_t ref;
	if(!type || type->kind != ASN1TYPED_TYPE_SEQUENCE_OF ||
		asn1typed_type_ref_init(&ref, ref_module, ref_source_name)) return -1;
	asn1typed_type_ref_clear(&type->element_type);
	type->element_type = ref;
	return 0;
}

int
asn1typed_type_set_primitive(asn1typed_type_t *type,
		asn1typed_primitive_kind_e primitive_kind) {
	if(!type || type->kind != ASN1TYPED_TYPE_PRIMITIVE ||
		primitive_kind <= ASN1TYPED_PRIMITIVE_INVALID ||
		primitive_kind > ASN1TYPED_PRIMITIVE_PRINTABLE_STRING) return -1;
	type->primitive_kind = primitive_kind;
	return 0;
}

int
asn1typed_type_set_element_primitive(asn1typed_type_t *type,
		asn1typed_primitive_kind_e primitive_kind) {
	asn1typed_type_ref_t ref;
	if(!type || type->kind != ASN1TYPED_TYPE_SEQUENCE_OF ||
		asn1typed_type_ref_init_primitive(&ref, primitive_kind)) return -1;
	asn1typed_type_ref_clear(&type->element_type);
	type->element_type = ref;
	return 0;
}

int
asn1typed_type_add_primitive_field(asn1typed_type_t *type,
		const char *source_name, asn1typed_primitive_kind_e primitive_kind,
		asn1typed_presence_e presence, const char *file, unsigned line) {
	asn1typed_field_t field;
	if(!type || type->kind != ASN1TYPED_TYPE_SEQUENCE || !source_name ||
		!file || presence < ASN1TYPED_PRESENCE_MANDATORY ||
		presence > ASN1TYPED_PRESENCE_CONDITIONAL) return -1;
	memset(&field, 0, sizeof(field));
	field.source_name = asn1typed_strdup(source_name);
	field.presence = presence;
	if(!field.source_name || asn1typed_type_ref_init_primitive(&field.type,
			primitive_kind) ||
		asn1typed_source_location_init(&field.location, file, line) ||
		asn1typed_reserve((void **)&type->fields, &type->field_capacity,
			type->field_count + 1, sizeof(*type->fields))) {
		free(field.source_name);
		asn1typed_type_ref_clear(&field.type);
		asn1typed_source_location_clear(&field.location);
		return -1;
	}
	type->fields[type->field_count++] = field;
	return 0;
}

int
asn1typed_type_add_enum_item(asn1typed_type_t *type,
		const char *source_name, const char *file, unsigned line) {
	asn1typed_enum_item_t item;
	if(!type || type->kind != ASN1TYPED_TYPE_ENUMERATED ||
		!source_name || !file) return -1;
	memset(&item, 0, sizeof(item));
	item.source_name = asn1typed_strdup(source_name);
	if(!item.source_name || asn1typed_source_location_init(&item.location,
			file, line) ||
		asn1typed_reserve((void **)&type->enum_items,
			&type->enum_item_capacity, type->enum_item_count + 1,
			sizeof(*type->enum_items))) {
		free(item.source_name);
		asn1typed_source_location_clear(&item.location);
		return -1;
	}
	type->enum_items[type->enum_item_count++] = item;
	return 0;
}
