#!/usr/bin/env python3
"""Rerun the frozen snapshots only, using exact ELF005; never read the live world.

python3 reproduce.py --inputs data --output data/reproduction-001
The new output must be inside this repository for the configured Docker host
mapping. It must not exist. A repeated deterministic result is a technical
reproduction, never an independent experimental replication.
"""
import argparse
import json
import shutil
import time
from pathlib import Path
import run as engine
import analyze


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--inputs',type=Path,required=True)
    p.add_argument('--output',type=Path,required=True)
    args=p.parse_args();source=args.inputs.resolve(strict=True);dest=args.output.resolve()
    if dest.exists(): p.error('output must be new')
    if not dest.is_relative_to(engine.ROOT) or dest.is_relative_to(engine.LIVE):
        p.error('output must be under the experiment directory, outside the live world')
    for wi in ('window1','window2'):
        assert (source/wi/'base/state.ck').is_file()
    assert engine.sha(engine.ELF)==engine.EXPECTED_ELF
    dest.mkdir(parents=True)
    for wi in ('window1','window2'):
        base=dest/wi/'base';base.mkdir(parents=True)
        shutil.copyfile(source/wi/'base/state.ck',base/'state.ck')
        engine.put(base.parent/'capture.json',{'method':'frozen snapshot technical reproduction','source':str(source/wi/'base/state.ck'),'sha256':engine.sha(base/'state.ck'),'captured_at_utc':engine.utc()})
    engine.DATA=dest;engine.START=time.monotonic()
    manifest={'began_at':engine.utc(),'type':'deterministic technical reproduction','elf_sha256':engine.sha(engine.ELF),'tool_sha256':engine.sha(engine.TOOL),'simulated_ticks':0,'runs':[],'completed':[]}
    for wi in ('window1','window2'):
        window=dest/wi;base=window/'base';engine.export(base,base/'baseline')
        for arm in engine.ARMS:
            trial=window/arm
            engine.subprocess.run([str(engine.TOOL),'transform',str(base),str(trial),arm],check=True)
            engine.export(trial,trial/'initial');shutil.copyfile(trial/'state.ck',trial/'initial.ck')
            previous=0
            for h in engine.HORIZONS:
                if engine.inspect(trial)['population']:
                    engine.run_segment(trial,h-previous,manifest)
                engine.export(trial,trial/f'at{h}')
                shutil.copyfile(trial/'state.ck',trial/f'at{h}.ck');previous=h
            manifest['completed'].append(wi+'/'+arm)
        analyze.analyze(window,list(engine.HORIZONS))
    manifest.update(ended_at=engine.utc(),data_bytes=engine.budget(),wall_seconds=time.monotonic()-engine.START)
    engine.put(dest/'run-manifest.json',manifest)
    print(json.dumps({'output':str(dest),'status':'complete','kind':'technical reproduction'}))

if __name__=='__main__': main()
