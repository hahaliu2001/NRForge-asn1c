"""Offline B1 regressions; only temporary JSON files, no build or network."""
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest

HERE=Path(__file__).resolve().parent
spec=importlib.util.spec_from_file_location('unified_producer',HERE/'produce.py')
producer=importlib.util.module_from_spec(spec);spec.loader.exec_module(producer)


class ComponentIdentities(unittest.TestCase):
    revision='a'*40

    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory();self.prefix=Path(self.tmp.name)
        for index,profile in enumerate(('ngap','f1ap','e1ap','rrc')):
            identity=dict(version='0.3.0',source_revision=self.revision,
                fingerprint=str(index+1)*64,schema_sha256='b'*64,runtime_sha256='c'*64)
            self.write(profile,dict(identity=identity,full_rrc_coverage=False) if profile=='rrc' else identity)

    def tearDown(self):self.tmp.cleanup()

    def path(self,profile):return self.prefix/('share/nrforge-'+profile+'/sdk-provenance.json')

    def write(self,profile,value):
        path=self.path(profile);path.parent.mkdir(parents=True,exist_ok=True);path.write_text(json.dumps(value))

    def change(self,profile,key,value):
        row=json.loads(self.path(profile).read_text())
        (row['identity'] if profile=='rrc' else row)[key]=value
        self.write(profile,row)

    def test_real_flat_aper_and_nested_rrc_contracts(self):
        result=producer.read_component_identities(self.prefix,self.revision)
        self.assertEqual(set(result),{'ngap','f1ap','e1ap','rrc'})
        self.assertEqual(result['rrc']['version'],'0.3.0')
        self.assertEqual(result['rrc']['fingerprint'],'4'*64)
        self.assertTrue(all(row['source_revision']==self.revision for row in result.values()))

    def test_explicit_component_revisions_preserved(self):
        self.change('rrc','source_revision','d'*40)
        expected={p:self.revision for p in ('ngap','f1ap','e1ap','rrc')}
        expected['rrc']='d'*40
        result=producer.read_component_identities(self.prefix,expected)
        self.assertEqual(result['ngap']['source_revision'],self.revision)
        self.assertEqual(result['rrc']['source_revision'],'d'*40)
        expected['rrc']='e'*40
        with self.assertRaisesRegex(ValueError,'revision mismatch: rrc'):
            producer.read_component_identities(self.prefix,expected)

    def test_incomplete_revision_map_rejected(self):
        with self.assertRaisesRegex(ValueError,'revision map'):
            producer.read_component_identities(self.prefix,{'ngap':self.revision})

    def test_rrc_mixed_version_rejected(self):
        self.change('rrc','version','0.2.0')
        with self.assertRaisesRegex(ValueError,'version: rrc'):producer.read_component_identities(self.prefix,self.revision)

    def test_aper_mixed_version_rejected(self):
        self.change('f1ap','version','0.1.0')
        with self.assertRaisesRegex(ValueError,'version: f1ap'):producer.read_component_identities(self.prefix,self.revision)

    def test_wrong_source_rejected_for_each_component(self):
        for profile in ('ngap','f1ap','e1ap','rrc'):
            with self.subTest(profile=profile):
                self.change(profile,'source_revision','d'*40)
                with self.assertRaisesRegex(ValueError,'revision mismatch: '+profile):producer.read_component_identities(self.prefix,self.revision)
                self.change(profile,'source_revision',self.revision)

    def test_missing_rrc_identity_does_not_fall_back_to_top_level(self):
        self.write('rrc',dict(version='0.3.0',source_revision=self.revision))
        with self.assertRaisesRegex(ValueError,'Missing component identity: rrc'):producer.read_component_identities(self.prefix,self.revision)

    def test_missing_component_is_not_skipped(self):
        self.path('e1ap').unlink()
        with self.assertRaises(FileNotFoundError):producer.read_component_identities(self.prefix,self.revision)

    def test_bad_digest_rejected(self):
        self.change('rrc','fingerprint','not-a-digest')
        with self.assertRaisesRegex(ValueError,'fingerprint: rrc'):producer.read_component_identities(self.prefix,self.revision)

    def test_malformed_identity_rejected(self):
        self.write('rrc',dict(identity=[]))
        with self.assertRaisesRegex(ValueError,'Missing component identity: rrc'):producer.read_component_identities(self.prefix,self.revision)


if __name__=='__main__':unittest.main()
