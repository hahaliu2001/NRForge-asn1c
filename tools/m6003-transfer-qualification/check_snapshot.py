"""Small export regression checks; no compiler/oracle/native build required."""
import io
from pathlib import Path
import tarfile
import tempfile
import unittest

from snapshot import CI_SKELETON_PREFIX, extract_source


def archive(entries):
    buffer = io.BytesIO()
    with tarfile.open(fileobj=buffer, mode='w') as tar:
        for name, kind, value in entries:
            member = tarfile.TarInfo(name)
            if kind == 'file':
                data = value.encode(); member.size = len(data)
                tar.addfile(member, io.BytesIO(data))
            else:
                member.type = tarfile.SYMTYPE; member.linkname = value
                tar.addfile(member)
    return buffer.getvalue()


class SnapshotChecks(unittest.TestCase):
    def export(self, entries):
        temp = tempfile.TemporaryDirectory(); self.addCleanup(temp.cleanup)
        source = Path(temp.name)/'source'
        return source, extract_source(archive(entries), source)

    def test_rebase_ci_link_to_same_snapshot_bytes(self):
        source, links = self.export([('skeletons/ANY.c','file','frozen source'),
                                    ('tests/ANY.c','link',CI_SKELETON_PREFIX+'ANY.c')])
        self.assertEqual((source/'tests/ANY.c').read_bytes(), b'frozen source')
        self.assertEqual((source/'tests/ANY.c').readlink(), Path('../skeletons/ANY.c'))
        self.assertTrue(links[0]['rebased_legacy_ci_link'])
        self.assertEqual(links[0]['archived_target'], CI_SKELETON_PREFIX+'ANY.c')

    def test_relative_link_preserved(self):
        source, links = self.export([('skeletons/ANY.c','file','frozen source'),
                                    ('tests/ANY.c','link','../skeletons/ANY.c')])
        self.assertEqual((source/'tests/ANY.c').read_text(), 'frozen source')
        self.assertFalse(links[0]['rebased_legacy_ci_link'])

    def test_reject_external_or_missing_targets(self):
        for name, target in [('tests/ANY.c','/etc/passwd'),
                ('other/ANY.c',CI_SKELETON_PREFIX+'ANY.c'),
                ('tests/ANY.c',CI_SKELETON_PREFIX+'INTEGER.c'),
                ('tests/ANY.c','../../outside'),
                ('tests/ANY.c',CI_SKELETON_PREFIX+'ANY.c')]:
            with self.subTest(name=name,target=target), self.assertRaises(RuntimeError):
                self.export([(name,'link',target)])

    def test_reject_traversal_and_duplicate(self):
        for entries in [[('../outside','file','bad')],
                        [('same','file','one'),('same','file','two')]]:
            with self.subTest(entries=entries), self.assertRaises(RuntimeError):
                self.export(entries)

    def test_reject_link_to_link(self):
        with self.assertRaises(RuntimeError):
            self.export([('skeletons/ANY.c','file','frozen'),
                         ('alias','link','skeletons/ANY.c'),('other','link','alias')])


if __name__ == '__main__':
    unittest.main()
