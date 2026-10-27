#!/usr/bin/env python3
import sys,pathlib,tempfile,subprocess,shutil,ctypes as C,collections,json,time,resource
BASE=pathlib.Path(__file__).resolve().parent.parent
sys.path.insert(0,str(BASE))
from select_successors import TYPES

def cmd(binary,action,d,*args,ok=True):
 r=subprocess.run([str(BASE/'native/build'/binary),action,str(d),*map(str,args)],capture_output=True,text=True)
 if ok:assert r.returncode==0,(binary,action,r.stderr)
 return r

def scrub(raw,typ,fields):
 b=bytearray(raw)
 for name,ft in typ._fields_:
  if name in fields:b[getattr(typ,name).offset:getattr(typ,name).offset+C.sizeof(ft)]=bytes(C.sizeof(ft))
 return bytes(b)

def chunks(b,n):return [b[i:i+n] for i in range(0,len(b),n)]

def main():
 results={}
 with tempfile.TemporaryDirectory(prefix='native-preserve-') as td:
  td=pathlib.Path(td)
  for binary,isa in [('legacy-terra','legacy'),('relief','relief'),('transmission','transmission')]:
   d=td/isa;sd=BASE/'selection-data';args=[sd/(isa+'.raw'),123456]
   if isa=='transmission':args+=[sd/'snapshots/experiments__vie-transmission-20261026/state.ck']
   timings={};start=time.monotonic();initial=cmd(binary,'init',d,*args).stdout;timings['init']=time.monotonic()-start
   assert cmd(binary,'init',d,*args,ok=False).returncode
   typ=TYPES[isa];size=C.sizeof(typ);raw=td/(isa+'.raw');cmd(binary,'export',d,raw)
   excluded={'legacy':{'id','parent','x','y'},'relief':{'id','parent','lineage','born','pos'},'transmission':{'id','parent','lineage','born','last_mail','last_reward','last_request'}}[isa]
   expected=collections.Counter(scrub(r,typ,excluded) for r in chunks((sd/(isa+'.raw')).read_bytes(),size))
   actual=collections.Counter(scrub(r,typ,excluded) for r in chunks(raw.read_bytes(),size))
   assert expected==actual,f'{isa} native bytes changed outside envelope'
   before=(d/'state.ck').read_bytes();start=time.monotonic();assert cmd(binary,'inspect',d).stdout==initial;timings['inspect']=time.monotonic()-start;assert before==(d/'state.ck').read_bytes()
   other=td/(isa+'-split');shutil.copytree(d,other)
   if isa=='legacy':
    for path in (d,other):shutil.copytree(BASE/'native/build/plugins',path/'lois',dirs_exist_ok=True)
   # Sample-aligned boundaries compare complete selected machine state after resume.
   steps=200 if isa=='legacy' else 30
   cmd(binary,'step',d,steps);cmd(binary,'step',other,steps//2);cmd(binary,'step',other,steps//2)
   ra=td/(isa+'A.raw');rb=td/(isa+'B.raw');cmd(binary,'export',d,ra);cmd(binary,'export',other,rb)
   assert ra.read_bytes()==rb.read_bytes(),f'{isa} split-run native state differs'
   reportA=cmd(binary,'inspect',d).stdout;reportB=cmd(binary,'inspect',other).stdout
   # nloi is a process registry, reconstructed at sampling, not biological state.
   assert reportA==reportB,(reportA,reportB)
   corrupt=td/(isa+'bad');shutil.copytree(d,corrupt);b=bytearray((corrupt/'state.ck').read_bytes());b[-1]^=1;(corrupt/'state.ck').write_bytes(b);assert cmd(binary,'inspect',corrupt,ok=False).returncode
   start=time.monotonic();r=cmd(binary,'step',d,32);timings['step32']=time.monotonic()-start
   assert f'tick={steps+32} ' in r.stdout,r.stdout
   results[isa]={'exact_native_import':True,'excluded_envelope_fields':sorted(excluded),'restart_machine_state_equal':True,'readonly_inspect':True,'corruption_rejected':True,'overwrite_rejected':True,'timings_seconds':timings,'after':r.stdout.strip()}
 print(json.dumps(results,indent=2))
if __name__=='__main__':main()
