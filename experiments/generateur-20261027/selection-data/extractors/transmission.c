#define main ignored_main
#include "/workspace/produits/octet-adn/experiments/transmission-20261026/transmission.c"
#undef main

static void hx(const void *p,size_t n){const unsigned char*b=p;for(size_t i=0;i<n;i++)printf("%02x",b[i]);}

int main(int argc,char**argv){directory=argv[1];World*w=load();for(unsigned j=0;j<CAP;j++){Agent*a=&w->agents[j];if(!a->alive)continue;printf("{\"source_id\":%lu,\"parent\":%lu,\"lineage\":%lu,\"age\":%u,\"energy\":%u,\"kind\":%u,\"pc\":%u,\"p\":%u,\"a\":%u,\"b\":%u,\"source\":%u,\"book\":%u,\"outlen\":%u,\"last_mail\":%lu,\"last_reward\":%lu,\"last_request\":%lu,\"tick\":%lu,\"genome_hex\":\"",a->id,a->parent,a->lineage,a->age,a->energy,a->kind,a->pc,a->p,a->a,a->b,a->source,a->book,a->outlen,a->last_mail,a->last_reward,a->last_request,w->tick);hx(a->code,CODE);printf("\",\"memory_hex\":\"");hx(a->ram,RAM);printf("\",\"native_hex\":\"");hx(a,sizeof(*a));puts("\"}");}return 0;}
