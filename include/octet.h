/* Octet-ADN — moteur natif. Un génome = une suite d'octets ; ce binaire est leur ribosome. */
#ifndef OCTET_H
#define OCTET_H
#include <stdint.h>
#include <stddef.h>

#define MAX_LEN       1024   /* taille max d'un génome (octets) */
#define MIN_LEN       8
#define OPS_PER_TICK  16     /* budget d'instructions par tick (introns gratuits) */
#define N_SYN         5      /* 4 poids (food, voisins, énergie, stimulus) + biais */
#define POOL_SIZE     16
#define POOL_MAX      32
#define ACCEPT_RATIO  0.9    /* règle historique (--select legacy) */
#ifndef SEL_EVALS
#define SEL_EVALS     4      /* sélection appariée : nb de graines de réévaluation */
#endif
#ifndef PP_PRED_MAX_AGE
#define PP_PRED_MAX_AGE 100  /* predator-prey : âge max d'un prédateur (ticks) */
#endif
#ifndef PP_PRED_METAB
#define PP_PRED_METAB   0    /* predator-prey : métabolisme supplémentaire des prédateurs */
#endif
#ifndef PP_ATTACK_COST
#define PP_ATTACK_COST  1    /* predator-prey : coût d'un ATTACK */
#endif
#define HIST_N        256
#define GRID_W        64
#define GRID_H        32
#define CMAX          1024   /* plafond dur de créatures par liste */
#define N_EXTRA       6
#define N_LABS        18
#define N_CORE_LABS   10     /* les 10 labos historiques (org-seeds ne croise que leurs champions) */

/* ---------------- RNG déterministe (xoshiro256**) ---------------- */
typedef struct { uint64_t s[4]; } Rng;
void     rng_seed(Rng *r, uint64_t seed);
uint64_t rng_next(Rng *r);
double   rng_double(Rng *r);                 /* [0,1) */
uint32_t rng_below(Rng *r, uint32_t n);      /* [0,n) */
int      rng_int(Rng *r, int a, int b);      /* [a,b] inclus */
double   rng_gauss(Rng *r, double mu, double sigma);
uint64_t fnv1a(const void *p, size_t n, uint64_t h);
#define FNV_INIT 0xcbf29ce484222325ULL

/* ---------------- ISA ---------------- */
enum {
    OP_NOP = 0x00, OP_INC_A, OP_DEC_A, OP_INC_B, OP_DEC_B, OP_CLR_A, OP_STORE, OP_LOAD,
    OP_JMP, OP_JZ, OP_SENSE, OP_TURN, OP_STEP, OP_EAT, OP_SPLIT, OP_YIELD,
    OP_SENSE_NB = 0x10, OP_SENSE_EN, OP_SENSE_STIM, OP_NEURAL, OP_EMIT, OP_MOV_BA, OP_MOV_AB,
    OP_ADD, OP_SUB, OP_XOR, OP_ATTACK, OP_JNZ, OP_LOADI, OP_SENSE_HIST, OP_LOOP, OP_AND,
    OP_COUNT = 0x20
};
int         op_valid(uint8_t op);
int         op_has_imm(uint8_t op);
const char *op_name(uint8_t op);             /* "INTRON" si hors ISA */
/* désassemble dans out (texte), retourne le nb d'octets écrits */
size_t      disasm(const uint8_t *dna, size_t len, int skip_introns, char *out, size_t cap);

/* ---------------- Créatures / monde ---------------- */
typedef struct Creature {
    int x, y, d;
    uint8_t a, b;
    int ip, e, age, id, species, parent;
    int stores, useful_stores, eaten, steps, emitted, splits;   /* emitted = -1 si rien */
    int len;
    uint8_t dna[MAX_LEN];
    uint8_t dirty[MAX_LEN / 8];   /* codons réécrits par STORE, pas encore exécutés */
    /* état propre à un labo (0 ailleurs, ne change rien aux 10 labos historiques) */
    int ls[6]; double lscore;
    int win_len;                  /* code-repair : > 0 = fenêtre patient mappée sur STORE/LOAD imm >= 0xE0 */
    uint8_t win[32], win0[32];
    /* Lignée (general) : copie de naissance. STORE n'y écrit pas. 0 = les autres labos l'ignorent. */
    int germ_len;
    uint8_t germ[MAX_LEN];
} Creature;

typedef struct {
    int w, h;
    double food_density;
    int food_regrow, max_pop, split_energy, start_energy;
    double mut_rate, dup_rate;
    int crossover;
    double pred_eat_factor;
    int attack_steal;
    /* prédation (espèces != 0) : 0 = désactivé, valeurs neutres hors predator-prey */
    int attack_cost;      /* énergie payée par ATTACK (défaut 1) */
    int pred_metab;       /* métabolisme supplémentaire par tick des prédateurs */
    int pred_max_age;     /* plafond d'âge des prédateurs (0 = aucun) */
    int self_evo;         /* self-evolve : paramètres de mutation lus dans le génome à SPLIT (0 = constantes) */
    int eat_guard;        /* commons : 1 = manger une case < 8 ou la vider baisse lscore (0 ailleurs) */
    int ops_per_tick;     /* general : budget d'instructions par tick (0 = OPS_PER_TICK) */
    int germline;         /* general : 1 = SPLIT et la sélection copient germ, pas le soma */
} WorldConfig;
WorldConfig cfg_default(void);

typedef struct {
    long born, dead, death_age_sum, store_writes, useful_stores, emits,
         energy_eaten, crossovers, attacks;
} Stats;

typedef struct { Creature *c; int v; } Emission;

struct Lab;
typedef struct World {
    WorldConfig cfg;
    Rng *rng;
    struct Lab *lab;
    uint8_t food[GRID_H][GRID_W];
    int occ[GRID_H][GRID_W];
    long t;
    int nid, stimulus;
    uint8_t hist[HIST_N];
    int hist_n, hist_pos;
    Emission em[CMAX]; int nem;
    Stats st;
    Creature *cs[CMAX]; int ncs;
    Creature *nb[CMAX]; int nnb;
    void (*before_vm)(struct World *w, Creature *c);   /* hook par créature (NULL hors labos qui le posent) */
} World;

typedef struct { int species, len; uint8_t dna[MAX_LEN]; } PoolEntry;

World   *world_new(const PoolEntry *genomes, int n, Rng *rng, const WorldConfig *cfg, struct Lab *lab);
void     world_free(World *w);
void     world_step(World *w);
uint64_t world_hash(const World *w);
Creature *creature_new(int x, int y, const uint8_t *dna, int len, int e, int id, int species, int parent);
int      food_ahead(const World *w, const Creature *c);
int      neighbors(const World *w, const Creature *c);
int      history_at(const World *w, int k);
void     world_spawn(World *w, Creature *c);   /* ajoute une créature (rejoint la population en fin de tick) */

/* VM : exécute un tick ; 0 = la créature meurt */
int vm_run(Creature *c, World *w);

/* mutations */
int mutate(const uint8_t *in, int n, Rng *r, double rate, double dup_rate, uint8_t *out);
int duplicate_block(uint8_t *buf, int n, Rng *r);       /* in place, retourne la nouvelle taille */
int crossover(const uint8_t *a, int la, const uint8_t *b, int lb, Rng *r, uint8_t *out);
/* self-evolve : paramètres d'évolution portés par le génome */
#define EVO_MUT_MAX   0.12
#define EVO_INDEL_MAX 0.06
#define EVO_PROT_MAX  16
#define EVO_META      0.08   /* taux de dérive (±4, ±16) des octets-paramètres 2..5, non protégeables */
typedef struct { double mut, indel; int xover, prot; } EvoParams;
EvoParams evo_params(const uint8_t *dna, int len);
int mutate_evo(const uint8_t *in, int n, Rng *r, const EvoParams *p, uint8_t *out);

/* ---------------- Labos ---------------- */
typedef struct Lab Lab;
typedef struct LabDef {
    const char *name, *title, *fitness_doc;
    const char *extra_names[N_EXTRA];
    void   (*config)(WorldConfig *c);
    int    (*seeds)(PoolEntry *out);
    void   (*pre)(Lab *l, World *w);
    void   (*post)(Lab *l, World *w);
    double (*fitness)(Lab *l, World *w, int ticks);
    void   (*rank)(const Creature *c, double k[2]);
    int    (*select)(Lab *l, World *w, PoolEntry *out);  /* NULL = top-16 par rank */
    void   (*init)(Lab *l, World *w);   /* après world_new (NULL = rien) */
    int    ticks_mult;                  /* horizon = ticks × mult (0 = 1) */
    int    show_extra;                  /* > 0 : la fitness affichée est extra[show_extra - 1] (la sélection garde le retour de fitness) */
} LabDef;

struct Lab {
    const LabDef *def;
    int id;
    int *pops, npops;
    double extra[N_EXTRA];
    /* état des labos de communication / prédiction */
    long correct, total, base_correct, base_total;
    Rng series; int series_init;
    int pend_id[CMAX], pend_v[CMAX], npend;
    int prev; int has_prev;
    double ar0, ar1, px;
    const double *csv; int ncsv, csv_i;
    uint8_t seq[8]; int nseq, seq_pos;
    int *prey, *pred;
    /* org-seeds : champions étrangers */
    PoolEntry *foreign; int nforeign;
    /* labos humanité */
    int counterfactual;                  /* 1 = rejouer sans interventions / paramètres fixes */
    const PoolEntry *pool0; int npool0; Rng rng0;
    void *ext;                           /* état alloué par le labo (libéré par lab_free) */
    void (*ext_free)(struct Lab *l);     /* libère ce que ext contient (appelé avant free(ext)) */
    double acc[16];
    int eval_only;                       /* 1 = monde jetable de pool_eval (rien n'est sélectionné) */
};

extern const LabDef LABS[N_LABS];
int  lab_index(const char *name);
/* general : un génome rejoué seul sur la mémoire 2 et 3 tenues à l'écart (voir labs_h.c) */
#define GN_CHECK_N 8
int  gn_check(const PoolEntry *g, double *v);   /* v : GN_CHECK_N valeurs */
void lab_init(Lab *l, int id, int max_ticks);
void lab_free(Lab *l);
void stock_set_csv(const double *closes, int n);

typedef struct {
    World *w;
    double fitness;
    PoolEntry newpool[POOL_MAX]; int nnew;
    int survived;
} CycleResult;
void run_cycle(Lab *l, const PoolEntry *pool, int n, Rng *rng, int ticks, CycleResult *res);

/* seeds embarquées (.incbin des fichiers seeds/NOM.adn, voir src/seeds.S) */
typedef struct { const char *name; const uint8_t *p; int len; } SeedBlob;
extern const int N_SEEDS;
const SeedBlob *seed_at(int i);
const SeedBlob *seed_get(const char *name);
void seed_entry(PoolEntry *e, const char *name, int species);

/* ---------------- Persistance binaire ---------------- */
#define CK_MAGIC  "OADNCKP1"
#define MET_MAGIC "OADNMET1"
typedef struct {
    int lab_id; uint32_t cycle; double best_fitness; uint64_t engine;
    PoolEntry best; int has_best;
    PoolEntry pool[POOL_MAX]; int npool;
} Checkpoint;
int ck_save(const char *path, const Checkpoint *ck);
int ck_load(const char *path, Checkpoint *ck);

typedef struct __attribute__((packed)) {
    int64_t  ts;            /* unix s */
    int32_t  tz_off;        /* décalage local (s) */
    uint32_t cycle, ticks, survived;
    double   fitness, best_fitness;
    uint8_t  accepted, lab_id;
    uint16_t pop_final;
    double   pop_mean;
    uint32_t born, dead, store_writes, useful_stores, emits;
    double   genome_len_mean;
    uint64_t engine, state_hash;
    double   elapsed_s;
    double   extra[N_EXTRA];
} MetricRec;
int met_append(const char *path, const MetricRec *r);
int met_read(const char *path, MetricRec **out);   /* retourne n, -1 si erreur */
void met_format(const MetricRec *r, char *out, size_t cap);

/* ---------------- Runner ---------------- */
typedef struct {
    int labs[N_LABS]; int nlabs;
    int ticks, cycles, cycles_done, dry_run, no_reload, quiet;
    int sel_legacy;            /* 1 = ancienne règle « garder si ≥ 90 % du meilleur historique » */
    uint64_t seed;
    double sleep_s;
    const char *data_dir;
    const char *exe_path;
    char **argv;
} RunOpts;
uint64_t engine_version(void);
int runner(RunOpts *o, MetricRec *rows_out, int rows_cap);   /* retourne nb de lignes */
/* fitness d'un pool sur la graine dérivée de key (monde jetable, rien n'est écrit) */
double pool_eval(int lab_id, const PoolEntry *pool, int n, const char *key, int ticks, PoolEntry *foreign, int nforeign);
/* idem, et copie les extras du labo (N_EXTRA) dans extra si non NULL */
double pool_eval_x(int lab_id, const PoolEntry *pool, int n, const char *key, int ticks, PoolEntry *foreign, int nforeign, double *extra);
/* champions des 9 autres labos (checkpoints de dir), pour org-seeds ; retourne leur nombre */
int collect_foreign(const char *dir, PoolEntry *out);
int write_status(const char *data_dir, const int *labs, int nlabs);
#endif
