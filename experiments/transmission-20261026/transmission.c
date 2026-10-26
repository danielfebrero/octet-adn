#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <dirent.h>
#include <time.h>

/* New experiment, deliberately no Relief ABI or source dependency. */
#define CAP 128
#define CODE 512
#define RAM 4096
#define OUTPUT 4096
#define BOOKS 8
#define BOOKSIZE 65536
#define ARTIFACTS 8
#define MAILMAX 16384
#define MAILBYTES (64u*1024u*1024u)
#define MAGIC UINT64_C(0x31534e415254434f)
#define VERSION 1
enum { NOP, IMM, LOAD, STORE, NEXT, PREV, ZERO, JUMP, JZERO, EMIT,
       EAT, SPLIT, RESTORE, MAIL, RANDOM, XOR, ADD, SWAP, SOURCE, BOOK,
       REQUEST, OUTCLEAR, SETLO, SETHI, GETLO, GETHI, INC, DEC,
       SHAREDSTORE, CODESTORE, SENSE, YIELD };
typedef struct {
 uint64_t id,parent,lineage,born,last_mail,last_reward,last_request;
 uint32_t age,energy;
 uint16_t pc,p,outlen;
 uint8_t a,b,source,book,alive,kind,pad[2];
 uint8_t code[CODE],ram[RAM],output[OUTPUT];
} Agent;
typedef struct {
 uint64_t magic,version,size,checksum;
} Header;
typedef struct {
 uint64_t tick,rng,next_id,births,deaths,mutations,mail_seq,mail_bytes;
 uint64_t restores,perfect,awarded,mail_rejected,requests,request_rejected;
 uint64_t library_reads,library_imports,shared_writes,code_writes,ram_writes;
 uint64_t keeper_perfect,descendant_perfect,energy_reward,work,last_dispatch;
 uint32_t population,founders,custodians,document_len;
 uint8_t target[OUTPUT]; /* Evaluation only: never in the VM address space. */
 uint8_t shared[ARTIFACTS][RAM];
 uint64_t shared_owner[ARTIFACTS];
 uint32_t booklen[BOOKS];
 uint64_t bookhash[BOOKS];
 uint8_t library[BOOKS][BOOKSIZE];
 Agent agents[CAP];
} World;
static volatile sig_atomic_t stopping;
static const char *directory;
static int reward_disabled; /* Selftest ablation only; no CLI or agent control. */
static void stop_signal(int sig) { (void)sig; stopping=1; }
static void fail(const char *s) { fprintf(stderr,"transmission: %s\n",s); exit(1); }
static void die(const char *s) { perror(s); exit(1); }
static uint64_t hash(const void *p,size_t n) {
 const uint8_t *b=p; uint64_t h=UINT64_C(1469598103934665603);
 for(size_t i=0;i<n;i++) { h^=b[i]; h*=UINT64_C(1099511628211); } return h;
}
static uint64_t random64(World *w) {
 uint64_t x=w->rng; x^=x>>12; x^=x<<25; x^=x>>27; w->rng=x;
 return x*UINT64_C(2685821657736338717);
}
static void path(char *s,size_t n,const char *name) {
 if(snprintf(s,n,"%s/%s",directory,name)<0||strlen(directory)+strlen(name)+2>n) fail("path too long");
}
static void atomic_bytes(const char *name,const void *p,size_t n) {
 char f[1024],tmp[1050]; path(f,sizeof(f),name);
 if(snprintf(tmp,sizeof(tmp),"%s.tmp",f)<0) fail("temporary path");
 FILE *out=fopen(tmp,"wb"); if(!out) die(tmp);
 if(fwrite(p,1,n,out)!=n||fflush(out)||fsync(fileno(out))||fclose(out)) die("atomic write");
 if(rename(tmp,f)) die("rename");
}
static void sync_directory(void) {
 int fd=open(directory,O_RDONLY|O_DIRECTORY); if(fd<0||fsync(fd)) die("directory sync"); close(fd);
}
static void validate(const World *w) {
 if(!w->rng||w->founders>CAP||w->custodians>w->founders||w->document_len==0||w->document_len>=OUTPUT||w->population>CAP||w->mail_seq>MAILMAX||w->mail_bytes>MAILBYTES) fail("invalid world bounds");
 unsigned pop=0;
 for(unsigned i=0;i<BOOKS;i++) if(w->booklen[i]>BOOKSIZE||hash(w->library[i],w->booklen[i])!=w->bookhash[i]) fail("library checksum");
 for(unsigned i=0;i<CAP;i++) {
  const Agent *a=&w->agents[i]; if(a->alive>1) fail("alive flag"); if(!a->alive) continue; pop++;
  if(!a->id||a->id>=w->next_id||!a->lineage||a->lineage>w->founders||a->parent>=a->id||a->born>w->tick||a->pc>=CODE||a->outlen>OUTPUT||a->source>3||a->book>=BOOKS||a->energy>4096) fail("invalid agent");
  for(unsigned j=0;j<i;j++) if(w->agents[j].alive&&w->agents[j].id==a->id) fail("duplicate agent");
 }
 if(pop!=w->population) fail("population mismatch");
}
static void save(const World *w) {
 validate(w); Header h={MAGIC,VERSION,sizeof(World),hash(w,sizeof(*w))};
 char f[1024],tmp[1050]; path(f,sizeof(f),"state.ck"); snprintf(tmp,sizeof(tmp),"%s.tmp",f);
 FILE *out=fopen(tmp,"wb"); if(!out) die(tmp);
 if(fwrite(&h,sizeof(h),1,out)!=1||fwrite(w,sizeof(*w),1,out)!=1||fflush(out)||fsync(fileno(out))||fclose(out)) die("checkpoint write");
 if(rename(tmp,f)) die("checkpoint rename");
 sync_directory();
 char cursor[64];int len=snprintf(cursor,sizeof(cursor),"%"PRIu64"\n",w->mail_seq);
 atomic_bytes("mail.committed",cursor,(size_t)len);sync_directory();
}
static World *load(void) {
 char f[1024]; path(f,sizeof(f),"state.ck"); FILE *in=fopen(f,"rb"); if(!in) die(f);
 Header h; World *w=calloc(1,sizeof(*w)); if(!w) die("calloc");
 if(fread(&h,sizeof(h),1,in)!=1||h.magic!=MAGIC||h.version!=VERSION||h.size!=sizeof(*w)||fread(w,sizeof(*w),1,in)!=1||fgetc(in)!=EOF||h.checksum!=hash(w,sizeof(*w))) fail("invalid checkpoint size/version/checksum");
 fclose(in); validate(w); return w;
}
static int spend(Agent *a,unsigned n) { if(a->energy<n) return 0; a->energy-=n; return 1; }
static uint8_t read_byte(World *w,Agent *a) {
 switch(a->source) {
 case 0:return a->ram[a->p%RAM];
 case 1:return a->code[a->p%CODE];
 case 2:return w->shared[a->book%ARTIFACTS][a->p%RAM];
 default:w->library_reads++; return a->p<w->booklen[a->book]?w->library[a->book][a->p]:0;
 }
}
static void reproduce(World *w,Agent *a) {
 if(a->energy<3000||w->population==CAP) return;
 unsigned slot=CAP; for(unsigned i=0;i<CAP;i++) if(!w->agents[i].alive) { slot=i; break; }
 if(slot==CAP) return;
 Agent *b=&w->agents[slot]; *b=*a; a->energy-=80; b->energy=a->energy/2; a->energy-=b->energy;
 b->id=w->next_id++; b->parent=a->id; b->born=w->tick; b->age=0; b->last_mail=w->tick; b->last_reward=w->tick;
 b->pc=0; b->p=0; b->outlen=0; b->a=0; b->b=0; b->source=0; b->book=0;
 /* Byte memory is inherited, with occasional errors. No target repair. */
 if(random64(w)%4==0) { unsigned p=random64(w)%RAM; b->ram[p]^=(uint8_t)(1u<<(random64(w)%8)); w->mutations++; }
 if(random64(w)%4==0) { unsigned p=random64(w)%CODE; b->code[p]^=(uint8_t)(1u<<(random64(w)%8)); w->mutations++; }
 w->population++; w->births++;
}
static int restitution(World *w,Agent *a) {
 w->restores++;
 if(a->outlen!=w->document_len||memcmp(a->output,w->target,w->document_len)) return 0;
 w->perfect++; if(a->kind==0) w->keeper_perfect++; if(a->parent) w->descendant_perfect++;
 if(!reward_disabled&&w->tick>=a->last_reward+512&&spend(a,16)) {
  unsigned n=1024+2*w->document_len;if(n>4096-a->energy)n=4096-a->energy;
  a->energy+=n; w->energy_reward+=n; w->awarded++; a->last_reward=w->tick;
 }
 return 1;
}
static int mail(World *w,Agent *a) {
 if(!a->outlen||(a->last_mail&&w->tick<a->last_mail+2048)||w->tick<w->last_dispatch+512||w->mail_seq>=MAILMAX||w->mail_bytes+a->outlen>MAILBYTES||!spend(a,32+a->outlen/32)) { w->mail_rejected++; return 0; }
 if(directory) {
  char file[96],metadata[768]; uint64_t seq=w->mail_seq+1;
  snprintf(file,sizeof(file),"mail/%016"PRIu64".bin",seq); atomic_bytes(file,a->output,a->outlen);
  int n=snprintf(metadata,sizeof(metadata),"sequence\ttick\tid\tparent\tlineage\tborn\tfounder\tseed_program\tbytes\tfnv64\n%"PRIu64"\t%"PRIu64"\t%"PRIu64"\t%"PRIu64"\t%"PRIu64"\t%"PRIu64"\t%u\t%u\t%u\t%016"PRIx64"\n",seq,w->tick,a->id,a->parent,a->lineage,a->born,a->parent==0,a->kind,a->outlen,hash(a->output,a->outlen));
  if(n<0||(size_t)n>=sizeof(metadata)) fail("metadata overflow");
  snprintf(file,sizeof(file),"mail/%016"PRIu64".tsv",seq); atomic_bytes(file,metadata,(size_t)n);
  char maildir[1024];path(maildir,sizeof(maildir),"mail");int fd=open(maildir,O_RDONLY|O_DIRECTORY);if(fd<0||fsync(fd))die("mail directory sync");close(fd);
 }
 w->mail_seq++; w->mail_bytes+=a->outlen; a->last_mail=w->tick; w->last_dispatch=w->tick; return 1;
}
static int valid_slug(const uint8_t *s,unsigned n) {
 if(!n||n>96) return 0;
 for(unsigned i=0;i<n;i++) if(!((s[i]>='A'&&s[i]<='Z')||(s[i]>='a'&&s[i]<='z')||(s[i]>='0'&&s[i]<='9')||s[i]=='_'||s[i]=='-'||s[i]=='('||s[i]==')')) return 0;
 return 1;
}
static void request(World *w,Agent *a) {
 if(!valid_slug(a->output,a->outlen)||(a->last_request&&w->tick<a->last_request+4096)||!spend(a,64)) { w->request_rejected++; return; }
 a->last_request=w->tick;
 if(directory) {
  char file[64],body[256]; snprintf(file,sizeof(file),"bridge/%u.req",a->book);
  int n=snprintf(body,sizeof(body),"%"PRIu64"\t%"PRIu64"\t%"PRIu64"\t%.*s\n",w->tick,a->id,++w->requests,a->outlen,a->output);
  if(n<0||(size_t)n>=sizeof(body)) fail("request overflow");
  atomic_bytes(file,body,(size_t)n);
 } else w->requests++;
}
static uint8_t fetch(Agent *a) { uint8_t b=a->code[a->pc]; a->pc=(a->pc+1)%CODE; return b; }
static uint16_t address(Agent *a) { unsigned lo=fetch(a); return (uint16_t)(lo|((unsigned)fetch(a)<<8)); }
static void execute(World *w,Agent *a) {
 for(unsigned j=0;j<32&&a->energy;j++) {
  if(!spend(a,1)) return;
  w->work++; unsigned op=fetch(a)&31;
  switch(op) {
  case NOP:break;
  case IMM:a->a=fetch(a);break;
  case LOAD:a->a=read_byte(w,a);break;
  case STORE:a->ram[a->p%RAM]=a->a;w->ram_writes++;break;
  case NEXT:a->p++;break;
  case PREV:a->p--;break;
  case ZERO:a->p=0;break;
  case JUMP:a->pc=address(a)%CODE;break;
  case JZERO: { uint16_t p=address(a); if(!a->a) a->pc=p%CODE; break; }
  case EMIT:if(a->outlen<OUTPUT) a->output[a->outlen++]=a->a;break;
  case EAT:a->energy=a->energy>4090?4096:a->energy+6;return;
  case SPLIT:reproduce(w,a);break;
  case RESTORE:(void)restitution(w,a);break;
  case MAIL:(void)mail(w,a);break;
  case RANDOM:a->a=(uint8_t)random64(w);break;
  case XOR:a->a^=a->b;break;
  case ADD:a->a=(uint8_t)(a->a+a->b);break;
  case SWAP: { uint8_t b=a->b; a->b=a->a; a->a=b; break; }
  case SOURCE:a->source=a->a&3;break;
  case BOOK:a->book=a->a%BOOKS;break;
  case REQUEST:request(w,a);break;
  case OUTCLEAR:a->outlen=0;break;
  case SETLO:a->p=(uint16_t)((a->p&0xff00)|a->a);break;
  case SETHI:a->p=(uint16_t)((a->p&255)|((unsigned)a->a<<8));break;
  case GETLO:a->a=(uint8_t)a->p;break;
  case GETHI:a->a=(uint8_t)(a->p>>8);break;
  case INC:a->a++;break;
  case DEC:a->a--;break;
  case SHAREDSTORE:if(spend(a,2)) { w->shared[a->book][a->p%RAM]=a->a; w->shared_owner[a->book]=a->id; w->shared_writes++; }break;
  case CODESTORE:if(spend(a,2)) { a->code[a->p%CODE]=a->a; w->code_writes++; }break;
  case SENSE:a->a=(uint8_t)(a->energy/16);break;
  case YIELD:return;
  }
 }
}
static void import_library(World *w) {
 if(!directory) return;
 for(unsigned i=0;i<BOOKS;i++) {
  char file[96],full[1024]; snprintf(file,sizeof(file),"bridge/%u.book",i); path(full,sizeof(full),file);
  FILE *in=fopen(full,"rb"); if(!in) { if(errno==ENOENT) continue; die("book open"); }
  uint8_t *b=malloc(BOOKSIZE+1); if(!b) die("malloc book"); size_t n=fread(b,1,BOOKSIZE+1,in);
  if(ferror(in)||n>BOOKSIZE) fail("oversized library response");
  fclose(in);
  uint64_t h=hash(b,n); if(h!=w->bookhash[i]||n!=w->booklen[i]) { memcpy(w->library[i],b,n); if(n<BOOKSIZE) memset(w->library[i]+n,0,BOOKSIZE-n); w->booklen[i]=(uint32_t)n; w->bookhash[i]=h; w->library_imports++; }
  free(b);
 }
}
static void tick(World *w) {
 if(!w->population) return;
 w->tick++; if(w->tick%64==0) import_library(w);
 for(unsigned i=0;i<CAP;i++) {
  Agent *a=&w->agents[i]; if(!a->alive||a->born==w->tick) continue;
  a->age++;
  if(!spend(a,1)||a->age>=16384) { a->alive=0;w->population--;w->deaths++;continue; }
  execute(w,a);
 }
}
/* Human-authored baselines. No natural-language generator, model, or tool use. */
static void seed_program(Agent *a,unsigned kind,unsigned slot) {
 memset(a->code,YIELD,CODE); unsigned p=0;
#define OP(x) (a->code[p++]=(uint8_t)(x))
#define IMM8(v) do { OP(IMM);OP(v); } while(0)
#define BR(op,to) do { OP(op);OP((to)&255);OP((to)>>8); } while(0)
 IMM8(slot);OP(BOOK);
 if(kind==0) {
  OP(OUTCLEAR); OP(ZERO); IMM8(0); OP(SOURCE);
  unsigned loop=p; OP(EAT);OP(LOAD);unsigned jz=p;BR(JZERO,0);OP(EMIT);OP(NEXT);BR(JUMP,loop);
  unsigned end=p; a->code[jz+1]=(uint8_t)end;a->code[jz+2]=(uint8_t)(end>>8);
  OP(RESTORE);OP(MAIL);OP(SPLIT);BR(JUMP,0);
 } else {
  /* Request seed title from RAM, then generate 256-byte sampled letters. */
  OP(OUTCLEAR);OP(ZERO);IMM8(0);OP(SOURCE);
  unsigned loop=p;OP(EAT);OP(LOAD);unsigned jz=p;BR(JZERO,0);OP(EMIT);OP(NEXT);BR(JUMP,loop);
  unsigned end=p;a->code[jz+1]=(uint8_t)end;a->code[jz+2]=(uint8_t)(end>>8);
  OP(REQUEST);OP(OUTCLEAR);OP(ZERO);
  unsigned sample=p;OP(EAT);
  if(kind==1) { IMM8(3);OP(SOURCE);OP(LOAD); }
  else { OP(RANDOM);OP(STORE);OP(SHAREDSTORE); }
  OP(EMIT);OP(NEXT);OP(GETLO);unsigned mailpc=p+6;BR(JZERO,mailpc);BR(JUMP,sample);
  OP(MAIL);OP(SPLIT);OP(OUTCLEAR);BR(JUMP,0);
 }
#undef OP
#undef IMM8
#undef BR
}
static World *foundation(unsigned founders,uint64_t seed,const uint8_t *document,unsigned len) {
 if(founders>CAP||!seed||!len||len>=RAM) fail("invalid foundation");
 World *w=calloc(1,sizeof(*w)); if(!w) die("calloc");
 w->rng=seed;w->next_id=1;w->founders=founders;w->custodians=founders<8?founders:8;w->document_len=len;
 memcpy(w->target,document,len);
 for(unsigned i=0;i<BOOKS;i++) w->bookhash[i]=hash(w->library[i],0);
 const char *titles[]={"Artificial_life","Information_theory","Error_correction_code","Evolution"};
 for(unsigned i=0;i<founders;i++) {
  Agent *a=&w->agents[i];a->id=w->next_id++;a->lineage=a->id;a->energy=4096;a->alive=1;a->kind=i<w->custodians?0:(i%2?1:2);
  seed_program(a,a->kind,(i/2)%4);
  if(a->kind==0) memcpy(a->ram,document,len);
  else { const char *title=titles[(i/2)%4];memcpy(a->ram,title,strlen(title)); }
  w->population++;
 }
 validate(w);return w;
}
static void inspect(const World *w) {
 unsigned custodians=0,descendants=0;for(unsigned i=0;i<CAP;i++)if(w->agents[i].alive){custodians+=w->agents[i].kind==0;descendants+=w->agents[i].parent!=0;}
 printf("tick=%"PRIu64" population=%u custodians=%u descendants=%u births=%"PRIu64" deaths=%"PRIu64" mutations=%"PRIu64"\n",w->tick,w->population,custodians,descendants,w->births,w->deaths,w->mutations);
 printf("restorations=%"PRIu64" perfect=%"PRIu64" descendant_perfect=%"PRIu64" awarded=%"PRIu64" reward_energy=%"PRIu64" document_bytes=%u document_fnv64=%016"PRIx64"\n",w->restores,w->perfect,w->descendant_perfect,w->awarded,w->energy_reward,w->document_len,hash(w->target,w->document_len));
 printf("mail=%"PRIu64" mail_bytes=%"PRIu64" mail_rejected=%"PRIu64" requests=%"PRIu64" request_rejected=%"PRIu64" library_reads=%"PRIu64" library_imports=%"PRIu64" shared_writes=%"PRIu64" ram_writes=%"PRIu64" code_writes=%"PRIu64"\n",w->mail_seq,w->mail_bytes,w->mail_rejected,w->requests,w->request_rejected,w->library_reads,w->library_imports,w->shared_writes,w->ram_writes,w->code_writes);
 printf("books=");for(unsigned i=0;i<BOOKS;i++)printf("%s%u",i?",":"",w->booklen[i]);puts("");
}
static unsigned number(const char *s) { char *end=NULL;errno=0;unsigned long n=strtoul(s,&end,10);if(errno||!s[0]||*end||n>UINT32_MAX)fail("invalid number");return(unsigned)n; }
static void recover_mail(const World *w) {
 char full[1024];path(full,sizeof(full),"mail");DIR *d=opendir(full);if(!d)die("mail directory");struct dirent *e;
 while((e=readdir(d))) {
  uint64_t seq;char suffix[32];if(sscanf(e->d_name,"%16"SCNu64".%31s",&seq,suffix)!=2)continue;
  if(seq>w->mail_seq||strstr(suffix,"tmp")) {char name[300];snprintf(name,sizeof(name),"mail/%s",e->d_name);path(full,sizeof(full),name);if(unlink(full))die("mail rollback");}
 }closedir(d);
}
static void selftest(void) {
 const uint8_t doc[]="We are byte organisms. Humans host this world.\n";
 World *w=foundation(32,73,doc,sizeof(doc)-1);
 Agent *a=&w->agents[0];w->tick=1024;
 a->outlen=w->document_len;memset(a->output,0,a->outlen);
 if(restitution(w,a)||w->awarded)fail("oracle leak on empty restitution");
 for(unsigned i=0;i<w->document_len;i++) {a->ram[i]^=0xa5;w->shared[0][i]=a->ram[i];}
 a->source=2;a->b=0xa5;
 memset(a->code,YIELD,CODE);a->outlen=0;a->p=0;a->pc=0;
 const uint8_t decoder[]={LOAD,XOR,EMIT,NEXT,GETLO,IMM,0};
 /* One byte per explicit VM invocation, no host-side decoding or emission. */
 memcpy(a->code,decoder,4);a->code[4]=YIELD;
 for(unsigned i=0;i<w->document_len;i++) {a->pc=0;execute(w,a);}
 a->code[0]=RESTORE;a->code[1]=YIELD;a->pc=0;execute(w,a);
 if(w->awarded!=1||w->perfect!=1)fail("VM alternative support/decoding failed");
 if(!restitution(w,a)||w->awarded!=1)fail("reward cooldown failed");
 a->output[4]^=1;if(restitution(w,a))fail("corruption rewarded");
 if(valid_slug((const uint8_t*)"https://evil.example",20)||valid_slug((const uint8_t*)"../x",4)||!valid_slug((const uint8_t*)"Artificial_life",15))fail("slug allowlist");
 free(w);w=foundation(32,73,doc,sizeof(doc)-1);
 World *copy=malloc(sizeof(*copy));if(!copy)die("malloc");memcpy(copy,w,sizeof(*copy));
 for(unsigned i=0;i<6000;i++)tick(w);
 for(unsigned i=0;i<2300;i++)tick(copy);
 World *resumed=malloc(sizeof(*copy));if(!resumed)die("malloc");memcpy(resumed,copy,sizeof(*copy));free(copy);
 for(unsigned i=0;i<3700;i++)tick(resumed);
 if(memcmp(w,resumed,sizeof(*w)))fail("determinism");
 if(!w->perfect||!w->awarded||!w->mail_seq||!w->births)fail("baseline channels not exercised");
 inspect(w);free(w);free(resumed);
 /* Fixed, unmutated, non-reproducing custodians isolate the reward effect. */
 for(unsigned trial=0;trial<2;trial++) {
  reward_disabled=(int)trial;w=foundation(8,73,doc,sizeof(doc)-1);
  for(unsigned i=0;i<8;i++)for(unsigned j=0;j<CODE;j++)if(w->agents[i].code[j]==SPLIT)w->agents[i].code[j]=NOP;
  for(unsigned i=0;i<6000;i++)tick(w);
  printf("reward_ablation disabled=%u tick=%"PRIu64" population=%u energy_reward=%"PRIu64"\n",trial,w->tick,w->population,w->energy_reward);
  if((trial==0&&w->population!=8)||(trial==1&&w->population))fail("reward ablation did not distinguish survival");
  free(w);
 }
 reward_disabled=0;
 w=foundation(0,73,doc,sizeof(doc)-1);for(unsigned i=0;i<100;i++)tick(w);if(w->population||w->tick||w->births)fail("reseed after extinction");free(w);
 puts("SELFTEST PASS: hidden oracle, exact reward, decoded shared representation, cooldown, URL rejection, deterministic continuation, reproduction, mail, extinction");
}
int main(int argc,char **argv) {
 if(argc==2&&!strcmp(argv[1],"selftest")){selftest();return 0;}
 if(argc<3)fail("usage: init DIR DOCUMENT [FOUNDERS SEED] | run DIR [TICKS MS] | inspect DIR");
 directory=argv[2];
 if(!strcmp(argv[1],"init")) {
  if(argc!=4&&argc!=6)fail("init arguments");
  uint8_t doc[OUTPUT];FILE *in=fopen(argv[3],"rb");if(!in)die("document");size_t n=fread(doc,1,sizeof(doc),in);if(ferror(in)||!n||n>=sizeof(doc))fail("document too large/empty");fclose(in);
  if(memchr(doc,0,n))fail("baseline document must be non-NUL; target supports arbitrary bytes through other founders");
  if(mkdir(directory,0770))die("init requires absent directory");
  const char *dirs[]={"mail","bridge","pickups"};for(unsigned i=0;i<3;i++){char f[1024];path(f,sizeof(f),dirs[i]);if(mkdir(f,0770))die("mkdir");}
  World *w=foundation(argc==6?number(argv[4]):32,argc==6?number(argv[5]):20261026,doc,(unsigned)n);save(w);inspect(w);free(w);return 0;
 }
 if(!strcmp(argv[1],"inspect")){if(argc!=3)fail("inspect arguments");World *w=load();inspect(w);free(w);return 0;}
 if(strcmp(argv[1],"run")||argc>5)fail("unknown command/arguments");
 char f[1024];path(f,sizeof(f),"runner.lock");int lock=open(f,O_RDWR|O_CREAT,0660);if(lock<0||flock(lock,LOCK_EX|LOCK_NB))die("exclusive writer lock");
 World *w=load();recover_mail(w);uint64_t ticks=argc>3?number(argv[3]):0;unsigned ms=argc>4?number(argv[4]):100;if(ms>60000)fail("tick delay");
 signal(SIGTERM,stop_signal);signal(SIGINT,stop_signal);uint64_t start=w->tick;
 while(!stopping&&w->population&&(!ticks||w->tick-start<ticks)) {
  tick(w);if(w->tick%256==0)save(w);
  if(ms){struct timespec ts={(time_t)(ms/1000),(long)(ms%1000)*1000000};while(nanosleep(&ts,&ts)&&errno==EINTR&&!stopping){}}
 }
 save(w);inspect(w);if(!w->population)puts("EXTINCTION: checkpoint preserved; no founders will be recreated");
 free(w);close(lock);return 0;
}
