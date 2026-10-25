/* Diagnostics bornés, sans run vivant, /proc ni fichier d'état.
 * CMAX est le plafond réel, pas un petit plafond de fixture. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "rendezvous.h"
#include "naissance.h"

static void gates(void) {
    puts("# portes : même créature avant chaque vm_run ; monde sans rendez-vous");
    puts("seed\tstatique_initial\ttick_initial\tstatique_64ticks\ttick_64ticks\tderive_statique_61\tderive_tick_61");
    int total_si=0, total_bi=0, total_s=0, total_b=0, total_ds=0, total_db=0;
    for (int i=0; i<N_SEEDS; i++) {
        const SeedBlob *seed=seed_at(i); PoolEntry g={.len=seed->len};
        memcpy(g.dna,seed->p,(size_t)seed->len);
        Rng r; rng_seed(&r,996+(uint64_t)i);
        WorldConfig cfg=cfg_default(); cfg.max_pop=1; cfg.food_regrow=0;
        World *w=world_new(&g,1,&r,&cfg,NULL);
        memset(w->food,14,sizeof w->food);
        int si=rendezvous_capable(w,w->cs[0],0), bi=rendezvous_capable(w,w->cs[0],1);
        int st=0,bt=0,ds=0,db=0;
        for(int t=0;t<64;t++) {
            if(w->ncs) { st+=rendezvous_capable(w,w->cs[0],0); bt+=rendezvous_capable(w,w->cs[0],1); }
            world_step(w);
        }
        world_free(w);
        Rng mutation; rng_seed(&mutation,0x996D0000ULL+(uint64_t)i);
        for(int generation=0;generation<=60;generation++) {
            rng_seed(&r,996+(uint64_t)i);
            w=world_new(&g,1,&r,&cfg,NULL); memset(w->food,14,sizeof w->food);
            ds+=rendezvous_capable(w,w->cs[0],0); db+=rendezvous_capable(w,w->cs[0],1);
            world_free(w);
            if(generation<60) { uint8_t next[MAX_LEN+16]; int n=mutate(g.dna,g.len,&mutation,.02,.05,next);
                g.len=n; memcpy(g.dna,next,(size_t)n); }
        }
        printf("%s\t%d\t%d\t%d\t%d\t%d\t%d\n",seed->name,si,bi,st,bt,ds,db);
        total_si+=si;total_bi+=bi;total_s+=st;total_b+=bt;total_ds+=ds;total_db+=db;
    }
    printf("TOTAL/%d\t%d\t%d\t%d\t%d\t%d\t%d\n",N_SEEDS,total_si,total_bi,total_s,total_b,total_ds,total_db);
}

static World *dense(Rng *r,int n,int local_full,int hole) {
    World *w=xcalloc(1,sizeof *w);w->cfg=cfg_default();w->rng=r;
    w->cfg.w=GRID_W;w->cfg.h=GRID_H;w->cfg.max_pop=CMAX;
    uint8_t dna[MIN_LEN];memset(dna,0xff,sizeof dna);dna[0]=OP_YIELD;
    for(int i=0;i<n;i++) {
        int x=10,y=10;
        if(i==0){x=1;y=1;}
        if(local_full && i>=1 && i<=8) {
            static const int dx[]={0,1,2,0,2,0,1,2},dy[]={0,0,0,1,1,2,2,2};
            x=dx[i-1];y=dy[i-1];
            if(hole && i==1){x=10;y=10;}
        }
        w->cs[w->ncs++]=creature_new(x,y,dna,MIN_LEN,160,i,0,-1);w->occ[y][x]++;
    }
    w->nid=n;w->st.born=n;return w;
}

static int pressure(void) {
    int failed=0;
    puts("# nurserie : 32 graines par condition, un oeuf mûr payé, zéro conception à t=1");
    puts("condition\tgraines\tpopulation_avant\tplafond\teclosions\trefus_cap\trefus_place\tattente\tdepense_nouvelle\tmorts");
    const char *names[]={"plafond_plein","plafond_moins_un","voisinage_plein","une_case_libre"};
    for(int mode=0;mode<4;mode++) {
        long hatched=0,cap=0,place=0,pending=0,paid=0,dead=0;
        int n=CMAX-(mode!=0);
        for(int seed=0;seed<32;seed++) {
            Rng r;rng_seed(&r,(uint64_t)seed+996);
            World *w=dense(&r,n,mode>=2,mode==3);w->t=1;
            NaissanceState *s=xcalloc(1,sizeof *s);naissance_init(s,(uint64_t)seed);
            s->eggs=1;s->npending=1;s->energy_paid=48;s->food_paid=2;
            NaissanceEgg *e=&s->pending[0];e->x=1;e->y=1;e->due=1;e->len=MIN_LEN;e->energy=48;
            memset(e->dna,0xff,MIN_LEN);e->dna[0]=OP_YIELD;e->parent_a=0;e->parent_b=1;
            NaissanceConfig cfg={.enabled=1,.period=16,.cost=24,.food_cost=2,.delay=24,.cap=CMAX};
            naissance_step(w,&cfg,s);
            int hatch_expected=mode==1||mode==3;
            if(s->hatched!=hatch_expected || s->refused_cap!=(mode==0) ||
                s->refused_place!=(mode==2) || s->npending!=!hatch_expected ||
                w->ncs!=n+hatch_expected || s->energy_paid!=48 || w->st.dead || w->ncs>CMAX)failed++;
            hatched+=s->hatched;cap+=s->refused_cap;place+=s->refused_place;
            pending+=s->npending;paid+=s->energy_paid-48;dead+=w->st.dead;
            free(s);world_free(w);
        }
        printf("%s\t32\t%d\t%d\t%ld\t%ld\t%ld\t%ld\t%ld\t%ld\n",names[mode],n,CMAX,hatched,cap,place,pending,paid,dead);
    }
    printf("pression_selftest %s\n",failed?"ECHEC":"OK");return !!failed;
}

int main(void) {
    int failures = rendezvous_selftest() | naissance_selftest();
    printf("modules_selftest %s\n", failures ? "ECHEC" : "OK");
    if (failures) return 1;
    gates(); return pressure();
}
