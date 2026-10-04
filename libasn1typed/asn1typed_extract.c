#include "asn1typed_extract.h"
#include "asn1typed_name.h"
#include <asn1fix_export.h>
#include <asn1_namespace.h>

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

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
	default: return ASN1TYPED_PRIMITIVE_INVALID;
	}
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

static int
add_field(asn1typed_type_t *type, asn1p_expr_t *field,
		const char *file, const char *module, char *error, size_t error_size) {
	asn1typed_presence_e presence;
	int marker_flags;
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
	{
		asn1typed_type_ref_t ref;
		memset(&ref, 0, sizeof(ref));
		if(put_ref(&ref, field)) {
			set_error(error, error_size, "%s.%s: unsupported field type at line %d",
				module, field->Identifier, field->_lineno);
			return -1;
		}
		if(ref.kind == ASN1TYPED_REF_PRIMITIVE) {
			result = asn1typed_type_add_primitive_field(type, field->Identifier,
				ref.primitive_kind, presence, file, field->_lineno);
		} else {
			result = asn1typed_type_add_field(type, field->Identifier,
				ref.module, ref.source_name, presence, file, field->_lineno);
		}
		asn1typed_type_ref_clear(&ref);
	}
	if(result) {
		set_error(error, error_size, "%s.%s: out of memory extracting field",
			module, field->Identifier);
		return -1;
	}
	return 0;
}

static int
populate_type(asn1typed_type_t *out, asn1p_expr_t *decl,
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
		if(primitive == ASN1TYPED_PRIMITIVE_INVALID)
			primitive = primitive_from_name(reference_name(body));
		if(asn1typed_type_set_primitive(out, primitive)) {
			set_error(error, error_size, "%s: unsupported primitive type",
				decl->Identifier);
			return -1;
		}
		return 0;
	}
	case ASN1TYPED_TYPE_SEQUENCE:
		TQ_FOR(member, &body->members, next) {
			if(member->expr_type == A1TC_EXTENSIBLE) {
				set_error(error, error_size,
					"%s: SEQUENCE extension marker is unsupported", decl->Identifier);
				return -1;
			}
			if(add_field(out, member, file, decl->module->ModuleName,
					error, error_size)) return -1;
		}
		return 0;
	case ASN1TYPED_TYPE_SEQUENCE_OF:
		member = TQ_FIRST(&body->members);
		if(!member || TQ_NEXT(member, next)) {
			set_error(error, error_size, "%s: malformed SEQUENCE OF element",
				decl->Identifier);
			return -1;
		}
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
		TQ_FOR(member, &body->members, next) {
			if(member->expr_type == A1TC_EXTENSIBLE) {
				set_error(error, error_size, "%s: extensible ENUMERATED is unsupported",
					decl->Identifier);
				return -1;
			}
			if(!member->Identifier || asn1typed_type_add_enum_item(out,
					member->Identifier, file, member->_lineno)) {
				set_error(error, error_size, "%s: invalid ENUMERATED item",
					decl->Identifier);
				return -1;
			}
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
		if(populate_type(type, decl, file, error, error_size)) goto fail;
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
	const asn1p_constraint_t *value_ct = NULL;
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
			ct->el_count != (bit == 4 ? 2u : 1u)) return -1;
		setting = ct->elements[0];
		if(!setting || setting->type != ACT_EL_VALUE || setting->el_count ||
			ioc_resolve(tree, member, ioc_set_reference(setting->value)) != set) return -1;
		if(bit == 1) id = member;
		if(bit == 4) value_ct = ct;
	}
	if(seen != 7 || !id || !id->Identifier || !value_ct) return -1;
	{
		const asn1p_constraint_t *at = value_ct->elements[1];
		asn1p_ref_t *ref;
		const char *path;
		if(!at || at->type != ACT_EL_VALUE || at->el_count || !at->value ||
			at->value->type != ATV_REFERENCED) return -1;
		ref = at->value->value.reference;
		if(!ref || ref->comp_count != 1 || !ref->components ||
			!(path = ref->components[0].name) || path[0] != '@' ||
			strcmp(path + 1, id->Identifier)) return -1;
	}
	return 0;
}

/* One mandatory parameterized SEQUENCE OF whose element uses the same set
 * for its class fields and selects &Value through its &id component. */
static asn1p_expr_t *
message_object_set(asn1p_t *tree, asn1p_expr_t *message) {
	asn1p_expr_t *body = terminal_type(message);
	asn1p_expr_t *field, *container, *set;
	if(!body || body->expr_type != ASN_CONSTR_SEQUENCE) return NULL;
	field = TQ_FIRST(&body->members);
	if(!field || TQ_NEXT(field, next) || field->marker.flags != EM_NOMARK ||
		field->expr_type != A1TC_REFERENCE || !field->rhs_pspecs) return NULL;
	container = terminal_type(ioc_resolve(tree, field, field->reference));
	if(!container || container->expr_type != ASN_CONSTR_SEQUENCE_OF) return NULL;
	set = ioc_actual_set(tree, field);
	if(!set || validate_ioc_element(tree, container, set)) return NULL;
	return set;
}

/* A symbolic value reference is semantic evidence; Identifier is not. */
static const char *
ioc_value_symbol(asn1p_expr_t *expr) {
	asn1p_ref_t *ref;
	if(!expr || !expr->value || expr->value->type != ATV_REFERENCED)
		return NULL;
	ref = expr->value->value.reference;
	if(!ref || !ref->components || !ref->comp_count || ref->comp_count > 2)
		return NULL;
	return ref->components[ref->comp_count - 1].name;
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
	asn1p_expr_t value, *id;
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
	symbol = ioc_value_symbol(cells[0]);
	if(!symbol || !*symbol) goto bad_id;
	/* Retain T3 source_name compatibility; naming owns the IOC convention. */
	name = asn1typed_name_ioc_identity(symbol);
	if(!*name) goto bad_id;
	id = ioc_resolve(tree, cells[0], cells[0]->value->value.reference);
	if(!id || id->meta_type != AMT_VALUE || !id->value ||
		id->value->type != ATV_INTEGER) goto bad_id;
	has_numeric_id = id->value->value.v_integer >= INTMAX_MIN &&
		id->value->value.v_integer <= INTMAX_MAX;
	if(has_numeric_id) numeric_id = (intmax_t)id->value->value.v_integer;
	for(i = 0; i < message->field_count; ++i) {
		if(!strcmp(message->fields[i].source_name, name)) {
			set_error(error, error_size, "duplicate IOC semantic field identity '%s'", name);
			return -1;
		}
	}
	p = ioc_value_symbol(cells[2]);
	if(p && !strcmp(p, "mandatory")) presence = ASN1TYPED_PRESENCE_MANDATORY;
	else if(p && !strcmp(p, "optional")) presence = ASN1TYPED_PRESENCE_OPTIONAL;
	else if(p && !strcmp(p, "conditional")) presence = ASN1TYPED_PRESENCE_CONDITIONAL;
	else {
		set_error(error, error_size, "unrecognized IOC presence");
		return -1;
	}
	c = ioc_value_symbol(cells[3]);
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

/* Add each reachable ordinary declaration once. The outer worklist handles
 * recursion without retaining a types-array pointer across reallocations. */
static int
add_ioc_dependency(asn1typed_module_t *out, asn1p_module_t *source,
		const asn1typed_type_ref_t *ref, char *error, size_t error_size) {
	asn1p_expr_t *decl;
	asn1typed_type_t *type;
	asn1typed_type_kind_e kind;
	size_t i;
	if(ref->kind == ASN1TYPED_REF_PRIMITIVE) return 0;
	if(!ref->module || !ref->source_name || strcmp(ref->module, source->ModuleName)) {
		set_error(error, error_size, "IOC dependency outside selected module is unsupported");
		return -1;
	}
	for(i = 0; i < out->type_count; ++i)
		if(!strcmp(out->types[i].identity.source_name, ref->source_name)) return 0;
	decl = named_declaration(source, ref->source_name);
	if(!decl || decl->meta_type != AMT_TYPE || decl->lhs_params || decl->rhs_pspecs ||
		(kind = kind_of_type(terminal_type(decl))) == (asn1typed_type_kind_e)-1) {
		set_error(error, error_size, "unresolved or unsupported IOC dependency '%s'", ref->source_name);
		return -1;
	}
	if(asn1typed_module_add_type(out, decl->Identifier, kind, out->location.file,
			decl->_lineno > 0 ? (unsigned)decl->_lineno : 0, &type)) {
		set_error(error, error_size, "out of memory storing IOC dependency");
		return -1;
	}
	return populate_type(type, decl, out->location.file, error, error_size);
}

int
asn1typed_extract_message(asn1p_t *tree, const char *module_name,
		const char *message_name, asn1typed_module_t *out,
		char *error, size_t error_size) {
	asn1p_module_t *source;
	asn1p_expr_t *message = NULL, *set;
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
	if(!set->ioc_table || !set->ioc_table->rows || !set->ioc_table->row) {
		set_error(error, error_size, "missing or empty IOC table for '%s'", message_name);
		return -1;
	}
	file = source->source_file_name ? source->source_file_name : "<unknown>";
	if(asn1typed_module_init(out, module_name, file, 1) ||
		asn1typed_module_add_type(out, message_name, ASN1TYPED_TYPE_SEQUENCE,
			file, message->_lineno > 0 ? (unsigned)message->_lineno : 0, &type)) {
		set_error(error, error_size, "out of memory storing IOC message");
		goto fail;
	}
	for(i = 0; i < set->ioc_table->rows; ++i)
		if(extract_ioc_row(tree, type, set->ioc_table->row[i], file, error, error_size))
			goto fail;
	for(i = 0; i < out->type_count; ++i) {
		for(j = 0; j < out->types[i].field_count; ++j)
			if(add_ioc_dependency(out, source, &out->types[i].fields[j].type,
					error, error_size)) goto fail;
		if(out->types[i].kind == ASN1TYPED_TYPE_SEQUENCE_OF) {
			/* element_type lives inside the reallocatable array: copy it first. */
			asn1typed_type_ref_t ref = out->types[i].element_type;
			if(add_ioc_dependency(out, source, &ref, error, error_size)) goto fail;
		}
	}
	return 0;
fail:
	asn1typed_module_clear(out);
	return -1;
}
