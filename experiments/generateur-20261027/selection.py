#!/usr/bin/env python3
"""Read-only native extraction, auditable multi-criterion selection; never tick a source."""
import argparse,base64,hashlib,json,pathlib,subprocess,collections,shutil
ROOT=pathlib.Path(__file__).resolve().parents[2]
HERE=pathlib.Path(__file__).resolve().parent
ARCH=pathlib.Path('/workspace/backups/octet-adn-20261025-20f62d86/repo')
def sha(b):return hashlib.sha256(b).hexdigest()
COMMON=r'''
static void hx(const void *p,size_t n){const unsigned char*b=p;for(size_t i=0;i<n;i++)printf("%02x",b[i]);}
'''
LEGACY=r'''
static void emit(Creature*c,long tick,int island){
 if(c->e<=0||c->len<=0||c->len>MAX_LEN)return;
 printf("{\"source_id\":%d,\"parent\":%d,\"age\":%d,\"energy\":%d,\"steps\":%d,\"stores\":%d,\"useful_stores\":%d,\"eaten\":%d,\"splits\":%d,\"species\":%d,\"pc\":%d,\"a\":%u,\"b\":%u,\"tick\":%ld,\"island\":%d,\"genome_hex\":\"",c->id,c->parent,c->age,c->e,c->steps,c->stores,c->useful_stores,c->eaten,c->splits,c->species,c->ip,c->a,c->b,tick,island);hx(c->dna,c->len);printf("\",\"memory_hex\":\"\",\"native_hex\":\"");hx(c,sizeof(*c));puts("\"}");}
static Creature*pool[262144];static int islands[262144],n;
static int metric;static int sorter(const void*a,const void*b){const Creature*x=*(Creature**)a,*y=*(Creature**)b; long xx=metric==0?x->age:metric==1?x->e:metric==2?x->useful_stores:metric==3?x->splits:metric==4?x->steps:metric==5?x->stores:metric==6?x->len:x->eaten; long yy=metric==0?y->age:metric==1?y->e:metric==2?y->useful_stores:metric==3?y->splits:metric==4?y->steps:metric==5?y->stores:metric==6?y->len:y->eaten;return xx==yy?x->id-y->id:xx<yy?1:-1;}
static void choose(long tick){for(metric=0;metric<8;metric++){qsort(pool,n,sizeof(*pool),sorter);for(int j=0;j<n&&j<32;j++)emit(pool[j],tick,0);}}
'''
def compile_helpers(build):
    build.mkdir(parents=True,exist_ok=True)
    terra='#define main ignored_main\n#include "'+str(ROOT/'experiments/terrarium.c')+'"\n#undef main\n'+COMMON+LEGACY+r'''
int main(int argc,char**argv){Terra*T=calloc(1,sizeof(*T));if(argc!=2||!load(T,argv[1]))return 3;for(int k=0;k<T->P.K;k++)for(int j=0;j<T->I[k].w->ncs;j++){Creature*c=T->I[k].w->cs[j];if(c->e>0)pool[n++]=c;}choose(T->t);return 0;}
'''
    raison='#define main ignored_main\n#include "'+str(ROOT/'experiments/raison.c')+'"\n#undef main\n'+COMMON+LEGACY+r'''
int main(int argc,char**argv){Eco*E=calloc(1,sizeof(*E));FILE*f=fopen(argv[1],"rb");if(!f||fread(E,sizeof(*E),1,f)!=1||E->magic!=MAGIC)return 3;fclose(f);for(int j=0;j<E->n;j++)pool[n++]=&E->ind[j].c;choose(E->step);return 0;}
'''
    relief='#define main ignored_main\n#include "'+str(ROOT/'experiments/relief-20261025/relief.c')+'"\n#undef main\n'+COMMON+r'''
int main(int argc,char**argv){World w=load(argv[1]);for(unsigned j=0;j<w.c.capacity;j++){Agent*a=&w.agents[j];if(!a->alive)continue;printf("{\"source_id\":%lu,\"parent\":%lu,\"lineage\":%lu,\"age\":%u,\"energy\":%u,\"body\":%u,\"store\":%u,\"pc\":%u,\"dir\":%u,\"tick\":%lu,\"genome_hex\":\"",a->id,a->parent,a->lineage,a->age,a->energy,a->body,a->store,a->pc,a->dir,w.tick);hx(a->genome,GENOME);printf("\",\"memory_hex\":\"");hx(a->memory,MEM);printf("\",\"native_hex\":\"");hx(a,sizeof(*a));puts("\"}");}return 0;}
'''
    trans='#define main ignored_main\n#include "'+str(ROOT/'experiments/transmission-20261026/transmission.c')+'"\n#undef main\n'+COMMON+r'''
int main(int argc,char**argv){directory=argv[1];World*w=load();for(unsigned j=0;j<CAP;j++){Agent*a=&w->agents[j];if(!a->alive)continue;printf("{\"source_id\":%lu,\"parent\":%lu,\"lineage\":%lu,\"age\":%u,\"energy\":%u,\"kind\":%u,\"pc\":%u,\"p\":%u,\"a\":%u,\"b\":%u,\"source\":%u,\"book\":%u,\"outlen\":%u,\"last_mail\":%lu,\"last_reward\":%lu,\"last_request\":%lu,\"tick\":%lu,\"genome_hex\":\"",a->id,a->parent,a->lineage,a->age,a->energy,a->kind,a->pc,a->p,a->a,a->b,a->source,a->book,a->outlen,a->last_mail,a->last_reward,a->last_request,w->tick);hx(a->code,CODE);printf("\",\"memory_hex\":\"");hx(a->ram,RAM);printf("\",\"native_hex\":\"");hx(a,sizeof(*a));puts("\"}");}return 0;}
'''
    core=['src/rng.c','src/isa.c','src/vm.c','src/mutate.c','src/world.c','src/labs.c','src/store.c','src/runner.c','src/seeds.S','src/labs_h.c','src/novelty.c','src/soup.c']
    for name,code in [('legacy',terra),('raison',raison),('relief',relief),('transmission',trans)]:
        source=build/(name+'.c');source.write_text(code)
        cmd=['cc','-O2','-std=gnu11','-I'+str(ROOT/'include'),str(source),'-o',str(build/name)]
        if name=='legacy':cmd +=[str(ROOT/x) for x in core]+['-pthread','-ldl']
        if name=='raison':cmd +=[str(ROOT/x) for x in ['src/vm.c','src/rng.c','src/isa.c']]
        subprocess.run(cmd+['-lm'],check=True,capture_output=True,cwd=ROOT)

def ranked_select(rows,quota):
    # Rotating maxima preserve diverse measurable achievements, then genes/lineages.
    chosen=[];seen=set();genes=collections.Counter();lineages=collections.Counter()
    metrics=['age','energy','useful_stores','splits','steps','stores','eaten','body','store','last_reward','last_mail','last_request']
    queues=[sorted(rows,key=lambda r:(r.get(m,0),r['source_id']),reverse=True) for m in metrics]
    for relaxed in [False,True]:
        while len(chosen)<quota:
            progress=False
            for m,q in zip(metrics,queues):
                for r in q:
                    key=r.get('selection_identity',r['source_id'])
                    if key in seen:continue
                    if not relaxed and genes[r['genome_hex']]:continue
                    seen.add(key);genes[r['genome_hex']]+=1;lineages[r.get('lineage',r.get('species',0))]+=1
                    c=dict(r);c['selection_reason']=f'quota de provenance; classement alterné {m}; valeur={r.get(m,0)}; '+('diversité génomique' if not relaxed else 'complément d’individus authentiques, génotype éventuellement partagé')
                    chosen.append(c);progress=True;break
                if len(chosen)>=quota:break
            if not progress:break
    if len(chosen)!=quota:raise ValueError(f'need {quota}, have {len(chosen)}')
    return chosen

def main():
    ap=argparse.ArgumentParser();ap.add_argument('--output',type=pathlib.Path,default=HERE/'selection-data');a=ap.parse_args();out=a.output;out.mkdir(parents=True,exist_ok=True);build=out/'extractors';compile_helpers(build)
    records=[json.loads(x) for x in pathlib.Path('/workspace/backups/octet-adn-20261025-20f62d86/final-verify.jsonl').read_text().splitlines()]
    sources=[(r['world'],ARCH/r['world']/('checkpoint.bin' if r['kind']=='raison' else 'state.ck'),'raison' if r['kind']=='raison' else 'legacy',10) for r in records if r.get('kind') in ('terra','raison','jardin')]
    sources += [('experiments/vie-relief-20261025',ROOT/'experiments/vie-relief-20261025/state.ck','relief',60),('experiments/vie-transmission-20261026',ROOT/'experiments/vie-transmission-20261026/state.ck','transmission',60)]
    all_creatures=[];source_manifests=[]
    for world,path,isa,quota in sources:
        snapshot=out/'snapshots'/world.replace('/','__');snapshot.mkdir(parents=True,exist_ok=True);snapshot_file=snapshot/path.name
        # Atomic source checkpoints are renamed; opening once supplies one complete immutable generation.
        blob=path.read_bytes();snapshot_file.write_bytes(blob);h=sha(blob)
        cmd=[str(build/isa),str(snapshot if isa in ('relief','transmission') else snapshot_file)]
        rows=[json.loads(x) for x in subprocess.check_output(cmd,text=True).splitlines()]
        rows=list({r['source_id']:r for r in rows}.values())
        picked=ranked_select(rows,quota)
        for r in picked:
            r.pop('island',None)  # exact source checkpoint offset is authoritative across islands
            raw=bytes.fromhex(r.pop('native_hex'));offset=blob.find(raw)
            if offset<0:raise ValueError('native record not found')
            r.update(id=f'{path.parent.name}:{r["source_id"]}',source_world=world,isa='legacy' if isa=='raison' else isa,native_record_base64=base64.b64encode(raw).decode(),native_record_sha256=sha(raw),genome_sha256=sha(bytes.fromhex(r['genome_hex'])),memory_sha256=sha(bytes.fromhex(r['memory_hex'])),native_record_size=len(raw),provenance={'checkpoint_path':str(path),'snapshot_path':str(snapshot_file),'checkpoint_sha256':h,'record_offset':offset,'source_kind':isa})
            all_creatures.append(r)
        source_manifests.append(dict(world=world,isa=isa,checkpoint_sha256=h,candidates=len(rows),selected=len(picked),tick=rows[0]['tick'] if rows else None))
    assert len(all_creatures)==420
    for isa in ('legacy','relief','transmission'):
        raw=b''.join(base64.b64decode(r['native_record_base64']) for r in all_creatures if r['isa']==isa);(out/(isa+'.raw')).write_bytes(raw)
    manifest={'schema':'eg.world.selection.v1','count':420,'selection_policy':'Provenance quotas plus rotating measurable maxima; distinct genomes first, no invented intelligence. All bytes and native VM state exact.','sources':source_manifests,'creatures':all_creatures,'exclusions':[{'world':'choix-e1','reason':'extinct; no surviving creatures; no resurrection'},{'world':'4 quantum worlds','reason':'non-byte quantum state requires separate native quantum adapters; not silently recoded'}]}
    (out/'selection.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2)+'\n')
    print(json.dumps({'count':420,'isas':dict(collections.Counter(r['isa'] for r in all_creatures)),'sources':len(sources),'manifest':str(out/'selection.json')},ensure_ascii=False))
if __name__=='__main__':main()
