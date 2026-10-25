/* Banc M1/M2 isolé. Lire choix/README.md et PLAN-VALIDE.md avant mesure.
 * La VM est liée telle quelle ; ce fichier fournit un autre milieu World.
 * Aucun état H/C n'est placé dans Creature par le banc. */
#include "octet.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>

enum { EPISODES=256, SEEDS=32, ENERGY=100000 };
typedef struct { int h,c1,c2,k; } Episode;
typedef struct { int action,reads,cost,reward,metab,dead; } Outcome;
typedef struct {
    World *w; int phase,decision,payment,c1,c2,price,blind,scramble,ablate;
    int charged[2],delivered[2]; Outcome out;
} Channel;
static Channel chan;

/* L'éprouvette n'a ni ressource libre, ni trace physique, ni reproduction.
 * Les opcodes de ces actions restent exécutés mais n'ont pas d'effet ici. */
void world_move(World *w,Creature *c) { (void)w;(void)c; }
void world_eat(World *w,Creature *c) { (void)w;(void)c; }
void world_split(World *w,Creature *c) { (void)w;(void)c; }
void world_attack(World *w,Creature *c) { (void)w;(void)c; }
int food_ahead(const World *w,const Creature *c) { (void)w;(void)c;return 0; }
int neighbors(const World *w,const Creature *c) { (void)w;(void)c;return 0; }
int history_at(const World *w,int k) { (void)w;(void)k;return 0; }
void world_emit(World *w,Creature *c,int v) {
    (void)w;(void)c;
    if(chan.phase==chan.decision && chan.out.action<0) chan.out.action=v&1;
}
static void watch(Creature *c,uint8_t op) {
    /* Appelé APRÈS décodage par vm_run : un immédiat 0x12 n'est pas lu. */
    chan.w->stimulus=0;
    if(op!=OP_SENSE_STIM && op!=OP_NEURAL) return;
    if(chan.phase<0 || chan.phase>=(chan.decision==5?1:2)) return;
    int i=chan.phase;
    chan.out.reads++;
    if(!chan.charged[i]) {
        chan.charged[i]=1; chan.out.cost+=chan.price;c->e-=chan.price;
    }
    if(!chan.blind) chan.w->stimulus=i?chan.c2:chan.c1;
    chan.delivered[i]=chan.w->stimulus;
}
static uint64_t address(int task,int seed,int ep,int stream) {
    return UINT64_C(0xC401202610250000) ^ ((uint64_t)task<<48) ^
           ((uint64_t)seed<<32) ^ ((uint64_t)ep<<8) ^ (uint64_t)stream;
}
static Episode episode(int task,int seed,int ep,int split) {
    Rng r;rng_seed(&r,address(task,seed,ep,split+1));Episode e={0};
    e.k=(int)rng_below(&r,256);
    if(task==1) {
        e.h=(int)rng_below(&r,2);
        e.c1=e.h ^ (rng_below(&r,4)==0);
    } else {
        do {e.c1=(int)rng_below(&r,20);e.c2=(int)rng_below(&r,20);}
        while (((e.c1+e.c2)%5==0)!=(split==1));
        e.h=(e.c1^e.c2)&1;
    }
    return e;
}
static Creature init_creature(const PoolEntry *g) {
    Creature c={0};c.e=ENERGY;c.id=1;c.parent=-1;c.emitted=-1;
    c.len=c.germ_len=g->len;
    memcpy(c.dna,g->dna,(size_t)g->len);memcpy(c.germ,g->dna,(size_t)g->len);
    return c;
}
static void clear_retention(Creature *c,const PoolEntry *g) {
    /* Énergie et âge conservés : ni l'un ni l'autre ne dépend de la VALEUR
     * de l'indice avant récompense. Tous les supports calculatoires réinitialisés. */
    int energy=c->e,age=c->age;*c=init_creature(g);c->e=energy;c->age=age;
}
static Outcome run_episode(World *w,Creature *c,const PoolEntry *g,Episode e,
                           int task,int price,int blind,int scramble,int ablate,
                           int seed,int ep,int split,FILE *log,const char *arm) {
    int decision=task==1?5:6,payment=decision+4;
    chan=(Channel){.w=w,.decision=decision,.payment=payment,.c1=e.c1,.c2=e.c2,
                   .price=price,.blind=blind,.scramble=scramble,.ablate=ablate,
                   .delivered={-1,-1},.out={.action=-1}};
    Rng r;rng_seed(&r,address(task,seed,ep,64+split));
    if(scramble==1)chan.c1=task==1?(int)rng_below(&r,2):(int)rng_below(&r,20);
    if(scramble==2)chan.c2=(int)rng_below(&r,20);
    memset(w->food,0,sizeof w->food);memset(w->occ,0,sizeof w->occ);
    memset(w->hist,0,sizeof w->hist);w->hist_n=w->hist_pos=w->nem=0;
    w->ncs=0;w->nnb=0;c->x=c->y=c->d=0;
    vm_set_watch(watch);
    for(int t=0;t<=payment;t++) {
        chan.phase=t;w->stimulus=0;w->t=(long)ep*(payment+1)+t;
        if(ablate && t==(task==1?1:2))clear_retention(c,g);
        int alive=c->e>0;
        if(alive) { vm_run(c,w);chan.out.metab++; }
        if(t==payment && c->e>0 && chan.out.action==e.h) {
            c->e+=20;chan.out.reward=20;
        }
        assert(!chan.out.reward || t==payment);
        if(log)fprintf(log,"%d\t%d\t%d\t%s\t%d\t%d\t%ld\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%016llx\t1\t1\n",
            task,seed,split,arm,ep,t,w->t,e.h,e.c1,e.c2,e.k,
            chan.out.reads,chan.out.cost,chan.out.action,chan.out.reward,c->age,c->e,c->e<=0,chan.out.metab,
            chan.delivered[0],chan.delivered[1],(unsigned long long)fnv1a(g->dna,(size_t)g->len,FNV_INIT));
    }
    chan.out.dead=c->e<=0;vm_set_watch(NULL);return chan.out;
}
static PoolEntry program(int task,int informed,int mode) {
    PoolEntry g={0};
    /* Contrôle écrit, étiqueté T8 ; aucun octet ajouté au candidat. */
    int n=0,total=task==1?10:11,decision=task==1?5:6;
    for(int t=0;t<total;t++) {
        if(informed && t==0) {
            g.dna[n++]=OP_SENSE_STIM;
            if(task==2)g.dna[n++]=OP_MOV_BA;
        }
        if(informed && task==2 && t==1) {g.dna[n++]=OP_SENSE_STIM;g.dna[n++]=OP_XOR;}
        if(t==decision) {
            if(!informed || informed==2) {g.dna[n++]=OP_LOADI;g.dna[n++]=(uint8_t)mode;}
            g.dna[n++]=OP_EMIT;
        }
        g.dna[n++]=OP_YIELD;
    }
    g.dna[n++]=OP_LOOP;g.len=n;return g;
}
static PoolEntry load_genome(const char *path) {
    PoolEntry g={0};FILE *f=fopen(path,"rb");if(!f){perror(path);exit(2);}
    g.len=(int)fread(g.dna,1,MAX_LEN,f);
    if(!g.len || fgetc(f)!=EOF || ferror(f)){fputs("Génome vide ou trop long\n",stderr);exit(2);}
    fclose(f);return g;
}
static World *arena(Rng *rng) {
    World *w=calloc(1,sizeof *w);if(!w)exit(2);
    w->cfg=(WorldConfig){.w=1,.h=1,.max_pop=1,.ops_per_tick=16,.regs=1};
    w->rng=rng;return w;
}
static void selftest(void) {
    Rng rng;rng_seed(&rng,1);World *w=arena(&rng);
    for(int task=1;task<=2;task++) {
        PoolEntry g=program(task,1,0);
        for(int a=0;a<2;a++)for(int b=0;b<2;b++) {
            Episode e={.h=task==1?a:a^b,.c1=a,.c2=b};
            Creature c=init_creature(&g);
            Outcome x=run_episode(w,&c,&g,e,task,1,0,0,0,0,0,0,NULL,"test");
            assert(x.action==e.h && x.reward==20 && x.cost==task);
            assert(x.metab==(task==1?10:11));
            assert(c.e==ENERGY+20-x.cost-x.metab);
        }
    }
    /* Reprise à une frontière d'épisode : état entier, pas seulement ADN. */
    PoolEntry g=program(1,1,0);Creature c=init_creature(&g);
    for(int i=0;i<10;i++)run_episode(w,&c,&g,episode(1,3,i,0),1,1,0,0,0,3,i,0,NULL,"test");
    Creature saved=c,full;
    for(int i=10;i<20;i++)run_episode(w,&c,&g,episode(1,3,i,0),1,1,0,0,0,3,i,0,NULL,"test");
    full=c;c=saved;
    for(int i=10;i<20;i++)run_episode(w,&c,&g,episode(1,3,i,0),1,1,0,0,0,3,i,0,NULL,"test");
    assert(!memcmp(&c,&full,sizeof c));
    /* Registre neutralisé, historique n'expose jamais le stimulus. */
    Creature c0=init_creature(&g),c1=c0;Episode e0={0},e1={.h=1,.c1=1};
    Outcome x=run_episode(w,&c0,&g,e0,1,1,0,0,1,0,0,0,NULL,"test");
    Outcome y=run_episode(w,&c1,&g,e1,1,1,0,0,1,0,0,0,NULL,"test");
    assert(x.action==y.action);
    /* Décodage immédiat + NEURAL : une seule facturation par fenêtre. */
    PoolEntry p={.len=8,.dna={OP_LOADI,OP_SENSE_STIM,OP_NEURAL,0,OP_SENSE_STIM,OP_YIELD,OP_LOOP,0}};
    c=init_creature(&p);
    x=run_episode(w,&c,&p,e1,1,8,0,0,0,0,0,0,NULL,"test");
    assert(x.cost==8 && x.reads==2);
    /* Contrefactuels même état, coût égal, seul C diffère. */
    c0=init_creature(&g);c1=c0;
    x=run_episode(w,&c0,&g,e0,1,1,0,0,0,0,0,0,NULL,"test");
    y=run_episode(w,&c1,&g,e1,1,1,0,0,0,0,0,0,NULL,"test");
    assert(x.action==0 && y.action==1 && x.cost==y.cost);
    /* Équilibre exact de la partition M2, y compris par ligne/colonne. */
    for(int split=0;split<2;split++)for(int i=0;i<20;i++) {
        int row[2]={0},col[2]={0};
        for(int j=0;j<20;j++)if(((i+j)%5==0)==split){row[(i^j)&1]++;col[(j^i)&1]++;}
        assert(row[0]==row[1] && col[0]==col[1]);
    }
    /* Le contrôleur lookup prescrit une action après deux lectures : sa
     * longueur doit aussi être celle utilisée par le pointeur de la VM. */
    g=program(2,2,0);c=init_creature(&g);
    for(int ep=0;ep<256;ep++) {
        Episode e=episode(2,0,ep,0);g=program(2,2,e.h);
        memcpy(c.dna,g.dna,(size_t)g.len);c.len=g.len;
        x=run_episode(w,&c,&g,e,2,1,0,0,0,0,ep,0,NULL,"lookup-test");
        assert(x.action==e.h && x.cost==2 && x.metab==11);
    }
    free(w);puts("selftest OK : VM, coûts, NEURAL, immédiats, rétention, contrefactuels, reprise, partition M2");
}
static int campaign(int task,const char *path,const char *outdir) {
    const char *names[]={"T8","oracle","A","B","alternance","aleatoire","brouille","ablation","cout-haut-informe","cout-haut-rationnel","candidat","candidat-brouille","candidat-ablation","candidat-haut","brouille-second","candidat-brouille-second","lookup","candidat-aveugle"};
    char file[4096];snprintf(file,sizeof file,"%s/episodes.tsv",outdir);FILE *log=fopen(file,"wx");
    snprintf(file,sizeof file,"%s/graines.tsv",outdir);FILE *sum=fopen(file,"wx");
    if(!log||!sum){perror("sorties (répertoire neuf requis)");return 2;}
    fputs("task\tseed\tsplit\tarm\tepisode\tphase\ttick\tH\tC1\tC2\tK\treads\tcost\taction\treward\tage\tenergy\tdead\tmetabolism\tread_C1\tread_C2\tgenome_fnv\tindividual\tlineage\n",log);
    fputs("task\tseed\tsplit\tarm\tcorrect\tnet\tconsults\tomissions\tdeaths\tmetabolism\n",sum);
    PoolEntry candidate=load_genome(path);
    for(int seed=0;seed<SEEDS;seed++)
    for(int split=0;split<(task==1?1:2);split++)for(int arm=0;arm<18;arm++) {
        if(task==1 && (arm==14||arm==15))continue;
        int informed=arm==0||arm==1||arm==6||arm==7||arm==8||arm==14;
        int is_candidate=(arm>=10 && arm<=13) || arm==15 || arm==17;
        PoolEntry g=is_candidate?candidate:program(task,informed,arm==3);
        Creature c=init_creature(&g);Rng rng;rng_seed(&rng,address(task,seed,0,200));World *w=arena(&rng);
        int correct=0,net=0,consults=0,omissions=0,deaths=0,metabolism=0;
        for(int ep=0;ep<EPISODES;ep++) {
            Episode e=episode(task,seed,ep,split);
            if(arm==4||arm==5||arm==16) {
                int action=ep&1;
                if(arm==5){Rng rr;rng_seed(&rr,address(task,seed,ep,201));action=(int)rng_below(&rr,2);}
                /* Lookup exhaustive des seuls tuples d'apprentissage M2.
                 * K ne prédit pas H dans M1 : repli aveugle à zéro. */
                if(arm==16)action=task==2 && split==0?((e.c1^e.c2)&1):0;
                g=program(task,arm==16 && task==2?2:0,action);
                memcpy(c.dna,g.dna,(size_t)g.len);
                c.len=g.len;
            }
            int price=arm==8||arm==9||arm==13?8:1;
            int scramble=arm==6||arm==11?1:arm==14||arm==15?2:0;
            Outcome o=run_episode(w,&c,&g,e,task,price,arm==17,scramble,arm==7||arm==12,seed,ep,split,log,names[arm]);
            correct+=o.action==e.h;net+=o.reward-o.cost;consults+=o.reads>0;
            omissions+=o.action<0;deaths+=o.dead;metabolism+=o.metab;
        }
        fprintf(sum,"%d\t%d\t%d\t%s\t%.9f\t%.9f\t%d\t%d\t%d\t%d\n",task,seed,split,names[arm],correct/256.,net/256.,consults,omissions,deaths,metabolism);
        free(w);
    }
    if(fclose(log)||fclose(sum)){perror("écriture résultats");return 2;}return 0;
}
enum { ECO_CAP=160 };
typedef struct {
    uint64_t magic,step,births,deaths,next_id,seed;
    long series_offset,events_offset;
    int n;Rng rng;Creature cs[ECO_CAP];PoolEntry gs[ECO_CAP];
} Ecology;
static volatile sig_atomic_t stopping;
static void stop_handler(int sig){(void)sig;stopping=1;}
static FILE *eco_file(const char *dir,const char *name,const char *mode) {
    char path[4096];snprintf(path,sizeof path,"%s/%s",dir,name);
    FILE *f=fopen(path,mode);if(!f){perror(path);exit(2);}return f;
}
static void save_ecology(const char *dir,Ecology *e,FILE *series,FILE *events) {
    if(fflush(series)||fflush(events)||fsync(fileno(series))||fsync(fileno(events)))exit(2);
    e->series_offset=ftell(series);e->events_offset=ftell(events);
    FILE *f=eco_file(dir,"checkpoint.tmp","wb");
    if(fwrite(e,sizeof *e,1,f)!=1 || fflush(f) || fsync(fileno(f)))exit(2);
    if(fclose(f))exit(2);
    char a[4096],b[4096];snprintf(a,sizeof a,"%s/checkpoint.tmp",dir);
    snprintf(b,sizeof b,"%s/checkpoint.bin",dir);if(rename(a,b))exit(2);
}
static int ecology(const char *dir,uint64_t cycles,int pause_ms) {
    Ecology *e=calloc(1,sizeof *e);if(!e)return 2;
    char path[4096];snprintf(path,sizeof path,"%s/checkpoint.bin",dir);
    FILE *f=fopen(path,"rb");int resume=f!=NULL;
    if(resume) {
        if(fread(e,sizeof *e,1,f)!=1 || fgetc(f)!=EOF || e->magic!=UINT64_C(0x43484F4958454331) || e->n<0 || e->n>ECO_CAP)return 2;
        fclose(f);
        for(int i=0;i<e->n;i++)if(e->gs[i].len<1||e->gs[i].len>MAX_LEN||e->cs[i].len!=e->gs[i].len)return 2;
    } else {
        e->magic=UINT64_C(0x43484F4958454331);e->seed=20261025;rng_seed(&e->rng,e->seed);
        const char *paths[]={"seeds/general.adn","seeds/echo.adn","seeds/neural.adn","seeds/forager.adn"};
        for(int i=0;i<64;i++) {
            e->gs[i]=load_genome(paths[i%4]);e->cs[i]=init_creature(&e->gs[i]);
            e->cs[i].e=160;e->cs[i].id=(int)e->next_id++;e->cs[i].species=i%4;
        }
        e->n=64;
    }
    FILE *series=eco_file(dir,"series.tsv",resume?"r+":"wx");
    FILE *events=eco_file(dir,"events.tsv",resume?"r+":"wx");
    if(resume) {
        if(ftruncate(fileno(series),e->series_offset)||ftruncate(fileno(events),e->events_offset))return 2;
        fseek(series,0,SEEK_END);fseek(events,0,SEEK_END);
    } else {
        fputs("t\tepisode\tpop\tgenotypes\tbirths\tdeaths\tcorrect\tconsults\tomissions\tmean_energy\n",series);
        fputs("episode\tevent\tid\tparent\tgenome_fnv\n",events);
        save_ecology(dir,e,series,events);
    }
    signal(SIGTERM,stop_handler);signal(SIGINT,stop_handler);
    World *w=arena(&e->rng);uint64_t count=0;
    while(e->n && !stopping && (!cycles||count<cycles)) {
        int n0=e->n,alive=0,nb=0,correct=0,consults=0,omissions=0;
        Creature children[ECO_CAP];PoolEntry genomes[ECO_CAP];
        for(int i=0;i<n0;i++) {
            Creature c=e->cs[i];PoolEntry g=e->gs[i];
            /* L'environnement est adressé par épisode et individu : aucun
             * accès du candidat ne fait avancer le flux du milieu. */
            Episode ep=episode(1,c.id,(int)e->step,0);
            Outcome o=run_episode(w,&c,&g,ep,1,1,0,0,0,c.id,(int)e->step,0,NULL,"ecologie");
            correct+=o.action==ep.h;consults+=o.reads>0;omissions+=o.action<0;
            if(c.e<=0) {
                e->deaths++;fprintf(events,"%llu\tdeath\t%d\t%d\t%016llx\n",(unsigned long long)e->step,c.id,c.parent,(unsigned long long)fnv1a(g.dna,(size_t)g.len,FNV_INIT));
                continue;
            }
            if(c.e>=120 && n0+nb<ECO_CAP) {
                c.e-=60;PoolEntry child=g;
                for(int k=0;k<child.len;k++)if(rng_below(&e->rng,100)==0)child.dna[k]=(uint8_t)rng_below(&e->rng,256);
                genomes[nb]=child;children[nb]=init_creature(&child);
                children[nb].e=60;children[nb].id=(int)e->next_id++;
                children[nb].parent=c.id;children[nb].species=c.species;
                fprintf(events,"%llu\tbirth\t%d\t%d\t%016llx\n",(unsigned long long)e->step,children[nb].id,c.id,(unsigned long long)fnv1a(child.dna,(size_t)child.len,FNV_INIT));
                nb++;e->births++;
            }
            e->cs[alive]=c;e->gs[alive++]=g;
        }
        for(int i=0;i<nb;i++){e->cs[alive]=children[i];e->gs[alive++]=genomes[i];}
        e->n=alive;e->step++;count++;
        int types=0;long energy=0;
        for(int i=0;i<e->n;i++) {
            energy+=e->cs[i].e;int seen=0;
            for(int j=0;j<i;j++)if(e->gs[i].len==e->gs[j].len && !memcmp(e->gs[i].dna,e->gs[j].dna,(size_t)e->gs[i].len)){seen=1;break;}
            types+=!seen;
        }
        fprintf(series,"%llu\t%llu\t%d\t%d\t%llu\t%llu\t%d\t%d\t%d\t%.6f\n",
            (unsigned long long)e->step*10,(unsigned long long)e->step,e->n,types,
            (unsigned long long)e->births,(unsigned long long)e->deaths,correct,consults,omissions,e->n?(double)energy/e->n:0);
        save_ecology(dir,e,series,events);
        if(pause_ms>0){struct timespec pause={pause_ms/1000,(pause_ms%1000)*1000000L};nanosleep(&pause,NULL);}
    }
    save_ecology(dir,e,series,events);fclose(series);fclose(events);free(w);
    printf("ecologie episode=%llu pop=%d births=%llu deaths=%llu\n",(unsigned long long)e->step,e->n,(unsigned long long)e->births,(unsigned long long)e->deaths);
    free(e);return 0;
}
static int export_ecology(const char *dir,const char *out) {
    FILE *f=eco_file(dir,"checkpoint.bin","rb");Ecology *e=calloc(1,sizeof *e);
    if(!e || fread(e,sizeof *e,1,f)!=1 || e->magic!=UINT64_C(0x43484F4958454331) || e->n<0 || e->n>ECO_CAP)return 2;
    fclose(f);FILE *index=eco_file(out,"genomes.tsv","wx");
    fputs("episode\tid\tparent\tenergy\tgenome\n",index);
    for(int i=0;i<e->n;i++) {
        PoolEntry *g=&e->gs[i];if(g->len<1||g->len>MAX_LEN)return 2;
        char name[100];snprintf(name,sizeof name,"individu-%d.adn",e->cs[i].id);
        f=eco_file(out,name,"wx");if(fwrite(g->dna,1,(size_t)g->len,f)!=(size_t)g->len)return 2;fclose(f);
        fprintf(index,"%llu\t%d\t%d\t%d\t%s\n",(unsigned long long)e->step,e->cs[i].id,e->cs[i].parent,e->cs[i].e,name);
    }
    fclose(index);free(e);return 0;
}
int main(int argc,char **argv) {
    if(argc==2 && !strcmp(argv[1],"selftest")){selftest();return 0;}
    if(argc==5 && !strcmp(argv[1],"run") && (!strcmp(argv[2],"M1")||!strcmp(argv[2],"M2")))
        return campaign(argv[2][1]-'0',argv[3],argv[4]);
    if(argc==5 && !strcmp(argv[1],"ecology"))return ecology(argv[2],strtoull(argv[3],NULL,10),atoi(argv[4]));
    if(argc==4 && !strcmp(argv[1],"export"))return export_ecology(argv[2],argv[3]);
    fprintf(stderr,"usage: %s selftest | run M1|M2 candidat.adn répertoire-neuf | ecology répertoire cycles(0=infini) pause_ms | export répertoire sortie-neuve\n",argv[0]);return 2;
}
