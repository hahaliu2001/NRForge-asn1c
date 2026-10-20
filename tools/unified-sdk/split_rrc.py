"""Move generated per-type UPER codec bodies out of large public root headers.

This is a layout transformation, not a codec renderer. Bodies are preserved byte
for byte except removal of `inline`; declarations retain the original defaults.
Only the six newly selected roots are changed. Historical 85 headers stay intact.
"""
import re
from pathlib import Path

START = re.compile(r'^inline (::nrforge::uper::Result<(?:void|T\d+|Decoded|::nrforge::uper::CompleteEncoding)> (?:put\d+|get\d+|encode_unchecked|decode_unchecked)\()', re.M)


def body_end(text, start):
    """Match C++ braces, ignoring strings/comments (including initializer braces)."""
    depth = 0
    state = None
    i = start
    while i < len(text):
        c = text[i]
        if state in ('"', "'"):
            if c == '\\': i += 2; continue
            if c == state: state = None
        elif state == '//':
            if c == '\n': state = None
        elif state == '/*':
            if text.startswith('*/', i): state = None; i += 2; continue
        elif text.startswith('//', i): state = '//'; i += 2; continue
        elif text.startswith('/*', i): state = '/*'; i += 2; continue
        elif c in ('"', "'"): state = c
        elif c == '{': depth += 1
        elif c == '}':
            depth -= 1
            if depth == 0: return i + 1
        i += 1
    raise ValueError('Unbalanced generated codec function')


def split_header(text):
    bodies = []
    pieces = []
    pos = 0
    for match in START.finditer(text):
        if match.start() < pos: raise ValueError('Nested generated codec definition')
        # A signature may contain default initializers `{}`. Parentheses delimit
        # the function parameters; only the following brace starts its body.
        level = 1; cursor = match.end()
        while level and cursor < len(text):
            level += (text[cursor] == '(') - (text[cursor] == ')')
            cursor += 1
        if level or text[cursor:cursor+1] != '{':
            raise ValueError('Unexpected generated codec signature')
        end = body_end(text, cursor)
        signature = text[match.start()+len('inline '):cursor]
        pieces += [text[pos:match.start()], signature+';']
        # Only the two root entry points have parameter defaults. Preserve their
        # defaults in declarations, strip exactly the renderer's known defaults
        # in the out-of-line definitions. Reject any remaining default syntax.
        definition = signature.replace('={}', '').replace(' = {}', '').replace(
            '=::nrforge::uper::Completion::octets', '')
        if '=' in definition: raise ValueError('Unknown generated default argument')
        bodies.append(definition+text[cursor:end])
        pos = end
    pieces.append(text[pos:])
    if not bodies or len([b for b in bodies if re.search(r'\bput0\(', b.split('{',1)[0])]) != 1:
        raise ValueError('Missing unique root writer definition')
    if sum(bool(re.search(r'\bget0\(', b.split('{',1)[0])) for b in bodies) != 1:
        raise ValueError('Missing unique root reader definition')
    if sum(' encode_unchecked(' in b for b in bodies) != 1 or sum(' decode_unchecked(' in b for b in bodies) != 1:
        raise ValueError('Missing generated public codec wrappers')
    result = ''.join(pieces)
    if re.search(r'^inline ::nrforge::uper::Result<', result, re.M):
        raise ValueError('Unrecognized codec definition remains inline')
    return result, bodies


def split_roots(generated, entries, *, functions_per_unit=32):
    if functions_per_unit < 1: raise ValueError('Invalid codec partition size')
    sources = []
    details = []
    for entry in entries:
        if entry['index'] < 85: continue
        header = generated/'include'/entry['header']
        text, bodies = split_header(header.read_text())
        header.write_text(text)
        stem = 'r%03d' % entry['index']
        files = []
        for offset in range(0, len(bodies), functions_per_unit):
            name = '%s-codecs-%04d.cpp' % (stem, offset//functions_per_unit)
            (generated/name).write_text('#include <'+entry['header']+'>\nnamespace '+entry['namespace']+' {\n'+'\n'.join(bodies[offset:offset+functions_per_unit])+'\n}\n')
            files.append(name)
        sources += files
        details.append(dict(index=entry['index'], functions=len(bodies), units=files))
    if len(details) != 6: raise ValueError('Exactly six additional roots must be partitioned')
    return sources, details


def reseal_layout(generated, producer_revision):
    """Derive a new sealed layout from a separately verified generation copy."""
    import hashlib
    import json
    def sha(path): return hashlib.sha256(path.read_bytes()).hexdigest()
    def digest(value):
        return hashlib.sha256(json.dumps(value,sort_keys=True,separators=(',',':')).encode()).hexdigest()
    provenance = json.loads((generated/'sdk-provenance.json').read_text())
    manifest = json.loads((generated/'manifest.json').read_text())
    original = sha(generated/'sdk-provenance.json')
    old = provenance['identity']['fingerprint']
    sources, details = split_roots(generated, manifest['roots'])
    cmake = generated/'CMakeLists.txt'
    before = cmake.read_text()
    needle = ')\nnrforge_package_rrc_sdk(nrforge_rrc)'
    if before.count(needle) != 1: raise ValueError('Unknown generated CMake layout')
    cmake.write_text(before.replace(needle, ''.join(' '+s+'\n' for s in sources)+needle))
    for entry in manifest['roots']:
        entry['installed_header_sha256'] = sha(generated/'include'/entry['header'])
    (generated/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
    # The exact transformation itself becomes a locked input of the new layout.
    (generated/'build-layout-producer.py').write_bytes(Path(__file__).read_bytes())
    layout = dict(producer_revision=producer_revision, original_provenance_sha256=original,
                  original_fingerprint=old, roots=details)
    (generated/'codec-layout.json').write_text(json.dumps(layout,indent=2)+'\n')
    # Same initial generated-input closure as build_rrc.py: identity/receipt files
    # are derived afterwards and independently covered by sdk-lock.cmake.
    derived = {'sdk-provenance.json','sdk-lock.cmake','sdk_identity.cpp',
               'sdk_link_check.cpp','include/nrforge/rrc/sdk_version.hpp'}
    inputs = {str(p.relative_to(generated)):sha(p) for p in sorted(generated.rglob('*'))
              if p.is_file() and str(p.relative_to(generated)) not in derived}
    fingerprint = digest(dict(version='0.3.0',sources=provenance['source_inputs'],
                              generated=inputs,schema=provenance['source_authority']))
    for name in ('include/nrforge/rrc/sdk_version.hpp','sdk_identity.cpp'):
        path=generated/name
        if old not in path.read_text(): raise ValueError('Missing old SDK identity')
        path.write_text(path.read_text().replace(old,fingerprint))
    provenance['identity']['fingerprint']=fingerprint
    provenance['generated_inputs']=inputs
    provenance['build_layout']=layout
    (generated/'sdk-provenance.json').write_text(json.dumps(provenance,indent=2,sort_keys=True)+'\n')
    hashes={str(p.relative_to(generated)):sha(p) for p in sorted(generated.rglob('*'))
            if p.is_file() and p.name!='sdk-lock.cmake'}
    identity=provenance['identity']
    lines=[f'set(NRFORGE_SDK_VERSION "{identity["version"]}")',
           f'set(NRFORGE_SDK_FINGERPRINT "{fingerprint}")']
    for key in ('schema_sha256','runtime_sha256'):
        lines.append(f'set(NRFORGE_{key.upper()} "{identity[key]}")')
    for scope,mapping in [('SOURCE',provenance['source_inputs']),('GENERATED',hashes)]:
        lines += ['set(NRFORGE_'+scope+'_HASHES']+[' "'+k+'|'+v+'"' for k,v in sorted(mapping.items())]+[')']
    (generated/'sdk-lock.cmake').write_text('\n'.join(lines)+'\n')
    return layout
