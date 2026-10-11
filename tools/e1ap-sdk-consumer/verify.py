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


def copy_sdk_file(source, destination):
    # Archives are immutable during consumer checks. Relocation may preserve
    # their inode; headers are copied independently for identity-negative edits.
    if Path(source).suffix == '.a':
        try:
            os.link(source, destination)
            return destination
        except OSError:
            pass
    return shutil.copy2(source, destination)


def digest(path):
    result = hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda: stream.read(1 << 20), b''): result.update(block)
    return result.hexdigest()


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
    parser.add_argument('--all-slots', type=Path, required=True)
    parser.add_argument('--wrong-archives', type=Path, required=True)
    parser.add_argument('--other-public', type=Path, required=True)
    parser.add_argument('--jobs', type=int, default=1)
    parser.add_argument('--linker', default='gold')
    args = parser.parse_args()
    if args.jobs < 1: raise ValueError('jobs must be positive')
    original = args.prefix.resolve()
    work = args.work.resolve()
    work.mkdir(parents=True, exist_ok=False)
    relocated = work / 'relocated-sdk'
    shutil.copytree(original, relocated, symlinks=False, copy_function=copy_sdk_file)
    source = work / 'consumer'
    source.mkdir()
    for name in ('e1_setup.cpp', 'CMakeLists.txt'):
        shutil.copyfile(Path(__file__).resolve().parent / name, source / name)
    # Ignore inherited CPATH, compiler flags and CMake search paths. Installed
    # package discovery is explicit; package registries are disabled.
    env = {'PATH': os.environ['PATH'], 'HOME': str(work / 'home'), 'LC_ALL': 'C'}
    Path(env['HOME']).mkdir()
    flags = ['-DCMAKE_PREFIX_PATH=' + str(relocated),
             '-DCMAKE_FIND_USE_PACKAGE_REGISTRY=OFF',
             '-DCMAKE_FIND_USE_SYSTEM_PACKAGE_REGISTRY=OFF',
             '-DCMAKE_EXPORT_COMPILE_COMMANDS=ON', '-DNRFORGE_CONSUMER_LINKER='+args.linker, '-DCMAKE_EXE_LINKER_FLAGS=-Wl,-s']
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
    binary = work / 'build' / 'e1ap_setup_consumer'
    direct = run([str(binary)], work, env, work / 'consumer.output')
    if direct.returncode or direct.stdout.count('PASS GNB-CU-') != 6:
        raise RuntimeError('six public outcomes not executed')
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
    if not any('/messages/GnbCuUpE1SetupRequest.hpp' in p for p in dependencies):
        raise RuntimeError('dependency evidence missing installed message header')
    if any('_codec.hpp' in p or '_mapping.hpp' in p for p in dependencies):
        raise RuntimeError('consumer includes private mapping/codec headers')
    commands = (work / 'build' / 'compile_commands.json').read_text()
    checkout = str(Path(__file__).resolve().parents[2])
    build_log = (work / 'build.log').read_text()
    if checkout in commands or str(original) in commands or checkout in build_log or str(original) in build_log:
        raise RuntimeError('consumer compilation depends on original checkout/prefix')
    mismatch = run([args.cmake, '-S', str(source), '-B', str(work / 'wrong-fingerprint')] +
                   flags + ['-DNRFORGE_E1AP_EXPECTED_FINGERPRINT=wrong-sdk'],
                   work, env, work / 'mismatch.log')
    if mismatch.returncode == 0 or 'fingerprint' not in mismatch.stdout.lower():
        raise RuntimeError('incompatible SDK fingerprint was not rejected')
    # Model headers from a different sealed SDK against this archive. The
    # inline initializer references a fingerprint-specific external symbol;
    # successful compilation must reach a link failure, not merely a syntax error.
    header = relocated / 'include/nrforge/e1ap/sdk_version.hpp'
    header_bytes = header.read_bytes()
    original_text = header_bytes.decode()
    tokens = set(re.findall(r'sdk_require_[0-9a-f]{64}', original_text))
    if len(tokens) != 1:
        raise RuntimeError('missing unique SDK header/library link guard')
    original_token = next(iter(tokens))
    bad_token = 'sdk_require_' + ('f' * 64 if original_token != 'sdk_require_' + 'f' * 64 else 'e' * 64)
    direct_projects = []
    for label, include in [('pdu-only', 'nrforge/e1ap/e1ap_pdu.hpp'), ('message-only', 'nrforge/e1ap/messages/GnbCuUpE1SetupRequest.hpp')]:
        direct_source = work / label
        direct_source.mkdir()
        shutil.copyfile(source / 'CMakeLists.txt', direct_source / 'CMakeLists.txt')
        (direct_source / 'e1_setup.cpp').write_text('#include <' + include + '>\nint main() { return 0; }\n')
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
        if wrong_build.returncode == 0 or bad_token not in wrong_build.stdout or not any(text in wrong_build.stdout for text in ('undefined reference', 'undefined symbol')):
            raise RuntimeError('mismatched SDK headers/archive did not fail at link guard')
        for label, direct_build in direct_projects:
            result = run([args.cmake, '--build', str(direct_build)], work, env,
                         work / (label + '-mismatch.log'))
            if result.returncode == 0 or bad_token not in result.stdout or not any(text in result.stdout for text in ('undefined reference', 'undefined symbol')):
                raise RuntimeError('direct public header bypasses SDK link guard: ' + label)
    finally:
        header.write_bytes(header_bytes)
    all_source = work / 'all-slot-source'
    shutil.copytree(args.all_slots, all_source)
    result = run([args.cmake, '-S', str(all_source), '-B', str(work/'all-slot-build')] + flags, work, env, work/'all-slot-configure.log')
    if result.returncode: raise RuntimeError('all-slot configure failed')
    result = run([args.cmake, '--build', str(work/'all-slot-build'), '--parallel', str(args.jobs)], work, env, work/'all-slot-build.log')
    if result.returncode: raise RuntimeError('all-slot build failed')
    result = run([str(work/'all-slot-build/all_slots')], work, env, work/'all-slot-run.log')
    if result.returncode or 'PASS all 72 installed slots' not in result.stdout: raise RuntimeError('all-slot run failed')
    # Distinct real protocol core archives exercise coexistence, without
    # claiming to rebuild the historical NGAP/F1AP generated SDK registries.
    for profile in ('ngap','f1ap'):
        archive = args.wrong_archives/(profile+'.a')
        wrong_source = work/('wrong-'+profile+'.cpp')
        wrong_source.write_text('#include <nrforge/e1ap/e1ap.hpp>\nint main() { return 0; }\n')
        result = run(['g++','-std=c++20','-I'+str(relocated/'include'),str(wrong_source),str(archive),'-o',str(work/('wrong-'+profile))],work,env,work/('wrong-'+profile+'.log'))
        if result.returncode == 0 or original_token not in result.stdout or not any(x in result.stdout for x in ('undefined reference','undefined symbol')):
            raise RuntimeError('genuine wrong-protocol archive accepted: '+profile)
    # Put copied development public core headers in the same relocated prefix.
    for path in args.other_public.rglob('*'):
        if path.is_file() and (relocated/'include'/path.relative_to(args.other_public)).exists():
            raise RuntimeError('protocol public header collision')
    shutil.copytree(args.other_public,relocated/'include',dirs_exist_ok=True)
    installed_archive = next(relocated.rglob('libnrforge_e1ap.a'))
    for order in ('e1ap-first','e1ap-last'):
        profiles = ['e1ap','ngap','f1ap'] if order == 'e1ap-first' else ['f1ap','ngap','e1ap']
        headers = {'e1ap':'e1ap_pdu.hpp','ngap':'pdu.hpp','f1ap':'f1ap_pdu.hpp'}
        project = work/order; project.mkdir()
        src = ''.join('#include <nrforge/'+profile+'/'+headers[profile]+'>\n' for profile in profiles)
        src += '// Explicit rejected empty-registry fixtures for development cores.\n'
        for profile in ('ngap','f1ap'):
            src += 'namespace nrforge::'+profile+' { const aper::Result<Registry>& '+profile+'_registry_state() noexcept { static const auto state = Registry::create({0,1,2},true,{},{}); return state; } }\n'
        src += 'int main() { const auto& e = nrforge::e1ap::e1ap_registry_state(); const auto& n = nrforge::ngap::ngap_registry_state(); const auto& f = nrforge::f1ap::f1ap_registry_state(); if (!e || e.value().message_count()!=72 || n || f || n.error().code!=nrforge::aper::ErrorCode::invalid_argument || f.error().code!=nrforge::aper::ErrorCode::invalid_argument) return 1; auto nd = nrforge::ngap::decode_ngap_pdu({}); auto fd = nrforge::f1ap::decode_f1ap_pdu({}); return nd || fd || nd.error().code!=nrforge::aper::ErrorCode::invalid_argument || fd.error().code!=nrforge::aper::ErrorCode::invalid_argument; }\n'
        (project/'main.cpp').write_text(src)
        libraries = {'e1ap':str(installed_archive),'ngap':str(args.wrong_archives/'ngap.a'),'f1ap':str(args.wrong_archives/'f1ap.a')}
        result = run(['g++','-std=c++20','-Wall','-Wextra','-Werror','-pedantic-errors','-I'+str(relocated/'include'),'-MMD','-MF',str(project/'coexist.o.d'),str(project/'main.cpp'),*[libraries[p] for p in profiles],'-fuse-ld='+args.linker,'-Wl,-s','-o',str(project/'coexist')],work,env,work/(order+'-build.log'))
        if result.returncode: raise RuntimeError('protocol core coexist compile/link failure')
        result = run([str(project/'coexist')],work,env,work/(order+'-run.log'))
        if result.returncode: raise RuntimeError('protocol core coexist run failure')
    # Audit every successful compilation, including all-slot and coexistence
    # sources, rather than only the small Setup example.
    for dep in work.rglob('*.o.d'):
        for token in dep.read_text().replace('\\\n', ' ').split(':',1)[1].split():
            path = Path(token).resolve()
            dependencies.add(str(path))
            if path.name.endswith(('_codec.hpp', '_mapping.hpp')):
                raise RuntimeError('private header in consumer dependencies: '+str(path))
            if path.is_relative_to(work) or any(path.is_relative_to(Path(base)) for base in ('/usr','/lib','/opt')):
                continue
            raise RuntimeError('consumer dependency escapes isolated workspace: '+str(path))
    for command_file in work.rglob('compile_commands.json'):
        contents = command_file.read_text()
        if str(original) in contents or checkout in contents:
            raise RuntimeError('original SDK/checkout leaked into compilation commands')
    for config in relocated.rglob('*.cmake'):
        contents = config.read_text()
        if str(original) in contents or checkout in contents:
            raise RuntimeError('nonrelocatable exported configuration')
    actual_archive = list(relocated.rglob('libnrforge_e1ap.a'))
    if len(actual_archive) != 1: raise RuntimeError('missing E1AP installed archive')
    installed_identity = json.loads((relocated/'share/nrforge-e1ap/sdk-provenance.json').read_text())
    args.output.write_text(json.dumps({
        'sdk_fingerprint': installed_identity['fingerprint'],
        'archive_sha256': digest(actual_archive[0]),
        'all_installed_slots': 72,
        'wrong_protocol_core_archive_rejected': ['ngap','f1ap'],
        'protocol_core_coexist_orders': ['e1ap-first','e1ap-last'],
        'protocol_core_coexist_scope': 'E1 complete installed registry plus real NGAP/F1AP cores with explicit rejected empty-registry fixtures; not complete historical SDK registries',
        'status': 'PASS', 'consumer_outcomes': 6, 'installed_only': True,
        'relocated_prefix': True, 'expected_fingerprint_mismatch_rejected': True,
        'header_library_mismatch_link_rejected': True,
        'direct_public_headers_guarded': ['e1ap.hpp', 'e1ap_pdu.hpp', 'messages/GnbCuUpE1SetupRequest.hpp'],
        'consumer_source_sha256': digest(source / 'e1_setup.cpp'),
        'consumer_binary_sha256': digest(binary),
        'output_sha256': digest(work / 'consumer.output'),
        'dependencies': sorted(dependencies),
        'scope': 'Frozen minimal mandatory IE fixtures; no live RAN procedure claim',
    }, indent=2) + '\n')
    print('PASS installed-only relocated E1AP consumer, all slots and fingerprint negatives')


if __name__ == '__main__':
    main()
