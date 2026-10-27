#!/usr/bin/env python3
"""Integration checks against the selected original native records; no live world access."""
import json, pathlib, shutil, subprocess, tempfile, ctypes
BASE=pathlib.Path(__file__).resolve().parent.parent

def run(isa,command,d,*args,ok=True):
    r=subprocess.run([str(BASE/'native/build'/isa),command,str(d),*map(str,args)],text=True,capture_output=True)
    if ok and r.returncode: raise AssertionError(r.stderr)
    return r

def main():
    sd=BASE/'selection-data'
    results={}
    with tempfile.TemporaryDirectory(prefix='octet-native-check-') as td:
        td=pathlib.Path(td)
        for isa,count in [('legacy',300),('relief',60),('transmission',60)]:
            d=td/isa
            args=[sd/(isa+'.raw'),123456]
            if isa=='transmission': args+=[sd/'snapshots/experiments__vie-transmission-20261026/state.ck']
            initial=run(isa,'init',d,*args).stdout
            assert f'population={count}' in initial,initial
            before=(d/'state.ck').read_bytes()
            assert run(isa,'inspect',d).stdout==initial
            assert before==(d/'state.ck').read_bytes()
            other=td/(isa+'-split');shutil.copytree(d,other)
            (d/'stimulus.bin').write_bytes(bytes(range(256)))
            (other/'stimulus.bin').write_bytes(bytes(range(256)))
            run(isa,'step',d,30)
            run(isa,'step',other,15);run(isa,'step',other,15)
            assert (d/'state.ck').read_bytes()==(other/'state.ck').read_bytes(),f'{isa}: restart differs'
            # Every checkpoint implementation must reject payload corruption.
            corrupt=td/(isa+'-corrupt');shutil.copytree(d,corrupt)
            b=bytearray((corrupt/'state.ck').read_bytes());b[-1]^=128;(corrupt/'state.ck').write_bytes(b)
            assert run(isa,'inspect',corrupt,ok=False).returncode!=0
            # Injection imports an exact first raw record, preserving its machine state.
            sizes={'legacy':len((sd/(isa+'.raw')).read_bytes())//count,'relief':len((sd/'relief.raw').read_bytes())//60,'transmission':len((sd/'transmission.raw').read_bytes())//60}
            record=td/(isa+'.record');record.write_bytes((sd/(isa+'.raw')).read_bytes()[:sizes[isa]])
            run(isa,'inject',d,record)
            results[isa]={'initial':initial.strip(),'after':run(isa,'inspect',d).stdout.strip(),'checkpoint_restart_identical':True,'corruption_rejected':True,'injection_ok':True}
    print(json.dumps(results,indent=2))
if __name__=='__main__':main()
