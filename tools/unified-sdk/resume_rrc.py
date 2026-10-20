"""Finish the failed RRC stage with explicit, authenticated component lineage."""
import json
import hashlib
import io
import tarfile
from pathlib import Path
import shutil
import subprocess

from resume_ngap import ensure_no_active_build

APER_REVISION = '71712e41e903634def9b056f4bcd1de4b210520e'
RRC_REVISION = 'b26e206f6b86ffc21dbebdfe70ee5b129393b72d'


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def read(path):
    return json.loads(path.read_text())


def installed_files(prefix, expected, sha):
    if not isinstance(expected,dict) or not expected:
        raise ValueError('Missing component install receipt')
    if any(p.is_symlink() for p in prefix.rglob('*')):
        raise ValueError('Symlink in reused component install')
    actual={str(p.relative_to(prefix)):sha(p) for p in prefix.rglob('*') if p.is_file()}
    if actual!=expected:
        raise ValueError('Reused component installation changed: '+str(prefix))


def qualification_source_hash(repo, source):
    """Authenticate scripts and C++ inputs used after the passed RRC snapshot."""
    archive=subprocess.check_output(['git','-C',str(repo),'archive',RRC_REVISION,'--',
        'python','tools','libaper','libngap','cmake'])
    with tarfile.open(fileobj=io.BytesIO(archive)) as contents:
        for member in contents:
            # Autoreconf legitimately rewrites generated Automake templates.
            if not member.isfile() or member.name.endswith('/Makefile.in'):
                continue
            if (source/member.name).read_bytes()!=contents.extractfile(member).read():
                raise ValueError('Qualification source differs from passed revision: '+member.name)
    return hashlib.sha256(archive).hexdigest()


def recover(repo, out, head, closure, command, write):
    ensure_no_active_build(out)
    ensure_no_active_build(closure)
    previous=read(out/'producer.json')
    if (previous['status']!='FAIL_OR_INTERRUPTED' or previous['source_revision']!=APER_REVISION
            or (out/'05-rrc/status').read_text().strip()!='EXIT 1'):
        raise ValueError('Not the reviewed RRC extraction failure after three APER installs')
    if any((out/name).exists() for name in ('sdk','rrc-prefix','06-venv','rrc-recovery-build')):
        raise ValueError('RRC installation or later unified stages already exist')
    passed=read(closure/'report.json')
    if (passed['status']!='PASS_RRC_GENERATION_ONLY' or passed['source_revision']!=RRC_REVISION
            or passed['selected_roots']!=91 or passed['historical_85_headers']!='UNCHANGED'):
        raise ValueError('Exact selected RRC generation receipt required')
    source=closure/'source'; generated=closure/'rrc-generation/generated'
    matrix=closure/'rrc-generation/matrix.json'
    if sha(matrix)!=passed['matrix_sha256']:
        raise ValueError('Passed RRC generation matrix changed')
    rows=read(matrix)['roots']
    if len(rows)!=91 or any(r['extract'] or r['render'] for r in rows):
        raise ValueError('Incomplete RRC closure matrix')
    rrc=read(generated/'sdk-provenance.json')['identity']
    if rrc['version']!='0.3.0' or rrc['source_revision']!=RRC_REVISION:
        raise ValueError('RRC sealed identity mismatch')
    # The qualification source remains the passed archive, not the newer runner.
    # Permit only the new orchestration/review/tests since that source revision.
    changes=subprocess.check_output(['git','-C',str(repo),'diff','--name-only',RRC_REVISION,head]).decode().splitlines()
    allowed={'tools/unified-sdk/produce.py','tools/unified-sdk/resume_rrc.py',
             'tools/unified-sdk/test_resume_rrc.py','tools/unified-sdk/test_component_identities.py',
             'docs/unified-sdk-expanded-source-review.md','Makefile.am'}
    if not changes or set(changes)-allowed:
        raise ValueError('Unreviewed changes after the passed RRC source')
    qualification_archive_sha256=qualification_source_hash(repo,source)
    receipts={}
    for profile in ('ngap','f1ap','e1ap'):
        if (out/('05-'+profile)/'status').read_text().strip()!='EXIT 0':
            raise ValueError('APER component did not finish: '+profile)
        summary=read(out/(profile+'-build/build-summary.json'))
        provenance=summary['sdk_provenance']
        if summary['status']!='BUILT_AND_INSTALLED_CONSUMER_VERIFICATION_PENDING' or provenance!=read(out/(profile+'-build/generated/sdk-provenance.json')):
            raise ValueError('Original build receipt/provenance differs: '+profile)
        if provenance['version']!='0.3.0' or provenance['source_revision']!=APER_REVISION:
            raise ValueError('Unexpected reused APER identity: '+profile)
        installed_files(out/(profile+'-prefix'),summary['installed_files'],sha)
        if sha(out/(profile+'-build/build/compile_commands.json'))!=summary['compile_commands_sha256']:
            raise ValueError('Original component compile evidence changed: '+profile)
        # Qualification uses these unchanged shared C++ inputs with old archives.
        for name,digest in provenance['source_hashes'].items():
            if name.startswith(('libaper/','libngap/')) and sha(source/name)!=digest:
                raise ValueError('Shared C++ source differs from reused SDK: '+name)
        receipts[profile]=sha(out/(profile+'-build/build-summary.json'))
    evidence=out/'rrc-recovery';evidence.mkdir()
    write(evidence/'original-producer.json',previous)
    shutil.copytree(out/'05-rrc',evidence/'original-05-rrc')
    write(evidence/'reuse-plan.json',dict(producer_revision=head,qualification_source_revision=RRC_REVISION,
        component_source_revisions={**{p:APER_REVISION for p in receipts},'rrc':RRC_REVISION},
        qualification_archive_sha256=qualification_archive_sha256,aper_build_receipts=receipts,rrc_generation_receipt=sha(closure/'report.json'),matrix_sha256=sha(matrix)))
    # Original source/generated/archive receipts must pass before any new build.
    for profile in receipts:
        build=out/(profile+'-build/build')
        command(evidence/('01-verify-'+profile),['cmake','-DNRFORGE_SOURCE_ROOT='+str(out/'source'),
            '-DNRFORGE_GENERATED_ROOT='+str(out/(profile+'-build/generated')),
            '-DNRFORGE_SDK_ARCHIVE='+str(build/('libnrforge_'+profile+'.a')),
            '-DNRFORGE_SDK_RECEIPT='+str(build/'sdk-build-receipt.cmake'),
            '-P',out/'source/cmake/VerifyNgapSdk.cmake'])
    command(evidence/'02-verify-rrc-generation',['cmake','-DNRFORGE_SOURCE_ROOT='+str(source),
        '-DNRFORGE_GENERATED_ROOT='+str(generated),'-P',source/'cmake/VerifyNgapSdk.cmake'])
    build=out/'rrc-recovery-build';prefix=out/'rrc-prefix'
    command(evidence/'03-configure',['cmake','-S',generated,'-B',build,
        '-DNRFORGE_SOURCE_ROOT='+str(source),'-DCMAKE_INSTALL_PREFIX='+str(prefix),
        '-DCMAKE_EXPORT_COMPILE_COMMANDS=ON',
        '-DCMAKE_CXX_FLAGS=-O0 -g0 -Wall -Wextra -Werror -Wconversion -Wsign-conversion -pedantic-errors --param=ggc-min-expand=5 --param=ggc-min-heapsize=16384',
        '-DCMAKE_EXE_LINKER_FLAGS=-fuse-ld=lld -Wl,--threads=1'])
    command(evidence/'04-build',['cmake','--build',build,'--parallel','1'],timeout=43200,live=True)
    command(evidence/'05-install',['cmake','--install',build],timeout=3600)
    if read(prefix/'share/nrforge-rrc/sdk-provenance.json')['identity']!=rrc:
        raise ValueError('Installed RRC identity differs from passed generation')
    return dict(qualification_source=str(source),qualification_source_revision=RRC_REVISION,
        component_source_revisions={**{p:APER_REVISION for p in receipts},'rrc':RRC_REVISION},
        evidence=str(evidence),rrc_generation_work=str(closure))
