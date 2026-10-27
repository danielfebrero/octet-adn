#define main ignored_main
#include "/workspace/produits/octet-adn/experiments/relief-20261025/relief.c"
#undef main

static void hx(const void *p,size_t n){const unsigned char*b=p;for(size_t i=0;i<n;i++)printf("%02x",b[i]);}

int main(int argc,char**argv){World w=load(argv[1]);for(unsigned j=0;j<w.c.capacity;j++){Agent*a=&w.agents[j];if(!a->alive)continue;printf("{\"source_id\":%lu,\"parent\":%lu,\"lineage\":%lu,\"age\":%u,\"energy\":%u,\"body\":%u,\"store\":%u,\"pc\":%u,\"dir\":%u,\"tick\":%lu,\"genome_hex\":\"",a->id,a->parent,a->lineage,a->age,a->energy,a->body,a->store,a->pc,a->dir,w.tick);hx(a->genome,GENOME);printf("\",\"memory_hex\":\"");hx(a->memory,MEM);printf("\",\"native_hex\":\"");hx(a,sizeof(*a));puts("\"}");}return 0;}
