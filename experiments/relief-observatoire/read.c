#define main relief_original_main
#include "../relief-20261025/relief.c"
#undef main
/* Separate reader. Never calls init, run, save or action. ABI/checksum validated
 * by the original loader. choose only operates on a local copy. */
int main(int argc,char **argv) {
 if(argc<2)return 2;
 World w=load(argv[1]);
 uint32_t x=argc>2?number32(argv[2]):w.c.width/2,y=argc>3?number32(argv[3]):w.c.height/2;
 uint64_t id=argc>4?number(argv[4]):0;
 Agent *selected=NULL;
 for(uint32_t i=0;i<w.c.capacity;i++)if(w.agents[i].alive&&id&&w.agents[i].id==id)selected=&w.agents[i];
 if(selected){x=selected->pos%w.c.width;y=selected->pos/w.c.width;}
 x%=w.c.width;y%=w.c.height;
 printf("{\"tick\":%"PRIu64",\"population\":%u,\"width\":%u,\"height\":%u,\"matter\":%"PRIu64",\"initial_matter\":%"PRIu64",\"rng\":\"%016"PRIx64"\",\"births\":%"PRIu64",\"deaths\":%"PRIu64",\"map\":[",w.tick,w.population,w.c.width,w.c.height,matter(&w),w.initial_matter,w.rng,w.births,w.deaths);
 unsigned sx=w.c.width/128,sy=w.c.height/64;if(!sx)sx=1;if(!sy)sy=1;
 for(unsigned by=0;by<64;by++)for(unsigned bx=0;bx<128;bx++){
  uint64_t food=0,elev=0,structure=0;unsigned pop=0,rock=0,n=0;
  for(unsigned dy=0;dy<sy;dy++)for(unsigned dx=0;dx<sx;dx++){
   unsigned cx=bx*sx+dx,cy=by*sy+dy;if(cx>=w.c.width||cy>=w.c.height)continue;
   unsigned p=cy*w.c.width+cx;Cell*c=&w.cells[p];n++;food+=c->food;elev+=c->elevation;structure+=c->structure;pop+=w.occupancy[p]>=0;rock+=c->rock!=0;
  }
  if(!n)n=1;
  printf("%s[%u,%"PRIu64",%"PRIu64",%"PRIu64",%u]",bx||by?",":"",pop,food/n,elev/n,structure/n,rock);
 }
 printf("],\"x\":%u,\"y\":%u,\"nearby\":[",x,y);unsigned printed=0;
 for(int dy=-8;dy<=8;dy++)for(int dx=-8;dx<=8;dx++){
  int cx=(int)x+dx,cy=(int)y+dy;if(cx<0||cy<0||cx>=(int)w.c.width||cy>=(int)w.c.height)continue;
  unsigned p=(unsigned)cy*w.c.width+(unsigned)cx;Cell*c=&w.cells[p];int a=w.occupancy[p];
  printf("%s[%d,%d,%u,%u,%u,\"%"PRIu64"\"]",printed++?",":"",cx,cy,c->food,c->structure,c->rock,a<0?0:w.agents[a].id);
  if(!id&&!selected&&a>=0)selected=&w.agents[a];
 }
 printf("],\"agent\":");
 if(!selected)printf("null");else{
  Agent a=*selected;unsigned steps=0;uint8_t ins=choose(&w,&a,0,0,&steps);Agent b=*selected,c=*selected;
  uint8_t nos=choose(&w,&b,1,0,NULL),nom=choose(&w,&c,0,1,NULL);
  printf("{\"id\":\"%"PRIu64"\",\"parent\":\"%"PRIu64"\",\"lineage\":\"%"PRIu64"\",\"x\":%u,\"y\":%u,\"age\":%u,\"born\":%"PRIu64",\"energy\":%u,\"body\":%u,\"store\":%u,\"pc\":%u,\"dir\":%u,\"choice\":%u,\"next_pc\":%u,\"steps\":%u,\"no_sensors\":%u,\"no_memory\":%u,\"genome\":[",selected->id,selected->parent,selected->lineage,selected->pos%w.c.width,selected->pos/w.c.width,selected->age,selected->born,selected->energy,selected->body,selected->store,selected->pc,selected->dir,ins,a.pc,steps,nos,nom);
  for(unsigned j=0;j<GENOME;j++)printf("%s%u",j?",":"",selected->genome[j]);
  printf("],\"memory\":[");for(unsigned j=0;j<MEM;j++)printf("%s%u",j?",":"",selected->memory[j]);
  printf("],\"sensors\":[");for(unsigned j=0;j<8;j++)printf("%s%u",j?",":"",sense(&w,selected,j));printf("]}");
 }
 printf("}\n");release(&w);return 0;
}
