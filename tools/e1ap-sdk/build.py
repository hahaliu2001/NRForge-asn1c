#!/usr/bin/env python3
"""Generate, seal, build and install the complete pinned E1AP source SDK."""
import argparse
import base64
import gzip
import hashlib
import json
import pathlib
import subprocess


def digest(data):
    return hashlib.sha256(data).hexdigest()


def run(command, log, cwd=None):
    with log.open('w') as stream:
        subprocess.run([str(x) for x in command], cwd=cwd, stdout=stream,
                       stderr=subprocess.STDOUT, check=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--repo', type=pathlib.Path, required=True)
    parser.add_argument('--asn1-root', type=pathlib.Path, required=True)
    parser.add_argument('--work', type=pathlib.Path, required=True)
    parser.add_argument('--prefix', type=pathlib.Path, required=True)
    parser.add_argument('--cmake', default='cmake')
    parser.add_argument('--jobs', type=int, default=1)
    args = parser.parse_args()
    if args.jobs < 1:
        raise ValueError('jobs must be positive')
    repo, root = args.repo.resolve(), args.asn1_root.resolve()
    work, prefix = args.work.resolve(), args.prefix.resolve()
    if prefix.exists() and any(prefix.iterdir()):
        raise ValueError('installation prefix must be new or empty')
    work.mkdir(parents=True, exist_ok=False)
    source = json.loads((repo / 'tools/e1ap-readiness/source-manifest.json').read_text())
    for item in source['modules']:
        path = (root / item['path']).resolve()
        if not path.is_relative_to(root):
            raise ValueError('schema path escapes root')
        data = path.read_bytes()
        blob = hashlib.sha1(b'blob ' + str(len(data)).encode() + b'\0' + data).hexdigest()
        if digest(data) != item['sha256'] or blob != item['git_blob']:
            raise ValueError('frozen schema mismatch: ' + item['path'])
    historical_path = repo / 'tools/e1ap-wire-qualification/accepted-profile.json'
    historical = historical_path.read_bytes()
    envelope = json.loads(historical)
    expanded = gzip.decompress(base64.b64decode(envelope["data"], validate=True))
    if digest(expanded) != envelope["profile_sha256"]:
        raise ValueError("historical profile transport hash mismatch")
    golden = json.loads(expanded)
    if golden['source_authority'] != source:
        raise ValueError('historical source authority mismatch')
    modules, messages = work / 'modules.txt', work / 'messages.txt'
    modules.write_text(''.join(x['path'] + '\n' for x in source['modules']))
    messages.write_text(''.join(x['message'] + '\n' for x in golden['identities']))
    generated, build = work / 'generated', work / 'build'
    generated.mkdir()
    run([repo / 'tools/asn1typed_ngap_dispatch', modules, root, messages, generated, "--e1ap"],
        work / 'generation.log')
    manifest = json.loads((generated / 'manifest.json').read_text())
    if manifest['message_count'] != 72 or len(manifest['schema']['procedures']) != 40:
        raise ValueError('frozen SDK registry closure mismatch')
    unified = golden
    unchanged = {name: expected for name, expected in unified['generated_sha256'].items()
                 if name not in {'manifest.json', 'CMakeLists.txt'}}
    actual_outputs = {str(p.relative_to(generated)) for p in generated.rglob('*')
                      if p.is_file() and p.name not in {'manifest.json', 'CMakeLists.txt'}}
    if actual_outputs != set(unchanged):
        raise ValueError('qualified generated production file closure changed')
    for name, expected in unchanged.items():
        path = (generated / name).resolve()
        if not path.is_relative_to(generated) or digest(path.read_bytes()) != expected:
            raise ValueError('qualified production output changed: ' + name)
    run(['python3', repo / 'tools/ngap-dispatch/seal_sdk.py', '--repo', repo,
         '--generated', generated, '--version', '0.1.0', '--profile', 'e1ap'], work / 'seal.log')
    run([args.cmake, '-S', generated, '-B', build,
         '-DNRFORGE_SOURCE_ROOT=' + str(repo), '-DCMAKE_INSTALL_PREFIX=' + str(prefix),
         '-DCMAKE_BUILD_TYPE=',
         '-DCMAKE_EXE_LINKER_FLAGS=-fuse-ld=gold -Wl,--no-keep-memory',
         '-DCMAKE_EXPORT_COMPILE_COMMANDS=ON',
         '-DCMAKE_CXX_FLAGS=-O0 -Wall -Wextra -Werror -pedantic-errors -Wconversion -Wsign-conversion'],
        work / 'configure.log')
    run([args.cmake, '--build', build, '--parallel', args.jobs], work / 'build.log')
    run([args.cmake, '--install', build], work / 'install.log')
    if historical_path.read_bytes() != historical:
        raise ValueError('historical accepted profile changed during build')
    record = {
        'status': 'BUILT_AND_INSTALLED_CONSUMER_VERIFICATION_PENDING',
        'source_authority': source,
        'historical_profile_sha256': digest(historical),
        'registry': {'message_count': 72, 'procedure_count': 40},
        'qualified_production_outputs_unchanged': len(unchanged),
        'sdk_provenance': json.loads((generated / 'sdk-provenance.json').read_text()),
        'installed_files': {str(p.relative_to(prefix)): digest(p.read_bytes())
                            for p in sorted(prefix.rglob('*')) if p.is_file()},
        'compile_commands_sha256': digest((build / 'compile_commands.json').read_bytes()),
    }
    (work / 'build-summary.json').write_text(json.dumps(record, indent=2) + '\n')
    print('PASS complete SDK build/install; consumer verification still required')


if __name__ == '__main__':
    main()
