#!/usr/bin/env python3
"""Build a copied consumer using only a relocated SDK installation and system tools."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def run(command, work, environment, log):
    result = subprocess.run(command, cwd=work, env=environment, text=True,
                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    log.write_text(result.stdout)
    return result


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--prefix', type=Path, required=True)
    parser.add_argument('--work', type=Path, required=True)
    parser.add_argument('--cmake', default='cmake')
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    original = args.prefix.resolve()
    work = args.work.resolve()
    work.mkdir(parents=True, exist_ok=False)
    relocated = work / 'relocated-sdk'
    shutil.copytree(original, relocated, symlinks=False)
    source = work / 'consumer'
    source.mkdir()
    for name in ('ng_setup.cpp', 'CMakeLists.txt'):
        shutil.copyfile(Path(__file__).resolve().parent / name, source / name)
    # Ignore inherited CPATH, compiler flags and CMake search paths. Installed
    # package discovery is explicit; package registries are disabled.
    env = {'PATH': os.environ['PATH'], 'HOME': str(work / 'home'), 'LC_ALL': 'C'}
    Path(env['HOME']).mkdir()
    flags = ['-DCMAKE_PREFIX_PATH=' + str(relocated),
             '-DCMAKE_FIND_USE_PACKAGE_REGISTRY=OFF',
             '-DCMAKE_FIND_USE_SYSTEM_PACKAGE_REGISTRY=OFF',
             '-DCMAKE_EXPORT_COMPILE_COMMANDS=ON']
    configure = run([args.cmake, '-S', str(source), '-B', str(work / 'build')] + flags,
                    work, env, work / 'configure.log')
    if configure.returncode:
        raise RuntimeError('installed consumer configure failed')
    build = run([args.cmake, '--build', str(work / 'build'), '--verbose'],
                work, env, work / 'build.log')
    if build.returncode:
        raise RuntimeError('installed consumer build failed')
    execution = run([args.cmake, '--build', str(work / 'build'), '--target', 'test'],
                    work, env, work / 'test.log')
    if execution.returncode:
        raise RuntimeError('installed consumer tests failed')
    binary = work / 'build' / 'ngap_setup_consumer'
    direct = run([str(binary)], work, env, work / 'consumer.output')
    if direct.returncode or direct.stdout.count('PASS NGSetup') != 3:
        raise RuntimeError('three public outcomes not executed')
    # Generated .d files prove every non-system C++ dependency is an installed
    # public header. Source checkout/generated build headers are forbidden.
    dependencies = set()
    for dep in (work / 'build').rglob('*.o.d'):
        text = dep.read_text().replace('\\\n', ' ')
        for token in text.split(':', 1)[1].split():
            path = Path(token).resolve()
            dependencies.add(str(path))
            if path.is_relative_to(source) or path.is_relative_to(relocated):
                continue
            if any(path.is_relative_to(Path(base)) for base in ('/usr', '/lib', '/opt')):
                continue
            raise RuntimeError('non-installed consumer dependency: ' + str(path))
    if not any('/messages/NgSetupRequest.hpp' in p for p in dependencies):
        raise RuntimeError('dependency evidence missing installed message header')
    if any('_codec.hpp' in p or '_mapping.hpp' in p for p in dependencies):
        raise RuntimeError('consumer includes private mapping/codec headers')
    commands = (work / 'build' / 'compile_commands.json').read_text()
    checkout = str(Path(__file__).resolve().parents[2])
    build_log = (work / 'build.log').read_text()
    if checkout in commands or str(original) in commands or checkout in build_log or str(original) in build_log:
        raise RuntimeError('consumer compilation depends on original checkout/prefix')
    mismatch = run([args.cmake, '-S', str(source), '-B', str(work / 'wrong-fingerprint')] +
                   flags + ['-DNRFORGE_NGAP_EXPECTED_FINGERPRINT=wrong-sdk'],
                   work, env, work / 'mismatch.log')
    if mismatch.returncode == 0 or 'fingerprint' not in mismatch.stdout.lower():
        raise RuntimeError('incompatible SDK fingerprint was not rejected')
    # Model headers from a different sealed SDK against this archive. The
    # inline initializer references a fingerprint-specific external symbol;
    # successful compilation must reach a link failure, not merely a syntax error.
    header = relocated / 'include/nrforge/ngap/sdk_version.hpp'
    header_bytes = header.read_bytes()
    original_text = header_bytes.decode()
    tokens = set(re.findall(r'sdk_require_[0-9a-f]{64}', original_text))
    if len(tokens) != 1:
        raise RuntimeError('missing unique SDK header/library link guard')
    original_token = next(iter(tokens))
    bad_token = 'sdk_require_' + ('f' * 64 if original_token != 'sdk_require_' + 'f' * 64 else 'e' * 64)
    direct_projects = []
    for label, include in [('pdu-only', 'pdu.hpp'), ('message-only', 'messages/NgSetupRequest.hpp')]:
        direct_source = work / label
        direct_source.mkdir()
        shutil.copyfile(source / 'CMakeLists.txt', direct_source / 'CMakeLists.txt')
        (direct_source / 'ng_setup.cpp').write_text('#include <' + include + '>\nint main() { return 0; }\n')
        direct_build = work / (label + '-build')
        result = run([args.cmake, '-S', str(direct_source), '-B', str(direct_build)] + flags,
                     work, env, work / (label + '-configure.log'))
        if result.returncode:
            raise RuntimeError('direct public-header configure failed: ' + label)
        result = run([args.cmake, '--build', str(direct_build)], work, env, work / (label + '-build.log'))
        if result.returncode:
            raise RuntimeError('direct public-header build failed: ' + label)
        direct_projects.append((label, direct_build))
    try:
        header.write_text(original_text.replace(original_token, bad_token))
        wrong_configure = run([args.cmake, '-S', str(source), '-B', str(work / 'wrong-library')] + flags,
                              work, env, work / 'wrong-library-configure.log')
        if wrong_configure.returncode:
            raise RuntimeError('header mismatch must reach linker, not configure failure')
        wrong_build = run([args.cmake, '--build', str(work / 'wrong-library'), '--verbose'],
                          work, env, work / 'wrong-library-build.log')
        if wrong_build.returncode == 0 or bad_token not in wrong_build.stdout or 'undefined reference' not in wrong_build.stdout:
            raise RuntimeError('mismatched SDK headers/archive did not fail at link guard')
        for label, direct_build in direct_projects:
            result = run([args.cmake, '--build', str(direct_build)], work, env,
                         work / (label + '-mismatch.log'))
            if result.returncode == 0 or bad_token not in result.stdout or 'undefined reference' not in result.stdout:
                raise RuntimeError('direct public header bypasses SDK link guard: ' + label)
    finally:
        header.write_bytes(header_bytes)
    args.output.write_text(json.dumps({
        'status': 'PASS', 'consumer_outcomes': 3, 'installed_only': True,
        'relocated_prefix': True, 'expected_fingerprint_mismatch_rejected': True,
        'header_library_mismatch_link_rejected': True,
        'direct_public_headers_guarded': ['ngap.hpp', 'pdu.hpp', 'messages/NgSetupRequest.hpp'],
        'consumer_source_sha256': digest(source / 'ng_setup.cpp'),
        'consumer_binary_sha256': digest(binary),
        'output_sha256': digest(work / 'consumer.output'),
        'dependencies': sorted(dependencies),
        'scope': 'Frozen minimal mandatory IE fixtures; no live RAN procedure claim',
    }, indent=2) + '\n')
    print('PASS installed-only relocated NG Setup consumer (3 outcomes)')


if __name__ == '__main__':
    main()
