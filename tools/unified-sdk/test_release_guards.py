"""Offline release-state regression sources; no network or native compilation."""
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

HERE=Path(__file__).resolve().parent
spec=importlib.util.spec_from_file_location('unified_release',HERE/'release.py')
release=importlib.util.module_from_spec(spec);spec.loader.exec_module(release)


class ReleaseGuards(unittest.TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory();self.root=Path(self.tmp.name)
        self.candidate=self.root/'candidate';self.candidate.mkdir()
        (self.candidate/'wheel.whl').write_bytes(b'fixture')
        self.manifest=dict(version=release.VERSION,repository=release.REPO,source_revision='a'*40,assets={'wheel.whl':release.sha(self.candidate/'wheel.whl')})
        (self.candidate/'candidate.json').write_text(json.dumps(self.manifest))
        self.acceptance=self.root/'acceptance.json'
        self.acceptance.write_text(json.dumps(dict(candidate_sha256=release.sha(self.candidate/'candidate.json'),owner_acceptance='ACCEPTED',independent_review='PASS')))
        self.args=type('Args',(),dict(candidate=self.candidate,acceptance=self.acceptance))()

    def tearDown(self):self.tmp.cleanup()

    def test_changed_candidate_is_refused_before_network(self):
        (self.candidate/'wheel.whl').write_bytes(b'changed')
        with patch.object(release,'gh') as api,self.assertRaisesRegex(ValueError,'asset identity'):
            release.publish(self.args)
        api.assert_not_called()

    def test_acceptance_binds_exact_candidate(self):
        self.manifest['source_revision']='b'*40
        (self.candidate/'candidate.json').write_text(json.dumps(self.manifest))
        with patch.object(release,'gh') as api,self.assertRaisesRegex(ValueError,'Exact candidate'):
            release.publish(self.args)
        api.assert_not_called()

    def test_existing_release_never_creates_or_uploads(self):
        calls=[]
        def api(endpoint,method='GET',body=None):
            calls.append((endpoint,method))
            return [dict(tag_name='asn1-sdk-v'+release.VERSION)]
        with patch.object(release,'gh',api),patch.object(release.subprocess,'run') as upload,self.assertRaisesRegex(ValueError,'already exists'):
            release.publish(self.args)
        upload.assert_not_called();self.assertTrue(all(method=='GET' for _,method in calls))

    def test_missing_migration_stops_before_delete(self):
        (self.candidate/'publication.json').write_text(json.dumps(dict(candidate_sha256=release.sha(self.candidate/'candidate.json'),release_id=7)))
        migrations=self.root/'migrations.json';migrations.write_text(json.dumps({'RAN':dict(status='PENDING')}))
        self.args.migrations=migrations;self.args.plan=self.root/'unused-plan.json'
        calls=[]
        def api(endpoint,method='GET',body=None):
            calls.append(method);return dict(draft=False,tag_name='asn1-sdk-v'+release.VERSION)
        with patch.object(release,'gh',api),self.assertRaisesRegex(ValueError,'Both consumers'):
            release.retire(self.args)
        self.assertNotIn('DELETE',calls)

    def test_current_asset_digest_cannot_be_retired(self):
        digest=release.sha(self.candidate/'candidate.json')
        (self.candidate/'publication.json').write_text(json.dumps(dict(candidate_sha256=digest,release_id=7,assets=[dict(id=8)])))
        migrations=self.root/'migrations.json';migrations.write_text(json.dumps({c:dict(status='PASS',version=release.VERSION,candidate_sha256=digest) for c in ('RAN','5GC')}))
        plan=self.root/'plan.json';plan.write_text(json.dumps(dict(assets=[dict(repository=release.REPO,asset_id=8,name='wheel.whl',size=7,sha256=self.manifest['assets']['wheel.whl'])])))
        self.args.migrations=migrations;self.args.plan=plan
        calls=[]
        def api(endpoint,method='GET',body=None):
            calls.append(method)
            return dict(draft=False,tag_name='asn1-sdk-v'+release.VERSION) if endpoint.endswith('/7') else dict(id=8,name='wheel.whl',size=7)
        with patch.object(release,'gh',api),self.assertRaisesRegex(ValueError,'current asset'):
            release.retire(self.args)
        self.assertNotIn('DELETE',calls)


if __name__=='__main__':unittest.main()
