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
    FoldState *s = xcalloc(1, sizeof *s); Rng r; rng_seed(&r, 12345); double t = 0;
    for (int k = 0; k < N_HP; k++) for (int j = 0; j < 64; j++) t += (double)fold_seq(s, k, NULL, &r) / HP[k].opt / 64;
    free(s);
    return cached = t / N_HP;
}
static void fold_before(World *w, Creature *c) {
    if (c->ls[0]) return;
    FoldState *s = w->lab->ext;
    c->ls[0] = 1;
    c->lscore = fold_score(s, c);
    c->e += (int)(200 * c->lscore); cap_e(c);      /* bien plier nourrit : pression de sélection (60 → 200 : sinon le meilleur plieur se perd) */
}
void hl_fold_free(Lab *l);
static void fold_init(Lab *l, World *w) {
    FoldState *s = xcalloc(1, sizeof *s);
    WorldConfig sc = cfg_default(); sc.max_pop = 0; sc.food_density = 0;
    Rng *r = xmalloc(sizeof *r); rng_seed(r, 0);
    s->sb = world_new(NULL, 0, r, &sc, NULL);
    l->ext = s; l->ext_free = hl_fold_free; w->before_vm = fold_before;
}
void hl_fold_free(Lab *l) { FoldState *s = l->ext; if (s) { free(s->sb->rng); world_free(s->sb); } }
void hl_fold_cfg(WorldConfig *c) { c->food_density = 0.25; c->food_regrow = 3; }
/* Pop initiale diverse : la graine seule (LOADI 1 ; EMIT) émet une constante, donc toute la pop
 * plie pareil (variance de fitness nulle, labo figé à 0.2935). On ajoute des variantes du seul
 * préfixe : constante 2 (spirale droite, 0.39) et une réactive au stimulus (PP → gauche, sinon
 * droite, 0.468 = optimum des politiques sans mémoire), corps inchangé. */
int hl_fold_seeds(PoolEntry *o) {
    static const uint8_t pre[2][6] = { { OP_LOADI, 2 }, { OP_SENSE_STIM, OP_JZ, 2, OP_LOADI, 1, OP_INC_A } };
    static const int pn[2] = { 2, 6 };
    seed_entry(&o[0], "fold", 0);                  /* graine d'origine, préfixe LOADI 1 (2 octets) */
    int rest = o[0].len - 2;
    for (int i = 0; i < 2; i++) {
        o[i + 1].species = 0; o[i + 1].len = pn[i] + rest;
        memcpy(o[i + 1].dna, pre[i], (size_t)pn[i]);
        memcpy(o[i + 1].dna + pn[i], o[0].dna + 2, (size_t)rest);
    }
    return 3;
}
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
    CycleResult *res = xmalloc(sizeof *res);
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
    l->ext = xcalloc(1, sizeof(AbState));
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
    CmState *s = xcalloc(1, sizeof *s);
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
    PdState *s = xcalloc(1, sizeof *s);
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
    CycleResult *res = xmalloc(sizeof *res);
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
    CrState *s = xcalloc(1, sizeof *s);
    WorldConfig sc = cfg_default(); sc.max_pop = 0; sc.food_density = 0;
    Rng *r = xmalloc(sizeof *r); rng_seed(r, 0);
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
    SeState *s = xcalloc(1, sizeof *s);
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
    DgState *s = xcalloc(1, sizeof *s);
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
    if (k == 0 && !((DgState *)l->ext)->has_best) {
        /* Aucun corps ne s'est divisé : un pool hérité tout stérile ne produit plus jamais
         * de nouveau pool (eval 0/0 à chaque cycle, labo figé). On remet les seeds, qui portent
         * SPLIT, derrière les meilleurs émetteurs stériles : la justesse ne compte que les
         * émissions, la sélection appariée tranche. */
        k = hl_dg_seeds(out);
        for (int i = 0; i < w->ncs && k < POOL_SIZE; i++) {
            Creature *c = w->cs[i];
            if (c->species != 0 || c->lscore <= 0) continue;
            out[k].species = 0; out[k].len = c->len; memcpy(out[k].dna, c->dna, (size_t)c->len); k++;
        }
        return k;
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

/* Expérience « null de la sélection directe » (experiments/general-joint-negative) :
 * compiler avec -DGN_DIRECT_SEL -DGN_DIRECT_FAM=6|7. La fitness et la sélection deviennent la
 * justesse tenue à l'écart de la mémoire 2 (6) ou 3 (7), sans les opérateurs de naissance.
 * Sans ce drapeau, le binaire est celui de main (même code, mêmes hashes). */
#ifndef GN_DIRECT_FAM
#define GN_DIRECT_FAM 6
#endif
/* Levier « regs » : RPUSH / RPEEK actifs dans general (monde et bac à sable). -DGN_REGS=0 = main. */
#ifndef GN_REGS
#define GN_REGS 1
#endif
/* ======================= 8. general : apprendre pendant sa vie, généraliser =======================
 * Définition opérationnelle : un génome est « intelligent » ici s'il apprend PENDANT sa vie une règle
 * qu'il ne connaît pas à la naissance, et s'il le fait sur des tâches que la sélection n'a jamais vues.
 * Vie = 64 essais. Stimulus = indice c (bits 0-1) | récompense affichée de l'essai précédent (bit 2) | début de tâche (bit 3) ;
 * réponse = EMIT v (v & 3), silence = faux. La bonne réponse n'est jamais dans le stimulus et
 * l'historique est vidé à chaque tick : se souvenir de l'indice précédent (p1, p2, p3) coûte un STORE.
 * Familles (table 2 bits / entrée) :
 *   0 association   T[c]                 1 renversement : T puis T' à l'essai 24 (ou 1-3 instants aléatoires)
 *   2 bandit        k (constant ou qui change)       3 mémoire-1  T[p1]   (cas particulier de la paire)
 *   4 paire         T16[c + 4·p1]        5 XOR  c ^ p1 ^ k
 *   6 mémoire-2     T[p2]                7 mémoire-3  T[p3]
 *   p1, p2, p3 = indice il y a 1, 2, 3 essais. Les familles 6 et 7 ne sont pas dans la seed :
 *   une table sur c ou sur (c, p1) tombe au hasard si p2 (resp. p3) est indépendant.
 * Entraînement, curriculum de 4 niveaux (le niveau du monde = le premier où le pool reçu ne dépasse
 * pas 0.9) : L0 {0,1,2} ; L1 + paire, renversements multiples ; L2 + XOR, bandit non stationnaire,
 * récompense affichée bruitée 10 % ; L3 bruit 20 %. Un quart des vies du monde est une mémoire 2 ou 3
 * (tables non réservées). Fitness de sélection = moitié curriculum, moitié mémoire 2 et 3.
 * Affichage (fitness et best) = mémoire 2 et 3 tenues à l'écart (tables réservées, sans bruit).
 * Le mélange des familles déjà écrites dans la seed reste dans extra « known » : il ne sert plus de note.
 * Score d'une vie = justesse des essais 32..63. Budget : 192 instructions / tick (ce labo).
 * Hérédité (ce labo seul) : l'enfant et le pool reçoivent le génome de naissance. Les STORE
 * de la vie restent dans le soma. Les clones de la même lignée ne remplissent pas les 32 places.
 * Niche mémoire : le pool a deux moitiés. Places 0-2 = T[p1], 3-5 = T[p2], 6-7 = T[p3]
 * (tables non réservées) ; places 8-15 = les meilleurs en sélection (curriculum + mémoire 2 et 3).
 * L'échelle ne moyenne plus les trois barreaux : un barreau qui dépasse 0.42 et le barreau d'avant
 * vaut 1 ou 2 de plus que sa justesse. Moyennés, le re-ciblage de la case précédente (mémoire-2
 * 0.51, mémoire-1 tombée à 0.43) perdait contre le registre parfait sur T[p1] seul. La réévaluation
 * appariée note les deux moitiés ; la note affichée et le champion viennent de la niche.
 * Un pool de 8 génomes ou moins est sa propre niche. */
#define GN_TRIALS 64
#define GN_LATE   32
#define GN_HELD_N 40
#define GN_TOP    8
#define GN_REWARD 2
#define GN_LEVELS 4
#define GN_OPS    192
typedef struct { int fam, t1, t2, nsw, sw[3], noise; uint32_t q1, q2; } GnTask;
static int gn_held_code(int code) { return (((unsigned)code * 167u) >> 3 & 3u) == 0; }
static int gn_draw_code(Rng *r, int held, int avoid) {
    for (;;) { int c = (int)rng_below(r, 256); if (gn_held_code(c) == held && c != avoid) return c; }
}
static GnTask gn_draw(Rng *r, int held, int level, int force) {
    static const int HF[5] = { 0, 1, 2, 3, 6 }, TF[GN_LEVELS] = { 3, 4, 5, 5 };   /* familles tirables */
    static const int TFAM[5] = { 0, 1, 2, 4, 5 };
    GnTask t; memset(&t, 0, sizeof t);
    /* force >= 0 : cette famille, avec les codes déjà réservés ou non selon held.
     * Les familles 6 et 7 ne sont pas dans le tirage : on les impose. */
    t.fam = force >= 0 ? force : held ? HF[rng_below(r, 5)] : TFAM[rng_below(r, (uint32_t)TF[level])];
    if (t.fam == 2) {
        int k = (int)rng_below(r, held ? 4 : 3), k2 = (k + 1 + (int)rng_below(r, 3)) & 3;
        if (!held) k2 = (k + 1 + (int)rng_below(r, 2)) % 3;
        t.t1 = k * 0x55; t.t2 = k2 * 0x55;
    } else { t.t1 = gn_draw_code(r, held, -1); t.t2 = gn_draw_code(r, held, t.t1); }
    t.q1 = (uint32_t)rng_next(r); t.q2 = (uint32_t)rng_next(r);
    if (t.fam == 1 || (t.fam == 2 && !held && level >= 2)) {
        if (held || level == 0) { if (t.fam == 1) { t.nsw = 1; t.sw[0] = 24; } }
        else { t.nsw = 1 + (int)rng_below(r, 3); for (int i = 0; i < t.nsw; i++) t.sw[i] = 8 + (int)rng_below(r, 48); }
    }
    if (!held && level == 4 - 1) t.noise = 200; else if (!held && level == 2) t.noise = 100;
    return t;
}
static int gn_target(const GnTask *t, int trial, int c, int p1, int p2, int p3) {
    int ph = 0; for (int i = 0; i < t->nsw; i++) ph += trial >= t->sw[i];
    int tab = (ph & 1) ? t->t2 : t->t1;
    switch (t->fam) {
    case 3: return (tab >> (2 * p1)) & 3;
    case 6: return (tab >> (2 * p2)) & 3;
    case 7: return (tab >> (2 * p3)) & 3;
    case 4: return (int)(((ph & 1) ? t->q2 : t->q1) >> (2 * (c + 4 * p1))) & 3;
    case 5: return (c ^ p1 ^ (int)(t->q1 & 3)) & 3;
    default: return (tab >> (2 * c)) & 3;
    }
}
typedef struct { World *sb; Creature tc; } GnState;
typedef struct { int level; } GnLab;
static GnState *gn_state_new(void) {
    GnState *s = xcalloc(1, sizeof *s);
    WorldConfig sc = cfg_default(); sc.max_pop = 0; sc.food_density = 0; sc.ops_per_tick = GN_OPS;
    sc.regs = GN_REGS;
    Rng *r = xmalloc(sizeof *r); rng_seed(r, 0);
    s->sb = world_new(NULL, 0, r, &sc, NULL);
    return s;
}
static void gn_state_free(GnState *s) { free(s->sb->rng); world_free(s->sb); free(s); }
/* Une vie en bac à sable. mode : 0 normal, 1 génome gelé (STORE sans effet durable),
 * 2 récompense masquée, 3 réponse aléatoire, 4+v réponse constante v. ok[i] += 1 si l'essai i est juste. */
static void gn_life(GnState *s, const PoolEntry *g, const GnTask *t, Rng *cues, int mode, int *ok) {
    Creature *c = &s->tc; World *w = s->sb;
    memset(c, 0, sizeof *c);
    c->len = g->len; memcpy(c->dna, g->dna, (size_t)g->len); c->x = c->y = 2; c->emitted = -1;
    /* p3 initial dérivé : pas un tirage de plus, le flux de cues des familles 0-6 ne bouge pas. */
    int p1 = (int)rng_below(cues, 4), p2 = (int)rng_below(cues, 4), p3 = (p1 + 2 * p2) & 3, r = 0;
    for (int i = 0; i < GN_TRIALS; i++) {
        int cue = (int)rng_below(cues, 4), tgt = gn_target(t, i, cue, p1, p2, p3), resp;
        int flip = t->noise && (int)rng_below(cues, 1000) < t->noise;
        if (mode >= 3) resp = mode == 3 ? (int)rng_below(cues, 4) : mode - 4;
        else {
            w->stimulus = cue | ((mode == 2 ? 0 : r) << 2) | (i == 0) << 3;
            w->hist_n = 0; w->nem = 0; c->e = 1000;
            vm_run(c, w);
            resp = c->emitted;
            if (mode == 1) memcpy(c->dna, g->dna, (size_t)g->len);
        }
        int good = resp >= 0 && (resp & 3) == tgt;
        ok[i] += good;
        r = good ^ flip;
        p3 = p2; p2 = p1; p1 = cue;
    }
}
/* Justesse moyenne (essais 32..63) de n génomes. held : jeu tenu à l'écart (fixe) ; sinon tâches
 * d'entraînement du niveau level tirées de seed. fam >= 0 : une seule famille.
 * curve (si non NULL) : [0] essais 0..15, [1] essais 48..63. */
double gn_eval(const PoolEntry *g, int n, int held, int level, uint64_t seed, int ntask, int mode, int fam, double *curve) {
    GnState *s = gn_state_new();
    int ok[GN_TRIALS] = { 0 }; long lives = 0;
    for (int j = 0; j < ntask; j++) {
        Rng tr; rng_seed(&tr, held ? 0x6E6E0000ULL + (uint64_t)j : seed + (uint64_t)j * 0x9E3779B97F4A7C15ULL);
        int force = (fam == 3 || fam >= 6) ? fam : -1;   /* 3, 6 et 7 : toutes les tâches, pas un sous-échantillon */
        GnTask t = gn_draw(&tr, held, level, force);
        if (force < 0 && fam >= 0 && t.fam != fam) continue;
        for (int i = 0; i < n; i++) { Rng cu = tr; gn_life(s, &g[i], &t, &cu, mode, ok); lives++; }
    }
    gn_state_free(s);
    if (!lives) { if (curve) curve[0] = curve[1] = 0; return 0; }
    double late = 0, a = 0, b = 0;
    for (int i = GN_LATE; i < GN_TRIALS; i++) late += ok[i];
    for (int i = 0; i < 16; i++) { a += ok[i]; b += ok[GN_TRIALS - 16 + i]; }
    if (curve) { curve[0] = a / (16.0 * lives); curve[1] = b / (16.0 * lives); }
    return late / ((double)(GN_TRIALS - GN_LATE) * lives);
}
/* Un génome sauvé, rejoué seul sur le jeu tenu à l'écart. v : [0] mémoire-2, [1] mémoire-3,
 * [2..3] les mêmes gelées, [4..5] sans récompense, [6] familles connues, [7] sélection (graine 0).
 * Retourne 2 ou 3 quand cette mémoire tient seule (≥ 0.95) et retombe d'au moins 0.5 gelée ET
 * sans récompense : la réponse est apprise pendant la vie, pas récitée. 0 sinon. */
double gn_train(const PoolEntry *g, int n, uint64_t seed);
int gn_check(const PoolEntry *g, double *v) {
    for (int k = 0; k < 2; k++)
        for (int m = 0; m < 3; m++) v[2 * m + k] = gn_eval(g, 1, 1, 0, 0, GN_HELD_N, m, 6 + k, NULL);
    v[6] = gn_eval(g, 1, 1, 0, 0, GN_HELD_N, 0, -1, NULL);
    v[7] = gn_train(g, 1, 0);
    for (int k = 0; k < 2; k++)
        if (v[k] >= 0.95 && v[2 + k] <= v[k] - 0.5 && v[4 + k] <= v[k] - 0.5) return 2 + k;
    return 0;
}
/* fitness de sélection : moitié curriculum (4 niveaux, 8 tâches), moitié mémoire 2 et 3
 * (tables non réservées, donc distinctes du jeu tenu à l'écart). */
double gn_train(const PoolEntry *g, int n, uint64_t seed) {
    double s = 0;
    for (int L = 0; L < GN_LEVELS; L++) s += gn_eval(g, n, 0, L, seed + (uint64_t)L * 7919, 8, 0, -1, NULL);
    s /= GN_LEVELS;
    double lag = 0.5 * (gn_eval(g, n, 0, 0, seed ^ 0x6A6C0001ULL, 8, 0, 6, NULL)
                      + gn_eval(g, n, 0, 0, seed ^ 0x6A6C0002ULL, 8, 0, 7, NULL));
    return 0.5 * s + 0.5 * lag;
}
/* Un barreau compte à part entière seulement s'il dépasse 0.42 (au-dessus du hasard et des
 * génomes sans second registre, mémoire-2 ≤ 0.39 sur 40 tâches) et le barreau précédent.
 * Sinon il ne sert qu'à départager. Moyenne des échelles, pas échelle de la moyenne :
 * un génome à mémoire-2 dans un pool qui sait encore T[p1] doit rester visible. */
static double gn_scale(double p1, double p2, double p3) {
    if (p3 >= 0.42 && p3 > p2 && p3 > p1) return 2.0 + p3;
    if (p2 >= 0.42 && p2 > p1) return 1.0 + p2;
    return p1;
}
double gn_mem(const PoolEntry *g, int n, uint64_t seed) {
    if (n <= 0) return 0;
    double s = 0;
    for (int i = 0; i < n; i++) {
        double p1 = gn_eval(g + i, 1, 0, 0, seed ^ 0x6A6C0003ULL, 8, 0, 3, NULL);
        double p2 = gn_eval(g + i, 1, 0, 0, seed ^ 0x6A6C0004ULL, 8, 0, 6, NULL);
        double p3 = gn_eval(g + i, 1, 0, 0, seed ^ 0x6A6C0005ULL, 8, 0, 7, NULL);
        s += gn_scale(p1, p2, p3);
    }
    return s / n;
}
/* Descripteurs de comportement (experiments/novelty-map-elites, hors sélection historique).
 * 8 vies sur des tâches d'entraînement fixes (4 tirages du niveau 1 + mémoire 2 et 3 ×2), mode normal.
 * d[0] entropie de la réponse (5 symboles : 0..3, silence ; bits)   d[1] fraction d'essais émis
 * d[2..5] information mutuelle I(réponse ; indice il y a L essais), L = 0..3 (bits, essais 3..63)
 * d[6] STORE effectifs / essai   d[7] octets du soma ≠ génome en fin de vie (moyenne)
 * d[8] codons réécrits puis exécutés / essai. Aucune note de tâche n'entre ici. */
static double gn_mi(const int (*j)[4], int n) {
    if (!n) return 0;
    double pr[5] = { 0 }, pc[4] = { 0 }, mi = 0;
    for (int r = 0; r < 5; r++) for (int c = 0; c < 4; c++) { pr[r] += j[r][c]; pc[c] += j[r][c]; }
    for (int r = 0; r < 5; r++) for (int c = 0; c < 4; c++)
        if (j[r][c]) mi += (double)j[r][c] / n * log2((double)j[r][c] * n / (pr[r] * pc[c]));
    return mi;
}
void gn_behavior(const PoolEntry *g, double *d) {
    GnState *s = gn_state_new();
    Creature *c = &s->tc; World *w = s->sb;
    int hist[5] = { 0 }, joint[4][5][4], nj = 0, nt = 0;
    long stores = 0, useful = 0, drift = 0;
    memset(joint, 0, sizeof joint);
    for (int j = 0; j < 8; j++) {
        Rng tr; rng_seed(&tr, 0xBE4A0000ULL + (uint64_t)j);
        GnTask t = gn_draw(&tr, 0, 1, j < 4 ? -1 : (j & 1) ? 7 : 6);
        Rng cues = tr;
        memset(c, 0, sizeof *c);
        c->len = g->len; memcpy(c->dna, g->dna, (size_t)g->len); c->x = c->y = 2; c->emitted = -1;
        int p[3]; p[0] = (int)rng_below(&cues, 4); p[1] = (int)rng_below(&cues, 4); p[2] = (p[0] + 2 * p[1]) & 3;
        int r = 0;
        for (int i = 0; i < GN_TRIALS; i++) {
            int cue = (int)rng_below(&cues, 4), tgt = gn_target(&t, i, cue, p[0], p[1], p[2]);
            int flip = t.noise && (int)rng_below(&cues, 1000) < t.noise;
            w->stimulus = cue | r << 2 | (i == 0) << 3;
            w->hist_n = 0; w->nem = 0; c->e = 1000;
            vm_run(c, w);
            int resp = c->emitted, sym = resp >= 0 ? (resp & 3) : 4;
            hist[sym]++; nt++;
            if (i >= 3) {
                int lag[4] = { cue, p[0], p[1], p[2] };
                for (int L = 0; L < 4; L++) joint[L][sym][lag[L]]++;
                nj++;
            }
            int good = resp >= 0 && (resp & 3) == tgt;
            r = good ^ flip;
            p[2] = p[1]; p[1] = p[0]; p[0] = cue;
        }
        stores += c->stores; useful += c->useful_stores;
        for (int k = 0; k < g->len; k++) drift += c->dna[k] != g->dna[k];
    }
    gn_state_free(s);
    double h = 0;
    for (int k = 0; k < 5; k++) if (hist[k]) { double q = (double)hist[k] / nt; h -= q * log2(q); }
    d[0] = h; d[1] = 1.0 - (double)hist[4] / nt;
    for (int L = 0; L < 4; L++) d[2 + L] = gn_mi((const int (*)[4])joint[L], nj);
    d[6] = (double)stores / nt; d[7] = drift / 8.0; d[8] = (double)useful / nt;
}
/* Dans le monde : épisodes de 64 essais (1 par tick) sur des tâches d'entraînement au niveau du
 * curriculum ; un essai juste rapporte de l'énergie. lscore = justesse des essais 32..63, rétrécie.
 * ls[0] = graine de la tâche, ls[1] = essai, ls[2] = c | p1<<2 | p2<<4, ls[3]/ls[4] = justes/essais tardifs,
 * ls[5] = cible | bruit<<2 (bit de récompense inversé pour cet essai). */
static GnTask gn_task_of(int seed, int level) {
    Rng r; rng_seed(&r, (uint64_t)seed);
    /* Un quart des vies : mémoire 2 ou 3, codes non réservés. Le curriculum reste le reste. */
    int force = ((seed & 6) == 6) ? ((seed & 8) ? 7 : 6) : -1;
    return gn_draw(&r, 0, level, force);
}
static void gn_before(World *w, Creature *c) {
    int level = ((GnLab *)w->lab->ext)->level, r = 0;
    if (!c->ls[0]) { c->ls[0] = (int)(rng_next(w->rng) & 0x7fffffff) | 1; c->ls[1] = 0; c->ls[2] = (int)rng_below(w->rng, 16) << 2; }
    else {
        int good = c->emitted >= 0 && (c->emitted & 3) == (c->ls[5] & 3);
        r = good ^ (c->ls[5] >> 2);
        if (good) { c->e += GN_REWARD; cap_e(c); }
        if (c->ls[1] >= GN_LATE) { c->ls[3] += good; c->ls[4]++; c->lscore = c->ls[3] / (c->ls[4] + 8.0); }
        if (++c->ls[1] == GN_TRIALS) { c->ls[0] = (int)(rng_next(w->rng) & 0x7fffffff) | 1; c->ls[1] = 0; }
    }
    GnTask t = gn_task_of(c->ls[0], level);
    int cue = (int)rng_below(w->rng, 4), p1 = c->ls[2] & 3, p2 = (c->ls[2] >> 2) & 3, p3 = (c->ls[2] >> 4) & 3;
    int flip = t.noise && (int)rng_below(w->rng, 1000) < t.noise;
    c->ls[5] = gn_target(&t, c->ls[1], cue, p1, p2, p3) | flip << 2;
    w->stimulus = cue | r << 2 | (c->ls[1] == 0) << 3;     /* bit 3 : premier essai d'une tâche */
    c->ls[2] = cue | p1 << 2 | p2 << 4 | p3 << 6;          /* p3 du prochain essai = p2 de celui-ci */
}
void hl_gn_cfg(WorldConfig *c) {
    c->food_density = 0.15; c->food_regrow = 2; c->ops_per_tick = GN_OPS;
    c->mut_rate = 0.005; c->dup_rate = 0.01; c->germline = 1;   /* génome long : mutation réduite ; lignée ≠ soma */
    c->regs = GN_REGS;      /* levier : RPUSH / RPEEK (experiments/general-regs) */
#if defined(GN_DIRECT_SEL) || defined(GN_BIRTH_OFF)
    c->delay_rate = 0.0; c->unmix_rate = 0.0; c->deepen_rate = 0.0;  /* pression seule, sans échafaudage */
#else
    c->delay_rate = 0.12;   /* décaler la case, puis relire la case d'avant. 0 chez les autres labos */
    c->unmix_rate = 0.12;   /* puis ne plus ajouter l'indice courant à la case relue */
    c->deepen_rate = 0.12;  /* puis recopier cette case dans un intron froid : la clé lit p3 */
#endif
}
int hl_gn_seeds(PoolEntry *o) { for (int i = 0; i < 4; i++) seed_entry(&o[i], "general", 0); return 4; }
/* curriculum : le niveau monte tant que le pool reçu sature (> 0.9) le niveau courant */
/* Les deux moitiés du pool : niche mémoire = places 0..7, curriculum = places 8..15.
 * Un pool de 8 génomes ou moins sert aux deux (la seed, un pool historique). */
static void gn_parts(int n, int *nb, const PoolEntry **a, const PoolEntry *g, int *na) {
    *nb = n < GN_TOP ? n : GN_TOP;
    if (n > GN_TOP) { *a = g + GN_TOP; *na = n - GN_TOP < GN_TOP ? n - GN_TOP : GN_TOP; }
    else { *a = g; *na = *nb; }
}
int gn_level_of(const PoolEntry *g, int n) {
    const PoolEntry *a; int nb, na;
    gn_parts(n, &nb, &a, g, &na);
    if (na > 4) na = 4;
    int L = 0;
    while (L < GN_LEVELS - 1 && gn_eval(a, na, 0, L, 0xC0C0 + (uint64_t)L, 8, 0, -1, NULL) > 0.9) L++;
    return L;
}
void hl_gn_init(Lab *l, World *w) {
    GnLab *s = xcalloc(1, sizeof *s);
    s->level = l->pool0 ? gn_level_of(l->pool0, l->npool0) : 0;
    l->ext = s; w->before_vm = gn_before;
    if (l->eval_only) {                                 /* réévaluation : la fitness ne lit que le pool, pas de monde à simuler */
        for (int i = 0; i < w->ncs; i++) free(w->cs[i]);
        w->ncs = 0;
    }
}
void hl_gn_pre(Lab *l, World *w) { (void)l; w->hist_n = 0; }
/* Graine des tâches d'entraînement d'un cycle : dérivée de l'état initial du RNG, donc la même pour le
 * nouveau pool et le pool en place dans la réévaluation appariée du runner. */
static uint64_t gn_cycle_seed(const Lab *l) { Rng t = l->rng0; return rng_next(&t); }
/* Fitness = le POOL lui-même (pas ses descendants après 200 ticks) : moitié échelle mémoire de la
 * niche, moitié sélection du curriculum, en bac à sable. Le monde ne sert qu'à produire les candidats. */
double hl_gn_fit(Lab *l, World *w, int ticks) {
    (void)ticks; (void)w;
#ifdef GN_DIRECT_SEL
    /* Piste A : fitness = justesse held-out de la mémoire cible. */
    const PoolEntry *pool = l->pool0; int n = l->npool0;
    memset(l->extra, 0, sizeof l->extra);
    if (n <= 0) return 0;
    double m2 = gn_eval(pool, n, 1, 0, 0, GN_HELD_N, 0, 6, NULL);
    double m3 = gn_eval(pool, n, 1, 0, 0, GN_HELD_N, 0, 7, NULL);
    double direct = gn_eval(pool, n, 1, 0, 0, GN_HELD_N, 0, GN_DIRECT_FAM, NULL);
    l->extra[0] = 0.5 * (m2 + m3);
    l->extra[1] = direct;
    l->extra[2] = 0.5 * (gn_eval(pool, n, 1, 0, 0, GN_HELD_N, 1, 6, NULL)
                       + gn_eval(pool, n, 1, 0, 0, GN_HELD_N, 1, 7, NULL));
    l->extra[3] = l->extra[0] - l->extra[2];
    l->extra[4] = gn_eval(pool, n, 1, 0, 0, GN_HELD_N, 0, -1, NULL);
    l->extra[5] = ((GnLab *)l->ext)->level;
    return direct;
#else
    const PoolEntry *a, *b = l->pool0; int nb, na;
    gn_parts(l->npool0, &nb, &a, l->pool0, &na);
    double tr = 0;
    memset(l->extra, 0, sizeof l->extra);
    if (nb > 0) {
        uint64_t sd = gn_cycle_seed(l);
        tr = 0.5 * gn_mem(b, nb, sd) + 0.5 * gn_train(a, na, sd);
        l->extra[1] = tr;
        if (l->eval_only) return tr;                    /* réévaluation appariée : pas besoin du held-out */
        double l6 = gn_eval(b, nb, 1, 0, 0, GN_HELD_N, 0, 6, NULL);
        double l7 = gn_eval(b, nb, 1, 0, 0, GN_HELD_N, 0, 7, NULL);
        l->extra[0] = 0.5 * (l6 + l7);                  /* note affichée : la niche, sur des mémoires jamais écrites dans la seed */
        l->extra[2] = 0.5 * (gn_eval(b, nb, 1, 0, 0, GN_HELD_N, 1, 6, NULL)
                           + gn_eval(b, nb, 1, 0, 0, GN_HELD_N, 1, 7, NULL));
        l->extra[4] = gn_eval(a, na, 1, 0, 0, GN_HELD_N, 0, -1, NULL);   /* familles que la seed sait déjà */
    }
    l->extra[1] = tr; l->extra[3] = l->extra[0] - l->extra[2];
    l->extra[5] = ((GnLab *)l->ext)->level;
    return tr;                                          /* la sélection ne voit que l'entraînement */
#endif
}
/* Copie héritée : la lignée si ce labo l'a armée, sinon le soma (les autres labos ne passent pas ici). */
static void gn_inherit(PoolEntry *e, const Creature *c, int germline) {
    int use = germline && c->germ_len > 0;
    e->species = 0;
    e->len = use ? c->germ_len : c->len;
    memcpy(e->dna, use ? c->germ : c->dna, (size_t)e->len);
}
static int gn_same(const PoolEntry *x, const PoolEntry *y) { return x->len == y->len && !memcmp(x->dna, y->dna, (size_t)x->len); }
/* Prend jusqu'à k génomes distincts de cand (au meilleur score s), absents de out[0..*m). */
static void gn_take(const PoolEntry *cand, double *s, int m, int k, PoolEntry *out, int *n) {
    for (int got = 0; got < k; ) {
        int b = -1;
        for (int i = 0; i < m; i++) if (s[i] > -1 && (b < 0 || s[i] > s[b])) b = i;
        if (b < 0) return;
        s[b] = -2;
        int dup = 0;
        for (int j = 0; j < *n; j++) if (gn_same(&out[j], &cand[b])) { dup = 1; break; }
        if (!dup) { out[(*n)++] = cand[b]; got++; }
    }
}
#define GN_CAND 96
/* Nouveau pool : toutes les lignées distinctes du monde (les clones du même génome de naissance ne
 * prennent qu'une place, au plus 96, rangées par lscore), plus le pool reçu, notées UNE PAR UNE en
 * bac à sable. Places 0-7 : échelle mémoire ; places 8-15 : sélection du curriculum. */
int hl_gn_sel(Lab *l, World *w, PoolEntry *out) {
    if (l->eval_only) return 0;                         /* réévaluation : seule la fitness du pool compte */
    Creature **v = xmalloc(sizeof(Creature *) * CMAX); int n = 0;
    for (int i = 0; i < w->ncs; i++) if (w->cs[i]->species == 0) v[n++] = w->cs[i];
    for (int i = 1; i < n; i++) {
        Creature *c = v[i]; int j = i - 1;
        while (j >= 0 && (v[j]->lscore < c->lscore || (v[j]->lscore == c->lscore && v[j]->e < c->e))) { v[j + 1] = v[j]; j--; }
        v[j + 1] = c;
    }
    int m = 0;
    PoolEntry *cand = xmalloc(sizeof(PoolEntry) * (GN_CAND + POOL_MAX));
    double *sm = xmalloc(sizeof(double) * (GN_CAND + POOL_MAX)), *st = xmalloc(sizeof(double) * (GN_CAND + POOL_MAX));
    for (int i = 0; i < n && m < GN_CAND; i++) {
        PoolEntry e; gn_inherit(&e, v[i], w->cfg.germline);
        int dup = 0;
        for (int j = 0; j < m; j++) if (gn_same(&cand[j], &e)) { dup = 1; break; }
        if (!dup) cand[m++] = e;
    }
    for (int i = 0; i < l->npool0 && i < POOL_MAX; i++) cand[m++] = l->pool0[i];
#ifdef GN_DIRECT_SEL
    (void)sm; (void)st;
    double *sd = xmalloc(sizeof(double) * (size_t)(GN_CAND + POOL_MAX));
    double *so = xmalloc(sizeof(double) * (size_t)(GN_CAND + POOL_MAX));
    int other = (GN_DIRECT_FAM == 6) ? 7 : 6;
    for (int i = 0; i < m; i++) {
        sd[i] = gn_eval(&cand[i], 1, 1, 0, 0, GN_HELD_N, 0, GN_DIRECT_FAM, NULL);
        so[i] = gn_eval(&cand[i], 1, 1, 0, 0, GN_HELD_N, 0, other, NULL);
    }
    int k = 0;
    gn_take(cand, sd, m, 10, out, &k);                /* priorité mémoire cible held-out */
    gn_take(cand, so, m, POOL_SIZE - k, out, &k);     /* complément : l'autre mémoire */
    free(cand); free(sm); free(st); free(sd); free(so); free(v);
    return k;
#else
    uint64_t seed = gn_cycle_seed(l) ^ 0x5E1EC7ULL;
    double *s1 = sm, *s2 = xmalloc(sizeof(double) * (size_t)(GN_CAND + POOL_MAX));
    double *s3 = xmalloc(sizeof(double) * (size_t)(GN_CAND + POOL_MAX));
    for (int i = 0; i < m; i++) {
        s1[i] = gn_eval(&cand[i], 1, 0, 0, seed ^ 0x6A6C0003ULL, 8, 0, 3, NULL);
        s2[i] = gn_eval(&cand[i], 1, 0, 0, seed ^ 0x6A6C0004ULL, 8, 0, 6, NULL);
        s3[i] = gn_eval(&cand[i], 1, 0, 0, seed ^ 0x6A6C0005ULL, 8, 0, 7, NULL);
        st[i] = gn_train(&cand[i], 1, seed);
    }
    int k = 0;
    gn_take(cand, s1, m, 3, out, &k);                 /* T[p1] */
    gn_take(cand, s2, m, 3, out, &k);                 /* T[p2], même si T[p1] est tombé */
    gn_take(cand, s3, m, 2, out, &k);                 /* T[p3] */
    gn_take(cand, st, m, POOL_SIZE - k, out, &k);     /* le reste : curriculum */
    free(cand); free(sm); free(st); free(s2); free(s3); free(v);
    return k;
#endif
}

/* ======================= dialog-mem : un dialogue qui exige une mémoire =======================
 * Un symbole s_t (0..15) neuf à chaque tick ; la bonne réponse est (s_{t-1} + 1) mod 16 :
 * elle dépend du message d'avant, que le monde ne redonne pas (historique vidé, SENSE_HIST = 0).
 * Comme s_{t-1} est uniforme et indépendant de s_t, toute politique sans mémoire a la même
 * espérance : 1/16 de réponses exactes et 0,2875 de crédit, quelle qu'elle soit. Seul un état
 * interne (registres A/B qui survivent au tick, ou un STORE dans le génome) dépasse ce plafond.
 * Crédit gradué sur les 4 bits bas (distance de Hamming 0/1/2/3/4 → 1/0,55/0,2/0,05/0).
 * Seed-amorce = la seed d'emit-dialog (répond s_t + 1 au message courant) : au plafond sans
 * mémoire, aucune mémoire écrite. Pour passer, la lignée doit répondre au message d'AVANT : garder
 * s_{t-1} + 1 dans A d'un tick à l'autre et l'émettre avant de relire le stimulus.
 * Baseline : même pool, même graine, cible calculée avec un faux s_{t-1} tiré au
 * hasard ; un génome qui se souvient y retombe au plafond sans mémoire, un génome sans mémoire non. */
typedef struct { int prev, fake, s; double credit, emits, exact; } DmState;
static double pop_ticks_h(const Lab *l) { long n = 0; for (int i = 0; i < l->npops; i++) n += l->pops[i]; return n > 0 ? (double)n : 1.0; }
int hl_dm_seeds(PoolEntry *o) { for (int i = 0; i < 4; i++) seed_entry(&o[i], "dialog", 0); return 4; }
void hl_dm_init(Lab *l, World *w) { DmState *s = xcalloc(1, sizeof *s); s->prev = (int)rng_below(w->rng, 16); l->ext = s; }
void hl_dm_rank(const Creature *c, double k[2]) { k[0] = c->age > 0 ? c->lscore / c->age : 0; k[1] = c->e; }
void hl_dm_pre(Lab *l, World *w) {
    DmState *s = l->ext;
    if (w->t > 0) s->prev = s->s;
    s->s = (int)rng_below(w->rng, 16);
    s->fake = (int)rng_below(w->rng, 16);                /* tiré dans les deux modes : rejeu apparié */
    w->stimulus = s->s;
    w->hist_n = 0;
}
void hl_dm_post(Lab *l, World *w) {
    DmState *s = l->ext;
    int pv = l->counterfactual ? s->fake : s->prev;
    int want = (pv + 1) & 15;
    for (int i = 0; i < w->nem; i++) {
        Creature *a = w->em[i].c;
        if (!a || a->species != 0) continue;
        int b = (w->em[i].v ^ want) & 15;
        b = (b & 1) + ((b >> 1) & 1) + ((b >> 2) & 1) + ((b >> 3) & 1);
        double c = b == 0 ? 1.0 : b == 1 ? 0.55 : b == 2 ? 0.20 : b == 3 ? 0.05 : 0.0;
        a->lscore += c;
        a->e += (b == 0 ? 4 : b == 1 ? 2 : b == 2 ? 1 : 0) - 1;
        cap_e(a);
        s->credit += c; s->emits += 1; s->exact += b == 0;
    }
}
double hl_dm_fit(Lab *l, World *w, int ticks) {
    (void)w;
    DmState *s = l->ext;
    double f = s->credit / pop_ticks_h(l);
    l->extra[0] = s->emits > 0 ? s->credit / s->emits : 0;
    l->extra[1] = s->emits > 0 ? s->exact / s->emits : 0;
    l->extra[2] = s->emits;
    if (!l->counterfactual) {
        double b = replay(l, ticks, NULL);
        l->extra[3] = b;
        l->extra[4] = f - b;
    }
    return f;
}

/* ======================= dialog-lag2 : deux cases mémoire, question aléatoire q_t =======================
 * Même dialogue que dialog-mem (un symbole s_t neuf par tick, historique vidé), mais chaque tick
 * pose une question q_t ∈ {1, 2} tirée au hasard : la bonne réponse est (s_{t-q} + 1) mod 16 et
 * la réponse à l'autre message est pénalisée :
 *   crédit = c(réponse, s_{t-q}+1) − c(réponse, s_{t-q'}+1), q' = 3 − q, c = crédit gradué Hamming.
 * La question est annoncée un tick d'avance, dans le bit 0x20 du stimulus (stimulus_t = s_t | 0x20
 * si q_{t+1} = 2) : elle arrive avec s_{t-1}, au moment où la lignée lit, comme le reste du labo
 * (réponse émise avant de relire). 0x20 et pas 0x10 : INC_A sur s = 15 déborde dans 0x10.
 * Plafonds : sans mémoire 0 (0,2875 − 0,2875) ; mémoire d'un tick constante 0 ((0,7125 − 0,7125)/2) ;
 * une case, avec ou sans phase de l'ip, ne tient qu'une des deux valeurs quand la question tombe
 * (la phase est choisie, q_t ne l'est pas) : au mieux (0,7125 + 0,2875) / 2 = 0,5 par émission.
 * Deux cases (A et B, un octet du génome en relais) : optimum 1 − 0,2875 = 0,7125, sur q = 1 et q = 2.
 * PR #16 avait q ≡ 2 : l'évolution y a trouvé 1 case + phase (plafond 0,5) ; q_t aléatoire ferme ce trou
 * de phase (gardé à 0,5 par construction, la seule montée au-dessus passe par deux cases).
 * Taxe de silence : une créature-tick sans EMIT coûte 0,1 (sinon se taire égale les plafonds bas).
 * Baseline : même pool, même graine, s_{t-q} remplacé par un faux tiré au hasard (pénalité inchangée). */
#define D2_SILENCE 0.1
#define D2_QBIT    0x20
typedef struct { int prev, prev2, fake, s, q, qn; double credit, emits, ex[3], n[3]; } D2State;
static double d2_c(int v, int want) {
    int b = (v ^ want) & 15;
    b = (b & 1) + ((b >> 1) & 1) + ((b >> 2) & 1) + ((b >> 3) & 1);
    return b == 0 ? 1.0 : b == 1 ? 0.55 : b == 2 ? 0.20 : b == 3 ? 0.05 : 0.0;
}
void hl_d2_init(Lab *l, World *w) {
    D2State *s = xcalloc(1, sizeof *s);
    s->prev = (int)rng_below(w->rng, 16); s->prev2 = (int)rng_below(w->rng, 16);
    s->qn = 1 + (int)rng_below(w->rng, 2);               /* question du tick 0 */
    l->ext = s;
}
void hl_d2_pre(Lab *l, World *w) {
    D2State *s = l->ext;
    if (w->t > 0) { s->prev2 = s->prev; s->prev = s->s; }
    s->q = s->qn;                                        /* annoncée au tick d'avant */
    s->s = (int)rng_below(w->rng, 16);
    s->fake = (int)rng_below(w->rng, 16);                /* tiré dans les deux modes : rejeu apparié */
    s->qn = 1 + (int)rng_below(w->rng, 2);               /* question du tick suivant, annoncée maintenant */
    w->stimulus = s->s | (s->qn == 2 ? D2_QBIT : 0);
    w->hist_n = 0;
}
void hl_d2_post(Lab *l, World *w) {
    D2State *s = l->ext;
    int asked = s->q == 2 ? s->prev2 : s->prev, other = s->q == 2 ? s->prev : s->prev2;
    int want = ((l->counterfactual ? s->fake : asked) + 1) & 15, wrong = (other + 1) & 15;
    for (int i = 0; i < w->nem; i++) {
        Creature *a = w->em[i].c;
        if (!a || a->species != 0) continue;
        int v = w->em[i].v & 15;
        double cw = d2_c(v, want), co = d2_c(v, wrong), c = cw - co;
        a->lscore += c;
        int g = cw == 1.0 ? 4 : cw == 0.55 ? 2 : cw == 0.20 ? 1 : 0;
        a->e += g - 1 - (v == wrong && v != want ? 2 : 0);   /* répondre à l'autre message coûte */
        cap_e(a);
        s->credit += c; s->emits += 1;
        s->ex[s->q] += v == want && v != wrong; s->n[s->q] += 1;
    }
    for (int i = 0; i < w->ncs; i++) {                    /* taxe de silence : se taire ne vaut pas 0 */
        Creature *c = w->cs[i];
        if (c->species == 0 && c->emitted < 0) { c->lscore -= D2_SILENCE; s->credit -= D2_SILENCE; }
    }
}
double hl_d2_fit(Lab *l, World *w, int ticks) {
    (void)w;
    D2State *s = l->ext;
    double f = s->credit / pop_ticks_h(l);
    l->extra[0] = s->emits > 0 ? s->credit / s->emits : 0;
    l->extra[1] = s->n[1] > 0 ? s->ex[1] / s->n[1] : 0;
    l->extra[2] = s->n[2] > 0 ? s->ex[2] / s->n[2] : 0;
    l->extra[3] = s->emits;
    if (!l->counterfactual) {
        double b = replay(l, ticks, NULL);
        l->extra[4] = b;
        l->extra[5] = f - b;
    }
    return f;
}
