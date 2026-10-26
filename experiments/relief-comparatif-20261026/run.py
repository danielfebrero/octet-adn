#!/usr/bin/env python3
"""Bounded, sequential exact-ELF experiment. Never mounts living data writable."""
import argparse, datetime, hashlib, json, os, re, shutil, subprocess, time
from pathlib import Path
ROOT = Path(__file__).resolve().parent
REPO = ROOT.parent.parent
DATA = ROOT/'data'
ELF = REPO/'experiments/relief-20261025/build-005/relief'
LIVE = REPO/'experiments/vie-relief-20261025'
EXPECTED_ELF = 'c6f68f7e7ad5388f3c9b6603cc885ccaaa225c34a344e39195fe4c12be649480'
IMAGE = 'sha256:7c7b2c966bc9ee8cedfeef67e0e279108992c77681fa595db4a9d65c06ccc587'
HOST_REPO = Path('/data/eternalgrok-grok-workspace/produits/octet-adn')
ARMS = ('control','unlock','redistribute')
HORIZONS = (600,4096,8192)
MAX_BYTES = 2*1024**3
MAX_TICKS = 60000
MAX_WALL = 7200
TOOL = ROOT/'build/state_tool'

def utc(): return datetime.datetime.now(datetime.timezone.utc).isoformat()
def sha(path):
    h=hashlib.sha256()
    with Path(path).open('rb') as f:
        while b:=f.read(1024*1024): h.update(b)
    return h.hexdigest()
def put(path,value):
    path=Path(path); tmp=path.with_suffix(path.suffix+'.tmp')
    tmp.write_text(json.dumps(value,ensure_ascii=False,indent=2)+'\n'); tmp.replace(path)
def inspect(path):
    text=subprocess.check_output([str(ELF),'inspect',str(path)],text=True)
    return {'text':text,**{k:int(v) if k!='rng' else v for k,v in re.findall(r'\b(tick|population|matter|initial_matter|births|deaths|rng)=([0-9a-f]+)',text)}}
def budget():
    size=sum(p.stat().st_size for p in DATA.rglob('*') if p.is_file())
    if size>MAX_BYTES: raise RuntimeError(f'disk budget exceeded {size}')
    if shutil.disk_usage(ROOT).free<100*1024**3: raise RuntimeError('free disk below100GiB')
    return size

def capture(window):
    dest=DATA/window/'base'; dest.mkdir(parents=True,exist_ok=False)
    with (LIVE/'state.ck').open('rb') as f:
        st=os.fstat(f.fileno()); b=f.read(); assert len(b)==st.st_size
    (dest/'state.ck').write_bytes(b)
    meta={'captured_at_utc':utc(),'method':'one fd on atomic checkpoint, read-only source','source':str(LIVE/'state.ck'),'source_inode':st.st_ino,'source_mtime_ns':st.st_mtime_ns,'sha256':sha(dest/'state.ck'),'size':len(b),'elf_sha256':sha(ELF),'commit':subprocess.check_output(['git','-c','safe.directory='+str(REPO),'rev-parse','HEAD'],cwd=REPO,text=True).strip(),'inspect':inspect(dest)}
    put(dest.parent/'capture.json',meta)
    print('CAPTURE',window,meta['inspect']['tick'],flush=True)
    return meta

def maybe_capture_second():
    if (DATA/'window2/base/state.ck').exists(): return
    # A validated reader sees a coherent atomic checkpoint even during live run.
    if inspect(LIVE)['tick']>=737400+4096:
        capture('window2')

def export(directory,prefix):
    subprocess.run([str(TOOL),'export',str(directory),str(prefix)],check=True,capture_output=True,text=True)

def run_segment(directory,ticks,manifest):
    assert sha(ELF)==EXPECTED_ELF
    assert directory.resolve().is_relative_to(DATA.resolve())
    assert directory.resolve()!=LIVE.resolve()
    assert ticks>0 and manifest['simulated_ticks']+ticks<=MAX_TICKS
    assert time.monotonic()-START<MAX_WALL
    budget()
    name='relief-comparatif-b8e25e22'
    host_dir=HOST_REPO/directory.relative_to(REPO)
    cmd=['docker','run','--rm','--name',name,'--restart=no','--cpus=0.75','--cpu-shares=128','--memory=256m','--memory-swap=256m','--pids-limit=16','--network=none','--read-only','--cap-drop=ALL','--security-opt=no-new-privileges','--user',str(os.getuid())+':'+str(os.getgid()),'--mount','type=bind,src='+str(HOST_REPO/ELF.relative_to(REPO))+',dst=/relief,readonly','--mount','type=bind,src='+str(host_dir)+',dst=/trial','--entrypoint','/relief',IMAGE,'run','/trial','--ticks',str(ticks),'--tick-ms','0']
    began=utc();t0=time.monotonic()
    try:
        p=subprocess.run(cmd,capture_output=True,text=True,timeout=min(3000,MAX_WALL-(time.monotonic()-START)))
    except BaseException:
        subprocess.run(['docker','rm','-f',name],capture_output=True)
        raise
    record={'directory':str(directory.relative_to(ROOT)),'began_at':began,'ended_at':utc(),'ticks_requested':ticks,'wall_seconds':time.monotonic()-t0,'returncode':p.returncode,'command':cmd,'stdout':p.stdout,'stderr':p.stderr,'final_state_sha256':sha(directory/'state.ck')}
    manifest['runs'].append(record);manifest['simulated_ticks']+=ticks
    manifest['data_bytes']=budget();put(DATA/'run-manifest.json',manifest)
    if p.returncode: raise RuntimeError(record)
    print('RUN',directory.parent.name,directory.name,inspect(directory)['tick'],round(record['wall_seconds'],2),'s',flush=True)
    maybe_capture_second()

def main():
    parser=argparse.ArgumentParser();parser.add_argument('--execute',action='store_true',required=True);parser.parse_args()
    assert sha(ELF)==EXPECTED_ELF
    assert not (DATA/'run-manifest.json').exists(),'refuse overwrite; use fresh experiment'
    manifest={'began_at':utc(),'elf_sha256':sha(ELF),'source_sha256':sha(REPO/'experiments/relief-20261025/relief.c'),'tool_sha256':sha(TOOL),'runner_sha256':sha(__file__),'protocol_sha256':sha(ROOT/'PROTOCOLE.md'),'simulated_ticks':600,'technical_replay_ticks':600,'limits':{'cpus':0.75,'bytes':MAX_BYTES,'ticks':MAX_TICKS,'wall_seconds':MAX_WALL},'runs':[],'live_before':inspect(LIVE)}
    put(DATA/'run-manifest.json',manifest)
    for wi in ('window1','window2'):
        while not (DATA/wi/'base/state.ck').exists():
            maybe_capture_second()
            if (DATA/wi/'base/state.ck').exists(): break
            assert time.monotonic()-START<MAX_WALL
            print('WAIT natural live checkpoint for second window',flush=True);time.sleep(20)
        window=DATA/wi; base=window/'base'; export(base,base/'baseline')
        # Audit all manipulations before starting this window's comparisons.
        for arm in ARMS:
            dest=window/arm
            p=subprocess.run([str(TOOL),'transform',str(base),str(dest),arm],check=True,capture_output=True,text=True)
            (window/(arm+'-transform.stdout')).write_text(p.stdout)
            (window/(arm+'-transform.stderr')).write_text(p.stderr)
            export(dest,dest/'initial')
            shutil.copyfile(dest/'state.ck',dest/'initial.ck')
            if arm=='control': assert sha(dest/'state.ck')==sha(base/'state.ck')
        for arm in ARMS:
            directory=window/arm;previous=0
            for h in HORIZONS:
                if inspect(directory)['population']:
                    run_segment(directory,h-previous,manifest)
                export(directory,directory/f'at{h}')
                shutil.copyfile(directory/'state.ck',directory/f'at{h}.ck')
                previous=h
            manifest.setdefault('completed',[]).append(wi+'/'+arm)
            put(DATA/'run-manifest.json',manifest)
    manifest.update(ended_at=utc(),live_after=inspect(LIVE),wall_seconds=time.monotonic()-START,data_bytes=budget())
    assert manifest['live_after']['tick']>manifest['live_before']['tick']
    put(DATA/'run-manifest.json',manifest)
    print('ALL RUNS COMPLETE',manifest['simulated_ticks'],manifest['data_bytes'],flush=True)

if __name__=='__main__':
    START=time.monotonic();main()
