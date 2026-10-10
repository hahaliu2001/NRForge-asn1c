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

static void enumerated_mapping_invalidate(asn1typed_type_t *);
static void sequence_extension_invalidate(asn1typed_type_t *);

static int
asn1typed_inline_enum_body_valid(const asn1typed_type_t *body) {
	size_t i;
	int saw_extension_addition = 0;
	int saw_root = 0;
	if(!body || body->kind != ASN1TYPED_TYPE_ENUMERATED ||
		body->identity.module || body->identity.source_name ||
		body->primitive_kind != ASN1TYPED_PRIMITIVE_INVALID ||
		body->size_constraint.has_size_constraint || body->size_constraint.is_extensible || body->size_constraint.lower_bound || body->size_constraint.upper_bound || body->size_constraint.has_extension_addition || body->size_constraint.extension_lower_bound || body->size_constraint.extension_upper_bound ||
		body->value_range.has_value_range || body->value_range.tail ||
		body->value_range.tail_count || body->value_range.extension_additions || body->value_range.extension_addition_count || body->location.file ||
		body->fields || body->field_count || body->field_capacity ||
		body->alternatives || body->alternative_count ||
		body->alternative_capacity || body->element_type.module ||
		body->element_type.source_name || body->element_type.actuals ||
		body->element_type.actual_count || body->ioc_container.module ||
		body->ioc_container.source_name || body->ioc_container.actuals ||
		body->ioc_container.actual_count || body->has_ioc_table ||
		body->ioc_object_set_is_extensible ||
		body->enum_item_count > body->enum_item_capacity ||
		!!body->enum_item_capacity != !!body->enum_items || !body->enum_items ||
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
	free(field->value_range.tail); free(field->value_range.extension_additions);
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
		 source->is_extensible || source->tail || source->tail_count || source->extension_additions || source->extension_addition_count)) ||
		(source->tail_count && !source->tail) ||
		(!source->tail_count && source->tail) ||
		(source->tail_count && !source->has_value_range) ||
		source->tail_count > SIZE_MAX / sizeof(*source->tail) ||
        (!!source->extension_additions != !!source->extension_addition_count) ||
        (source->extension_addition_count && (!source->has_value_range || !source->is_extensible)) ||
        source->extension_addition_count > SIZE_MAX / sizeof(*source->extension_additions)) return -1;
    for(i = 0; i < source->extension_addition_count; ++i) {
        const asn1typed_integer_interval_t *a = &source->extension_additions[i];
        if(a->lower_bound > a->upper_bound || (i &&
            (source->extension_additions[i-1].upper_bound == INTMAX_MAX ||
             a->lower_bound <= source->extension_additions[i-1].upper_bound + 1))) return -1;
    }
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
	target->extension_additions = NULL;
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
	if(source->extension_addition_count) {
		target->extension_additions = malloc(source->extension_addition_count * sizeof(*source->extension_additions));
		if(!target->extension_additions) { free(target->tail); memset(target, 0, sizeof(*target)); return -1; }
		memcpy(target->extension_additions, source->extension_additions, source->extension_addition_count * sizeof(*source->extension_additions));
	}
	return 0;
}

static int
copy_enum_body(asn1typed_type_t *target, const asn1typed_type_t *source) {
	size_t i;
	if(!asn1typed_inline_enum_body_valid(source) ||
		source->enum_item_count > source->enum_item_capacity ||
		!!source->enum_item_capacity != !!source->enum_items ||
		(source->has_valid_per_enumeration_mapping &&
		 asn1typed_enumerated_evidence_validate(source, NULL, 0))) return -1;
	target->kind = ASN1TYPED_TYPE_ENUMERATED;
	target->is_extensible = source->is_extensible;
	for(i = 0; i < source->enum_item_count; ++i) {
		const asn1typed_enum_item_t *item = &source->enum_items[i];
		if(asn1typed_type_add_enum_item_ex(target, item->source_name,
			item->is_extension_addition, item->location.file, item->location.line)) return -1;
		target->enum_items[i].numeric_evidence = item->numeric_evidence;
		target->enum_items[i].assigned_number = item->assigned_number;
	}
	if(source->has_valid_per_enumeration_mapping) {
		for(i = 0; i < source->enum_item_count; ++i) {
			target->enum_items[i].has_per_enumeration_index = 1;
			target->enum_items[i].per_enumeration_index = source->enum_items[i].per_enumeration_index;
		}
		target->has_valid_per_enumeration_mapping = 1;
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
		target->inline_enumerated = (asn1typed_type_t *)calloc(1,
			sizeof(*target->inline_enumerated));
		if(!target->inline_enumerated ||
			copy_enum_body(target->inline_enumerated, source->inline_enumerated)) goto fail;
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
		primitive_kind > ASN1TYPED_PRIMITIVE_OPEN_TYPE) return -1;
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
	free(type->value_range.tail); free(type->value_range.extension_additions);
	for(i = 0; i < type->alternative_count; ++i) {
		if(type->alternatives[i].inline_enumerated) {
            asn1typed_type_clear(type->alternatives[i].inline_enumerated);
            free(type->alternatives[i].inline_enumerated);
        }
		free(type->alternatives[i].source_name);
		asn1typed_type_ref_clear(&type->alternatives[i].type_ref);
		free(type->alternatives[i].value_range.tail); free(type->alternatives[i].value_range.extension_additions);
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

static int
ioc_storage_valid(size_t count, size_t capacity, const void *data) {
	return count <= capacity && !!capacity == !!data;
}
static int
ioc_text(const char *s) { return s && s[0]; }
static void
ioc_registry_clear(asn1typed_ioc_registry_t *r) {
	size_t i;
	for(i = 0; i < r->row_count; ++i) {
		free(r->rows[i].symbolic_id);
		asn1typed_type_ref_clear(&r->rows[i].payload_type);
	}
	free(r->rows); free(r->class_module); free(r->class_source_name);
	free(r->object_set_module); free(r->object_set_source_name);
	free(r->selected_class_field_source_name); memset(r, 0, sizeof(*r));
}
static int
ioc_registry_storage(const asn1typed_ioc_registry_t *r) {
	return r && ioc_storage_valid(r->row_count, r->row_capacity, r->rows);
}
int
asn1typed_module_add_ioc_registry(asn1typed_module_t *m, const char *cm,
		const char *cn, const char *sm, const char *sn, const char *selected, size_t *index) {
	asn1typed_ioc_registry_t pending;
	size_t i;
	if(index) *index = SIZE_MAX;
	if(!m || !ioc_text(cm) || !ioc_text(cn) || !ioc_text(sm) || !ioc_text(sn) ||
		!ioc_text(selected) || !ioc_storage_valid(m->ioc_registry_count, m->ioc_registry_capacity, m->ioc_registries)) return -1;
	for(i = 0; i < m->ioc_registry_count; ++i) {
		asn1typed_ioc_registry_t *r = &m->ioc_registries[i];
		if(!ioc_text(r->class_module) || !ioc_text(r->class_source_name) || !ioc_text(r->object_set_module) || !ioc_text(r->object_set_source_name) || !ioc_text(r->selected_class_field_source_name)) return -1;
		if(!strcmp(cm, r->class_module) && !strcmp(cn, r->class_source_name) && !strcmp(sm, r->object_set_module) && !strcmp(sn, r->object_set_source_name)) {
			if(strcmp(selected, r->selected_class_field_source_name)) return -1;
			if(index) *index = i;
			return 0;
		}
	}
	memset(&pending, 0, sizeof(pending));
	pending.class_module = asn1typed_strdup(cm); pending.class_source_name = asn1typed_strdup(cn);
	pending.object_set_module = asn1typed_strdup(sm); pending.object_set_source_name = asn1typed_strdup(sn);
	pending.selected_class_field_source_name = asn1typed_strdup(selected);
	if(!pending.class_module || !pending.class_source_name || !pending.object_set_module || !pending.object_set_source_name || !pending.selected_class_field_source_name ||
		m->ioc_registry_count == SIZE_MAX || asn1typed_reserve((void **)&m->ioc_registries, &m->ioc_registry_capacity, m->ioc_registry_count + 1, sizeof(pending))) {
		ioc_registry_clear(&pending); return -1;
	}
	m->ioc_registries[m->ioc_registry_count] = pending;
	if(index) *index = m->ioc_registry_count;
	++m->ioc_registry_count; return 0;
}
static int ioc_payload_ref_valid(const asn1typed_type_ref_t *);
int
asn1typed_ioc_registry_add_row(asn1typed_ioc_registry_t *r, const asn1typed_ioc_dispatch_row_t *source) {
	asn1typed_ioc_dispatch_row_t row;
	if(!ioc_registry_storage(r) || !source || r->row_count == SIZE_MAX || !ioc_payload_ref_valid(&source->payload_type)) return -1;
	memset(&row, 0, sizeof(row));
	row.has_numeric_id = source->has_numeric_id; row.numeric_id = source->numeric_id;
	row.criticality = source->criticality; row.has_presence = source->has_presence; row.presence = source->presence;
	if(source->symbolic_id && !(row.symbolic_id = asn1typed_strdup(source->symbolic_id))) return -1;
	if(asn1typed_type_ref_copy(&row.payload_type, &source->payload_type) ||
		asn1typed_reserve((void **)&r->rows, &r->row_capacity, r->row_count + 1, sizeof(row))) {
		free(row.symbolic_id); asn1typed_type_ref_clear(&row.payload_type); return -1;
	}
	r->rows[r->row_count++] = row; r->evidence = ASN1TYPED_WIRE_EVIDENCE_UNAVAILABLE;
	r->declared_row_count = 0; r->has_valid_dispatch = 0; return 0;
}
int
asn1typed_ioc_registry_set_evidence(asn1typed_ioc_registry_t *r, size_t count, int ext) {
	if(!ioc_registry_storage(r) || count != r->row_count || (ext != 0 && ext != 1) || (!count && !ext)) return -1;
	r->evidence = ASN1TYPED_WIRE_EVIDENCE_RESOLVED; r->declared_row_count = count;
	r->object_set_is_extensible = ext; r->has_valid_dispatch = 0; return 0;
}
static int
ioc_registry_set_missing(asn1typed_ioc_registry_t *r, asn1typed_wire_evidence_e status) {
	if(!ioc_registry_storage(r)) return -1;
	r->evidence = status; r->declared_row_count = 0; r->object_set_is_extensible = 0; r->has_valid_dispatch = 0; return 0;
}
int
asn1typed_ioc_registry_set_unavailable(asn1typed_ioc_registry_t *r) { return ioc_registry_set_missing(r, ASN1TYPED_WIRE_EVIDENCE_UNAVAILABLE); }
int
asn1typed_ioc_registry_set_unsupported(asn1typed_ioc_registry_t *r) { return ioc_registry_set_missing(r, ASN1TYPED_WIRE_EVIDENCE_UNSUPPORTED); }
static int
ioc_payload_ref_valid(const asn1typed_type_ref_t *r) {
	if(r->actuals || r->actual_count) return 0;
	if(r->kind == ASN1TYPED_REF_NAMED) return ioc_text(r->module) && ioc_text(r->source_name) && r->primitive_kind == ASN1TYPED_PRIMITIVE_INVALID;
	return r->kind == ASN1TYPED_REF_PRIMITIVE && !r->module && !r->source_name &&
		r->primitive_kind > ASN1TYPED_PRIMITIVE_INVALID && r->primitive_kind <= ASN1TYPED_PRIMITIVE_OPEN_TYPE;
}
static int
ioc_registry_check(const asn1typed_ioc_registry_t *r, int published, char *error, size_t size) {
	size_t i, j;
#define IOC_BAD(text) do { if(error && size) snprintf(error, size, "%s", text); return -1; } while(0)
	if(error && size) error[0] = 0;
	if(!ioc_registry_storage(r)) IOC_BAD("IOC registry malformed storage");
	if(!ioc_text(r->class_module) || !ioc_text(r->class_source_name) || !ioc_text(r->object_set_module) || !ioc_text(r->object_set_source_name) || !ioc_text(r->selected_class_field_source_name)) IOC_BAD("IOC registry missing key identity");
	if(strcmp(r->selected_class_field_source_name, "Value") && strcmp(r->selected_class_field_source_name, "Extension")) IOC_BAD("IOC registry unsupported selected class field");
	if(r->evidence != ASN1TYPED_WIRE_EVIDENCE_RESOLVED || (published && r->has_valid_dispatch != 1)) IOC_BAD("IOC registry evidence unavailable or stale");
	if(r->declared_row_count != r->row_count || (r->object_set_is_extensible != 0 && r->object_set_is_extensible != 1) || (!r->row_count && !r->object_set_is_extensible)) IOC_BAD("IOC registry declaration count or extension evidence invalid");
	for(i = 0; i < r->row_count; ++i) {
		const asn1typed_ioc_dispatch_row_t *row = &r->rows[i];
		if(row->has_numeric_id != 1 || row->numeric_id < 0 || row->numeric_id > 65535) IOC_BAD("IOC registry unresolved or unsupported numeric ID");
		if(row->symbolic_id && !row->symbolic_id[0]) IOC_BAD("IOC registry empty symbolic ID");
		if(row->criticality < ASN1TYPED_CRITICALITY_REJECT || row->criticality > ASN1TYPED_CRITICALITY_NOTIFY) IOC_BAD("IOC registry invalid criticality");
		if((!strcmp(r->selected_class_field_source_name, "Value") && row->has_presence != 1) ||
			(row->has_presence != 0 && row->has_presence != 1) ||
			(i && row->has_presence != r->rows[0].has_presence) ||
			(row->has_presence && (row->presence < ASN1TYPED_PRESENCE_MANDATORY || row->presence > ASN1TYPED_PRESENCE_CONDITIONAL)) ||
			(!row->has_presence && row->presence != ASN1TYPED_PRESENCE_MANDATORY)) IOC_BAD("IOC registry invalid presence metadata");
		if(!ioc_payload_ref_valid(&row->payload_type)) IOC_BAD("IOC registry unsupported payload reference");
		for(j = 0; j < i; ++j) if(row->numeric_id == r->rows[j].numeric_id) IOC_BAD("IOC registry duplicate numeric ID");
	}
#undef IOC_BAD
	return 0;
}
int
asn1typed_ioc_registry_validate(const asn1typed_ioc_registry_t *r, char *error, size_t size) { return ioc_registry_check(r, 1, error, size); }
asn1typed_wire_finalize_result_e
asn1typed_ioc_registry_finalize(asn1typed_ioc_registry_t *r, char *error, size_t size) {
	if(r) r->has_valid_dispatch = 0;
	if(!ioc_registry_storage(r)) { if(error && size) snprintf(error, size, "IOC registry malformed API/storage"); return ASN1TYPED_WIRE_FINALIZE_ERROR; }
	if(ioc_registry_check(r, 0, error, size)) return ASN1TYPED_WIRE_FINALIZE_UNAVAILABLE;
	r->has_valid_dispatch = 1; return ASN1TYPED_WIRE_FINALIZE_OK;
}
static int
ioc_empty_ref(const asn1typed_type_ref_t *r) {
	return r->kind == ASN1TYPED_REF_NAMED && !r->module && !r->source_name && !r->actuals && !r->actual_count && r->primitive_kind == ASN1TYPED_PRIMITIVE_INVALID;
}
static int
ioc_empty_size(const asn1typed_size_constraint_t *s) {
	return !s->has_size_constraint && !s->is_extensible && !s->lower_bound && !s->upper_bound && !s->has_extension_addition && !s->extension_lower_bound && !s->extension_upper_bound;
}
static int
ioc_empty_range(const asn1typed_integer_value_range_t *r) {
	return !r->has_value_range && !r->is_extensible && !r->lower_bound && !r->upper_bound && !r->tail && !r->tail_count && !r->extension_additions && !r->extension_addition_count;
}
static const asn1typed_type_t *
ioc_named_type(const asn1typed_module_t *m, const asn1typed_type_ref_t *r) {
	size_t i;
	if(r->kind != ASN1TYPED_REF_NAMED || !ioc_payload_ref_valid(r) || !ioc_storage_valid(m->type_count, m->type_capacity, m->types)) return NULL;
	for(i = 0; i < m->type_count; ++i) {
		const asn1typed_type_t *t = &m->types[i];
		if(t->identity.module && t->identity.source_name && !strcmp(t->identity.module, r->module) && !strcmp(t->identity.source_name, r->source_name)) return t;
	}
	return NULL;
}
static int
ioc_scalar_clean(const asn1typed_type_t *t) {
	return t && !t->is_extensible && !t->fields && !t->field_count && !t->field_capacity &&
		!t->alternatives && !t->alternative_count && !t->alternative_capacity && !t->has_valid_per_root_mapping && !t->choice_root_only_extension_owned &&
		!t->has_ioc_table && !t->ioc_object_set_is_extensible && ioc_empty_ref(&t->element_type) && ioc_empty_ref(&t->ioc_container) &&
		ioc_empty_size(&t->size_constraint) && t->sequence_extension_evidence == ASN1TYPED_WIRE_EVIDENCE_UNAVAILABLE &&
		!t->sequence_root_field_count && !t->sequence_known_addition_count && !t->has_valid_sequence_extension_structure;
}
static int
ioc_relation_matches(const asn1typed_field_t *f, const asn1typed_ioc_registry_t *r, const char *role, const char *selector) {
	const asn1typed_class_field_relation_t *c = &f->class_field_relation;
	return f->has_class_field_relation == 1 && asn1typed_class_field_relation_valid(c) && c->has_selector == 1 && !c->actual_index &&
		!strcmp(c->class_module, r->class_module) && !strcmp(c->class_source_name, r->class_source_name) && !strcmp(c->class_field_source_name, role) && !strcmp(c->selector_source_name, selector);
}
static int
ioc_binding_check(const asn1typed_module_t *m, size_t index, int published, char *error, size_t size) {
	const asn1typed_bound_instance_t *instance;
	const asn1typed_type_t *body, *id_type, *criticality;
	const asn1typed_ioc_registry_t *r;
	const asn1typed_field_t *id, *crit, *value;
	size_t i;
#define BIND_BAD(text) do { if(error && size) snprintf(error, size, "%s", text); return -1; } while(0)
	if(error && size) error[0] = 0;
	if(!m || !ioc_storage_valid(m->bound_instance_count, m->bound_instance_capacity, m->bound_instances) || !ioc_storage_valid(m->ioc_registry_count, m->ioc_registry_capacity, m->ioc_registries) || index >= m->bound_instance_count) BIND_BAD("IOC binding malformed module/index/storage");
	instance = &m->bound_instances[index]; body = &instance->body;
	if(instance->has_empty_private_binding || instance->empty_private_object_set.module || instance->empty_private_object_set.source_name) BIND_BAD("private binding cannot be interpreted as numeric IOC");
	if(instance->ioc_binding.evidence != ASN1TYPED_WIRE_EVIDENCE_RESOLVED || (published && instance->ioc_binding.has_valid_binding != 1)) BIND_BAD("IOC binding evidence unavailable or stale");
	if(instance->ioc_binding.registry_index >= m->ioc_registry_count) BIND_BAD("IOC binding registry index out of range");
	r = &m->ioc_registries[instance->ioc_binding.registry_index];
	if(asn1typed_ioc_registry_validate(r, error, size)) return -1;
	if(instance->body_materialized != 1 || body->kind != ASN1TYPED_TYPE_SEQUENCE || body->is_extensible ||
		body->field_count != 3 || !ioc_storage_valid(body->field_count, body->field_capacity, body->fields) ||
		!ioc_empty_size(&body->size_constraint) || !ioc_empty_range(&body->value_range) || !ioc_empty_ref(&body->element_type) || !ioc_empty_ref(&body->ioc_container) || body->has_ioc_table || body->ioc_object_set_is_extensible ||
		body->primitive_kind != ASN1TYPED_PRIMITIVE_INVALID || body->enum_items || body->enum_item_count || body->enum_item_capacity || body->has_valid_per_enumeration_mapping || body->alternatives || body->alternative_count || body->alternative_capacity || body->has_valid_per_root_mapping || body->choice_root_only_extension_owned ||
		body->sequence_extension_evidence != ASN1TYPED_WIRE_EVIDENCE_UNAVAILABLE || body->sequence_root_field_count || body->sequence_known_addition_count || body->has_valid_sequence_extension_structure)
		BIND_BAD("IOC binding unsupported physical field body");
	if(instance->ioc_binding.id_field_ordinal != 0 || instance->ioc_binding.criticality_field_ordinal != 1 || instance->ioc_binding.value_field_ordinal != 2) BIND_BAD("IOC binding physical role ordinals invalid");
	if(instance->identity.kind != ASN1TYPED_REF_NAMED || !ioc_text(instance->identity.module) || !ioc_text(instance->identity.source_name) || instance->identity.primitive_kind != ASN1TYPED_PRIMITIVE_INVALID || instance->identity.actual_count != 1 || !instance->identity.actuals || instance->identity.actuals[0].kind != ASN1TYPED_ACTUAL_OBJECT_SET_REFERENCE || !ioc_text(instance->identity.actuals[0].module) || !ioc_text(instance->identity.actuals[0].source_name) || strcmp(instance->identity.actuals[0].module, r->object_set_module) || strcmp(instance->identity.actuals[0].source_name, r->object_set_source_name)) BIND_BAD("IOC binding actual object-set identity mismatch");
	id = &body->fields[0]; crit = &body->fields[1]; value = &body->fields[2];
	if(ioc_text(id->source_name) && ioc_text(crit->source_name) && ioc_text(value->source_name) && (!strcmp(id->source_name, crit->source_name) || !strcmp(id->source_name, value->source_name) || !strcmp(crit->source_name, value->source_name))) BIND_BAD("IOC binding duplicate physical field identity");
	for(i = 0; i < 3; ++i) {
		const asn1typed_field_t *f = &body->fields[i];
		if(!ioc_text(f->source_name) || f->presence != ASN1TYPED_PRESENCE_MANDATORY || f->inline_enumerated || !ioc_empty_size(&f->size_constraint) || !ioc_empty_range(&f->value_range) || f->ioc.symbolic_id || f->ioc.has_numeric_id || f->ioc.numeric_id || f->ioc.criticality != ASN1TYPED_CRITICALITY_REJECT) BIND_BAD("IOC binding unsupported physical field metadata");
	}
	if(id->type_semantics != ASN1TYPED_FIELD_FIXED_TYPE || id->has_class_field_relation || id->class_field_relation.class_module || id->class_field_relation.class_source_name || id->class_field_relation.class_field_source_name || id->class_field_relation.actual_index || id->class_field_relation.has_selector || id->class_field_relation.selector_source_name ||
		crit->type_semantics != ASN1TYPED_FIELD_FIXED_TYPE || !ioc_relation_matches(crit, r, "criticality", id->source_name) ||
		value->type_semantics != ASN1TYPED_FIELD_CLASS_FIELD_SELECTED_TYPE || !ioc_empty_ref(&value->type) || !ioc_relation_matches(value, r, r->selected_class_field_source_name, id->source_name)) BIND_BAD("IOC binding selector/class role mismatch");
	id_type = ioc_named_type(m, &id->type); criticality = ioc_named_type(m, &crit->type);
	if(!ioc_scalar_clean(id_type) || id_type->kind != ASN1TYPED_TYPE_PRIMITIVE || id_type->primitive_kind != ASN1TYPED_PRIMITIVE_INTEGER || id_type->value_range.has_value_range != 1 || id_type->value_range.is_extensible || id_type->value_range.lower_bound != 0 || id_type->value_range.upper_bound != 65535 || id_type->value_range.tail || id_type->value_range.tail_count || id_type->value_range.extension_additions || id_type->value_range.extension_addition_count || id_type->enum_items || id_type->enum_item_count || id_type->enum_item_capacity || id_type->has_valid_per_enumeration_mapping) BIND_BAD("IOC binding identifier type/domain unsupported");
	if(!ioc_scalar_clean(criticality) || criticality->kind != ASN1TYPED_TYPE_ENUMERATED || criticality->primitive_kind != ASN1TYPED_PRIMITIVE_INVALID || !ioc_empty_range(&criticality->value_range) || criticality->enum_item_count != 3 || asn1typed_enumerated_evidence_validate(criticality, error, size)) BIND_BAD("IOC binding criticality type evidence unsupported");
	for(i = 0; i < 3; ++i) {
		const asn1typed_enum_item_t *item = &criticality->enum_items[i];
		const char *names[] = {"reject", "ignore", "notify"};
		if(item->assigned_number < 0 || item->assigned_number > 2 || item->is_extension_addition || !ioc_text(item->source_name) || strcmp(item->source_name, names[(size_t)item->assigned_number])) BIND_BAD("IOC binding criticality values unsupported");
	}
#undef BIND_BAD
	return 0;
}
int
asn1typed_bound_instance_set_empty_private_binding(asn1typed_module_t *m, size_t index) {
    asn1typed_type_actual_t pending = {0};
    asn1typed_bound_instance_t *b;
    if(!m || !ioc_storage_valid(m->bound_instance_count, m->bound_instance_capacity, m->bound_instances) || index >= m->bound_instance_count) return -1;
    b = &m->bound_instances[index];
    if(b->identity.actual_count != 1 || !b->identity.actuals || b->identity.actuals[0].kind != ASN1TYPED_ACTUAL_OBJECT_SET_REFERENCE || !ioc_text(b->identity.actuals[0].module) || !ioc_text(b->identity.actuals[0].source_name)) return -1;
    pending.kind = ASN1TYPED_ACTUAL_OBJECT_SET_REFERENCE;
    pending.module = asn1typed_strdup(b->identity.actuals[0].module);
    pending.source_name = asn1typed_strdup(b->identity.actuals[0].source_name);
    if(!pending.module || !pending.source_name) { free(pending.module); free(pending.source_name); return -1; }
    free(b->empty_private_object_set.module); free(b->empty_private_object_set.source_name);
    b->empty_private_object_set = pending; b->has_empty_private_binding = 1;
    return 0;
}

int
asn1typed_bound_instance_empty_private_validate(const asn1typed_module_t *m,
        size_t index, char *error, size_t size) {
    const asn1typed_bound_instance_t *instance;
    const asn1typed_type_t *body, *key, *criticality;
    const asn1typed_field_t *id, *crit, *value;
    size_t i;
#define PRIVATE_BAD(message) do { if(error && size) snprintf(error, size, "%s", message); return -1; } while(0)
    if(error && size) error[0] = 0;
    if(!m || !ioc_storage_valid(m->type_count, m->type_capacity, m->types) ||
        !ioc_storage_valid(m->bound_instance_count, m->bound_instance_capacity, m->bound_instances) || index >= m->bound_instance_count) PRIVATE_BAD("malformed empty-private module/storage");
    instance = &m->bound_instances[index]; body = &instance->body;
    if(instance->has_empty_private_binding != 1 || instance->body_materialized != 1 ||
        instance->ioc_binding.evidence != ASN1TYPED_WIRE_EVIDENCE_UNAVAILABLE || instance->ioc_binding.has_valid_binding ||
        instance->ioc_binding.registry_index || instance->ioc_binding.id_field_ordinal || instance->ioc_binding.criticality_field_ordinal || instance->ioc_binding.value_field_ordinal ||
        instance->identity.actual_count != 1 || !instance->identity.actuals || instance->identity.actuals[0].kind != ASN1TYPED_ACTUAL_OBJECT_SET_REFERENCE ||
        !ioc_text(instance->identity.actuals[0].module) || !ioc_text(instance->identity.actuals[0].source_name)) PRIVATE_BAD("empty-private evidence/actual mismatch");
    if(instance->empty_private_object_set.kind != ASN1TYPED_ACTUAL_OBJECT_SET_REFERENCE || !ioc_text(instance->empty_private_object_set.module) || !ioc_text(instance->empty_private_object_set.source_name) || strcmp(instance->empty_private_object_set.module, instance->identity.actuals[0].module) || strcmp(instance->empty_private_object_set.source_name, instance->identity.actuals[0].source_name)) PRIVATE_BAD("empty-private object-set snapshot mismatch");
    if(body->kind != ASN1TYPED_TYPE_SEQUENCE || body->is_extensible || body->field_count != 3 ||
        !ioc_storage_valid(body->field_count, body->field_capacity, body->fields) ||
        !ioc_empty_size(&body->size_constraint) || !ioc_empty_range(&body->value_range) || !ioc_empty_ref(&body->ioc_container) || !ioc_empty_ref(&body->element_type) || body->has_ioc_table || body->ioc_object_set_is_extensible ||
        body->primitive_kind != ASN1TYPED_PRIMITIVE_INVALID || body->enum_items || body->enum_item_count || body->enum_item_capacity || body->alternatives || body->alternative_count || body->alternative_capacity) PRIVATE_BAD("unsupported empty-private field body");
    id = &body->fields[0]; crit = &body->fields[1]; value = &body->fields[2];
    for(i = 0; i < 3; ++i) {
        const asn1typed_field_t *f = &body->fields[i];
        if(!ioc_text(f->source_name) || f->presence != ASN1TYPED_PRESENCE_MANDATORY || f->inline_enumerated || !ioc_empty_size(&f->size_constraint) || !ioc_empty_range(&f->value_range) || f->ioc.symbolic_id || f->ioc.has_numeric_id || f->ioc.numeric_id || f->ioc.criticality != ASN1TYPED_CRITICALITY_REJECT) PRIVATE_BAD("unsupported empty-private field metadata");
    }
    if(id->type_semantics != ASN1TYPED_FIELD_FIXED_TYPE || id->has_class_field_relation ||
        crit->type_semantics != ASN1TYPED_FIELD_FIXED_TYPE || !crit->has_class_field_relation ||
        value->type_semantics != ASN1TYPED_FIELD_CLASS_FIELD_SELECTED_TYPE || !value->has_class_field_relation || !ioc_empty_ref(&value->type)) PRIVATE_BAD("empty-private selector roles invalid");
    for(i = 1; i < 3; ++i) {
        const asn1typed_class_field_relation_t *r = &body->fields[i].class_field_relation;
        if(!ioc_text(r->class_module) || !ioc_text(r->class_source_name) || !ioc_text(r->class_field_source_name) ||
            strcmp(r->class_field_source_name, i == 1 ? "criticality" : "Value") || r->actual_index || r->has_selector != 1 ||
            !ioc_text(r->selector_source_name) || strcmp(r->selector_source_name, id->source_name) ||
            strcmp(r->class_module, crit->class_field_relation.class_module) || strcmp(r->class_source_name, crit->class_field_relation.class_source_name)) PRIVATE_BAD("empty-private class relation mismatch");
    }
    key = ioc_named_type(m, &id->type); criticality = ioc_named_type(m, &crit->type);
    if(!key || key->kind != ASN1TYPED_TYPE_CHOICE || key->is_extensible || key->alternative_count != 2 ||
        asn1typed_choice_wire_evidence_validate(key, error, size)) PRIVATE_BAD("empty-private key CHOICE evidence unavailable");
    for(i = 0; i < 2; ++i) {
        const asn1typed_choice_alternative_t *a = &key->alternatives[i];
        if(a->wire_evidence != ASN1TYPED_WIRE_EVIDENCE_RESOLVED || a->effective_tag_class != ASN1TYPED_TAG_CLASS_CONTEXT_SPECIFIC || a->effective_tag_number != (intmax_t)i || a->per_root_index != i ||
            a->type_ref.kind != ASN1TYPED_REF_PRIMITIVE || a->type_ref.module || a->type_ref.source_name || a->type_ref.actuals || a->type_ref.actual_count ||
            a->type_ref.primitive_kind != (i ? ASN1TYPED_PRIMITIVE_OBJECT_IDENTIFIER : ASN1TYPED_PRIMITIVE_INTEGER) || !ioc_empty_size(&a->size_constraint)) PRIVATE_BAD("empty-private key domain/tag order unsupported");
        if(i ? !ioc_empty_range(&a->value_range) : (a->value_range.has_value_range != 1 || a->value_range.is_extensible || a->value_range.lower_bound || a->value_range.upper_bound != 65535 || a->value_range.tail || a->value_range.tail_count || a->value_range.extension_additions || a->value_range.extension_addition_count)) PRIVATE_BAD("empty-private local/OID key constraint unsupported");
    }
    if(!ioc_scalar_clean(criticality) || criticality->kind != ASN1TYPED_TYPE_ENUMERATED || criticality->enum_item_count != 3 || asn1typed_enumerated_evidence_validate(criticality, error, size)) PRIVATE_BAD("empty-private criticality evidence unavailable");
    for(i = 0; i < 3; ++i) {
        const asn1typed_enum_item_t *item = &criticality->enum_items[i];
        const char *names[] = {"reject", "ignore", "notify"};
        if(item->assigned_number < 0 || item->assigned_number > 2 || item->is_extension_addition || !ioc_text(item->source_name) || strcmp(item->source_name, names[(size_t)item->assigned_number])) PRIVATE_BAD("empty-private criticality domain unsupported");
    }
#undef PRIVATE_BAD
    return 0;
}

int
asn1typed_bound_instance_set_ioc_binding(asn1typed_module_t *m, size_t index, size_t registry,
		size_t id, size_t crit, size_t value) {
	asn1typed_ioc_binding_t old, pending;
	if(!m || !ioc_storage_valid(m->bound_instance_count, m->bound_instance_capacity, m->bound_instances) || index >= m->bound_instance_count) return -1;
	memset(&pending, 0, sizeof(pending)); pending.evidence = ASN1TYPED_WIRE_EVIDENCE_RESOLVED;
	pending.registry_index = registry; pending.id_field_ordinal = id; pending.criticality_field_ordinal = crit; pending.value_field_ordinal = value;
	old = m->bound_instances[index].ioc_binding; m->bound_instances[index].ioc_binding = pending;
	if(ioc_binding_check(m, index, 0, NULL, 0)) { m->bound_instances[index].ioc_binding = old; return -1; }
	return 0;
}
asn1typed_wire_finalize_result_e
asn1typed_bound_instance_ioc_binding_finalize(asn1typed_module_t *m, size_t index, char *error, size_t size) {
	if(!m || !ioc_storage_valid(m->bound_instance_count, m->bound_instance_capacity, m->bound_instances) || index >= m->bound_instance_count) { if(error && size) snprintf(error, size, "IOC binding malformed API/storage"); return ASN1TYPED_WIRE_FINALIZE_ERROR; }
	m->bound_instances[index].ioc_binding.has_valid_binding = 0;
	if(ioc_binding_check(m, index, 0, error, size)) return ASN1TYPED_WIRE_FINALIZE_UNAVAILABLE;
	m->bound_instances[index].ioc_binding.has_valid_binding = 1; return ASN1TYPED_WIRE_FINALIZE_OK;
}
int
asn1typed_bound_instance_ioc_binding_validate(const asn1typed_module_t *m, size_t index, char *error, size_t size) { return ioc_binding_check(m, index, 1, error, size); }

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
		free(module->bound_instances[i].empty_private_object_set.module);
		free(module->bound_instances[i].empty_private_object_set.source_name);
	}
	free(module->bound_instances);
	for(i = 0; i < module->ioc_registry_count; ++i) ioc_registry_clear(&module->ioc_registries[i]);
	free(module->ioc_registries);
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
	memset(&instance->ioc_binding, 0, sizeof(instance->ioc_binding));
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
	sequence_extension_invalidate(type);
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
	sequence_extension_invalidate(type);
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
	sequence_extension_invalidate(type);
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
	sequence_extension_invalidate(type);
	return 0;
}

int
asn1typed_type_add_inline_enumerated_alternative(asn1typed_type_t *type,
        const char *name, const asn1typed_type_t *body, const char *file, unsigned line) {
    asn1typed_type_t *owned; asn1typed_type_ref_t temporary = {0};
    /* Reject scalar residue before copy_enum_body can discard it. Keep this
     * stronger guard local to the new CHOICE API, preserving the field API. */
    if(!type || type->kind != ASN1TYPED_TYPE_CHOICE || !name || !name[0] || !file ||
       !body || body->enum_item_capacity > SIZE_MAX / sizeof(*body->enum_items) ||
       body->size_constraint.is_extensible || body->size_constraint.lower_bound ||
       body->size_constraint.upper_bound || body->value_range.is_extensible ||
       body->value_range.lower_bound || body->value_range.upper_bound ||
       body->location.line || body->element_type.kind != ASN1TYPED_REF_NAMED ||
       body->element_type.primitive_kind != ASN1TYPED_PRIMITIVE_INVALID ||
       body->ioc_container.kind != ASN1TYPED_REF_NAMED ||
       body->ioc_container.primitive_kind != ASN1TYPED_PRIMITIVE_INVALID ||
       body->has_valid_per_root_mapping || body->choice_root_only_extension_owned ||
       body->sequence_extension_evidence != ASN1TYPED_WIRE_EVIDENCE_UNAVAILABLE ||
       body->sequence_root_field_count || body->sequence_known_addition_count ||
       body->has_valid_sequence_extension_structure ||
       !asn1typed_inline_enum_body_valid(body)) return -1;
    owned = (asn1typed_type_t *)calloc(1, sizeof(*owned));
    if(!owned) return -1;
    if(copy_enum_body(owned, body)) { asn1typed_type_clear(owned); free(owned); return -1; }
    /* Existing add operation provides reserve/name/location transactionality.
     * Its temporary primitive reference is cleared before the body is published. */
    if(asn1typed_type_ref_init_primitive(&temporary, ASN1TYPED_PRIMITIVE_NULL) ||
       asn1typed_type_add_choice_alternative(type, name, &temporary, NULL, NULL, file, line)) {
        asn1typed_type_ref_clear(&temporary); asn1typed_type_clear(owned); free(owned); return -1;
    }
    asn1typed_type_ref_clear(&temporary);
    asn1typed_type_ref_clear(&type->alternatives[type->alternative_count - 1].type_ref);
    type->alternatives[type->alternative_count - 1].inline_enumerated = owned;
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
	if(copy_enum_body(field.inline_enumerated, body)) {
		asn1typed_field_clear(&field);
		return -1;
	}
	if(asn1typed_reserve((void **)&type->fields, &type->field_capacity,
			type->field_count + 1, sizeof(*type->fields))) {
		asn1typed_field_clear(&field);
		return -1;
	}
	type->fields[type->field_count++] = field;
	sequence_extension_invalidate(type);
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
		primitive_kind > ASN1TYPED_PRIMITIVE_OPEN_TYPE) return -1;
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
	sequence_extension_invalidate(type);
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
	enumerated_mapping_invalidate(type);
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
		size_constraint->lower_bound > size_constraint->upper_bound ||
        (size_constraint->is_extensible != 0 && size_constraint->is_extensible != 1) ||
        (size_constraint->has_extension_addition != 0 && size_constraint->has_extension_addition != 1) ||
        (!size_constraint->has_extension_addition && (size_constraint->extension_lower_bound || size_constraint->extension_upper_bound)) ||
        (size_constraint->has_extension_addition && (!size_constraint->is_extensible || size_constraint->extension_lower_bound < 0 || size_constraint->extension_upper_bound < size_constraint->extension_lower_bound)))) return -1;
	if(value_range && (!value_range->has_value_range || size_constraint ||
		(type_ref->kind == ASN1TYPED_REF_PRIMITIVE &&
		 type_ref->primitive_kind != ASN1TYPED_PRIMITIVE_INTEGER))) return -1;
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
	free(alternative.value_range.tail); free(alternative.value_range.extension_additions);
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
	if((type->choice_root_only_extension_owned && (type->choice_root_only_extension_owned != 1 || type->is_extensible != 1)) || (type->is_extensible && type->choice_root_only_extension_owned != 1)) {
		if(error && error_size) snprintf(error, error_size,
			"extensible CHOICE wire mapping lacks owned root-only marker");
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

static int
enumerated_storage_valid(const asn1typed_type_t *type) {
	return type && type->kind == ASN1TYPED_TYPE_ENUMERATED &&
		type->enum_item_count <= type->enum_item_capacity &&
		!!type->enum_item_capacity == !!type->enum_items;
}

static void
enumerated_mapping_invalidate(asn1typed_type_t *type) {
	size_t i;
	type->has_valid_per_enumeration_mapping = 0;
	for(i = 0; i < type->enum_item_count; ++i) {
		type->enum_items[i].has_per_enumeration_index = 0;
		type->enum_items[i].per_enumeration_index = 0;
	}
}

static int
enumerated_set_numeric(asn1typed_type_t *type, size_t index,
		asn1typed_wire_evidence_e evidence, intmax_t number) {
	if(!enumerated_storage_valid(type) || index >= type->enum_item_count)
		return -1;
	enumerated_mapping_invalidate(type);
	type->enum_items[index].numeric_evidence = evidence;
	type->enum_items[index].assigned_number = number;
	return 0;
}

int
asn1typed_enum_item_set_numeric_evidence(asn1typed_type_t *type,
		size_t index, intmax_t number) {
	return enumerated_set_numeric(type, index,
		ASN1TYPED_WIRE_EVIDENCE_RESOLVED, number);
}

int
asn1typed_enum_item_set_numeric_unavailable(asn1typed_type_t *type,
		size_t index) {
	return enumerated_set_numeric(type, index,
		ASN1TYPED_WIRE_EVIDENCE_UNAVAILABLE, 0);
}

int
asn1typed_enum_item_set_numeric_unsupported(asn1typed_type_t *type,
		size_t index) {
	return enumerated_set_numeric(type, index,
		ASN1TYPED_WIRE_EVIDENCE_UNSUPPORTED, 0);
}

/* -1 is malformed storage/API; 1 is unusable semantic evidence. */
static int
enumerated_evidence_check(const asn1typed_type_t *type,
		char *error, size_t error_size, int indexes) {
	size_t i, j;
	int saw_addition = 0, saw_root = 0;
	intmax_t previous_addition = 0;
#define ENUM_FAIL(result, message) do { \
	if(error && error_size) snprintf(error, error_size, "%s", message); \
	return result; \
} while(0)
	if(error && error_size) error[0] = 0;
	if(!enumerated_storage_valid(type))
		ENUM_FAIL(-1, "ENUMERATED has invalid kind or item storage");
	if(type->is_extensible != 0 && type->is_extensible != 1)
		ENUM_FAIL(1, "ENUMERATED has invalid extensibility flag");
	for(i = 0; i < type->enum_item_count; ++i) {
		const asn1typed_enum_item_t *item = &type->enum_items[i];
		if(!item->source_name || !item->source_name[0])
			ENUM_FAIL(1, "ENUMERATED item has missing source name");
		if(item->is_extension_addition != 0 && item->is_extension_addition != 1)
			ENUM_FAIL(1, "ENUMERATED item has invalid extension membership");
		if(item->numeric_evidence != ASN1TYPED_WIRE_EVIDENCE_RESOLVED)
			ENUM_FAIL(1, item->numeric_evidence == ASN1TYPED_WIRE_EVIDENCE_UNSUPPORTED ?
				"ENUMERATED item has unsupported numeric evidence" :
				"ENUMERATED item has unavailable numeric evidence");
		if(item->is_extension_addition) {
			if(!saw_root || !type->is_extensible)
				ENUM_FAIL(1, "ENUMERATED addition requires an extensible root");
			if(saw_addition && item->assigned_number <= previous_addition)
				ENUM_FAIL(1, "ENUMERATED addition numbers are not strictly increasing");
			saw_addition = 1;
			previous_addition = item->assigned_number;
		} else {
			if(saw_addition)
				ENUM_FAIL(1, "ENUMERATED root occurs after an addition");
			saw_root = 1;
		}
		for(j = 0; j < i; ++j) {
			const asn1typed_enum_item_t *other = &type->enum_items[j];
			if(!strcmp(item->source_name, other->source_name))
				ENUM_FAIL(1, "ENUMERATED has duplicate source names");
			if(item->assigned_number == other->assigned_number)
				ENUM_FAIL(1, "ENUMERATED has duplicate assigned numbers");
		}
	}
	if(!saw_root) ENUM_FAIL(1, "ENUMERATED requires a root item");
	if(indexes) {
		for(i = 0; i < type->enum_item_count; ++i) {
			const asn1typed_enum_item_t *item = &type->enum_items[i];
			size_t rank = 0;
			if(item->has_per_enumeration_index != 1)
				ENUM_FAIL(1, "ENUMERATED item has missing PER enumeration index");
			for(j = 0; j < type->enum_item_count; ++j) {
				const asn1typed_enum_item_t *other = &type->enum_items[j];
				if(other->is_extension_addition != item->is_extension_addition) continue;
				if(other->assigned_number < item->assigned_number) ++rank;
				if(j < i && other->per_enumeration_index == item->per_enumeration_index)
					ENUM_FAIL(1, "ENUMERATED has duplicate PER enumeration indexes");
			}
			if(item->per_enumeration_index != rank)
				ENUM_FAIL(1, "ENUMERATED PER enumeration index disagrees with numeric order");
		}
	}
#undef ENUM_FAIL
	return 0;
}

int
asn1typed_enumerated_evidence_validate(const asn1typed_type_t *type,
		char *error, size_t error_size) {
	if(enumerated_evidence_check(type, error, error_size, 1)) return -1;
	if(type->has_valid_per_enumeration_mapping != 1) {
		if(error && error_size) snprintf(error, error_size,
			"ENUMERATED mapping has not been finalized");
		return -1;
	}
	return 0;
}

asn1typed_wire_finalize_result_e
asn1typed_enumerated_evidence_finalize(asn1typed_type_t *type,
		char *error, size_t error_size) {
	size_t i, j, *pending;
	int check;
	if(!enumerated_storage_valid(type)) {
		if(error && error_size) snprintf(error, error_size,
			"ENUMERATED finalization requires valid kind and item storage");
		return ASN1TYPED_WIRE_FINALIZE_ERROR;
	}
	enumerated_mapping_invalidate(type);
	check = enumerated_evidence_check(type, error, error_size, 0);
	if(check) return check < 0 ? ASN1TYPED_WIRE_FINALIZE_ERROR :
		ASN1TYPED_WIRE_FINALIZE_UNAVAILABLE;
	if(type->enum_item_count > SIZE_MAX / sizeof(*pending)) {
		if(error && error_size) snprintf(error, error_size,
			"ENUMERATED index allocation size overflow");
		return ASN1TYPED_WIRE_FINALIZE_ERROR;
	}
	pending = (size_t *)malloc(type->enum_item_count * sizeof(*pending));
	if(!pending) {
		if(error && error_size) snprintf(error, error_size,
			"out of memory finalizing ENUMERATED indexes");
		return ASN1TYPED_WIRE_FINALIZE_ERROR;
	}
	for(i = 0; i < type->enum_item_count; ++i) {
		pending[i] = 0;
		for(j = 0; j < type->enum_item_count; ++j)
			if(type->enum_items[j].is_extension_addition == type->enum_items[i].is_extension_addition &&
				type->enum_items[j].assigned_number < type->enum_items[i].assigned_number)
				++pending[i];
	}
	/* Check the private permutation before publishing any item index. */
	for(i = 0; i < type->enum_item_count; ++i) {
		size_t part_count = 0;
		for(j = 0; j < type->enum_item_count; ++j) {
			if(type->enum_items[j].is_extension_addition != type->enum_items[i].is_extension_addition) continue;
			++part_count;
			if(j < i && pending[j] == pending[i]) break;
		}
		if(j != type->enum_item_count || pending[i] >= part_count) {
			free(pending);
			if(error && error_size) snprintf(error, error_size,
				"ENUMERATED internal index permutation failure");
			return ASN1TYPED_WIRE_FINALIZE_ERROR;
		}
	}
	for(i = 0; i < type->enum_item_count; ++i) {
		type->enum_items[i].per_enumeration_index = pending[i];
		type->enum_items[i].has_per_enumeration_index = 1;
	}
	free(pending);
	type->has_valid_per_enumeration_mapping = 1;
	if(asn1typed_enumerated_evidence_validate(type, error, error_size)) {
		enumerated_mapping_invalidate(type);
		return ASN1TYPED_WIRE_FINALIZE_ERROR;
	}
	return ASN1TYPED_WIRE_FINALIZE_OK;
}

static int
sequence_extension_storage_valid(const asn1typed_type_t *type) {
	return type && type->kind == ASN1TYPED_TYPE_SEQUENCE &&
		type->field_count <= type->field_capacity &&
		!!type->field_capacity == !!type->fields;
}
static void
sequence_extension_invalidate(asn1typed_type_t *type) {
	type->sequence_extension_evidence = ASN1TYPED_WIRE_EVIDENCE_UNAVAILABLE;
	type->sequence_root_field_count = 0;
	type->sequence_known_addition_count = 0;
	type->has_valid_sequence_extension_structure = 0;
}
int
asn1typed_sequence_set_extension_structure(asn1typed_type_t *type,
		size_t roots, size_t additions) {
	if(!sequence_extension_storage_valid(type) || type->is_extensible != 1 ||
		roots != type->field_count || additions != 0) return -1;
	sequence_extension_invalidate(type);
	type->sequence_extension_evidence = ASN1TYPED_WIRE_EVIDENCE_RESOLVED;
	type->sequence_root_field_count = roots;
	return 0;
}
int
asn1typed_sequence_set_extension_unavailable(asn1typed_type_t *type) {
	if(!sequence_extension_storage_valid(type)) return -1;
	sequence_extension_invalidate(type);
	return 0;
}
int
asn1typed_sequence_set_extension_unsupported(asn1typed_type_t *type) {
	if(!sequence_extension_storage_valid(type)) return -1;
	sequence_extension_invalidate(type);
	type->sequence_extension_evidence = ASN1TYPED_WIRE_EVIDENCE_UNSUPPORTED;
	return 0;
}
static int
sequence_extension_check(const asn1typed_type_t *type, char *error, size_t size) {
#define SEQUENCE_FAIL(code, message) do { \
	if(error && size) snprintf(error, size, "%s", message); \
	return code; \
} while(0)
	if(error && size) error[0] = 0;
	if(!sequence_extension_storage_valid(type))
		SEQUENCE_FAIL(-1, "SEQUENCE extension structure has invalid kind or field storage");
	if(type->is_extensible != 1)
		SEQUENCE_FAIL(1, "SEQUENCE extension structure requires an extensible SEQUENCE");
	if(type->sequence_extension_evidence != ASN1TYPED_WIRE_EVIDENCE_RESOLVED)
		SEQUENCE_FAIL(1, type->sequence_extension_evidence == ASN1TYPED_WIRE_EVIDENCE_UNSUPPORTED ?
			"SEQUENCE extension structure evidence is unsupported" :
			"SEQUENCE extension structure evidence is unavailable");
	if(type->sequence_root_field_count != type->field_count)
		SEQUENCE_FAIL(1, "SEQUENCE root field count disagrees with owned physical fields");
	if(type->sequence_known_addition_count != 0)
		SEQUENCE_FAIL(1, "SEQUENCE known extension additions are unsupported");
#undef SEQUENCE_FAIL
	return 0;
}
int
asn1typed_sequence_extension_structure_validate(const asn1typed_type_t *type,
		char *error, size_t size) {
	if(sequence_extension_check(type, error, size)) return -1;
	if(type->has_valid_sequence_extension_structure != 1) {
		if(error && size) snprintf(error, size, "SEQUENCE extension structure has not been finalized");
		return -1;
	}
	return 0;
}
asn1typed_wire_finalize_result_e
asn1typed_sequence_extension_structure_finalize(asn1typed_type_t *type,
		char *error, size_t size) {
	int check;
	if(!sequence_extension_storage_valid(type)) {
		if(error && size) snprintf(error, size, "SEQUENCE finalization requires valid kind and field storage");
		return ASN1TYPED_WIRE_FINALIZE_ERROR;
	}
	type->has_valid_sequence_extension_structure = 0;
	check = sequence_extension_check(type, error, size);
	if(check) return check < 0 ? ASN1TYPED_WIRE_FINALIZE_ERROR : ASN1TYPED_WIRE_FINALIZE_UNAVAILABLE;
	type->has_valid_sequence_extension_structure = 1;
	return ASN1TYPED_WIRE_FINALIZE_OK;
}
