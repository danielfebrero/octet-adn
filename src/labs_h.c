/* Labos « humanité » : versions jouets de problèmes réels. Chacun garde le même moteur ;
 * les mécanismes propres passent par les hooks de labo (init, pre, post, before_vm), jamais
 * par un changement de comportement des 10 labos historiques. Ce sont des terrains
 * d'entraînement, pas des percées : un gagnant est un algorithme à étudier. */
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "octet.h"

static int clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }
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
