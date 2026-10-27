#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
static FILE *emissions;
static void adapter_emit(unsigned b){if(emissions&&ftell(emissions)<65536)fputc(b&255,emissions);}
static void bridge_path(char *p,const char *d,const char *n){if(snprintf(p,4096,"%s/%s",d,n)>=4096){fprintf(stderr,"path too long\n");exit(1);}}
static void durable(FILE *f){if(fflush(f)||fsync(fileno(f))||fclose(f)){perror("write");exit(1);}}
static size_t stimulus(const char *d,unsigned char *b,size_t n){char p[4096];bridge_path(p,d,"stimulus.bin");FILE *f=fopen(p,"rb");if(!f)return 0;size_t z=fread(b,1,n,f);fclose(f);return z;}
static double parameter(const char *d,const char *key,double def,double lo,double hi){char p[4096],k[128];double v;bridge_path(p,d,"params.tsv");FILE*f=fopen(p,"r");if(!f)return def;while(fscanf(f,"%127s %lf",k,&v)==2){if(!strcmp(k,key)){fclose(f);if(!(v>=lo&&v<=hi)){fprintf(stderr,"invalid param %s\n",key);exit(1);}return v;}}fclose(f);return def;}
static FILE *records(const char*p){FILE*f=fopen(p,"rb");if(!f){perror(p);exit(1);}return f;}
static int read_record(FILE *f,void*p,size_t n){size_t z=fread(p,1,n,f);if(z&&z!=n){fprintf(stderr,"truncated native record\n");exit(1);}return z!=0;}
static void begin_emit(const char*d){char p[4096];bridge_path(p,d,"emissions.bin");emissions=fopen(p,"wb");if(!emissions){perror(p);exit(1);}}
static void end_emit(void){durable(emissions);emissions=NULL;}

static void refuse_existing(const char*d){char p[4096];bridge_path(p,d,"state.ck");if(!access(p,F_OK)){fprintf(stderr,"refusing init over existing world\n");exit(1);}}
