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
    parser.add_argument('--ngap-prefix', type=Path, required=True)
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
    for name in ('f1_setup.cpp', 'CMakeLists.txt'):
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
    binary = work / 'build' / 'f1ap_setup_consumer'
    direct = run([str(binary)], work, env, work / 'consumer.output')
    if direct.returncode or direct.stdout.count('PASS F1Setup') != 3:
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
    if not any('/messages/F1SetupRequest.hpp' in p for p in dependencies):
        raise RuntimeError('dependency evidence missing installed message header')
    if any('_codec.hpp' in p or '_mapping.hpp' in p for p in dependencies):
        raise RuntimeError('consumer includes private mapping/codec headers')
    commands = (work / 'build' / 'compile_commands.json').read_text()
    checkout = str(Path(__file__).resolve().parents[2])
    build_log = (work / 'build.log').read_text()
    if checkout in commands or str(original) in commands or checkout in build_log or str(original) in build_log:
        raise RuntimeError('consumer compilation depends on original checkout/prefix')
    mismatch = run([args.cmake, '-S', str(source), '-B', str(work / 'wrong-fingerprint')] +
                   flags + ['-DNRFORGE_F1AP_EXPECTED_FINGERPRINT=wrong-sdk'],
                   work, env, work / 'mismatch.log')
    if mismatch.returncode == 0 or 'fingerprint' not in mismatch.stdout.lower():
        raise RuntimeError('incompatible SDK fingerprint was not rejected')
    # Model headers from a different sealed SDK against this archive. The
    # inline initializer references a fingerprint-specific external symbol;
    # successful compilation must reach a link failure, not merely a syntax error.
    header = relocated / 'include/nrforge/f1ap/sdk_version.hpp'
    header_bytes = header.read_bytes()
    original_text = header_bytes.decode()
    tokens = set(re.findall(r'sdk_require_[0-9a-f]{64}', original_text))
    if len(tokens) != 1:
        raise RuntimeError('missing unique SDK header/library link guard')
    original_token = next(iter(tokens))
    bad_token = 'sdk_require_' + ('f' * 64 if original_token != 'sdk_require_' + 'f' * 64 else 'e' * 64)
    direct_projects = []
    for label, include in [('pdu-only', 'nrforge/f1ap/f1ap_pdu.hpp'), ('message-only', 'nrforge/f1ap/messages/F1SetupRequest.hpp')]:
        direct_source = work / label
        direct_source.mkdir()
        shutil.copyfile(source / 'CMakeLists.txt', direct_source / 'CMakeLists.txt')
        (direct_source / 'f1_setup.cpp').write_text('#include <' + include + '>\nint main() { return 0; }\n')
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
    if result.returncode or 'PASS all 158 installed slots' not in result.stdout: raise RuntimeError('all-slot run failed')
    ngap = args.ngap_prefix.resolve()
    # Install both independent trees into one relocated prefix. Reject any
    # colliding file identity rather than silently overwriting another protocol.
    for path in ngap.rglob('*'):
        if path.is_file():
            destination = relocated / path.relative_to(ngap)
            if destination.exists(): raise RuntimeError('protocol installation collision: '+str(destination))
    shutil.copytree(ngap, relocated, dirs_exist_ok=True, copy_function=copy_sdk_file)
    for order in ('f1ap-first', 'ngap-first'):
        project = work/order
        project.mkdir()
        protocols = ['f1ap','ngap'] if order == 'f1ap-first' else ['ngap','f1ap']
        includes = ''.join('#include <nrforge/'+p+'/'+p+'.hpp>\n' for p in protocols)
        f1_example = (source/'f1_setup.cpp').read_text().replace('int main()', 'void check_f1_examples()')
        ng_example = (Path(__file__).resolve().parents[1]/'ngap-sdk-consumer/ng_setup.cpp').read_text().replace('int main()', 'void check_ng_examples()')
        # Qualify legacy NGAP entry points too for unambiguous dual-SDK use.
        ng_example = ng_example.replace('#include <ngap.hpp>', '#include <nrforge/ngap/ngap.hpp>').replace('#include <messages/', '#include <nrforge/ngap/messages/')
        (project/'f1_setup.cpp').write_text(f1_example)
        (project/'ng_setup.cpp').write_text(ng_example)
        (project/'main.cpp').write_text(includes + '''void check_f1_examples();
void check_ng_examples();
int main() {
check_f1_examples(); check_ng_examples();
const auto& f = nrforge::f1ap::f1ap_registry_state();
const auto& n = nrforge::ngap::ngap_registry_state();
return (!f || !n || f.value().message_count()!=158 || n.value().message_count()!=131 ||
nrforge::f1ap::sdk_identity().fingerprint == nrforge::ngap::sdk_identity().fingerprint) ? 1 : 0;
}\n''')
        (project/'CMakeLists.txt').write_text('cmake_minimum_required(VERSION 3.20)\nproject(Coexist LANGUAGES CXX)\nfind_package(NRForgeNGAP 0.1.0 EXACT CONFIG REQUIRED)\nfind_package(NRForgeF1AP 0.1.0 EXACT CONFIG REQUIRED)\nadd_executable(coexist main.cpp f1_setup.cpp ng_setup.cpp)\ntarget_link_libraries(coexist PRIVATE ' + ' '.join('NRForge::'+p for p in protocols) + ')\ntarget_link_options(coexist PRIVATE -fuse-ld=${NRFORGE_CONSUMER_LINKER} -Wl,-s)\n')
        coexist_flags = [f'-DCMAKE_PREFIX_PATH={relocated}', '-DCMAKE_EXPORT_COMPILE_COMMANDS=ON', '-DCMAKE_FIND_USE_PACKAGE_REGISTRY=OFF', '-DCMAKE_FIND_USE_SYSTEM_PACKAGE_REGISTRY=OFF', '-DNRFORGE_CONSUMER_LINKER='+args.linker]
        for label, command in [('configure',[args.cmake,'-S',str(project),'-B',str(project/'build')]+coexist_flags), ('build',[args.cmake,'--build',str(project/'build'),'--verbose']), ('run',[str(project/'build/coexist')])]:
            result = run(command,work,env,work/(order+'-'+label+'.log'))
            if result.returncode: raise RuntimeError('coexist '+order+' '+label+' failed')
    # A genuine wrong-protocol archive must fail even for a no-op program.
    wrong_source = work/'wrong-protocol.cpp'
    wrong_source.write_text('#include <nrforge/f1ap/f1ap.hpp>\nint main() { return 0; }\n')
    ng_archives = list(relocated.rglob('libnrforge_ngap.a'))
    if len(ng_archives) != 1: raise RuntimeError('missing NGAP archive')
    wrong = run(['g++','-std=c++20','-I'+str(relocated/'include'),str(wrong_source),str(ng_archives[0]),'-o',str(work/'wrong-protocol')],work,env,work/'wrong-protocol.log')
    if wrong.returncode == 0 or original_token not in wrong.stdout or not any(text in wrong.stdout for text in ('undefined reference', 'undefined symbol')):
        raise RuntimeError('wrong protocol archive accepted or did not fail at identity guard')
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
        if str(original) in contents or checkout in contents or str(args.ngap_prefix.resolve()) in contents:
            raise RuntimeError('original SDK/checkout leaked into compilation commands')
    for config in relocated.rglob('*.cmake'):
        contents = config.read_text()
        if str(original) in contents or checkout in contents or str(args.ngap_prefix.resolve()) in contents:
            raise RuntimeError('nonrelocatable exported configuration')
    actual_archive = list(relocated.rglob('libnrforge_f1ap.a'))
    if len(actual_archive) != 1: raise RuntimeError('missing F1AP installed archive')
    installed_identity = json.loads((relocated/'share/nrforge-f1ap/sdk-provenance.json').read_text())
    args.output.write_text(json.dumps({
        'sdk_fingerprint': installed_identity['fingerprint'],
        'archive_sha256': digest(actual_archive[0]),
        'all_installed_slots': 158,
        'same_prefix_coinstallation': True,
        'wrong_protocol_archive_link_rejected': True,
        'coexist_native_outcomes_per_order': {'F1Setup': 3, 'NGSetup': 3},
        'coexist_orders': ['f1ap-first', 'ngap-first'],
        'status': 'PASS', 'consumer_outcomes': 3, 'installed_only': True,
        'relocated_prefix': True, 'expected_fingerprint_mismatch_rejected': True,
        'header_library_mismatch_link_rejected': True,
        'direct_public_headers_guarded': ['f1ap.hpp', 'f1ap_pdu.hpp', 'messages/F1SetupRequest.hpp'],
        'consumer_source_sha256': digest(source / 'f1_setup.cpp'),
        'consumer_binary_sha256': digest(binary),
        'output_sha256': digest(work / 'consumer.output'),
        'dependencies': sorted(dependencies),
        'scope': 'Frozen minimal mandatory IE fixtures; no live RAN procedure claim',
    }, indent=2) + '\n')
    print('PASS installed-only relocated F1AP consumer, all slots and NGAP coexistence')


if __name__ == '__main__':
    main()
