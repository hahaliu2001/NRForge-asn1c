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
	memset(ref, 0, sizeof(*ref));
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
asn1typed_type_ref_init_parameterized(asn1typed_type_ref_t *ref,
		const char *module, const char *source_name,
		const asn1typed_type_actual_t *actuals, size_t actual_count) {
	size_t i;
	if(!ref || !module || !source_name || !actuals || !actual_count) return -1;
	memset(ref, 0, sizeof(*ref));
	if(asn1typed_type_ref_init(ref, module, source_name)) return -1;
	ref->actuals = (asn1typed_type_actual_t *)calloc(actual_count,
		sizeof(*ref->actuals));
	if(!ref->actuals) goto fail;
	for(i = 0; i < actual_count; ++i) {
		if(actuals[i].kind != ASN1TYPED_ACTUAL_OBJECT_SET_REFERENCE ||
			!actuals[i].module || !actuals[i].source_name) goto fail;
		ref->actual_count = i + 1;
		ref->actuals[i].module = asn1typed_strdup(actuals[i].module);
		ref->actuals[i].source_name = asn1typed_strdup(actuals[i].source_name);
		if(!ref->actuals[i].module || !ref->actuals[i].source_name) goto fail;
		ref->actuals[i].kind = actuals[i].kind;
	}
	return 0;
fail:
	asn1typed_type_ref_clear(ref);
	return -1;
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

int
asn1typed_type_ref_copy(asn1typed_type_ref_t *ref,
		const asn1typed_type_ref_t *source) {
	if(!ref || !source) return -1;
	if(source->kind == ASN1TYPED_REF_PRIMITIVE && !source->actual_count)
		return asn1typed_type_ref_init_primitive(ref, source->primitive_kind);
	if(source->kind == ASN1TYPED_REF_NAMED && source->actual_count == 0)
		return asn1typed_type_ref_init(ref, source->module, source->source_name);
	if(source->kind == ASN1TYPED_REF_NAMED && source->actuals &&
			source->actual_count)
		return asn1typed_type_ref_init_parameterized(ref, source->module,
			source->source_name, source->actuals, source->actual_count);
	return -1;
}

void
asn1typed_type_ref_clear(asn1typed_type_ref_t *ref) {
	size_t i;
	if(!ref) return;
	for(i = 0; i < ref->actual_count; ++i) {
		free(ref->actuals[i].module);
		free(ref->actuals[i].source_name);
	}
	free(ref->actuals);
	free(ref->module);
	free(ref->source_name);
	ref->module = ref->source_name = NULL;
	ref->actuals = NULL;
	ref->actual_count = 0;
	ref->kind = ASN1TYPED_REF_NAMED;
	ref->primitive_kind = ASN1TYPED_PRIMITIVE_INVALID;
}

int
asn1typed_type_ref_equal(const asn1typed_type_ref_t *left,
		const asn1typed_type_ref_t *right) {
	size_t i;
	if(!left || !right || left->kind != right->kind) return 0;
	if(left->kind == ASN1TYPED_REF_PRIMITIVE)
		return left->primitive_kind == right->primitive_kind &&
			left->actual_count == 0 && right->actual_count == 0;
	if(!left->module || !right->module || !left->source_name ||
		!right->source_name || strcmp(left->module, right->module) ||
		strcmp(left->source_name, right->source_name) ||
		left->actual_count != right->actual_count ||
		(left->actual_count && (!left->actuals || !right->actuals))) return 0;
	for(i = 0; i < left->actual_count; ++i)
		if(left->actuals[i].kind != right->actuals[i].kind ||
			!left->actuals[i].module || !right->actuals[i].module ||
			!left->actuals[i].source_name || !right->actuals[i].source_name ||
			strcmp(left->actuals[i].module, right->actuals[i].module) ||
			strcmp(left->actuals[i].source_name,
				right->actuals[i].source_name)) return 0;
	return 1;
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
		free(type->fields[i].ioc.symbolic_id);
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
	for(i = 0; i < type->alternative_count; ++i) {
		free(type->alternatives[i].source_name);
		asn1typed_type_ref_clear(&type->alternatives[i].type_ref);
		asn1typed_source_location_clear(&type->alternatives[i].location);
	}
	free(type->alternatives);
	memset(type, 0, sizeof(*type));
}

int
asn1typed_field_set_ioc(asn1typed_field_t *field,
		const char *symbolic_id, asn1typed_criticality_e criticality,
		int has_numeric_id, intmax_t numeric_id) {
	char *copy;
	if(!field || !symbolic_id || !*symbolic_id ||
		criticality < ASN1TYPED_CRITICALITY_REJECT ||
		criticality > ASN1TYPED_CRITICALITY_NOTIFY) return -1;
	copy = asn1typed_strdup(symbolic_id);
	if(!copy) return -1;
	free(field->ioc.symbolic_id);
	field->ioc.symbolic_id = copy;
	field->ioc.criticality = criticality;
	field->ioc.has_numeric_id = !!has_numeric_id;
	field->ioc.numeric_id = has_numeric_id ? numeric_id : 0;
	return 0;
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
	for(i = 0; i < module->bound_instance_count; ++i)
		asn1typed_type_ref_clear(&module->bound_instances[i].identity);
	free(module->bound_instances);
	free(module->source_name);
	asn1typed_source_location_clear(&module->location);
	memset(module, 0, sizeof(*module));
}

int
asn1typed_module_add_bound_instance(asn1typed_module_t *module,
		const asn1typed_type_ref_t *identity,
		asn1typed_bound_instance_t **instance_out) {
	asn1typed_bound_instance_t pending;
	size_t i;
	if(instance_out) *instance_out = NULL;
	if(!module || !identity || identity->kind != ASN1TYPED_REF_NAMED ||
		!identity->module || !identity->module[0] ||
		!identity->source_name || !identity->source_name[0] ||
		!identity->actual_count || !identity->actuals) return -1;
	for(i = 0; i < identity->actual_count; ++i)
		if(identity->actuals[i].kind != ASN1TYPED_ACTUAL_OBJECT_SET_REFERENCE ||
			!identity->actuals[i].module || !identity->actuals[i].module[0] ||
			!identity->actuals[i].source_name ||
			!identity->actuals[i].source_name[0]) return -1;
	for(i = 0; i < module->bound_instance_count; ++i)
		if(asn1typed_type_ref_equal(&module->bound_instances[i].identity,
				identity)) {
			if(instance_out) *instance_out = &module->bound_instances[i];
			return 0;
		}
	memset(&pending, 0, sizeof(pending));
	if(asn1typed_type_ref_copy(&pending.identity, identity)) return -1;
	pending.body_materialized = 0;
	if(asn1typed_reserve((void **)&module->bound_instances,
			&module->bound_instance_capacity,
			module->bound_instance_count + 1,
			sizeof(*module->bound_instances))) {
		asn1typed_type_ref_clear(&pending.identity);
		return -1;
	}
	module->bound_instances[module->bound_instance_count] = pending;
	if(instance_out)
		*instance_out = &module->bound_instances[module->bound_instance_count];
	module->bound_instance_count++;
	return 0;
}

int
asn1typed_module_add_type(asn1typed_module_t *module,
		const char *source_name, asn1typed_type_kind_e kind,
		const char *file, unsigned line, asn1typed_type_t **type_out) {
	if(!module) return -1;
	return asn1typed_module_add_type_identity(module, module->source_name,
		source_name, kind, file, line, type_out);
}

int
asn1typed_module_add_type_identity(asn1typed_module_t *module,
		const char *module_name, const char *source_name,
		asn1typed_type_kind_e kind, const char *file, unsigned line,
		asn1typed_type_t **type_out) {
	asn1typed_type_t type;
	if(!module || !module->source_name || !module_name || !source_name || !file ||
		kind < ASN1TYPED_TYPE_PRIMITIVE || kind > ASN1TYPED_TYPE_CHOICE)
		return -1;
	memset(&type, 0, sizeof(type));
	type.identity.module = asn1typed_strdup(module_name);
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
asn1typed_type_add_field_ref(asn1typed_type_t *type,
		const char *source_name, const asn1typed_type_ref_t *type_ref,
		asn1typed_presence_e presence, const char *file, unsigned line) {
	asn1typed_field_t field;
	if(!type || type->kind != ASN1TYPED_TYPE_SEQUENCE || !source_name ||
		!type_ref || !file || presence < ASN1TYPED_PRESENCE_MANDATORY ||
		presence > ASN1TYPED_PRESENCE_CONDITIONAL) return -1;
	memset(&field, 0, sizeof(field));
	field.source_name = asn1typed_strdup(source_name);
	field.presence = presence;
	if(!field.source_name || asn1typed_type_ref_copy(&field.type, type_ref) ||
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

int
asn1typed_type_add_choice_alternative(asn1typed_type_t *type,
		const char *source_name, const asn1typed_type_ref_t *type_ref,
		const char *file, unsigned line) {
	asn1typed_choice_alternative_t alternative;
	if(!type || type->kind != ASN1TYPED_TYPE_CHOICE || !source_name ||
		!type_ref || !file) return -1;
	memset(&alternative, 0, sizeof(alternative));
	alternative.source_name = asn1typed_strdup(source_name);
	if(!alternative.source_name) return -1;
	if(type_ref->kind == ASN1TYPED_REF_PRIMITIVE && !type_ref->actual_count) {
		if(asn1typed_type_ref_init_primitive(&alternative.type_ref,
				type_ref->primitive_kind)) goto fail;
	} else if(type_ref->kind == ASN1TYPED_REF_NAMED &&
		type_ref->actual_count == 0) {
		if(asn1typed_type_ref_init(&alternative.type_ref, type_ref->module,
				type_ref->source_name)) goto fail;
	} else if(type_ref->kind == ASN1TYPED_REF_NAMED &&
		type_ref->actual_count && type_ref->actuals &&
		!asn1typed_type_ref_init_parameterized(&alternative.type_ref,
				type_ref->module, type_ref->source_name,
				type_ref->actuals, type_ref->actual_count)) {
		/* The reference constructor deep-copies the ordered actual list. */
	} else goto fail;
	if(asn1typed_source_location_init(&alternative.location, file, line) ||
		asn1typed_reserve((void **)&type->alternatives,
			&type->alternative_capacity, type->alternative_count + 1,
			sizeof(*type->alternatives))) goto fail;
	type->alternatives[type->alternative_count++] = alternative;
	return 0;
fail:
	free(alternative.source_name);
	asn1typed_type_ref_clear(&alternative.type_ref);
	asn1typed_source_location_clear(&alternative.location);
	return -1;
}
