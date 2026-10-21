/* Labos « humanité » : versions jouets de problèmes réels. Chacun garde le même moteur ;
 * les mécanismes propres passent par les hooks de labo (init, pre, post, before_vm), jamais
 * par un changement de comportement des 10 labos historiques. Ce sont des terrains
 * d'entraînement, pas des percées : un gagnant est un algorithme à étudier. */
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "octet.h"

static int cheb(const Creature *a, const Creature *b, int W, int H) {
    int dx = abs(a->x - b->x), dy = abs(a->y - b->y);
    if (dx > W / 2) dx = W - dx;
    if (dy > H / 2) dy = H - dy;
    return dx > dy ? dx : dy;
}
static void cap_e(Creature *c) { if (c->e > 220) c->e = 220; }
void hl_rank_score(const Creature *c, double k[2]) { k[0] = c->lscore; k[1] = c->e; }

/* ======================= 1. fold : modèle HP 2D =======================
 * Benchmarks classiques (Unger & Moult 1993), optimum connu = nb de contacts H-H.
 * Le génome est rejoué dans un bac à sable : pour placer le résidu i, stimulus =
 * H(i) | H(i+1)<<1 ; EMIT v → v%3 : 0 tout droit, 1 gauche, 2 droite (pas d'EMIT = tout droit).
 * Collision : on essaie les deux autres directions dans l'ordre ; bloqué = repliement tronqué. */
static const struct { const char *seq; int opt; } HP[] = {
    { "HPHPPHHPHPPHPHHPPHPH", 9 },
    { "HHPPHPPHPPHPPHPPHPPHPPHH", 9 },
    { "PPHPPHHPPPPHHPPPPHHPPPPHH", 8 },
    { "PPPHHPPHHPPPPPHHHHHHHPPHHPPPPHHPPHPP", 14 },
    { "PPHPPHHPPHHPPPPPHHHHHHHHHHPPPPPPHHPPHHPPHPPHHHHH", 23 },
    { "HHPHPHPHPHHHHPHPPPHPPPHPPPPHPPPHPPPHPHHHHPHPHPHPHH", 21 },
};
#define N_HP ((int)(sizeof HP / sizeof HP[0]))
typedef struct { World *sb; Creature tc; int grid[128][128]; int stamp; } FoldState;

/* contacts H-H d'un repliement donné par ses moves (0/1/2) ; renvoie les contacts, -1 si invalide */
static int hp_contacts(FoldState *s, const char *q, int n, const int *px, const int *py, int placed) {
    (void)s; int c = 0;
    for (int i = 0; i < placed; i++) if (q[i] == 'H')
        for (int j = i + 2; j < placed; j++)
            if (q[j] == 'H' && abs(px[i] - px[j]) + abs(py[i] - py[j]) == 1) c++;
    (void)n; return c;
}
static const int FDX[4] = { 1, 0, -1, 0 }, FDY[4] = { 0, 1, 0, -1 };
/* plie la séquence k ; moves via callback : genome (tc) ou aléatoire (rng) */
static int fold_seq(FoldState *s, int k, const Creature *src, Rng *rnd) {
    const char *q = HP[k].seq; int n = (int)strlen(q);
    int px[64], py[64], d = 0;
    s->stamp++;
    px[0] = 64; py[0] = 64; px[1] = 65; py[1] = 64;
    s->grid[64][64] = s->grid[64][65] = s->stamp;
    World *w = s->sb; Creature *c = &s->tc;
    if (src) {
        memcpy(c, src, sizeof *c);
        c->ip = 0; c->a = c->b = 0; c->age = 0; c->win_len = 0; c->x = 2; c->y = 2; c->d = 0;
        w->hist_n = w->hist_pos = 0; w->nem = 0;
    }
    int placed = 2;
    for (int i = 2; i < n; i++) {
        int m = 0;
        if (src) {
            w->stimulus = (q[i] == 'H') | ((i + 1 < n && q[i + 1] == 'H') << 1);
            c->e = 1000; w->nem = 0;
            vm_run(c, w);
            if (c->emitted >= 0) m = c->emitted % 3;
            w->hist[w->hist_pos] = (uint8_t)w->stimulus; w->hist_pos = (w->hist_pos + 1) % HIST_N;
            if (w->hist_n < HIST_N) w->hist_n++;
        } else m = (int)rng_below(rnd, 3);
        int ok = 0;
        for (int t = 0; t < 3 && !ok; t++) {
            int mm = (m + t) % 3, nd = mm == 0 ? d : mm == 1 ? (d + 3) & 3 : (d + 1) & 3;
            int x = px[i - 1] + FDX[nd], y = py[i - 1] + FDY[nd];
            if (s->grid[y][x] != s->stamp) { d = nd; px[i] = x; py[i] = y; s->grid[y][x] = s->stamp; ok = 1; }
        }
        if (!ok) break;
        placed++;
    }
    return hp_contacts(s, q, n, px, py, placed);
}
static double fold_score(FoldState *s, const Creature *c) {
    double sc = 0;
    for (int k = 0; k < N_HP; k++) sc += (double)fold_seq(s, k, c, NULL) / HP[k].opt;
    return sc / N_HP;
}
static double fold_random_baseline(void) {        /* marche auto-évitante aléatoire, 64 essais / séquence */
    static double cached = -1;
    if (cached >= 0) return cached;
    FoldState *s = calloc(1, sizeof *s); Rng r; rng_seed(&r, 12345); double t = 0;
    for (int k = 0; k < N_HP; k++) for (int j = 0; j < 64; j++) t += (double)fold_seq(s, k, NULL, &r) / HP[k].opt / 64;
    free(s);
    return cached = t / N_HP;
}
static void fold_before(World *w, Creature *c) {
    if (c->ls[0]) return;
    FoldState *s = w->lab->ext;
    c->ls[0] = 1;
    c->lscore = fold_score(s, c);
    c->e += (int)(60 * c->lscore); cap_e(c);       /* bien plier nourrit : pression de sélection */
}
void hl_fold_free(Lab *l);
static void fold_init(Lab *l, World *w) {
    FoldState *s = calloc(1, sizeof *s);
    WorldConfig sc = cfg_default(); sc.max_pop = 0; sc.food_density = 0;
    Rng *r = malloc(sizeof *r); rng_seed(r, 0);
    s->sb = world_new(NULL, 0, r, &sc, NULL);
    l->ext = s; l->ext_free = hl_fold_free; w->before_vm = fold_before;
}
void hl_fold_free(Lab *l) { FoldState *s = l->ext; if (s) { free(s->sb->rng); world_free(s->sb); } }
void hl_fold_cfg(WorldConfig *c) { c->food_density = 0.25; c->food_regrow = 3; }
int hl_fold_seeds(PoolEntry *o) { seed_entry(o, "fold", 0); return 1; }
double hl_fold_fit(Lab *l, World *w, int t) {
    (void)t; double s = 0, mx = 0;
    for (int i = 0; i < w->ncs; i++) { s += w->cs[i]->lscore; if (w->cs[i]->lscore > mx) mx = w->cs[i]->lscore; }
    double m = w->ncs ? s / w->ncs : 0;
    l->extra[0] = m; l->extra[1] = mx; l->extra[2] = fold_random_baseline(); l->extra[3] = m - l->extra[2];
    return m;
}
void hl_fold_initp(Lab *l, World *w) { fold_init(l, w); }

/* rejeu contrefactuel : même pool, même graine, interventions coupées (l->counterfactual) */
static double replay_pool(Lab *l, int ticks, const PoolEntry *pool, int n, double *extra) {
    int base = l->def->ticks_mult > 1 ? ticks / l->def->ticks_mult : ticks;
    Lab l2; lab_init(&l2, l->id, base);
    l2.counterfactual = 1;
    Rng r = l->rng0;
    CycleResult *res = malloc(sizeof *res);
    run_cycle(&l2, pool, n, &r, base, res);
    double f = res->fitness;
    if (extra) memcpy(extra, l2.extra, sizeof l2.extra);
    world_free(res->w); lab_free(&l2); free(res);
    return f;
}
static double replay(Lab *l, int ticks, double *extra) { return replay_pool(l, ticks, l->pool0, l->npool0, extra); }
static int sel_species0(Lab *l, World *w, PoolEntry *out) {
    (void)l;
    Creature *v[CMAX]; int n = 0, k = 0;
    for (int i = 0; i < w->ncs; i++) if (w->cs[i]->species == 0) v[n++] = w->cs[i];
    for (int i = 1; i < n; i++) {                       /* contribution (lscore), puis énergie, puis âge */
        Creature *c = v[i]; int j = i - 1;
        while (j >= 0 && (v[j]->lscore < c->lscore
               || (v[j]->lscore == c->lscore && (v[j]->e < c->e || (v[j]->e == c->e && v[j]->age < c->age))))) {
            v[j + 1] = v[j]; j--;
        }
        v[j + 1] = c;
    }
    for (int i = 0; i < n && k < POOL_SIZE; i++) {
        out[k].species = 0; out[k].len = v[i]->len; memcpy(out[k].dna, v[i]->dna, (size_t)v[i]->len); k++;
    }
    return k;
}

/* ======================= 2. antibiotic : pathogène co-évolutif =======================
 * Tissu de 40 cellules hôtes (espèce 2, immobiles). 12 pathogènes (espèce 2→1) y sont injectés ;
 * ils volent l'énergie des hôtes (ATTACK) et se divisent. Cible du pathogène = dna[0] & 7, héritée
 * et mutée à chaque division : la résistance émerge par mutation PENDANT le cycle. Les agents
 * (espèce 0, le pool) lisent un antibiogramme (stimulus = clé majoritaire, 8 = aucun) et EMIT v
 * libère le médicament v&7 en rayon 1 : tue les pathogènes de clé v&7, coûte 2 à l'agent, abîme
 * de 1 chaque hôte touché. Les pathogènes ne font pas partie du pool : ré-injectés frais à chaque
 * cycle (sinon la sélection garderait des pathogènes faibles). */
#define AB_HOSTS 40
#define AB_PATH  12
#define AB_KEY_MUT 0.15   /* prob. qu'un pathogène né change de cible (virus à ARN) */
typedef struct {
    int esc_t, erad_t, peak; long damage;
    PoolEntry champ; double champ_ls; int has_champ;   /* meilleur apport vivant ou mort */
} AbState;
static int ab_key(const Creature *c) { return c->dna[0] & 7; }
void hl_ab_cfg(WorldConfig *c) { c->max_pop = 260; c->attack_steal = 6; }
int hl_ab_seeds(PoolEntry *o) { for (int i = 0; i < 4; i++) seed_entry(&o[i], "antibiotic", 0); return 4; }
void hl_ab_init(Lab *l, World *w) {
    l->ext = calloc(1, sizeof(AbState));
    static const uint8_t host[8] = { 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f };
    uint8_t path[8] = { 0x20, 0x1a, 0x0e, 0x0f, 0x1e, 0x1e, 0x1e, 0x1e };   /* clé ; ATTACK SPLIT YIELD LOOP */
    int x0 = w->cfg.w / 2 - 4, y0 = w->cfg.h / 2 - 2;
    for (int i = 0; i < w->ncs; i++) {                 /* agents administrés autour du tissu */
        Creature *c = w->cs[i]; w->occ[c->y][c->x]--;
        c->x = x0 - 2 + (int)rng_below(w->rng, 12); c->y = y0 - 2 + (int)rng_below(w->rng, 9); w->occ[c->y][c->x]++;
    }
    for (int i = 0; i < AB_HOSTS; i++)
        world_spawn(w, creature_new(x0 + i % 8, y0 + i / 8, host, 8, 220, w->nid++, 2, -1));
    path[0] = (uint8_t)(0x20 + rng_below(w->rng, 8));
    for (int i = 0; i < AB_PATH; i++) {
        int k = (int)rng_below(w->rng, AB_HOSTS);
        world_spawn(w, creature_new(x0 + k % 8, y0 + k / 8, path, 8, 60, w->nid++, 1, -1));
    }
}
void hl_ab_pre(Lab *l, World *w) {
    AbState *s = l->ext; int cnt[8] = { 0 }, np = 0;
    for (int i = 0; i < w->ncs; i++) {
        Creature *c = w->cs[i];
        if (c->species == 2) { c->e += 1; cap_e(c); }      /* les hôtes ne s'usent pas seuls */
        else if (c->species == 1) {
            if (!c->ls[0] && c->parent >= 0 && rng_double(w->rng) < AB_KEY_MUT)   /* hypermutation de la cible */
                c->dna[0] = (uint8_t)(0x20 + rng_below(w->rng, 8));
            c->ls[0] = 1;
            cnt[ab_key(c)]++; np++;
        }
    }
    int diag = 8, best = 0;
    for (int k = 0; k < 8; k++) if (cnt[k] > best) { best = cnt[k]; diag = k; }
    w->stimulus = diag;
    if (w->t <= 1) return;                              /* pathogènes injectés en fin de tick 1 */
    if (np > s->peak) s->peak = np;
    if (!s->esc_t && np > 5 * AB_PATH) s->esc_t = (int)w->t;
    if (!s->esc_t && !s->erad_t && np == 0) s->erad_t = (int)w->t;
}
void hl_ab_post(Lab *l, World *w) {
    AbState *s = l->ext;
    if (l->counterfactual) return;                      /* baseline : aucun traitement */
    for (int i = 0; i < w->nem; i++) {
        Creature *a = w->em[i].c;
        if (a->species != 0) continue;
        int drug = w->em[i].v & 7;
        a->e -= 2;
        for (int j = 0; j < w->ncs; j++) {
            Creature *o = w->cs[j];
            if (o->e <= 0 || cheb(a, o, w->cfg.w, w->cfg.h) > 1) continue;
            if (o->species == 1 && ab_key(o) == drug) { o->e = 0; a->lscore += 1; }
            else if (o->species == 2) { o->e -= 1; s->damage++; a->lscore -= 0.25; }
        }
        /* Le corps meurt souvent (coût du médicament). Le pic positif reste : c'est ce génome
         * qui a fait le travail, pas le glouton encore en vie à score nul. */
        if (a->lscore > s->champ_ls) {
            s->champ_ls = a->lscore; s->has_champ = 1;
            s->champ.species = 0; s->champ.len = a->len;
            memcpy(s->champ.dna, a->dna, (size_t)a->len);
        }
    }
}
double hl_ab_fit(Lab *l, World *w, int ticks) {
    AbState *s = l->ext; int hosts = 0;
    for (int i = 0; i < w->ncs; i++) hosts += w->cs[i]->species == 2;
    double T = s->esc_t ? s->esc_t : ticks;
    int erad = s->erad_t > 0;
    double dmg = fmin(1.0, s->damage / (AB_HOSTS * 100.0));
    double f = T / ticks * (1 + 0.25 * erad) * ((double)hosts / AB_HOSTS) * (1 - dmg);
    l->extra[0] = T; l->extra[1] = erad; l->extra[2] = (double)hosts / AB_HOSTS; l->extra[3] = dmg;
    if (!l->counterfactual) { l->extra[4] = replay(l, ticks, NULL); l->extra[5] = f - l->extra[4]; }
    return f;
}
int hl_ab_sel(Lab *l, World *w, PoolEntry *out) {
    int n = sel_species0(l, w, out);
    AbState *s = l->ext;
    if (!s->has_champ || s->champ_ls <= 0) return n;
    double top = 0;
    for (int i = 0; i < w->ncs; i++)
        if (w->cs[i]->species == 0 && w->cs[i]->lscore > top) top = w->cs[i]->lscore;
    if (s->champ_ls <= top) return n;                   /* un vivant porte déjà mieux */
    if (n < POOL_SIZE) n++;
    for (int i = n - 1; i > 0; i--) out[i] = out[i - 1];
    out[0] = s->champ;
    return n;
}

/* ======================= 3. commons : biens communs =======================
 * Horizon 5 × ticks. Chaque case a une capacité (14) ; la nourriture repousse en logistique
 * (rien ne repousse d'une case vide). Vider une case (stock → 0) lui retire 4 de capacité POUR
 * TOUJOURS ; capacité 0 = case morte. Pas de repousse aléatoire globale. */
typedef struct { uint8_t cap[GRID_H][GRID_W], snap[GRID_H][GRID_W]; long r0; } CmState;
void hl_cm_cfg(WorldConfig *c) { c->food_density = 1.0; c->food_regrow = 0; c->max_pop = 150; c->eat_guard = 1; }
int hl_cm_seeds(PoolEntry *o) { for (int i = 0; i < 4; i++) seed_entry(&o[i], "commons", 0); return 4; }
static long cm_cap(const World *w, const CmState *s) {
    long r = 0;
    for (int y = 0; y < w->cfg.h; y++) for (int x = 0; x < w->cfg.w; x++) r += s->cap[y][x];
    return r;
}
void hl_cm_init(Lab *l, World *w) {
    CmState *s = calloc(1, sizeof *s);
    for (int y = 0; y < w->cfg.h; y++) for (int x = 0; x < w->cfg.w; x++) { s->cap[y][x] = 14; s->snap[y][x] = w->food[y][x]; }
    s->r0 = cm_cap(w, s);
    l->ext = s;
}
void hl_cm_pre(Lab *l, World *w) {
    CmState *s = l->ext;
    for (int y = 0; y < w->cfg.h; y++) for (int x = 0; x < w->cfg.w; x++) {
        uint8_t *f = &w->food[y][x], *cp = &s->cap[y][x];
        if (s->snap[y][x] > 0 && *f == 0) *cp = *cp > 4 ? (uint8_t)(*cp - 4) : 0;   /* surrécolte */
        if (*f > *cp) *f = *cp;
        if (*f == 0 && *cp > 0 && rng_double(w->rng) < 0.002) *f = 1;               /* recolonisation lente */
        else if (*f > 0 && *f < *cp && rng_double(w->rng) < 0.06 * *f * (*cp - *f) / *cp) (*f)++;
        s->snap[y][x] = *f;
    }
}
double hl_cm_fit(Lab *l, World *w, int ticks) {
    CmState *s = l->ext; long sp = 0; int h = l->npops / 2, dead = 0;
    for (int i = ticks / 2; i < l->npops; i++) sp += l->pops[i];
    (void)h;
    double late = (double)sp / (ticks - ticks / 2);
    for (int y = 0; y < w->cfg.h; y++) for (int x = 0; x < w->cfg.w; x++) dead += s->cap[y][x] == 0;
    double rf = (double)cm_cap(w, s) / s->r0;
    double f = late / 100.0 * rf;                    /* effondrement : pop tardive → 0 et capacité perdue */
    l->extra[0] = late; l->extra[1] = rf; l->extra[2] = dead;
    if (!l->counterfactual) {                         /* baseline : fourrageur glouton, même graine */
        PoolEntry g[4]; for (int i = 0; i < 4; i++) seed_entry(&g[i], "forager", 0);
        l->extra[3] = replay_pool(l, ticks, g, 4, NULL); l->extra[4] = f - l->extra[3];
    }
    return f;
}

/* ======================= 4. pandemic : contagion SEIR =======================
 * À t = 20, 8 cas index. Infectieux (I) → sain (S) à Chebyshev ≤ 1 : p = 0.3 / tick → exposé (E, 3 ticks,
 * sans symptôme) → I (12 ticks) → mort (p = 0.3) ou guéri (R). Stimulus par créature : bit0 = je suis I,
 * bit1 = alerte reçue (5 ticks), bit2 = immunisé. Actions par EMIT v&3 : 1 alerter (rayon 2, coût 1),
 * 2 s'isoler ce tick (ni contaminer ni être contaminé, coût 2), 3 se vacciner (S → R, coût 25).
 * Nombres aléatoires communs : chaque tirage épidémique (contagion par paire d'agents et par tick,
 * issue de la maladie, cas index) est un hachage (clé d'épidémie, ids, tick), indépendant des actions
 * et du RNG du monde. Score = moyenne sur PD_REPS paires appariées (même graine avec / sans effet des
 * actions) de : morts évitées / max(morts sans actions, 10) × maintien pop − énergie des actions / mangée. */
#define PD_REPS 4
enum { PS_S = 0, PS_E, PS_I, PS_R };
typedef struct { long deaths, cost, vacc, alerts, isol; uint64_t key; } PdState;
static double pd_u(uint64_t key, uint64_t a, uint64_t b, uint64_t t) {   /* uniforme [0,1) haché */
    uint64_t z = key ^ (a * 0x9E3779B97F4A7C15ULL) ^ (b * 0xC2B2AE3D27D4EB4FULL) ^ (t * 0x165667B19E3779F9ULL);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL; z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL; z ^= z >> 31;
    return (double)(z >> 11) * 0x1.0p-53;
}
void hl_pd_init(Lab *l, World *w);
static void pd_before(World *w, Creature *c) {
    w->stimulus = (c->ls[1] == PS_I) | ((c->ls[4] > w->t) << 1) | ((c->ls[1] == PS_R) << 2);
}
void hl_pd_init(Lab *l, World *w) {
    PdState *s = calloc(1, sizeof *s);
    s->key = fnv1a(l->rng0.s, sizeof l->rng0.s, FNV_INIT);   /* identique dans les deux runs d'une paire */
    l->ext = s; w->before_vm = pd_before;
}
int hl_pd_seeds(PoolEntry *o) { for (int i = 0; i < 4; i++) seed_entry(&o[i], "pandemic", 0); return 4; }
void hl_pd_post(Lab *l, World *w) {
    PdState *s = l->ext; int W = w->cfg.w, H = w->cfg.h;
    if (l->counterfactual != 1)
        for (int i = 0; i < w->nem; i++) {
            Creature *c = w->em[i].c; int a = w->em[i].v & 3;
            if (c->e <= 0) continue;
            if (a == 1) { c->e -= 1; s->cost += 1; s->alerts++;
                if (c->ls[1] == PS_I) c->lscore += 0.25;
                for (int j = 0; j < w->ncs; j++) if (w->cs[j] != c && cheb(c, w->cs[j], W, H) <= 2) w->cs[j]->ls[4] = (int)w->t + 5; }
            else if (a == 2) { c->e -= 2; s->cost += 2; s->isol++; c->ls[3] = (int)w->t;
                if (c->ls[1] == PS_I) c->lscore += 1; }
            else if (a == 3 && c->ls[1] == PS_S) { c->e -= 25; s->cost += 25; s->vacc++; c->lscore += 0.5; c->ls[1] = PS_R; }
        }
    if (w->t == 20) {                                   /* cas index : les 8 plus petits hachages d'id */
        for (int k = 0; k < 8; k++) {
            Creature *best = NULL; double bu = 2;
            for (int i = 0; i < w->ncs; i++) {
                Creature *c = w->cs[i];
                if (c->ls[1] != PS_S || c->e <= 0) continue;
                double u = pd_u(s->key, (uint64_t)c->id, 7, 0);
                if (u < bu) { bu = u; best = c; }
            }
            if (best) { best->ls[1] = PS_I; best->ls[2] = 12; }
        }
    }
    for (int i = 0; i < w->ncs; i++) {                  /* transmission */
        Creature *c = w->cs[i];
        if (c->ls[1] != PS_I || c->e <= 0 || c->ls[3] == w->t) continue;
        for (int j = 0; j < w->ncs; j++) {
            Creature *o = w->cs[j];
            if (o->ls[1] != PS_S || o->ls[3] == w->t || o->e <= 0 || cheb(c, o, W, H) > 1) continue;
            if (pd_u(s->key, (uint64_t)c->id, (uint64_t)o->id, (uint64_t)w->t) < 0.3) { o->ls[1] = PS_E; o->ls[2] = 4; }
        }
    }
    for (int i = 0; i < w->ncs; i++) {                  /* progression */
        Creature *c = w->cs[i];
        if (c->ls[1] == PS_E && --c->ls[2] <= 0) { c->ls[1] = PS_I; c->ls[2] = 12; }
        else if (c->ls[1] == PS_I && --c->ls[2] <= 0) {
            if (pd_u(s->key, (uint64_t)c->id, 3, 1) < 0.3) { if (c->e > 0) { c->e = 0; s->deaths++; } }
            else c->ls[1] = PS_R;
        }
    }
}
/* un run du labo sur une graine donnée ; cf = 1 sans effet des actions, 2 avec (pas de récursion) */
static double pd_run(Lab *l, int ticks, const Rng *rng, int cf, double *ex) {
    Lab l2; lab_init(&l2, l->id, ticks);
    l2.counterfactual = cf;
    Rng r = *rng;
    CycleResult *res = malloc(sizeof *res);
    run_cycle(&l2, l->pool0, l->npool0, &r, ticks, res);
    double f = res->fitness;
    if (ex) memcpy(ex, l2.extra, sizeof l2.extra);
    world_free(res->w); lab_free(&l2); free(res);
    return f;
}
static double pd_pair(double d, double pm, double cost, const double *b) {
    return (b[0] - d) / fmax(b[0], 10.0) * fmin(1.0, b[1] > 0 ? pm / b[1] : 0) - cost;
}
double hl_pd_fit(Lab *l, World *w, int ticks) {
    PdState *s = l->ext;
    double pm = 0; for (int i = 0; i < l->npops; i++) pm += l->pops[i];
    pm /= ticks;
    double cost = (double)s->cost / (w->st.energy_eaten + 1.0);
    l->extra[0] = (double)s->deaths; l->extra[1] = pm; l->extra[3] = cost;
    l->extra[4] = (double)s->vacc; l->extra[5] = (double)s->isol;
    if (l->counterfactual) return 0;
    double ex[N_EXTRA], f = 0, d0 = 0;
    for (int k = 0; k < PD_REPS; k++) {
        Rng r = l->rng0;
        if (k) { uint64_t sd = l->rng0.s[0] ^ (0x9E3779B97F4A7C15ULL * (uint64_t)k); rng_seed(&r, sd); }
        double d = s->deaths, p = pm, c = cost;
        if (k) { pd_run(l, ticks, &r, 2, ex); d = ex[0]; p = ex[1]; c = ex[3]; }
        pd_run(l, ticks, &r, 1, ex);                    /* baseline appariée : même graine, actions sans effet */
        f += pd_pair(d, p, c, ex) / PD_REPS; d0 += ex[0] / PD_REPS;
    }
    l->extra[2] = d0;
    return f;
}

/* ======================= 5. code-repair : réparation de programme =======================
 * Chaque créature reçoit un patient : programme de 8 octets tiré d'une famille aléatoire
 * (SENSE_STIM ; MOV_BA ; op1 ; op2 ; EMIT ; YIELD, op ∈ INC/DEC/ADD/SUB/XOR/AND k) puis cassé
 * par 1 ou 2 bit-flips connus (bits 0..4 d'un opcode, 0..7 d'un immédiat), cassure visible sur
 * les tests. Il est mappé sur STORE/LOAD imm 0xE0..0xE7 (fenêtre win, voir vm.c). Signal par tick :
 * stimulus = tests passés (0..16) | 0x40 si mieux que le meilleur vu | 0x20 nouveau patient.
 * Réussite = les 256 entrées donnent la sortie de référence (pas seulement les 16 tests) ;
 * score = 1 + 0.5 × (nb de flips injectés / octets modifiés). Délai 80 ticks, sinon échec. */
#define CR_LEN 8
#define CR_TIMEOUT 80
static const uint8_t CR_TESTS[16] = { 0, 1, 2, 3, 5, 7, 8, 13, 21, 34, 55, 89, 100, 144, 200, 255 };
typedef struct { World *sb; Creature tc; long attempts, succ, timeouts; double score; } CrState;
static int cr_run(CrState *s, const uint8_t *prog, int x) {
    Creature *c = &s->tc;
    memcpy(c->dna, prog, CR_LEN); c->len = CR_LEN; c->dirty[0] = 0;
    c->ip = 0; c->a = c->b = 0; c->e = 1000; c->win_len = 0; c->x = c->y = 1; c->d = 0;
    s->sb->stimulus = x; s->sb->nem = 0;
    vm_run(c, s->sb);
    return c->emitted;
}
static int cr_pass(CrState *s, const uint8_t *p, const uint8_t *ref) {
    int n = 0;
    for (int i = 0; i < 16; i++) n += cr_run(s, p, CR_TESTS[i]) == cr_run(s, ref, CR_TESTS[i]);
    return n;
}
static int cr_equiv(CrState *s, const uint8_t *p, const uint8_t *ref) {
    for (int x = 0; x < 256; x++) if (cr_run(s, p, x) != cr_run(s, ref, x)) return 0;
    return 1;
}
static void cr_assign(World *w, CrState *s, Creature *c) {
    static const uint8_t OPS[6] = { OP_INC_A, OP_DEC_A, OP_ADD, OP_SUB, OP_XOR, OP_AND };
    uint8_t *ref = c->win0 + 8, *brk = c->win0;
    for (;;) {
        int k = 0;
        ref[k++] = OP_SENSE_STIM; ref[k++] = OP_MOV_BA;
        for (int j = 0; j < 2; j++) {
            uint8_t op = OPS[rng_below(w->rng, 6)];
            ref[k++] = op;
            if (op == OP_AND) ref[k++] = (uint8_t)rng_below(w->rng, 256);
        }
        ref[k++] = OP_EMIT;
        while (k < CR_LEN) ref[k++] = OP_YIELD;
        if (!cr_equiv(s, ref, (const uint8_t[CR_LEN]){ OP_SENSE_STIM, OP_EMIT, OP_YIELD, OP_YIELD, OP_YIELD, OP_YIELD, OP_YIELD, OP_YIELD })) break;  /* pas l'identité */
    }
    for (int tries = 0; tries < 50; tries++) {
        memcpy(brk, ref, CR_LEN);
        int m = 1 + (int)rng_below(w->rng, 2);
        for (int j = 0; j < m; j++) {
            int p = (int)rng_below(w->rng, 6);           /* zone code (avant le remplissage) */
            int imm = p > 0 && ref[p - 1] == OP_AND;
            brk[p] ^= (uint8_t)(1u << rng_below(w->rng, imm ? 8 : 5));
        }
        c->ls[3] = 0; for (int i = 0; i < CR_LEN; i++) c->ls[3] += brk[i] != ref[i];
        if (c->ls[3] && cr_pass(s, brk, ref) < 16) break;   /* cassure visible sur les tests */
    }
    memcpy(c->win, brk, CR_LEN); c->win_len = CR_LEN;
    c->ls[0] = 1; c->ls[1] = cr_pass(s, c->win, ref); c->ls[2] = (int)w->t; c->ls[4] = c->ls[1];
    memcpy(c->win0 + 16, c->win, CR_LEN);
    s->attempts++;
}
static void cr_before(World *w, Creature *c) {
    CrState *s = w->lab->ext;
    if (!c->ls[0]) { cr_assign(w, s, c); w->stimulus = c->ls[1] | 0x60; return; }
    int p = c->ls[4];
    if (memcmp(c->win, c->win0 + 16, CR_LEN)) { p = cr_pass(s, c->win, c->win0 + 8); c->ls[4] = p; memcpy(c->win0 + 16, c->win, CR_LEN); }
    if (p == 16 && cr_equiv(s, c->win, c->win0 + 8)) {
        int patch = 0; for (int i = 0; i < CR_LEN; i++) patch += c->win[i] != c->win0[i];
        s->succ++; c->ls[5]++;
        s->score += 1 + 0.5 * c->ls[3] / (double)(patch > c->ls[3] ? patch : c->ls[3]);
        c->e += 100; cap_e(c);
        cr_assign(w, s, c); w->stimulus = c->ls[1] | 0x60; return;
    }
    if (w->t - c->ls[2] > CR_TIMEOUT) { s->timeouts++; cr_assign(w, s, c); w->stimulus = c->ls[1] | 0x60; return; }
    w->stimulus = p | (p > c->ls[1] ? 0x40 : 0);
    if (p > c->ls[1]) c->ls[1] = p;
}
static void cr_free(Lab *l) { CrState *s = l->ext; if (s) { free(s->sb->rng); world_free(s->sb); } }
void hl_cr_init(Lab *l, World *w) {
    CrState *s = calloc(1, sizeof *s);
    WorldConfig sc = cfg_default(); sc.max_pop = 0; sc.food_density = 0;
    Rng *r = malloc(sizeof *r); rng_seed(r, 0);
    s->sb = world_new(NULL, 0, r, &sc, NULL);
    l->ext = s; l->ext_free = cr_free; w->before_vm = cr_before;
}
int hl_cr_seeds(PoolEntry *o) { for (int i = 0; i < 4; i++) seed_entry(&o[i], "repair", 0); return 4; }
void hl_cr_rank(const Creature *c, double k[2]) { k[0] = c->ls[5]; k[1] = c->e; }
/* baseline : chercheur aléatoire sans mémoire (flip, pas de retour arrière) */
static int cr_random_genome(PoolEntry *o) {
    int k = 0; uint8_t *d = o->dna;
    d[k++] = OP_MOV_AB; d[k++] = OP_JNZ; d[k++] = 1; d[k++] = OP_INC_B;
    for (int p = 0; p < CR_LEN; p++) { d[k++] = OP_LOAD; d[k++] = (uint8_t)(0xE0 + p); d[k++] = OP_XOR; d[k++] = OP_STORE; d[k++] = (uint8_t)(0xE0 + p); d[k++] = OP_YIELD; }
    d[k++] = OP_MOV_AB; d[k++] = OP_ADD; d[k++] = OP_MOV_BA; d[k++] = OP_SPLIT; d[k++] = OP_LOOP;
    o->len = k; o->species = 0;
    return 1;
}
double hl_cr_fit(Lab *l, World *w, int ticks) {
    CrState *s = l->ext; long active = 0;
    for (int i = 0; i < w->ncs; i++) active += w->cs[i]->ls[0] != 0;
    long resolved = s->attempts - active;
    double f = resolved > 0 ? s->score / resolved : 0;
    l->extra[0] = (double)s->succ; l->extra[1] = (double)resolved; l->extra[2] = resolved > 0 ? (double)s->succ / resolved : 0;
    l->extra[3] = s->succ ? s->score / s->succ - 1 : 0;
    if (!l->counterfactual) {
        PoolEntry g[4]; for (int i = 0; i < 4; i++) cr_random_genome(&g[i]);
        l->extra[4] = replay_pool(l, ticks, g, 4, NULL); l->extra[5] = f - l->extra[4];
    }
    return f;
}

/* ======================= 6. self-evolve : la seed pilote son évolution =======================
 * Octets 2..5 du génome = taux de substitution, taux d'indel, crossover, zone protégée (lus à SPLIT
 * par mutate_evo, bornés, voir mutate.c ; cfg.self_evo n'est posé QUE dans ce labo). Le monde change
 * de régime toutes les 50 ticks : une cible R ∈ 0..15 (nouvelle à chaque régime, tirée par la graine
 * du cycle, donc impossible à mémoriser) ; EMIT v rapporte 6 − 2·|v&15 − R| si l'écart d < 4, sinon coûte 1.
 * Fitness = moyenne sur les régimes de Σ(1 − d/4)⁺ / créature-tick : vitesse d'adaptation.
 * Fitness et baseline = moyennes sur SE_REPS = 4 graines appariées ; baseline = même pool, paramètres fixes
 * du moteur (mut 2 %, dup 5 %). */
#define SE_PERIOD 50
#define SE_MAX_AGE 60    /* ferme l'exploit « ne plus se reproduire » (gel par absence d'enfants) */
#define SE_REPS 4
typedef struct { int R; double corr[16]; long ct[16]; uint64_t key; } SeState;
void hl_se_cfg(WorldConfig *c) { (void)c; }
int hl_se_seeds(PoolEntry *o) { for (int i = 0; i < 4; i++) seed_entry(&o[i], "selfevo", 0); return 4; }
void hl_se_init(Lab *l, World *w) {
    SeState *s = calloc(1, sizeof *s);
    s->key = fnv1a(l->rng0.s, sizeof l->rng0.s, FNV_INIT);   /* cibles communes aux runs appariés */
    l->ext = s; w->cfg.self_evo = l->counterfactual != 1;
}
void hl_se_pre(Lab *l, World *w) {
    SeState *s = l->ext;
    for (int i = 0; i < w->ncs; i++) if (w->cs[i]->age >= SE_MAX_AGE) w->cs[i]->e = 0;   /* mortels : sans descendance, pas d'évolution */
    if ((w->t - 1) % SE_PERIOD == 0) { int r; uint64_t k = 0; do r = (int)(pd_u(s->key, (uint64_t)w->t, k++, 5) * 16); while (w->t > 1 && r == s->R); s->R = r; }
}
void hl_se_post(Lab *l, World *w) {
    SeState *s = l->ext; int g = (int)((w->t - 1) / SE_PERIOD) & 15;
    for (int i = 0; i < w->nem; i++) {
        Creature *c = w->em[i].c;
        int d = abs((w->em[i].v & 15) - s->R);           /* récompense graduée : la dérive ±1 peut grimper */
        c->e += d < 4 ? 6 - 2 * d : -1; cap_e(c);
        if (d < 4) { double cred = 1.0 - d / 4.0; s->corr[g] += cred; c->lscore += cred; }
    }
    s->ct[g] += w->ncs;
}
double hl_se_fit(Lab *l, World *w, int ticks) {
    SeState *s = l->ext; int ng = (ticks + SE_PERIOD - 1) / SE_PERIOD; double f = 0;
    if (ng > 16) ng = 16;
    for (int g = 0; g < ng; g++) f += s->ct[g] ? (double)s->corr[g] / s->ct[g] : 0;
    f /= ng;
    if (!l->counterfactual) {
        /* SE_REPS paires appariées (même graine, mêmes cibles) : paramètres du génome vs paramètres fixes */
        double fe = f, fb = 0;
        for (int k = 0; k < SE_REPS; k++) {
            Rng r = l->rng0;
            if (k) { rng_seed(&r, l->rng0.s[0] ^ (0x9E3779B97F4A7C15ULL * (uint64_t)k)); fe += pd_run(l, ticks, &r, 2, NULL); }
            fb += pd_run(l, ticks, &r, 1, NULL);
        }
        f = fe / SE_REPS;
        l->extra[0] = fb / SE_REPS; l->extra[1] = f - l->extra[0];
        double m = 0, id = 0, xo = 0, pr = 0;
        for (int i = 0; i < w->ncs; i++) { EvoParams p = evo_params(w->cs[i]->dna, w->cs[i]->len); m += p.mut; id += p.indel; xo += p.xover; pr += p.prot; }
        int n = w->ncs ? w->ncs : 1;
        l->extra[2] = m / n; l->extra[3] = id / n; l->extra[4] = xo / n; l->extra[5] = pr / n;
    }
    return f;
}
/* pandemic : la contribution (isolement, alerte, vaccin) d'abord. L'âge départage :
 * l'énergie favoriserait ceux qui ne paient jamais l'isolement. */
void hl_pd_rank(const Creature *c, double k[2]) { k[0] = c->lscore; k[1] = c->age; }

/* ======================= 7. diagnose : le médicament n'est pas dans le stimulus =======================
 * La marque m = (m0 + tick) mod 8 est posée dans le stimulus. Elle change à chaque tick : une
 * constante (LOADI) ne peut pas la suivre. Le médicament est ((m + 1) XOR 4) mod 8 : incrémenter,
 * puis retourner le bit 2. Les deux ordres commutent sur 3 bits, donc (m XOR 4) + 1 est le même
 * calcul. On note la distance de Hamming des 3 bits bas, pas la distance circulaire :
 *   0 bit  → 1,00    l'incrément seul est toujours à 1 bit (0,55), plus un palier
 *   1 bit  → 0,55    une constante tourne autour de 0,41 et ne dépasse pas ce palier
 *   2 bits → 0,20    copier la marque reste en dessous (souvent 2 ou 3 bits)
 *   3 bits → 0,05
 * Le silence vaut 0. La baseline renverse le stimulus (m XOR 7) et note toujours contre le vrai
 * médicament : un génome qui lit s'effondre, un génome qui récite non.
 * La seed ne contient ni SENSE_STIM ni EMIT. On photographie le meilleur taux après 40 ticks,
 * seulement si le corps s'est divisé : une constante chanceuse ne tient pas sur une marque qui
 * tourne, et un calculateur stérile ne remplace pas la lignée. */
typedef struct {
    int mark;
    double credit, emits, best_rate;
    PoolEntry best;
    int has_best;
} DgState;

void hl_dg_cfg(WorldConfig *c) {
    c->mut_rate = 0.05;
    c->dup_rate = 0.02;
    c->max_pop = 100;
    c->food_density = 0.5;
}
int hl_dg_seeds(PoolEntry *o) { for (int i = 0; i < 4; i++) seed_entry(&o[i], "diagnose", 0); return 4; }
void hl_dg_init(Lab *l, World *w) {
    DgState *s = calloc(1, sizeof *s);
    s->best_rate = -1;
    s->mark = (int)rng_below(w->rng, 8);
    l->ext = s;
}
static int dg_mark(const DgState *s, int t) { return (s->mark + t) & 7; }
void hl_dg_pre(Lab *l, World *w) {
    DgState *s = l->ext;
    int m = dg_mark(s, w->t);
    w->stimulus = l->counterfactual ? (m ^ 7) : m;
    /* ((m + 1) XOR 4) == (m - 3) mod 8. SENSE_HIST 2 relit cette marque et tombe juste
     * sans rien calculer. L'historique est vidé : le seul capteur utile est le stimulus courant. */
    w->hist_n = 0;
}
void hl_dg_post(Lab *l, World *w) {
    DgState *s = l->ext;
    int drug = ((dg_mark(s, w->t) + 1) ^ 4) & 7;
    for (int i = 0; i < w->nem; i++) {
        Creature *a = w->em[i].c;
        if (!a || a->species != 0 || a->e < 0) continue;
        int got = w->em[i].v & 7;
        int bits = (got ^ drug) & 7;
        bits = (bits & 1) + ((bits >> 1) & 1) + ((bits >> 2) & 1);
        double c = bits == 0 ? 1.0 : bits == 1 ? 0.55 : bits == 2 ? 0.20 : 0.05;
        a->lscore += c;
        s->credit += c;
        s->emits += 1;
        double rate = a->age > 0 ? a->lscore / a->age : 0;
        if (a->age >= 40 && a->splits > 0 && rate > s->best_rate) {
            s->best_rate = rate;
            s->has_best = 1;
            s->best.species = 0;
            s->best.len = a->len;
            memcpy(s->best.dna, a->dna, (size_t)a->len);
        }
    }
}
double hl_dg_fit(Lab *l, World *w, int ticks) {
    (void)w;
    DgState *s = l->ext;
    double acc = s->emits > 0 ? s->credit / s->emits : 0;
    l->extra[0] = acc;
    l->extra[1] = s->emits;
    l->extra[2] = s->best_rate < 0 ? 0 : s->best_rate;
    if (!l->counterfactual) {
        double b = replay(l, ticks, NULL);
        l->extra[3] = b;
        l->extra[4] = acc - b;
    }
    return acc;
}
int hl_dg_sel(Lab *l, World *w, PoolEntry *out) {
    /* Seuls les corps qui se sont divisés entrent dans le pool. Un émetteur stérile
     * (plus de SPLIT) figeait la recherche : né à chaque cycle, jamais muté. */
    Creature *v[CMAX]; int n = 0, k = 0;
    (void)l;
    for (int i = 0; i < w->ncs; i++)
        if (w->cs[i]->species == 0 && w->cs[i]->splits > 0) v[n++] = w->cs[i];
    for (int i = 1; i < n; i++) {
        Creature *c = v[i]; int j = i - 1;
        while (j >= 0 && (v[j]->lscore < c->lscore
               || (v[j]->lscore == c->lscore && (v[j]->e < c->e || (v[j]->e == c->e && v[j]->age < c->age))))) {
            v[j + 1] = v[j]; j--;
        }
        v[j + 1] = c;
    }
    for (int i = 0; i < n && k < POOL_SIZE; i++) {
        out[k].species = 0; out[k].len = v[i]->len; memcpy(out[k].dna, v[i]->dna, (size_t)v[i]->len); k++;
    }
    n = k;
    DgState *s = l->ext;
    if (!s->has_best) return n;
    double top = -1;
    for (int i = 0; i < w->ncs; i++) {
        Creature *c = w->cs[i];
        if (c->species != 0 || c->age <= 0 || c->splits <= 0) continue;
        double rate = c->lscore / c->age;
        if (rate > top) top = rate;
    }
    if (top >= 0 && s->best_rate <= top) return n;
    if (n < POOL_SIZE) n++;
    for (int i = n - 1; i > 0; i--) out[i] = out[i - 1];
    out[0] = s->best;
    return n < 1 ? 1 : n;
}

/* ======================= 8. general : apprendre pendant sa vie, généraliser =======================
 * Définition opérationnelle : un génome est « intelligent » ici s'il apprend PENDANT sa vie une règle
 * qu'il ne connaît pas à la naissance, et s'il le fait sur des tâches que la sélection n'a jamais vues.
 * Une vie = 64 essais. À chaque essai le stimulus vaut  indice (bits 0-1) | récompense de l'essai
 * précédent (bit 2) ; la réponse est EMIT v (v & 3), le silence est faux. La bonne réponse n'est jamais
 * dans le stimulus, l'historique est vidé à chaque tick. Familles de tâches (table = 4 réponses 2 bits) :
 *   0 association  réponse = table[indice]
 *   1 renversement  idem, la table change à l'essai 24 (contingence inversée en cours de vie)
 *   2 bandit        une seule réponse payée, quel que soit l'indice
 *   3 mémoire       réponse = table[indice de l'essai PRÉCÉDENT]   ← famille jamais vue en sélection
 * Split : la sélection (monde + fitness renvoyée au runner) ne voit que les familles 0-2 avec les tables
 * dont le hachage n'est pas réservé, et les bandits 0..2. Le jeu tenu à l'écart (32 tâches fixes) :
 * tables réservées, bandits 0..3, et la famille mémoire. Score d'une vie = justesse des essais 32..63.
 * Fitness affichée = jeu tenu à l'écart ; fitness de sélection = tâches d'entraînement du cycle. */
#define GN_TRIALS 64
#define GN_REV    24
#define GN_LATE   32
#define GN_HELD_N 32
#define GN_TRAIN_N 16
#define GN_TOP    8
#define GN_REWARD 2
typedef struct { int fam, t1, t2; } GnTask;
static int gn_held_code(int code) { return (((unsigned)code * 167u) >> 3 & 3u) == 0; }
static int gn_draw_code(Rng *r, int held, int avoid) {
    for (;;) { int c = (int)rng_below(r, 256); if (gn_held_code(c) == held && c != avoid) return c; }
}
static GnTask gn_draw(Rng *r, int held) {
    GnTask t; t.fam = (int)rng_below(r, held ? 4 : 3);
    if (t.fam == 2) { int k = (int)rng_below(r, held ? 4 : 3); t.t1 = t.t2 = k * 0x55; }
    else { t.t1 = gn_draw_code(r, held, -1); t.t2 = t.fam == 1 ? gn_draw_code(r, held, t.t1) : t.t1; }
    return t;
}
static int gn_target(const GnTask *t, int trial, int cue, int prev) {
    int tab = (t->fam == 1 && trial >= GN_REV) ? t->t2 : t->t1;
    return (tab >> (2 * (t->fam == 3 ? prev : cue))) & 3;
}
typedef struct { World *sb; Creature tc; } GnState;
void hl_gn_free(Lab *l) { GnState *s = l->ext; if (s) { free(s->sb->rng); world_free(s->sb); } }
static GnState *gn_state_new(void) {
    GnState *s = calloc(1, sizeof *s);
    WorldConfig sc = cfg_default(); sc.max_pop = 0; sc.food_density = 0;
    Rng *r = malloc(sizeof *r); rng_seed(r, 0);
    s->sb = world_new(NULL, 0, r, &sc, NULL);
    return s;
}
/* Une vie en bac à sable. mode : 0 normal, 1 génome gelé (STORE sans effet durable),
 * 2 récompense masquée, 3 réponse aléatoire, 4+v réponse constante v. ok[i] += 1 si l'essai i est juste. */
static void gn_life(GnState *s, const PoolEntry *g, const GnTask *t, Rng *cues, int mode, int *ok) {
    Creature *c = &s->tc; World *w = s->sb;
    memset(c, 0, sizeof *c);
    c->len = g->len; memcpy(c->dna, g->dna, (size_t)g->len); c->x = c->y = 2; c->emitted = -1;
    int prev = (int)rng_below(cues, 4), r = 0;
    for (int i = 0; i < GN_TRIALS; i++) {
        int cue = (int)rng_below(cues, 4), tgt = gn_target(t, i, cue, prev), resp;
        if (mode >= 3) resp = mode == 3 ? (int)rng_below(cues, 4) : mode - 4;
        else {
            w->stimulus = cue | ((mode == 2 ? 0 : r) << 2);
            w->hist_n = 0; w->nem = 0; c->e = 1000;
            vm_run(c, w);
            resp = c->emitted;
            if (mode == 1) memcpy(c->dna, g->dna, (size_t)g->len);
        }
        r = resp >= 0 && (resp & 3) == tgt;
        ok[i] += r;
        prev = cue;
    }
}
/* Justesse moyenne (essais 32..63) de n génomes sur un jeu de tâches. held : jeu tenu à l'écart
 * (fixe, graine constante) ; sinon tâches d'entraînement tirées de seed. fam >= 0 : une seule famille.
 * curve (si non NULL) : [0] essais 0..15, [1] essais 48..63. */
double gn_eval(const PoolEntry *g, int n, int held, uint64_t seed, int ntask, int mode, int fam, double *curve) {
    GnState *s = gn_state_new();
    int ok[GN_TRIALS] = { 0 }; long lives = 0;
    for (int j = 0; j < ntask; j++) {
        Rng tr; rng_seed(&tr, held ? 0x6E6E0000ULL + (uint64_t)j : seed + (uint64_t)j * 0x9E3779B97F4A7C15ULL);
        GnTask t = gn_draw(&tr, held);
        if (fam >= 0 && t.fam != fam) continue;
        for (int i = 0; i < n; i++) { Rng cu = tr; gn_life(s, &g[i], &t, &cu, mode, ok); lives++; }
    }
    free(s->sb->rng); world_free(s->sb); free(s);
    if (!lives) { if (curve) curve[0] = curve[1] = 0; return 0; }
    double late = 0, a = 0, b = 0;
    for (int i = GN_LATE; i < GN_TRIALS; i++) late += ok[i];
    for (int i = 0; i < 16; i++) { a += ok[i]; b += ok[GN_TRIALS - 16 + i]; }
    if (curve) { curve[0] = a / (16.0 * lives); curve[1] = b / (16.0 * lives); }
    return late / ((double)(GN_TRIALS - GN_LATE) * lives);
}
/* Dans le monde : chaque créature vit des épisodes de 64 essais (1 par tick) sur des tâches
 * d'entraînement ; un essai juste rapporte de l'énergie. lscore = justesse des essais 32..63, rétrécie. */
static void gn_before(World *w, Creature *c) {
    int r = 0;
    if (!c->ls[0]) { GnTask t = gn_draw(w->rng, 0); c->ls[0] = 1 | t.fam << 1 | t.t1 << 3 | t.t2 << 11; c->ls[1] = 0; c->ls[2] = (int)rng_below(w->rng, 4); }
    else {
        r = c->emitted >= 0 && (c->emitted & 3) == c->ls[5];
        if (r) { c->e += GN_REWARD; cap_e(c); }
        if (c->ls[1] >= GN_LATE) { c->ls[3] += r; c->ls[4]++; c->lscore = c->ls[3] / (c->ls[4] + 8.0); }
        if (++c->ls[1] == GN_TRIALS) { GnTask t = gn_draw(w->rng, 0); c->ls[0] = 1 | t.fam << 1 | t.t1 << 3 | t.t2 << 11; c->ls[1] = 0; }
    }
    GnTask t = { (c->ls[0] >> 1) & 3, (c->ls[0] >> 3) & 255, (c->ls[0] >> 11) & 255 };
    int cue = (int)rng_below(w->rng, 4);
    c->ls[5] = gn_target(&t, c->ls[1], cue, c->ls[2] & 3);
    c->ls[2] = cue;
    w->stimulus = cue | r << 2;
}
void hl_gn_cfg(WorldConfig *c) { c->food_density = 0.15; c->food_regrow = 2; }
int hl_gn_seeds(PoolEntry *o) { for (int i = 0; i < 4; i++) seed_entry(&o[i], "general", 0); return 4; }
void hl_gn_init(Lab *l, World *w) { (void)l; w->before_vm = gn_before; }
void hl_gn_pre(Lab *l, World *w) { (void)l; w->hist_n = 0; }
double hl_gn_fit(Lab *l, World *w, int ticks) {
    (void)ticks;
    Creature *v[CMAX]; int n = 0;
    for (int i = 0; i < w->ncs; i++) v[n++] = w->cs[i];
    for (int i = 1; i < n; i++) {                       /* lscore puis énergie */
        Creature *c = v[i]; int j = i - 1;
        while (j >= 0 && (v[j]->lscore < c->lscore || (v[j]->lscore == c->lscore && v[j]->e < c->e))) { v[j + 1] = v[j]; j--; }
        v[j + 1] = c;
    }
    if (n > GN_TOP) n = GN_TOP;
    PoolEntry *g = malloc(sizeof(PoolEntry) * GN_TOP);
    for (int i = 0; i < n; i++) { g[i].species = 0; g[i].len = v[i]->len; memcpy(g[i].dna, v[i]->dna, (size_t)v[i]->len); }
    double tr = 0, curve[2] = { 0, 0 };
    memset(l->extra, 0, sizeof l->extra);
    if (n) {
        tr = gn_eval(g, n, 0, rng_next(w->rng), GN_TRAIN_N, 0, -1, NULL);
        l->extra[0] = gn_eval(g, n, 1, 0, GN_HELD_N, 0, -1, curve);
        l->extra[2] = gn_eval(g, n, 1, 0, GN_HELD_N, 1, -1, NULL);
    }
    free(g);
    l->extra[1] = tr; l->extra[3] = l->extra[0] - l->extra[2]; l->extra[4] = curve[0]; l->extra[5] = curve[1];
    return tr;                                          /* la sélection ne voit que l'entraînement */
}
