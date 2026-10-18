#!/usr/bin/env python3
"""ASN1-owned serial producer. Produces candidates; never publishes/deletes."""
import argparse
import hashlib
import json
import os
import re
from pathlib import Path
import shutil
import subprocess
import sys

REPO=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(REPO/'tools/m6003-transfer-qualification'))
sys.path.insert(0,str(REPO/'tools/ngap-transfer-sdk'))
from resource_guard import verify
from snapshot import extract_source
from support import command, sha, write
VERSION='0.3.0'


def git(*args):return subprocess.check_output(['git','-C',str(REPO),*args])


def merge(source,target):
    for path in sorted(source.rglob('*')):
        if path.is_symlink(): raise ValueError('Symlinks are not allowed in installed SDK bundle')
        if path.is_file():
            dest=target/path.relative_to(source)
            dest.parent.mkdir(parents=True,exist_ok=True)
            if dest.exists() and dest.read_bytes()!=path.read_bytes():raise ValueError('SDK install collision: '+str(dest))
            shutil.copyfile(path,dest)


def consumer(work, source, prefix, python, name):
    build=work/(name+'-build')
    command(work/(name+'-configure'),['cmake','-S',source,'-B',build,'-DCMAKE_PREFIX_PATH='+str(prefix),
        '-DCMAKE_CXX_FLAGS=-O0 -g0 --param=ggc-min-expand=5 --param=ggc-min-heapsize=16384',
        '-DCMAKE_EXE_LINKER_FLAGS=-fuse-ld=lld -Wl,--threads=1','-DCMAKE_EXPORT_COMPILE_COMMANDS=ON'])
    command(work/(name+'-compile'),['cmake','--build',build,'--parallel','1'],timeout=43200,live=True)
    dependencies=list(build.rglob('*.o.d'))
    if not dependencies:raise ValueError('No compiler dependency evidence for '+name)
    forbidden=[str(work/'source')+'/',str(work/'schema')+'/',str(work/'sdk')+'/']+[str(work/(p+'-prefix'))+'/' for p in ('ngap','f1ap','e1ap','rrc')]
    for dep in dependencies:
        if any(path in dep.read_text() for path in forbidden):raise ValueError('Consumer depends on original build/source/schema: '+str(dep))
    write(work/(name+'-dependency-audit.json'),{str(dep.relative_to(build)):sha(dep) for dep in dependencies})
    command(work/(name+'-execute'),['ctest','--test-dir',build,'-j1','--output-on-failure'],timeout=21600,live=True)


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--schema-root',type=Path,required=True)
    p.add_argument('--work',type=Path,required=True)
    a=p.parse_args()
    resource=verify()
    if sys.platform!='linux' or sys.version_info[:2]!=(3,12):raise RuntimeError('Native Linux CPython 3.12 required')
    if git('status','--porcelain','--untracked-files=no').strip():raise ValueError('Commit producer changes before execution')
    head=git('rev-parse','HEAD').decode().strip()
    out=a.work.expanduser().resolve();out.mkdir(parents=True,exist_ok=False)
    report=dict(status='RUNNING',version=VERSION,source_revision=head,resources=resource,publication='NOT_RUN',owner_acceptance='PENDING')
    write(out/'producer.json',report)
    try:
        source=out/'source'
        archive=git('archive',head,'--','.',':(exclude)verification',':(exclude)_codeql_detected_source_root',':(exclude).vscode')
        write(out/'source-links.json',extract_source(archive,source))
        # Import all schemas as frozen external inputs. No RAN/5GC code, checkout
        # mutation, subprocess or publication tool participates in production.
        schema=out/'schema';schema.mkdir()
        authorities={}
        for profile,rel in [('ngap','n11-envelope-qualification'),('f1ap','f1ap-readiness'),('e1ap','e1ap-readiness'),('rrc','rrc-readiness')]:
            authority=json.loads((source/'tools'/rel/'source-manifest.json').read_text())
            authorities[profile]=authority
            for row in authority['modules']:
                origin=(a.schema_root/row['path']).resolve()
                if not origin.is_relative_to(a.schema_root.resolve()):raise ValueError('Escaped frozen schema input')
                data=origin.read_bytes()
                blob=hashlib.sha1(b'blob '+str(len(data)).encode()+b'\0'+data).hexdigest()
                if hashlib.sha256(data).hexdigest()!=row['sha256'] or blob!=row['git_blob']:raise ValueError('Frozen schema identity mismatch: '+row['path'])
                dest=schema/row['path'];dest.parent.mkdir(parents=True,exist_ok=True);dest.write_bytes(data)
        write(out/'schema-authorities.json',authorities)
        e1_containing=sorted({name for row in authorities['e1ap']['modules'] for name in re.findall(r'CONTAINING\s+([\w-]+)',re.sub(r'--[^\n]*','',(schema/row['path']).read_text()))})
        write(out/'e1ap-containing-audit.json',dict(scope='explicit CONTAINING targets in all six frozen E1AP modules',targets=e1_containing,full_protocol_coverage=False))
        if e1_containing:raise ValueError('E1AP containing roots require explicit scope reconciliation')
        os.environ.update(CC='gcc-13',CXX='g++-13',CFLAGS='-O0 -g0',CMAKE_BUILD_PARALLEL_LEVEL='1',MAKEFLAGS='-j1',
            CXXFLAGS='-O0 -g0 --param=ggc-min-expand=5 --param=ggc-min-heapsize=16384',LDFLAGS='-fuse-ld=lld -Wl,--threads=1',PYTHONDONTWRITEBYTECODE='1')
        command(out/'01-autoreconf',['autoreconf','-iv'],cwd=source)
        command(out/'02-configure',[source/'configure'],cwd=source)
        for library in ('libasn1common','libasn1parser','libasn1fix','libasn1typed'):
            command(out/('03-'+library),['make','-j1','-C',source/library],cwd=source,timeout=21600,live=True)
        command(out/'04-generators',['make','-j1','-C',source/'tools','asn1typed_ioc_probe','asn1typed_ngap_dispatch'],cwd=source,timeout=21600)
        prefixes={}
        for profile in ('ngap','f1ap','e1ap','rrc'):
            prefix=out/(profile+'-prefix');prefixes[profile]=prefix
            script=source/('tools/unified-sdk/build_rrc.py' if profile=='rrc' else 'tools/'+profile+'-sdk/build.py')
            argv=[sys.executable,script,'--asn1-root',schema,'--work',out/(profile+'-build'),'--prefix',prefix,'--jobs','1']
            if profile!='rrc':argv+=['--repo',source]
            argv+=['--source-revision',head]
            command(out/('05-'+profile),argv,timeout=43200,live=True)
        # A single installation and umbrella CMake package; component archives
        # are implementation details of this one versioned four-protocol SDK.
        prefix=out/'sdk';prefix.mkdir()
        for value in prefixes.values():merge(value,prefix)
        configs=prefix/'lib/cmake/NRForgeASN1';configs.mkdir(parents=True)
        config=['include(CMakeFindDependencyMacro)']
        for name in ('NGAP','F1AP','E1AP','RRC'):
            config+=['find_dependency(NRForge'+name+' '+VERSION+' EXACT CONFIG PATHS "${CMAKE_CURRENT_LIST_DIR}/../NRForge'+name+'" NO_DEFAULT_PATH)']
        config+=['if(NOT TARGET NRForge::asn1)','  add_library(NRForge::asn1 INTERFACE IMPORTED)',
                 '  set_target_properties(NRForge::asn1 PROPERTIES INTERFACE_LINK_LIBRARIES "NRForge::ngap;NRForge::f1ap;NRForge::e1ap;NRForge::rrc")','endif()']
        (configs/'NRForgeASN1Config.cmake').write_text('\n'.join(config)+'\n')
        (configs/'NRForgeASN1ConfigVersion.cmake').write_text('set(PACKAGE_VERSION "'+VERSION+'")\nif(PACKAGE_FIND_VERSION VERSION_EQUAL PACKAGE_VERSION)\nset(PACKAGE_VERSION_EXACT TRUE)\nset(PACKAGE_VERSION_COMPATIBLE TRUE)\nendif()\n')
        identity={p:json.loads((prefix/('share/nrforge-'+p+'/sdk-provenance.json')).read_text())['version'] for p in prefixes}
        if set(identity.values())!={VERSION}:raise ValueError('Mixed component versions in unified SDK')
        shutil.copyfile(source/'LICENSE',prefix/'LICENSE')
        write(prefix/'nrforge-asn1-bundle.json',dict(version=VERSION,protocols=list(prefixes),source_revision=head,scope=json.loads((source/'tools/unified-sdk/scope.json').read_text()),component_versions=identity,qualification='PENDING'))
        env=out/'tool-venv';command(out/'06-venv',[sys.executable,'-m','venv',env]);python=env/'bin/python'
        command(out/'07-build-tools',[python,'-I','-m','pip','install','build==1.3.0','scikit-build-core==0.11.6','pybind11==3.0.1'])
        command(out/'08-oracle-tools',[python,'-I','-m','pip','install','--no-deps','--require-hashes','-r',source/'tools/m6003-transfer-qualification/oracle-requirements.lock'])
        for profile in ('f1ap','e1ap'):
            command(out/('08-'+profile+'-outer'),[python,source/('tools/'+profile+'-readiness/check_dispatch.py'),
                '--repo',source,'--generated',out/(profile+'-build/generated'),'--build',out/(profile+'-build/build'),
                '--work',out/(profile+'-outer-work'),'--output',out/(profile+'-outer.json'),'--jobs','1','--linker','lld'],timeout=43200,live=True)
        # Relocate installed inputs before any public-consumer compilation.
        relocated=out/'relocated-sdk';shutil.copytree(prefix,relocated)
        command(out/'09-ngap-vectors',[python,source/'tools/ngap-transfer-sdk/make_vectors.py','--repo',source,'--asn1-root',schema,'--prefix',relocated,'--output',out/'vectors.json'],timeout=7200)
        consumer(out,out/'ngap-consumers',relocated,python,'ngap-installed')
        command(out/'10-rrc-vectors',[python,source/'tools/unified-sdk/prepare_rrc_consumers.py','--schema-root',schema,'--prefix',relocated,'--output',out/'rrc-vectors'],timeout=7200)
        consumer(out,out/'rrc-vectors/consumers',relocated,python,'rrc-installed')
        # Compile the shared runtime and concrete F1AP fourth-root regression
        # through the existing Makefile gate in Owner's bounded terminal.
        command(out/'10-f1-oracle',[python,source/'tools/unified-sdk/check_f1_oracle.py','--schema-root',schema,'--output',out/'f1-oracle'],timeout=7200)
        command(out/'11-aper-core',['make','-j1','-C',source/'libaper'],timeout=7200)
        command(out/'11-f1ap-core',['make','-j1','-C',source/'libngap','check_f1ap_pdu'],timeout=7200)
        command(out/'12-f1ap-extension',[source/'libngap/check_f1ap_pdu'],timeout=900)
        # Frozen outer-message regression and fresh wheel consumer are separate
        # from the already accepted 250-check native campaign (not rerun).
        dist=out/'dist'
        command(out/'13-wheel',[python,'-I','-m','build','--wheel','--no-isolation',source/'python','--outdir',dist,
            '-Ccmake.define.NRFORGE_PYTHON_SDK_PREFIX='+str(relocated),'-Ccmake.define.CMAKE_EXE_LINKER_FLAGS=-fuse-ld=lld -Wl,--threads=1'],timeout=43200,live=True)
        wheels=list(dist.glob('*.whl'))
        if len(wheels)!=1 or wheels[0].name!='nrforge_ngap-'+VERSION+'-cp312-cp312-linux_x86_64.whl':raise ValueError('Unexpected wheel identity')
        wheel=wheels[0];installed=out/'consumer-venv';command(out/'14-consumer-venv',[sys.executable,'-m','venv',installed])
        req=out/'wheel-requirements.txt';req.write_text(wheel.as_uri()+' --hash=sha256:'+sha(wheel)+'\n')
        cp=installed/'bin/python';command(out/'15-install-wheel',[cp,'-I','-m','pip','install','--no-index','--no-deps','--require-hashes','-r',req])
        command(out/'16-outer-regression',[python,source/'python/tests/run_installed.py','--python',cp,'--expected-version',VERSION,'--output',out/'outer-report.json'],timeout=3600)
        command(out/'17-root-binding-regression',[cp,'-I',source/'tools/ngap-transfer-sdk/installed_check.py',out/'vectors.json',relocated/'share/nrforge-ngap/sdk-provenance.json',out/'transfer-report.json'],timeout=3600)
        # Four-profile coexistence must link every public target together.
        coexist=out/'coexist';coexist.mkdir()
        (coexist/'CMakeLists.txt').write_text('cmake_minimum_required(VERSION 3.20)\nproject(Coexist LANGUAGES CXX)\nfind_package(NRForgeASN1 '+VERSION+' EXACT CONFIG REQUIRED)\nadd_executable(coexist main.cpp)\ntarget_link_libraries(coexist PRIVATE NRForge::asn1)\nenable_testing()\nadd_test(NAME coexist COMMAND coexist)\n')
        (coexist/'main.cpp').write_text('#include <nrforge/ngap/ngap.hpp>\n#include <nrforge/f1ap/f1ap.hpp>\n#include <nrforge/e1ap/e1ap.hpp>\n#include <nrforge/rrc/rrc.hpp>\nint main(){return nrforge::ngap::sdk_identity().version!="'+VERSION+'" || nrforge::f1ap::sdk_identity().version!="'+VERSION+'" || nrforge::e1ap::sdk_identity().version!="'+VERSION+'" || nrforge::rrc::sdk_identity().version!="'+VERSION+'";}\n')
        consumer(out,coexist,relocated,python,'coexist')
        # Packaging is a distinct later command. A candidate receipt is never
        # treated as independent review, Owner acceptance or a release.
        report.update(status='PASS_LOCAL_QUALIFICATION',prefix=str(prefix),wheel=str(wheel),wheel_sha256=sha(wheel),
            identities={p:json.loads((prefix/('share/nrforge-'+p+'/sdk-provenance.json')).read_text()) for p in prefixes},
            sdk_files={str(f.relative_to(prefix)):sha(f) for f in prefix.rglob('*') if f.is_file()},
            reports={n:sha(out/n) for n in ('outer-report.json','transfer-report.json','vectors.json','rrc-vectors/cases.json','f1ap-outer.json','e1ap-outer.json','f1-oracle/report.json','e1ap-containing-audit.json')})
    except BaseException as e:
        report.update(status='FAIL_OR_INTERRUPTED',diagnostic=str(e));raise
    finally:write(out/'producer.json',report)


if __name__=='__main__':main()
