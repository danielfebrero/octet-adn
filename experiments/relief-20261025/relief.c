#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <fcntl.h>
#include <time.h>

/* The binary format deliberately uses this ABI, validated before allocation. */
#define VERSION 1u
#define GENOME 64
#define MEM 8
#define MAX_CELLS (4096u*4096u)
#define MAX_AGENTS 262144u
#define MAGIC UINT64_C(0x315645494c45524f)
static volatile sig_atomic_t stopping;
static void on_signal(int sig) { (void)sig; stopping=1; }
static void die(const char *s) { perror(s); exit(1); }
static void fail(const char *s) { fprintf(stderr,"relief: %s\n",s); exit(1); }
static void *alloc(size_t n,size_t s) { void *p=calloc(n,s); if(!p) die("calloc"); return p; }

typedef struct {
 uint32_t width,height,capacity,founders,season,sample,checkpoint;
 uint64_t seed;
} Config;
typedef struct {
 uint32_t mineral,food,detritus,structure,cache;
 uint32_t owner; /* low bits of builder's lineage: a persistent physical trace */
 uint16_t elevation,moisture,signal;
 uint8_t rock,pad;
} Cell;
typedef struct {
 uint64_t id,parent,lineage,born;
 uint32_t pos,age,energy,body,store;
 uint8_t genome[GENOME],memory[MEM],pc,dir,alive,pad;
} Agent;
typedef struct {
 Config c;
 uint64_t tick,rng,next_id,initial_matter,births,deaths,moves,eats,builds,breaks;
 uint64_t gifts,attacks,parasites,mutations,energy_spent,solar_food;
 uint64_t sensor_witness,memory_witness,witness_samples;
 uint32_t population,pad;
 Cell *cells;
 Agent *agents;
 int32_t *occupancy;
 FILE *events;
} World;
typedef struct {
 uint64_t magic;
 uint32_t version,endian,cell_size,agent_size,world_size,header_size;
 uint64_t payload_size,checksum;
 World state; /* pointers nulled; no addresses persisted */
} Header;
static uint64_t hash_bytes(uint64_t h,const void *p,size_t n) {
 const unsigned char *b=p; for(size_t i=0;i<n;i++) { h^=b[i]; h*=UINT64_C(1099511628211); } return h;
}
static uint64_t random64(World *w) { uint64_t x=w->rng; x^=x>>12; x^=x<<25; x^=x>>27; w->rng=x; return x*UINT64_C(2685821657736338717); }
static uint32_t below(World *w,uint32_t n) { return (uint32_t)(random64(w)%n); }
static uint32_t minimum(uint32_t a,uint32_t b) { return a<b?a:b; }
static uint64_t genotype(const Agent *a) { return hash_bytes(UINT64_C(1469598103934665603),a->genome,GENOME); }
static uint32_t count_cells(const World *w) { return w->c.width*w->c.height; }
static void validate_config(const Config *c) {
 if(c->width<8||c->height<8||c->width>4096||c->height>4096||(uint64_t)c->width*c->height>MAX_CELLS) fail("dimensions outside 8..4096 / cell limit");
 if(!c->capacity||c->capacity>MAX_AGENTS||c->capacity>(uint64_t)c->width*c->height||c->founders>c->capacity) fail("invalid capacity/founders");
 if(c->season<16||c->season>10000000||!c->sample||!c->checkpoint||!c->seed) fail("invalid season/sample/checkpoint/seed");
}
static void allocate_world(World *w) {
 validate_config(&w->c); uint32_t n=count_cells(w);
 w->cells=alloc(n,sizeof(Cell)); w->agents=alloc(w->c.capacity,sizeof(Agent)); w->occupancy=alloc(n,sizeof(int32_t));
 for(uint32_t i=0;i<n;i++) w->occupancy[i]=-1;
}
static void release(World *w) { free(w->cells); free(w->agents); free(w->occupancy); memset(w,0,sizeof(*w)); }
static uint32_t neighbor(const World *w,uint32_t p,unsigned d) {
 uint32_t x=p%w->c.width,y=p/w->c.width;
 if(d==0) { if(x+1<w->c.width) x++; }
 else if(d==1) { if(y+1<w->c.height) y++; }
 else if(d==2) { if(x) x--; }
 else { if(y) y--; }
 return y*w->c.width+x; /* reflecting, not a torus */
}
static uint64_t matter(const World *w) {
 uint64_t n=0; for(uint32_t i=0;i<count_cells(w);i++) { const Cell *c=&w->cells[i]; n+=(uint64_t)c->mineral+c->food+c->detritus+c->structure+c->cache; }
 for(uint32_t i=0;i<w->c.capacity;i++) if(w->agents[i].alive) n+=(uint64_t)w->agents[i].body+w->agents[i].store;
 return n;
}
static void validate_world(World *w) {
 uint32_t pop=0,n=count_cells(w); uint64_t maxid=0;
 for(uint32_t i=0;i<n;i++) {
  Cell *c=&w->cells[i];
  if(c->rock>1||c->moisture>1023||c->elevation>1023||c->signal>4095||
     c->mineral>100000000||c->food>100000000||c->detritus>100000000||c->structure>100000000||c->cache>100000000) fail("invalid cell in checkpoint");
  w->occupancy[i]=-1;
 }
 for(uint32_t i=0;i<w->c.capacity;i++) {
  Agent *a=&w->agents[i]; if(a->alive>1) fail("invalid alive flag"); if(!a->alive) continue;
  if(a->pos>=n||w->cells[a->pos].rock||w->occupancy[a->pos]!=-1||a->body<1||a->body>256||a->store>4096||a->energy>100000||a->pc>=GENOME||a->dir>3||!a->id||a->id>=w->next_id||a->born>w->tick||!a->lineage||a->parent>=a->id) fail("invalid or overlapping agent");
  w->occupancy[a->pos]=(int32_t)i; pop++; if(a->id>maxid) maxid=a->id;
 }
 if(pop!=w->population||maxid>=w->next_id||matter(w)!=w->initial_matter) fail("population/matter invariant failed");
}
static void event(World *w,const char *kind,const Agent *a,uint64_t other,uint32_t amount) {
 if(!w->events) return;
 /* Complete lineage events; repetitive physical interactions are sampled 1/256.
    Hashing deliberately does not advance the simulation RNG. */
 if(strcmp(kind,"birth")&&strcmp(kind,"founder")&&strcmp(kind,"death")) {
  uint64_t key=hash_bytes(UINT64_C(1469598103934665603),&w->tick,sizeof(w->tick));
  key=hash_bytes(key,&a->id,sizeof(a->id)); key=hash_bytes(key,kind,strlen(kind));
  if((key&255)!=0) return;
 }
 if(fprintf(w->events,"%"PRIu64"\t%s\t%"PRIu64"\t%"PRIu64"\t%"PRIu64"\t%u\t%u\t%016"PRIx64"\t",w->tick,kind,a->id,a->lineage,other,a->pos,amount,genotype(a))<0) die("event write");
 if(!strcmp(kind,"birth")||!strcmp(kind,"founder")) for(unsigned j=0;j<GENOME;j++) if(fprintf(w->events,"%02x",a->genome[j])<0) die("genome write");
 if(fputc('\n',w->events)==EOF) die("event newline");
}
static void death(World *w,Agent *a) {
 if(!a->alive) return;
 Cell *c=&w->cells[a->pos]; c->detritus+=a->body+a->store;
 event(w,"death",a,0,a->age); a->body=0; a->store=0; a->energy=0; a->alive=0;
 w->occupancy[a->pos]=-1; w->population--; w->deaths++;
}
static int spend(World *w,Agent *a,uint32_t n) {
 if(a->energy<n) return 0;
 a->energy-=n; w->energy_spent+=n; return 1;
}
static uint8_t sense(const World *w,const Agent *a,unsigned k) {
 const Cell *c=&w->cells[a->pos]; uint32_t q=neighbor(w,a->pos,a->dir); const Cell *b=&w->cells[q];
 switch(k&7) {
 case 0:return (uint8_t)minimum(c->food*8,255);
 case 1:return (uint8_t)minimum(a->energy/4,255);
 case 2:return (uint8_t)minimum(a->store*8,255);
 case 3:return (uint8_t)minimum(b->food*8,255);
 case 4:return (uint8_t)(w->occupancy[q]>=0?255:0);
 case 5:return (uint8_t)(c->moisture/4);
 case 6:return (uint8_t)minimum(c->signal/8,255);
 default:return (uint8_t)minimum(c->structure*8,255);
 }
}
/* Decode only: copied state allows counterfactual witnesses without RNG/effects. */
static uint8_t choose(const World *w,Agent *a,int blank_sensors,int blank_memory,unsigned *executed) {
 if(executed) *executed=0;
 if(blank_memory) memset(a->memory,0,MEM);
 for(unsigned j=0;j<8;j++) {
  if(executed) (*executed)++;
  uint8_t ins=a->genome[a->pc]; a->pc=(a->pc+1)%GENOME;
  unsigned op=ins&15,arg=ins>>4;
  if(op==13) { a->memory[arg&7]=blank_sensors?0:sense(w,a,arg); continue; }
  if(op==14) { if(a->memory[arg&7]<(arg<8?64:192)) a->pc=(a->pc+1)%GENOME; continue; }
  return ins;
 }
 return 15; /* bounded VM work; no free infinite loop */
}
static int free_slot(World *w) { for(uint32_t i=0;i<w->c.capacity;i++) if(!w->agents[i].alive) return (int)i; return -1; }
static void reproduce(World *w,Agent *a,uint32_t q) {
 if(q==a->pos||w->occupancy[q]>=0||w->cells[q].rock||a->body<12||a->store<8||a->energy<100||w->population==w->c.capacity) return;
 int slot=free_slot(w); if(slot<0) return;
 spend(w,a,20); Agent *b=&w->agents[slot]; memset(b,0,sizeof(*b));
 b->alive=1; b->id=w->next_id++; b->parent=a->id; b->lineage=a->lineage; b->born=w->tick; b->pos=q;
 b->body=a->body/2; a->body-=b->body; b->store=a->store/2; a->store-=b->store;
 b->energy=a->energy/2; a->energy-=b->energy; memcpy(b->genome,a->genome,GENOME);
 /* Inherited 4-byte memory; other memories need to be acquired. */
 memcpy(b->memory,a->memory,4); b->dir=(uint8_t)below(w,4);
 unsigned mutations=below(w,100)<25?1:0;
 if(below(w,1000)==0) mutations+=3;
 for(unsigned j=0;j<mutations;j++) { unsigned p=below(w,GENOME); b->genome[p]^=(uint8_t)(1u<<below(w,8)); w->mutations++; }
 w->occupancy[q]=slot; w->population++; w->births++; event(w,"birth",b,a->id,mutations);
}
static void action(World *w,Agent *a,uint8_t ins) {
 unsigned op=ins&15,arg=ins>>4;
 uint32_t q=neighbor(w,a->pos,(a->dir+(arg&3))&3); Cell *c=&w->cells[a->pos]; Cell *b=&w->cells[q];
 Agent *other=w->occupancy[q]>=0?&w->agents[w->occupancy[q]]:NULL;
 if(other==a) other=NULL;
 uint32_t amount=1+(arg>>2);
 switch(op) {
 case 0: { /* consume: matter becomes inventory, chemical energy is available */
  uint32_t n=minimum(amount,c->food); n=minimum(n,4096-a->store);
  if(n&&spend(w,a,1)) { c->food-=n; a->store+=n; a->energy=minimum(100000,a->energy+n*24); w->eats+=n; }
  break;
 }
 case 1:
  if(q!=a->pos&&!b->rock&&!other&&abs((int)b->elevation-(int)c->elevation)<180&&spend(w,a,2+a->body/16+b->structure/8)) {
   w->occupancy[a->pos]=-1; w->occupancy[q]=(int32_t)(a-w->agents); a->pos=q; w->moves++;
  } break;
 case 2: if(spend(w,a,1)) a->dir=(a->dir+1+(arg%3))&3; break;
 case 3:
  if(a->store&&a->body<256&&spend(w,a,4)) { a->body++; a->store--; } break;
 case 4: reproduce(w,a,q); break;
 case 5:
  if(a->store&&c->structure<256&&spend(w,a,6)) { a->store--; c->structure++; c->owner=(uint32_t)a->lineage; w->builds++; event(w,"build",a,0,c->structure); } break;
 case 6:
  if(c->structure&&a->store<4096&&spend(w,a,4)) { c->structure--; a->store++; w->breaks++; } break;
 case 7:
  if(a->store&&c->cache<65536&&spend(w,a,2)) { a->store--; c->cache++; event(w,"deposit",a,0,1); } break;
 case 8:
  if(c->cache&&a->store<4096&&spend(w,a,2)) { c->cache--; a->store++; } break;
 case 9:
  if(other&&a->store<4096&&a->body+(a->genome[63]&15)>other->body+b->structure/2&&spend(w,a,8)) {
   uint32_t take=minimum(amount,4096-a->store);
   uint32_t n=minimum(take,other->store); if(!n) { n=minimum(take,other->body); other->body-=n; } else other->store-=n;
   a->store+=n; w->attacks++; event(w,"attack",a,other->id,n); if(!other->body) death(w,other);
  } break;
 case 10:
  if(other&&a->body<=other->body&&spend(w,a,4)) {
   uint32_t n=minimum(8,other->energy); n=minimum(n,100000-a->energy); other->energy-=n; a->energy+=n; w->parasites++; event(w,"parasite",a,other->id,n);
  } break;
 case 11:
  if(other&&a->store&&other->store<4096&&a->energy>=8&&other->energy<=99996&&spend(w,a,2)) {
   a->store--; other->store++; a->energy-=4; other->energy+=4; w->gifts++; event(w,"gift",a,other->id,1);
  } break;
 case 12:
  if(spend(w,a,3)) c->signal=(uint16_t)minimum(4095,c->signal+16+arg*4);
  break;
 case 15: default: break;
 }
}
static void environment(World *w) {
 uint32_t n=count_cells(w),period=w->c.season;
 uint32_t phase=(uint32_t)(w->tick%period),sun=phase<period/2?phase*512/period:(period-phase)*512/period;
 /* Integer local dynamics and pair transfers: no mass is created by diffusion. */
 for(uint32_t i=0;i<n;i++) {
  Cell *c=&w->cells[i];
  uint32_t rain=(uint32_t)((i*UINT32_C(2654435761)+(uint32_t)(w->tick/64)*UINT32_C(2246822519))>>24);
  int moist=(int)c->moisture+(rain<64?8:0)-(int)(1+sun/128);
  c->moisture=(uint16_t)(moist<0?0:moist>1023?1023:moist);
  if(c->signal) c->signal--;
  if(c->detritus&&((i+w->tick)&7)==0) { c->detritus--; c->mineral++; }
  if(c->structure&&((i+w->tick)&511)==0) { c->structure--; c->detritus++; if(!c->structure)c->owner=0; }
  if(c->cache&&((i+w->tick)&255)==0) { c->cache--; c->detritus++; }
  if(c->food&&((i+w->tick)&63)==0) { c->food--; c->detritus++; }
  uint32_t productivity=(sun*(1024-c->elevation)*(c->moisture+64))/UINT32_C(1048576);
  if(!c->rock&&c->mineral&&c->food<96&&((i+w->tick)&255)<productivity) { c->mineral--; c->food++; w->solar_food++; }
  /* alternating axes and phase avoid a fixed directional sweep */
  unsigned axis=(unsigned)(w->tick&1), parity=(unsigned)((w->tick/2)&1);
  uint32_t coordinate=axis?i%w->c.width:i/w->c.width;
  uint32_t q=neighbor(w,i,axis?0:1);
  if(q==i||(coordinate&1)!=parity||c->rock||w->cells[q].rock) continue;
  Cell *b=&w->cells[q];
  if(c->mineral>b->mineral+4) { c->mineral--; b->mineral++; }
  else if(b->mineral>c->mineral+4) { b->mineral--; c->mineral++; }
  if(c->signal>b->signal+8) { c->signal-=2; b->signal+=2; }
  else if(b->signal>c->signal+8) { b->signal-=2; c->signal+=2; }
 }
}
static void step(World *w) {
 w->tick++; environment(w);
 uint32_t start=below(w,w->c.capacity);
 for(uint32_t k=0;k<w->c.capacity;k++) {
  Agent *a=&w->agents[(start+k)%w->c.capacity]; if(!a->alive||a->born==w->tick) continue;
  a->age++; Cell *c=&w->cells[a->pos];
  unsigned active_memory=0; for(unsigned mi=0;mi<MEM;mi++) active_memory+=(a->memory[mi]!=0);
  uint32_t maintenance=1+a->body/24+a->store/128+active_memory/4+(c->moisture<64&&c->structure<4?1:0);
  if(!spend(w,a,maintenance)) {
   /* Starvation consumes real body mass; none is silently erased. */
   if(a->body>1) { a->body--; c->detritus++; a->energy+=8; }
   else { death(w,a); continue; }
  }
  if(a->age>1000+(uint32_t)a->genome[62]*16&&below(w,64)==0) { death(w,a); continue; }
  if((a->id+w->tick)%257==0) {
   Agent s=*a,m=*a,base=*a; uint8_t real=choose(w,&base,0,0,NULL);
   w->sensor_witness+=(real!=choose(w,&s,1,0,NULL)); w->memory_witness+=(real!=choose(w,&m,0,1,NULL)); w->witness_samples++;
  }
  unsigned instructions=0; Agent decoded=*a; uint8_t ins=choose(w,&decoded,0,0,&instructions);
  /* Every instruction after the first costs one energy, including sensing. */
  if(spend(w,a,instructions-1)) { a->pc=decoded.pc; memcpy(a->memory,decoded.memory,MEM); action(w,a,ins); }
 }
}
static World initial(Config config) {
 World w; memset(&w,0,sizeof(w)); w.c=config; allocate_world(&w); w.rng=config.seed; w.next_id=1;
 uint32_t n=count_cells(&w);
 /* Smooth multi-scale deterministic topography; sharp rock walls still have gaps. */
 for(uint32_t y=0;y<config.height;y++) for(uint32_t x=0;x<config.width;x++) {
  uint32_t i=y*config.width+x; Cell *c=&w.cells[i];
  c->elevation=(uint16_t)((((x/32)*97+(y/32)*131+(uint32_t)config.seed)%640)+(below(&w,64)));
  c->moisture=(uint16_t)(384+below(&w,512)); c->rock=(uint8_t)(((x%127)==0&&(y%31)>3)||below(&w,100)<3);
  c->mineral=80+below(&w,81); c->food=c->rock?0:8+below(&w,24); c->detritus=below(&w,16);
 }
 /* Recipes supply viable operations, but no fitness, species reward, or resurrection. */
 static const uint8_t recipe[]={0x0d,0x0e,0x00,0x10,0x20,0x03,0x01,0x02,0x00,0x10,0x04,0x03,0x00,0x20,0x01,0x02};
 for(uint32_t k=0;k<config.founders;k++) {
  Agent *a=&w.agents[k]; uint32_t p;
  unsigned tries=0; do { p=below(&w,n); if(++tries>n*4u) fail("cannot place founders"); } while(w.cells[p].rock||w.occupancy[p]>=0);
  a->id=w.next_id++; a->lineage=a->id; a->alive=1; a->pos=p; a->body=12; a->store=8; a->energy=320; a->dir=(uint8_t)below(&w,4);
  for(unsigned j=0;j<GENOME;j++) a->genome[j]=recipe[(j+(k%4)*4)%sizeof(recipe)];
  /* 1/4 pioneers gets structural/social actions; 1/4 random instructions. */
  if(k%4==1) for(unsigned j=8;j<GENOME;j+=16) a->genome[j]=(uint8_t)(5+below(&w,8));
  if(k%4==2) for(unsigned j=0;j<GENOME;j+=4) a->genome[j]=(uint8_t)below(&w,256);
  a->genome[62]=(uint8_t)(64+below(&w,128)); a->genome[63]=(uint8_t)below(&w,256);
  w.occupancy[p]=(int32_t)k; w.population++;
 }
 w.initial_matter=matter(&w); return w;
}
static void path(char *out,size_t n,const char *dir,const char *file) { if(snprintf(out,n,"%s/%s",dir,file)>=(int)n) fail("path too long"); }
static int lock_world(const char *dir) { char p[4096]; path(p,sizeof(p),dir,"runner.lock"); int fd=open(p,O_CREAT|O_RDWR,0600); if(fd<0||flock(fd,LOCK_EX|LOCK_NB)) die("world lock (already running?)"); return fd; }
static void sync_file(FILE *f) { if(fflush(f)||fsync(fileno(f))) die("fsync"); }
static void save(World *w,const char *dir) {
 Header h; memset(&h,0,sizeof(h)); h.magic=MAGIC; h.version=VERSION; h.endian=0x01020304; h.cell_size=sizeof(Cell); h.agent_size=sizeof(Agent); h.world_size=sizeof(World); h.header_size=sizeof(Header);
 h.state=*w; h.state.cells=NULL; h.state.agents=NULL; h.state.occupancy=NULL; h.state.events=NULL;
 h.payload_size=(uint64_t)count_cells(w)*sizeof(Cell)+(uint64_t)w->c.capacity*sizeof(Agent);
 uint64_t sum=hash_bytes(UINT64_C(1469598103934665603),&h,sizeof(h)); sum=hash_bytes(sum,w->cells,(size_t)count_cells(w)*sizeof(Cell)); sum=hash_bytes(sum,w->agents,(size_t)w->c.capacity*sizeof(Agent)); h.checksum=sum;
 char tmp[4096],dst[4096]; path(tmp,sizeof(tmp),dir,"state.ck.tmp"); path(dst,sizeof(dst),dir,"state.ck");
 FILE *f=fopen(tmp,"wb"); if(!f) die("checkpoint open");
 if(fwrite(&h,sizeof(h),1,f)!=1||fwrite(w->cells,sizeof(Cell),count_cells(w),f)!=count_cells(w)||fwrite(w->agents,sizeof(Agent),w->c.capacity,f)!=w->c.capacity) die("checkpoint write");
 sync_file(f); if(fclose(f)||rename(tmp,dst)) die("checkpoint rename"); int d=open(dir,O_RDONLY|O_DIRECTORY); if(d<0||fsync(d)) die("checkpoint directory fsync"); close(d);
}
static World load(const char *dir) {
 char p[4096]; path(p,sizeof(p),dir,"state.ck"); FILE *f=fopen(p,"rb"); if(!f) die("checkpoint open"); Header h;
 if(fread(&h,sizeof(h),1,f)!=1) fail("truncated header");
 if(h.magic!=MAGIC||h.version!=VERSION||h.endian!=0x01020304||h.cell_size!=sizeof(Cell)||h.agent_size!=sizeof(Agent)||h.world_size!=sizeof(World)||h.header_size!=sizeof(Header)) fail("incompatible checkpoint format/ABI");
 validate_config(&h.state.c);
 uint64_t size=(uint64_t)h.state.c.width*h.state.c.height*sizeof(Cell)+(uint64_t)h.state.c.capacity*sizeof(Agent);
 if(h.payload_size!=size||h.state.cells||h.state.agents||h.state.occupancy||h.state.events||!h.state.rng||!h.state.next_id) fail("invalid checkpoint header");
 struct stat st; if(fstat(fileno(f),&st)||st.st_size<0||(uint64_t)st.st_size!=sizeof(h)+size) fail("checkpoint file size mismatch");
 World w=h.state; allocate_world(&w);
 if(fread(w.cells,sizeof(Cell),count_cells(&w),f)!=count_cells(&w)||fread(w.agents,sizeof(Agent),w.c.capacity,f)!=w.c.capacity) fail("truncated checkpoint payload");
 if(fclose(f)) die("checkpoint close");
 uint64_t wanted=h.checksum; h.checksum=0;
 uint64_t sum=hash_bytes(UINT64_C(1469598103934665603),&h,sizeof(h)); sum=hash_bytes(sum,w.cells,(size_t)count_cells(&w)*sizeof(Cell)); sum=hash_bytes(sum,w.agents,(size_t)w.c.capacity*sizeof(Agent));
 if(sum!=wanted) fail("checkpoint checksum mismatch");
 validate_world(&w); return w;
}
static void inspect(const World *w,FILE *out) {
 fprintf(out,"tick=%"PRIu64" population=%u cells=%u dimensions=%ux%u capacity=%u seed=%"PRIu64"\nbirths=%"PRIu64" deaths=%"PRIu64" matter=%"PRIu64" initial_matter=%"PRIu64" rng=%016"PRIx64"\n",w->tick,w->population,count_cells(w),w->c.width,w->c.height,w->c.capacity,w->c.seed,w->births,w->deaths,matter(w),w->initial_matter,w->rng);
 if(!w->population) fprintf(out,"EXTINCTION: no automatic founders; retain this checkpoint.\n");
}
static int cmp64(const void *a,const void *b) { uint64_t x=*(const uint64_t*)a,y=*(const uint64_t*)b; return x<y?-1:x>y; }
static void series(World *w,FILE *f) {
 uint64_t food=0,mineral=0,detritus=0,structures=0,caches=0,energy=0,body=0,store=0,oldest=0;
 uint64_t *g=alloc(w->population+1,sizeof(uint64_t)),*l=alloc(w->population+1,sizeof(uint64_t)); uint32_t pop=0,genotypes=0,lineages=0;
 for(uint32_t i=0;i<count_cells(w);i++) { Cell *c=&w->cells[i]; food+=c->food; mineral+=c->mineral; detritus+=c->detritus; structures+=c->structure; caches+=c->cache; }
 for(uint32_t i=0;i<w->c.capacity;i++) if(w->agents[i].alive) { Agent *a=&w->agents[i]; energy+=a->energy; body+=a->body; store+=a->store; if(a->age>oldest) oldest=a->age; g[pop]=genotype(a); l[pop++]=a->lineage; }
 qsort(g,pop,sizeof(uint64_t),cmp64); qsort(l,pop,sizeof(uint64_t),cmp64);
 for(uint32_t i=0;i<pop;i++) { if(!i||g[i]!=g[i-1])genotypes++; if(!i||l[i]!=l[i-1])lineages++; } free(g);free(l);
 fprintf(f,"%"PRIu64"\t%u\t%u\t%u\t%"PRIu64"\t%"PRIu64"\t%"PRIu64"\t%"PRIu64"\t%"PRIu64"\t%"PRIu64"\t%"PRIu64"\t%"PRIu64"\t%"PRIu64"\t%"PRIu64"\t%"PRIu64"\t%"PRIu64"\t%"PRIu64"\t%"PRIu64"\t%"PRIu64"\t%"PRIu64"\t%"PRIu64"\t%"PRIu64"\t%"PRIu64"\t%"PRIu64"\t%"PRIu64"\t%"PRIu64"\t%"PRIu64"\t%"PRIu64"\n",w->tick,pop,genotypes,lineages,w->births,w->deaths,food,mineral,detritus,structures,caches,body,store,energy,oldest,w->moves,w->eats,w->builds,w->breaks,w->gifts,w->attacks,w->parasites,w->mutations,w->energy_spent,w->solar_food,w->witness_samples,w->sensor_witness,w->memory_witness);
 if(ferror(f)) die("series write");
}
/* Remove log rows newer than the committed snapshot before deterministic replay. */
static void trim_log(const char *dir,const char *name,uint64_t tick) {
 char p[4096],t[4096]; path(p,sizeof(p),dir,name); if(snprintf(t,sizeof(t),"%s.trim",p)>=(int)sizeof(t)) fail("trim path too long");
 FILE *in=fopen(p,"r"); if(!in) { if(errno==ENOENT) return; die("log read"); }
 FILE *out=fopen(t,"w"); if(!out) die("log trim"); char *line=NULL; size_t cap=0; ssize_t len;
 while((len=getline(&line,&cap,in))>=0) { char *end; errno=0; uint64_t row=strtoull(line,&end,10); if((end==line||(*end=='\t'&&!errno&&row<=tick))&&line[len-1]=='\n') if(fwrite(line,1,(size_t)len,out)!=(size_t)len) die("trim write"); }
 if(ferror(in)) die("trim read");
 free(line); if(fclose(in))die("trim close"); sync_file(out); if(fclose(out)||rename(t,p))die("trim rename");
}
static FILE *open_log(const char *dir,const char *name,const char *header) {
 char p[4096]; path(p,sizeof(p),dir,name); FILE *f=fopen(p,"a+"); if(!f) die("log open"); if(fseek(f,0,SEEK_END))die("log seek"); if(ftell(f)==0&&fprintf(f,"%s\n",header)<0)die("log header"); return f;
}
static const char *series_header="tick\tpopulation\tgenotypes_hash\tlineages\tbirths\tdeaths\tfood\tmineral\tdetritus\tstructures\tcaches\tbody\tstore\tenergy\toldest\tmoves\teats\tbuilds\tbreaks\tgifts\tattacks\tparasites\tmutations\tenergy_spent\tsolar_food\twitness_samples\tsensor_witness\tmemory_witness";
static const char *event_header="tick\tkind\tid\tlineage\tother\tposition\tamount\tgenotype_hash\tgenome_hex";
static uint64_t number(const char *s) { char *e; errno=0; if(!*s||*s=='-')fail("invalid unsigned number"); uint64_t n=strtoull(s,&e,10); if(errno||*e)fail("invalid unsigned number"); return n; }
static uint32_t number32(const char *s) { uint64_t n=number(s); if(n>UINT32_MAX)fail("integer overflow"); return (uint32_t)n; }
static Config defaults(void) { Config c; memset(&c,0,sizeof(c)); c.width=1024; c.height=512; c.capacity=65536; c.founders=8192; c.season=4096; c.sample=100; c.checkpoint=2000; c.seed=20261025; return c; }
static void check(int okay,const char *message) { if(!okay)fail(message); }
static void selftest(void) {
 for(uint64_t seed=1;seed<=4;seed++) {
  Config c=defaults();c.width=32;c.height=24;c.capacity=256;c.founders=80;c.seed=seed;c.season=64;
  World a=initial(c),b=initial(c); char dir[]="/tmp/relief-test-XXXXXX"; if(!mkdtemp(dir))die("mkdtemp");
  for(unsigned i=0;i<160;i++) { step(&a); check(matter(&a)==a.initial_matter,"matter conservation"); }
  for(unsigned i=0;i<71;i++)step(&b);
  save(&b,dir); release(&b); b=load(dir);
  for(unsigned i=71;i<160;i++)step(&b);
  check(!memcmp(a.cells,b.cells,(size_t)count_cells(&a)*sizeof(Cell)),"cell determinism");
  check(!memcmp(a.agents,b.agents,(size_t)a.c.capacity*sizeof(Agent)),"agent determinism");
  World aa=a,bb=b;aa.cells=bb.cells=NULL;aa.agents=bb.agents=NULL;aa.occupancy=bb.occupancy=NULL;
  check(!memcmp(&aa,&bb,sizeof(World)),"state determinism"); validate_world(&a); validate_world(&b);
  char p[4096];path(p,sizeof(p),dir,"state.ck");unlink(p);rmdir(dir);release(&a);release(&b);
 }
 /* Each parity of each spatial axis must actually transport matter. */
 for(unsigned phase=0;phase<4;phase++) {
  Config d=defaults();d.width=d.height=8;d.capacity=4;d.founders=0;d.season=10000000;
  World diffusion=initial(d);memset(diffusion.cells,0,count_cells(&diffusion)*sizeof(Cell));
  unsigned axis=phase&1, parity=(phase/2)&1;
  uint32_t from=axis?parity:parity*8, to=neighbor(&diffusion,from,axis?0:1);
  diffusion.cells[from].mineral=100;diffusion.initial_matter=100;diffusion.tick=phase;
  environment(&diffusion);
  check(diffusion.cells[to].mineral==1&&matter(&diffusion)==100,"both diffusion matchings per axis");release(&diffusion);
 }
 Config c=defaults();c.width=c.height=8;c.capacity=4;c.founders=2;World w=initial(c);
 Agent *a=&w.agents[0],*b=&w.agents[1];
 w.occupancy[a->pos]=-1;w.occupancy[b->pos]=-1;a->pos=9;b->pos=10;w.cells[9].rock=w.cells[10].rock=0;w.occupancy[9]=0;w.occupancy[10]=1;a->dir=0;
 uint64_t total=matter(&w);a->energy=200;a->store=8;a->body=20;b->body=12;b->store=10;w.initial_matter=matter(&w);total=w.initial_matter;
 uint32_t e=a->energy;action(&w,a,5);check(a->energy==e-6&&matter(&w)==total,"construction has cost and conserves matter");
 action(&w,a,6);check(matter(&w)==total,"deconstruction conserves matter");
 action(&w,a,11);check(w.gifts==1&&matter(&w)==total,"gifts conserve matter");
 action(&w,a,9);check(w.attacks==1&&matter(&w)==total,"attack conserves matter");
 a->store=4095;w.initial_matter=matter(&w);action(&w,a,0xc9);check(matter(&w)==w.initial_matter,"near-full predator conservation");
 death(&w,b); check(matter(&w)==w.initial_matter,"death conserves matter");
 a->energy=200;a->store=16;a->body=24;w.initial_matter=matter(&w);action(&w,a,4);check(w.births==1&&matter(&w)==w.initial_matter,"reproduction conservation");
 death(&w,a);for(uint32_t i=0;i<w.c.capacity;i++)if(w.agents[i].alive)death(&w,&w.agents[i]);
 uint64_t births_before=w.births;
 for(unsigned i=0;i<50;i++)step(&w);
 check(!w.population&&w.births==births_before,"no reseeding");release(&w);
 puts("selftest PASS: four seeds; exact restart; conservation, costly construction, gifts, predation, reproduction, extinction");
}
int main(int argc,char **argv) {
 if(argc==2&&!strcmp(argv[1],"selftest")){selftest();return 0;}
 if(argc<3) { fprintf(stderr,"usage: relief init DIR [--width N --height N --capacity N --founders N --seed N --season N --sample N --checkpoint N]\n       relief run DIR [--ticks N] [--tick-ms N]\n       relief inspect DIR\n       relief selftest\n");return 2; }
 const char *cmd=argv[1],*dir=argv[2];
 if(!strcmp(cmd,"init")) {
  Config c=defaults();for(int i=3;i<argc;i+=2) { if(i+1==argc)fail("missing argument");
   if(!strcmp(argv[i],"--width"))c.width=number32(argv[i+1]);else if(!strcmp(argv[i],"--height"))c.height=number32(argv[i+1]);else if(!strcmp(argv[i],"--capacity"))c.capacity=number32(argv[i+1]);else if(!strcmp(argv[i],"--founders"))c.founders=number32(argv[i+1]);else if(!strcmp(argv[i],"--seed"))c.seed=number(argv[i+1]);else if(!strcmp(argv[i],"--season"))c.season=number32(argv[i+1]);else if(!strcmp(argv[i],"--sample"))c.sample=number32(argv[i+1]);else if(!strcmp(argv[i],"--checkpoint"))c.checkpoint=number32(argv[i+1]);else fail("unknown init option"); }
  validate_config(&c);if(mkdir(dir,0750))die("init needs a NEW directory");int lock=lock_world(dir);World w=initial(c);
  w.events=open_log(dir,"events.tsv",event_header);for(uint32_t i=0;i<w.c.founders;i++)event(&w,"founder",&w.agents[i],0,0);sync_file(w.events);fclose(w.events);w.events=NULL;
  FILE *s=open_log(dir,"series.tsv",series_header);series(&w,s);sync_file(s);fclose(s);save(&w,dir);inspect(&w,stdout);release(&w);close(lock);return 0;
 }
 if(!strcmp(cmd,"inspect")) { if(argc!=3)fail("inspect takes no options");World w=load(dir);inspect(&w,stdout);release(&w);return 0; }
 if(strcmp(cmd,"run"))fail("unknown command");
 uint64_t ticks=0; uint32_t tick_ms=0;
 for(int i=3;i<argc;i+=2) {
  if(i+1==argc)fail("missing run argument");
  if(!strcmp(argv[i],"--ticks")) { ticks=number(argv[i+1]);if(!ticks)fail("bounded ticks must be positive"); }
  else if(!strcmp(argv[i],"--tick-ms")) { tick_ms=number32(argv[i+1]);if(tick_ms>1000)fail("tick-ms outside 0..1000"); }
  else fail("unknown run option");
 }
 int lock=lock_world(dir);World w=load(dir);trim_log(dir,"events.tsv",w.tick);trim_log(dir,"series.tsv",w.tick);
 w.events=open_log(dir,"events.tsv",event_header);FILE *s=open_log(dir,"series.tsv",series_header);
 struct sigaction sa;memset(&sa,0,sizeof(sa));sa.sa_handler=on_signal;sigemptyset(&sa.sa_mask);if(sigaction(SIGTERM,&sa,NULL)||sigaction(SIGINT,&sa,NULL))die("sigaction");
 uint64_t done=0;while(!stopping&&w.population&&(!ticks||done<ticks)) {
  struct timespec began; if(clock_gettime(CLOCK_MONOTONIC,&began))die("clock_gettime");
  step(&w);done++;if(w.tick%w.c.sample==0) { validate_world(&w);series(&w,s);if(fflush(s))die("series flush"); }
  if(w.tick%w.c.checkpoint==0){sync_file(w.events);sync_file(s);save(&w,dir);}
  if(tick_ms&&!stopping) {
   struct timespec now; if(clock_gettime(CLOCK_MONOTONIC,&now))die("clock_gettime");
   int64_t elapsed=(int64_t)(now.tv_sec-began.tv_sec)*INT64_C(1000000000)+now.tv_nsec-began.tv_nsec;
   int64_t remaining=(int64_t)tick_ms*1000000-elapsed;
   if(remaining>0) { struct timespec wait={(time_t)(remaining/1000000000),(long)(remaining%1000000000)};
    while(nanosleep(&wait,&wait)&&!stopping) { if(errno!=EINTR)die("nanosleep"); }
   }
  }
 }
 validate_world(&w);if(w.tick%w.c.sample)series(&w,s);sync_file(w.events);sync_file(s);save(&w,dir);inspect(&w,stdout);
 if(fclose(w.events)||fclose(s))die("log close");
 release(&w);close(lock);return 0;
}
