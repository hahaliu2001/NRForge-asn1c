#!/usr/bin/env python3
"""Offline reuse/lineage guards; no SDK compilation or installed native calls."""
import hashlib
import io
import json
from pathlib import Path
import tarfile
import tempfile
import unittest
from unittest.mock import patch

import resume_rrc as recovery


def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def write(path,value):
    path.parent.mkdir(parents=True,exist_ok=True)
    path.write_text(json.dumps(value))


class ResumeRrcTests(unittest.TestCase):
    def exercise(self, mutation=None):
        with tempfile.TemporaryDirectory() as root:
            out=Path(root)/'unified';out.mkdir()
            closure=Path(root)/'closure';closure.mkdir()
            source=closure/'source';(source/'libaper').mkdir(parents=True)
            cpp=source/'libaper/runtime.hpp';cpp.write_bytes(b'unchanged shared runtime')
            previous=dict(status='FAIL_OR_INTERRUPTED',source_revision=recovery.APER_REVISION)
            write(out/'producer.json',previous)
            (out/'05-rrc').mkdir();(out/'05-rrc/status').write_text('EXIT 1\n')
            matrix=closure/'rrc-generation/matrix.json'
            write(matrix,dict(roots=[dict(extract=0,render=0) for _ in range(91)]))
            passed=dict(status='PASS_RRC_GENERATION_ONLY',source_revision=recovery.RRC_REVISION,
                selected_roots=91,historical_85_headers='UNCHANGED',matrix_sha256=sha(matrix))
            write(closure/'report.json',passed)
            rrc=dict(version='0.3.0',source_revision=recovery.RRC_REVISION,
                fingerprint='a'*64,runtime_sha256='b'*64,schema_sha256='c'*64)
            write(closure/'rrc-generation/generated/sdk-provenance.json',dict(identity=rrc))
            for profile in ('ngap','f1ap','e1ap'):
                stage=out/('05-'+profile);stage.mkdir();(stage/'status').write_text('EXIT 0\n')
                provenance=dict(version='0.3.0',source_revision=recovery.APER_REVISION,
                    source_hashes={'libaper/runtime.hpp':sha(cpp)})
                prefix=out/(profile+'-prefix')
                write(prefix/('share/nrforge-'+profile+'/sdk-provenance.json'),provenance)
                archive=prefix/'lib/component.a';archive.parent.mkdir();archive.write_bytes(b'old archive')
                write(out/(profile+'-build/generated/sdk-provenance.json'),provenance)
                commands=out/(profile+'-build/build/compile_commands.json');write(commands,[])
                write(out/(profile+'-build/build-summary.json'),dict(
                    status='BUILT_AND_INSTALLED_CONSUMER_VERIFICATION_PENDING',sdk_provenance=provenance,
                    installed_files={str(p.relative_to(prefix)):sha(p) for p in prefix.rglob('*') if p.is_file()},
                    compile_commands_sha256=sha(commands)))
            if mutation=='installed':(out/'ngap-prefix/lib/component.a').write_bytes(b'changed')
            if mutation=='matrix':write(matrix,dict(roots=[]))
            if mutation=='source':cpp.write_bytes(b'changed shared runtime')
            archive=io.BytesIO()
            with tarfile.open(fileobj=archive,mode='w') as tar:
                data=b'unchanged shared runtime';entry=tarfile.TarInfo('libaper/runtime.hpp');entry.size=len(data)
                tar.addfile(entry,io.BytesIO(data))
            def git(argv):
                if 'archive' in argv:return archive.getvalue()
                return b'tools/unified-sdk/produce.py\n'
            calls=[]
            def command(directory,argv,**kwargs):
                calls.append(directory.name)
                if directory.name=='04-build':self.assertEqual(argv[-2:],['--parallel','1'])
                if directory.name=='05-install':
                    write(out/'rrc-prefix/share/nrforge-rrc/sdk-provenance.json',dict(identity=rrc))
            with patch.object(recovery,'ensure_no_active_build'),patch.object(recovery.subprocess,'check_output',side_effect=git):
                if mutation:
                    with self.assertRaises(ValueError):
                        recovery.recover(Path(root),out,'d'*40,closure,command,write)
                    self.assertEqual(calls,[])
                    self.assertFalse((out/'rrc-recovery').exists())
                else:
                    result=recovery.recover(Path(root),out,'d'*40,closure,command,write)
                    self.assertEqual(result['component_source_revisions']['ngap'],recovery.APER_REVISION)
                    self.assertEqual(result['component_source_revisions']['rrc'],recovery.RRC_REVISION)
                    self.assertEqual(result['qualification_source'],str(source))
                    self.assertEqual(calls,['01-verify-ngap','01-verify-f1ap','01-verify-e1ap',
                        '02-verify-rrc-generation','03-configure','04-build','05-install'])
                    self.assertEqual(json.loads((out/'rrc-recovery/original-producer.json').read_text()),previous)
                    self.assertEqual((out/'05-rrc/status').read_text(),'EXIT 1\n')
                    self.assertEqual((out/'ngap-prefix/lib/component.a').read_bytes(),b'old archive')
                    self.assertEqual(json.loads((closure/'report.json').read_text()),passed)

    def test_reuse_preserves_evidence_and_component_sources(self):self.exercise()
    def test_modified_install_refused(self):self.exercise('installed')
    def test_modified_matrix_refused(self):self.exercise('matrix')
    def test_modified_qualification_source_refused(self):self.exercise('source')


if __name__=='__main__':unittest.main()
