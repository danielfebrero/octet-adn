#define main ignored_main
#include "/workspace/produits/octet-adn/experiments/raison.c"
#undef main

static void hx(const void *p,size_t n){const unsigned char*b=p;for(size_t i=0;i<n;i++)printf("%02x",b[i]);}

static void emit(Creature*c,long tick,int island){
 if(c->e<=0||c->len<=0||c->len>MAX_LEN)return;
 printf("{\"source_id\":%d,\"parent\":%d,\"age\":%d,\"energy\":%d,\"steps\":%d,\"stores\":%d,\"useful_stores\":%d,\"eaten\":%d,\"splits\":%d,\"species\":%d,\"pc\":%d,\"a\":%u,\"b\":%u,\"tick\":%ld,\"island\":%d,\"genome_hex\":\"",c->id,c->parent,c->age,c->e,c->steps,c->stores,c->useful_stores,c->eaten,c->splits,c->species,c->ip,c->a,c->b,tick,island);hx(c->dna,c->len);printf("\",\"memory_hex\":\"\",\"native_hex\":\"");hx(c,sizeof(*c));puts("\"}");}
static Creature*pool[262144];static int islands[262144],n;
static int metric;static int sorter(const void*a,const void*b){const Creature*x=*(Creature**)a,*y=*(Creature**)b; long xx=metric==0?x->age:metric==1?x->e:metric==2?x->useful_stores:metric==3?x->splits:metric==4?x->steps:metric==5?x->stores:metric==6?x->len:x->eaten; long yy=metric==0?y->age:metric==1?y->e:metric==2?y->useful_stores:metric==3?y->splits:metric==4?y->steps:metric==5?y->stores:metric==6?y->len:y->eaten;return xx==yy?x->id-y->id:xx<yy?1:-1;}
static void choose(long tick){for(metric=0;metric<8;metric++){qsort(pool,n,sizeof(*pool),sorter);for(int j=0;j<n&&j<32;j++)emit(pool[j],tick,0);}}

int main(int argc,char**argv){Eco*E=calloc(1,sizeof(*E));FILE*f=fopen(argv[1],"rb");if(!f||fread(E,sizeof(*E),1,f)!=1||E->magic!=MAGIC)return 3;fclose(f);for(int j=0;j<E->n;j++)pool[n++]=&E->ind[j].c;choose(E->step);return 0;}
