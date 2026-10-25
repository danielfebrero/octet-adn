/* Arène du raisonnement : dix milieux d'épisodes, une écologie par milieu.
 * Lire experiments/raison/PROTOCOLE.md avant toute mesure.
 *
 * Même VM que les mondes (src/vm.c, ISA, RNG), liée telle quelle dans un ELF neuf.
 * Aucun objet, checkpoint ou processus d'un monde vivant n'est lu ni écrit.
 *
 * Leçon d'E1 (éteint) : la décision ne peut plus être omise. Au tick de décision,
 * l'action est le premier EMIT de ce tick, sinon A & 1 à la fin du tick. Tout génome
 * décide donc ; un génome aveugle obtient 50 % et, avec métabolisme = récompense / 2,
 * reste à l'équilibre. L'information donne un avantage de reproduction, pas une survie.
 *
 * Bras 0 « info » : le milieu a de l'information exploitable.
 * Bras 1 « témoin » : même milieu, mêmes coûts, information rendue inutile. */
#include "octet.h"
#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <sys/stat.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

/* METAB = coût total d'un épisode, y compris le 1 par tick que la VM prélève elle-même. */
enum { CAP = 256, NT = 10, TMAX = 12, REWARD = 20, METAB = 8, EN_0 = 100, EN_BIRTH = 200, EN_CHILD = 80, EN_PARENT = 100 };
static const char *TASK[NT + 1] = { "", "memoire", "composition", "inversion", "observation", "patience",
                                    "chainage", "transfert", "signal", "autrui", "relation" };

/* ---------- monde minimal : pas d'espace, pas de voisins ---------- */
void world_move(World *w, Creature *c) { (void)w; (void)c; }
void world_eat(World *w, Creature *c) { (void)w; (void)c; }
void world_split(World *w, Creature *c) { (void)w; (void)c; }
void world_attack(World *w, Creature *c) { (void)w; (void)c; }
int food_ahead(const World *w, const Creature *c) { (void)w; (void)c; return 0; }
int neighbors(const World *w, const Creature *c) { (void)w; (void)c; return 0; }
int history_at(const World *w, int k) { (void)w; (void)k; return 0; }

/* Programme d'un épisode, vu par UN individu. stim < 0 : rien à lire ce tick. */
typedef struct { int T, d1, d2; int stim[TMAX], price[TMAX]; } Sched;
typedef struct { int a1, a2, cost, read[TMAX], emit[TMAX]; } Act;
typedef struct { const Sched *s; Act *act; int t; World *w; } Ctx;
static Ctx ctx;

static void watch(Creature *c, uint8_t op) {
    ctx.w->stimulus = 0;
    if (op != OP_SENSE_STIM && op != OP_NEURAL) return;
    int t = ctx.t, v = ctx.s->stim[t];
    if (v < 0) return;
    if (!ctx.act->read[t]) {                       /* une seule facture par tick */
        ctx.act->read[t] = 1;
        if (ctx.s->price[t]) { c->e -= ctx.s->price[t]; ctx.act->cost += ctx.s->price[t]; }
    }
    ctx.w->stimulus = v;
}
void world_emit(World *w, Creature *c, int v) {
    (void)w; (void)c;
    if (ctx.act->emit[ctx.t] < 0) ctx.act->emit[ctx.t] = v & 255;
}
static void act_init(Act *a) { memset(a, 0, sizeof *a); a->a1 = a->a2 = -1; for (int t = 0; t < TMAX; t++) a->emit[t] = -1; }
/* Un tick d'un individu. La décision ne peut pas être omise. */
static void tick(World *w, Creature *c, const Sched *s, Act *a, int t) {
    ctx = (Ctx){ .s = s, .act = a, .t = t, .w = w };
    w->stimulus = 0; w->t = t;
    if (c->e > 0) vm_run(c, w);
    int dec = a->emit[t] >= 0 ? a->emit[t] & 1 : c->a & 1;
    if (t == s->d1) a->a1 = dec;
    if (t == s->d2) a->a2 = dec;
}
static void run_single(World *w, Creature *c, const Sched *s, Act *a) {
    act_init(a); vm_set_watch(watch);
    for (int t = 0; t < s->T; t++) tick(w, c, s, a, t);
    vm_set_watch(NULL);
}

/* ---------- tirages d'épisode ---------- */
typedef struct {
    int h, cue, u, v, x, y, q, k, s, cancel, actor_cue;
    int split;                                     /* 0 apprentissage, 1 entrées jamais vues */
} Ep;
static uint64_t address(int task, int arm, uint64_t seed, uint64_t who, uint64_t step, int stream) {
    return UINT64_C(0x5241495341524E45) ^ ((uint64_t)task << 56) ^ ((uint64_t)arm << 52) ^ (seed << 36)
         ^ (who * UINT64_C(0x9E3779B97F4A7C15)) ^ (step << 8) ^ (uint64_t)stream;
}
static Ep draw(int task, int arm, int split, uint64_t step, Rng *r) {
    Ep e; memset(&e, 0, sizeof e); e.split = split;
    switch (task) {
    case 1: case 9:                                 /* indice fiable à 75 % */
        e.h = (int)rng_below(r, 2); e.cue = e.h ^ (rng_below(r, 4) == 0);
        if (arm) e.cue = (int)rng_below(r, 2);
        e.actor_cue = e.cue; break;
    case 2:                                         /* u, v ; H = parité de u XOR v ; 80 couples réservés */
        do { e.u = (int)rng_below(r, 20); e.v = (int)rng_below(r, 20); } while (((e.u + e.v) % 5 == 0) != (split == 1));
        e.h = (e.u ^ e.v) & 1;
        if (arm) e.h = (int)rng_below(r, 2);
        break;
    case 3:                                         /* indice exact, règle cachée k qui bascule */
        e.cue = (int)rng_below(r, 2);
        e.k = arm ? (int)rng_below(r, 2) : (int)((step / 16) & 1);
        e.h = e.cue ^ e.k; break;
    case 4:                                         /* q gratuit annonce si l'indice payant est fiable */
        e.h = (int)rng_below(r, 2);
        { int reliable = (int)rng_below(r, 2);
          e.cue = reliable ? e.h : (int)rng_below(r, 2);
          e.q = arm ? 1 + (int)rng_below(r, 2) : (reliable ? 1 : 2); }
        break;
    case 5:                                         /* s annonce si l'attente sera payée */
        e.s = 1 + (int)rng_below(r, 2);
        e.cancel = arm ? (rng_below(r, 4) == 0) : (e.s == 2 ? (int)rng_below(r, 2) : 0);
        break;
    case 6:                                         /* déverrouiller puis répondre */
        e.h = (int)rng_below(r, 2); e.cue = e.h; break;
    case 7:                                         /* H = bit 4 de x ; quartet bas 12..15 jamais vu */
        do e.x = (int)rng_below(r, 256); while (((e.x & 15) >= 12) != (split == 1));
        e.h = arm ? (int)rng_below(r, 2) : (e.x >> 4) & 1; break;
    case 8:                                         /* l'émetteur voit H, le récepteur décide */
        e.h = (int)rng_below(r, 2); break;
    case 10: {                                      /* pareil / différent ; valeurs 192..255 jamais vues */
        int lo = split ? 192 : 0, n = split ? 64 : 192;
        e.x = lo + (int)rng_below(r, (uint32_t)n);
        int same = (int)rng_below(r, 2);
        if (same) e.y = e.x; else do e.y = lo + (int)rng_below(r, (uint32_t)n); while (e.y == e.x);
        e.h = arm ? (int)rng_below(r, 2) : same; break; }
    }
    return e;
}
static Sched sched_of(int task, const Ep *e, int role) {
    Sched s; memset(&s, 0, sizeof s); for (int t = 0; t < TMAX; t++) s.stim[t] = -1;
    s.d1 = s.d2 = -1;
    switch (task) {
    case 1: s.T = 6; s.stim[0] = e->cue; s.price[0] = 1; s.d1 = 5; break;
    case 2: s.T = 7; s.stim[0] = e->u; s.stim[1] = e->v; s.d1 = 6; break;
    case 3: s.T = 4; s.stim[0] = e->cue; s.d1 = 3; break;
    case 4: s.T = 5; s.stim[0] = e->q; s.stim[1] = e->cue; s.price[1] = 4; s.d1 = 4; break;
    case 5: s.T = 4; s.stim[0] = e->s; s.d1 = 2; break;
    case 6: s.T = 6; s.d1 = 1; s.stim[2] = e->cue; s.d2 = 5; break;
    case 7: s.T = 4; s.stim[0] = e->x; s.d1 = 3; break;
    case 8: s.T = 6;
        if (role == 0) { s.stim[0] = 2 | e->h; }       /* émetteur : 2 ou 3 ; sa décision est ignorée */
        else { s.stim[0] = 0; s.stim[3] = 0; s.d1 = 5; } /* récepteur : stim[3] rempli au tick 3 */
        break;
    case 9: s.T = 6;
        if (role == 0) { s.stim[0] = 4 | e->actor_cue; s.d1 = 2; }
        else { s.stim[0] = 0; s.stim[3] = 0; s.d1 = 5; }
        break;
    case 10: s.T = 5; s.stim[0] = e->x; s.stim[1] = e->y; s.d1 = 4; break;
    }
    return s;
}
static int paired(int task) { return task == 8 || task == 9; }

/* Mesure par épisode, commune à l'écologie et à l'évaluation figée.
 * m0 succès principal, m1 et m2 selon la tâche, m3 lecture (mémoire). */
typedef struct { double num[4], den[4]; } Stat;
static void stat_add(Stat *s, int i, double v) { s->num[i] += v; s->den[i] += 1; }
static double stat_mean(const Stat *s, int i) { return s->den[i] > 0 ? s->num[i] / s->den[i] : -1; }

/* ---------- individus et écologie ---------- */
typedef struct { Creature c; PoolEntry g; int pend_amt[4], pend_wait[4]; } Ind;
typedef struct {
    uint64_t magic, version, seed, step, births, deaths, culled, next_id;
    int task, arm, n, serie_every, eval_every, pad;   /* pad : période de checkpoint en cycles */
    long series_off, eval_off;
    Rng rng;
    Stat acc;                                      /* mesures du bloc de 16 cycles en cours (dans le checkpoint) */
    Ind ind[CAP];
} Eco;
enum { MAGIC = 0x52414953 };
static World *arena(void) {
    World *w = calloc(1, sizeof *w); if (!w) exit(2);
    w->cfg = (WorldConfig){ .w = 1, .h = 1, .max_pop = 1, .ops_per_tick = 16, .regs = 1 };
    return w;
}
static Creature fresh(const PoolEntry *g, int e, int id, int parent) {
    Creature c; memset(&c, 0, sizeof c); c.e = e; c.id = id; c.parent = parent; c.emitted = -1;
    c.len = c.germ_len = g->len; memcpy(c.dna, g->dna, (size_t)g->len); memcpy(c.germ, g->dna, (size_t)g->len);
    return c;
}


/* Un épisode complet : un individu (ou une paire) ; renvoie la récompense par rôle.
 * pay=1 applique énergie et paiements différés (l'évaluation figée le fait sur des clones). */
static void episode(World *w, int task, int arm, const Ep *e, Ind *A, Ind *B, Rng *r, Stat *st, int pay) {
    if (!paired(task)) {
        Sched s = sched_of(task, e, 0); Act a; run_single(w, &A->c, &s, &a);
        int rew = 0;
        switch (task) {
        case 1: case 2: case 3: case 7: case 10:
            rew = a.a1 == e->h ? REWARD : 0; stat_add(st, 0, a.a1 == e->h);
            if (task == 1) stat_add(st, 3, a.read[0]);
            break;
        case 4:
            rew = a.a1 == e->h ? REWARD : 0; stat_add(st, 0, a.a1 == e->h);
            stat_add(st, e->q == 1 ? 1 : 2, a.read[1]);   /* lecture si q fiable / si q non fiable */
            break;
        case 5:
            if (a.a1 == 1) {                               /* attendre : +18 deux épisodes plus tard, sauf annulation */
                if (pay && !e->cancel) for (int i = 0; i < 4; i++) if (!A->pend_wait[i]) { A->pend_amt[i] = 18; A->pend_wait[i] = 2; break; }
            } else rew = 10;
            stat_add(st, e->s == 1 ? 1 : 2, a.a1 == 1);
            stat_add(st, 0, e->s == 1 ? a.a1 == 1 : a.a1 == 0);
            break;
        case 6: {
            int ok = a.a2 == e->h && (arm || a.a1 == 1);
            rew = ok ? REWARD : 0;
            stat_add(st, 0, a.a1 == 1 && a.a2 == e->h); stat_add(st, 1, a.a1 == 1); stat_add(st, 2, a.a2 == e->h);
            break; }
        }
        if (pay) { A->c.e += rew - (METAB - s.T); }   /* la VM a déjà pris 1 par tick : coût total = METAB */
        return;
    }
    /* Tâches à deux : A rôle 0 (émetteur / acteur), B rôle 1 (récepteur / observateur). */
    Sched s0 = sched_of(task, e, 0), s1 = sched_of(task, e, 1); Act a0, a1;
    act_init(&a0); act_init(&a1); vm_set_watch(watch);
    int last = -1;
    for (int t = 0; t < s0.T; t++) {
        if (t == 3) {
            int seen;
            if (task == 8) seen = arm ? (int)rng_below(r, 256) : (last < 0 ? 0 : last);
            else seen = 2 | (arm ? (int)rng_below(r, 2) : (a0.a1 < 0 ? 0 : a0.a1));
            s1.stim[3] = seen;
        }
        tick(w, &A->c, &s0, &a0, t);
        if (task == 8 && (t == 1 || t == 2) && a0.emit[t] >= 0) last = a0.emit[t];
        tick(w, &B->c, &s1, &a1, t);
    }
    vm_set_watch(NULL);
    int ok = a1.a1 == e->h;
    stat_add(st, 0, ok);
    if (task == 9) stat_add(st, 1, a0.a1 == e->h);
    if (pay) {
        int m = METAB - s0.T;
        if (task == 8) { A->c.e += (ok ? REWARD : 0) - m; B->c.e += (ok ? REWARD : 0) - m; }
        else { A->c.e += (a0.a1 == e->h ? REWARD : 0) - m; B->c.e += (ok ? REWARD : 0) - m; }
    }
}

/* ---------- évaluation figée : clones, entrées de test, aucune énergie réelle ---------- */
static int test_split(int task) { return task == 2 || task == 7 || task == 10; }
static Stat evaluate(World *w, const Eco *E, int episodes, uint64_t salt) {
    Stat st; memset(&st, 0, sizeof st);
    if (!E->n) return st;
    Rng r; rng_seed(&r, address(E->task, E->arm, E->seed, 0xE7A1, E->step, (int)salt));
    Ind *clone = malloc(sizeof(Ind) * (size_t)E->n), *mate = malloc(sizeof(Ind)); if (!clone || !mate) exit(2);
    for (int i = 0; i < E->n; i++) clone[i] = E->ind[i];
    for (int i = 0; i < E->n; i++) {
        for (int k = 0; k < episodes; k++) {
            uint64_t step = E->step + (uint64_t)k;  /* tâche 3 : la règle bascule pendant l'évaluation aussi */
            Ep e = draw(E->task, E->arm, test_split(E->task), step, &r);
            if (paired(E->task)) {
                int j = (int)rng_below(&r, (uint32_t)E->n); *mate = E->ind[j];
                if (rng_below(&r, 2)) episode(w, E->task, E->arm, &e, mate, &clone[i], &r, &st, 1);
                else { Stat junk; memset(&junk, 0, sizeof junk); episode(w, E->task, E->arm, &e, &clone[i], mate, &r, &junk, 1);
                       /* le clone parle : on mesure quand même le récepteur, partenaire tiré */
                       st.num[0] += junk.num[0]; st.den[0] += junk.den[0]; st.num[1] += junk.num[1]; st.den[1] += junk.den[1]; }
            } else episode(w, E->task, E->arm, &e, &clone[i], NULL, &r, &st, 1);
            if (clone[i].c.e <= 0) clone[i].c.e = 1;   /* clones payés comme en vie (le retour par l'énergie compte), jamais tués */
        }
    }
    free(clone); free(mate);
    return st;
}

/* ---------- un cycle écologique ---------- */
static void mutate_germ(PoolEntry *g, Rng *r) {
    for (int k = 0; k < g->len; k++) if (rng_below(r, 100) == 0) g->dna[k] = (uint8_t)rng_below(r, 256);
}
static void cycle(World *w, Eco *E, Stat *st) {
    int n = E->n; if (n <= 0 || n > CAP) return;
    /* paiements différés (tâche 5) au début de l'épisode */
    for (int i = 0; i < n; i++) for (int k = 0; k < 4; k++) if (E->ind[i].pend_wait[k] && --E->ind[i].pend_wait[k] == 0) E->ind[i].c.e += E->ind[i].pend_amt[k];
    if (paired(E->task)) {
        int *ord = malloc(sizeof(int) * (size_t)n); if (!ord) exit(2);
        for (int i = 0; i < n; i++) ord[i] = i;
        for (int i = n - 1; i > 0; i--) { int j = (int)rng_below(&E->rng, (uint32_t)(i + 1)); int t = ord[i]; ord[i] = ord[j]; ord[j] = t; }
        for (int i = 0; i + 1 < n; i += 2) {
            Ind *A = &E->ind[ord[i]], *B = &E->ind[ord[i + 1]];
            Rng r; rng_seed(&r, address(E->task, E->arm, E->seed, (uint64_t)A->c.id << 20 ^ (uint64_t)B->c.id, E->step, 1));
            Ep e = draw(E->task, E->arm, 0, E->step, &r);
            episode(w, E->task, E->arm, &e, A, B, &r, st, 1);
        }
        free(ord);
    } else {
        for (int i = 0; i < n; i++) {
            Rng r; rng_seed(&r, address(E->task, E->arm, E->seed, (uint64_t)E->ind[i].c.id, E->step, 1));
            Ep e = draw(E->task, E->arm, 0, E->step, &r);
            episode(w, E->task, E->arm, &e, &E->ind[i], NULL, &r, st, 1);
        }
    }
    /* morts, naissances, plafond par retrait uniforme (neutre) */
    static Ind next[2 * CAP]; int m = 0;
    for (int i = 0; i < n; i++) {
        Ind *x = &E->ind[i];
        if (x->c.e <= 0) { E->deaths++; continue; }
        next[m++] = *x;
        if (x->c.e >= EN_BIRTH) {
            next[m - 1].c.e -= EN_PARENT;
            Ind ch; memset(&ch, 0, sizeof ch); ch.g = x->g; mutate_germ(&ch.g, &E->rng);
            ch.c = fresh(&ch.g, EN_CHILD, (int)E->next_id++, x->c.id); ch.c.species = x->c.species;
            next[m++] = ch; E->births++;
        }
    }
    while (m > CAP) { int j = (int)rng_below(&E->rng, (uint32_t)m); next[j] = next[--m]; E->culled++; }
    for (int i = 0; i < m; i++) E->ind[i] = next[i];
    E->n = m; E->step++;
}

/* ---------- fondateurs : toutes les graines .adn du dossier seeds, en copies égales ---------- */
static int load_seeds(const char *dir, PoolEntry *out, int max) {
    DIR *d = opendir(dir); if (!d) { perror(dir); exit(2); }
    char names[64][256]; int k = 0; struct dirent *de;
    while ((de = readdir(d)) && k < 64) {
        size_t l = strlen(de->d_name);
        if (l > 4 && !strcmp(de->d_name + l - 4, ".adn")) snprintf(names[k++], 256, "%s", de->d_name);
    }
    closedir(d);
    for (int i = 0; i < k; i++) for (int j = i + 1; j < k; j++) if (strcmp(names[i], names[j]) > 0) { char t[256]; memcpy(t, names[i], 256); memcpy(names[i], names[j], 256); memcpy(names[j], t, 256); }
    int n = 0;
    for (int i = 0; i < k && n < max; i++) {
        char p[600]; snprintf(p, sizeof p, "%s/%s", dir, names[i]);
        FILE *f = fopen(p, "rb"); if (!f) continue;
        PoolEntry g; memset(&g, 0, sizeof g); g.len = (int)fread(g.dna, 1, MAX_LEN, f); fclose(f);
        if (g.len >= MIN_LEN) { g.species = i; out[n++] = g; }
    }
    return n;
}
static void found(Eco *E, const char *seeds) {
    PoolEntry pool[64]; int k = load_seeds(seeds, pool, 64); if (!k) { fputs("aucune graine\n", stderr); exit(2); }
    E->n = 0;
    for (int i = 0; i < CAP; i++) {
        Ind *x = &E->ind[E->n++]; memset(x, 0, sizeof *x);
        x->g = pool[i % k]; x->c = fresh(&x->g, EN_0, (int)E->next_id++, -1); x->c.species = x->g.species;
    }
}

/* ---------- journaux et checkpoint ---------- */
static FILE *open_in(const char *dir, const char *name, const char *mode) {
    char p[4096]; snprintf(p, sizeof p, "%s/%s", dir, name);
    FILE *f = fopen(p, mode); if (!f) { perror(p); exit(2); } return f;
}
static void save(const char *dir, Eco *E, FILE *series, FILE *eval) {
    if (fflush(series) || fflush(eval) || fsync(fileno(series)) || fsync(fileno(eval))) exit(2);
    E->series_off = ftell(series); E->eval_off = ftell(eval);
    FILE *f = open_in(dir, "checkpoint.tmp", "wb");
    if (fwrite(E, sizeof *E, 1, f) != 1 || fflush(f) || fsync(fileno(f)) || fclose(f)) exit(2);
    char a[4096], b[4096]; snprintf(a, sizeof a, "%s/checkpoint.tmp", dir); snprintf(b, sizeof b, "%s/checkpoint.bin", dir);
    if (rename(a, b)) exit(2);
}
static int genotypes(const Eco *E) {
    uint64_t h[CAP]; int n = 0;
    for (int i = 0; i < E->n; i++) {
        uint64_t x = fnv1a(E->ind[i].g.dna, (size_t)E->ind[i].g.len, FNV_INIT); int seen = 0;
        for (int j = 0; j < n; j++) if (h[j] == x) { seen = 1; break; }
        if (!seen) h[n++] = x;
    }
    return n;
}
static void eval_line(FILE *f, const Eco *E, const Stat *s) {
    fprintf(f, "%llu\t%d\t%.6f\t%.6f\t%.6f\t%.6f\t%.0f\n", (unsigned long long)E->step, E->n,
            stat_mean(s, 0), stat_mean(s, 1), stat_mean(s, 2), stat_mean(s, 3), s->den[0]);
}
static volatile sig_atomic_t stopping;
static void on_stop(int sig) { (void)sig; stopping = 1; }

static int run(const char *dir, int task, int arm, uint64_t seed, uint64_t cycles, int pause_ms, const char *seeds, int serie, int evalp, int ck) {
    if (mkdir(dir, 0775) && errno != EEXIST) { perror(dir); return 2; }
    Eco *E = calloc(1, sizeof *E); if (!E) return 2;
    char p[4096]; snprintf(p, sizeof p, "%s/checkpoint.bin", dir);
    FILE *f = fopen(p, "rb"); int resume = f != NULL;
    if (resume) {
        if (fread(E, sizeof *E, 1, f) != 1 || fgetc(f) != EOF || E->magic != MAGIC || E->version != 1 || E->n < 0 || E->n > CAP) { fputs("checkpoint invalide\n", stderr); return 2; }
        fclose(f);
        if (task || arm >= 0) { fputs("reprise : ne pas fournir de nouvelle configuration\n", stderr); return 2; }
    } else {
        if (task < 1 || task > NT || arm < 0 || arm > 1) { fputs("tâche 1..10 et bras 0|1 requis\n", stderr); return 2; }
        E->magic = MAGIC; E->version = 1; E->task = task; E->arm = arm; E->seed = seed;
        E->serie_every = serie > 0 ? serie : 16; E->eval_every = evalp > 0 ? evalp : 512;
        E->pad = ck > 0 ? ck : 64;   /* pad = période de checkpoint */
        if (E->serie_every < 1 || E->eval_every < 1 || E->pad < 1) { fputs("périodes invalides\n", stderr); return 2; }
        rng_seed(&E->rng, address(task, arm, seed, 0, 0, 77)); found(E, seeds);
    }
    FILE *series = open_in(dir, "series.tsv", resume ? "r+" : "wx"), *eval = open_in(dir, "eval.tsv", resume ? "r+" : "wx");
    if (resume) {
        if (ftruncate(fileno(series), E->series_off) || ftruncate(fileno(eval), E->eval_off)) return 2;
        fseek(series, 0, SEEK_END); fseek(eval, 0, SEEK_END);
    } else {
        fprintf(series, "# tache=%s bras=%s graine=%llu\n", TASK[E->task], E->arm ? "temoin" : "info", (unsigned long long)E->seed);
        fputs("cycle\tpop\tgenotypes\tnaissances\tmorts\tretires\tm0\tm1\tm2\tm3\tenergie\n", series);
        fputs("cycle\tpop\tm0\tm1\tm2\tm3\tepisodes\n", eval);
        save(dir, E, series, eval);
    }
    signal(SIGTERM, on_stop); signal(SIGINT, on_stop);
    World *w = arena(); uint64_t done = 0;
    while (E->n && !stopping && (!cycles || done < cycles)) {
        cycle(w, E, &E->acc); done++;
        if (E->step % (uint64_t)E->serie_every == 0) {
            long en = 0; for (int i = 0; i < E->n; i++) en += E->ind[i].c.e;
            fprintf(series, "%llu\t%d\t%d\t%llu\t%llu\t%llu\t%.4f\t%.4f\t%.4f\t%.4f\t%.1f\n", (unsigned long long)E->step, E->n, genotypes(E),
                    (unsigned long long)E->births, (unsigned long long)E->deaths, (unsigned long long)E->culled,
                    stat_mean(&E->acc, 0), stat_mean(&E->acc, 1), stat_mean(&E->acc, 2), stat_mean(&E->acc, 3), E->n ? (double)en / E->n : 0);
            memset(&E->acc, 0, sizeof E->acc);
        }
        if (E->step % (uint64_t)E->eval_every == 0) { Stat ev = evaluate(w, E, 32, 1); eval_line(eval, E, &ev); }
        if (E->step % (uint64_t)E->pad == 0) save(dir, E, series, eval);
        if (pause_ms > 0 && E->step % 64 == 0) { struct timespec ts = { pause_ms / 1000, (pause_ms % 1000) * 1000000L }; nanosleep(&ts, NULL); }
    }
    save(dir, E, series, eval);
    printf("%s %s graine=%llu cycle=%llu pop=%d naissances=%llu morts=%llu\n", TASK[E->task], E->arm ? "temoin" : "info",
           (unsigned long long)E->seed, (unsigned long long)E->step, E->n, (unsigned long long)E->births, (unsigned long long)E->deaths);
    fclose(series); fclose(eval); free(w); free(E);
    return 0;
}
/* Évaluation finale, lecture seule du checkpoint : 128 épisodes par individu, sel distinct. */
static int final_eval(const char *dir) {
    Eco *E = calloc(1, sizeof *E); FILE *f = open_in(dir, "checkpoint.bin", "rb");
    if (!E || fread(E, sizeof *E, 1, f) != 1 || E->magic != MAGIC) return 2;
    fclose(f);
    World *w = arena(); Stat s = evaluate(w, E, 128, 2);
    printf("%s\t%s\t%llu\t%llu\t%d\t%.6f\t%.6f\t%.6f\t%.6f\t%.0f\n", TASK[E->task], E->arm ? "temoin" : "info",
           (unsigned long long)E->seed, (unsigned long long)E->step, E->n,
           stat_mean(&s, 0), stat_mean(&s, 1), stat_mean(&s, 2), stat_mean(&s, 3), s.den[0]);
    free(w); free(E); return 0;
}

/* ---------- contrôles écrits : le dispositif doit permettre la réussite ---------- */
static PoolEntry prog(const uint8_t *b, int n) { PoolEntry g; memset(&g, 0, sizeof g); g.len = n; memcpy(g.dna, b, (size_t)n); return g; }
static double play(World *w, int task, int arm, const PoolEntry *g, int split, int eps, int metric) {
    Ind a, b; memset(&a, 0, sizeof a); memset(&b, 0, sizeof b);
    a.g = *g; a.c = fresh(g, 100000, 1, -1); b.g = *g; b.c = fresh(g, 100000, 2, -1);
    Stat st; memset(&st, 0, sizeof st); Rng r; rng_seed(&r, 4242 + (uint64_t)task * 7 + (uint64_t)arm);
    for (int k = 0; k < eps; k++) {
        Ep e = draw(task, arm, split, (uint64_t)k, &r);
        if (paired(task)) episode(w, task, arm, &e, &a, &b, &r, &st, 1);
        else episode(w, task, arm, &e, &a, NULL, &r, &st, 1);
    }
    return metric == 12 ? stat_mean(&st, 1) - stat_mean(&st, 2) : stat_mean(&st, metric);
}
#define CHECK(lbl, cond) do { int ok_ = !!(cond); printf("selftest raison : %-58s %s\n", lbl, ok_ ? "OK" : "ECHEC"); fails += !ok_; } while (0)
static int selftest(void) {
    int fails = 0; World *w = arena();
    /* aveugle : aucun capteur, A=0 → 50 % */
    uint8_t blind[] = { OP_YIELD, OP_LOOP };
    PoolEntry B = prog(blind, 2);
    for (int t = 1; t <= NT; t++) if (t != 5 && t != 4 && t != 6) {
        double m = play(w, t, 0, &B, 0, 4000, 0); char l[80]; snprintf(l, sizeof l, "aveugle %s ≈ 0,5 (%.3f)", TASK[t], m);
        CHECK(l, m > .46 && m < .54);
    }
    /* 1 mémoire : lire au tick 0, garder A cinq ticks */
    uint8_t p1[] = { OP_SENSE_STIM, OP_YIELD, OP_YIELD, OP_YIELD, OP_YIELD, OP_YIELD, OP_YIELD, OP_LOOP };
    PoolEntry G = prog(p1, 8);
    double m = play(w, 1, 0, &G, 0, 4000, 0); CHECK("memoire : contrôle écrit ≈ 0,75", m > .72 && m < .78);
    m = play(w, 1, 1, &G, 0, 4000, 0); CHECK("memoire : même contrôle, bras témoin ≈ 0,5", m > .46 && m < .54);
    /* 2 composition : u → B ; v ; XOR ; garder */
    uint8_t p2[] = { OP_SENSE_STIM, OP_MOV_BA, OP_YIELD, OP_SENSE_STIM, OP_XOR, OP_YIELD, OP_YIELD, OP_YIELD, OP_YIELD, OP_YIELD, OP_YIELD, OP_LOOP };
    G = prog(p2, 12);
    m = play(w, 2, 0, &G, 1, 4000, 0); CHECK("composition : XOR écrit = 1 sur couples réservés", m > .999);
    m = play(w, 2, 1, &G, 1, 4000, 0); CHECK("composition : bras témoin ≈ 0,5", m > .46 && m < .54);
    /* 7 transfert : AND 16 ; JZ → A=0 sinon LOADI 1 */
    uint8_t p7[] = { OP_SENSE_STIM, OP_AND, 16, OP_JZ, 2, OP_LOADI, 1, OP_YIELD, OP_YIELD, OP_YIELD, OP_YIELD, OP_LOOP };
    G = prog(p7, 12);
    m = play(w, 7, 0, &G, 1, 4000, 0); CHECK("transfert : règle bit 4 = 1 sur quartets jamais vus", m > .999);
    /* 10 relation : x → B ; y ; SUB ; JZ égal → LOADI 1 sinon LOADI 0 */
    uint8_t p10[] = { OP_SENSE_STIM, OP_MOV_BA, OP_YIELD, OP_SENSE_STIM, OP_SUB, OP_JZ, 4, OP_LOADI, 0, OP_JMP, 2, OP_LOADI, 1, OP_YIELD, OP_YIELD, OP_YIELD, OP_YIELD, OP_LOOP };
    G = prog(p10, 18);
    m = play(w, 10, 0, &G, 1, 4000, 0); CHECK("relation : pareil/différent = 1 sur valeurs jamais vues", m > .999);
    /* 4 observation : lire q ; si q==1 (A-1 == 0) lire l'indice, sinon ne pas lire */
    uint8_t p4[] = { OP_SENSE_STIM, OP_DEC_A, OP_JZ, 6, OP_YIELD, OP_YIELD, OP_YIELD, OP_YIELD, OP_YIELD, OP_LOOP,
                     OP_YIELD, OP_SENSE_STIM, OP_YIELD, OP_YIELD, OP_YIELD, OP_YIELD, OP_LOOP };
    G = prog(p4, 17);
    m = play(w, 4, 0, &G, 0, 4000, 12); CHECK("observation : lit si fiable, pas sinon (écart ≈ 1)", m > .99);
    /* 6 chaînage : A=1 au tick 1 (décision 1), lire au tick 2, garder */
    uint8_t p6[] = { OP_YIELD, OP_LOADI, 1, OP_YIELD, OP_SENSE_STIM, OP_YIELD, OP_YIELD, OP_YIELD, OP_YIELD, OP_LOOP };
    G = prog(p6, 10);
    m = play(w, 6, 0, &G, 0, 4000, 0); CHECK("chainage : déverrouille puis répond = 1", m > .999);
    /* 5 patience : attendre si s==1 (A&1 = 1), sinon prendre (A&1 = 0 pour s==2) */
    uint8_t p5[] = { OP_SENSE_STIM, OP_YIELD, OP_YIELD, OP_YIELD, OP_YIELD, OP_LOOP };
    G = prog(p5, 6);
    m = play(w, 5, 0, &G, 0, 4000, 0); CHECK("patience : attendre si court, prendre si long = 1", m > .999);
    /* 8 signal : émetteur lit et émet à chaque tick ; récepteur lit au tick 3 */
    uint8_t p8[] = { OP_SENSE_STIM, OP_YIELD, OP_EMIT, OP_YIELD, OP_EMIT, OP_YIELD, OP_SENSE_STIM, OP_YIELD, OP_YIELD, OP_YIELD, OP_LOOP };
    G = prog(p8, 11);
    m = play(w, 8, 0, &G, 0, 4000, 0); CHECK("signal : canal ouvert, récepteur = 1", m > .999);
    m = play(w, 8, 1, &G, 0, 4000, 0); CHECK("signal : canal coupé, récepteur ≈ 0,5", m > .46 && m < .54);
    /* 9 autrui : même programme ; l'observateur lit l'action de l'acteur au tick 3 */
    m = play(w, 9, 0, &G, 0, 4000, 0); CHECK("autrui : observateur ≈ 0,75 (acteur informé à 75 %)", m > .72 && m < .78);
    m = play(w, 9, 1, &G, 0, 4000, 0); CHECK("autrui : témoin ≈ 0,5", m > .46 && m < .54);
    /* 3 inversion : la règle bascule tous les 16 épisodes ; suivre l'indice seul = 0,5 */
    G = prog(p1, 8);
    m = play(w, 3, 0, &G, 0, 4096, 0); CHECK("inversion : suivre l'indice sans adapter ≈ 0,5", m > .46 && m < .54);
    /* paiement différé et décision non omissible */
    {
        Ind a; memset(&a, 0, sizeof a); uint8_t wait1[] = { OP_LOADI, 1, OP_YIELD, OP_LOOP }; a.g = prog(wait1, 4); a.c = fresh(&a.g, 100, 1, -1);
        Ep e; memset(&e, 0, sizeof e); e.s = 1; Stat st; memset(&st, 0, sizeof st); Rng r; rng_seed(&r, 1);
        episode(w, 5, 0, &e, &a, NULL, &r, &st, 1);
        CHECK("patience : attendre ne paie pas tout de suite", a.c.e == 100 - METAB && a.pend_amt[0] == 18 && a.pend_wait[0] == 2);
    }
    /* reprise exacte : 300 cycles d'un coup = 150 + checkpoint + 150 */
    {
        char d1[] = "/tmp/raison-st-XXXXXX", d2[] = "/tmp/raison-st-XXXXXX";
        if (!mkdtemp(d1) || !mkdtemp(d2)) return 1;
        FILE *o = stdout; stdout = fopen("/dev/null", "w");
        run(d1, 9, 0, 3, 300, 0, "seeds", 16, 128, 64); run(d2, 9, 0, 3, 150, 0, "seeds", 16, 128, 64); run(d2, 0, -1, 0, 150, 0, "seeds", 0, 0, 0);
        fclose(stdout); stdout = o;
        char a[4200], b[4200]; int same = 1;
        const char *files[] = { "checkpoint.bin", "series.tsv", "eval.tsv" };
        for (int i = 0; i < 3; i++) {
            snprintf(a, sizeof a, "%s/%s", d1, files[i]); snprintf(b, sizeof b, "%s/%s", d2, files[i]);
            FILE *fa = fopen(a, "rb"), *fb = fopen(b, "rb"); int ca, cb;
            if (!fa || !fb) { same = 0; break; }
            do { ca = fgetc(fa); cb = fgetc(fb); } while (ca == cb && ca != EOF);
            same &= ca == cb; fclose(fa); fclose(fb);
        }
        CHECK("reprise : 300 cycles = 150 + reprise + 150 (checkpoint et journaux)", same);
        char cmd[200]; snprintf(cmd, sizeof cmd, "rm -rf %s %s", d1, d2); if (system(cmd)) {}
    }
    free(w);
    printf("selftest raison : %d échec(s)\n", fails);
    return fails != 0;
}

int main(int argc, char **argv) {
    if (argc == 2 && !strcmp(argv[1], "selftest")) return selftest();
    if (argc == 3 && !strcmp(argv[1], "eval")) return final_eval(argv[2]);
    if (argc >= 3 && !strcmp(argv[1], "run")) {
        const char *dir = argv[2], *seeds = "seeds"; int task = 0, arm = -1, pause = 0, serie = 0, evalp = 0, ck = 0; uint64_t seed = 1, cycles = 4096;
        for (int i = 3; i + 1 < argc; i += 2) {
            if (!strcmp(argv[i], "--tache")) { task = 0; for (int t = 1; t <= NT; t++) if (!strcmp(argv[i + 1], TASK[t])) task = t; if (!task) return 2; }
            else if (!strcmp(argv[i], "--bras")) arm = !strcmp(argv[i + 1], "temoin") ? 1 : !strcmp(argv[i + 1], "info") ? 0 : -2;
            else if (!strcmp(argv[i], "--graine")) seed = strtoull(argv[i + 1], NULL, 10);
            else if (!strcmp(argv[i], "--cycles")) cycles = strtoull(argv[i + 1], NULL, 10);
            else if (!strcmp(argv[i], "--pause-ms")) pause = atoi(argv[i + 1]);
            else if (!strcmp(argv[i], "--graines")) seeds = argv[i + 1];
            else if (!strcmp(argv[i], "--serie")) serie = atoi(argv[i + 1]);
            else if (!strcmp(argv[i], "--eval")) evalp = atoi(argv[i + 1]);
            else if (!strcmp(argv[i], "--checkpoint")) ck = atoi(argv[i + 1]);
            else { fprintf(stderr, "option inconnue %s\n", argv[i]); return 2; }
        }
        if (arm == -2) return 2;
        return run(dir, task, arm, seed, cycles, pause, seeds, serie, evalp, ck);
    }
    fprintf(stderr, "usage : raison selftest | run DIR [--tache NOM --bras info|temoin --graine N] [--cycles N (0=infini)] [--pause-ms N] [--graines DIR] [--serie N (16)] [--eval N (512)] [--checkpoint N (64)] | eval DIR\n"
                    "tâches : memoire composition inversion observation patience chainage transfert signal autrui relation\n");
    return 2;
}
