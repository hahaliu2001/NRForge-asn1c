#!/usr/bin/env python3
"""Generate fail-closed converters from installed public NGAP C++ headers only.

This deliberately recognizes the finite declaration grammar emitted by the SDK,
not arbitrary C++. Unrecognized declarations/types abort before outputs publish.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import re

IDENT = r'[A-Za-z_][A-Za-z_0-9]*'
BASE = {'bool', '::std::uint64_t', '::std::int64_t', '::std::byte',
        '::std::string', '::std::monostate', '::nrforge::aper::BitString',
        '::nrforge::aper::SequenceExtensionData'}


def split_types(value):
    result, start, depth = [], 0, 0
    for i, ch in enumerate(value):
        if ch == '<': depth += 1
        elif ch == '>': depth -= 1
        elif ch == ',' and depth == 0:
            result.append(value[start:i].strip()); start = i + 1
        if depth < 0: raise ValueError('unbalanced type')
    if depth: raise ValueError('unbalanced type')
    result.append(value[start:].strip())
    return result


def validate_type(value, namespace, known):
    if value in BASE: return
    prefix = '::' + namespace + '::'
    if value.startswith(prefix) and value[len(prefix):] in known: return
    m = re.fullmatch(r'::std::(optional|vector|variant)<(.+)>', value)
    if m:
        args = split_types(m[2])
        if m[1] != 'variant' and len(args) != 1: raise ValueError('invalid type arity')
        for arg in args: validate_type(arg, namespace, known)
        return
    raise ValueError(f'unrecognized or forward type: {value}')


def parse_header(text, namespace):
    allowed_includes = {'array', 'cstddef', 'cstdint', 'optional', 'sdk_version.hpp',
                        'sequence_extensions.hpp', 'span', 'string', 'variant', 'vector'}
    for line in text.splitlines():
        if line.lstrip().startswith('#'):
            include = re.fullmatch(r'\s*#include <([^>]+)>\s*', line)
            if not include or include[1] not in allowed_includes:
                raise ValueError('unrecognized preprocessor declaration: ' + line)
    text = re.sub(r'^\s*#.*$', '', text, flags=re.M)
    text = re.sub(r'//[^\n]*', '', text)
    text = re.sub(r'namespace ' + re.escape(namespace) + r'\s*\{', '', text)
    # Namespace closing braces occupy their own line, unlike struct bodies.
    text = re.sub(r'^\s*}\s*$', '', text, flags=re.M)
    declarations, known, pos = [], set(), 0
    while pos < len(text):
        if text[pos].isspace(): pos += 1; continue
        alias = re.match(r'using (' + IDENT + r')\s*=\s*([^;]+);', text[pos:])
        if alias:
            name, target = alias.groups(); validate_type(target.strip(), namespace, known)
            if name in known: raise ValueError('duplicate name ' + name)
            known.add(name); declarations.append({'name': name, 'kind': 'alias', 'target': target.strip()})
            pos += alias.end(); continue
        start = re.match(r'struct (' + IDENT + r')\s*\{', text[pos:])
        if not start: raise ValueError('unrecognized declaration: ' + text[pos:pos + 100])
        name = start[1]; body_start = pos + start.end(); end, depth = body_start, 1
        while end < len(text) and depth:
            if text[end] == '{': depth += 1
            elif text[end] == '}': depth -= 1
            end += 1
        if depth or text[end:end + 1] != ';': raise ValueError('unterminated struct')
        body = text[body_start:end - 1].strip(); pos = end + 1
        if name in known: raise ValueError('duplicate name ' + name)
        if name.endswith('_constraint'):
            pattern = r'static constexpr ::std::(?:u?int64_t) (?:lower_bound|upper_bound) = [^;]+;'
            entries = re.findall(pattern, body)
            if len(entries) != 2 or re.sub(pattern, '', body).strip(): raise ValueError('unknown constraint shape')
            known.add(name); declarations.append({'name': name, 'kind': 'constraint'}); continue
        if body.startswith('enum class Known'):
            match = re.fullmatch(r'enum class Known : ::std::int64_t\s*\{(.*?)\};\s*(.*?)', body, re.S)
            if not match: raise ValueError('unknown enum shape')
            enum_items = re.findall(r'(' + IDENT + r') = INT64_C\((-?\d+)\),', match[1])
            if not enum_items or re.sub(r'' + IDENT + r' = INT64_C\(-?\d+\),', '', match[1]).strip():
                raise ValueError('unknown enum members')
            tail = match[2]
            plain = re.fullmatch(r'Known value\{Known::' + IDENT + r'\};', tail)
            extended = re.fullmatch(r'struct UnknownExtension \{ ::std::uint64_t index; \};\s*::std::variant<Known, UnknownExtension> value\{Known::' + IDENT + r'\};', tail)
            if not (plain or extended): raise ValueError('unknown enum storage')
            declarations.append({'name': name, 'kind': 'enum', 'items': [x[0] for x in enum_items], 'extensible': bool(extended)})
        else:
            fields = []
            for field in body.split(';'):
                if not field.strip(): continue
                match = re.fullmatch(r'\s*(.+?)\s+(' + IDENT + r')\{\}\s*', field)
                if not match: raise ValueError('unknown member ' + name + ': ' + field)
                field_type, field_name = match.groups(); validate_type(field_type, namespace, known)
                fields.append({'name': field_name, 'type': field_type, 'optional': field_type.startswith('::std::optional<')})
            if len({x['name'] for x in fields}) != len(fields): raise ValueError('duplicate member')
            declarations.append({'name': name, 'kind': 'struct', 'fields': fields})
        known.add(name)
    return declarations


def render(stem, declarations, profile="ngap"):
    namespace = 'nrforge::' + profile + '::messages::' + stem
    qual = lambda name: '::' + namespace + '::' + name
    converted = [d for d in declarations if d['kind'] in ('struct', 'enum')]
    out = ['// Generated solely from installed public SDK headers.', '#include "conversion.hpp"',
           f'#include <messages/{stem}.hpp>', 'namespace nrforge::python_sdk {']
    for d in converted:
        q = qual(d['name'])
        out += [f'template<> struct TypeName<{q}> {{ static const char* name() {{ return "{d["name"]}"; }} }};',
                f'template<> struct Convert<{q}> {{ static py::object to(const {q}&, ConversionBudget&); static {q} from(py::handle, ConversionBudget&); }};']
    for d in converted:
        q = qual(d['name'])
        out += [f'py::object Convert<{q}>::to(const {q}& v, ConversionBudget& budget) {{', '    auto scope = budget.enter();']
        if d['kind'] == 'enum':
            known = 'std::get<' + q + '::Known>(v.value)' if d['extensible'] else 'v.value'
            if d['extensible']:
                out += [f'    if (const auto* ext = std::get_if<{q}::UnknownExtension>(&v.value)) {{',
                        '        py::dict result;', '        result["extension_index"] = Convert<::std::uint64_t>::to(ext->index, budget);', '        return result;', '    }']
            out.append(f'    switch ({known}) {{')
            for label in d['items']: out.append(f'    case {q}::Known::{label}: return named_label("{label}", budget);')
            out += ['    }', '    throw py::value_error("invalid C++ enum value");']
        else:
            out += ['    (void)v;', '    py::dict result;']
            for f in d['fields']: out.append(f'    result["{f["name"]}"] = Convert<{f["type"]}>::to(v.{f["name"]}, budget);')
            out.append('    return result;')
        out += ['}', f'{q} Convert<{q}>::from(py::handle value, ConversionBudget& budget) {{', '    auto scope = budget.enter();']
        if d['kind'] == 'enum':
            out += ['    if (py::isinstance<py::str>(value)) {', '        const auto label = read_label(value, budget);']
            for label in d['items']: out.append(f'        if (label == "{label}") return {q}{{{q}::Known::{label}}};')
            out += ['        throw py::value_error("unknown enum label");', '    }']
            if d['extensible']:
                out += ['    auto dict = require_dict(value);', '    check_keys(dict, {"extension_index"});',
                        f'    return {q}{{{q}::UnknownExtension{{Convert<::std::uint64_t>::from(required(dict, "extension_index"), budget)}}}};']
            else: out.append('    throw py::type_error("enum requires a known string label");')
        else:
            out += ['    auto dict = require_dict(value);', '    check_keys(dict, {' + ', '.join('"' + f['name'] + '"' for f in d['fields']) + '});', f'    {q} result{{}};']
            for f in d['fields']:
                access = 'optional_value' if f['optional'] else 'required'
                out.append(f'    result.{f["name"]} = Convert<{f["type"]}>::from({access}(dict, "{f["name"]}"), budget);')
            out.append('    return result;')
        out.append('}')
    body = '::' + namespace + '::Body'
    out += [f'bool body_is_{stem}(const Pdu& pdu) {{ return pdu.body_if<{body}>() != nullptr; }}',
            f'const std::string& body_name_{stem}() {{', f'    static const std::string name = [] {{ auto pdu = unwrap(::nrforge::{profile}::make_{profile}_pdu({body}{{}})); return std::string(pdu.message_info()->message); }}();', '    return name;', '}',
            f'py::object body_to_{stem}(const Pdu& pdu, ConversionBudget& budget) {{', f'    const auto* body = pdu.body_if<{body}>();', '    if (!body) throw py::type_error("PDU body type mismatch");', f'    return Convert<{body}>::to(*body, budget);', '}',
            f'Pdu body_from_{stem}(py::handle value, std::optional<Criticality> criticality, ConversionBudget& budget) {{', f'    auto body = Convert<{body}>::from(value, budget);', f'    return unwrap(::nrforge::{profile}::make_{profile}_pdu(std::move(body), criticality));', '}', '} // namespace nrforge::python_sdk', '']
    return '\n'.join(out)


def verify_installed_sdk(prefix, expected_fingerprint=None, profile="ngap"):
    """Bind declaration parsing to the installed sealed SDK public inputs.

    Installed provenance omits the location-normalized manifest used to create
    the fingerprint, so reconstructing that digest is intentionally impossible.
    The CMake package supplies the independently read expected fingerprint; every
    exposed header is still checked against the recorded original source hash.
    """
    if profile not in ("ngap", "f1ap"): raise ValueError("unsupported protocol profile")
    provenance_path = prefix / f'share/nrforge-{profile}/sdk-provenance.json'
    provenance_bytes = provenance_path.read_bytes()
    def unique_object(pairs):
        result = {}
        for key, value in pairs:
            if key in result: raise ValueError('duplicate provenance key: ' + key)
            result[key] = value
        return result
    provenance = json.loads(provenance_bytes, object_pairs_hook=unique_object)
    for key in ('fingerprint', 'schema_sha256', 'runtime_sha256'):
        if not re.fullmatch(r'[0-9a-f]{64}', provenance[key]): raise ValueError('invalid SDK identity')
    if provenance['version'] != '0.1.0': raise ValueError('unsupported SDK version')
    if expected_fingerprint is not None and provenance['fingerprint'] != expected_fingerprint:
        raise ValueError('SDK fingerprint differs from CMake package')
    include = prefix / f'include/nrforge/{profile}'
    qualified = (prefix / f'include/nrforge/{profile}/{profile}.hpp').read_bytes().startswith(f'#include <nrforge/{profile}/sdk_version.hpp>\n'.encode())
    pdu_headers = (['pdu.hpp', 'pdu_declarations.inc'] if qualified else ['pdu.hpp']) if profile == 'ngap' else ['f1ap_pdu.hpp', 'pdu_declarations.inc']
    expected = {'sdk_version.hpp', profile + '.hpp', 'runtime.hpp', 'sequence_extensions.hpp', *pdu_headers}
    original = {profile + '.hpp': provenance['generated_inputs'][profile + '.hpp'],
                'runtime.hpp': provenance['source_hashes']['libaper/runtime.hpp'],
                'sequence_extensions.hpp': provenance['source_hashes']['libaper/sequence_extensions.hpp']}
    original.update({name: provenance['source_hashes']['libngap/' + name] for name in pdu_headers})
    for name, digest in provenance['generated_inputs'].items():
        if re.fullmatch(r'messages/' + IDENT + r'(?:_types)?\.hpp', name) and not name.endswith(('_mapping.hpp', '_codec.hpp')):
            expected.add(name); original[name] = digest
    if len(expected) != ((268 if qualified else 267) if profile == 'ngap' else 322): raise ValueError('incomplete SDK public header inventory')
    actual = {p.relative_to(include).as_posix() for p in include.rglob('*') if p.is_file()}
    if actual != expected: raise ValueError('SDK public header inventory mismatch')
    texts, header_hashes = {}, {}
    guard = (f'#include <nrforge/{profile}/sdk_version.hpp>\n'.encode() if qualified else b'#include <sdk_version.hpp>\n')
    for name in sorted(expected):
        path = include / name
        if path.is_symlink() or not path.resolve().is_relative_to(include.resolve()):
            raise ValueError('escaped SDK public header')
        data = path.read_bytes(); header_hashes[name] = hashlib.sha256(data).hexdigest()
        if name != 'sdk_version.hpp':
            if not data.startswith(guard): raise ValueError('SDK public header guard missing: ' + name)
            content = data[len(guard):].decode('utf-8')
            if qualified:
                def original_include(match):
                    included = match[1]
                    if included not in expected: raise ValueError('unknown qualified include')
                    if name in pdu_headers or (name.startswith('messages/') and included.startswith('messages/')):
                        return '#include "' + Path(included).name + '"'
                    return '#include <' + included + '>'
                content = re.sub(r'#include <nrforge/' + profile + r'/([^>]+)>', original_include, content)
                def installed_include(match):
                    included = match[1]
                    candidate = included if included in expected else str(Path(name).parent / included)
                    if candidate in expected:
                        return '#include <nrforge/' + profile + '/' + candidate + '>'
                    return match[0]
                canonical = re.sub(r'#include [<"]([^>"]+)[>"]', installed_include, content)
                if guard + canonical.encode() != data:
                    raise ValueError('noncanonical SDK installed include rewrite: ' + name)
            if hashlib.sha256(content.encode()).hexdigest() != original[name]:
                raise ValueError('SDK public header hash mismatch: ' + name)
        texts[name] = data.decode('utf-8')
        if qualified and name.startswith('messages/'):
            texts[name] = texts[name].replace(f'nrforge/{profile}/', '')
    version = texts['sdk_version.hpp']
    identity = re.search(r'inline constexpr SdkIdentity header_sdk_identity\{\s*(.*?)\s*\};', version, re.S)
    values = re.findall(r'"([^"\n]*)"', identity[1]) if identity else []
    if values != [provenance[k] for k in ('version', 'fingerprint', 'schema_sha256', 'runtime_sha256', 'source_revision')]:
        raise ValueError('SDK version header identity mismatch')
    symbol = 'sdk_require_' + provenance['fingerprint']
    if f'void {symbol}() noexcept;' not in version or f'{symbol}(); return true;' not in version:
        raise ValueError('SDK version header link guard mismatch')
    expected_version = (f'#ifndef NRFORGE_{profile.upper()}_SDK_VERSION_HPP\n'
                        f'#define NRFORGE_{profile.upper()}_SDK_VERSION_HPP\n#include <string_view>\n'
                        'namespace nrforge::' + profile + ' {\nstruct SdkIdentity {\n'
                        '    std::string_view version, fingerprint, schema_sha256, runtime_sha256, source_revision;\n'
                        '};\nconst SdkIdentity& sdk_identity() noexcept;\n'
                        'inline constexpr SdkIdentity header_sdk_identity{\n')
    expected_version += ',\n'.join('    ' + json.dumps(provenance[key])
                                    for key in ('version', 'fingerprint', 'schema_sha256', 'runtime_sha256', 'source_revision'))
    expected_version += ('\n};\nnamespace detail {\nvoid ' + symbol + '() noexcept;\n'
                         'inline const bool sdk_header_library_guard = [] { ' + symbol + '(); return true; }();\n}\n}\n#endif\n')
    if version != expected_version: raise ValueError('unrecognized SDK version header content')
    receipt = {'fingerprint': provenance['fingerprint'], 'schema_sha256': provenance['schema_sha256'],
               'runtime_sha256': provenance['runtime_sha256'], 'version': provenance['version'],
               'provenance_sha256': hashlib.sha256(provenance_bytes).hexdigest(), 'public_header_sha256': header_hashes}
    return texts, receipt


def generate(prefix, output, expected_fingerprint=None, profile="ngap"):
    public_texts, sdk_receipt = verify_installed_sdk(prefix, expected_fingerprint, profile)
    include = prefix / f'include/nrforge/{profile}'
    headers = sorted((include / 'messages').glob('*_types.hpp'))
    count = 131 if profile == 'ngap' else 158
    if len(headers) != count: raise ValueError(f'expected complete {count} message SDK, got {len(headers)}')
    rendered, metadata = {}, {}
    for header in headers:
        stem = header.name.removesuffix('_types.hpp'); ns = 'nrforge::' + profile + '::messages::' + stem
        public = public_texts['messages/' + stem + '.hpp']
        body_match = re.search(r'using Body = ::' + re.escape(ns) + r'::(' + IDENT + r');', public)
        if not body_match: raise ValueError('unrecognized public Body alias: ' + stem)
        declarations = parse_header(public_texts['messages/' + header.name], ns)
        if body_match[1] not in {x['name'] for x in declarations}: raise ValueError('Body missing')
        metadata[stem] = {'body': body_match[1], 'declarations': declarations, 'header_sha256': sdk_receipt['public_header_sha256']['messages/' + header.name]}
        rendered[stem + '.cpp'] = render(stem, declarations, profile)
    stems = list(metadata)
    h = ['#pragma once', '#include "conversion.hpp"', 'namespace nrforge::python_sdk {']
    for stem in stems:
        h += [f'bool body_is_{stem}(const Pdu&);', f'const std::string& body_name_{stem}();', f'py::object body_to_{stem}(const Pdu&, ConversionBudget&);', f'Pdu body_from_{stem}(py::handle, std::optional<Criticality>, ConversionBudget&);']
    h += ['py::object to_body(const Pdu&, ConversionBudget&);', 'Pdu from_body(const std::string&, py::handle, std::optional<Criticality>, ConversionBudget&);', 'py::dict message_schema();', '}', '']
    rendered['bindings.hpp'] = '\n'.join(h)
    dispatch = ['#include "bindings.hpp"', 'namespace nrforge::python_sdk {', 'py::object to_body(const Pdu& pdu, ConversionBudget& budget) {']
    for stem in stems: dispatch.append(f'    if (body_is_{stem}(pdu)) return body_to_{stem}(pdu, budget);')
    dispatch += ['    throw py::type_error("PDU has no registered typed body");', '}', 'Pdu from_body(const std::string& name, py::handle value, std::optional<Criticality> criticality, ConversionBudget& budget) {']
    for stem in stems: dispatch.append(f'    if (name == body_name_{stem}()) return body_from_{stem}(value, criticality, budget);')
    dispatch += [f'    throw py::value_error("unknown {profile.upper()} message name");', '}', 'py::dict message_schema() {', '    py::dict result;']
    for stem in stems:
        # JSON text is parsed by Python's standard library, not schema tools.
        data = json.dumps(metadata[stem], separators=(',', ':'))
        dispatch.append(f'    result[py::str(body_name_{stem}())] = py::module_::import("json").attr("loads")({json.dumps(data)});')
    dispatch += ['    return result;', '}', '}', '']
    rendered['dispatch.cpp'] = '\n'.join(dispatch)
    rendered['sources.cmake'] = 'set(NRFORGE_PYTHON_SOURCES\n' + ''.join(f'  "${{CMAKE_CURRENT_LIST_DIR}}/{s}.cpp"\n' for s in stems) + '  "${CMAKE_CURRENT_LIST_DIR}/dispatch.cpp"\n)\n'
    counts = {kind: sum(d['kind'] == kind for m in metadata.values() for d in m['declarations']) for kind in ('struct', 'enum', 'alias', 'constraint')}
    rendered['conversion-schema.json'] = json.dumps({'messages': metadata, 'counts': counts, 'sdk': sdk_receipt}, indent=2, sort_keys=True) + '\n'
    output.mkdir(parents=True, exist_ok=True)
    for name, content in rendered.items():
        destination = output / name
        if not destination.exists() or destination.read_text() != content:
            destination.write_text(content)
    return counts


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--profile', choices=('ngap', 'f1ap'), default='ngap')
    parser.add_argument('--sdk-prefix', type=Path, required=True)
    parser.add_argument('--output', type=Path)
    parser.add_argument('--verify-only', action='store_true')
    parser.add_argument('--expected-fingerprint')
    args = parser.parse_args()
    if args.verify_only:
        _, receipt = verify_installed_sdk(args.sdk_prefix.resolve(), args.expected_fingerprint, args.profile)
        print(json.dumps({'status': 'PASS', 'fingerprint': receipt['fingerprint'],
                          'public_header_count': len(receipt['public_header_sha256'])}, sort_keys=True))
    else:
        if args.output is None: parser.error('--output is required for generation')
        print(json.dumps(generate(args.sdk_prefix.resolve(), args.output.resolve(), args.expected_fingerprint, args.profile), sort_keys=True))
