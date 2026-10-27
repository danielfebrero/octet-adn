#define main original_main
#include "common.h"
#include "frozen/relief.c"
#undef main
static void append(World*w,const char*file){FILE*f=records(file);Agent a;while(read_record(f,&a,sizeof a)){int s=free_slot(w);if(s<0)fail("capacity exceeded");uint32_t p;do{p=below(w,count_cells(w));}while(w->cells[p].rock||w->occupancy[p]>=0);a.id=w->next_id++;a.parent=0;a.lineage=a.id;a.born=w->tick;a.pos=p;w->agents[s]=a;w->occupancy[p]=s;w->population++;}fclose(f);w->initial_matter=matter(w);validate_world(w);}
int main(int argc,char**argv){if(argc<3)fail("init DIR RAW SEED | step DIR N | inspect DIR | inject DIR RAW");const char*d=argv[2];World w;
if(!strcmp(argv[1],"init")){if(argc<5)fail("args");refuse_existing(d);mkdir(d,0700);Config c=defaults();c.width=128;c.height=128;c.capacity=4096;c.founders=0;c.seed=number(argv[4]);w=initial(c);append(&w,argv[3]);save(&w,d);}
else{w=load(d);if(!strcmp(argv[1],"step")){if(argc<4)fail("args");w.c.season=(uint32_t)parameter(d,"season",w.c.season,16,10000000);unsigned char b[1];if(stimulus(d,b,1))for(uint32_t i=0;i<count_cells(&w);i++)w.cells[i].signal=b[0];begin_emit(d);for(unsigned n=number32(argv[3]);n&&w.population;n--)step(&w);end_emit();save(&w,d);}else if(!strcmp(argv[1],"inject")){if(argc<4)fail("args");append(&w,argv[3]);save(&w,d);}else if(!strcmp(argv[1],"export")){if(argc<4)fail("args");FILE*f=fopen(argv[3],"wb");if(!f)fail("export open");for(unsigned i=0;i<w.c.capacity;i++)if(w.agents[i].alive&&fwrite(&w.agents[i],sizeof(Agent),1,f)!=1)fail("export write");durable(f);}else if(strcmp(argv[1],"inspect"))fail("command");}
inspect(&w,stdout);release(&w);return 0;}
