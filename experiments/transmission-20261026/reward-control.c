/* Isolated in-memory controls. Never loads or modifies a living checkpoint. */
#define main transmission_cli_main
#include "transmission.c"
#undef main
int main(int argc,char **argv) {
 if(argc!=2)fail("control requires initial document");
 FILE *in=fopen(argv[1],"rb");if(!in)die("control document");
 uint8_t document[OUTPUT];size_t n=fread(document,1,sizeof(document),in);fclose(in);
 if(!n||n>=RAM)fail("control document size");
 for(unsigned trial=0;trial<2;trial++) {
  reward_disabled=(int)trial;World *w=foundation(8,20261026,document,(unsigned)n);
  for(unsigned i=0;i<8;i++)for(unsigned j=0;j<CODE;j++)if(w->agents[i].code[j]==SPLIT)w->agents[i].code[j]=NOP;
  for(unsigned i=0;i<10000;i++)tick(w);
  printf("initial_document_control disabled=%u tick=%"PRIu64" population=%u perfect=%"PRIu64" energy_reward=%"PRIu64"\n",trial,w->tick,w->population,w->perfect,w->energy_reward);
  if((trial==0&&w->population!=8)||(trial==1&&w->population))fail("real document reward control failed");
  free(w);
 }
 reward_disabled=0;
 World *w=foundation(32,20261026,document,(unsigned)n);
 for(unsigned i=0;i<200000;i++)tick(w);
 puts("200000-tick control, no external library, no disk/mail side effects:");inspect(w);
 if(!w->population||!w->births||!w->deaths)fail("renewal control");
 free(w);return 0;
}
