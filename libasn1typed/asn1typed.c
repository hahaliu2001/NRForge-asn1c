#include "asn1typed.h"

#include <stdint.h>
#include <stdio.h>
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
asn1typed_class_field_relation_valid(
		const asn1typed_class_field_relation_t *relation) {
	if(!relation || !relation->class_module || !relation->class_module[0] ||
		!relation->class_source_name || !relation->class_source_name[0] ||
		!relation->class_field_source_name ||
		!relation->class_field_source_name[0] ||
		(relation->has_selector != 0 && relation->has_selector != 1)) return 0;
	if(relation->has_selector)
		return relation->selector_source_name &&
			relation->selector_source_name[0];
	return relation->selector_source_name == NULL;
}

static int
asn1typed_inline_enum_body_valid(const asn1typed_type_t *body) {
	size_t i;
	int saw_extension_addition = 0;
	int saw_root = 0;
	if(!body || body->kind != ASN1TYPED_TYPE_ENUMERATED ||
		body->identity.module || body->identity.source_name ||
		body->primitive_kind != ASN1TYPED_PRIMITIVE_INVALID ||
		body->size_constraint.has_size_constraint ||
		body->value_range.has_value_range || body->value_range.tail ||
		body->value_range.tail_count || body->location.file ||
		body->fields || body->field_count || body->field_capacity ||
		body->alternatives || body->alternative_count ||
		body->alternative_capacity || body->element_type.module ||
		body->element_type.source_name || body->element_type.actuals ||
		body->element_type.actual_count || body->ioc_container.module ||
		body->ioc_container.source_name || body->ioc_container.actuals ||
		body->ioc_container.actual_count || body->has_ioc_table ||
		body->ioc_object_set_is_extensible || !body->enum_items ||
		!body->enum_item_count ||
		(body->is_extensible != 0 && body->is_extensible != 1)) return 0;
	for(i = 0; i < body->enum_item_count; ++i) {
		const asn1typed_enum_item_t *item = &body->enum_items[i];
		if(!item->source_name || !item->source_name[0] || !item->location.file ||
			(item->is_extension_addition != 0 &&
			 item->is_extension_addition != 1)) return 0;
		if(item->is_extension_addition) {
			if(!saw_root || !body->is_extensible) return 0;
			saw_extension_addition = 1;
		} else {
			if(saw_extension_addition) return 0;
			saw_root = 1;
		}
	}
	return saw_root;
}

static int
asn1typed_inline_enum_field_valid(const asn1typed_field_t *field) {
	return field && field->type_semantics ==
		ASN1TYPED_FIELD_INLINE_ENUMERATED && field->inline_enumerated &&
		!field->has_class_field_relation && !field->type.module &&
		!field->type.source_name && !field->type.actuals &&
		!field->type.actual_count && field->type.kind == ASN1TYPED_REF_NAMED &&
		field->type.primitive_kind == ASN1TYPED_PRIMITIVE_INVALID &&
		asn1typed_inline_enum_body_valid(field->inline_enumerated);
}

static void
asn1typed_class_field_relation_clear(
		asn1typed_class_field_relation_t *relation) {
	if(!relation) return;
	free(relation->class_module);
	free(relation->class_source_name);
	free(relation->class_field_source_name);
	free(relation->selector_source_name);
	memset(relation, 0, sizeof(*relation));
}

static int
asn1typed_class_field_relation_copy(
		asn1typed_class_field_relation_t *target,
		const asn1typed_class_field_relation_t *source) {
	if(!target || !asn1typed_class_field_relation_valid(source)) return -1;
	memset(target, 0, sizeof(*target));
	target->class_module = asn1typed_strdup(source->class_module);
	target->class_source_name = asn1typed_strdup(source->class_source_name);
	target->class_field_source_name =
		asn1typed_strdup(source->class_field_source_name);
	target->actual_index = source->actual_index;
	target->has_selector = source->has_selector;
	if(source->has_selector)
		target->selector_source_name =
			asn1typed_strdup(source->selector_source_name);
	if(!target->class_module || !target->class_source_name ||
		!target->class_field_source_name ||
		(source->has_selector && !target->selector_source_name)) {
		asn1typed_class_field_relation_clear(target);
		return -1;
	}
	return 0;
}

static void
asn1typed_field_clear(asn1typed_field_t *field) {
	if(!field) return;
	free(field->source_name);
	free(field->ioc.symbolic_id);
	asn1typed_type_ref_clear(&field->type);
	if(field->inline_enumerated) {
		asn1typed_type_clear(field->inline_enumerated);
		free(field->inline_enumerated);
	}
	if(field->has_class_field_relation)
		asn1typed_class_field_relation_clear(&field->class_field_relation);
	free(field->value_range.tail);
	asn1typed_source_location_clear(&field->location);
	memset(field, 0, sizeof(*field));
}

static int
asn1typed_integer_value_range_copy(asn1typed_integer_value_range_t *target,
		const asn1typed_integer_value_range_t *source) {
	size_t i;
	if(!target || !source ||
		(source->has_value_range != 0 && source->has_value_range != 1) ||
		(source->is_extensible != 0 && source->is_extensible != 1) ||
		(source->has_value_range && source->lower_bound > source->upper_bound) ||
		(!source->has_value_range && (source->lower_bound || source->upper_bound ||
		 source->is_extensible || source->tail || source->tail_count)) ||
		(source->tail_count && !source->tail) ||
		(!source->tail_count && source->tail) ||
		(source->tail_count && !source->has_value_range) ||
		source->tail_count > SIZE_MAX / sizeof(*source->tail)) return -1;
	for(i = 0; i < source->tail_count; ++i) {
		const asn1typed_integer_interval_t *interval = &source->tail[i];
		intmax_t previous_upper = i ? source->tail[i - 1].upper_bound :
			source->upper_bound;
		int adjacent = previous_upper != INTMAX_MAX &&
			interval->lower_bound == previous_upper + 1;
		if(interval->lower_bound > interval->upper_bound ||
		interval->lower_bound <= previous_upper || adjacent) return -1;
	}
	*target = *source;
	target->tail = NULL;
	if(source->tail_count) {
		target->tail = (asn1typed_integer_interval_t *)malloc(
			source->tail_count * sizeof(*source->tail));
		if(!target->tail) {
			memset(target, 0, sizeof(*target));
			return -1;
		}
		memcpy(target->tail, source->tail,
			source->tail_count * sizeof(*source->tail));
	}
	return 0;
}

static int
asn1typed_field_copy(asn1typed_field_t *target,
		const asn1typed_field_t *source) {
	if(!target || !source || !source->source_name || !source->source_name[0] ||
		(source->type_semantics != ASN1TYPED_FIELD_FIXED_TYPE &&
		 source->type_semantics != ASN1TYPED_FIELD_CLASS_FIELD_SELECTED_TYPE &&
		 source->type_semantics != ASN1TYPED_FIELD_INLINE_ENUMERATED) ||
		(source->has_class_field_relation != 0 &&
		 source->has_class_field_relation != 1) ||
		(source->has_class_field_relation &&
		 !asn1typed_class_field_relation_valid(&source->class_field_relation)) ||
		(source->type_semantics == ASN1TYPED_FIELD_CLASS_FIELD_SELECTED_TYPE &&
		 (!source->has_class_field_relation ||
		  source->type.kind != ASN1TYPED_REF_NAMED || source->type.module ||
		  source->type.source_name || source->type.actuals ||
		  source->type.actual_count ||
		  source->type.primitive_kind != ASN1TYPED_PRIMITIVE_INVALID)) ||
		(source->type_semantics == ASN1TYPED_FIELD_INLINE_ENUMERATED &&
		 !asn1typed_inline_enum_field_valid(source)) ||
		(source->type_semantics != ASN1TYPED_FIELD_INLINE_ENUMERATED &&
		 source->inline_enumerated)) return -1;
	memset(target, 0, sizeof(*target));
	target->source_name = asn1typed_strdup(source->source_name);
	target->type_semantics = source->type_semantics;
	target->size_constraint = source->size_constraint;
	target->presence = source->presence;
	target->ioc.criticality = source->ioc.criticality;
	target->ioc.has_numeric_id = source->ioc.has_numeric_id;
	target->ioc.numeric_id = source->ioc.numeric_id;
	if(!target->source_name || asn1typed_integer_value_range_copy(
			&target->value_range, &source->value_range)) goto fail;
	if(source->type_semantics == ASN1TYPED_FIELD_FIXED_TYPE &&
		!source->inline_enumerated &&
		asn1typed_type_ref_copy(&target->type, &source->type)) goto fail;
	if(source->inline_enumerated) {
		const asn1typed_type_t *body = source->inline_enumerated;
		size_t i;
		target->inline_enumerated = (asn1typed_type_t *)calloc(1,
			sizeof(*target->inline_enumerated));
		if(!target->inline_enumerated) goto fail;
		target->inline_enumerated->kind = ASN1TYPED_TYPE_ENUMERATED;
		target->inline_enumerated->is_extensible = body->is_extensible;
		for(i = 0; i < body->enum_item_count; ++i) {
			const asn1typed_enum_item_t *item = &body->enum_items[i];
			if(!item->source_name || !item->source_name[0] ||
				(item->is_extension_addition != 0 &&
				 item->is_extension_addition != 1) || !item->location.file ||
				asn1typed_type_add_enum_item_ex(target->inline_enumerated,
					item->source_name, item->is_extension_addition,
					item->location.file, item->location.line)) goto fail;
		}
	}
	if(source->has_class_field_relation) {
		if(asn1typed_class_field_relation_copy(&target->class_field_relation,
				&source->class_field_relation)) goto fail;
		target->has_class_field_relation = 1;
	}
	if(source->ioc.symbolic_id) {
		target->ioc.symbolic_id = asn1typed_strdup(source->ioc.symbolic_id);
		if(!target->ioc.symbolic_id) goto fail;
	}
	if(source->location.file && asn1typed_source_location_init(
			&target->location, source->location.file, source->location.line))
		goto fail;
	if(!source->location.file && source->location.line) goto fail;
	return 0;
fail:
	asn1typed_field_clear(target);
	return -1;
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
		primitive_kind > ASN1TYPED_PRIMITIVE_BIT_STRING) return -1;
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
		asn1typed_field_clear(&type->fields[i]);
	}
	free(type->fields);
	asn1typed_type_ref_clear(&type->element_type);
	asn1typed_type_ref_clear(&type->ioc_container);
	for(i = 0; i < type->enum_item_count; ++i) {
		free(type->enum_items[i].source_name);
		asn1typed_source_location_clear(&type->enum_items[i].location);
	}
	free(type->enum_items);
	free(type->value_range.tail);
	for(i = 0; i < type->alternative_count; ++i) {
		free(type->alternatives[i].source_name);
		asn1typed_type_ref_clear(&type->alternatives[i].type_ref);
		free(type->alternatives[i].value_range.tail);
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
	for(i = 0; i < module->bound_instance_count; ++i) {
		asn1typed_type_ref_clear(&module->bound_instances[i].identity);
		asn1typed_type_clear(&module->bound_instances[i].body);
	}
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
asn1typed_bound_instance_set_body(asn1typed_module_t *module,
		size_t instance_index, asn1typed_type_t *body) {
	asn1typed_bound_instance_t *instance;
	size_t i, j;
	if(!module || instance_index >= module->bound_instance_count || !body ||
		body->identity.module || body->identity.source_name) return -1;
	instance = &module->bound_instances[instance_index];
	if(instance->body_materialized || instance->body.kind != 0) return -1;
	if(body->kind == ASN1TYPED_TYPE_SEQUENCE) {
		if(body->field_count == 0) return -1;
	} else if(body->kind == ASN1TYPED_TYPE_SEQUENCE_OF) {
		const asn1typed_type_ref_t *element = &body->element_type;
		if(body->field_count || !body->size_constraint.has_size_constraint ||
			body->size_constraint.lower_bound > body->size_constraint.upper_bound ||
			element->kind != ASN1TYPED_REF_NAMED ||
			!element->module || !element->module[0] || !element->source_name ||
			!element->source_name[0] || element->actual_count != 1 ||
			!element->actuals ||
			element->actuals[0].kind != ASN1TYPED_ACTUAL_OBJECT_SET_REFERENCE ||
			!element->actuals[0].module || !element->actuals[0].module[0] ||
			!element->actuals[0].source_name ||
			!element->actuals[0].source_name[0]) return -1;
	} else {
		return -1;
	}
	for(i = 0; i < body->field_count; ++i) {
		const asn1typed_field_t *field = &body->fields[i];
		if(field->type_semantics != ASN1TYPED_FIELD_FIXED_TYPE &&
			field->type_semantics != ASN1TYPED_FIELD_CLASS_FIELD_SELECTED_TYPE &&
			field->type_semantics != ASN1TYPED_FIELD_INLINE_ENUMERATED)
			return -1;
		if(field->type_semantics == ASN1TYPED_FIELD_INLINE_ENUMERATED &&
			!asn1typed_inline_enum_field_valid(field)) return -1;
		if(field->type_semantics != ASN1TYPED_FIELD_INLINE_ENUMERATED &&
			field->inline_enumerated) return -1;
		if(field->type_semantics == ASN1TYPED_FIELD_CLASS_FIELD_SELECTED_TYPE &&
			(!field->has_class_field_relation || field->type.module ||
			 field->type.source_name || field->type.actuals || field->type.actual_count))
			return -1;
		if(!field->has_class_field_relation) continue;
		if(field->class_field_relation.actual_index >= instance->identity.actual_count)
			return -1;
		if(field->class_field_relation.has_selector) {
			int found = 0;
			for(j = 0; j < body->field_count; ++j)
				if(!strcmp(body->fields[j].source_name,
					field->class_field_relation.selector_source_name)) found = 1;
			if(!found) return -1;
		}
	}
	instance->body = *body;
	memset(body, 0, sizeof(*body));
	instance->body_materialized = 1;
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
asn1typed_type_add_class_field(asn1typed_type_t *type,
		const char *source_name,
		asn1typed_field_type_semantics_e type_semantics,
		const asn1typed_type_ref_t *type_ref,
		const asn1typed_class_field_relation_t *relation,
		asn1typed_presence_e presence, const char *file, unsigned line) {
	asn1typed_field_t field;
	if(!type || type->kind != ASN1TYPED_TYPE_SEQUENCE || !source_name ||
		!source_name[0] || !relation ||
		!asn1typed_class_field_relation_valid(relation) || !file ||
		presence < ASN1TYPED_PRESENCE_MANDATORY ||
		presence > ASN1TYPED_PRESENCE_CONDITIONAL ||
		(type_semantics != ASN1TYPED_FIELD_FIXED_TYPE &&
		 type_semantics != ASN1TYPED_FIELD_CLASS_FIELD_SELECTED_TYPE) ||
		(type_semantics == ASN1TYPED_FIELD_FIXED_TYPE && !type_ref) ||
		(type_semantics == ASN1TYPED_FIELD_CLASS_FIELD_SELECTED_TYPE && type_ref))
		return -1;
	memset(&field, 0, sizeof(field));
	field.source_name = asn1typed_strdup(source_name);
	field.type_semantics = type_semantics;
	field.presence = presence;
	if(!field.source_name ||
		(type_ref && asn1typed_type_ref_copy(&field.type, type_ref))) {
		asn1typed_field_clear(&field);
		return -1;
	}
	if(asn1typed_class_field_relation_copy(&field.class_field_relation,
			relation)) {
		asn1typed_field_clear(&field);
		return -1;
	}
	field.has_class_field_relation = 1;
	if(asn1typed_source_location_init(&field.location, file, line) ||
		asn1typed_reserve((void **)&type->fields, &type->field_capacity,
			type->field_count + 1, sizeof(*type->fields))) {
		asn1typed_field_clear(&field);
		return -1;
	}
	type->fields[type->field_count++] = field;
	return 0;
}

int
asn1typed_type_add_field_copy(asn1typed_type_t *type,
		const asn1typed_field_t *source) {
	asn1typed_field_t field;
	if(!type || type->kind != ASN1TYPED_TYPE_SEQUENCE || !source ||
		source->presence < ASN1TYPED_PRESENCE_MANDATORY ||
		source->presence > ASN1TYPED_PRESENCE_CONDITIONAL ||
		asn1typed_field_copy(&field, source)) return -1;
	if(asn1typed_reserve((void **)&type->fields, &type->field_capacity,
			type->field_count + 1, sizeof(*type->fields))) {
		asn1typed_field_clear(&field);
		return -1;
	}
	type->fields[type->field_count++] = field;
	return 0;
}

int
asn1typed_type_add_inline_enumerated_field(asn1typed_type_t *type,
		const char *source_name, const asn1typed_type_t *body,
		asn1typed_presence_e presence, const char *file, unsigned line) {
	asn1typed_field_t field;
	if(!type || type->kind != ASN1TYPED_TYPE_SEQUENCE || !body ||
		!asn1typed_inline_enum_body_valid(body) ||
		!source_name || !source_name[0] || !file ||
		presence < ASN1TYPED_PRESENCE_MANDATORY ||
		presence > ASN1TYPED_PRESENCE_OPTIONAL) return -1;
	memset(&field, 0, sizeof(field));
	field.source_name = asn1typed_strdup(source_name);
	field.presence = presence;
	field.inline_enumerated = (asn1typed_type_t *)calloc(1,
		sizeof(*field.inline_enumerated));
	if(!field.source_name || !field.inline_enumerated ||
		asn1typed_source_location_init(&field.location, file, line)) {
		asn1typed_field_clear(&field);
		return -1;
	}
	field.type_semantics = ASN1TYPED_FIELD_INLINE_ENUMERATED;
	field.inline_enumerated->kind = ASN1TYPED_TYPE_ENUMERATED;
	field.inline_enumerated->is_extensible = body->is_extensible;
	{
		size_t i;
		for(i = 0; i < body->enum_item_count; ++i) {
			const asn1typed_enum_item_t *item = &body->enum_items[i];
			if(!item->source_name || !item->source_name[0] ||
				!item->location.file ||
				(item->is_extension_addition != 0 &&
				 item->is_extension_addition != 1) ||
				asn1typed_type_add_enum_item_ex(field.inline_enumerated,
					item->source_name, item->is_extension_addition,
					item->location.file, item->location.line)) {
				asn1typed_field_clear(&field);
				return -1;
			}
		}
	}
	if(asn1typed_reserve((void **)&type->fields, &type->field_capacity,
			type->field_count + 1, sizeof(*type->fields))) {
		asn1typed_field_clear(&field);
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
		primitive_kind > ASN1TYPED_PRIMITIVE_BIT_STRING) return -1;
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
	return asn1typed_type_add_enum_item_ex(type, source_name, 0, file, line);
}

int
asn1typed_type_add_enum_item_ex(asn1typed_type_t *type,
		const char *source_name, int is_extension_addition,
		const char *file, unsigned line) {
	asn1typed_enum_item_t item;
	if(!type || type->kind != ASN1TYPED_TYPE_ENUMERATED ||
		!source_name || !file) return -1;
	memset(&item, 0, sizeof(item));
	item.is_extension_addition = !!is_extension_addition;
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
		const asn1typed_size_constraint_t *size_constraint,
		const asn1typed_integer_value_range_t *value_range,
		const char *file, unsigned line) {
	asn1typed_choice_alternative_t alternative;
	if(!type || type->kind != ASN1TYPED_TYPE_CHOICE || !source_name ||
		!source_name[0] || !type_ref || !file || !file[0] ||
		(type_ref->kind != ASN1TYPED_REF_PRIMITIVE &&
		 type_ref->kind != ASN1TYPED_REF_NAMED)) return -1;
	if(size_constraint && (!size_constraint->has_size_constraint ||
		size_constraint->lower_bound > size_constraint->upper_bound)) return -1;
	if(value_range && (!value_range->has_value_range || size_constraint ||
		type_ref->kind != ASN1TYPED_REF_PRIMITIVE ||
		type_ref->primitive_kind != ASN1TYPED_PRIMITIVE_INTEGER)) return -1;
	memset(&alternative, 0, sizeof(alternative));
	alternative.source_name = asn1typed_strdup(source_name);
	if(size_constraint) alternative.size_constraint = *size_constraint;
	if(!alternative.source_name || (value_range &&
		asn1typed_integer_value_range_copy(&alternative.value_range,
			value_range))) goto fail;
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
	type->has_valid_per_root_mapping = 0;
	{
		size_t i;
		for(i = 0; i < type->alternative_count; ++i) {
			type->alternatives[i].has_per_root_index = 0;
			type->alternatives[i].per_root_index = 0;
		}
	}
	return 0;
fail:
	free(alternative.source_name);
	asn1typed_type_ref_clear(&alternative.type_ref);
	free(alternative.value_range.tail);
	asn1typed_source_location_clear(&alternative.location);
	return -1;
}

static void
choice_wire_mapping_invalidate(asn1typed_type_t *choice) {
	size_t i;
	if(!choice) return;
	choice->has_valid_per_root_mapping = 0;
	for(i = 0; i < choice->alternative_count; ++i) {
		choice->alternatives[i].has_per_root_index = 0;
		choice->alternatives[i].per_root_index = 0;
	}
}

int
asn1typed_choice_alternative_set_wire_evidence(
		asn1typed_type_t *choice, size_t alternative_index,
		asn1typed_tag_class_e tag_class, intmax_t tag_number) {
	asn1typed_choice_alternative_t *alternative;
	if(!choice || choice->kind != ASN1TYPED_TYPE_CHOICE ||
		alternative_index >= choice->alternative_count || !choice->alternatives ||
		tag_class < ASN1TYPED_TAG_CLASS_UNIVERSAL ||
		tag_class > ASN1TYPED_TAG_CLASS_PRIVATE || tag_number < 0) return -1;
	choice_wire_mapping_invalidate(choice);
	alternative = &choice->alternatives[alternative_index];
	alternative->wire_evidence = ASN1TYPED_WIRE_EVIDENCE_RESOLVED;
	alternative->effective_tag_class = tag_class;
	alternative->effective_tag_number = tag_number;
	return 0;
}

void
asn1typed_choice_alternative_set_wire_unavailable(
		asn1typed_type_t *choice, size_t alternative_index) {
	asn1typed_choice_alternative_t *alternative;
	if(!choice || choice->kind != ASN1TYPED_TYPE_CHOICE ||
		alternative_index >= choice->alternative_count || !choice->alternatives) return;
	choice_wire_mapping_invalidate(choice);
	alternative = &choice->alternatives[alternative_index];
	alternative->wire_evidence = ASN1TYPED_WIRE_EVIDENCE_UNAVAILABLE;
	alternative->effective_tag_class = ASN1TYPED_TAG_CLASS_UNKNOWN;
	alternative->effective_tag_number = 0;
}

void
asn1typed_choice_alternative_set_wire_unsupported(
		asn1typed_type_t *choice, size_t alternative_index) {
	asn1typed_choice_alternative_t *alternative;
	if(!choice || choice->kind != ASN1TYPED_TYPE_CHOICE ||
		alternative_index >= choice->alternative_count || !choice->alternatives) return;
	choice_wire_mapping_invalidate(choice);
	alternative = &choice->alternatives[alternative_index];
	alternative->wire_evidence = ASN1TYPED_WIRE_EVIDENCE_UNSUPPORTED;
	alternative->effective_tag_class = ASN1TYPED_TAG_CLASS_UNKNOWN;
	alternative->effective_tag_number = 0;
}

static int
wire_tag_precedes(const asn1typed_choice_alternative_t *left,
		const asn1typed_choice_alternative_t *right) {
	if(left->effective_tag_class != right->effective_tag_class)
		return left->effective_tag_class < right->effective_tag_class;
	return left->effective_tag_number < right->effective_tag_number;
}

static int
choice_wire_evidence_check(const asn1typed_type_t *type,
		char *error, size_t error_size, int check_indexes) {
	size_t i, j;
	if(error && error_size) error[0] = '\0';
	if(!type || type->kind != ASN1TYPED_TYPE_CHOICE ||
		!type->alternative_count || !type->alternatives) {
		if(error && error_size) snprintf(error, error_size,
			"wire evidence requires a non-empty CHOICE");
		return -1;
	}
	if(type->is_extensible) {
		if(error && error_size) snprintf(error, error_size,
			"extensible CHOICE wire mapping is unsupported");
		return 1;
	}
	for(i = 0; i < type->alternative_count; ++i) {
		const asn1typed_choice_alternative_t *alt = &type->alternatives[i];
		if(!alt->source_name || !alt->source_name[0] ||
			alt->wire_evidence != ASN1TYPED_WIRE_EVIDENCE_RESOLVED ||
			(check_indexes && !alt->has_per_root_index) ||
			alt->effective_tag_class < ASN1TYPED_TAG_CLASS_UNIVERSAL ||
			alt->effective_tag_class > ASN1TYPED_TAG_CLASS_PRIVATE ||
			alt->effective_tag_number < 0) {
			if(error && error_size) snprintf(error, error_size,
				"CHOICE alternative %s has unavailable or unsupported wire evidence",
				alt->source_name ? alt->source_name : "<unnamed>");
			return 1;
		}
		if(check_indexes && alt->per_root_index >= type->alternative_count) {
			if(error && error_size) snprintf(error, error_size,
				"CHOICE alternative %s has out-of-range PER root index",
				alt->source_name);
			return -1;
		}
		for(j = 0; j < i; ++j) {
			const asn1typed_choice_alternative_t *prior = &type->alternatives[j];
			if(alt->effective_tag_class == prior->effective_tag_class &&
				alt->effective_tag_number == prior->effective_tag_number) {
				if(error && error_size) snprintf(error, error_size,
					"CHOICE alternatives %s and %s have duplicate effective tags",
					prior->source_name, alt->source_name);
				return 1;
			}
			if(check_indexes && alt->per_root_index == prior->per_root_index) {
				if(error && error_size) snprintf(error, error_size,
					"CHOICE alternatives %s and %s have duplicate PER root indexes",
					prior->source_name, alt->source_name);
				return 1;
			}
		}
	}
	if(!check_indexes) return 0;
	/* Unique indexes in [0,count) prove a complete continuous mapping. */
	for(i = 0; i < type->alternative_count; ++i) {
		size_t found = 0;
		for(j = 0; j < type->alternative_count; ++j) {
			if(type->alternatives[j].per_root_index == i) ++found;
		}
		if(found != 1) {
			if(error && error_size) snprintf(error, error_size,
				"CHOICE PER root index mapping is incomplete at index %zu", i);
			return 1;
		}
	}
	/* Check that the supplied index actually follows X.680 tag ordering. */
	for(i = 0; i < type->alternative_count; ++i) {
		const asn1typed_choice_alternative_t *alt = &type->alternatives[i];
		size_t before = 0;
		for(j = 0; j < type->alternative_count; ++j)
			if(wire_tag_precedes(&type->alternatives[j], alt)) ++before;
		if(alt->per_root_index != before) {
			if(error && error_size) snprintf(error, error_size,
				"CHOICE alternative %s PER index disagrees with canonical tag order",
				alt->source_name);
			return 1;
		}
	}
	return 0;
}

int
asn1typed_choice_wire_evidence_validate(const asn1typed_type_t *type,
		char *error, size_t error_size) {
	int check = choice_wire_evidence_check(type, error, error_size, 1);
	if(check != 0) return -1;
	if(!type->has_valid_per_root_mapping) {
		if(error && error_size) snprintf(error, error_size,
			"CHOICE PER root mapping has not been finalized");
		return -1;
	}
	return 0;
}

asn1typed_wire_finalize_result_e
asn1typed_choice_wire_evidence_finalize(asn1typed_type_t *type,
		char *error, size_t error_size) {
	size_t i, j, *pending_indexes;
	int check;
	if(!type) {
		if(error && error_size) snprintf(error, error_size,
			"wire evidence finalization requires a CHOICE");
		return ASN1TYPED_WIRE_FINALIZE_ERROR;
	}
	choice_wire_mapping_invalidate(type);
	check = choice_wire_evidence_check(type, error, error_size, 0);
	if(check != 0) return check > 0 ?
		ASN1TYPED_WIRE_FINALIZE_UNAVAILABLE : ASN1TYPED_WIRE_FINALIZE_ERROR;
	if(type->alternative_count > SIZE_MAX / sizeof(*pending_indexes)) {
		if(error && error_size) snprintf(error, error_size,
			"CHOICE PER index allocation size overflow");
		return ASN1TYPED_WIRE_FINALIZE_ERROR;
	}
	pending_indexes = (size_t *)malloc(type->alternative_count *
		sizeof(*pending_indexes));
	if(!pending_indexes) {
		if(error && error_size) snprintf(error, error_size,
			"out of memory finalizing CHOICE PER indexes");
		return ASN1TYPED_WIRE_FINALIZE_ERROR;
	}
	/* Derive and validate ranks privately; publish only the complete mapping. */
	for(i = 0; i < type->alternative_count; ++i) {
		const asn1typed_choice_alternative_t *alt = &type->alternatives[i];
		size_t rank = 0;
		for(j = 0; j < type->alternative_count; ++j)
			if(wire_tag_precedes(&type->alternatives[j], alt)) ++rank;
		pending_indexes[i] = rank;
	}
	for(i = 0; i < type->alternative_count; ++i) {
		if(pending_indexes[i] >= type->alternative_count) {
			free(pending_indexes);
			if(error && error_size) snprintf(error, error_size,
				"CHOICE PER root index is out of range");
			return ASN1TYPED_WIRE_FINALIZE_UNAVAILABLE;
		}
		for(j = 0; j < i; ++j) {
			if(pending_indexes[i] == pending_indexes[j]) {
				free(pending_indexes);
				if(error && error_size) snprintf(error, error_size,
					"CHOICE PER root indexes are not unique");
				return ASN1TYPED_WIRE_FINALIZE_UNAVAILABLE;
			}
		}
	}
	for(i = 0; i < type->alternative_count; ++i) {
		type->alternatives[i].per_root_index = pending_indexes[i];
		type->alternatives[i].has_per_root_index = 1;
	}
	free(pending_indexes);
	type->has_valid_per_root_mapping = 1;
	if(asn1typed_choice_wire_evidence_validate(type, error, error_size)) {
		choice_wire_mapping_invalidate(type);
		return ASN1TYPED_WIRE_FINALIZE_UNAVAILABLE;
	}
	return ASN1TYPED_WIRE_FINALIZE_OK;
}
