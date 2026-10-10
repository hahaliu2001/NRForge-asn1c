#include "asn1typed_extract.h"
#include "asn1typed_name.h"
#include <asn1fix_export.h>
#include <asn1_namespace.h>
#include <asn1p_constr.h>
#include <asn1p_integer.h>
#include <asn1p_value.h>

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static asn1p_expr_t *ioc_resolve(asn1p_t *tree, asn1p_expr_t *context,
		asn1p_ref_t *ref);

static void
set_error(char *error, size_t size, const char *format, ...) {
	va_list ap;
	if(!error || !size) return;
	va_start(ap, format);
	vsnprintf(error, size, format, ap);
	va_end(ap);
}

static asn1typed_primitive_kind_e
primitive_from_name(const char *name) {
	if(!name) return ASN1TYPED_PRIMITIVE_INVALID;
	if(!strcmp(name, "BOOLEAN")) return ASN1TYPED_PRIMITIVE_BOOLEAN;
	if(!strcmp(name, "INTEGER")) return ASN1TYPED_PRIMITIVE_INTEGER;
	if(!strcmp(name, "UTF8String")) return ASN1TYPED_PRIMITIVE_UTF8_STRING;
	if(!strcmp(name, "PrintableString")) return ASN1TYPED_PRIMITIVE_PRINTABLE_STRING;
	if(!strcmp(name, "VisibleString")) return ASN1TYPED_PRIMITIVE_VISIBLE_STRING;
	if(!strcmp(name, "OCTET STRING")) return ASN1TYPED_PRIMITIVE_OCTET_STRING;
	return ASN1TYPED_PRIMITIVE_INVALID;
}

static asn1typed_primitive_kind_e
primitive_from_expr(const asn1p_expr_t *expr) {
	if(!expr) return ASN1TYPED_PRIMITIVE_INVALID;
	switch(expr->expr_type) {
	case ASN_BASIC_BOOLEAN: return ASN1TYPED_PRIMITIVE_BOOLEAN;
	case ASN_BASIC_INTEGER: return ASN1TYPED_PRIMITIVE_INTEGER;
	case ASN_STRING_UTF8String: return ASN1TYPED_PRIMITIVE_UTF8_STRING;
	case ASN_STRING_PrintableString: return ASN1TYPED_PRIMITIVE_PRINTABLE_STRING;
	case ASN_STRING_VisibleString: return ASN1TYPED_PRIMITIVE_VISIBLE_STRING;
	case ASN_BASIC_OCTET_STRING: return ASN1TYPED_PRIMITIVE_OCTET_STRING;
	case ASN_BASIC_BIT_STRING:
		/* Typed has no BIT STRING named-bit model; never silently drop it. */
		return expr->members.tq_head ? ASN1TYPED_PRIMITIVE_INVALID :
			ASN1TYPED_PRIMITIVE_BIT_STRING;
	default: return ASN1TYPED_PRIMITIVE_INVALID;
	}
}

static asn1typed_tag_default_e
module_tag_default(const asn1p_module_t *module) {
	if(!module) return ASN1TYPED_TAG_DEFAULT_UNKNOWN;
	if(module->module_flags & MSF_AUTOMATIC_TAGS)
		return ASN1TYPED_TAG_DEFAULT_AUTOMATIC;
	if(module->module_flags & MSF_IMPLICIT_TAGS)
		return ASN1TYPED_TAG_DEFAULT_IMPLICIT;
	if(module->module_flags & MSF_EXPLICIT_TAGS)
		return ASN1TYPED_TAG_DEFAULT_EXPLICIT;
	/* X.680 13.2: empty TagDefault means EXPLICIT TAGS. */
	if((module->module_flags & MSF_MASK_TAGS) == 0)
		return ASN1TYPED_TAG_DEFAULT_EXPLICIT;
	return ASN1TYPED_TAG_DEFAULT_UNKNOWN;
}

static asn1typed_tag_class_e
owned_tag_class(int tag_class) {
	switch(tag_class) {
	case TC_UNIVERSAL: return ASN1TYPED_TAG_CLASS_UNIVERSAL;
	case TC_APPLICATION: return ASN1TYPED_TAG_CLASS_APPLICATION;
	case TC_CONTEXT_SPECIFIC: return ASN1TYPED_TAG_CLASS_CONTEXT_SPECIFIC;
	case TC_PRIVATE: return ASN1TYPED_TAG_CLASS_PRIVATE;
	default: return ASN1TYPED_TAG_CLASS_UNKNOWN;
	}
}

static int
primitive_accepts_exact_size(asn1typed_primitive_kind_e primitive) {
	return primitive == ASN1TYPED_PRIMITIVE_OCTET_STRING ||
		primitive == ASN1TYPED_PRIMITIVE_BIT_STRING;
}

static int
reject_unowned_inline_constraint(const asn1p_expr_t *expr,
		const char *owner, const char *use, char *error, size_t error_size) {
	if(!expr || !expr->constraints) return 0;
	set_error(error, error_size, "%s.%s: inline constrained type is unsupported",
		owner, use);
	return -1;
}

/* Family-C compatibility: recognize only the fixed-tree representation of
 * one complete ContentsConstraint. The contained type is deliberately not
 * transferred into Typed IR; the owner of this field retains opaque bytes. */
static int
is_opaque_contents_constraint(const asn1p_constraint_t *constraint) {
	const asn1p_constraint_t *contents;
	const asn1p_value_t *contained;
	if(!constraint || constraint->type != ACT_CA_SET ||
		constraint->el_count != 1 || !constraint->elements ||
		constraint->value || constraint->containedSubtype ||
		constraint->range_start || constraint->range_stop ||
		!(contents = constraint->elements[0]) ||
		contents->type != ACT_CT_CTNG || contents->el_count != 0 ||
		contents->elements || contents->containedSubtype ||
		contents->range_start || contents->range_stop ||
		!(contained = contents->value) || contained->type != ATV_TYPE ||
		!contained->value.v_type || contained->value.v_type->expr_type == A1TC_INVALID)
		return 0;
	return 1;
}

static int
constraint_bound(const asn1p_value_t *value, intmax_t *out) {
	if(!value || value->type != ATV_INTEGER) return -1;
#if defined(__SIZEOF_INT128__)
	if(value->value.v_integer < (asn1c_integer_t)INTMAX_MIN ||
		value->value.v_integer > (asn1c_integer_t)INTMAX_MAX) return -1;
#endif
	*out = (intmax_t)value->value.v_integer;
	return 0;
}

/* Accept fixed-tree bounded SIZE forms and the bounded exact OCTET STRING
 * form needed by ordinary NGAP dependencies. */
static int
extract_size_constraint(const asn1p_constraint_t *constraint,
		asn1typed_size_constraint_t *out, int accept_exact_size) {
	const asn1p_constraint_t *size, *set, *list, *range;
	int extensible = 0;
	if(!constraint) return 0;
	if(constraint->type != ACT_CA_SET || constraint->el_count != 1 ||
		!constraint->elements || !(size = constraint->elements[0]) ||
		size->type != ACT_CT_SIZE || size->el_count != 1 || !size->elements ||
		!(set = size->elements[0]) || set->type != ACT_CA_SET ||
		set->el_count != 1 || !set->elements || !(list = set->elements[0]))
		return -1;
	if(list->type == ACT_EL_RANGE) {
		range = list;
	} else if(list->type == ACT_CA_CSV && list->elements &&
		list->el_count == 2 && list->elements[0] &&
		list->elements[0]->type == ACT_EL_VALUE && accept_exact_size) {
		const asn1p_constraint_t *extension = list->elements[1];
		intmax_t exact_size;
		if(!extension || extension->type != ACT_EL_EXT ||
			extension->el_count != 0 || extension->elements || extension->value ||
			extension->containedSubtype || extension->range_start ||
			extension->range_stop ||
			constraint_bound(list->elements[0]->value, &exact_size) ||
			exact_size < 0) return -1;
		out->has_size_constraint = 1;
		out->lower_bound = exact_size;
		out->upper_bound = exact_size;
		out->is_extensible = 1;
		return 0;
	} else if(list->type == ACT_CA_CSV && list->elements &&
		(list->el_count == 1 || list->el_count == 2)) {
		range = list->elements[0];
		if(list->el_count == 2) {
			const asn1p_constraint_t *extension = list->elements[1];
			if(!extension || extension->type != ACT_EL_EXT ||
				extension->el_count != 0 || extension->elements ||
				extension->value || extension->containedSubtype ||
				extension->range_start || extension->range_stop) return -1;
			extensible = 1;
		}
	} else if(list->type == ACT_EL_VALUE && accept_exact_size) {
		intmax_t exact_size;
		if(constraint_bound(list->value, &exact_size) || exact_size < 0)
			return -1;
		out->has_size_constraint = 1;
		out->lower_bound = exact_size;
		out->upper_bound = exact_size;
		out->is_extensible = 0;
		return 0;
	} else {
		return -1;
	}
	if(!range || range->range_start == NULL ||
		range->range_stop == NULL ||
		constraint_bound(range->range_start, &out->lower_bound) ||
		constraint_bound(range->range_stop, &out->upper_bound) ||
		out->lower_bound > out->upper_bound) return -1;
	out->has_size_constraint = 1;
	out->is_extensible = extensible;
	return 0;
}

/* Bound bodies may retain direct named INTEGER constraints. Normalize only a
 * direct named integer constant here; Typed IR stores the exact closed range
 * numerically and does not retain parser nodes. */
static int
bound_body_constraint_value(asn1p_t *tree, asn1p_expr_t *context,
		const asn1p_value_t *value, intmax_t *out) {
	asn1p_expr_t *resolved;
	asn1p_ref_t *ref;
	asn1p_value_t integer;
	if(!value) return -1;
	if(value->type == ATV_INTEGER) return constraint_bound(value, out);
	if(value->type != ATV_REFERENCED || !(ref = value->value.reference) ||
		ref->comp_count != 1 || !ref->components ||
		!ref->components[0].name || !ref->components[0].name[0]) return -1;
	resolved = ioc_resolve(tree, context, ref);
	if(!resolved || resolved->meta_type != AMT_VALUE || !resolved->value ||
		resolved->value->type != ATV_INTEGER) return -1;
	memset(&integer, 0, sizeof(integer));
	integer.type = ATV_INTEGER;
	integer.value.v_integer = resolved->value->value.v_integer;
	return constraint_bound(&integer, out);
}

/* Exact bounded SIZE form for the newly owned bound SEQUENCE OF body. */
static int
extract_bound_body_size_constraint(asn1p_t *tree, asn1p_expr_t *context,
		const asn1p_constraint_t *constraint,
		asn1typed_size_constraint_t *out) {
	const asn1p_constraint_t *size, *set, *list, *range;
	int extensible = 0;
	if(!constraint || constraint->type != ACT_CA_SET ||
		constraint->el_count != 1 || !constraint->elements ||
		!(size = constraint->elements[0]) || size->type != ACT_CT_SIZE ||
		size->el_count != 1 || !size->elements ||
		!(set = size->elements[0]) || set->type != ACT_CA_SET ||
		set->el_count != 1 || !set->elements || !(list = set->elements[0]))
		return -1;
	if(list->type == ACT_EL_RANGE) {
		range = list;
	} else if(list->type == ACT_CA_CSV && list->elements &&
		(list->el_count == 1 || list->el_count == 2)) {
		range = list->elements[0];
		if(list->el_count == 2) {
			const asn1p_constraint_t *extension = list->elements[1];
			if(!extension || extension->type != ACT_EL_EXT) return -1;
			extensible = 1;
		}
	} else {
		return -1;
	}
	if(!range || range->type != ACT_EL_RANGE ||
		!range->range_start || !range->range_stop ||
		bound_body_constraint_value(tree, context, range->range_start,
			&out->lower_bound) ||
		bound_body_constraint_value(tree, context, range->range_stop,
			&out->upper_bound) || out->lower_bound > out->upper_bound)
		return -1;
	out->has_size_constraint = 1;
	out->is_extensible = extensible;
	return 0;
}

typedef struct integer_interval_list_s {
	asn1typed_integer_interval_t *items;
	size_t count;
	size_t capacity;
} integer_interval_list_t;

static int
integer_interval_append(integer_interval_list_t *list, intmax_t lower,
		intmax_t upper) {
	asn1typed_integer_interval_t *grown;
	size_t capacity;
	if(lower > upper || list->count == SIZE_MAX / sizeof(*list->items)) return -1;
	if(list->count == list->capacity) {
		capacity = list->capacity ? list->capacity * 2 : 4;
		if(capacity < list->capacity || capacity > SIZE_MAX / sizeof(*list->items))
			capacity = list->count + 1;
		if(capacity > SIZE_MAX / sizeof(*list->items)) return -1;
		grown = (asn1typed_integer_interval_t *)realloc(list->items,
			capacity * sizeof(*list->items));
		if(!grown) return -1;
		list->items = grown;
		list->capacity = capacity;
	}
	list->items[list->count].lower_bound = lower;
	list->items[list->count].upper_bound = upper;
	++list->count;
	return 0;
}

static int
integer_interval_compare(const void *left, const void *right) {
	const asn1typed_integer_interval_t *a =
		(const asn1typed_integer_interval_t *)left;
	const asn1typed_integer_interval_t *b =
		(const asn1typed_integer_interval_t *)right;
	if(a->lower_bound < b->lower_bound) return -1;
	if(a->lower_bound > b->lower_bound) return 1;
	if(a->upper_bound < b->upper_bound) return -1;
	if(a->upper_bound > b->upper_bound) return 1;
	return 0;
}

static int
integer_constraint_node_empty(const asn1p_constraint_t *node) {
	return node && !node->value && !node->containedSubtype &&
		!node->range_start && !node->range_stop;
}

static int
integer_constraint_leaf(const asn1p_constraint_t *node) {
	return node && !node->elements && !node->el_size;
}

static int
collect_integer_union_terms(const asn1p_constraint_t *node,
		integer_interval_list_t *terms) {
	intmax_t lower, upper;
	if(!node || node->containedSubtype) return -1;
	if(node->type == ACT_CA_UNI) {
		unsigned int i;
		if(node->value || node->range_start || node->range_stop ||
			node->el_count < 2 || !node->elements ||
			node->el_size < node->el_count) return -1;
		for(i = 0; i < node->el_count; ++i)
			if(!node->elements[i] ||
				collect_integer_union_terms(node->elements[i], terms)) return -1;
		return 0;
	}
	if(node->el_count != 0 || !integer_constraint_leaf(node)) return -1;
	if(node->type == ACT_EL_RANGE) {
		if(node->value || !node->range_start || !node->range_stop ||
			constraint_bound(node->range_start, &lower) ||
			constraint_bound(node->range_stop, &upper) || lower > upper)
			return -1;
		return integer_interval_append(terms, lower, upper);
	}
	if(node->type == ACT_EL_VALUE) {
		if(node->range_start || node->range_stop || !node->value ||
			node->value->type != ATV_INTEGER ||
			constraint_bound(node->value, &lower)) return -1;
		return integer_interval_append(terms, lower, lower);
	}
	return -1;
}

/* Keep the original inline representation for a simple range. For a bounded
 * UNION, collect into temporary storage, normalize, then publish atomically. */
static int
extract_integer_value_range(const asn1p_constraint_t *constraint,
		asn1typed_integer_value_range_t *out) {
	const asn1p_constraint_t *list, *root, *range;
	integer_interval_list_t terms = {0};
	asn1typed_integer_value_range_t pending = {0};
	int extensible = 0;
	size_t i, normalized;
	if(!constraint) return 0;
	if(!out || constraint->type != ACT_CA_SET || constraint->el_count != 1 ||
		!constraint->elements || constraint->el_size < constraint->el_count ||
		!(list = constraint->elements[0]) ||
		!integer_constraint_node_empty(constraint)) return -1;
	root = list;
	if(list->type == ACT_CA_CSV) {
		if(list->el_count != 2 || !list->elements ||
			list->el_size < list->el_count || !list->elements[0] ||
			!list->elements[1] || !integer_constraint_node_empty(list) ||
			list->elements[1]->type != ACT_EL_EXT ||
			list->elements[1]->el_count != 0 ||
			!integer_constraint_leaf(list->elements[1]) ||
			!integer_constraint_node_empty(list->elements[1])) return -1;
		root = list->elements[0];
		extensible = 1;
	}
	if(root->type == ACT_EL_RANGE) {
		range = root;
		if(range->el_count != 0 || !integer_constraint_leaf(range) ||
			range->value || range->containedSubtype ||
			!range->range_start || !range->range_stop ||
			constraint_bound(range->range_start, &pending.lower_bound) ||
			constraint_bound(range->range_stop, &pending.upper_bound) ||
			pending.lower_bound > pending.upper_bound) return -1;
		pending.has_value_range = 1;
		pending.is_extensible = extensible;
		*out = pending;
		return 0;
	}
	if(root->type != ACT_CA_UNI ||
		collect_integer_union_terms(root, &terms) || terms.count < 2) {
		free(terms.items);
		return -1;
	}
	qsort(terms.items, terms.count, sizeof(*terms.items),
		integer_interval_compare);
	normalized = 0;
	for(i = 0; i < terms.count; ++i) {
		asn1typed_integer_interval_t next = terms.items[i];
		if(normalized) {
			asn1typed_integer_interval_t *previous = &terms.items[normalized - 1];
			int adjacent = previous->upper_bound != INTMAX_MAX &&
				next.lower_bound == previous->upper_bound + 1;
			if(next.lower_bound <= previous->upper_bound || adjacent) {
				if(next.upper_bound > previous->upper_bound)
					previous->upper_bound = next.upper_bound;
				continue;
			}
		}
		terms.items[normalized++] = next;
	}
	if(!normalized || normalized - 1 > SIZE_MAX / sizeof(*pending.tail)) {
		free(terms.items);
		return -1;
	}
	pending.has_value_range = 1;
	pending.lower_bound = terms.items[0].lower_bound;
	pending.upper_bound = terms.items[0].upper_bound;
	pending.is_extensible = extensible;
	pending.tail_count = normalized - 1;
	if(pending.tail_count) {
		pending.tail = (asn1typed_integer_interval_t *)malloc(
			pending.tail_count * sizeof(*pending.tail));
		if(!pending.tail) {
			free(terms.items);
			return -1;
		}
		memcpy(pending.tail, terms.items + 1,
			pending.tail_count * sizeof(*pending.tail));
	}
	free(terms.items);
	*out = pending;
	return 0;
}

static const char *
reference_name(const asn1p_expr_t *expr) {
	if(!expr || !expr->reference || expr->reference->comp_count != 1) return NULL;
	return expr->reference->components[0].name;
}

static asn1p_expr_t *
referenced_type(asn1p_expr_t *expr) {
	if(!expr || expr->expr_type != A1TC_REFERENCE || !expr->reference) return NULL;
	return expr->reference->ref_expr;
}

static asn1typed_type_kind_e
kind_of_type(asn1p_expr_t *expr) {
	if(!expr) return (asn1typed_type_kind_e)-1;
	if(primitive_from_name(reference_name(expr)) != ASN1TYPED_PRIMITIVE_INVALID)
		return ASN1TYPED_TYPE_PRIMITIVE;
	switch(expr->expr_type) {
	case ASN_CONSTR_SEQUENCE: return ASN1TYPED_TYPE_SEQUENCE;
	case ASN_CONSTR_SEQUENCE_OF: return ASN1TYPED_TYPE_SEQUENCE_OF;
	case ASN_CONSTR_CHOICE: return ASN1TYPED_TYPE_CHOICE;
	case ASN_BASIC_ENUMERATED: return ASN1TYPED_TYPE_ENUMERATED;
	default:
		if(primitive_from_expr(expr) != ASN1TYPED_PRIMITIVE_INVALID)
			return ASN1TYPED_TYPE_PRIMITIVE;
		return (asn1typed_type_kind_e)-1;
	}
}

static asn1p_expr_t *
terminal_type(asn1p_expr_t *expr) {
	unsigned hops;
	for(hops = 0; expr && hops < 128; ++hops) {
		asn1p_expr_t *next;
		if(expr->expr_type != A1TC_REFERENCE) return expr;
		if(primitive_from_name(reference_name(expr)) != ASN1TYPED_PRIMITIVE_INVALID)
			return expr;
		next = referenced_type(expr);
		if(!next || next == expr) return NULL;
		expr = next;
	}
	return NULL;
}

static int
put_ref(asn1typed_type_ref_t *ref, asn1p_expr_t *expr) {
	asn1typed_primitive_kind_e primitive;
	if(!expr) return -1;
	primitive = primitive_from_name(reference_name(expr));
	if(primitive == ASN1TYPED_PRIMITIVE_INVALID)
		primitive = primitive_from_expr(expr);
	if(primitive != ASN1TYPED_PRIMITIVE_INVALID)
		return asn1typed_type_ref_init_primitive(ref, primitive);
	if(expr->expr_type == A1TC_REFERENCE) {
		asn1p_expr_t *target = referenced_type(expr);
		if(target && target->meta_type == AMT_TYPE && target->Identifier &&
			target->module && target->module->ModuleName)
			return asn1typed_type_ref_init(ref, target->module->ModuleName,
				target->Identifier);
	}
	return -1;
}

static int put_parameterized_object_set_ref(asn1p_t *tree,
		asn1typed_type_ref_t *ref, asn1p_expr_t *use,
		char *error, size_t error_size);

static int
populate_enumerated_items(asn1typed_type_t *out, asn1p_expr_t *body,
		const char *file, char *error, size_t error_size,
		const char *owner_name) {
	asn1p_expr_t *member;
	int saw_extension_marker = 0;
	int saw_root_item = 0;
	if(!out || out->kind != ASN1TYPED_TYPE_ENUMERATED || !body ||
		body->expr_type != ASN_BASIC_ENUMERATED || body->rhs_pspecs ||
		body->constraints || body->combined_constraints) {
		set_error(error, error_size,
			"%s: unsupported inline ENUMERATED body", owner_name);
		return -1;
	}
	TQ_FOR(member, &body->members, next) {
		if(member->expr_type == A1TC_EXTENSIBLE) {
			if(saw_extension_marker || !saw_root_item) {
				set_error(error, error_size,
					"%s: unsupported ENUMERATED extension marker placement",
					owner_name);
				return -1;
			}
			saw_extension_marker = 1;
			out->is_extensible = 1;
			continue;
		}
		if(member->meta_type != AMT_VALUE ||
			member->expr_type != A1TC_UNIVERVAL || !member->Identifier ||
			!member->Identifier[0] ||
			asn1typed_type_add_enum_item_ex(out, member->Identifier,
				saw_extension_marker, file, member->_lineno)) {
			set_error(error, error_size, "%s: invalid ENUMERATED item",
				owner_name);
			return -1;
		}
		{
			size_t index = out->enum_item_count - 1;
			intmax_t number;
			if(!member->value || member->value->type != ATV_INTEGER)
				asn1typed_enum_item_set_numeric_unavailable(out, index);
			else if(constraint_bound(member->value, &number))
				asn1typed_enum_item_set_numeric_unsupported(out, index);
			else asn1typed_enum_item_set_numeric_evidence(out, index, number);
		}
		if(!saw_extension_marker) saw_root_item = 1;
	}
	if(!saw_root_item) {
		set_error(error, error_size, "%s: ENUMERATED requires a root item",
			owner_name);
		return -1;
	}
	{
		asn1typed_wire_finalize_result_e finalized =
			asn1typed_enumerated_evidence_finalize(out, error, error_size);
		if(finalized == ASN1TYPED_WIRE_FINALIZE_ERROR) return -1;
		if(finalized == ASN1TYPED_WIRE_FINALIZE_UNAVAILABLE && error && error_size)
			error[0] = 0;
	}
	return 0;
}

static int
add_field(asn1p_t *tree, asn1typed_type_t *type, asn1p_expr_t *field,
		const char *file, const char *module, char *error, size_t error_size) {
	asn1typed_presence_e presence;
	asn1typed_size_constraint_t field_size = {0};
	asn1typed_integer_value_range_t field_value_range = {0};
	int marker_flags;
	int owns_inline_size = 0;
	int owns_inline_integer_range = 0;
	const asn1p_constraint_t *inline_constraint = field->combined_constraints ?
		field->combined_constraints : field->constraints;
	int result;
	if(!field->Identifier) {
		set_error(error, error_size, "%s: unnamed SEQUENCE component at line %d",
			module, field->_lineno);
		return -1;
	}
	marker_flags = field->marker.flags;
	if((marker_flags & EM_DEFAULT) == EM_DEFAULT) {
		set_error(error, error_size, "%s.%s: DEFAULT presence is unsupported",
			module, field->Identifier);
		return -1;
	}
	presence = (marker_flags & EM_OPTIONAL) == EM_OPTIONAL ?
		ASN1TYPED_PRESENCE_OPTIONAL : ASN1TYPED_PRESENCE_MANDATORY;
	if(field->expr_type == ASN_CONSTR_SEQUENCE ||
		field->expr_type == ASN_CONSTR_SEQUENCE_OF ||
		field->expr_type == ASN_CONSTR_CHOICE ||
		field->expr_type == ASN_CONSTR_SET ||
		field->expr_type == ASN_CONSTR_SET_OF) {
		set_error(error, error_size, "%s.%s: inline constructed field is unsupported",
			module, field->Identifier);
		return -1;
	}
	if(field->constraints) {
		asn1typed_primitive_kind_e primitive = primitive_from_expr(field);
		if(field->meta_type != AMT_TYPE || field->rhs_pspecs ||
			primitive == ASN1TYPED_PRIMITIVE_INVALID) {
			if(reject_unowned_inline_constraint(field, module, field->Identifier,
					error, error_size)) return -1;
			return -1;
		}
		if(primitive == ASN1TYPED_PRIMITIVE_OCTET_STRING &&
			is_opaque_contents_constraint(field->constraints) &&
			(!field->combined_constraints ||
			 is_opaque_contents_constraint(field->combined_constraints))) {
			/* Existing OCTET STRING field ownership is sufficient. */
		} else if(primitive == ASN1TYPED_PRIMITIVE_INTEGER) {
			if(extract_integer_value_range(inline_constraint,
					&field_value_range) || !field_value_range.has_value_range) {
				set_error(error, error_size,
					"%s.%s: unsupported inline INTEGER constraint", module,
					field->Identifier);
				return -1;
			}
			owns_inline_integer_range = 1;
		} else {
			if(extract_size_constraint(field->constraints, &field_size,
					primitive_accepts_exact_size(primitive))) {
				if(reject_unowned_inline_constraint(field, module, field->Identifier,
						error, error_size)) return -1;
				return -1;
			}
			owns_inline_size = 1;
		}
	}
	if(field->meta_type == AMT_TYPE &&
		field->expr_type == ASN_BASIC_ENUMERATED) {
		asn1typed_type_t body;
		memset(&body, 0, sizeof(body));
		body.kind = ASN1TYPED_TYPE_ENUMERATED;
		if(field->rhs_pspecs || populate_enumerated_items(&body, field, file,
				error, error_size, field->Identifier)) {
			asn1typed_type_clear(&body);
			return -1;
		}
		result = asn1typed_type_add_inline_enumerated_field(type,
			field->Identifier, &body, presence, file, field->_lineno);
		asn1typed_type_clear(&body);
		if(result) {
			set_error(error, error_size,
				"%s.%s: out of memory extracting inline ENUMERATED field",
				module, field->Identifier);
			return -1;
		}
		return 0;
	}
	{
		asn1typed_type_ref_t ref;
		memset(&ref, 0, sizeof(ref));
		if(field->rhs_pspecs ?
			put_parameterized_object_set_ref(tree, &ref, field,
				error, error_size) : put_ref(&ref, field)) {
			if(field->rhs_pspecs) {
				free(field_value_range.tail);
				return -1;
			}
			set_error(error, error_size, "%s.%s: unsupported field type at line %d",
				module, field->Identifier, field->_lineno);
			free(field_value_range.tail);
			return -1;
		}
		if(field->rhs_pspecs) {
			result = asn1typed_type_add_field_ref(type, field->Identifier,
				&ref, presence, file, field->_lineno);
		} else if(ref.kind == ASN1TYPED_REF_PRIMITIVE) {
			result = asn1typed_type_add_primitive_field(type, field->Identifier,
				ref.primitive_kind, presence, file, field->_lineno);
		} else {
			result = asn1typed_type_add_field(type, field->Identifier,
				ref.module, ref.source_name, presence, file, field->_lineno);
		}
		asn1typed_type_ref_clear(&ref);
		if(!result && owns_inline_size)
			type->fields[type->field_count - 1].size_constraint = field_size;
		if(!result && owns_inline_integer_range) {
			type->fields[type->field_count - 1].value_range = field_value_range;
			field_value_range.tail = NULL;
		}
		if(result) {
			set_error(error, error_size, "%s.%s: out of memory extracting field",
				module, field->Identifier);
			free(field_value_range.tail);
			return -1;
		}
	}
	return 0;
}

static int
populate_type(asn1p_t *tree, asn1typed_type_t *out, asn1p_expr_t *decl,
		const char *file, char *error, size_t error_size) {
	asn1p_expr_t *body = terminal_type(decl);
	asn1p_expr_t *member;
	if(!body) {
		set_error(error, error_size, "%s: unresolved or cyclic type reference",
			decl->Identifier);
		return -1;
	}
	switch(out->kind) {
	case ASN1TYPED_TYPE_PRIMITIVE: {
		asn1typed_primitive_kind_e primitive = primitive_from_expr(body);
		asn1typed_size_constraint_t size_constraint = {0};
		asn1typed_integer_value_range_t value_range = {0};
		const asn1p_constraint_t *constraint = decl->combined_constraints ?
			decl->combined_constraints : decl->constraints;
		if(primitive == ASN1TYPED_PRIMITIVE_INVALID)
			primitive = primitive_from_name(reference_name(body));
		if(asn1typed_type_set_primitive(out, primitive)) {
			set_error(error, error_size, "%s: unsupported primitive type",
				decl->Identifier);
			return -1;
		}
		if((primitive == ASN1TYPED_PRIMITIVE_INTEGER ?
			extract_integer_value_range(constraint, &value_range) :
			extract_size_constraint(constraint, &size_constraint,
				primitive_accepts_exact_size(primitive)))) {
			set_error(error, error_size,
				"%s: unsupported or unrepresentable primitive constraint",
				decl->Identifier);
			return -1;
		}
		out->size_constraint = size_constraint;
		out->value_range = value_range;
		return 0;
	}
	case ASN1TYPED_TYPE_SEQUENCE: {
		int saw_extension_marker = 0;
		size_t physical_roots = 0;
		int one_to_one = 1;
		TQ_FOR(member, &body->members, next) {
			if(member->expr_type == A1TC_EXTENSIBLE) {
				if(saw_extension_marker) {
					set_error(error, error_size,
						"%s: multiple SEQUENCE extension markers are unsupported",
						decl->Identifier);
					return -1;
				}
				saw_extension_marker = 1;
				out->is_extensible = 1;
				continue;
			}
			if(saw_extension_marker) {
				set_error(error, error_size,
					"%s: SEQUENCE component after extension marker is unsupported",
					decl->Identifier);
				return -1;
			}
			{
				size_t before = out->field_count;
				if(add_field(tree, out, member, file, decl->module->ModuleName,
						error, error_size)) return -1;
				if(out->field_count <= before || out->field_count - before != 1 || physical_roots == SIZE_MAX)
					one_to_one = 0;
				else ++physical_roots;
			}
		}
		if(saw_extension_marker) {
			if(!one_to_one) asn1typed_sequence_set_extension_unsupported(out);
			else if(asn1typed_sequence_set_extension_structure(out, physical_roots, 0) ||
				asn1typed_sequence_extension_structure_finalize(out, error, error_size) != ASN1TYPED_WIRE_FINALIZE_OK) {
				if(error && error_size && !error[0]) set_error(error, error_size, "could not finalize owned SEQUENCE extension structure");
				return -1;
			}
		}
		return 0;
	}
	case ASN1TYPED_TYPE_SEQUENCE_OF:
		{
			asn1typed_size_constraint_t size_constraint = {0};
			const asn1p_constraint_t *constraint = decl->combined_constraints ?
				decl->combined_constraints : decl->constraints;
			if(extract_size_constraint(constraint, &size_constraint, 1) ||
				(size_constraint.has_size_constraint && size_constraint.lower_bound < 0)) {
				set_error(error, error_size,
					"%s: unsupported or unrepresentable SEQUENCE OF SIZE constraint",
					decl->Identifier);
				return -1;
			}
			out->size_constraint = size_constraint;
		}
		member = TQ_FIRST(&body->members);
		if(!member || TQ_NEXT(member, next)) {
			set_error(error, error_size, "%s: malformed SEQUENCE OF element",
				decl->Identifier);
			return -1;
		}
		if(member->rhs_pspecs) {
			set_error(error, error_size,
				"%s: parameterized SEQUENCE OF element reference is unsupported",
				decl->Identifier);
			return -1;
		}
		if(reject_unowned_inline_constraint(member, decl->module->ModuleName,
				decl->Identifier, error, error_size)) return -1;
		{
			asn1typed_type_ref_t ref;
			int rc;
			memset(&ref, 0, sizeof(ref));
			if(put_ref(&ref, member)) {
				set_error(error, error_size, "%s: unsupported SEQUENCE OF element type",
					decl->Identifier);
				return -1;
			}
			if(ref.kind == ASN1TYPED_REF_PRIMITIVE)
				rc = asn1typed_type_set_element_primitive(out, ref.primitive_kind);
			else
				rc = asn1typed_type_set_element_type(out, ref.module, ref.source_name);
			asn1typed_type_ref_clear(&ref);
			if(rc) {
				set_error(error, error_size, "%s: out of memory storing element type",
					decl->Identifier);
				return -1;
			}
		}
		return 0;
	case ASN1TYPED_TYPE_ENUMERATED:
		return populate_enumerated_items(out, body, file, error, error_size,
			decl->Identifier);
	case ASN1TYPED_TYPE_CHOICE:
		{
			asn1typed_wire_finalize_result_e finalize_result;
			TQ_FOR(member, &body->members, next) {
				if(member->expr_type == A1TC_EXTENSIBLE) {
					set_error(error, error_size,
						"%s: CHOICE extension marker/additions are unsupported",
						decl->Identifier);
					return -1;
				}
			}
		TQ_FOR(member, &body->members, next) {
			asn1typed_type_ref_t ref;
			asn1typed_size_constraint_t alternative_size = {0};
			asn1typed_integer_value_range_t alternative_value_range = {0};
			const asn1typed_size_constraint_t *size_ptr = NULL;
			const asn1typed_integer_value_range_t *value_range_ptr = NULL;
			if(!member->Identifier) {
				set_error(error, error_size,
					"%s: unnamed CHOICE alternative at line %d",
					decl->Identifier, member->_lineno);
				return -1;
			}
			if(member->constraints) {
				asn1typed_primitive_kind_e primitive = primitive_from_expr(member);
				if(member->meta_type != AMT_TYPE || member->rhs_pspecs ||
					primitive == ASN1TYPED_PRIMITIVE_INVALID) {
					if(reject_unowned_inline_constraint(member, decl->Identifier,
							member->Identifier, error, error_size)) return -1;
					return -1;
				}
				if(primitive == ASN1TYPED_PRIMITIVE_INTEGER) {
					const asn1p_constraint_t *constraint =
						member->combined_constraints ? member->combined_constraints :
						member->constraints;
					if(extract_integer_value_range(constraint,
							&alternative_value_range) ||
						!alternative_value_range.has_value_range) {
						if(reject_unowned_inline_constraint(member,
								decl->Identifier, member->Identifier,
								error, error_size)) return -1;
						return -1;
					}
					value_range_ptr = &alternative_value_range;
				} else {
					if(extract_size_constraint(member->constraints,
							&alternative_size,
							primitive_accepts_exact_size(primitive))) {
						if(reject_unowned_inline_constraint(member,
								decl->Identifier, member->Identifier,
								error, error_size)) return -1;
						return -1;
					}
					size_ptr = &alternative_size;
				}
			}
			memset(&ref, 0, sizeof(ref));
			if(member->rhs_pspecs ?
				put_parameterized_object_set_ref(tree, &ref,
					member, error, error_size) : put_ref(&ref, member)) {
				if(member->rhs_pspecs) {
					free(alternative_value_range.tail);
					return -1;
				}
				set_error(error, error_size,
					"%s.%s: unsupported or unresolved CHOICE alternative type at line %d",
					decl->Identifier, member->Identifier, member->_lineno);
				free(alternative_value_range.tail);
				return -1;
			}
			if(asn1typed_type_add_choice_alternative(out, member->Identifier,
					&ref, size_ptr, value_range_ptr, file, member->_lineno)) {
				asn1typed_type_ref_clear(&ref);
				free(alternative_value_range.tail);
				set_error(error, error_size,
					"%s.%s: could not store CHOICE alternative",
					decl->Identifier, member->Identifier);
				return -1;
			}
			{
				size_t alternative_index = out->alternative_count - 1;
				asn1typed_tag_class_e tag_class =
					owned_tag_class(member->tag.tag_class);
				if(tag_class != ASN1TYPED_TAG_CLASS_UNKNOWN &&
					member->tag.tag_value >= 0 &&
					member->tag.tag_value <= INTMAX_MAX) {
					if(asn1typed_choice_alternative_set_wire_evidence(
						out, alternative_index, tag_class,
						(intmax_t)member->tag.tag_value)) {
						asn1typed_type_ref_clear(&ref);
						free(alternative_value_range.tail);
						set_error(error, error_size,
							"%s.%s: could not store CHOICE tag evidence",
							decl->Identifier, member->Identifier);
						return -1;
					}
				} else if(member->tag.tag_class != TC_NOCLASS) {
					asn1typed_choice_alternative_set_wire_unsupported(out,
						alternative_index);
				}
			}
			asn1typed_type_ref_clear(&ref);
			free(alternative_value_range.tail);
		}
		if(!out->alternative_count) {
			set_error(error, error_size, "%s: empty CHOICE is unsupported",
				decl->Identifier);
			return -1;
		}
		finalize_result = asn1typed_choice_wire_evidence_finalize(out,
			error, error_size);
		if(finalize_result == ASN1TYPED_WIRE_FINALIZE_ERROR) {
			if(!error || !error_size || !error[0])
				set_error(error, error_size,
					"%s: internal CHOICE wire evidence finalization failure",
					decl->Identifier);
			return -1;
		}
		if(finalize_result == ASN1TYPED_WIRE_FINALIZE_UNAVAILABLE &&
			error && error_size) error[0] = '\0';
		}
		return 0;
	default:
		set_error(error, error_size, "%s: unsupported ASN.1 type", decl->Identifier);
		return -1;
	}
}

int
asn1typed_extract_module(asn1p_t *tree, const char *module_name,
		asn1typed_module_t *out, char *error, size_t error_size) {
	asn1p_module_t *source, *mod;
	asn1p_expr_t *decl;
	const char *file;
	unsigned first_line = 1;
	if(error && error_size) error[0] = '\0';
	if(!tree || !module_name || !out) {
		set_error(error, error_size, "invalid extractor arguments");
		return -1;
	}
	memset(out, 0, sizeof(*out));
	source = NULL;
	TQ_FOR(mod, &tree->modules, mod_next) {
		if(mod->ModuleName && !strcmp(mod->ModuleName, module_name)) {
			source = mod;
			break;
		}
	}
	if(!source) {
		set_error(error, error_size, "ASN.1 module '%s' not found", module_name);
		return -1;
	}
	file = source->source_file_name ? source->source_file_name : "<unknown>";
	decl = TQ_FIRST(&source->members);
	if(decl) first_line = decl->_lineno > 0 ? (unsigned)decl->_lineno : 1;
	if(asn1typed_module_init(out, source->ModuleName, file, first_line)) {
		set_error(error, error_size, "%s: out of memory creating IR module", module_name);
		return -1;
	}
	out->tag_default = module_tag_default(source);
	TQ_FOR(decl, &source->members, next) {
		asn1typed_type_kind_e kind;
		asn1typed_type_t *type;
		if(decl->meta_type != AMT_TYPE || !decl->Identifier) continue;
		kind = kind_of_type(terminal_type(decl));
		if(kind == (asn1typed_type_kind_e)-1) {
			set_error(error, error_size, "%s: unsupported ASN.1 construct at line %d",
				decl->Identifier, decl->_lineno);
			goto fail;
		}
		/* Complete this type before adding another: add_type may realloc types. */
		if(asn1typed_module_add_type(out, decl->Identifier, kind, file,
				decl->_lineno > 0 ? (unsigned)decl->_lineno : 0, &type)) {
			set_error(error, error_size, "%s: out of memory adding IR type",
				decl->Identifier);
			goto fail;
		}
		if(populate_type(tree, type, decl, file, error, error_size)) goto fail;
	}
	return 0;
fail:
	asn1typed_module_clear(out);
	return -1;
}

/* IOC settings and actual parameters are not all dereferenced by the fixer.
 * Resolve those through its namespace rules, never a global name scan. */
static asn1p_expr_t *
ioc_resolve(asn1p_t *tree, asn1p_expr_t *context, asn1p_ref_t *ref) {
	asn1_namespace_t *ns;
	asn1p_expr_t *result;
	asn1p_ref_t lookup;
	if(!context || !context->module || !ref || !ref->comp_count ||
		!ref->components) return NULL;
	if(ref->ref_expr) return ref->ref_expr;
	ns = asn1_namespace_new_from_module(context->module, 1);
	if(!ns) return NULL;
	lookup = *ref;
	result = asn1f_lookup_symbol_ex(tree, ns, context, &lookup);
	asn1_namespace_free(ns);
	return result;
}

static asn1p_expr_t *
named_declaration(asn1p_module_t *module, const char *name) {
	asn1p_expr_t *decl;
	TQ_FOR(decl, &module->members, next)
		if(decl->Identifier && !strcmp(decl->Identifier, name)) return decl;
	return NULL;
}

/* The fixed parameter and table-constraint representations both contain a
 * reference to the actual object set, sometimes wrapped as a value set. */
static asn1p_ref_t *
ioc_set_reference(asn1p_value_t *value) {
	asn1p_constraint_t *ct;
	if(!value) return NULL;
	if(value->type == ATV_REFERENCED) return value->value.reference;
	if(value->type == ATV_TYPE) {
		asn1p_expr_t *expr = value->value.v_type;
		return expr && expr->expr_type == A1TC_REFERENCE ? expr->reference : NULL;
	}
	if(value->type != ATV_VALUESET) return NULL;
	ct = value->value.constraint;
	if(!ct || ct->type != ACT_EL_TYPE) return NULL;
	value = ct->containedSubtype;
	if(!value) return NULL;
	if(value->type == ATV_REFERENCED) return value->value.reference;
	if(value->type == ATV_TYPE) {
		asn1p_expr_t *expr = value->value.v_type;
		return expr && expr->expr_type == A1TC_REFERENCE ? expr->reference : NULL;
	}
	return NULL;
}

static asn1p_expr_t *
ioc_actual_set(asn1p_t *tree, asn1p_expr_t *expr) {
	asn1p_expr_t *parameter, *set;
	asn1p_constraint_t *ct;
	if(!expr || !expr->rhs_pspecs) return NULL;
	parameter = TQ_FIRST(&expr->rhs_pspecs->members);
	if(!parameter || TQ_NEXT(parameter, next)) return NULL;
	ct = parameter->constraints;
	if(!ct || ct->type != ACT_EL_TYPE) return NULL;
	set = ioc_resolve(tree, parameter, ioc_set_reference(ct->containedSubtype));
	return set && set->meta_type == AMT_VALUESET ? set : NULL;
}

static int
simple_object_set_setting(asn1p_value_t *value) {
	if(!value) return 0;
	if(value->type == ATV_VALUESET) {
		asn1p_constraint_t *ct = value->value.constraint;
		if(!ct || ct->type != ACT_EL_TYPE || !ct->containedSubtype) return 0;
		value = ct->containedSubtype;
	}
	if(value->type == ATV_REFERENCED) return value->value.reference != NULL;
	if(value->type == ATV_TYPE) {
		asn1p_expr_t *expr = value->value.v_type;
		return expr && expr->expr_type == A1TC_REFERENCE && expr->reference &&
			!expr->rhs_pspecs;
	}
	return 0;
}

/* Normalize the single B7a object-set actual carried by one parameter. The
 * returned strings and expression are borrowed from the fixed tree. */
static int
object_set_actual(asn1p_t *tree, asn1p_expr_t *parameter,
		asn1typed_type_actual_t *actual, asn1p_expr_t **set_out) {
	asn1p_ref_t *ref;
	asn1p_expr_t *set;
	if(set_out) *set_out = NULL;
	if(!tree || !parameter || !actual || !parameter->constraints ||
		parameter->constraints->type != ACT_EL_TYPE ||
		!simple_object_set_setting(parameter->constraints->containedSubtype))
		return -1;
	ref = ioc_set_reference(parameter->constraints->containedSubtype);
	if(!ref || ref->comp_count != 1 || !ref->components ||
		!ref->components[0].name || !*ref->components[0].name) return -1;
	set = ioc_resolve(tree, parameter, ref);
	if(!set || set->meta_type != AMT_VALUESET || !set->Identifier ||
		!set->Identifier[0] || !set->module || !set->module->ModuleName ||
		!set->module->ModuleName[0]) return -1;
	actual->kind = ASN1TYPED_ACTUAL_OBJECT_SET_REFERENCE;
	actual->module = set->module->ModuleName;
	actual->source_name = set->Identifier;
	if(set_out) *set_out = set;
	return 0;
}

static int
class_field_governor_matches(asn1p_t *tree, asn1p_expr_t *generic,
		size_t parameter_index, asn1p_expr_t *set) {
	asn1p_expr_t *class_expr;
	if(!generic || !generic->lhs_params ||
		parameter_index >= (size_t)generic->lhs_params->params_count ||
		!generic->lhs_params->params[parameter_index].governor || !set ||
		!set->reference) return 0;
	class_expr = ioc_resolve(tree, generic,
		generic->lhs_params->params[parameter_index].governor);
	return class_expr && class_expr->expr_type == A1TC_CLASSDEF &&
		ioc_resolve(tree, set, set->reference) == class_expr;
}

/* Recover the original generic declaration through its specialization table.
 * Clone metadata is deliberately not copied into the owned reference. */
static asn1p_expr_t *
generic_for_specialization(asn1p_t *tree, asn1p_expr_t *specialization) {
	asn1p_module_t *module;
	asn1p_expr_t *decl;
	if(!tree || !specialization) return NULL;
	TQ_FOR(module, &tree->modules, mod_next) {
		TQ_FOR(decl, &module->members, next) {
			int i;
			if(!decl->lhs_params || !decl->Identifier || !decl->module) continue;
			for(i = 0; i < decl->specializations.pspecs_count; ++i)
				if(decl->specializations.pspec[i].my_clone == specialization)
					return decl;
		}
	}
	return NULL;
}

static int
put_parameterized_object_set_ref(asn1p_t *tree, asn1typed_type_ref_t *ref,
		asn1p_expr_t *use, char *error, size_t error_size) {
	asn1p_expr_t *specialization, *generic, *formal_class, *actual_set;
	asn1p_expr_t *parameter;
	asn1typed_type_actual_t actual;
	if(!tree || !ref || !use || !use->rhs_pspecs || !use->reference ||
		use->reference->comp_count != 1 || !use->reference->components ||
		!use->reference->components[0].name || !*use->reference->components[0].name) {
		set_error(error, error_size, "%s.%s: malformed parameterized type reference",
			use && use->parent_expr && use->parent_expr->Identifier ?
				use->parent_expr->Identifier : "CHOICE",
			use && use->Identifier ? use->Identifier : "<unnamed>");
		return -1;
	}
	specialization = ioc_resolve(tree, use, use->reference);
	generic = generic_for_specialization(tree, specialization);
	if(!generic || generic->lhs_params->params_count != 1 ||
		!generic->lhs_params->params[0].governor ||
		!generic->lhs_params->params[0].governor->comp_count) {
		set_error(error, error_size, "%s.%s: unsupported parameterized target/formal",
			generic && generic->Identifier ? generic->Identifier :
				use->reference->components[0].name,
			use->Identifier ? use->Identifier : "<unnamed>");
		return -1;
	}
	formal_class = ioc_resolve(tree, generic,
		generic->lhs_params->params[0].governor);
	parameter = TQ_FIRST(&use->rhs_pspecs->members);
	if(!parameter || TQ_NEXT(parameter, next) || !parameter->constraints ||
		parameter->constraints->type != ACT_EL_TYPE ||
		!simple_object_set_setting(parameter->constraints->containedSubtype)) {
		set_error(error, error_size, "%s.%s: unsupported actual parameter representation",
			generic->Identifier, use->Identifier ? use->Identifier : "<unnamed>");
		return -1;
	}
	if(object_set_actual(tree, parameter, &actual, &actual_set)) {
		set_error(error, error_size, "%s.%s: unresolved or non-object-set actual '%s'",
			generic->Identifier, use->Identifier ? use->Identifier : "<unnamed>",
			parameter->Identifier ? parameter->Identifier : "<unnamed>");
		return -1;
	}
	if(!formal_class || formal_class->expr_type != A1TC_CLASSDEF ||
		!class_field_governor_matches(tree, generic, 0, actual_set)) {
		set_error(error, error_size, "%s.%s: object-set actual is incompatible with formal",
			generic->Identifier, use->Identifier ? use->Identifier : "<unnamed>");
		return -1;
	}
	if(asn1typed_type_ref_init_parameterized(ref, generic->module->ModuleName,
			generic->Identifier, &actual, 1)) {
		set_error(error, error_size, "%s.%s: out of memory storing parameterized reference",
			generic->Identifier, use->Identifier ? use->Identifier : "<unnamed>");
		return -1;
	}
	return 0;
}

/* Accept exactly the bounded table/component relation, not an arbitrary
 * constraint containing a suitable-looking descendant. */
static const asn1p_constraint_t *
ioc_relation(asn1p_expr_t *component) {
	const asn1p_constraint_t *ct = component->constraints;
	if(ct && ct->type == ACT_CA_SET && ct->el_count == 1 && ct->elements)
		ct = ct->elements[0];
	return ct && ct->type == ACT_CA_CRC && ct->elements ? ct : NULL;
}

static int
validate_ioc_element(asn1p_t *tree, asn1p_expr_t *container, asn1p_expr_t *set) {
	asn1p_expr_t *element = TQ_FIRST(&container->members), *body, *member;
	asn1p_expr_t *class_expr, *id = NULL;
	const asn1p_constraint_t *criticality_ct = NULL, *value_ct = NULL;
	unsigned seen = 0;
	if(!element || TQ_NEXT(element, next) || element->expr_type != A1TC_REFERENCE ||
		ioc_actual_set(tree, element) != set) return -1;
	body = terminal_type(ioc_resolve(tree, element, element->reference));
	if(!body || body->expr_type != ASN_CONSTR_SEQUENCE) return -1;
	class_expr = ioc_resolve(tree, set, set->reference);
	if(!class_expr || class_expr->expr_type != A1TC_CLASSDEF) return -1;
	TQ_FOR(member, &body->members, next) {
		asn1p_ref_t prefix, *ref = member->reference;
		const asn1p_constraint_t *ct = ioc_relation(member), *setting;
		const char *name;
		unsigned bit;
		if(member->expr_type != A1TC_REFERENCE || member->marker.flags != EM_NOMARK ||
			!ref || ref->comp_count != 2 || !ref->components ||
			!ref->components[0].name || !(name = ref->components[1].name)) return -1;
		if(!strcmp(name, "&id")) bit = 1;
		else if(!strcmp(name, "&criticality")) bit = 2;
		else if(!strcmp(name, "&Value")) bit = 4;
		else return -1;
		if(seen & bit) return -1;
		seen |= bit;
		prefix = *ref;
		prefix.comp_count = 1;
		prefix.ref_expr = NULL; /* Full reference denotes the field, not its class. */
		if(ioc_resolve(tree, member, &prefix) != class_expr || !ct ||
			(bit == 1 ? ct->el_count != 1u :
			 bit == 2 ? (ct->el_count < 1u || ct->el_count > 2u) :
			 ct->el_count != 2u)) return -1;
		setting = ct->elements[0];
		if(!setting || setting->type != ACT_EL_VALUE || setting->el_count ||
			ioc_resolve(tree, member, ioc_set_reference(setting->value)) != set) return -1;
		if(bit == 1) id = member;
		if(bit == 2) criticality_ct = ct;
		if(bit == 4) value_ct = ct;
	}
	if(seen != 7 || !id || !id->Identifier || !criticality_ct || !value_ct) return -1;
	{
		const asn1p_constraint_t *relations[2];
		size_t relation_count = 0;
		size_t i;
		if(criticality_ct->el_count == 2) {
			if(!criticality_ct->elements[1]) return -1;
			relations[relation_count++] = criticality_ct->elements[1];
		}
		if(!value_ct->elements[1]) return -1;
		relations[relation_count++] = value_ct->elements[1];
		for(i = 0; i < relation_count; ++i) {
			const asn1p_constraint_t *at = relations[i];
			asn1p_ref_t *ref;
			const char *path;
			if(at->type != ACT_EL_VALUE || at->el_count || !at->value ||
				at->value->type != ATV_REFERENCED) return -1;
			ref = at->value->value.reference;
			if(!ref || ref->comp_count != 1 || !ref->components ||
				!(path = ref->components[0].name) || path[0] != '@' ||
				strcmp(path + 1, id->Identifier)) return -1;
		}
	}
	return 0;
}

/* One mandatory parameterized SEQUENCE OF whose element uses the same set
 * for its class fields and selects &Value through its &id component. */
static asn1p_expr_t *
message_object_set(asn1p_t *tree, asn1p_expr_t *message) {
	asn1p_expr_t *body = terminal_type(message);
	asn1p_expr_t *field, *marker, *container, *set;
	if(!body || body->expr_type != ASN_CONSTR_SEQUENCE) return NULL;
	field = TQ_FIRST(&body->members);
	if(!field || field->marker.flags != EM_NOMARK ||
		field->expr_type != A1TC_REFERENCE || !field->rhs_pspecs) return NULL;
	/* An extension marker carries no payload and does not change which
	 * parameterized field supplies the object-set association. Keep the shape
	 * bounded to one trailing marker; other sequence members remain invalid. */
	marker = TQ_NEXT(field, next);
	if(marker && (marker->expr_type != A1TC_EXTENSIBLE ||
		TQ_NEXT(marker, next))) return NULL;
	container = terminal_type(ioc_resolve(tree, field, field->reference));
	if(!container || container->expr_type != ASN_CONSTR_SEQUENCE_OF) return NULL;
	set = ioc_actual_set(tree, field);
	if(!set || validate_ioc_element(tree, container, set)) return NULL;
	return set;
}

/* The semantically identified IOC &criticality setting retains its source
 * identity when fixing resolves an enum reference to an integer. */
static const char *
ioc_criticality_identity(asn1p_expr_t *setting) {
	asn1p_ref_t *ref;
	size_t i;
	if(!setting || setting->meta_type != AMT_VALUE || !setting->Identifier ||
		!*setting->Identifier || !setting->value) return NULL;
	if(setting->value->type == ATV_INTEGER) return setting->Identifier;
	if(setting->value->type != ATV_REFERENCED) return NULL;
	ref = setting->value->value.reference;
	if(!ref || !ref->components || !ref->comp_count || ref->comp_count > 2)
		return NULL;
	for(i = 0; i < ref->comp_count; ++i)
		if(!ref->components[i].name || !*ref->components[i].name) return NULL;
	if(strcmp(ref->components[ref->comp_count - 1].name, setting->Identifier))
		return NULL;
	return setting->Identifier;
}

/* IOC &presence keeps its semantic identity in Identifier after fixing may
 * replace the source reference with its resolved integer value. */
static const char *
ioc_presence_identity(asn1p_expr_t *setting) {
	asn1p_ref_t *ref;
	size_t i;
	if(!setting || setting->meta_type != AMT_VALUE || !setting->Identifier ||
		!*setting->Identifier || !setting->value) return NULL;
	if(setting->value->type == ATV_INTEGER) return setting->Identifier;
	if(setting->value->type != ATV_REFERENCED) return NULL;
	ref = setting->value->value.reference;
	if(!ref || !ref->components || !ref->comp_count || ref->comp_count > 2)
		return NULL;
	for(i = 0; i < ref->comp_count; ++i)
		if(!ref->components[i].name || !*ref->components[i].name) return NULL;
	if(strcmp(ref->components[ref->comp_count - 1].name, setting->Identifier))
		return NULL;
	return setting->Identifier;
}

/* An IOC &id setting carries its source name independently of its value:
 * fixing may replace ATV_REFERENCED with the resolved ATV_INTEGER in place. */
static int
ioc_id_identity(asn1p_t *tree, asn1p_expr_t *setting,
		const char **symbol, int *has_numeric_id, intmax_t *numeric_id) {
	asn1p_ref_t *ref;
	asn1p_expr_t *resolved;
	asn1c_integer_t integer;
	size_t i;
	if(!setting || setting->meta_type != AMT_VALUE || !setting->Identifier ||
		!*setting->Identifier || !setting->value) return -1;
	*symbol = setting->Identifier;
	*has_numeric_id = 0;
	*numeric_id = 0;
	if(setting->value->type == ATV_INTEGER) {
		integer = setting->value->value.v_integer;
	} else if(setting->value->type == ATV_REFERENCED) {
		ref = setting->value->value.reference;
		if(!ref || !ref->components || !ref->comp_count || ref->comp_count > 2)
			return -1;
		for(i = 0; i < ref->comp_count; ++i)
			if(!ref->components[i].name || !*ref->components[i].name) return -1;
		if(strcmp(ref->components[ref->comp_count - 1].name,
				setting->Identifier)) return -1;
		resolved = ioc_resolve(tree, setting, ref);
		if(!resolved || resolved->meta_type != AMT_VALUE || !resolved->value ||
			resolved->value->type != ATV_INTEGER) return -1;
		integer = resolved->value->value.v_integer;
	} else {
		return -1;
	}
	if(integer >= INTMAX_MIN && integer <= INTMAX_MAX) {
		*has_numeric_id = 1;
		*numeric_id = (intmax_t)integer;
	}
	return 0;
}

static int
ioc_columns(asn1p_ioc_row_t *row, asn1p_expr_t **cells) {
	static const char *const names[] = { "id", "Value", "presence", "criticality" };
	size_t i, j;
	if(!row || !row->columns || !row->column) return -1;
	for(i = 0; i < row->columns; ++i) {
		const char *name;
		if(!row->column[i].field || !(name = row->column[i].field->Identifier))
			return -1;
		if(*name == '&') ++name;
		for(j = 0; j < 4; ++j) {
			if(strcmp(name, names[j])) continue;
			if(cells[j] || !row->column[i].value) return -1;
			cells[j] = row->column[i].value;
			break;
		}
	}
	return cells[0] && cells[1] && cells[2] && cells[3] ? 0 : -1;
}

static int
extract_ioc_row(asn1p_t *tree, asn1typed_type_t *message,
		asn1p_ioc_row_t *row, const char *file, char *error, size_t error_size) {
	asn1p_expr_t *cells[4] = { NULL, NULL, NULL, NULL };
	asn1p_expr_t value;
	asn1p_ref_t reference;
	asn1typed_type_ref_t ref;
	asn1typed_presence_e presence;
	asn1typed_criticality_e criticality;
	const char *symbol, *name, *p, *c;
	size_t i;
	int rc, has_numeric_id;
	intmax_t numeric_id = 0;
	if(ioc_columns(row, cells)) {
		set_error(error, error_size, "malformed IOC row: required id/Value/presence/criticality cell missing or duplicated");
		return -1;
	}
	if(ioc_id_identity(tree, cells[0], &symbol, &has_numeric_id, &numeric_id))
		goto bad_id;
	/* Retain T3 source_name compatibility; naming owns the IOC convention. */
	name = asn1typed_name_ioc_identity(symbol);
	if(!*name) goto bad_id;
	for(i = 0; i < message->field_count; ++i) {
		if(!strcmp(message->fields[i].source_name, name)) {
			set_error(error, error_size, "duplicate IOC semantic field identity '%s'", name);
			return -1;
		}
	}
	p = ioc_presence_identity(cells[2]);
	if(p && !strcmp(p, "mandatory")) presence = ASN1TYPED_PRESENCE_MANDATORY;
	else if(p && !strcmp(p, "optional")) presence = ASN1TYPED_PRESENCE_OPTIONAL;
	else if(p && !strcmp(p, "conditional")) presence = ASN1TYPED_PRESENCE_CONDITIONAL;
	else {
		set_error(error, error_size, "unrecognized IOC presence");
		return -1;
	}
	c = ioc_criticality_identity(cells[3]);
	if(c && !strcmp(c, "reject")) criticality = ASN1TYPED_CRITICALITY_REJECT;
	else if(c && !strcmp(c, "ignore")) criticality = ASN1TYPED_CRITICALITY_IGNORE;
	else if(c && !strcmp(c, "notify")) criticality = ASN1TYPED_CRITICALITY_NOTIFY;
	else {
		set_error(error, error_size, "unrecognized IOC criticality");
		return -1;
	}
	value = *cells[1];
	if(value.meta_type != AMT_TYPE && value.meta_type != AMT_TYPEREF) {
		set_error(error, error_size, "malformed IOC Value: expected a type setting");
		return -1;
	}
	if(value.expr_type == A1TC_REFERENCE && value.reference) {
		reference = *value.reference;
		reference.ref_expr = ioc_resolve(tree, cells[1], value.reference);
		value.reference = &reference;
	}
	memset(&ref, 0, sizeof(ref));
	if(kind_of_type(terminal_type(&value)) == (asn1typed_type_kind_e)-1 ||
		put_ref(&ref, &value)) {
		set_error(error, error_size, "unresolved or unsupported IOC Value type");
		return -1;
	}
	if(ref.kind == ASN1TYPED_REF_PRIMITIVE)
		rc = asn1typed_type_add_primitive_field(message, name, ref.primitive_kind,
			presence, file, cells[1]->_lineno > 0 ? (unsigned)cells[1]->_lineno : 0);
	else
		rc = asn1typed_type_add_field(message, name, ref.module, ref.source_name,
			presence, file, cells[1]->_lineno > 0 ? (unsigned)cells[1]->_lineno : 0);
	asn1typed_type_ref_clear(&ref);
	if(rc || asn1typed_field_set_ioc(&message->fields[message->field_count - 1],
			symbol, criticality, has_numeric_id, numeric_id)) {
		set_error(error, error_size, "out of memory storing IOC field");
		return -1;
	}
	return 0;
bad_id:
	set_error(error, error_size, "missing or invalid symbolic IOC id");
	return -1;
}

static int
actual_equal(const asn1typed_type_actual_t *left,
		const asn1typed_type_actual_t *right) {
	return left->kind == right->kind && left->module && right->module &&
		left->source_name && right->source_name &&
		!strcmp(left->module, right->module) &&
		!strcmp(left->source_name, right->source_name);
}

/* Resolve a durable instance key to exactly one specialization in that
 * generic's own table. Candidate identities are normalized through the same
 * namespace-based object-set resolver used by B7a references. */
static int
find_instance_specialization(asn1p_t *tree,
		const asn1typed_type_ref_t *identity, asn1p_expr_t **generic_out,
		asn1p_expr_t **specialization_out, char *error, size_t error_size) {
	asn1p_module_t *module = NULL;
	asn1p_expr_t *generic, *candidate;
	size_t i, matches = 0;
	if(generic_out) *generic_out = NULL;
	if(specialization_out) *specialization_out = NULL;
	if(!tree || !identity || identity->kind != ASN1TYPED_REF_NAMED ||
		!identity->module || !identity->source_name ||
		!identity->actual_count || !identity->actuals) {
		set_error(error, error_size, "invalid bound-instance semantic identity");
		return -1;
	}
	TQ_FOR(module, &tree->modules, mod_next)
		if(module->ModuleName && !strcmp(module->ModuleName, identity->module)) break;
	if(!module || !module->ModuleName) {
		set_error(error, error_size, "bound-instance generic module '%s' not found",
			identity->module);
		return -1;
	}
	generic = named_declaration(module, identity->source_name);
	if(!generic) {
		set_error(error, error_size,
			"bound-instance generic declaration '%s.%s' not found",
			identity->module, identity->source_name);
		return -1;
	}
	if(!generic->lhs_params || !generic->lhs_params->params) {
		set_error(error, error_size,
			"bound-instance generic '%s.%s' has unsupported parameter form",
			identity->module, identity->source_name);
		return -1;
	}
	if((size_t)generic->lhs_params->params_count != identity->actual_count) {
		set_error(error, error_size,
			"bound-instance generic '%s.%s' formal/actual count mismatch (%d/%lu)",
			identity->module, identity->source_name,
			generic->lhs_params->params_count,
			(unsigned long)identity->actual_count);
		return -1;
	}
	if(generic->specializations.pspecs_count < 0 ||
		(generic->specializations.pspecs_count &&
		 !generic->specializations.pspec)) {
		set_error(error, error_size,
			"%s.%s: malformed specialization table",
			identity->module, identity->source_name);
		return -1;
	}
	for(i = 0; i < (size_t)generic->specializations.pspecs_count; ++i) {
		asn1p_expr_t *rhs, *parameter, *clone;
		asn1typed_type_actual_t *actuals;
		size_t n = 0, j;
		int equal = 1;
		if(!(rhs = generic->specializations.pspec[i].rhs_pspecs) ||
			!(clone = generic->specializations.pspec[i].my_clone)) {
			set_error(error, error_size,
				"%s.%s: malformed specialization candidate",
				identity->module, identity->source_name);
			return -1;
		}
		actuals = (asn1typed_type_actual_t *)calloc(identity->actual_count,
			sizeof(*actuals));
		if(!actuals) {
			set_error(error, error_size, "out of memory matching bound specialization");
			return -1;
		}
		TQ_FOR(parameter, &rhs->members, next) {
			asn1p_expr_t *set = NULL;
			if(n >= identity->actual_count ||
				object_set_actual(tree, parameter, &actuals[n], &set) ||
				!class_field_governor_matches(tree, generic, n, set)) {
				free(actuals);
				set_error(error, error_size,
					"%s.%s: malformed or unsupported specialization actual",
					identity->module, identity->source_name);
				return -1;
			}
			++n;
		}
		if(n != identity->actual_count) {
			free(actuals);
			set_error(error, error_size,
				"%s.%s: specialization actual count mismatch",
				identity->module, identity->source_name);
			return -1;
		}
		for(j = 0; j < n; ++j)
			if(!actual_equal(&actuals[j], &identity->actuals[j])) equal = 0;
		free(actuals);
		if(equal) {
			++matches;
			candidate = clone;
		}
	}
	if(matches != 1) {
		set_error(error, error_size,
			"%s.%s: expected one semantic specialization match, found %lu",
			identity->module, identity->source_name, (unsigned long)matches);
		return -1;
	}
	if(generic_out) *generic_out = generic;
	if(specialization_out) *specialization_out = candidate;
	return 0;
}

static int
resolve_relation_actual(asn1p_t *tree, asn1p_expr_t *generic,
		asn1p_expr_t *field, const asn1typed_type_ref_t *identity,
		const asn1p_constraint_t *relation, size_t *index_out) {
	const asn1p_constraint_t *binding;
	asn1p_ref_t *ref;
	asn1p_expr_t *set = NULL;
	asn1typed_type_actual_t actual;
	size_t i, match_count = 0, match = 0;
	if(!tree || !generic || !field || !identity || !relation ||
		!relation->elements || relation->el_count < 1 || !index_out) return -1;
	binding = relation->elements[0];
	if(!binding || binding->type != ACT_EL_VALUE || binding->el_count ||
		!binding->value) return -1;
	ref = ioc_set_reference(binding->value);
	if(!ref || ref->comp_count != 1 || !ref->components ||
		!ref->components[0].name || !*ref->components[0].name) return -1;
	set = ioc_resolve(tree, field, ref);
	if(set && set->meta_type == AMT_VALUESET && set->Identifier && set->module &&
		set->module->ModuleName) {
		actual.kind = ASN1TYPED_ACTUAL_OBJECT_SET_REFERENCE;
		actual.module = set->module->ModuleName;
		actual.source_name = set->Identifier;
	} else {
		/* A specialized constraint may retain the formal parameter name.
		 * Resolve that name through the matching generic's ordered formals and
		 * the already-validated instance actuals. */
		for(i = 0; i < (size_t)generic->lhs_params->params_count; ++i) {
			const char *formal = generic->lhs_params->params[i].argument;
			if(formal && !strcmp(formal, ref->components[0].name)) break;
		}
		if(i >= identity->actual_count) return -1;
		actual = identity->actuals[i];
	}
	if(actual.kind != ASN1TYPED_ACTUAL_OBJECT_SET_REFERENCE) return -1;
	for(i = 0; i < identity->actual_count; ++i)
		if(actual_equal(&actual, &identity->actuals[i])) {
			match = i;
			++match_count;
		}
	if(match_count != 1 || match >= identity->actual_count) return -1;
	*index_out = match;
	return 0;
}

static char *
component_selector(const asn1p_constraint_t *relation) {
	const asn1p_constraint_t *component;
	asn1p_ref_t *ref;
	char *name;
	if(!relation || relation->el_count != 2 || !relation->elements ||
		!(component = relation->elements[1]) || component->type != ACT_EL_VALUE ||
		component->el_count || !component->value ||
		component->value->type != ATV_REFERENCED) return NULL;
	ref = component->value->value.reference;
	if(!ref || ref->comp_count != 1 || !ref->components ||
		!(name = ref->components[0].name) || name[0] != '@' || !name[1])
		return NULL;
	return name + 1;
}

static int
materialize_class_field(asn1p_t *tree, asn1p_expr_t *generic,
		const asn1typed_type_ref_t *identity, asn1typed_type_t *body,
		asn1p_expr_t *member, const char *file, unsigned line,
		char *error, size_t error_size) {
	asn1p_ref_t prefix;
	asn1p_expr_t *class_expr, *class_field, *class_member;
	const asn1p_constraint_t *relation;
	char *field_component, *field_identity, *selector = NULL;
	asn1typed_class_field_relation_t owned_relation;
	asn1typed_type_ref_t fixed_ref;
	asn1typed_presence_e presence;
	size_t actual_index;
	int found = 0, rc = -1;
	memset(&owned_relation, 0, sizeof(owned_relation));
	memset(&fixed_ref, 0, sizeof(fixed_ref));
	if(!member->Identifier || !member->reference ||
		member->reference->comp_count != 2 || !member->reference->components ||
		!member->reference->components[0].name ||
		!member->reference->components[1].name ||
		member->reference->components[1].name[0] != '&') goto malformed;
	field_component = member->reference->components[1].name;
	field_identity = field_component + 1;
	if(!*field_identity) goto malformed;
	prefix = *member->reference;
	prefix.comp_count = 1;
	prefix.ref_expr = NULL;
	class_expr = ioc_resolve(tree, member, &prefix);
	if(!class_expr || class_expr->expr_type != A1TC_CLASSDEF ||
		!class_expr->Identifier || !class_expr->module ||
		!class_expr->module->ModuleName) goto malformed;
	TQ_FOR(class_member, &class_expr->members, next)
		if(class_member->Identifier &&
			!strcmp(class_member->Identifier, field_component)) {
			class_field = class_member;
			found = 1;
			break;
		}
	if(!found || class_field->meta_type != AMT_OBJECTFIELD) goto malformed;
	relation = ioc_relation(member);
	if(!relation || relation->el_count < 1 || relation->el_count > 2 ||
		resolve_relation_actual(tree, generic, member, identity, relation,
			&actual_index) || actual_index >= identity->actual_count) goto malformed;
	if(relation->el_count == 2) {
		selector = component_selector(relation);
		if(!selector) goto malformed;
	}
	if((member->marker.flags & EM_DEFAULT) == EM_DEFAULT) goto malformed;
	presence = (member->marker.flags & EM_OPTIONAL) == EM_OPTIONAL ?
		ASN1TYPED_PRESENCE_OPTIONAL : ASN1TYPED_PRESENCE_MANDATORY;
	if(class_field->expr_type == A1TC_CLASSFIELD_FTVFS) {
		asn1p_expr_t *fixed_type = TQ_FIRST(&class_field->members);
		if(!fixed_type || TQ_NEXT(fixed_type, next) || put_ref(&fixed_ref, fixed_type))
			goto malformed;
		if(selector) {
			owned_relation.class_module = class_expr->module->ModuleName;
			owned_relation.class_source_name = class_expr->Identifier;
			owned_relation.class_field_source_name = field_identity;
			owned_relation.actual_index = actual_index;
			owned_relation.has_selector = 1;
			owned_relation.selector_source_name = selector;
			rc = asn1typed_type_add_class_field(body, member->Identifier,
				ASN1TYPED_FIELD_FIXED_TYPE, &fixed_ref, &owned_relation,
			presence, file, line);
		} else {
			/* The studied fixed-type/no-selector shape is the UNIQUE class
			 * field. Keep its ordinary type without relation metadata. */
			if(!class_field->unique) goto malformed;
			rc = fixed_ref.kind == ASN1TYPED_REF_PRIMITIVE ?
				asn1typed_type_add_primitive_field(body, member->Identifier,
					fixed_ref.primitive_kind, presence, file, line) :
				asn1typed_type_add_field_ref(body, member->Identifier,
					&fixed_ref, presence, file, line);
		}
	} else if(class_field->expr_type == A1TC_CLASSFIELD_TFS && selector) {
		owned_relation.class_module = class_expr->module->ModuleName;
		owned_relation.class_source_name = class_expr->Identifier;
		owned_relation.class_field_source_name = field_identity;
		owned_relation.actual_index = actual_index;
		owned_relation.has_selector = 1;
		owned_relation.selector_source_name = selector;
		rc = asn1typed_type_add_class_field(body, member->Identifier,
			ASN1TYPED_FIELD_CLASS_FIELD_SELECTED_TYPE, NULL, &owned_relation,
		presence, file, line);
	} else {
		goto malformed;
	}
	asn1typed_type_ref_clear(&fixed_ref);
	if(rc) {
		set_error(error, error_size, "%s.%s: could not own specialized class field",
			generic->Identifier, member->Identifier);
		return -1;
	}
	return 0;
malformed:
	asn1typed_type_ref_clear(&fixed_ref);
	set_error(error, error_size,
		"%s.%s: malformed or unsupported specialized class-field reference",
		generic && generic->Identifier ? generic->Identifier : "<generic>",
		member && member->Identifier ? member->Identifier : "<unnamed>");
	return -1;
}

static int
materialize_bound_sequence_of(asn1p_t *tree, asn1p_expr_t *generic,
		const asn1typed_type_ref_t *identity, asn1p_expr_t *resolved,
		asn1typed_type_t *body, char *error, size_t error_size) {
	asn1p_expr_t *element = TQ_FIRST(&resolved->members);
	if(!element || TQ_NEXT(element, next) ||
		element->expr_type != A1TC_REFERENCE || !element->rhs_pspecs ||
		!element->reference || element->reference->comp_count != 1 ||
		!element->reference->components ||
		!element->reference->components[0].name ||
		!element->reference->components[0].name[0]) {
		set_error(error, error_size,
			"%s.%s: unsupported specialized SEQUENCE OF element",
			identity->module, identity->source_name);
		return -1;
	}
	body->kind = ASN1TYPED_TYPE_SEQUENCE_OF;
	if(extract_bound_body_size_constraint(tree, generic, resolved->constraints,
			&body->size_constraint)) {
		set_error(error, error_size,
			"%s.%s: unsupported specialized SEQUENCE OF SIZE constraint",
			identity->module, identity->source_name);
		return -1;
	}
	if(put_parameterized_object_set_ref(tree, &body->element_type, element,
			error, error_size)) return -1;
	if(body->element_type.kind != ASN1TYPED_REF_NAMED ||
		body->element_type.actual_count != 1 || !body->element_type.actuals ||
		body->element_type.actuals[0].kind !=
			ASN1TYPED_ACTUAL_OBJECT_SET_REFERENCE) {
		set_error(error, error_size,
			"%s.%s: specialized SEQUENCE OF element lost its parameterized actual",
			identity->module, identity->source_name);
		return -1;
	}
	return 0;
}

static int
materialize_bound_instance(asn1p_t *tree, asn1typed_module_t *out,
		size_t instance_index, char *error, size_t error_size) {
	const asn1typed_type_ref_t *identity;
	asn1typed_type_t body;
	asn1p_expr_t *generic, *specialization, *resolved, *member;
	int saw_extension = 0;
	size_t physical_roots = 0;
	int one_to_one = 1;
	if(!tree || !out || instance_index >= out->bound_instance_count) return -1;
	identity = &out->bound_instances[instance_index].identity;
	if(out->bound_instances[instance_index].body_materialized) return 0;
	if(find_instance_specialization(tree, identity, &generic, &specialization,
			error, error_size)) return -1;
	resolved = terminal_type(specialization);
	if(!resolved) {
		set_error(error, error_size,
			"%s.%s: specialization body is not a supported SEQUENCE",
			identity->module, identity->source_name);
		return -1;
	}
	memset(&body, 0, sizeof(body));
	if(resolved->expr_type == ASN_CONSTR_SEQUENCE_OF) {
		if(materialize_bound_sequence_of(tree, generic, identity, resolved,
				&body, error, error_size)) goto fail;
		if(asn1typed_bound_instance_set_body(out, instance_index, &body)) {
			set_error(error, error_size,
				"%s.%s: could not atomically own specialized SEQUENCE OF body",
				identity->module, identity->source_name);
			goto fail;
		}
		return 0;
	}
	if(resolved->expr_type != ASN_CONSTR_SEQUENCE) {
		set_error(error, error_size,
			"%s.%s: specialization body is not a supported SEQUENCE",
			identity->module, identity->source_name);
		return -1;
	}
	body.kind = ASN1TYPED_TYPE_SEQUENCE;
	TQ_FOR(member, &resolved->members, next) {
		const char *member_file = member->_lineno > 0 &&
			generic->module->source_file_name ?
			generic->module->source_file_name : "<unknown>";
		unsigned member_line = member->_lineno > 0 ?
			(unsigned)member->_lineno : 0;
		size_t before = body.field_count;
		if(member->expr_type == A1TC_EXTENSIBLE) {
			if(saw_extension) goto malformed_body;
			saw_extension = 1;
			body.is_extensible = 1;
			continue;
		}
		if(saw_extension || !member->Identifier || !member->Identifier[0])
			goto malformed_body;
		if(member->reference && member->reference->components &&
			member->reference->comp_count == 2 &&
			member->reference->components[1].name &&
			member->reference->components[1].name[0] == '&') {
			if(materialize_class_field(tree, generic, identity, &body, member,
					member_file, member_line, error, error_size)) goto fail;
		} else if(member->meta_type == AMT_TYPE &&
				member->expr_type == ASN_BASIC_ENUMERATED) {
			if(add_field(tree, &body, member, member_file,
					generic->module->ModuleName, error, error_size)) goto fail;
		} else {
			asn1typed_type_ref_t ref;
			asn1typed_presence_e presence;
			int rc;
			memset(&ref, 0, sizeof(ref));
			if(member->rhs_pspecs || put_ref(&ref, member)) {
				asn1typed_type_ref_clear(&ref);
				goto malformed_body;
			}
			if((member->marker.flags & EM_DEFAULT) == EM_DEFAULT) {
				asn1typed_type_ref_clear(&ref);
				goto malformed_body;
			}
			presence = (member->marker.flags & EM_OPTIONAL) == EM_OPTIONAL ?
				ASN1TYPED_PRESENCE_OPTIONAL : ASN1TYPED_PRESENCE_MANDATORY;
			rc = ref.kind == ASN1TYPED_REF_PRIMITIVE ?
				asn1typed_type_add_primitive_field(&body, member->Identifier,
					ref.primitive_kind, presence, member_file, member_line) :
				asn1typed_type_add_field_ref(&body, member->Identifier,
					&ref, presence, member_file, member_line);
			asn1typed_type_ref_clear(&ref);
			if(rc) goto malformed_body;
		}
		if(body.field_count <= before || body.field_count - before != 1 || physical_roots == SIZE_MAX)
			one_to_one = 0;
		else ++physical_roots;
	}
	if(saw_extension) {
		if(!one_to_one) asn1typed_sequence_set_extension_unsupported(&body);
		else if(asn1typed_sequence_set_extension_structure(&body, physical_roots, 0) ||
			asn1typed_sequence_extension_structure_finalize(&body, error, error_size) != ASN1TYPED_WIRE_FINALIZE_OK) goto fail;
	}
	if(!body.field_count || asn1typed_bound_instance_set_body(out,
			instance_index, &body)) goto malformed_body;
	return 0;
malformed_body:
	set_error(error, error_size, "%s.%s: malformed or unsupported specialized SEQUENCE body",
		identity->module, identity->source_name);
fail:
	asn1typed_type_clear(&body);
	return -1;
}

/* Add each reachable ordinary declaration once. The outer worklist handles
 * recursion without retaining a types-array pointer across reallocations. */
static int
inline_enum_field_identity_free(const asn1typed_field_t *field) {
	return field && field->type_semantics ==
		ASN1TYPED_FIELD_INLINE_ENUMERATED && field->inline_enumerated &&
		field->inline_enumerated->kind == ASN1TYPED_TYPE_ENUMERATED &&
		field->inline_enumerated->enum_item_count &&
		field->inline_enumerated->enum_items &&
		field->type.kind == ASN1TYPED_REF_NAMED && !field->type.module &&
		!field->type.source_name && !field->type.actuals &&
		!field->type.actual_count &&
		field->type.primitive_kind == ASN1TYPED_PRIMITIVE_INVALID &&
		!field->has_class_field_relation;
}

static int
add_ioc_dependency(asn1p_t *tree, asn1typed_module_t *out,
		const asn1typed_type_ref_t *ref, char *error, size_t error_size) {
	asn1p_module_t *source = NULL, *module;
	asn1p_expr_t *decl;
	asn1typed_type_t *type;
	asn1typed_type_kind_e kind;
	const char *file;
	size_t i;
	/* B7b.1 owns this identity, but closure cannot claim success until B7b.2
	 * materializes and traverses the bound instance's semantic body. */
	if(ref->actual_count) {
		asn1typed_bound_instance_t *bound_instance;
		asn1typed_type_ref_t *dependencies = NULL;
		size_t instance_index, dependency_count = 0, dependency_index;
		if(asn1typed_module_add_bound_instance(out, ref, &bound_instance)) {
			set_error(error, error_size,
				"parameterized dependency identity could not be owned");
			return -1;
		}
		instance_index = (size_t)(bound_instance - out->bound_instances);
		if(bound_instance->body_materialized) return 0;
		if(materialize_bound_instance(tree, out, instance_index, error, error_size)) {
			return -1;
		}
		/* Copy all fixed dependencies before recursive closure can append and
		 * reallocate bound_instances[]. */
		bound_instance = &out->bound_instances[instance_index];
		if(bound_instance->body.kind == ASN1TYPED_TYPE_SEQUENCE_OF) {
			asn1typed_type_ref_t element = {0};
			int rc;
			if(asn1typed_type_ref_copy(&element,
					&bound_instance->body.element_type)) {
				set_error(error, error_size,
					"out of memory copying bound SEQUENCE OF element reference");
				return -1;
			}
			rc = add_ioc_dependency(tree, out, &element, error, error_size);
			asn1typed_type_ref_clear(&element);
			return rc;
		}
		for(i = 0; i < bound_instance->body.field_count; ++i) {
			asn1typed_field_t *field = &bound_instance->body.fields[i];
			if(field->type_semantics == ASN1TYPED_FIELD_FIXED_TYPE) {
				if(field->inline_enumerated) {
					set_error(error, error_size,
						"%s.%s: fixed field has unexpected inline ENUMERATED body",
						ref->source_name, field->source_name);
					return -1;
				}
				++dependency_count;
			} else if(field->type_semantics ==
					ASN1TYPED_FIELD_INLINE_ENUMERATED) {
				if(!inline_enum_field_identity_free(field)) {
					set_error(error, error_size,
						"%s.%s: malformed inline ENUMERATED field ownership",
						ref->source_name, field->source_name);
					return -1;
				}
			} else if(field->type_semantics ==
					ASN1TYPED_FIELD_CLASS_FIELD_SELECTED_TYPE) {
				if(!field->has_class_field_relation) {
					set_error(error, error_size,
						"%s.%s: selected bound field has no class-field relation",
						ref->source_name, field->source_name);
					return -1;
				}
			} else {
				set_error(error, error_size,
					"%s.%s: unknown bound field type semantics",
					ref->source_name, field->source_name);
				return -1;
			}
		}
		if(dependency_count) {
			size_t next = 0;
			dependencies = (asn1typed_type_ref_t *)calloc(dependency_count,
				sizeof(*dependencies));
			if(!dependencies) {
				set_error(error, error_size,
					"out of memory copying bound-instance dependencies");
				return -1;
			}
			for(i = 0; i < bound_instance->body.field_count; ++i) {
				asn1typed_field_t *field = &bound_instance->body.fields[i];
				if(field->type_semantics != ASN1TYPED_FIELD_FIXED_TYPE) continue;
				if(asn1typed_type_ref_copy(&dependencies[next], &field->type)) {
					for(dependency_index = 0; dependency_index < next;
						++dependency_index)
						asn1typed_type_ref_clear(&dependencies[dependency_index]);
					free(dependencies);
					set_error(error, error_size,
						"out of memory copying bound-instance field reference");
					return -1;
				}
				++next;
			}
		}
		for(dependency_index = 0; dependency_index < dependency_count;
			++dependency_index) {
			if(add_ioc_dependency(tree, out, &dependencies[dependency_index],
					error, error_size)) {
				for(i = 0; i < dependency_count; ++i)
					asn1typed_type_ref_clear(&dependencies[i]);
				free(dependencies);
				return -1;
			}
		}
		for(i = 0; i < dependency_count; ++i)
			asn1typed_type_ref_clear(&dependencies[i]);
		free(dependencies);
		return 0;
	}
	if(ref->kind == ASN1TYPED_REF_PRIMITIVE) return 0;
	if(!ref->module || !ref->source_name) {
		set_error(error, error_size, "IOC dependency has incomplete module-qualified identity");
		return -1;
	}
	for(i = 0; i < out->type_count; ++i)
		if(!strcmp(out->types[i].identity.module, ref->module) &&
			!strcmp(out->types[i].identity.source_name, ref->source_name)) return 0;
	TQ_FOR(module, &tree->modules, mod_next)
		if(module->ModuleName && !strcmp(module->ModuleName, ref->module)) {
			source = module;
			break;
		}
	if(!source) {
		set_error(error, error_size, "IOC dependency module '%s' not found", ref->module);
		return -1;
	}
	decl = named_declaration(source, ref->source_name);
	if(!decl || decl->meta_type != AMT_TYPE || decl->lhs_params || decl->rhs_pspecs ||
		(kind = kind_of_type(terminal_type(decl))) == (asn1typed_type_kind_e)-1) {
		set_error(error, error_size, "unresolved or unsupported IOC dependency '%s.%s'",
			ref->module, ref->source_name);
		return -1;
	}
	file = source->source_file_name ? source->source_file_name : out->location.file;
	if(asn1typed_module_add_type_identity(out, ref->module, decl->Identifier, kind, file,
			decl->_lineno > 0 ? (unsigned)decl->_lineno : 0, &type)) {
		set_error(error, error_size, "out of memory storing IOC dependency");
		return -1;
	}
	return populate_type(tree, type, decl, file, error, error_size);
}

int
asn1typed_extract_message(asn1p_t *tree, const char *module_name,
		const char *message_name, asn1typed_module_t *out,
		char *error, size_t error_size) {
	asn1p_module_t *source;
	asn1p_expr_t *message = NULL, *set;
	asn1p_expr_t *message_body, *container_use;
	asn1typed_type_t *type;
	const char *file;
	size_t i, j;
	if(error && error_size) error[0] = '\0';
	if(out) memset(out, 0, sizeof(*out));
	if(!tree || !module_name || !message_name || !out) {
		set_error(error, error_size, "invalid IOC extractor arguments");
		return -1;
	}
	TQ_FOR(source, &tree->modules, mod_next) {
		if(source->ModuleName && !strcmp(source->ModuleName, module_name)) {
			message = named_declaration(source, message_name);
			break;
		}
	}
	if(!message || message->meta_type != AMT_TYPE ||
		!(set = message_object_set(tree, message))) {
		set_error(error, error_size, "missing or invalid IOC object-set association for '%s'", message_name);
		return -1;
	}
	if(!set->ioc_table || (set->ioc_table->rows && !set->ioc_table->row) ||
		(!set->ioc_table->rows && !set->ioc_table->extensible)) {
		set_error(error, error_size, "missing IOC table or non-extensible empty IOC table for '%s'", message_name);
		return -1;
	}
	file = source->source_file_name ? source->source_file_name : "<unknown>";
	if(asn1typed_module_init(out, module_name, file, 1) ||
		asn1typed_module_add_type(out, message_name, ASN1TYPED_TYPE_SEQUENCE,
			file, message->_lineno > 0 ? (unsigned)message->_lineno : 0, &type)) {
		set_error(error, error_size, "out of memory storing IOC message");
		goto fail;
	}
	out->tag_default = module_tag_default(source);
	message_body = terminal_type(message);
	container_use = message_body ? TQ_FIRST(&message_body->members) : NULL;
	if(!container_use || put_parameterized_object_set_ref(tree,
			&type->ioc_container, container_use, error, error_size)) goto fail;
	type->has_ioc_table = 1;
	type->ioc_object_set_is_extensible = !!set->ioc_table->extensible;
	{
		asn1p_expr_t *marker = TQ_NEXT(container_use, next);
		type->is_extensible = marker && marker->expr_type == A1TC_EXTENSIBLE;
	}
	for(i = 0; i < set->ioc_table->rows; ++i)
		if(extract_ioc_row(tree, type, set->ioc_table->row[i], file, error, error_size))
			goto fail;
	/* IOC rows are flattened fields, not the physical SEQUENCE root layout. */
	asn1typed_sequence_set_extension_unsupported(type);
	for(i = 0; i < out->type_count; ++i) {
		for(j = 0; j < out->types[i].field_count; ++j) {
			asn1typed_field_t *field = &out->types[i].fields[j];
			switch(field->type_semantics) {
			case ASN1TYPED_FIELD_FIXED_TYPE:
				if(field->inline_enumerated) {
					set_error(error, error_size,
						"%s.%s: fixed field has unexpected inline ENUMERATED body",
						out->types[i].identity.source_name, field->source_name);
					goto fail;
				}
				if(add_ioc_dependency(tree, out, &field->type,
						error, error_size)) goto fail;
				break;
			case ASN1TYPED_FIELD_INLINE_ENUMERATED:
				if(!inline_enum_field_identity_free(field)) {
					set_error(error, error_size,
						"%s.%s: malformed inline ENUMERATED field ownership",
						out->types[i].identity.source_name, field->source_name);
					goto fail;
				}
				break;
			case ASN1TYPED_FIELD_CLASS_FIELD_SELECTED_TYPE:
				if(!field->has_class_field_relation) {
					set_error(error, error_size,
						"%s.%s: selected class-field type has no relation",
						out->types[i].identity.source_name, field->source_name);
					goto fail;
				}
				break;
			default:
				set_error(error, error_size,
					"%s.%s: unknown field type semantics",
					out->types[i].identity.source_name, field->source_name);
				goto fail;
			}
		}
		if(out->types[i].kind == ASN1TYPED_TYPE_CHOICE) {
			for(j = 0; j < out->types[i].alternative_count; ++j) {
				/* Adding a dependency may realloc the type array. */
				asn1typed_type_ref_t ref =
					out->types[i].alternatives[j].type_ref;
				if(add_ioc_dependency(tree, out, &ref, error, error_size))
					goto fail;
			}
		}
		if(out->types[i].kind == ASN1TYPED_TYPE_SEQUENCE_OF) {
			/* element_type lives inside the reallocatable array: copy it first. */
			asn1typed_type_ref_t ref = out->types[i].element_type;
			if(add_ioc_dependency(tree, out, &ref, error, error_size)) goto fail;
		}
	}
	return 0;
fail:
	asn1typed_module_clear(out);
	return -1;
}

static asn1p_expr_t *
physical_named_declaration(asn1p_t *tree, const char *module_name, const char *name) {
	asn1p_module_t *module;
	TQ_FOR(module, &tree->modules, mod_next)
		if(module->ModuleName && !strcmp(module->ModuleName, module_name)) return named_declaration(module, name);
	return NULL;
}
static int
physical_class_shape(asn1p_expr_t *class_expr, const char **selected, int *has_presence) {
	asn1p_expr_t *member;
	unsigned seen = 0;
	if(!class_expr || class_expr->expr_type != A1TC_CLASSDEF || !class_expr->Identifier || !class_expr->module || !class_expr->module->ModuleName) return -1;
	*selected = NULL;
	*has_presence = 0;
	TQ_FOR(member, &class_expr->members, next) {
		const char *name = member->Identifier;
		unsigned bit;
		if(!name || name[0] != '&' || member->meta_type != AMT_OBJECTFIELD ||
			member->marker.flags != EM_NOMARK || member->marker.default_value ||
			member->constraints || member->combined_constraints || member->rhs_pspecs || member->lhs_params) return -1;
		++name;
		if(!strcmp(name, "id")) {
			bit = 1;
			if(member->expr_type != A1TC_CLASSFIELD_FTVFS || !member->unique) return -1;
		} else if(!strcmp(name, "criticality")) {
			bit = 2;
			if(member->expr_type != A1TC_CLASSFIELD_FTVFS || member->unique) return -1;
		} else if(!strcmp(name, "Value") || !strcmp(name, "Extension")) {
			bit = 4;
			if(member->expr_type != A1TC_CLASSFIELD_TFS || member->unique) return -1;
			*selected = name;
		} else if(!strcmp(name, "presence")) {
			bit = 8;
			if(member->expr_type != A1TC_CLASSFIELD_FTVFS || member->unique) return -1;
		} else return -1;
		if(seen & bit) return -1;
		seen |= bit;
	}
	*has_presence = !!(seen & 8);
	return *selected && ((!strcmp(*selected, "Value") && seen == 15) || (!strcmp(*selected, "Extension") && (seen == 7 || seen == 15))) ? 0 : -1;
}
static int
physical_ioc_cells(asn1p_ioc_row_t *row, const char *selected, int has_presence, asn1p_expr_t **cells) {
	size_t i;
	if(!row || !row->column || row->columns != (has_presence ? 4u : 3u)) return -1;
	for(i = 0; i < row->columns; ++i) {
		const char *name;
		size_t role;
		if(!row->column[i].field || !(name = row->column[i].field->Identifier) || !row->column[i].value) return -1;
		if(*name == '&') ++name;
		if(!strcmp(name, "id")) role = 0;
		else if(!strcmp(name, selected)) role = 1;
		else if(!strcmp(name, "criticality")) role = 2;
		else if(has_presence && !strcmp(name, "presence")) role = 3;
		else return -1;
		if(cells[role]) return -1;
		cells[role] = row->column[i].value;
	}
	return cells[0] && cells[1] && cells[2] && (!has_presence || cells[3]) ? 0 : -1;
}
static int
physical_registry_row(asn1p_t *tree, asn1typed_ioc_registry_t *registry,
		asn1p_ioc_row_t *source, int has_presence, char *error, size_t size) {
	asn1p_expr_t *cells[4] = {NULL, NULL, NULL, NULL};
	asn1p_expr_t value;
	asn1p_ref_t reference;
	asn1typed_ioc_dispatch_row_t row;
	const char *symbol, *name;
	memset(&row, 0, sizeof(row));
	if(physical_ioc_cells(source, registry->selected_class_field_source_name, has_presence, cells) ||
		ioc_id_identity(tree, cells[0], &symbol, &row.has_numeric_id, &row.numeric_id) || !row.has_numeric_id) goto malformed;
	if(symbol) {
		row.symbolic_id = malloc(strlen(symbol) + 1);
		if(!row.symbolic_id) { set_error(error, size, "out of memory owning physical IOC symbolic ID"); return -1; }
		strcpy(row.symbolic_id, symbol);
	}
	name = ioc_criticality_identity(cells[2]);
	if(name && !strcmp(name, "reject")) row.criticality = ASN1TYPED_CRITICALITY_REJECT;
	else if(name && !strcmp(name, "ignore")) row.criticality = ASN1TYPED_CRITICALITY_IGNORE;
	else if(name && !strcmp(name, "notify")) row.criticality = ASN1TYPED_CRITICALITY_NOTIFY;
	else goto malformed;
	if(cells[3]) {
		row.has_presence = 1; name = ioc_presence_identity(cells[3]);
		if(name && !strcmp(name, "mandatory")) row.presence = ASN1TYPED_PRESENCE_MANDATORY;
		else if(name && !strcmp(name, "optional")) row.presence = ASN1TYPED_PRESENCE_OPTIONAL;
		else if(name && !strcmp(name, "conditional")) row.presence = ASN1TYPED_PRESENCE_CONDITIONAL;
		else goto malformed;
	}
	if(reject_unowned_inline_constraint(cells[1], registry->object_set_source_name, registry->selected_class_field_source_name, error, size)) { free(row.symbolic_id); return -1; }
	value = *cells[1];
	if(value.meta_type != AMT_TYPE && value.meta_type != AMT_TYPEREF) goto malformed;
	if(value.expr_type == A1TC_REFERENCE && value.reference) {
		reference = *value.reference; reference.ref_expr = ioc_resolve(tree, cells[1], value.reference); value.reference = &reference;
	}
	if(value.rhs_pspecs || kind_of_type(terminal_type(&value)) == (asn1typed_type_kind_e)-1 || put_ref(&row.payload_type, &value)) goto malformed;
	if(asn1typed_ioc_registry_add_row(registry, &row)) {
		free(row.symbolic_id); asn1typed_type_ref_clear(&row.payload_type); set_error(error, size, "out of memory owning physical IOC dispatch row"); return -1;
	}
	asn1typed_type_ref_clear(&row.payload_type); return 0;
malformed:
	asn1typed_type_ref_clear(&row.payload_type);
	set_error(error, size, "malformed or unresolved physical IOC row numeric ID/payload/criticality/presence"); return -1;
}
static int
physical_register_binding(asn1p_t *tree, asn1typed_module_t *out, size_t index, char *error, size_t size) {
	asn1typed_bound_instance_t *instance = &out->bound_instances[index];
	asn1p_expr_t *set, *class_expr, *generic, *specialization, *body, *member;
	const char *selected, *stage = "actual key";
	size_t registry_index, ordinal = 0, j;
	int has_presence;
	if(instance->identity.actual_count != 1 || !instance->identity.actuals) goto malformed;
	stage = "fixed object-set table";
	set = physical_named_declaration(tree, instance->identity.actuals[0].module, instance->identity.actuals[0].source_name);
	if(!set || !set->ioc_table || (set->ioc_table->rows && !set->ioc_table->row) || (!set->ioc_table->rows && !set->ioc_table->extensible)) goto malformed;
	stage = "class shape or specialization";
	class_expr = ioc_resolve(tree, set, set->reference);
	if(physical_class_shape(class_expr, &selected, &has_presence) || find_instance_specialization(tree, &instance->identity, &generic, &specialization, error, size)) goto malformed;
	stage = "physical component roles";
	body = terminal_type(specialization);
	if(!body || body->expr_type != ASN_CONSTR_SEQUENCE) goto malformed;
	/* UNIQUE id provenance is not recoverable solely from its owned fixed ref. */
	TQ_FOR(member, &body->members, next) {
		const char *role;
		asn1p_ref_t prefix;
		if(ordinal >= 3 || !member->reference || member->reference->comp_count != 2 || !member->reference->components || !member->reference->components[0].name || !(role = member->reference->components[1].name)) goto malformed;
		if(strcmp(role, ordinal == 0 ? "&id" : ordinal == 1 ? "&criticality" : !strcmp(selected, "Value") ? "&Value" : "&Extension")) goto malformed;
		prefix = *member->reference; prefix.comp_count = 1; prefix.ref_expr = NULL;
		if(ioc_resolve(tree, member, &prefix) != class_expr) goto malformed;
		++ordinal;
	}
	if(ordinal != 3 || asn1typed_module_add_ioc_registry(out, class_expr->module->ModuleName, class_expr->Identifier, set->module->ModuleName, set->Identifier, selected, &registry_index)) goto malformed;
	if(out->ioc_registries[registry_index].evidence == ASN1TYPED_WIRE_EVIDENCE_UNAVAILABLE) {
		for(j = 0; j < set->ioc_table->rows; ++j)
			if(physical_registry_row(tree, &out->ioc_registries[registry_index], set->ioc_table->row[j], has_presence, error, size)) return -1;
		if(asn1typed_ioc_registry_set_evidence(&out->ioc_registries[registry_index], set->ioc_table->rows, !!set->ioc_table->extensible) ||
			asn1typed_ioc_registry_finalize(&out->ioc_registries[registry_index], error, size) != ASN1TYPED_WIRE_FINALIZE_OK) return -1;
	}
	if(asn1typed_bound_instance_set_ioc_binding(out, index, registry_index, 0, 1, 2) ||
		asn1typed_bound_instance_ioc_binding_finalize(out, index, error, size) != ASN1TYPED_WIRE_FINALIZE_OK) {
		if(error && size && !error[0]) set_error(error, size, "unsupported physical IOC selector binding");
		return -1;
	}
	return 0;
malformed:
	set_error(error, size, "unsupported physical IOC %s source evidence for %s.%s", stage, instance->identity.module, instance->identity.source_name); return -1;
}
static int
physical_close_type(asn1p_t *tree, asn1typed_module_t *out, size_t index, char *error, size_t size) {
	size_t j;
	for(j = 0; j < out->types[index].field_count; ++j) {
		asn1typed_field_t *f = &out->types[index].fields[j];
		asn1typed_type_ref_t copy = {0};
		if(f->type_semantics == ASN1TYPED_FIELD_INLINE_ENUMERATED && inline_enum_field_identity_free(f)) continue;
		if(f->type_semantics != ASN1TYPED_FIELD_FIXED_TYPE || asn1typed_type_ref_copy(&copy, &f->type)) goto malformed;
		if(add_ioc_dependency(tree, out, &copy, error, size)) { asn1typed_type_ref_clear(&copy); return -1; }
		asn1typed_type_ref_clear(&copy);
	}
	for(j = 0; j < out->types[index].alternative_count; ++j) {
		asn1typed_type_ref_t copy = {0};
		if(asn1typed_type_ref_copy(&copy, &out->types[index].alternatives[j].type_ref)) goto malformed;
		if(add_ioc_dependency(tree, out, &copy, error, size)) { asn1typed_type_ref_clear(&copy); return -1; }
		asn1typed_type_ref_clear(&copy);
	}
	if(out->types[index].kind == ASN1TYPED_TYPE_SEQUENCE_OF) {
		asn1typed_type_ref_t copy = {0};
		if(asn1typed_type_ref_copy(&copy, &out->types[index].element_type)) goto malformed;
		if(add_ioc_dependency(tree, out, &copy, error, size)) { asn1typed_type_ref_clear(&copy); return -1; }
		asn1typed_type_ref_clear(&copy);
	}
	return 0;
malformed:
	set_error(error, size, "unsupported physical message dependency or out of memory copying reference"); return -1;
}
int
asn1typed_extract_physical_message(asn1p_t *tree, const char *module_name, const char *message_name,
		asn1typed_module_t *out, char *error, size_t size) {
	asn1typed_module_t pending;
	asn1p_expr_t *message;
	asn1typed_type_t *root;
	size_t type_index = 0, bound_index = 0, registry_index = 0;
	if(error && size) error[0] = 0;
	if(out) memset(out, 0, sizeof(*out));
	memset(&pending, 0, sizeof(pending));
	if(!tree || !module_name || !message_name || !out) { set_error(error, size, "invalid physical IOC extractor arguments"); return -1; }
	message = physical_named_declaration(tree, module_name, message_name);
	if(!message || message->meta_type != AMT_TYPE || message->lhs_params || message->rhs_pspecs || kind_of_type(terminal_type(message)) != ASN1TYPED_TYPE_SEQUENCE) goto unsupported;
	if(asn1typed_module_init(&pending, module_name, message->module->source_file_name ? message->module->source_file_name : "<unknown>", 1) ||
		asn1typed_module_add_type(&pending, message_name, ASN1TYPED_TYPE_SEQUENCE, pending.location.file, message->_lineno > 0 ? (unsigned)message->_lineno : 0, &root) ||
		populate_type(tree, root, message, pending.location.file, error, size)) goto fail;
	pending.tag_default = module_tag_default(message->module);
	if(root->field_count != 1 || root->fields[0].type_semantics != ASN1TYPED_FIELD_FIXED_TYPE || root->fields[0].presence != ASN1TYPED_PRESENCE_MANDATORY || root->fields[0].type.actual_count != 1) goto unsupported;
	while(type_index < pending.type_count || bound_index < pending.bound_instance_count || registry_index < pending.ioc_registry_count) {
		if(type_index < pending.type_count) {
			if(physical_close_type(tree, &pending, type_index++, error, size)) goto fail;
		} else if(bound_index < pending.bound_instance_count) {
			asn1typed_bound_instance_t *b = &pending.bound_instances[bound_index];
			if(b->body.kind == ASN1TYPED_TYPE_SEQUENCE) {
				if(physical_register_binding(tree, &pending, bound_index, error, size)) goto fail;
			} else if(b->body.kind != ASN1TYPED_TYPE_SEQUENCE_OF || b->body.is_extensible || b->body.size_constraint.is_extensible || !b->body.size_constraint.has_size_constraint || b->body.size_constraint.lower_bound < 0 || b->body.size_constraint.upper_bound > 65535) goto unsupported;
			++bound_index;
		} else {
			asn1typed_ioc_registry_t *r = &pending.ioc_registries[registry_index];
			size_t j;
			for(j = 0; j < r->row_count; ++j) {
				asn1typed_type_ref_t copy = {0};
				if(asn1typed_type_ref_copy(&copy, &pending.ioc_registries[registry_index].rows[j].payload_type)) goto unsupported;
				if(add_ioc_dependency(tree, &pending, &copy, error, size)) { asn1typed_type_ref_clear(&copy); goto fail; }
				asn1typed_type_ref_clear(&copy);
			}
			++registry_index;
		}
	}
	/* The physical message root must hold a collection, not a single field. */
	{
		asn1typed_type_ref_t *ref = &pending.types[0].fields[0].type;
		size_t i;
		for(i = 0; i < pending.bound_instance_count; ++i)
			if(asn1typed_type_ref_equal(ref, &pending.bound_instances[i].identity)) break;
		if(i == pending.bound_instance_count || pending.bound_instances[i].body.kind != ASN1TYPED_TYPE_SEQUENCE_OF) goto unsupported;
	}
	*out = pending; return 0;
unsupported:
	set_error(error, size, "unsupported physical single-container IOC message graph");
fail:
	if(error && size && !error[0]) set_error(error, size, "out of memory owning physical IOC message graph");
	asn1typed_module_clear(&pending); return -1;
}
