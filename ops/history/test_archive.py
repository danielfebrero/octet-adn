import importlib.util, io, json, sqlite3, tarfile, tempfile, unittest
from pathlib import Path
from unittest.mock import patch
import archive as a
import restore as r

class HistoryTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory(); self.root = Path(self.tmp.name)
        self.state = self.root/'state'; self.state.mkdir()
        self.patch = patch.object(a, 'STATE', self.state); self.patch.start()
        self.db = sqlite3.connect(':memory:')
        self.db.execute('CREATE TABLE blocks (path TEXT, signature TEXT, offset INTEGER, size INTEGER, sha TEXT, PRIMARY KEY(path,signature,offset))')
        self.db.execute('CREATE TABLE files (path TEXT PRIMARY KEY, signature TEXT)')
    def tearDown(self):
        self.patch.stop(); self.db.close(); self.tmp.cleanup()
    def test_failed_upload_never_marks_verified_or_deletes(self):
        p=self.root/'original'; p.write_bytes(b'life')
        e=a.Export(self.db); e.add('backups/octet-adn-20261025-20f62d86/x', a.signature(p), 0o640, 0, p.read_bytes())
        with patch.object(a,'get_release',side_effect=RuntimeError('offline')):
            with self.assertRaises(RuntimeError): e.flush()
        self.assertEqual(p.read_bytes(), b'life')
        self.assertEqual(self.db.execute('select count(*) from blocks').fetchone()[0],0)
    def test_changed_file_is_not_evicted(self):
        p=self.root/'original'; p.write_bytes(b'life'); sig=a.signature(p)
        self.db.execute('insert into blocks values (?,?,?,?,?)', ('backups/octet-adn-20261025-20f62d86/x', json.dumps(sig),0,4,a.sha(b'life')))
        p.write_bytes(b'changed')
        self.assertEqual(a.finalize(self.db,[(p,'backups/octet-adn-20261025-20f62d86/x',sig)],set()),0)
        self.assertTrue(p.exists())
    def test_restore_blocks_roundtrip_and_refuse_overwrite(self):
        data=b'abc\x00\xff'; stream=io.BytesIO()
        with tarfile.open(fileobj=stream,mode='w:gz') as t:
            for i,b in enumerate((data[:2],data[2:])):
                ti=tarfile.TarInfo(str(i));ti.size=len(b);t.addfile(ti,io.BytesIO(b))
        payload=stream.getvalue(); sig=[1,2,len(data),3,4]
        meta={'sha256':a.sha(payload)}
        rows=[(meta,dict(signature=sig,offset=offset,size=len(b),sha256=a.sha(b),member=str(i),mode=0o640),'memory') for i,(offset,b) in enumerate(((0,data[:2]),(2,data[2:])))]
        out=self.root/'restored'
        with patch.object(r,'get',return_value=payload):
            self.assertEqual(r.restore(rows,out),len(data)); self.assertEqual(out.read_bytes(),data)
            with self.assertRaises(FileExistsError):r.restore(rows,out)
    def test_restore_rejects_corruption_and_incomplete(self):
        row=({},dict(signature=[1,2,5,3,4],offset=0,size=2),'memory')
        with self.assertRaises(ValueError):r.restore([row],self.root/'missing')
    def test_live_and_mail_protection(self):
        for key in ('repo/experiments/vie-future/state.ck','repo/experiments/vie-future/events.tsv','repo/experiments/vie-relief-20261025/state.ck','repo/experiments/vie-future/reports/000.json','repo/experiments/vie-future/mail/0001.bin'):
            self.assertFalse(a.evictable(key,1e9,set()),key)
        self.assertTrue(a.evictable('repo/experiments/vie-future/histoire/events-20261025.tsv.gz',1e9,set()))
    def test_verified_eviction_requires_complete_coverage(self):
        p=self.root/'original';p.write_bytes(b'life');sig=a.signature(p)
        with self.assertRaises(RuntimeError): a.finalize(self.db,[(p,'backups/octet-adn-20261025-20f62d86/x',sig)],set())
        self.assertTrue(p.exists())
        self.db.execute('insert into blocks values (?,?,?,?,?)', ('backups/octet-adn-20261025-20f62d86/x',json.dumps(sig),0,4,a.sha(b'life')))
        self.assertEqual(a.finalize(self.db,[(p,'backups/octet-adn-20261025-20f62d86/x',sig)],set()),4)
        self.assertFalse(p.exists())
if __name__=='__main__': unittest.main()
