#include "asn1typed_extract.h"

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
