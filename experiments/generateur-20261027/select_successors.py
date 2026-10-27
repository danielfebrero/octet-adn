#!/usr/bin/env python3
"""Select a successor cohort from generated habitats, without initialization or ticks."""
import argparse,base64,collections,ctypes as C,hashlib,json,pathlib,re,subprocess
from selection import ranked_select
HERE=pathlib.Path(__file__).resolve().parent
U8=C.c_uint8;U16=C.c_uint16;U32=C.c_uint32;U64=C.c_uint64;I=C.c_int
class Legacy(C.Structure):
 _fields_=[('x',I),('y',I),('d',I),('a',U8),('b',U8)]+[(s,I) for s in ['ip','e','age','id','species','parent','stores','useful_stores','eaten','steps','emitted','splits','len']]+[('dna',U8*1024),('dirty',U8*128),('ls',I*6),('lscore',C.c_double),('win_len',I),('win',U8*32),('win0',U8*32),('germ_len',I),('germ',U8*1024),('r',U8*4)]
class Relief(C.Structure):
 _fields_=[(s,U64) for s in ['id','parent','lineage','born']]+[(s,U32) for s in ['pos','age','energy','body','store']]+[('genome',U8*64),('memory',U8*8)]+[(s,U8) for s in ['pc','dir','alive','pad']]
class Transmission(C.Structure):
 _fields_=[(s,U64) for s in ['id','parent','lineage','born','last_mail','last_reward','last_request']]+[(s,U32) for s in ['age','energy']]+[(s,U16) for s in ['pc','p','outlen']]+[(s,U8) for s in ['a','b','source','book','alive','kind']]+[('pad',U8*2),('code',U8*512),('ram',U8*4096),('output',U8*4096)]
TYPES={'legacy':Legacy,'relief':Relief,'transmission':Transmission}
EXPECTED={'legacy':2360,'relief':128,'transmission':8784}
def sha(b):return hashlib.sha256(b).hexdigest()
def binary(isa,expected_sha=None):
 candidates=[HERE/'native/build'/isa,HERE/'native'/isa]
 if isa=='legacy':candidates.insert(0,HERE/'native/build/legacy-terra')
 for path in candidates:
  if expected_sha and (not path.is_file() or sha(path.read_bytes())!=expected_sha):continue
  if path.is_file() and path.stat().st_mode&0o111:return path
 raise ValueError('missing native adapter '+isa)
def quota(counts,n):
 """Proportional source representation, at least one per nonempty habitat."""
 if n<sum(bool(x) for x in counts.values()):raise ValueError('count cannot preserve every nonempty ISA habitat')
 total=sum(counts.values());result={k:int(v>0) for k,v in counts.items()}
 for _ in range(n-sum(result.values())):
  k=max((k for k in counts if result[k]<counts[k]),key=lambda k:(n*counts[k]/total-result[k],k))
  result[k]+=1
 return result

def decode(isa,data,tick):
 typ=TYPES[isa];size=C.sizeof(typ)
 if size!=EXPECTED[isa] or len(data)%size:raise ValueError('native ABI/record-size mismatch '+isa)
 rows=[]
 for off in range(0,len(data),size):
  raw=data[off:off+size];a=typ.from_buffer_copy(raw)
  if isa=='legacy':
   if a.e<=0:continue
   if not 0<a.len<=1024:raise ValueError('invalid legacy DNA length')
   genome=bytes(a.dna[:a.len]);memory=b''
  else:
   if not a.alive:continue
   genome=bytes(a.genome if isa=='relief' else a.code);memory=bytes(a.memory if isa=='relief' else a.ram)
  fields={name:getattr(a,name) for name,_ in typ._fields_ if isinstance(getattr(a,name),(int,float))}
  fields.update(export_record_index=off//size,selection_identity=f'{isa}:{off//size}',source_id=fields.pop('id'),tick=tick,genome_hex=genome.hex(),memory_hex=memory.hex(),native_record_base64=base64.b64encode(raw).decode(),native_record_sha256=sha(raw),native_record_size=size,genome_sha256=sha(genome),memory_sha256=sha(memory))
  if isa=='legacy':fields.update(energy=fields['e'],pc=fields['ip'])
  rows.append(fields)
 return rows

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('world',type=pathlib.Path);p.add_argument('--count',type=int,default=420);p.add_argument('--output',type=pathlib.Path,required=True);a=p.parse_args()
 if a.count<=0:raise ValueError('positive count required')
 world=a.world.resolve();output=a.output.resolve()
 if output.exists():raise ValueError('output already exists; preserve previous selection')
 output.parent.mkdir(parents=True,exist_ok=True);snap=output.with_name(output.name+'.sources')
 if snap.exists():raise ValueError('snapshot destination exists')
 snap.mkdir();candidates={};sources=[];blobs={}
 generation_manifest=json.loads((world/'manifest.json').read_text()) if (world/'manifest.json').exists() else {}
 for isa in TYPES:
  live=world/isa/'state.ck'
  if not live.exists():candidates[isa]=[];continue
  directory=snap/isa;directory.mkdir();blob=live.read_bytes();(directory/'state.ck').write_bytes(blob);blobs[isa]=blob
  adapter=binary(isa,generation_manifest.get('native_binaries',{}).get(isa));rawpath=directory/'population.raw'
  subprocess.run([str(adapter),'export',str(directory),str(rawpath)],check=True,capture_output=True,timeout=120)
  report=subprocess.check_output([str(adapter),'inspect',str(directory)],text=True,timeout=120)
  m=re.search(r'(?:tick|t)=(\d+)',report);tick=int(m[1]) if m else None
  candidates[isa]=decode(isa,rawpath.read_bytes(),tick)
  cursor=0
  for row in candidates[isa]:
   raw=base64.b64decode(row['native_record_base64']);off=blob.find(raw,cursor)
   if off<0:raise ValueError('export not exact/in source checkpoint order '+isa)
   row['source_record_offset']=off;cursor=off+len(raw)
  sources.append({'world':str(world),'habitat':isa,'checkpoint_path':str(live),'snapshot_path':str(directory/'state.ck'),'checkpoint_sha256':sha(blob),'native_adapter_sha256':sha(adapter.read_bytes()),'candidates':len(candidates[isa]),'tick':tick})
 counts={k:len(v) for k,v in candidates.items()}
 if sum(counts.values())<a.count:raise ValueError(f'only {sum(counts.values())} living organisms available; requested {a.count}; no artificial founders created')
 quotas=quota(counts,a.count);creatures=[]
 for isa,rows in candidates.items():
  if not quotas[isa]:continue
  source=next(s for s in sources if s['habitat']==isa)
  for r in ranked_select(rows,quotas[isa]):
   raw=base64.b64decode(r['native_record_base64']);offset=r.pop('source_record_offset')
   if offset<0:raise ValueError('native export changed source bytes '+isa)
   r.update(id=f'{world.name}:{isa}:record{r["export_record_index"]}:id{r["source_id"]}',source_world=str(world/isa),isa=isa,provenance={**source,'record_offset':offset})
   creatures.append(r)
 manifest={'schema':'eg.world.selection.v1','count':len(creatures),'source_generation':str(world),'selection_policy':'Living generated-world successors; proportional habitat quotas; rotating measured maxima and distinct genes first; no synthesized founders or byte conversion.','snapshot_consistency':'Each habitat checkpoint is copied from one atomic generation. Habitat ticks may differ while the source controller runs.','sources':sources,'quotas':quotas,'creatures':creatures,'transmission_context':str(snap/'transmission/state.ck') if candidates.get('transmission') else None}
 data=(json.dumps(manifest,ensure_ascii=False,indent=2)+'\n').encode();output.write_bytes(data)
 print(json.dumps({'count':len(creatures),'quotas':quotas,'output':str(output),'sha256':sha(data),'transmission_context':manifest['transmission_context']},ensure_ascii=False))
if __name__=='__main__':main()
