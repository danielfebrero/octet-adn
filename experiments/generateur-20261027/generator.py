#!/usr/bin/env python3
"""Successive heterogeneous worlds. Native organisms remain native; no code execution from mail."""
import argparse
import base64
import copy
import fcntl
import hashlib
import json
import os
from pathlib import Path
import random
import shutil
import signal
import subprocess
import time

HERE = Path(__file__).resolve().parent
ISAS = ('legacy', 'relief', 'transmission')
STOP = False
PARAMS = {
 'legacy': {'forces':(0,3),'stress':(0,1),'attention':(0,1),'osmo':(0,32),'season_amp':(0,1),'season_period':(16,1000000),'migrate':(0,1000000),'depense':(0,1),'causales':(0,1),'saison':(0,1),'tronc':(0,1),'food_regrow': (1, 32), 'split_energy': (16, 1000), 'mut_rate': (0, .2), 'dup_rate': (0, .2), 'max_pop': (300, 4096), 'ops_per_tick': (1, 128), 'regs': (0, 1), 'germline': (0, 1), 'attack_steal': (0, 255)},
 'relief': {'season': (16, 1000000)},
 'transmission': {'native_eat_gain': (2, 16), 'native_mutation_divisor': (2, 64), 'native_max_age': (16384, 1000000)},
}

def encoded(x):
 return json.dumps(x, ensure_ascii=False, sort_keys=True, separators=(',', ':')).encode()

def digest(x):
 return hashlib.sha256(x).hexdigest()

def atomic(path, data):
 path = Path(path)
 tmp = path.with_name(path.name + '.tmp')
 with tmp.open('wb') as f:
  f.write(data); f.flush(); os.fsync(f.fileno())
 os.replace(tmp, path)
 fd = os.open(path.parent, os.O_DIRECTORY)
 try: os.fsync(fd)
 finally: os.close(fd)

def write_json(path, data): atomic(path, encoded(data) + b'\n')
def read_json(path): return json.loads(Path(path).read_bytes())

def lock(world):
 f = (world / 'world.lock').open('a')
 fcntl.flock(f, fcntl.LOCK_EX | fcntl.LOCK_NB)
 return f

def native(isa, *args):
 binary=HERE / 'native' / 'build' / isa
 if isa=='legacy' and (HERE / 'native' / 'build' / 'legacy-terra').exists():binary=HERE / 'native' / 'build' / 'legacy-terra'
 if len(args)>1 and (Path(args[1]).parent/'manifest.json').exists():
  entry=read_json(Path(args[1]).parent/'manifest.json')['native_binaries'][isa];binary=Path(entry['path'])
 p = subprocess.run([str(binary), *map(str, args)], capture_output=True, check=True, timeout=120)
 return p.stdout.decode(errors='replace')

def native_summary(world):
 return {isa: native(isa, 'inspect', world / isa).strip() for isa in ISAS}

def parse_metrics(text):
 import re
 return {k: float(v) if '.' in v else int(v) for k, v in re.findall(r'([a-zA-Z_]+)=(-?\d+(?:\.\d+)?)', text)}

def validate_patch(patch):
 if not isinstance(patch, dict) or set(patch) - {'native_params', 'reactions', 'instructions', 'note'}:
  raise ValueError('unknown patch field')
 for isa, values in patch.get('native_params', {}).items():
  if isa not in PARAMS: raise ValueError('unknown ISA')
  for k, v in values.items():
   if k not in PARAMS[isa] or type(v) not in (float, int) or not PARAMS[isa][k][0] <= v <= PARAMS[isa][k][1]:
    raise ValueError('native parameter outside bounds: ' + k)
   if k not in ('mut_rate','dup_rate','season_amp') and type(v) is not int: raise ValueError('integer required')
 for field in ('reactions', 'instructions'):
  rules = patch.get(field, [])
  if not isinstance(rules, list) or len(rules) > 64: raise ValueError('64 new rules maximum')
  for rule in rules:
   if not isinstance(rule, dict) or set(rule) != {'id', 'left', 'right', 'product', 'rate', 'spec', 'descr'}: raise ValueError('rule fields')
   if not isinstance(rule['id'], str) or not 1 <= len(rule['id']) <= 80: raise ValueError('rule ID')
   if not all(type(rule[x]) is int and 0 <= rule[x] <= 255 for x in ('left', 'right', 'product')): raise ValueError('rule bytes')
   if type(rule['rate']) not in (int, float) or not 0 <= rule['rate'] <= 1: raise ValueError('rule rate')
   if not all(isinstance(rule[x], str) and len(rule[x]) <= 4096 for x in ('spec','descr')): raise ValueError('rule prose')
 return patch

def defaults(seed):
 from laws import generate
 registry = generate(seed)
 if type(seed) is not int or not 0 < seed < 2**63: raise ValueError('seed must be in 1..2^63-1')
 rng = random.Random(seed ^ 0x50415353414745)
 params = registry.get('native_params') or {
  'legacy': {'food_regrow': rng.randint(2, 5), 'split_energy': rng.randint(80, 180), 'mut_rate': rng.uniform(.005, .025), 'dup_rate': rng.uniform(.01, .04), 'max_pop': 1024, 'ops_per_tick': rng.randint(16, 40), 'regs': 1, 'germline': 0, 'attack_steal': rng.randint(2, 8)},
  'relief': {'season': rng.randint(2048, 8192)},
  'transmission': {'native_eat_gain': rng.randint(5, 8), 'native_mutation_divisor': rng.randint(4, 8), 'native_max_age': rng.randint(20000, 32000)},
 }
 a,b=rng.randrange(256),rng.randrange(256)
 mass=registry['byte_properties']
 product=rng.choice([n for n in range(256) if mass[n]['mass']<=mass[a]['mass']+mass[b]['mass']])
 return {'schema': 1, 'law_version':1, 'seed': seed, 'name': 'Passage', 'native_params': params,
         'registry': registry, 'reactions': [{'id':'primordial-binding','left':a,'right':b,'product':product,'rate':rng.uniform(.2,.7),'spec':'Deux atomes libres se transforment en un produit typé, avec rejet du résidu au réservoir.','descr':'Réaction élémentaire conçue, catalysée par les assemblages fonctionnels.'}], 'instructions': [],
         'chemistry': {'bond_probability': rng.uniform(.12,.35), 'decay_probability': rng.uniform(.001,.01), 'max_chain': registry['chemistry']['max_chain'], 'max_artifacts': 1024, 'max_free_atoms': 65536},
         'limits': {'batch_ticks': 32, 'batch_seconds': 3.2},
         'compatibility': 'Three native habitats share an emitted-byte chemistry and sensory bridge; no inter-ISA reproduction. Historical protocols remain catalogued, not all executed.'}

def persist_params(world, recipe):
 for isa in ISAS:
  atomic(world / isa / 'params.tsv', ''.join(f'{k} {v}\n' for k,v in recipe['native_params'][isa].items()).encode())

def initial_chemistry():
 return {'free': [8]*256, 'artifacts': [], 'emissions': 0, 'reactions': 0, 'compositions': 0, 'catalyses': 0, 'decayed': 0, 'overflow': 0, 'bridges': 0, 'mass_in':0, 'mass_reservoir':0, 'reaction_energy_spent':0, 'reaction_energy':0}

def properties(recipe):
 """Normalize catalog property names without changing source ISA."""
 props = recipe['registry']['byte_properties']
 if isinstance(props, dict): props = [props[str(i)] if str(i) in props else props[i] for i in range(256)]
 return props

def chemistry_step(chem, recipe, outputs, rng):
 props = properties(recipe)
 accepted=sum(chem['free'])
 for data in outputs.values():
  for b in data:
   chem['emissions'] += 1
   if accepted < recipe['chemistry']['max_free_atoms']: chem['free'][b] += 1; chem['mass_in']+=props[b]['mass']; accepted+=1
   else: chem['overflow'] += 1
 # Artifacts consist of native emitted atom values, never interpreted as host programs.
 free = chem['free']; cfg = recipe['chemistry']; cp=recipe['registry']['chemistry']
 chem['reaction_energy']=min(4096,chem['reaction_energy']+cp['instruction_budget'])
 existing = {tuple(a['bytes']) for a in chem['artifacts']}
 # Bounded random collisions, independent of population order.
 for _ in range(256 // cp['reaction_period']):
  a,b = rng.randrange(256),rng.randrange(256)
  if free[a] < 1 + (a == b) or free[b] < 1: continue
  pa,pb = props[a],props[b]
  # Valence and affinity are actual causes of bonding; catalyst affects rate.
  val_a = int(pa.get('valence', 1)); val_b = int(pb.get('valence',1))
  charge_a = int(pa.get('charge',0)); charge_b = int(pb.get('charge',0))
  if abs(charge_a + charge_b) > cp['bond_threshold']: continue
  affinity = .5 + ((pa['affinity'] ^ pb['affinity']) / 510)
  if min(val_a,val_b) == 0 or rng.random() >= cfg['bond_probability'] * affinity: continue
  if len(chem['artifacts']) >= cfg['max_artifacts']: continue
  free[a]-=1; free[b]-=1
  # Ordered chains have catalyst function in later reactions and an exact sensory encoding.
  chain = [a,b]
  if tuple(chain) in existing:
   free[a]+=1;free[b]+=1;continue
  cat = min(int(pa['catalysis']), int(pb['catalysis']))
  chem['artifacts'].append({'bytes':chain,'born_batch':chem['bridges'], 'catalyst':cat, 'uses':0, 'strength':min(val_a,val_b)+min(pa['hardness'],pb['hardness']), 'retention':min(pa['retention'],pb['retention'])})
  existing.add(tuple(chain));chem['compositions']+=1
 # Existing catalysts have a measurable effect on the number/rate of rule applications.
 catalysts = [a for a in chem['artifacts'] if a['catalyst']]
 for rule in recipe['reactions'] + recipe['instructions']:
  a,b,c = (rule[x] for x in ('left','right','product'))
  boost = 1 + min(cp['catalyst_gain'], len(catalysts))
  for _ in range(min(boost,cp['instruction_budget'])):
   if free[a] < 1+(a==b) or free[b] < 1 or rng.random() >= rule['rate']: continue
   residue=props[a]['mass']+props[b]['mass']-props[c]['mass']
   if residue < 0 and chem['mass_reservoir'] < -residue: continue
   if chem['reaction_energy'] < cp['reaction_cost']:continue
   free[a]-=1;free[b]-=1;free[c]+=1;chem['reactions']+=1
   chem['mass_reservoir']+=residue;chem['reaction_energy_spent']+=cp['reaction_cost'];chem['reaction_energy']-=cp['reaction_cost']
   if catalysts:
    tool = catalysts[rng.randrange(len(catalysts))];tool['uses']+=1;chem['catalyses']+=1
 # Chains grow by incorporating an emitted/free atom while respecting valence and max length.
 for artifact in chem['artifacts']:
  if len(artifact['bytes']) < cfg['max_chain'] and rng.random() < .02:
   b=rng.randrange(256)
   if free[b] and int(props[b].get('valence',1)):
    free[b]-=1;artifact['bytes'].append(b)
 survivors=[]
 for artifact in chem['artifacts']:
  if rng.random() < 1 / (cp['decay_period'] * max(1, artifact['retention'] + artifact['strength']/4)):
   for b in artifact['bytes']: free[b]+=1
   chem['decayed']+=1
  else: survivors.append(artifact)
 chem['artifacts']=survivors;chem['bridges']+=1
 total_mass=sum(n*props[b]['mass'] for b,n in enumerate(free))+sum(props[b]['mass'] for a in survivors for b in a['bytes'])+chem['mass_reservoir']
 expected=8*sum(p['mass'] for p in props)+chem['mass_in']
 if total_mass!=expected:raise ValueError('shared chemistry mass invariant violated')
 chem['total_mass']=total_mass
 # Channel byte is causally derived from the shared chemistry. Transmission receives exact chains.
 dominant=max(range(256), key=lambda n:free[n])
 chains=b''.join(bytes(a['bytes'])+b'\0' for a in survivors[-64:])
 return bytes([dominant])+chains[:4095]

def make_world(destination, selection, seed, context):
 if destination.exists(): raise ValueError('destination exists; use a new world directory')
 creatures=read_json(selection)
 if isinstance(creatures,dict): creatures=creatures.get('creatures',creatures.get('selected'))
 if not isinstance(creatures,list) or len(creatures)!=420: raise ValueError('exactly 420 selected records required')
 for c in creatures:
  raw=base64.b64decode(c['native_record_base64'],validate=True)
  if digest(raw)!=c['native_record_sha256']: raise ValueError('native source record SHA mismatch')
 recipe=defaults(seed)
 destination.mkdir(parents=True)
 write_json(destination/'recipe.json',recipe)
 shutil.copyfile(selection,destination/'founders.json')
 manifest={'seed':seed,'created_at':time.strftime('%Y-%m-%dT%H:%M:%SZ',time.gmtime()),'founders_sha256':digest(Path(selection).read_bytes()),'native_binaries':{},'compartments':{},'source_worlds':sorted({c['source_world'] for c in creatures})}
 for isa in ISAS:
  records=[c for c in creatures if c['isa']==isa]
  rawpath=destination/(isa+'.founders.bin')
  rawpath.write_bytes(b''.join(base64.b64decode(c['native_record_base64']) for c in records))
  args=['init',destination/isa,rawpath,seed]
  if isa=='transmission': args.append(context)
  native(isa,*args)
  if isa=='legacy':
   plugins=destination/isa/'lois';plugins.mkdir(exist_ok=True)
   for plugin in (HERE/'native'/'build'/'plugins').glob('*.so'):shutil.copyfile(plugin,plugins/plugin.name)
  binary=HERE/'native'/'build'/('legacy-terra' if isa=='legacy' and (HERE/'native'/'build'/'legacy-terra').exists() else isa)
  manifest['native_binaries'][isa]={'path':str(binary),'sha256':digest(binary.read_bytes())}
  manifest['compartments'][isa]=len(records)
 persist_params(destination,recipe)
 write_json(destination/'manifest.json',manifest)
 write_json(destination/'chemistry.json',initial_chemistry())
 write_json(destination/'status.json',{'batches':0,'ticks_requested':0,'law_version':1,'founders':420,'summary':native_summary(destination)})
 (destination/'injections').mkdir();(destination/'injected').mkdir();(destination/'updates').mkdir();(destination/'applied').mkdir();(destination/'rejected').mkdir()
 export_laws(destination)
 return manifest

def export_laws(world):
 recipe=read_json(world/'recipe.json')
 lines=['# Lois de '+recipe['name'],'','Graine : '+str(recipe['seed']), '', recipe['compatibility'], '', '## Paramètres natifs effectifs', '', '```json',json.dumps(recipe['native_params'],ensure_ascii=False,indent=2),'```','']
 registry=recipe['registry']
 laws=registry['laws']
 if isinstance(laws,dict): laws=list(laws.values())
 for law in laws + recipe['reactions'] + recipe['instructions']:
  lines += ['## '+str(law.get('id',law.get('name','loi'))),'', '**Statut :** '+str(law.get('status','active-chemistry')), '**Spec :** '+str(law.get('spec','')), '**Descr :** '+str(law.get('descr',law.get('description',''))),'']
  if law.get('source'): lines += ['Source : '+str(law['source']),'']
 lines += ['## Propriétés des 256 octets','','Les opérations ISA restent celles du moteur natif. Ces propriétés pilotent la chimie commune.','','```json',json.dumps(registry['byte_properties'],ensure_ascii=False,indent=2),'```']
 atomic(world/'LOIS.md','\n'.join(lines).encode())
 write_json(world/'LOIS.json',{'seed':recipe['seed'],'effective_native_params':recipe['native_params'],'registry':registry,'reactions':recipe['reactions'],'instructions':recipe['instructions']})

def apply_updates(world, recipe, status):
 changed=False
 for p in sorted((world/'updates').glob('*.json')):
  try:
   patch=validate_patch(read_json(p));new=copy.deepcopy(recipe)
   if p.name in recipe.get('applied_updates',[]):
    status['law_version']=recipe['law_version'];p.unlink();continue
   for isa,values in patch.get('native_params',{}).items():new['native_params'][isa].update(values)
   for field in ('reactions','instructions'):
    known={r['id'] for r in new['reactions']+new['instructions']}
    for rule in patch.get(field,[]):
     if rule['id'] in known: raise ValueError('duplicate rule ID')
     new[field].append(rule);known.add(rule['id'])
    if len(new[field])>256:raise ValueError('world rule limit')
   # Durable declarative recipe before native execution; crash retry is idempotent by receipt.
   receipt=world/'applied'/p.name
   if not receipt.exists():
    record={'patch':patch,'sha256':digest(p.read_bytes()),'at_batch':status['batches'],'version':status['law_version']+1}
    new.setdefault('applied_updates',[]).append(p.name)
    new['law_version']=recipe.get('law_version',status['law_version'])+1
    write_json(world/'recipe.json',new);persist_params(world,new)
    write_json(receipt,record);status['law_version']=new['law_version']
   recipe=new;p.unlink();changed=True
  except (ValueError,KeyError,TypeError,UnicodeError,json.JSONDecodeError) as exc:
   write_json(world/'rejected'/p.name,{'error':str(exc),'patch_raw_sha256':digest(p.read_bytes())});p.unlink()
 if changed:write_json(world/'status.json',status);export_laws(world)
 return recipe

def validate_injection(data):
 creatures=data['creatures']
 if not 1<=len(creatures)<=420:raise ValueError('injection requires1..420 exact records')
 for c in creatures:
  if c['isa'] not in ISAS:raise ValueError('unsupported ISA')
  if digest(base64.b64decode(c['native_record_base64'],validate=True))!=c['native_record_sha256']:raise ValueError('record SHA mismatch')
 return data

def queue_injection(world, selection_path):
 data=validate_injection(read_json(selection_path));creatures=data['creatures']
 raw=encoded(data);name=digest(raw)+'.json'
 if (world/'injected'/name).exists():return {'already_injected':name}
 atomic(world/'injections'/name,raw)
 return {'queued_injection':name,'count':len(creatures)}

def apply_injections(world,status):
 for p in sorted((world/'injections').glob('*.json')):
  if p.name in status.get('injected_ids',[]):p.unlink();continue
  try:data=validate_injection(read_json(p))
  except (ValueError,KeyError,TypeError) as exc:
   write_json(world/'rejected'/p.name,{'error':str(exc)});p.unlink();continue
  creatures=data['creatures'];snapshots={isa:(world/isa/'state.ck').read_bytes() for isa in ISAS}
  try:
   for isa in ISAS:
    rows=[c for c in creatures if c['isa']==isa]
    if not rows:continue
    path=world/(isa+'.injection.raw')
    atomic(path,b''.join(base64.b64decode(c['native_record_base64']) for c in rows))
    native(isa,'inject',world/isa,path);path.unlink()
   status.setdefault('injected_ids',[]).append(p.name)
   status['injected_count']=status.get('injected_count',0)+len(creatures)
   write_json(world/'injected'/p.name,{'manifest':data,'at_batch':status['batches']})
  except subprocess.CalledProcessError as exc:
   for isa,raw in snapshots.items():atomic(world/isa/'state.ck',raw)
   write_json(world/'rejected'/p.name,{'kind':'injection','error':exc.stderr.decode(errors='replace')[:2048]})
  # The queue is retained until status commits. Receipt alone never suppresses replay after rollback.

def acknowledge_injections(world,status):
 for name in status.get('injected_ids',[]):
  p=world/'injections'/name
  if p.exists():p.unlink()
 for p in (world/'injections').glob('*.json'):
  if (world/'rejected'/p.name).exists():p.unlink()

def verify_binaries(world):
 manifest=read_json(world/'manifest.json')
 for isa,entry in manifest['native_binaries'].items():
  if digest(Path(entry['path']).read_bytes())!=entry['sha256']:raise ValueError('native ELF changed: '+isa)

def recover_transaction(world):
 pending=world/'transaction'
 if not (pending/'READY').exists():return
 for source in pending.rglob('*'):
  if source.is_file() and source.name!='READY':
   target=world/source.relative_to(pending)
   atomic(target,source.read_bytes())
 shutil.rmtree(pending)

def begin_transaction(world):
 pending=world/'transaction'
 if pending.exists():shutil.rmtree(pending)
 pending.mkdir()
 names=['chemistry.json','status.json','transmission/mail.committed']
 for isa in ISAS:names += [isa+'/state.ck',isa+'/stimulus.bin']
 for name in names:
  src=world/name
  if src.exists():
   dest=pending/name;dest.parent.mkdir(parents=True,exist_ok=True);atomic(dest,src.read_bytes())
 atomic(pending/'READY',b'rollback-before-next-batch\n')

def finish_transaction(world):
 # Removing marker is the durable commit; backups without marker can be discarded.
 marker=world/'transaction'/'READY';marker.unlink()
 fd=os.open(marker.parent,os.O_DIRECTORY)
 try:os.fsync(fd)
 finally:os.close(fd)
 shutil.rmtree(world/'transaction')

def run(world, batches=0, realtime=True):
 global STOP
 guard=lock(world);verify_binaries(world);recover_transaction(world)
 recipe=read_json(world/'recipe.json');status=read_json(world/'status.json');status['law_version']=recipe.get('law_version',status['law_version']);chem=read_json(world/'chemistry.json')
 if 'rng' in chem:
  rng=random.Random();rng.setstate((chem['rng'][0],tuple(chem['rng'][1]),chem['rng'][2]))
 else:rng=random.Random(recipe['seed'] ^ 0x4348454d)
 count=0
 while not STOP and (not batches or count<batches):
  start=time.monotonic();recipe=apply_updates(world,recipe,status)
  begin_transaction(world)
  apply_injections(world,status)
  outputs={};summaries={}
  for isa in ISAS:
   native(isa,'step',world/isa,recipe['limits']['batch_ticks'])
   emissions=world/isa/'emissions.bin'
   outputs[isa]=emissions.read_bytes() if emissions.exists() else b''
   summaries[isa]=native(isa,'inspect',world/isa).strip()
  stimulus=chemistry_step(chem,recipe,outputs,rng)
  for isa in ISAS:atomic(world/isa/'stimulus.bin',stimulus)
  status.update(batches=status['batches']+1,ticks_requested=status['ticks_requested']+recipe['limits']['batch_ticks'],summary=summaries,updated_at=time.strftime('%Y-%m-%dT%H:%M:%SZ',time.gmtime()))
  status['chemistry']={k:v for k,v in chem.items() if k not in ('free','artifacts','rng')}
  status['chemistry'].update(artifacts=len(chem['artifacts']),free_atoms=sum(chem['free']),functional_tools=sum(a['uses']>0 for a in chem['artifacts']))
  status['population']=sum(parse_metrics(t).get('population',parse_metrics(t).get('pop',0)) for t in summaries.values())
  chem['rng']=rng.getstate();write_json(world/'chemistry.json',chem);write_json(world/'status.json',status)
  # Fixed-size ring of 256 reports, no unbounded event stream.
  reports=world/'reports';reports.mkdir(exist_ok=True);write_json(reports/f'{status["batches"]%256:03}.json',status)
  finish_transaction(world)
  # Immutable committed reports are offloaded automatically by ops/history.
  history=world/'histoire'/'reports';history.mkdir(parents=True,exist_ok=True)
  write_json(history/f'{status["batches"]:016d}.json',status)
  acknowledge_injections(world,status)
  count+=1
  if status['population']==0:break
  if realtime:
   remaining=recipe['limits']['batch_seconds']-(time.monotonic()-start)
   while remaining>0 and not STOP:
    time.sleep(min(.2,remaining));remaining=recipe['limits']['batch_seconds']-(time.monotonic()-start)
 if status.get('population')==0:
  write_json(world/'.archive-closed.json',{'closed':True,'reason':'extinction','last_batch':status['batches']})
 guard.close()
 return status

def queue_update(world, patch_path):
 patch=validate_patch(read_json(patch_path));raw=encoded(patch)
 key=digest(raw)
 if (world/'applied'/(key+'.json')).exists():return {'already_applied':key}
 atomic(world/'updates'/(key+'.json'),raw)
 return {'queued':key}

def stop_handler(*_):
 global STOP
 STOP=True

def main():
 p=argparse.ArgumentParser(description=__doc__);sub=p.add_subparsers(dest='command',required=True)
 a=sub.add_parser('create');a.add_argument('world',type=Path);a.add_argument('--selection',type=Path,required=True);a.add_argument('--seed',type=int,required=True);a.add_argument('--transmission-context',type=Path,required=True)
 a=sub.add_parser('run');a.add_argument('world',type=Path);a.add_argument('--batches',type=int,default=0);a.add_argument('--fast',action='store_true')
 a=sub.add_parser('inspect');a.add_argument('world',type=Path)
 a=sub.add_parser('update');a.add_argument('world',type=Path);a.add_argument('patch',type=Path)
 a=sub.add_parser('inject');a.add_argument('world',type=Path);a.add_argument('selection',type=Path)
 a=sub.add_parser('laws');a.add_argument('world',type=Path)
 args=p.parse_args();signal.signal(signal.SIGTERM,stop_handler);signal.signal(signal.SIGINT,stop_handler)
 if args.command=='create':result=make_world(args.world.resolve(),args.selection.resolve(),args.seed,args.transmission_context.resolve())
 elif args.command=='run':result=run(args.world.resolve(),args.batches,not args.fast)
 elif args.command=='inspect':result=read_json(args.world/'status.json')
 elif args.command=='inject':result=queue_injection(args.world,args.selection)
 elif args.command=='update':result=queue_update(args.world,args.patch)
 else:export_laws(args.world);result={'laws':str(args.world/'LOIS.md')}
 print(json.dumps(result,ensure_ascii=False,indent=2))
if __name__=='__main__':main()
