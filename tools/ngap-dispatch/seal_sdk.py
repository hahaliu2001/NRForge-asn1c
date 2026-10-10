#!/usr/bin/env python3
"""Seal freshly generated SDK inputs before configuring CMake.

Hashes are reproducibility/consistency evidence, not a signature or a replacement
for the accepted finite interoperability profile. No build-time paths are shipped
in installed package metadata.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def digest(value):
    return hashlib.sha256(json.dumps(value, sort_keys=True, separators=(',', ':')).encode()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--repo', type=Path, required=True)
    parser.add_argument('--generated', type=Path, required=True)
    parser.add_argument('--version', default='0.1.0')
    args = parser.parse_args()
    if args.version != '0.1.0':
        raise ValueError('only the current SDK contract version 0.1.0 is supported')
    repo, generated = args.repo.resolve(), args.generated.resolve()
    sealed_names = {'sdk-lock.cmake', 'sdk-provenance.json', 'sdk_version.hpp',
                    'sdk_identity.cpp', 'sdk_link_check.cpp', 'sdk-public'}
    if any((generated / name).exists() for name in sealed_names):
        raise ValueError('SDK is already sealed; regenerate in a new empty directory')
    manifest = json.loads((generated / 'manifest.json').read_text())
    if not all(manifest.get(key) is True for key in ('parser_deleted', 'deterministic')):
        raise ValueError('missing successful owned/deterministic generation evidence')
    if manifest.get('parse') != 'PASS' or manifest.get('fix') != 'PASS':
        raise ValueError('missing parse/fix success')
    authority_path = repo / 'tools/n11-envelope-qualification/source-manifest.json'
    authority = json.loads(authority_path.read_text())
    root = Path(manifest['source_inputs']['asn1_root']).resolve()
    module_paths = [row['path'] for row in authority['modules']]
    selected = Path(manifest['source_inputs']['module_list']).read_text().splitlines()
    selected = [line.strip() for line in selected if line.strip() and not line.lstrip().startswith('#')]
    if sorted(selected) != sorted(module_paths):
        raise ValueError('generation module list differs from pinned schema authority')
    schema_files = {}
    for row in authority['modules']:
        path = (root / row['path']).resolve()
        if not path.is_relative_to(root) or sha(path) != row['sha256']:
            raise ValueError('schema source hash mismatch: ' + row['path'])
        schema_files[row['path']] = row['sha256']
    messages = manifest['messages']
    if len(messages) != manifest['message_count'] or len({(m['role'], m['code']) for m in messages}) != len(messages):
        raise ValueError('duplicate or incomplete message manifest')
    expected = {(role, row['code']) for row in manifest['schema']['procedures']
                for role, present in enumerate(row['payload_present']) if present}
    if {(m['role'], m['code']) for m in messages} != expected:
        raise ValueError('manifest is not complete procedure slot closure')
    for message in messages:
        for key in ('public_header', 'types_header', 'mapping_header', 'codec_header', 'adapter'):
            path = (generated / message[key]).resolve()
            if not path.is_relative_to(generated) or not path.is_file():
                raise ValueError('missing or escaped generated input')
    paths = [*sorted((repo / 'libaper').glob('*.hpp')),
             repo / 'libaper/runtime.cpp', repo / 'libngap/pdu.hpp', repo / 'libngap/pdu.cpp',
             *sorted((repo / 'libasn1typed').glob('asn1typed*.c')),
             *sorted((repo / 'libasn1typed').glob('asn1typed*.h')),
             repo / 'tools/developer_tree.c', repo / 'tools/developer_tree.h',
             repo / 'tools/ngap-dispatch/generate.c', Path(__file__).resolve(), authority_path,
             *sorted((repo / 'cmake').glob('*'))]
    source_hashes = {str(p.relative_to(repo)): sha(p) for p in paths if p.is_file()}
    generated_hashes = {str(p.relative_to(generated)): sha(p) for p in sorted(generated.rglob('*')) if p.is_file()}
    for relative in [*source_hashes, *generated_hashes]:
        if not re.fullmatch(r'[A-Za-z0-9_./-]+', relative):
            raise ValueError('unsafe SDK relative path: ' + relative)
    schema_sha = digest({'release': authority['release'], 'modules': schema_files,
                         'schema': manifest['schema'], 'registry': manifest['registry']})
    runtime_sha = digest({name: value for name, value in source_hashes.items() if name.startswith('libaper/')})
    # Baseline revision plus content fingerprints describe modified source honestly.
    revision = subprocess.check_output(['git', '-C', str(repo), 'rev-parse', 'HEAD'], text=True).strip()
    # The raw manifest is checked by the lock, but build-input locations must
    # not change SDK identity when identical content is generated elsewhere.
    logical_manifest = {key: value for key, value in manifest.items() if key != 'source_inputs'}
    logical_generated_hashes = dict(generated_hashes)
    logical_generated_hashes['manifest.json'] = digest(logical_manifest)
    fingerprint = digest({'version': args.version, 'source_hashes': source_hashes,
                          'generated_hashes': logical_generated_hashes, 'schema_sha256': schema_sha})
    identity = {'version': args.version, 'fingerprint': fingerprint, 'schema_sha256': schema_sha,
                'runtime_sha256': runtime_sha, 'source_revision': revision}
    header = '''#ifndef NRFORGE_NGAP_SDK_VERSION_HPP
#define NRFORGE_NGAP_SDK_VERSION_HPP
#include <string_view>
namespace nrforge::ngap {
struct SdkIdentity {
    std::string_view version, fingerprint, schema_sha256, runtime_sha256, source_revision;
};
const SdkIdentity& sdk_identity() noexcept;
inline constexpr SdkIdentity header_sdk_identity{
'''
    header += ',\n'.join('    ' + json.dumps(identity[key]) for key in identity)
    symbol = 'sdk_require_' + fingerprint
    header += '\n};\nnamespace detail {\nvoid ' + symbol + '() noexcept;\n'
    header += 'inline const bool sdk_header_library_guard = [] { ' + symbol + '(); return true; }();\n}\n}\n#endif\n'
    (generated / 'sdk_version.hpp').write_text(header)
    (generated / 'sdk_identity.cpp').write_text('#include "sdk_version.hpp"\nnamespace nrforge::ngap {\n'
        'const SdkIdentity& sdk_identity() noexcept { return header_sdk_identity; }\n'
        'namespace detail { void ' + symbol + '() noexcept {} }\n}\n')
    (generated / 'sdk_link_check.cpp').write_text(
        '#include <pdu.hpp>\n#include <sdk_version.hpp>\n'
        'int main() {\n'
        '    const auto& registry = ::nrforge::ngap::ngap_registry_state();\n'
        f'    if(!registry || registry.value().message_count() != {len(messages)}U) return 1;\n'
        '    const auto& actual = ::nrforge::ngap::sdk_identity();\n'
        '    const auto& header = ::nrforge::ngap::header_sdk_identity;\n'
        '    return actual.fingerprint == header.fingerprint &&\n'
        '           actual.schema_sha256 == header.schema_sha256 &&\n'
        '           actual.runtime_sha256 == header.runtime_sha256 ? 0 : 2;\n}\n')
    # Every installed entry point participates in the fingerprint link guard,
    # including direct runtime/core/types includes without the umbrella header.
    public = generated / 'sdk-public'
    (public / 'messages').mkdir(parents=True)
    public_inputs = [(repo / 'libngap/pdu.hpp', 'pdu.hpp'),
                     (repo / 'libaper/runtime.hpp', 'runtime.hpp'),
                     (repo / 'libaper/sequence_extensions.hpp', 'sequence_extensions.hpp'),
                     (generated / 'ngap.hpp', 'ngap.hpp')]
    for message in messages:
        public_inputs.extend((generated / message[key], message[key])
                             for key in ('public_header', 'types_header'))
    for path, relative in public_inputs:
        (public / relative).write_text('#include <sdk_version.hpp>\n' + path.read_text())
    (public / 'sdk_version.hpp').write_text(header)
    provenance = {**identity, 'schema_release': authority['release'], 'source_hashes': source_hashes,
                  'generated_inputs': generated_hashes, 'schema_files': schema_files,
                  'source_revision_kind': 'baseline revision; source_hashes identify exact build content',
                  'qualification_scope': 'finite accepted NGAP profile; not exhaustive values or live interoperability'}
    (generated / 'sdk-provenance.json').write_text(json.dumps(provenance, indent=2, sort_keys=True) + '\n')
    generated_hashes.update({name: sha(generated / name) for name in sealed_names
                             if name not in {'sdk-lock.cmake', 'sdk-public'}})
    generated_hashes.update({str(path.relative_to(generated)): sha(path)
                             for path in sorted(public.rglob('*')) if path.is_file()})
    lock = ['# Generated immutable SDK consistency lock. Regenerate rather than editing.']
    for key, value in [('NRFORGE_SDK_VERSION', args.version), ('NRFORGE_SDK_FINGERPRINT', fingerprint),
                       ('NRFORGE_SCHEMA_SHA256', schema_sha), ('NRFORGE_RUNTIME_SHA256', runtime_sha)]:
        lock.append(f'set({key} "{value}")')
    for scope, hashes in [('SOURCE', source_hashes), ('GENERATED', generated_hashes)]:
        lock.append(f'set(NRFORGE_{scope}_HASHES')
        lock.extend(f'  "{name}|{value}"' for name, value in sorted(hashes.items()))
        lock.append(')')
    (generated / 'sdk-lock.cmake').write_text('\n'.join(lock) + '\n')
    print(json.dumps(identity, sort_keys=True))


if __name__ == '__main__':
    main()
