import copy
import json
from pathlib import Path
import random
import tempfile
import unittest
import generator as g

class Tests(unittest.TestCase):
 def test_successor_adapter_manifest_path_and_digest(self):
  import hashlib
  import select_successors as s
  with tempfile.TemporaryDirectory() as d:
   adapter=Path(d)/'pinned-adapter';adapter.write_bytes(b'fixture');adapter.chmod(0o700)
   entry={'path':str(adapter),'sha256':hashlib.sha256(b'fixture').hexdigest()}
   self.assertEqual(adapter,s.binary('legacy',entry))
   adapter.write_bytes(b'changed')
   with self.assertRaises(ValueError):s.binary('legacy',entry)
 def test_reproducible_properties_and_mutation(self):
  r=g.defaults(2026102701)
  self.assertEqual(r,g.defaults(2026102701))
  self.assertNotEqual(r['registry']['byte_properties'],g.defaults(2026102702)['registry']['byte_properties'])
  self.assertEqual(256,len(r['registry']['byte_properties']))
  chem=g.initial_chemistry();clone=copy.deepcopy(chem)
  rng=random.Random(2);rng2=random.Random(2)
  for _ in range(200):
   a=g.chemistry_step(chem,r,{'legacy':bytes(range(256))},rng)
   b=g.chemistry_step(clone,r,{'legacy':bytes(range(256))},rng2)
   self.assertEqual(a,b)
  self.assertEqual(chem,clone)
  self.assertGreater(chem['compositions'],0)
  self.assertGreater(chem['reactions'],0)
  self.assertGreater(chem['catalyses'],0)
  self.assertTrue(all(n>=0 for n in chem['free']))
 def test_patch_rejection(self):
  for patch in ({'native_params':{'legacy':{'ops_per_tick':0}}},{'native_params':{'legacy':{'max_pop':1}}},{'exec':'bad'}, {'native_params':{'relief':{'season':float('nan')}}}):
   with self.assertRaises(ValueError):g.validate_patch(patch)
 def test_hot_update_idempotent(self):
  with tempfile.TemporaryDirectory() as d:
   world=Path(d);r=g.defaults(53);s={'batches':4,'law_version':1}
   for name in (*g.ISAS,'updates','applied','rejected'):(world/name).mkdir()
   patch={'native_params':{'relief':{'season':3333}},'instructions':[{'id':'new-chemical-instruction','left':0,'right':1,'product':2,'rate':1,'spec':'0+1->2','descr':'test'}]}
   g.write_json(world/'patch.json',patch);g.queue_update(world,world/'patch.json')
   new=g.apply_updates(world,r,s)
   self.assertEqual(3333,new['native_params']['relief']['season']);self.assertEqual(2,s['law_version'])
   self.assertIn('3333',(world/'relief'/'params.tsv').read_text())
   self.assertIn('already_applied',g.queue_update(world,world/'patch.json'))
   self.assertEqual(new,g.apply_updates(world,new,s));self.assertEqual(2,s['law_version'])
 def test_crash_transaction_restores_all(self):
  with tempfile.TemporaryDirectory() as d:
   world=Path(d)
   for isa in g.ISAS:
    (world/isa).mkdir();(world/isa/'state.ck').write_bytes(isa.encode());(world/isa/'stimulus.bin').write_bytes(b'old')
   g.write_json(world/'status.json',{'batch':5});g.write_json(world/'chemistry.json',{'a':1})
   g.begin_transaction(world)
   for isa in g.ISAS:(world/isa/'state.ck').write_bytes(b'bad')
   g.write_json(world/'status.json',{'batch':6});g.recover_transaction(world)
   self.assertEqual({'batch':5},g.read_json(world/'status.json'))
   for isa in g.ISAS:self.assertEqual(isa.encode(),(world/isa/'state.ck').read_bytes())
   g.recover_transaction(world)

if __name__=='__main__':unittest.main()
