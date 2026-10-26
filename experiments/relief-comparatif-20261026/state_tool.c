/* Offline checkpoint audit/transform only. Never invokes step() or initial(). */
#define _XOPEN_SOURCE 700
#define main relief_original_main
#include "../relief-20261025/relief.c"
#undef main
#include <math.h>

typedef struct {
 uint64_t lineage,population,body,store,energy,local_cache,local_food;
} LineageSummary;
typedef struct { uint32_t owner; uint64_t cells,cache; } OwnerSummary;

static void checked_print(FILE *f) { if(ferror(f)) die("output write"); }
static void close_output(FILE *f) { checked_print(f); sync_file(f); if(fclose(f))die("output close"); }
static int directory_prefix(const char *parent,const char *child) {
 size_t n=strlen(parent); return !strncmp(parent,child,n)&&(!child[n]||child[n]=='/');
}
static void forbidden_output(const char *p) {
 if(strstr(p,"/vie-relief-20261025")||strstr(p,"/octet-relief")) fail("refusing output in live Relief path");
}
/* Resolve a new path through its existing parent. Existing destinations are refused. */
static void new_path(const char *given,char *resolved,size_t size) {
 char copy[PATH_MAX],parent[PATH_MAX];
 if(strlen(given)>=sizeof(copy))fail("path too long");
 strcpy(copy,given);char *slash=strrchr(copy,'/');const char *leaf=copy;
 if(slash) { *slash=0;leaf=slash+1;if(!realpath(*copy?copy:"/",parent))die("output parent realpath"); }
 else if(!realpath(".",parent))die("output parent realpath");
 if(!*leaf||!strcmp(leaf,".")||!strcmp(leaf,".."))fail("invalid new output name");
 if(snprintf(resolved,size,"%s/%s",parent,leaf)>=(int)size)fail("path too long");
 forbidden_output(resolved);struct stat st;
 if(!lstat(resolved,&st))fail("output already exists");
 if(errno!=ENOENT)die("output lstat");
}
static FILE *new_output(const char *p) {
 char resolved[PATH_MAX];new_path(p,resolved,sizeof(resolved));
 FILE *f=fopen(resolved,"wx");if(!f)die("exclusive output create");return f;
}
static FILE *suffix_output(const char *prefix,const char *suffix) {
 char p[PATH_MAX];if(snprintf(p,sizeof(p),"%s%s",prefix,suffix)>=(int)sizeof(p))fail("output path too long");
 return new_output(p);
}
static int cmp_agent_genome(const void *x,const void *y) {
 const Agent *a=*(const Agent *const*)x,*b=*(const Agent *const*)y;return memcmp(a->genome,b->genome,GENOME);
}
static int cmp_agent_lineage(const void *x,const void *y) {
 const Agent *a=*(const Agent *const*)x,*b=*(const Agent *const*)y;
 return a->lineage<b->lineage?-1:a->lineage>b->lineage;
}
static int cmp_owner(const void *x,const void *y) {
 const OwnerSummary *a=x,*b=y;return a->owner<b->owner?-1:a->owner>b->owner;
}
static void concentration(uint64_t *values,size_t n,double *gini,double *top) {
 *gini=0;*top=0;if(!n)return;qsort(values,n,sizeof(*values),cmp64);
 long double sum=0,weighted=0,topsum=0;size_t k=(n+99)/100;
 for(size_t i=0;i<n;i++) { sum+=values[i];weighted+=(long double)(i+1)*values[i];if(i>=n-k)topsum+=values[i]; }
 if(sum) { *gini=(double)((2*weighted)/((long double)n*sum)-((long double)n+1)/n);*top=(double)(topsum/sum); }
}
static void export_state(const char *dir,const char *prefix) {
 World w=load(dir);uint32_t n=count_cells(&w),pop=w.population;
 Agent **living=alloc(pop+1,sizeof(*living));uint64_t *hashes=alloc(pop+1,sizeof(*hashes));
 uint64_t *cache_values=alloc(n,sizeof(*cache_values)),*occupied_values=alloc(pop+1,sizeof(*occupied_values));
 LineageSummary *ls=alloc(pop+1,sizeof(*ls));OwnerSummary *owners=alloc(n,sizeof(*owners));
 uint64_t mineral=0,food=0,detritus=0,structure=0,cache=0,body=0,store=0,energy=0;
 uint64_t occupied_total=0,occupied_positive=0,positive=0,trace_cache=0,trace_cells=0;
 uint32_t j=0,no=0;
 for(uint32_t i=0;i<n;i++) {
  Cell *c=&w.cells[i];mineral+=c->mineral;food+=c->food;detritus+=c->detritus;structure+=c->structure;cache+=c->cache;
  cache_values[i]=c->cache;if(c->cache)positive++;
  if(c->cache&&c->owner) { owners[no++]=(OwnerSummary){c->owner,1,c->cache};trace_cache+=c->cache;trace_cells++; }
 }
 FILE *agents=suffix_output(prefix,".agents.tsv");
 fprintf(agents,"id\tparent\tlineage\tborn\tage\tenergy\tbody\tstore\tgenotype_hash\tgenome_hex\n");
 for(uint32_t i=0;i<w.c.capacity;i++)if(w.agents[i].alive) {
  Agent *a=&w.agents[i];living[j]=a;hashes[j]=genotype(a);occupied_values[j]=w.cells[a->pos].cache;j++;
  body+=a->body;store+=a->store;energy+=a->energy;occupied_total+=w.cells[a->pos].cache;
  if(w.cells[a->pos].cache)occupied_positive++;
  fprintf(agents,"%"PRIu64"\t%"PRIu64"\t%"PRIu64"\t%"PRIu64"\t%u\t%u\t%u\t%u\t%016"PRIx64"\t",a->id,a->parent,a->lineage,a->born,a->age,a->energy,a->body,a->store,genotype(a));
  for(unsigned k=0;k<GENOME;k++)fprintf(agents,"%02x",a->genome[k]);
  fputc('\n',agents);
 }
 close_output(agents);
 qsort(living,pop,sizeof(*living),cmp_agent_genome);qsort(hashes,pop,sizeof(*hashes),cmp64);
 uint32_t exact=0,nhash=0,nlineages=0;
 for(uint32_t i=0;i<pop;i++) {
  if(!i||memcmp(living[i]->genome,living[i-1]->genome,GENOME))exact++;
  if(!i||hashes[i]!=hashes[i-1])nhash++;
 }
 qsort(living,pop,sizeof(*living),cmp_agent_lineage);
 for(uint32_t i=0;i<pop;i++) {
  Agent *a=living[i];if(!i||a->lineage!=living[i-1]->lineage)ls[nlineages++].lineage=a->lineage;
  LineageSummary *l=&ls[nlineages-1];l->population++;l->body+=a->body;l->store+=a->store;l->energy+=a->energy;
  l->local_cache+=w.cells[a->pos].cache;l->local_food+=w.cells[a->pos].food;
 }
 double gini,top,ogini,otop;concentration(cache_values,n,&gini,&top);concentration(occupied_values,pop,&ogini,&otop);
 qsort(owners,no,sizeof(*owners),cmp_owner);uint32_t nowners=0;
 for(uint32_t i=0;i<no;i++) {
  if(!nowners||owners[i].owner!=owners[nowners-1].owner)owners[nowners++]=owners[i];
  else { owners[nowners-1].cells+=owners[i].cells;owners[nowners-1].cache+=owners[i].cache; }
 }
 FILE *lf=suffix_output(prefix,".lineages.tsv");
 fprintf(lf,"lineage\tpopulation\tbody\tstore\tenergy\tlocal_cache\tlocal_food\n");
 for(uint32_t i=0;i<nlineages;i++) { LineageSummary *l=&ls[i];
  fprintf(lf,"%"PRIu64"\t%"PRIu64"\t%"PRIu64"\t%"PRIu64"\t%"PRIu64"\t%"PRIu64"\t%"PRIu64"\n",l->lineage,l->population,l->body,l->store,l->energy,l->local_cache,l->local_food);
 }
 close_output(lf);FILE *of=suffix_output(prefix,".owners.tsv");fprintf(of,"owner_trace_low32\tpositive_cache_cells\tcache\n");
 for(uint32_t i=0;i<nowners;i++)fprintf(of,"%u\t%"PRIu64"\t%"PRIu64"\n",owners[i].owner,owners[i].cells,owners[i].cache);
 close_output(of);FILE *f=suffix_output(prefix,".json");
 fprintf(f,"{\n\"schema\":\"relief.offline.state.v1\",\"tick\":%"PRIu64",\"population\":%u,\"rng_hex\":\"%016"PRIx64"\",\"next_id\":%"PRIu64",\"initial_matter\":%"PRIu64",\"matter\":%"PRIu64",\n",w.tick,pop,w.rng,w.next_id,w.initial_matter,matter(&w));
 fprintf(f,"\"config\":{\"width\":%u,\"height\":%u,\"capacity\":%u,\"founders\":%u,\"season\":%u,\"sample\":%u,\"checkpoint\":%u,\"seed\":%"PRIu64"},\n",w.c.width,w.c.height,w.c.capacity,w.c.founders,w.c.season,w.c.sample,w.c.checkpoint,w.c.seed);
 fprintf(f,"\"counters\":{\"births\":%"PRIu64",\"deaths\":%"PRIu64",\"moves\":%"PRIu64",\"eats\":%"PRIu64",\"builds\":%"PRIu64",\"breaks\":%"PRIu64",\"gifts\":%"PRIu64",\"attacks\":%"PRIu64",\"parasites\":%"PRIu64",\"mutations\":%"PRIu64",\"energy_spent\":%"PRIu64",\"solar_food\":%"PRIu64",\"sensor_witness\":%"PRIu64",\"memory_witness\":%"PRIu64",\"witness_samples\":%"PRIu64"},\n",w.births,w.deaths,w.moves,w.eats,w.builds,w.breaks,w.gifts,w.attacks,w.parasites,w.mutations,w.energy_spent,w.solar_food,w.sensor_witness,w.memory_witness,w.witness_samples);
 fprintf(f,"\"resources\":{\"mineral\":%"PRIu64",\"food\":%"PRIu64",\"detritus\":%"PRIu64",\"structure\":%"PRIu64",\"cache\":%"PRIu64",\"body\":%"PRIu64",\"store\":%"PRIu64",\"energy\":%"PRIu64"},\n",mineral,food,detritus,structure,cache,body,store,energy);
 fprintf(f,"\"diversity\":{\"genotypes_exact\":%u,\"genotype_hashes\":%u,\"lineages\":%u},\n",exact,nhash,nlineages);
 fprintf(f,"\"caches\":{\"positive_cells\":%"PRIu64",\"total\":%"PRIu64",\"gini_all_cells\":%.12g,\"top1pct_share_all_cells\":%.12g,\"occupied_total\":%"PRIu64",\"occupied_positive_cells\":%"PRIu64",\"gini_occupied\":%.12g,\"top1pct_share_occupied\":%.12g},\n",positive,cache,gini,top,occupied_total,occupied_positive,ogini,otop);
 fprintf(f,"\"owner_trace\":{\"meaning\":\"historical builder lineage low32, not enforced cache ownership\",\"positive_cache_cells_with_trace\":%"PRIu64",\"cache_with_trace\":%"PRIu64",\"distinct_trace_values\":%u},\n\"lineages\":[",trace_cells,trace_cache,nowners);
 for(uint32_t i=0;i<nlineages;i++) { LineageSummary *l=&ls[i];
  fprintf(f,"%s{\"lineage\":%"PRIu64",\"population\":%"PRIu64",\"body\":%"PRIu64",\"store\":%"PRIu64",\"energy\":%"PRIu64",\"local_cache\":%"PRIu64",\"local_food\":%"PRIu64"}",i?",":"",l->lineage,l->population,l->body,l->store,l->energy,l->local_cache,l->local_food);
 }
 fprintf(f,"]\n}\n");close_output(f);free(living);free(hashes);free(cache_values);free(occupied_values);free(ls);free(owners);release(&w);
}

static World normalized_world(World w) { w.cells=NULL;w.agents=NULL;w.occupancy=NULL;w.events=NULL;return w; }
static void same_world_header(const World *a,const World *b) {
 World aa=normalized_world(*a),bb=normalized_world(*b);if(memcmp(&aa,&bb,sizeof(aa)))fail("transformation changed World/config/counters/RNG bytes");
}
static void same_entire_state(const World *a,const World *b) {
 same_world_header(a,b);
 if(memcmp(a->cells,b->cells,(size_t)count_cells(a)*sizeof(Cell))||memcmp(a->agents,b->agents,(size_t)a->c.capacity*sizeof(Agent))||memcmp(a->occupancy,b->occupancy,(size_t)count_cells(a)*sizeof(int32_t)))fail("save/reload state differs");
}
static void copy_checkpoint_exact(const char *src,const char *dst) {
 char inpath[PATH_MAX],outpath[PATH_MAX];path(inpath,sizeof(inpath),src,"state.ck");path(outpath,sizeof(outpath),dst,"state.ck");
 FILE *in=fopen(inpath,"rb"),*out=new_output(outpath);if(!in)die("copy checkpoint read");unsigned char b[65536];size_t got;
 while((got=fread(b,1,sizeof(b),in)))if(fwrite(b,1,got,out)!=got)die("copy checkpoint write");
 if(ferror(in)||fclose(in))die("copy checkpoint read");
 close_output(out);
}
static void transform_state(const char *source,const char *destination,const char *mode) {
 int unlock=!strcmp(mode,"unlock"),redistribute=!strcmp(mode,"redistribute"),control=!strcmp(mode,"control");
 if(!unlock&&!redistribute&&!control)fail("mode must be control, unlock or redistribute");
 char src[PATH_MAX],dst[PATH_MAX];if(!realpath(source,src))die("source realpath");new_path(destination,dst,sizeof(dst));
 if(directory_prefix(src,dst))fail("destination must be outside source directory");
 World before=load(src),after=load(src);uint32_t n=count_cells(&before);
 uint64_t strata_total[256]={0},strata_n[256]={0},strata_seen[256]={0};
 for(uint32_t p=0;p<n;p++)if(before.occupancy[p]>=0&&before.cells[p].cache) { strata_total[p&255]+=before.cells[p].cache;strata_n[p&255]++; }
 if(unlock)for(uint32_t p=0;p<n;p++) { uint32_t d=after.cells[p].cache/4;if(d>100000000-after.cells[p].food)fail("unlock exceeds food bound");after.cells[p].cache-=d;after.cells[p].food+=d; }
 if(redistribute)for(uint32_t p=0;p<n;p++)if(before.occupancy[p]>=0&&before.cells[p].cache) {
  unsigned s=p&255;uint64_t value=strata_total[s]/strata_n[s]+(strata_seen[s]++<strata_total[s]%strata_n[s]);
  if(!value||value>100000000)fail("redistribution support/bounds failed");
  after.cells[p].cache=(uint32_t)value;
 }
 same_world_header(&before,&after);
 if(memcmp(before.agents,after.agents,(size_t)before.c.capacity*sizeof(Agent))||memcmp(before.occupancy,after.occupancy,(size_t)n*sizeof(int32_t)))fail("transformation changed agents or occupancy");
 uint64_t changed_cache=0,changed_food=0,released=0,support_before=0,support_after=0;
 uint64_t total_before[256]={0},total_after[256]={0},occupied_before=0,occupied_after=0;
 for(uint32_t p=0;p<n;p++) {
  Cell a=before.cells[p],b=after.cells[p],allowed=a;unsigned s=p&255;
  if(a.cache)support_before++;
  if(b.cache)support_after++;
  total_before[s]+=a.cache;total_after[s]+=b.cache;
  if(before.occupancy[p]>=0) { occupied_before+=a.cache;occupied_after+=b.cache; }
  if(unlock) { uint32_t d=a.cache/4;allowed.cache-=d;allowed.food+=d;released+=d;if(a.cache&&!b.cache)fail("unlock removed positive support"); }
  if(redistribute&&before.occupancy[p]>=0&&a.cache)allowed.cache=b.cache;
  if(memcmp(&allowed,&b,sizeof(Cell)))fail("transformation changed forbidden cell bytes");
  if(a.cache!=b.cache)changed_cache++;
  if(a.food!=b.food)changed_food++;
 }
 if(support_before!=support_after)fail("transformation changed cache positive support");
 if(redistribute) {
  if(occupied_before!=occupied_after)fail("redistribution changed occupied cache total");
  for(unsigned s=0;s<256;s++)if(total_before[s]!=total_after[s])fail("redistribution changed phase-stratum cache total");
 }
 validate_world(&after);if(matter(&before)!=matter(&after)||before.rng!=after.rng)fail("matter/RNG changed");
 if(mkdir(dst,0750))die("new transformation directory");
 if(control)copy_checkpoint_exact(src,dst);else save(&after,dst);
 World reloaded=load(dst);same_entire_state(&after,&reloaded);
 char p[PATH_MAX];path(p,sizeof(p),dst,"transformation.json");FILE *f=new_output(p);
 fprintf(f,"{\n\"schema\":\"relief.offline.transform.v1\",\"mode\":\"%s\",\"tick\":%"PRIu64",\"matter_before\":%"PRIu64",\"matter_after\":%"PRIu64",\"rng_before_hex\":\"%016"PRIx64"\",\"rng_after_hex\":\"%016"PRIx64"\",\n",mode,before.tick,matter(&before),matter(&after),before.rng,after.rng);
 fprintf(f,"\"changed_fields\":{\"cell_cache\":%"PRIu64",\"cell_food\":%"PRIu64",\"other_cell_bytes\":0,\"agent_bytes\":0,\"world_bytes\":0,\"occupancy_bytes\":0},\"released_cache_to_food\":%"PRIu64",\"cache_positive_cells_before\":%"PRIu64",\"cache_positive_cells_after\":%"PRIu64",\"occupied_cache_before\":%"PRIu64",\"occupied_cache_after\":%"PRIu64",\n",changed_cache,changed_food,released,support_before,support_after,occupied_before,occupied_after);
 fprintf(f,"\"strict_contract_passed\":true,\"save_reload_bytewise_state_equal\":true,\"rng_calls\":0,\"strata\":[");
 for(unsigned s=0;s<256;s++)fprintf(f,"%s{\"phase\":%u,\"eligible_cells\":%"PRIu64",\"eligible_cache_before\":%"PRIu64",\"all_cache_before\":%"PRIu64",\"all_cache_after\":%"PRIu64"}",s?",":"",s,strata_n[s],strata_total[s],total_before[s],total_after[s]);
 fprintf(f,"]\n}\n");close_output(f);
 path(p,sizeof(p),dst,"transformation.cells.tsv");FILE *audit=new_output(p);
 fprintf(audit,"position\tphase\toccupant_id\toccupant_lineage\towner_trace_low32\tcache_before\tcache_after\tfood_before\tfood_after\n");
 for(uint32_t pos=0;pos<n;pos++)if(before.cells[pos].cache!=after.cells[pos].cache||before.cells[pos].food!=after.cells[pos].food) {
  int32_t slot=before.occupancy[pos];const Agent *a=slot>=0?&before.agents[slot]:NULL;
  fprintf(audit,"%u\t%u\t%"PRIu64"\t%"PRIu64"\t%u\t%u\t%u\t%u\t%u\n",pos,pos&255,a?a->id:0,a?a->lineage:0,before.cells[pos].owner,before.cells[pos].cache,after.cells[pos].cache,before.cells[pos].food,after.cells[pos].food);
 }
 close_output(audit);printf("%s tick=%"PRIu64" matter=%"PRIu64" rng=%016"PRIx64" cache_fields=%"PRIu64" food_fields=%"PRIu64" released=%"PRIu64" strict_contract=PASS\n",mode,before.tick,matter(&after),after.rng,changed_cache,changed_food,released);
 release(&before);release(&after);release(&reloaded);
}
int main(int argc,char **argv) {
 if(argc==4&&!strcmp(argv[1],"export")) { export_state(argv[2],argv[3]);return 0; }
 if(argc==5&&!strcmp(argv[1],"transform")) { transform_state(argv[2],argv[3],argv[4]);return 0; }
 fprintf(stderr,"usage: state_tool export DIR PREFIX\n       state_tool transform SRC NEW_DEST control|unlock|redistribute\n");return 2;
}
