#define _GNU_SOURCE
#define main original_main
#include "common.h"
#include <fcntl.h>
static uint64_t filehash(FILE*f,long n){uint64_t h=1469598103934665603ULL;rewind(f);for(long i=0;i<n;i++){int c=fgetc(f);if(c==EOF){fprintf(stderr,"checkpoint short\n");exit(1);}h=(h^(unsigned)c)*1099511628211ULL;}return h;}
static int terra_finish(FILE*f){if(fflush(f))return 0;long n=ftell(f);uint64_t h=filehash(f,n);fseek(f,0,SEEK_END);if(fwrite(&h,8,1,f)!=1||fflush(f)||fsync(fileno(f)))return 0;return fclose(f)==0;}
static int terra_integrity(const char*p){FILE*f=fopen(p,"rb");if(!f)return 0;fseek(f,0,SEEK_END);long n=ftell(f);if(n<8){fclose(f);return 0;}fseek(f,n-8,SEEK_SET);uint64_t want;if(fread(&want,8,1,f)!=1){fclose(f);return 0;}uint64_t got=filehash(f,n-8);fclose(f);return want==got;}
static int adapter_save_extra(FILE*,void*);
static int adapter_load_extra(FILE*,void*);
static int adapter_plugin_index(const char*);
#define TERRA_SAVE_EXTRA(f,T) adapter_save_extra(f,T)
#define TERRA_LOAD_EXTRA(f,T) adapter_load_extra(f,T)
static void terra_emissions(void *);
#define TERRA_AFTER_WORLD(I) terra_emissions(I)
#include "frozen/terrarium.c"
#undef main
static int adapter_plugin_index(const char*n){const char*names[]={"osmo.so","competition.so","heritage.so","memoire.so","renaissance.so","lecture.so","trace.so"};for(int i=0;i<7;i++)if(!strcmp(n,names[i]))return i;return -1;}
static int adapter_save_extra(FILE*f,void*p){Terra*T=p;int ok=fwrite(T->adapter_plugin_state,sizeof T->adapter_plugin_state,1,f)==1;for(int k=0;k<T->P.K&&ok;k++)ok=fwrite(T->I[k].mark,sizeof T->I[k].mark,1,f)==1;return ok;}
static int adapter_load_extra(FILE*f,void*p){Terra*T=p;int ok=fread(T->adapter_plugin_state,sizeof T->adapter_plugin_state,1,f)==1;for(int k=0;k<T->P.K&&ok;k++)ok=fread(T->I[k].mark,sizeof T->I[k].mark,1,f)==1;return ok;}
static void terra_emissions(void*p){Island*I=p;for(int j=0;j<I->w->ncs;j++)if(I->w->cs[j]->emitted>=0)adapter_emit(I->w->cs[j]->emitted);}
static void fail(const char*s){fprintf(stderr,"legacy-terra: %s\n",s);exit(1);}
static void append(Terra*T,const char*file){FILE*f=records(file);Creature c;unsigned n=0;while(read_record(f,&c,sizeof c)){Island*I=&T->I[(n++)%T->P.K];World*w=I->w;if(w->ncs>=CMAX)fail("capacity exceeded");if(c.len<1||c.len>MAX_LEN||c.germ_len<0||c.germ_len>MAX_LEN)fail("invalid native creature");Creature*a=malloc(sizeof c);*a=c;a->id=w->nid++;a->parent=-1;a->x=rng_below(&I->tr,w->cfg.w);a->y=rng_below(&I->tr,w->cfg.h);w->occ[a->y][a->x]++;w->cs[w->ncs++]=a;I->sh[I->nsh++]=FH(a);struct SG*g=&I->sg[I->nsg++];g->species=a->species;g->len=a->germ_len;memcpy(g->dna,a->germ,g->len);}fclose(f);}
static void inspect_native(Terra*T){long born=0,dead=0,emits=0;for(int k=0;k<T->P.K;k++){born+=T->I[k].w->st.born;dead+=T->I[k].w->st.dead;emits+=T->I[k].w->st.emits;}printf("tick=%ld population=%d births=%ld deaths=%ld emits=%ld islands=%d laws=%d capacity=%d hash=%016llx forces=%d stress=%d attention=%d traits=%d depense=%d causales=%d saison=%d tronc=%d\n",T->t,total_pop(T),born,dead,emits,T->P.K,T->nloi,T->P.K*CMAX,(unsigned long long)terra_hash(T),T->P.forces,T->P.stress,T->P.attention,T->traits,T->depense,T->causales,T->saison,T->tronc);}
#define P(field,lo,hi) T.P.field=parameter(d,#field,T.P.field,lo,hi)
int main(int argc,char**argv){if(argc<3)fail("init DIR RAW SEED | step DIR N | inspect DIR | inject DIR RAW | export DIR RAW");const char*d=argv[2];static Terra T;char p[4096];bridge_path(p,d,"state.ck");int init=!strcmp(argv[1],"init");if(init){if(argc<5||!access(p,F_OK))fail("args or existing world");mkdir(d,0700);Params P0=defaults();P0.K=4;P0.pop0=0;P0.seed=strtoull(argv[4],NULL,10);P0.forces=3;P0.stress=P0.attention=1;P0.sample=100;P0.ck_every=1000;P0.osmo=1;terra_new(&T,&P0);T.traits=T.depense=T.causales=T.saison=T.tronc=1;append(&T,argv[3]);trait_arm(&T);for(int k=0;k<T.P.K;k++)T.I[k].w->st.born=T.I[k].w->ncs;}
else if(!terra_integrity(p)||!load(&T,p)){fail("checkpoint load/integrity failed");}
snprintf(T.dir,sizeof T.dir,"%s",d);
if(init){if(!open_outputs(&T,0)||!save(&T))fail("initial save");}
else if(!strcmp(argv[1],"step")){if(argc<4)fail("args");P(regrow,0,10000);T.P.regrow=parameter(d,"food_regrow",T.P.regrow,0,10000);P(mut,0,1);T.P.mut=parameter(d,"mut_rate",T.P.mut,0,1);P(dup,0,1);T.P.dup=parameter(d,"dup_rate",T.P.dup,0,1);P(max_pop,1,4096);P(forces,0,3);P(stress,0,1);P(attention,0,1);P(osmo,0,32);P(season_amp,0,1);P(season_period,1,10000000);P(migrate,0,10000000);
T.depense=parameter(d,"depense",T.depense,0,1);T.causales=parameter(d,"causales",T.causales,0,1);T.saison=parameter(d,"saison",T.saison,0,1);T.tronc=parameter(d,"tronc",T.tronc,0,1);
unsigned char b;size_t got=stimulus(d,&b,1);for(int k=0;k<T.P.K;k++){World*w=T.I[k].w;w->cfg.food_regrow=T.P.regrow;w->cfg.mut_rate=T.P.mut;w->cfg.dup_rate=T.P.dup;w->cfg.max_pop=T.P.max_pop;w->cfg.split_energy=parameter(d,"split_energy",w->cfg.split_energy,1,1000000);w->cfg.ops_per_tick=parameter(d,"ops_per_tick",w->cfg.ops_per_tick,1,4096);w->cfg.regs=parameter(d,"regs",w->cfg.regs,0,1);w->cfg.germline=parameter(d,"germline",w->cfg.germline,0,1);w->cfg.attack_steal=parameter(d,"attack_steal",w->cfg.attack_steal,0,1000000);if(got)T.I[k].env=b&15;}
if(!open_outputs(&T,1)){fail("outputs");}
begin_emit(d);if(total_pop(&T)&&terra_run(&T,T.t+strtoul(argv[3],NULL,10),1))fail("step");end_emit();if(!save(&T))fail("save");}
else if(!strcmp(argv[1],"inject")){if(argc<4)fail("args");append(&T,argv[3]);if(!save(&T))fail("save");}
else if(!strcmp(argv[1],"export")){if(argc<4)fail("args");FILE*f=fopen(argv[3],"wb");if(!f)fail("export open");for(int k=0;k<T.P.K;k++)for(int j=0;j<T.I[k].w->ncs;j++)if(fwrite(T.I[k].w->cs[j],sizeof(Creature),1,f)!=1)fail("export write");durable(f);}
else if(strcmp(argv[1],"inspect")){fail("command");}
inspect_native(&T);terra_free(&T);return 0;}
