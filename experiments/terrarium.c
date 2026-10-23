/* Terrarium : un monde ouvert, peuplé, qui tourne longtemps — sans fitness de labo.
 *
 * Écologie libre : nourriture qui repousse, énergie, SPLIT normal (cœur inchangé : world_step + hook
 * before_vm). Pour dépasser le plafond d'un World (64×32, 1024 créatures), le terrarium est un archipel
 * de K îles (un World chacune, un thread chacune) reliées en anneau par une migration lente.
 *
 * Canal social : à chaque tick, chaque créature reçoit dans le quartet bas du stimulus la valeur EMIT
 * (& 15) d'UN voisin (3×3) qui a émis au tick précédent, tiré au hasard ; le quartet haut porte un
 * environnement de Markov (change avec p = 1/50). Sans voisin émetteur, le quartet bas = l'environnement.
 * --channel off : rien n'est livré (témoin).
 *
 * Mesures (une ligne de series.tsv par échantillon, voir README) :
 *  - population, génotypes distincts (hash fonctionnel : opcodes valides + imm, à leur position), entropie,
 *  - nouveauté cumulée façon Bedau-Packard, à deux niveaux :
 *    · génotypes nouveaux ayant PERSISTÉ ≥ P échantillons consécutifs avec ≥ CLONES porteurs, contre une
 *      ombre neutre (mêmes naissances/morts/taux d'innovation, sans sélection) ;
 *    · composants : 5-grammes d'opcodes valides (sans position ni immédiats) portés par ≥ 1 % de la pop
 *      pendant P échantillons (comp_cum) — le niveau génotype est trop fin (≈ un génotype par individu),
 *  - longueur et octets codants des génomes,
 *  - MI sociale : L(a|a₋₁) − L(a|a₋₁, sig) où sig = l'émission LIVRÉE, contre le témoin « voisin non livré »
 *    (un autre voisin émetteur au même tick, même environnement local) ; excès = réel − témoin (bits/paire),
 *  - spécialisation : niches comportementales (5 fractions binées) qui coexistent de façon stable,
 *  - cycles : autocorrélation de la population et des niches.
 *  Événements (events.tsv) : signal social significatif, nouvelle niche stable, cycle, extinction.
 *
 *   ./experiments/terrarium_run run --dir D [options]   (reprend tout seul depuis D/state.ck)
 *   ./experiments/terrarium_run report D [D...]          (résumé des séries)
 *   ./experiments/terrarium_run selftest */
#define _GNU_SOURCE
#include <math.h>
#include <pthread.h>
#include <signal.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include "octet.h"

void world_eat(World *w, Creature *c);

#define KMAX    16
#define PERSIST 10          /* échantillons consécutifs pour « persister » */
#define COMPK   5           /* longueur des composants (k-grammes d'opcodes) : 3 sature l'espace (~30k), 5 ≈ 28M */
#define CLONES  3           /* … et au moins CLONES porteurs simultanés à un moment (pas un seul individu qui vit vieux) */
#define RING    96
#define NNICHE  243         /* 3^5 */
#define SOC_CAP 20000       /* paires codées max par île et par fenêtre */
#define ASYM    (17 * 8)    /* action : (EMIT&15 | rien) × (bougé, tourné, divisé) — sans « a mangé » */
#define TBITS   19
#define TMASK   ((1u << TBITS) - 1)
static const int TDX[4] = { 0, 1, 0, -1 }, TDY[4] = { -1, 0, 1, 0 };   /* = world.c */

typedef struct {               /* paramètres (sauvés dans le checkpoint) */
    int K, w, h, max_pop, regrow, sample, ck_every, migrate, channel, founders, pop0;
    double mut, dup, season_amp, soc_thr; int season_period;
    uint64_t seed;
    /* experiments/soup-terrarium (tout à 0 = terrarium d'origine, mêmes tirages) */
    int repro;                  /* 0 = SPLIT du cœur ; 1 = copie réelle (soup_exec sur parent‖bruit, SPLIT coupé) */
    int rep_steps, rep_cost, rep_energy; double rep_p, rep_mut;
    int pred, pred_dmg, pred_every;   /* prédateurs-environnement : le canal social peut payer */
    int osmo;                   /* pont métabolique : absorbe ≤ osmo unités de nourriture de sa case par tick (sans EAT) */
    char ffile[256];            /* --founders soup:FICHIER */
    /* OADNTER4 : 0 = terrarium d'origine (mêmes tirages). Un checkpoint OADNTER3 se relit, ces champs valent 0. */
    int forces;                 /* 1 = espèce = (germen contient l'opcode ATTACK) ; 0 = héritage */
    int expand_every;           /* 0 = K fixe ; N = +1 île vide tous les N ticks (multiple de --sample), jusqu'à KMAX */
} Params;
enum { F_SEEDS, F_RANDOM, F_SOUP, F_HANDMADE };

/* ---- codeur adaptatif hiérarchique (Dirichlet, comptes hachés) ---- */
typedef struct { uint64_t key; uint32_t n; } Slot;
typedef struct { Slot *t; uint32_t *used; int nused; } Tab;
static uint32_t *tab_get(Tab *T, uint64_t key) {
    key |= 1ULL << 63;
    for (uint32_t i = (uint32_t)((key * 0x9E3779B97F4A7C15ULL) >> (64 - TBITS));; i = (i + 1) & TMASK) {
        if (T->t[i].key == key) return &T->t[i].n;
        if (!T->t[i].key) { T->t[i].key = key; T->t[i].n = 0; T->used[T->nused++] = i; return &T->t[i].n; }
    }
}
static void tab_clear(Tab *T) { for (int i = 0; i < T->nused; i++) T->t[T->used[i]].key = 0; T->nused = 0; }
#define KEY(lv, ctx, sym) (((uint64_t)(lv) << 56) | ((uint64_t)(ctx) << 12) | (uint64_t)(sym))
#define CONC 1.0
typedef struct { double L0, Lr, Ln; long pairs; } Soc;
/* une paire : action a, action précédente p, signal livré s, signal non livré n (0..15) */
static void soc_code(Tab *T, Soc *S, int a, int p, int s, int n) {
    uint32_t *n0 = tab_get(T, KEY(0, 0, a)), *N0 = tab_get(T, KEY(0, 0, 4095));
    double P0 = (*n0 + 0.5) / (*N0 + 0.5 * ASYM);
    uint32_t *n1 = tab_get(T, KEY(1, p, a)), *N1 = tab_get(T, KEY(1, p, 4095));
    double P1 = (*n1 + CONC * P0) / (*N1 + CONC);
    uint32_t *nr = tab_get(T, KEY(2, p * 16 + s, a)), *Nr = tab_get(T, KEY(2, p * 16 + s, 4095));
    uint32_t *nn = tab_get(T, KEY(3, p * 16 + n, a)), *Nn = tab_get(T, KEY(3, p * 16 + n, 4095));
    double Pr = (*nr + CONC * P1) / (*Nr + CONC), Pn = (*nn + CONC * P1) / (*Nn + CONC);
    S->L0 -= log2(P1); S->Lr -= log2(Pr); S->Ln -= log2(Pn); S->pairs++;
    (*n0)++; (*N0)++; (*n1)++; (*N1)++; (*nr)++; (*Nr)++; (*nn)++; (*Nn)++;
}

/* ---- île ---- */
typedef struct {
    World *w; Rng wr, tr; int env, idx;
    Tab tab; Soc soc;
    long births, deaths, novel, emits0;
    int em_n[GRID_H][GRID_W]; uint8_t em_v[GRID_H][GRID_W][4]; int em_id[GRID_H][GRID_W][4];
    int snap_id[CMAX]; uint64_t snap_fh[CMAX]; int nsnap;
    uint64_t *sh; int nsh; uint64_t sh_next;         /* ombre neutre : une étiquette par individu */
    /* ombre neutre au niveau composant : un génome par individu, mêmes naissances/morts, même mutation, sans sélection */
    struct SG { int species, len; uint8_t dna[MAX_LEN]; } *sg; int nsg; Rng sr;
    Rng rr;                                           /* copie réelle + prédateurs (jamais tiré dans le mode d'origine) */
    int npred, px[16], py[16];
    long rep_try, rep_ok, pred_hits;
    const Params *P;
    long t_end;
} Island;

static __thread Island *cur;

/* un opcode ATTACK dans le germen (pas un immédiat d'un autre op) : la force naît de la seed */
static int tape_has_attack(const uint8_t *g, int len) {
    for (int i = 0; i < len; i++) {
        uint8_t op = g[i];
        if (op == OP_ATTACK) return 1;
        if (op_has_imm(op) && i + 1 < len) i++;
    }
    return 0;
}

/* champs de travail de la créature (labo-libres : win_len = 0, la VM ne les lit pas) */
/* accès par memcpy : win/win0 ne sont pas alignés sur 8 */
static uint64_t FH(const Creature *c) { uint64_t v; memcpy(&v, &c->win[16], 8); return v; }
static uint64_t RAWH(const Creature *c) { uint64_t v; memcpy(&v, &c->win0[0], 8); return v; }
static uint16_t COD(const Creature *c) { uint16_t v; memcpy(&v, &c->win[24], 2); return v; }
/* compteurs de comportement : obs, bouge, mange, tourne, divise, émet (6 × uint16 dans win[2..13]) */
static void cnt_get(const Creature *c, uint16_t q[6]) { memcpy(q, &c->win[2], 12); }
static void cnt_put(Creature *c, const uint16_t q[6]) { memcpy(&c->win[2], q, 12); }

static void genotype(Creature *c) {
    uint64_t h = FNV_INIT; int cod = 0;
    h = fnv1a(&c->species, sizeof c->species, h);
    for (int i = 0; i < c->germ_len; i++) {
        uint8_t op = c->germ[i];
        if (op == OP_NOP || !op_valid(op)) continue;
        uint8_t rec[4] = { (uint8_t)i, (uint8_t)(i >> 8), op, 0 };
        cod++;
        if (op_has_imm(op) && i + 1 < c->germ_len) { rec[3] = c->germ[++i]; cod++; }
        h = fnv1a(rec, 4, h);
    }
    uint64_t fh = h ? h : 1, rh = fnv1a(c->germ, (size_t)c->germ_len, fnv1a(&c->species, sizeof c->species, FNV_INIT));
    uint16_t cd = (uint16_t)cod;
    memcpy(&c->win[16], &fh, 8); memcpy(&c->win0[0], &rh, 8); memcpy(&c->win[24], &cd, 2);
    c->ls[4] = 1;
}

static void hook(World *w, Creature *c) {
    Island *I = cur;
    if (I->P->forces) {                               /* forces : l'espèce suit le germen, pas un --pred collé */
        int sp = tape_has_attack(c->germ, c->germ_len) ? 1 : 0;
        if (c->species != sp) { c->species = sp; if (c->ls[4]) genotype(c); }
    }
    if (I->P->osmo) {                                 /* osmotrophie : avant le relevé, donc « a mangé » = EAT exécuté */
        int f = w->food[c->y][c->x], take = f < I->P->osmo ? f : I->P->osmo;
        if (take) { w->food[c->y][c->x] = (uint8_t)(f - take); c->e = c->e + take * 5 > 220 ? 220 : c->e + take * 5; }
    }
    c->ls[0] = c->steps; c->ls[1] = c->eaten; c->ls[2] = c->splits; c->ls[3] = c->d; c->ls[5] = (int)w->t;
    int cand_v[36], m = 0, W = w->cfg.w, H = w->cfg.h;
    for (int dy = -1; dy <= 1; dy++)
        for (int dx = -1; dx <= 1; dx++) {
            int y = (c->y + dy + H) % H, x = (c->x + dx + W) % W;
            for (int k = 0; k < I->em_n[y][x]; k++)
                if (I->em_id[y][x][k] != c->id) cand_v[m++] = I->em_v[y][x][k];
        }
    int sig = 0, nsig = 0;
    if (m >= 1) {
        int i = (int)rng_below(&I->tr, (uint32_t)m);
        sig = cand_v[i] + 1;
        if (m >= 2) { int j = (int)rng_below(&I->tr, (uint32_t)(m - 1)); if (j >= i) j++; nsig = cand_v[j] + 1; }
    }
    c->win[0] = (uint8_t)sig; c->win[1] = (uint8_t)nsig;
    int low = (I->P->channel && sig) ? sig - 1 : I->env, high = I->env;
    if (I->npred) {                                   /* zone de mort : Chebyshev ≤ 1 (tore) ; perçue directement (quartet haut = 15) */
        for (int q = 0; q < I->npred; q++) {
            int dx = abs(c->x - I->px[q]), dy = abs(c->y - I->py[q]);
            if (dx > W - dx) dx = W - dx;
            if (dy > H - dy) dy = H - dy;
            if (dx <= 1 && dy <= 1) { high = 15; c->e -= I->P->pred_dmg; I->pred_hits++; }
        }
    }
    w->stimulus = (high << 4) | low;
}

/* prédateur : un pas (4-voisinage) vers la case la plus peuplée (3×3) avec p = 0.7, sinon au hasard */
static void pred_move(Island *I) {
    World *w = I->w; int W = w->cfg.w, H = w->cfg.h;
    for (int q = 0; q < I->npred; q++) {
        int best = (int)rng_below(&I->rr, 4);
        if (rng_double(&I->rr) < 0.7) {
            int bv = -1;
            for (int d = 0; d < 4; d++) {
                int x = (I->px[q] + TDX[d] + W) % W, y = (I->py[q] + TDY[d] + H) % H, v = 0;
                for (int dy = -1; dy <= 1; dy++) for (int dx = -1; dx <= 1; dx++) v += w->occ[(y + dy + H) % H][(x + dx + W) % W];
                if (v > bv) { bv = v; best = d; }
            }
        }
        I->px[q] = (I->px[q] + TDX[best] + W) % W; I->py[q] = (I->py[q] + TDY[best] + H) % H;
    }
}

/* copie réelle (option a) : la bande = soma du parent (64 o) ‖ 64 octets de bruit (matière première), dans un
 * ordre tiré au hasard comme dans la soupe, exécutée par soup_exec (addr_b). Naissance = critère de la soupe :
 * la moitié brute a ≥ 8 octets écrits ET changés, ≥ SOUP_FID octets identiques au parent, et n'en était pas une
 * copie. Rien n'est offert : un génome qui ne sait pas se copier ne se reproduit pas. */
static void copy_attempt(Island *I, Creature *c) {
    World *w = I->w; const Params *P = I->P;
    uint8_t tape[SOUP_TAPE], old[SOUP_TAPE], wrt[SOUP_TAPE] = { 0 };
    int po = (int)rng_below(&I->rr, 2) * SOUP_PROG, ro = SOUP_PROG - po;
    memcpy(tape + po, c->dna, SOUP_PROG);
    for (int j = 0; j < SOUP_PROG; j++) tape[ro + j] = (uint8_t)rng_next(&I->rr);
    memcpy(old, tape, SOUP_TAPE);
    SoupCfg sc = { .addr_b = 1, .no_store = 0, .steps = P->rep_steps };
    soup_exec(tape, &sc, wrt);
    c->e -= P->rep_cost; I->rep_try++;
    memcpy(c->dna, tape + po, SOUP_PROG);              /* le soma du parent a aussi tourné sur la bande */
    int nw = 0, same = 0;
    for (int j = 0; j < SOUP_PROG; j++) { nw += wrt[ro + j] && tape[ro + j] != old[ro + j]; same += tape[ro + j] == old[po + j]; }
    if (nw < 8 || same < SOUP_FID || c->e <= 0) return;
    uint8_t child[SOUP_PROG]; memcpy(child, tape + ro, SOUP_PROG);
    for (int j = 0; j < SOUP_PROG; j++) if (rng_double(&I->rr) < P->rep_mut) child[j] = (uint8_t)rng_next(&I->rr);
    int dist = 0; for (int j = 0; j < SOUP_PROG; j++) dist += child[j] != old[po + j];
    int d = (c->d + 1) & 3, give = c->e / 3;
    int sp = P->forces ? (tape_has_attack(child, SOUP_PROG) ? 1 : 0) : c->species;
    Creature *k = creature_new((c->x + TDX[d] + w->cfg.w) % w->cfg.w, (c->y + TDY[d] + w->cfg.h) % w->cfg.h,
                               child, SOUP_PROG, give, w->nid++, sp, c->id);
    k->win[30] = (uint8_t)(dist > 255 ? 255 : dist);
    int nb0 = w->nnb; world_spawn(w, k);
    if (w->nnb > nb0) { c->splits++; c->e -= give + 8; I->rep_ok++; w->st.born++; }
}

static int find_parent(const Island *I, int id) {
    int lo = 0, hi = I->nsnap - 1;
    while (lo <= hi) { int mid = (lo + hi) / 2; if (I->snap_id[mid] == id) return mid; if (I->snap_id[mid] < id) lo = mid + 1; else hi = mid - 1; }
    return -1;
}

static void island_tick(Island *I) {
    World *w = I->w; const Params *P = I->P;
    if (rng_below(&I->tr, 50) == 0) I->env = (int)rng_below(&I->tr, P->pred ? 15 : 16);   /* 15 = danger, réservé */
    if (I->npred && w->t % P->pred_every == 0) pred_move(I);
    if (P->season_amp > 0) {
        double f = 1 + P->season_amp * sin(2 * M_PI * (double)w->t / P->season_period);
        w->cfg.food_regrow = (int)lround(P->regrow * f);
    }
    memset(I->em_n, 0, sizeof I->em_n);
    I->nsnap = w->ncs;
    for (int i = 0; i < w->ncs; i++) {
        Creature *c = w->cs[i];
        I->snap_id[i] = c->id; I->snap_fh[i] = FH(c);
        if (c->emitted >= 0 && I->em_n[c->y][c->x] < 4) {
            int k = I->em_n[c->y][c->x]++;
            I->em_v[c->y][c->x][k] = (uint8_t)(c->emitted & 15); I->em_id[c->y][c->x][k] = c->id;
        }
    }
    int n_before = w->ncs;
    world_step(w);
    if (P->repro) {                                   /* copie réelle, avant le relevé (splits++ visible ce tick) */
        int n = w->ncs;
        for (int i = 0; i < n && w->ncs + w->nnb < w->cfg.max_pop; i++) {
            Creature *c = w->cs[i];
            if (c->e < P->rep_energy || c->len != SOUP_PROG) continue;
            if (P->rep_p < 1 && rng_double(&I->rr) >= P->rep_p) continue;
            copy_attempt(I, c);
        }
    }
    w->hist[(w->hist_pos + HIST_N - 1) % HIST_N] = (uint8_t)((I->env << 4) | I->env);   /* l'histoire = l'environnement */
    w->stimulus = (I->env << 4) | I->env;
    int born = 0, novel = 0; static __thread int bd[CMAX];
    for (int i = 0; i < w->ncs; i++) {
        Creature *c = w->cs[i];
        if (!c->ls[4]) {                              /* nouveau-né */
            genotype(c); bd[born] = P->repro ? c->win[30] : -1; born++;
            int p = find_parent(I, c->parent);
            if (p < 0 || I->snap_fh[p] != FH(c)) novel++;
            c->lscore = 0;
            continue;
        }
        if (c->ls[5] != (int)w->t) continue;
        int moved = c->steps != c->ls[0], ate = c->eaten != c->ls[1], split = c->splits != c->ls[2], turned = c->d != c->ls[3];
        int em = c->emitted >= 0 ? (c->emitted & 15) : 16;
        int a = em * 8 + (moved | turned << 1 | split << 2);
        uint16_t k[6]; cnt_get(c, k);
        if (k[0] < 65535) { k[0]++; k[1] += moved; k[2] += ate; k[3] += turned; k[4] += split; k[5] += c->emitted >= 0; cnt_put(c, k); }
        if (c->win[0] && c->win[1] && I->soc.pairs < SOC_CAP)
            soc_code(&I->tab, &I->soc, a, (int)c->lscore, c->win[0] - 1, c->win[1] - 1);
        c->lscore = a + 1;
    }
    int dead = n_before + born - w->ncs;
    I->births += born; I->deaths += dead; I->novel += novel;
    /* ombre neutre : mêmes morts, mêmes naissances, autant d'innovations, parents tirés au hasard */
    for (int d = 0; d < dead && I->nsh > 0; d++) { int j = (int)rng_below(&I->tr, (uint32_t)I->nsh); I->sh[j] = I->sh[--I->nsh]; }
    int base = I->nsh;
    for (int b = 0; b < born; b++) {
        uint64_t lab;
        if (b < novel || base == 0) lab = ((uint64_t)(I->idx + 1) << 48) | I->sh_next++;
        else lab = I->sh[rng_below(&I->tr, (uint32_t)base)];
        I->sh[I->nsh++] = lab;
    }
    /* ombre composant : morts au hasard ; naissance = parent d'ombre au hasard + même opérateur de mutation */
    for (int d = 0; d < dead && I->nsg > 0; d++) { int j = (int)rng_below(&I->sr, (uint32_t)I->nsg); I->sg[j] = I->sg[--I->nsg]; }
    int sbase = I->nsg;
    for (int b = 0; b < born && I->nsg < 2 * CMAX; b++) {
        if (sbase == 0) break;
        const struct SG *pa = &I->sg[rng_below(&I->sr, (uint32_t)sbase)]; struct SG *ch = &I->sg[I->nsg++];
        ch->species = pa->species;
        if (bd[b] >= 0) {                             /* copie : autant de substitutions que l'écart réel enfant/parent */
            ch->len = pa->len; memcpy(ch->dna, pa->dna, (size_t)pa->len);
            for (int q = 0; q < bd[b] && ch->len > 0; q++) ch->dna[rng_below(&I->sr, (uint32_t)ch->len)] = (uint8_t)rng_next(&I->sr);
        } else {
            uint8_t out[MAX_LEN + 16]; int m = mutate(pa->dna, pa->len, &I->sr, P->mut, P->dup, out);
            if (m > MAX_LEN) m = MAX_LEN;
            ch->len = m; memcpy(ch->dna, out, (size_t)m);
        }
    }
}

static void *island_run(void *arg) {
    Island *I = arg; cur = I;
    while (I->w->t < I->t_end) island_tick(I);
    return NULL;
}

/* ---- suivi des lignées / étiquettes (génotypes réels et ombre) ---- */
typedef struct { uint64_t key; int consec, maxab; uint8_t counted; } Lin;
typedef struct { Lin *v; int n, cap; long cum; } Lins;
static int cmp_u64(const void *a, const void *b) { uint64_t x = *(const uint64_t *)a, y = *(const uint64_t *)b; return x < y ? -1 : x > y; }
/* keys : triées. Met à jour la liste, renvoie le nombre de nouvelles persistances. founders : tout compter comme déjà vu. */
static int lins_update(Lins *L, const uint64_t *keys, int n, int founders, int minab, int *alive_persist, double *H, int *ndist) {
    Lin *nv = malloc(sizeof(Lin) * (size_t)(n + 1)); int m = 0, j = 0, fresh = 0, ap = 0; double h = 0;
    for (int i = 0; i < n;) {
        int k = i; while (k < n && keys[k] == keys[i]) k++;
        double p = (double)(k - i) / n; h -= p * log2(p);
        while (j < L->n && L->v[j].key < keys[i]) j++;
        Lin x = { keys[i], 1, k - i, (uint8_t)founders };
        if (j < L->n && L->v[j].key == keys[i]) {
            x.consec = L->v[j].consec + 1; x.counted = L->v[j].counted; if (L->v[j].maxab > x.maxab) x.maxab = L->v[j].maxab;
        }
        if (x.consec >= PERSIST && x.maxab >= minab) { ap++; if (!x.counted) { x.counted = 1; fresh++; } }
        nv[m++] = x; i = k;
    }
    free(L->v); L->v = nv; L->n = m; L->cap = n + 1; L->cum += fresh;
    if (alive_persist) *alive_persist = ap;
    if (H) *H = h;
    if (ndist) *ndist = m;
    return fresh;
}

/* ---- niches et cycles ---- */
typedef struct { double ring[RING]; int stable, hi, lo; long first_stable; } Niche;
typedef struct { int armed, quiet; } Cyc;
static int acf_cycle(const double *ring, int n, long pos, double *peak) {   /* renvoie la période (en échantillons) ou 0 */
    if (n < 64) return 0;
    double x[RING], sx = 0, sy = 0, sxy = 0, sxx = 0;
    for (int i = 0; i < n; i++) { x[i] = ring[(pos - n + i + RING * 4) % RING]; sx += i; sy += x[i]; sxy += i * x[i]; sxx += (double)i * i; }
    double b = (n * sxy - sx * sy) / (n * sxx - sx * sx), a = (sy - b * sx) / n, v = 0;
    for (int i = 0; i < n; i++) { x[i] -= a + b * i; v += x[i] * x[i]; }
    if (v <= 1e-12) return 0;
    int trough = 0;
    for (int k = 1; k <= 32; k++) {
        double r = 0; for (int i = 0; i + k < n; i++) r += x[i] * x[i + k]; r /= v;
        if (!trough && r < -0.2) trough = k;
        if (trough && k > trough && k >= 4 && r > 0.4) { *peak = r; return k; }
    }
    return 0;
}

typedef struct {
    Params P; Island I[KMAX]; Rng gr;
    long t; long sample_i;
    Lins geno, shadow, comp, scomp; int founders_done;
    Rng sgr; int last_ess;                            /* ombre composant (migration) ; dernier min_ess mesuré */
    Niche niche[NNICHE];
    double pop_ring[RING]; Cyc cyc_pop, cyc_niche[NNICHE];
    int soc_streak, soc_on; long soc_events;
    long n_events;
    long emits_prev;
    FILE *series, *events, *forces_out; char dir[512];
    int saw_sp1;
} Terra;

static void event(Terra *T, const char *type, const char *fmt, ...) __attribute__((format(printf, 3, 4)));
#include <stdarg.h>
static void event(Terra *T, const char *type, const char *fmt, ...) {
    char buf[512]; va_list ap; va_start(ap, fmt); vsnprintf(buf, sizeof buf, fmt, ap); va_end(ap);
    if (T->events) { fprintf(T->events, "%ld\t%s\t%s\n", T->t, type, buf); fflush(T->events); }
    T->n_events++;
}

/* composants d'un génome : COMPK-grammes d'opcodes valides (sans immédiats ni position), dédoublonnés, ajoutés à ck */
static int comp_keys(int species, const uint8_t *g, int len, uint64_t **ck, long *nc, long *cap) {
    uint8_t ops[MAX_LEN]; int no = 0;
    for (int b = 0; b < len; b++) {
        uint8_t op = g[b];
        if (op == OP_NOP || !op_valid(op)) continue;
        ops[no++] = op; if (op_has_imm(op)) b++;
    }
    if (*nc + no + 1 > *cap) { while (*nc + no + 1 > *cap) *cap *= 2; *ck = realloc(*ck, sizeof(uint64_t) * (size_t)*cap); }
    long st = *nc; uint64_t *k = *ck;
    for (int b = 0; b + COMPK - 1 < no; b++) { uint64_t x = (uint64_t)species; for (int q = 0; q < COMPK; q++) x = x << 8 | ops[b + q]; k[(*nc)++] = x; }
    qsort(k + st, (size_t)(*nc - st), sizeof *k, cmp_u64);
    long u = st; for (long b = st; b < *nc; b++) if (b == st || k[b] != k[b - 1]) k[u++] = k[b];
    *nc = u; return (int)(u - st);
}

static int total_pop(const Terra *T) { int n = 0; for (int k = 0; k < T->P.K; k++) n += T->I[k].w->ncs; return n; }

static void sample(Terra *T) {
    const Params *P = &T->P;
    int N = total_pop(T);
    uint64_t *keys = malloc(sizeof(uint64_t) * (size_t)(N + 1)), *raw = malloc(sizeof(uint64_t) * (size_t)(N + 1));
    double len_sum = 0, cod_sum = 0; int len_max = 0, j = 0;
    int nh[NNICHE] = { 0 }, ncls = 0;
    long births = 0, deaths = 0, emits = 0, pairs = 0; double L0 = 0, Lr = 0, Ln = 0;
    for (int k = 0; k < P->K; k++) {
        Island *I = &T->I[k]; World *w = I->w;
        for (int i = 0; i < w->ncs; i++) {
            Creature *c = w->cs[i];
            keys[j] = FH(c); raw[j] = RAWH(c); j++;
            len_sum += c->germ_len; cod_sum += COD(c); if (c->germ_len > len_max) len_max = c->germ_len;
            uint16_t q[6]; cnt_get(c, q);
            if (q[0] >= 10) {
                int id = 0, m3 = 1;
                for (int f = 1; f <= 5; f++) { double x = (double)q[f] / q[0]; id += (x < 0.05 ? 0 : x < 0.5 ? 1 : 2) * m3; m3 *= 3; }
                nh[id]++; ncls++;
            }
            memset(&c->win[2], 0, 12);
        }
        births += I->births; deaths += I->deaths; I->births = I->deaths = I->novel = 0;
        emits += w->st.emits;
        pairs += I->soc.pairs; L0 += I->soc.L0; Lr += I->soc.Lr; Ln += I->soc.Ln;
        memset(&I->soc, 0, sizeof I->soc); tab_clear(&I->tab);
    }
    qsort(keys, (size_t)N, sizeof *keys, cmp_u64); qsort(raw, (size_t)N, sizeof *raw, cmp_u64);
    int nraw = 0; for (int i = 0; i < N; i++) if (!i || raw[i] != raw[i - 1]) nraw++;
    int ap = 0, ng = 0; double Hg = 0;
    int fresh = lins_update(&T->geno, keys, N, !T->founders_done, CLONES, &ap, &Hg, &ng);
    /* ombre */
    int NS = 0; for (int k = 0; k < P->K; k++) NS += T->I[k].nsh;
    uint64_t *sk = malloc(sizeof(uint64_t) * (size_t)(NS + 1)); int s = 0;
    for (int k = 0; k < P->K; k++) for (int i = 0; i < T->I[k].nsh; i++) sk[s++] = T->I[k].sh[i];
    qsort(sk, (size_t)NS, sizeof *sk, cmp_u64);
    lins_update(&T->shadow, sk, NS, !T->founders_done, CLONES, NULL, NULL, NULL);
    /* composants (« gènes ») : COMPK-grammes d'opcodes valides consécutifs (sans immédiats ni position), par espèce.
     * Bedau-Packard au niveau composant : un trigramme compte quand il est porté par ≥ max(CLONES, 1 % de la pop)
     * individus et présent PERSIST échantillons consécutifs. */
    long cap = 1024, nc = 0; uint64_t *ck = malloc(sizeof(uint64_t) * (size_t)cap); double comp_per = 0;
    for (int k = 0; k < P->K; k++) {
        World *w = T->I[k].w;
        for (int i = 0; i < w->ncs; i++) comp_per += comp_keys(w->cs[i]->species, w->cs[i]->germ, w->cs[i]->germ_len, &ck, &nc, &cap);
    }
    qsort(ck, (size_t)nc, sizeof *ck, cmp_u64);
    int cap_alive = 0, nco = 0, minab = N / 100 > CLONES ? N / 100 : CLONES;
    int cfresh = lins_update(&T->comp, ck, (int)nc, !T->founders_done, minab, &cap_alive, NULL, &nco);
    /* même mesure sur l'ombre composant (mêmes seuils, taille d'ombre = taille réelle) */
    nc = 0; int NG = 0;
    for (int k = 0; k < P->K; k++) for (int i = 0; i < T->I[k].nsg; i++, NG++) { struct SG *g = &T->I[k].sg[i]; comp_keys(g->species, g->dna, g->len, &ck, &nc, &cap); }
    qsort(ck, (size_t)nc, sizeof *ck, cmp_u64);
    int s_alive = 0, sminab = NG / 100 > CLONES ? NG / 100 : CLONES;
    int sfresh = lins_update(&T->scomp, ck, (int)nc, !T->founders_done, sminab, &s_alive, NULL, NULL);
    free(ck);
    /* réplicateurs (mode copie ou fondateurs de 64 o) : part de copieurs vérifiés (soup_copier, même budget) sur ≤ 64
     * individus tirés par un Rng dérivé du tick (aucune influence sur la dynamique) ; toutes les 25 mesures,
     * octets essentiels (ablation) du meilleur de 2 copieurs */
    double cfrac = -1; int ness = -1;
    if (P->repro || P->founders >= F_SOUP) {
        Rng mr; rng_seed(&mr, P->seed * 0x9E3779B97F4A7C15ULL ^ (uint64_t)T->t);
        SoupCfg sc = { .addr_b = 1, .no_store = 0, .steps = P->rep_steps };
        int tried = 0, cop = 0; int want_ess = T->sample_i % 25 == 0; int nab = 0;
        for (int q = 0; q < 64 && N > 0; q++) {
            int r = (int)rng_below(&mr, (uint32_t)N), k = 0;
            while (r >= T->I[k].w->ncs) r -= T->I[k++].w->ncs;
            Creature *c = T->I[k].w->cs[r]; if (c->germ_len != SOUP_PROG) continue;
            tried++;
            if (soup_copier(c->germ, &sc)) {
                cop++;
                if (want_ess && nab < 2) { uint8_t e[SOUP_PROG]; int n = soup_ablate(c->germ, &sc, e); nab++; if (n >= 0 && (ness < 0 || n < ness)) ness = n; }
            }
        }
        if (tried) cfrac = (double)cop / tried;
        if (want_ess) T->last_ess = ness;
    }
    long rtry = 0, rok = 0, phits = 0;
    for (int k = 0; k < P->K; k++) { rtry += T->I[k].rep_try; rok += T->I[k].rep_ok; phits += T->I[k].pred_hits; T->I[k].rep_try = T->I[k].rep_ok = T->I[k].pred_hits = 0; }
    T->founders_done = 1;
    free(keys); free(raw); free(sk);
    /* niches */
    long pos = T->sample_i % RING; int occ = 0, nstable = 0; double Hb = 0;
    for (int n = 0; n < NNICHE; n++) {
        Niche *z = &T->niche[n];
        double f = ncls ? (double)nh[n] / ncls : 0;
        z->ring[pos] = f;
        if (nh[n]) { occ++; Hb -= f * log2(f); }
        z->hi = f >= 0.05 ? z->hi + 1 : 0;
        z->lo = f < 0.02 ? z->lo + 1 : 0;
        if (!z->stable && z->hi >= PERSIST) {
            z->stable = 1;
            if (z->first_stable < 0) z->first_stable = T->t;
            event(T, "niche_stable", "niche=%d bouge=%d mange=%d tourne=%d divise=%d emet=%d frac=%.3f",
                  n, n % 3, n / 3 % 3, n / 9 % 3, n / 27 % 3, n / 81 % 3, f);
        }
        if (z->stable && z->lo >= PERSIST) { z->stable = 0; event(T, "niche_perdue", "niche=%d", n); }
        nstable += z->stable;
    }
    /* cycles */
    T->pop_ring[pos] = N;
    int navail = T->sample_i + 1 < RING ? (int)T->sample_i + 1 : RING;
    double pk = 0; int per;
    Cyc *cy = &T->cyc_pop;
    if ((per = acf_cycle(T->pop_ring, navail, pos + 1, &pk)) && !cy->armed) {
        event(T, "cycle", "serie=pop periode=%d ticks acf=%.2f", per * P->sample, pk); cy->armed = 1; cy->quiet = 0;
    } else if (!per && cy->armed && ++cy->quiet >= 32) cy->armed = 0;
    for (int n = 0; n < NNICHE; n++) {
        if (!T->niche[n].stable) continue;
        cy = &T->cyc_niche[n];
        if ((per = acf_cycle(T->niche[n].ring, navail, pos + 1, &pk)) && !cy->armed) {
            event(T, "cycle", "serie=niche%d periode=%d ticks acf=%.2f", n, per * P->sample, pk); cy->armed = 1; cy->quiet = 0;
        } else if (!per && cy->armed && ++cy->quiet >= 32) cy->armed = 0;
    }
    /* social */
    double mi = pairs ? (L0 - Lr) / pairs : 0, mn = pairs ? (L0 - Ln) / pairs : 0, ex = mi - mn;
    if (pairs >= 1000 && ex > P->soc_thr) T->soc_streak++; else T->soc_streak = 0;
    if (T->soc_streak >= 3 && !T->soc_on) { T->soc_on = 1; T->soc_events++; event(T, "social", "exces=%.4f bits/paire paires=%ld (3 fenetres > %.3f)", ex, pairs, P->soc_thr); }
    if (T->soc_streak == 0) T->soc_on = 0;
    if (N == 0) event(T, "extinction", "pop=0");
    fprintf(T->series, "%ld\t%d\t%d\t%.4f\t%d\t%ld\t%ld\t%d\t%d\t%.2f\t%d\t%.2f\t%d\t%d\t%.4f\t%ld\t%.5f\t%.5f\t%.5f\t%ld\t%ld\t%ld\t%d\t%.1f\t%ld\t%d\t%d"
                       "\t%ld\t%ld\t%.3f\t%d\t%ld\t%ld\t%d\t%d\n",
            T->t, N, ng, Hg, nraw, T->geno.cum, T->shadow.cum, fresh, ap, N ? len_sum / N : 0, len_max, N ? cod_sum / N : 0,
            occ, nstable, Hb, pairs, mi, mn, ex, births, deaths, emits - T->emits_prev,
            nco, N ? comp_per / N : 0, T->comp.cum, cfresh, cap_alive,
            rtry, rok, cfrac, T->last_ess, phits, T->scomp.cum, sfresh, s_alive);
    fflush(T->series);
    T->emits_prev = emits;
    if (T->forces_out) {
        int sp1 = 0, isles = 0, food = 0; long attacks = 0;
        for (int k = 0; k < P->K; k++) {
            World *w = T->I[k].w;
            if (w->ncs) isles++;
            attacks += w->st.attacks;
            for (int i = 0; i < w->ncs; i++) if (w->cs[i]->species) sp1++;
            for (int y = 0; y < w->cfg.h; y++) for (int x = 0; x < w->cfg.w; x++) food += w->food[y][x];
        }
        fprintf(T->forces_out, "%ld\t%d\t%d\t%ld\t%d\t%d\t%d\n", T->t, N, sp1, attacks, P->K, isles, food);
        fflush(T->forces_out);
        if (sp1 && !T->saw_sp1) { T->saw_sp1 = 1; event(T, "forces", "sp1=%d pop=%d attacks=%ld K=%d", sp1, N, attacks, P->K); }
    }
    T->sample_i++;
}

static const char *HEADER = "tick\tpop\tn_geno\tH_geno\tn_raw\tpersist_cum\tshadow_cum\tnew_persist\tpersist_alive\tlen_mean\tlen_max\tcoding_mean"
                            "\tniches_occ\tstable_niches\tH_behav\tsoc_pairs\tsoc_mi\tsoc_null\tsoc_excess\tbirths\tdeaths\temits"
                            "\tn_comp\tcomp_per_ind\tcomp_cum\tcomp_new\tcomp_alive"
                            "\trep_try\trep_ok\tcopier_frac\tmin_ess\tpred_hits\tscomp_cum\tscomp_new\tscomp_alive\n";

/* ---- checkpoint ---- */
#define TMAGIC3 "OADNTER3"
#define TMAGIC4 "OADNTER4"
static int wr(FILE *f, const void *p, size_t n) { return fwrite(p, 1, n, f) == n; }
static int rd(FILE *f, void *p, size_t n) { return fread(p, 1, n, f) == n; }
static int save(Terra *T) {
    char tmp[600], path[600]; snprintf(tmp, sizeof tmp, "%s/state.ck.tmp", T->dir); snprintf(path, sizeof path, "%s/state.ck", T->dir);
    FILE *f = fopen(tmp, "wb"); if (!f) return 0;
    int ok = wr(f, TMAGIC4, 8) && wr(f, &T->P, sizeof T->P) && wr(f, &T->gr, sizeof T->gr) && wr(f, &T->t, sizeof T->t) &&
             wr(f, &T->sample_i, sizeof T->sample_i) && wr(f, &T->founders_done, sizeof(int)) &&
             wr(f, T->niche, sizeof T->niche) && wr(f, T->pop_ring, sizeof T->pop_ring) && wr(f, &T->cyc_pop, sizeof T->cyc_pop) &&
             wr(f, T->cyc_niche, sizeof T->cyc_niche) && wr(f, &T->soc_streak, sizeof(int)) && wr(f, &T->soc_on, sizeof(int)) &&
             wr(f, &T->soc_events, sizeof(long)) && wr(f, &T->n_events, sizeof(long)) && wr(f, &T->emits_prev, sizeof(long)) &&
             wr(f, &T->sgr, sizeof(Rng)) && wr(f, &T->last_ess, sizeof(int));
    Lins *ls[4] = { &T->geno, &T->shadow, &T->comp, &T->scomp };
    for (int i = 0; i < 4 && ok; i++) ok = wr(f, &ls[i]->n, sizeof(int)) && wr(f, &ls[i]->cum, sizeof(long)) && wr(f, ls[i]->v, sizeof(Lin) * (size_t)ls[i]->n);
    for (int k = 0; k < T->P.K && ok; k++) {
        Island *I = &T->I[k];
        ok = wr(f, I->w, sizeof(World)) && wr(f, &I->wr, sizeof(Rng)) && wr(f, &I->tr, sizeof(Rng)) && wr(f, &I->env, sizeof(int)) &&
             wr(f, &I->nsh, sizeof(int)) && wr(f, &I->sh_next, sizeof(uint64_t)) && wr(f, I->sh, sizeof(uint64_t) * (size_t)I->nsh) &&
             wr(f, &I->sr, sizeof(Rng)) && wr(f, &I->rr, sizeof(Rng)) && wr(f, &I->npred, sizeof(int)) && wr(f, I->px, sizeof I->px) &&
             wr(f, I->py, sizeof I->py) && wr(f, &I->nsg, sizeof(int));
        for (int i = 0; i < I->nsg && ok; i++) ok = wr(f, &I->sg[i].species, sizeof(int)) && wr(f, &I->sg[i].len, sizeof(int)) && wr(f, I->sg[i].dna, (size_t)I->sg[i].len);
        for (int i = 0; i < I->w->ncs && ok; i++) ok = wr(f, I->w->cs[i], sizeof(Creature));
    }
    ok = fclose(f) == 0 && ok;
    return ok && rename(tmp, path) == 0;
}
static void island_alloc(Island *I) {
    I->tab.t = calloc(TMASK + 1, sizeof(Slot)); I->tab.used = malloc(sizeof(uint32_t) * (TMASK + 1));
    I->sh = malloc(sizeof(uint64_t) * 2 * CMAX);
    I->sg = malloc(sizeof(struct SG) * 2 * CMAX);
}
static int load(Terra *T, const char *path) {
    FILE *f = fopen(path, "rb"); if (!f) return 0;
    char m[8]; int ok = rd(f, m, 8);
    int v4 = ok && !memcmp(m, TMAGIC4, 8), v3 = ok && !memcmp(m, TMAGIC3, 8);
    if (!v3 && !v4) { fclose(f); return 0; }
    if (v4) ok = rd(f, &T->P, sizeof T->P);
    else {
        memset(&T->P, 0, sizeof T->P);
        ok = rd(f, &T->P, offsetof(Params, forces));
        T->P.forces = 0; T->P.expand_every = 0;
    }
    ok = ok && rd(f, &T->gr, sizeof T->gr) && rd(f, &T->t, sizeof T->t) &&
             rd(f, &T->sample_i, sizeof T->sample_i) && rd(f, &T->founders_done, sizeof(int)) &&
             rd(f, T->niche, sizeof T->niche) && rd(f, T->pop_ring, sizeof T->pop_ring) && rd(f, &T->cyc_pop, sizeof T->cyc_pop) &&
             rd(f, T->cyc_niche, sizeof T->cyc_niche) && rd(f, &T->soc_streak, sizeof(int)) && rd(f, &T->soc_on, sizeof(int)) &&
             rd(f, &T->soc_events, sizeof(long)) && rd(f, &T->n_events, sizeof(long)) && rd(f, &T->emits_prev, sizeof(long)) &&
             rd(f, &T->sgr, sizeof(Rng)) && rd(f, &T->last_ess, sizeof(int));
    Lins *ls[4] = { &T->geno, &T->shadow, &T->comp, &T->scomp };
    for (int i = 0; i < 4 && ok; i++) {
        ok = rd(f, &ls[i]->n, sizeof(int)) && rd(f, &ls[i]->cum, sizeof(long));
        ls[i]->v = malloc(sizeof(Lin) * (size_t)(ls[i]->n + 1));
        ok = ok && rd(f, ls[i]->v, sizeof(Lin) * (size_t)ls[i]->n);
    }
    for (int k = 0; k < T->P.K && ok; k++) {
        Island *I = &T->I[k]; island_alloc(I); I->idx = k; I->P = &T->P;
        I->w = malloc(sizeof(World));
        ok = rd(f, I->w, sizeof(World)) && rd(f, &I->wr, sizeof(Rng)) && rd(f, &I->tr, sizeof(Rng)) && rd(f, &I->env, sizeof(int)) &&
             rd(f, &I->nsh, sizeof(int)) && rd(f, &I->sh_next, sizeof(uint64_t)) && I->nsh <= 2 * CMAX &&
             rd(f, I->sh, sizeof(uint64_t) * (size_t)I->nsh) &&
             rd(f, &I->sr, sizeof(Rng)) && rd(f, &I->rr, sizeof(Rng)) && rd(f, &I->npred, sizeof(int)) && rd(f, I->px, sizeof I->px) &&
             rd(f, I->py, sizeof I->py) && rd(f, &I->nsg, sizeof(int)) && I->nsg <= 2 * CMAX && I->npred <= 16;
        for (int i = 0; i < I->nsg && ok; i++) ok = rd(f, &I->sg[i].species, sizeof(int)) && rd(f, &I->sg[i].len, sizeof(int)) &&
                                                   I->sg[i].len >= 0 && I->sg[i].len <= MAX_LEN && rd(f, I->sg[i].dna, (size_t)I->sg[i].len);
        World *w = I->w; w->rng = &I->wr; w->lab = NULL; w->before_vm = hook; w->nnb = 0; w->nem = 0;
        for (int i = 0; i < w->ncs && ok; i++) { w->cs[i] = malloc(sizeof(Creature)); ok = rd(f, w->cs[i], sizeof(Creature)); }
    }
    fclose(f);
    return ok;
}

/* ---- création ---- */
static uint8_t *soup_buf; static int soup_n;
static int load_soup(const char *path) {
    FILE *f = fopen(path, "rb"); if (!f) return 0;
    fseek(f, 0, SEEK_END); long n = ftell(f); rewind(f);
    if (n < SOUP_PROG || n % SOUP_PROG) { fclose(f); return 0; }
    soup_buf = malloc((size_t)n); int ok = fread(soup_buf, 1, (size_t)n, f) == (size_t)n; fclose(f);
    soup_n = (int)(n / SOUP_PROG); return ok;
}
static void make_genome(Terra *T, PoolEntry *e, int i) {
    if (T->P.founders == 0) {                 /* seeds du dépôt, en rotation ; predator = espèce 1 */
        const SeedBlob *s = seed_at(i % N_SEEDS);
        seed_entry(e, s->name, strcmp(s->name, "predator") == 0);
    } else if (T->P.founders == F_RANDOM) {   /* génomes aléatoires (témoin) ; 64 o en mode copie */
        e->species = 0; e->len = T->P.repro ? SOUP_PROG : rng_int(&T->gr, 32, 256);
        for (int j = 0; j < e->len; j++) e->dna[j] = (uint8_t)rng_next(&T->gr);
    } else if (T->P.founders == F_HANDMADE) { /* réplicateur conçu (soup_handmade, addr_b) */
        e->species = 0; e->len = SOUP_PROG; soup_handmade(e->dna, 1);
    } else {                                  /* soupe : un programme tiré uniformément dans la soupe finale (dominants + entourage) */
        e->species = 0; e->len = SOUP_PROG;
        memcpy(e->dna, soup_buf + (size_t)rng_below(&T->gr, (uint32_t)soup_n) * SOUP_PROG, SOUP_PROG);
    }
}
static void terra_new(Terra *T, const Params *P) {
    memset(T, 0, sizeof *T); T->P = *P;
    rng_seed(&T->gr, P->seed);
    rng_seed(&T->sgr, P->seed ^ 0x5C0A905EULL); T->last_ess = -1;
    for (int n = 0; n < NNICHE; n++) T->niche[n].first_stable = -1;
    static PoolEntry pool[CMAX];
    for (int k = 0; k < P->K; k++) {
        Island *I = &T->I[k]; island_alloc(I); I->idx = k; I->P = &T->P;
        rng_seed(&I->wr, P->seed * 1000003ULL + (uint64_t)k * 7919 + 1);
        rng_seed(&I->tr, P->seed * 1000033ULL + (uint64_t)k * 104729 + 2);
        rng_seed(&I->sr, P->seed * 1000037ULL + (uint64_t)k * 130363 + 3);
        rng_seed(&I->rr, P->seed * 1000039ULL + (uint64_t)k * 155921 + 4);
        I->env = (int)rng_below(&I->tr, 16);
        if (P->pred && I->env == 15) I->env = 14;
        I->npred = P->pred;
        for (int q = 0; q < P->pred; q++) { I->px[q] = (int)rng_below(&I->rr, (uint32_t)P->w); I->py[q] = (int)rng_below(&I->rr, (uint32_t)P->h); }
        int n = P->pop0;
        for (int i = 0; i < n; i++) make_genome(T, &pool[i], i + k);
        WorldConfig cfg = cfg_default();
        cfg.w = P->w; cfg.h = P->h; cfg.max_pop = P->max_pop; cfg.food_regrow = P->regrow;
        cfg.mut_rate = P->mut; cfg.dup_rate = P->dup;
        if (P->repro) cfg.split_energy = 1 << 29;    /* SPLIT du cœur sans effet : seule la copie réelle reproduit */
        I->w = world_new(pool, n, &I->wr, &cfg, NULL);
        I->w->before_vm = hook;
        I->w->stimulus = (I->env << 4) | I->env;
        for (int i = 0; i < I->w->ncs; i++) {
            Creature *c = I->w->cs[i]; c->d = (int)rng_below(&I->tr, 4); genotype(c);
            I->sh[I->nsh++] = FH(c);              /* ombre : départ = mêmes génotypes que le réel */
            struct SG *g = &I->sg[I->nsg++]; g->species = c->species; g->len = c->germ_len; memcpy(g->dna, c->germ, (size_t)c->germ_len);
        }
    }
}
static void terra_free(Terra *T) {
    for (int k = 0; k < T->P.K; k++) { world_free(T->I[k].w); free(T->I[k].tab.t); free(T->I[k].tab.used); free(T->I[k].sh); free(T->I[k].sg); }
    free(T->geno.v); free(T->shadow.v); free(T->comp.v); free(T->scomp.v);
}

/* migration en anneau : ~1 % de chaque île part chez la voisine (et autant d'étiquettes d'ombre) */
static void migrate(Terra *T) {
    int K = T->P.K; if (K < 2) return;
    static Creature *mv[KMAX][CMAX]; int nmv[KMAX]; static uint64_t ms[KMAX][CMAX]; int nms[KMAX];
    for (int k = 0; k < K; k++) {
        World *w = T->I[k].w; Island *I = &T->I[k];
        int m = (w->ncs + 50) / 100; nmv[k] = 0; nms[k] = 0;
        for (int q = 0; q < m && w->ncs > 0; q++) {
            int j = (int)rng_below(&T->gr, (uint32_t)w->ncs);
            Creature *c = w->cs[j]; w->occ[c->y][c->x]--;
            memmove(&w->cs[j], &w->cs[j + 1], sizeof(Creature *) * (size_t)(w->ncs - j - 1)); w->ncs--;
            mv[k][nmv[k]++] = c;
            if (I->nsh > 0) { int s = (int)rng_below(&T->gr, (uint32_t)I->nsh); ms[k][nms[k]++] = I->sh[s]; I->sh[s] = I->sh[--I->nsh]; }
        }
    }
    for (int k = 0; k < K; k++) {
        Island *D = &T->I[(k + 1) % K]; World *w = D->w;
        for (int q = 0; q < nmv[k]; q++) {
            Creature *c = mv[k][q];
            if (w->ncs >= CMAX || w->ncs >= w->cfg.max_pop) { free(c); continue; }
            c->x = (int)rng_below(&T->gr, (uint32_t)w->cfg.w); c->y = (int)rng_below(&T->gr, (uint32_t)w->cfg.h);
            c->id = w->nid++; c->emitted = -1; c->lscore = 0;
            w->occ[c->y][c->x]++; w->cs[w->ncs++] = c;
        }
        for (int q = 0; q < nms[k] && D->nsh < 2 * CMAX; q++) D->sh[D->nsh++] = ms[k][q];
    }
    /* ombre composant : même nombre de migrants, Rng à part (le mode d'origine garde ses tirages) */
    static struct SG mg[KMAX][CMAX / 50 + 8]; int nmg[KMAX];
    for (int k = 0; k < K; k++) {
        Island *I = &T->I[k]; nmg[k] = 0;
        for (int q = 0; q < nmv[k] && I->nsg > 0 && nmg[k] < CMAX / 50 + 8; q++) {
            int s = (int)rng_below(&T->sgr, (uint32_t)I->nsg); mg[k][nmg[k]++] = I->sg[s]; I->sg[s] = I->sg[--I->nsg];
        }
    }
    for (int k = 0; k < K; k++) { Island *D = &T->I[(k + 1) % K]; for (int q = 0; q < nmg[k] && D->nsg < 2 * CMAX; q++) D->sg[D->nsg++] = mg[k][q]; }
    for (int k = 0; k < K; k++) {
        Island *I = &T->I[k];
        while (I->nsg > I->w->ncs) { int s = (int)rng_below(&T->sgr, (uint32_t)I->nsg); I->sg[s] = I->sg[--I->nsg]; }
        while (I->nsg < I->w->ncs && I->nsg > 0) { I->sg[I->nsg] = I->sg[rng_below(&T->sgr, (uint32_t)I->nsg)]; I->nsg++; }
    }
    /* l'ombre doit garder la taille du réel */
    for (int k = 0; k < K; k++) {
        Island *I = &T->I[k];
        while (I->nsh > I->w->ncs) { int s = (int)rng_below(&T->gr, (uint32_t)I->nsh); I->sh[s] = I->sh[--I->nsh]; }
        while (I->nsh < I->w->ncs) {
            uint64_t lab = I->nsh ? I->sh[rng_below(&T->gr, (uint32_t)I->nsh)] : ((uint64_t)(k + 1) << 48 | I->sh_next++);
            I->sh[I->nsh++] = lab;
        }
    }
}

/* expansion douce : une île vide (nourriture primordiale, personne). La migration la colonise. */
static void expand_one(Terra *T) {
    if (T->P.K >= KMAX) return;
    int k = T->P.K;
    Island *I = &T->I[k];
    memset(I, 0, sizeof *I);
    island_alloc(I); I->idx = k; I->P = &T->P;
    rng_seed(&I->wr, rng_next(&T->gr) ^ (0xE11AULL * (uint64_t)(k + 1)));
    rng_seed(&I->tr, rng_next(&T->gr) ^ (0xE11BULL * (uint64_t)(k + 1)));
    rng_seed(&I->sr, rng_next(&T->gr) ^ (0xE11CULL * (uint64_t)(k + 1)));
    rng_seed(&I->rr, rng_next(&T->gr) ^ (0xE11DULL * (uint64_t)(k + 1)));
    I->env = (int)rng_below(&I->tr, 16);
    WorldConfig cfg = cfg_default();
    cfg.w = T->P.w; cfg.h = T->P.h; cfg.max_pop = T->P.max_pop; cfg.food_regrow = T->P.regrow;
    cfg.mut_rate = T->P.mut; cfg.dup_rate = T->P.dup;
    if (T->P.repro) cfg.split_energy = 1 << 29;
    I->w = world_new(NULL, 0, &I->wr, &cfg, NULL);
    I->w->before_vm = hook;
    I->w->t = T->t;
    I->w->stimulus = (I->env << 4) | I->env;
    T->P.K++;
    event(T, "expand", "K=%d ile_vide t=%ld", T->P.K, T->t);
}

static volatile sig_atomic_t stop_flag;
static void on_sig(int s) { (void)s; stop_flag = 1; }

static long next_boundary(const Terra *T, long t) {
    long b = (t / T->P.sample + 1) * T->P.sample;
    if (T->P.migrate > 0) { long m = (t / T->P.migrate + 1) * T->P.migrate; if (m < b) b = m; }
    return b;
}

/* tronque un TSV aux lignes de tick ≤ t (reprise) */
static void truncate_tsv(const char *path, long t) {
    FILE *f = fopen(path, "r"); if (!f) return;
    char tmp[600]; snprintf(tmp, sizeof tmp, "%s.tmp", path);
    FILE *o = fopen(tmp, "w"); char line[4096]; int first = 1;
    while (fgets(line, sizeof line, f)) {
        if (first || strtol(line, NULL, 10) <= t) fputs(line, o);
        first = 0;
    }
    fclose(f); fclose(o); rename(tmp, path);
}

/* boucle principale : jusqu'au tick t_target (0 = sans fin) */
static int terra_run(Terra *T, long t_target, int quiet) {
    const Params *P = &T->P;
    time_t t0 = time(NULL); long tick0 = T->t;
    while (!stop_flag && (t_target == 0 || T->t < t_target)) {
        long b = next_boundary(T, T->t);
        if (t_target && b > t_target) b = t_target;
        pthread_t th[KMAX];
        for (int k = 0; k < P->K; k++) { T->I[k].t_end = b; pthread_create(&th[k], NULL, island_run, &T->I[k]); }
        for (int k = 0; k < P->K; k++) pthread_join(th[k], NULL);
        T->t = b;
        if (P->migrate > 0 && T->t % P->migrate == 0) migrate(T);
        if (P->expand_every > 0 && T->t % P->expand_every == 0 && T->P.K < KMAX) expand_one(T);
        if (T->t % P->sample == 0) {
            sample(T);
            if (!quiet && T->sample_i % 10 == 0) {
                double el = difftime(time(NULL), t0);
                fprintf(stderr, "t=%ld pop=%d genotypes=%d persist_cum=%ld ombre=%ld (%.0f ticks/s)\n", T->t, total_pop(T),
                        T->geno.n, T->geno.cum, T->shadow.cum, el > 0 ? (T->t - tick0) / el : 0);
            }
            if (T->t % P->ck_every == 0 && !save(T)) { fprintf(stderr, "checkpoint : échec d'écriture\n"); return 1; }
            if (total_pop(T) == 0) break;
        }
    }
    if (T->t % P->sample == 0) save(T);
    return 0;
}

static int open_outputs(Terra *T, int resumed) {
    char p[600];
    snprintf(p, sizeof p, "%s/series.tsv", T->dir); if (resumed) truncate_tsv(p, T->t);
    T->series = fopen(p, resumed ? "a" : "w"); if (!T->series) return 0;
    if (!resumed) fputs(HEADER, T->series);
    snprintf(p, sizeof p, "%s/events.tsv", T->dir); if (resumed) truncate_tsv(p, T->t);
    T->events = fopen(p, resumed ? "a" : "w"); if (!T->events) return 0;
    if (!resumed) fputs("tick\ttype\tdetail\n", T->events);
    T->forces_out = NULL;
    if (T->P.forces || T->P.expand_every) {
        snprintf(p, sizeof p, "%s/forces.tsv", T->dir); if (resumed) truncate_tsv(p, T->t);
        int fresh = access(p, F_OK) != 0;
        T->forces_out = fopen(p, resumed && !fresh ? "a" : "w");
        if (!T->forces_out) return 0;
        if (!resumed || fresh) fputs("tick\tpop\tsp1\tattacks\tK\tisles\tfood\n", T->forces_out);
    }
    return 1;
}

static Params defaults(void) {
    Params P = { .K = 4, .w = 64, .h = 32, .max_pop = 1000, .regrow = 80, .sample = 200, .ck_every = 2000, .migrate = 1000,
                 .channel = 1, .founders = 0, .pop0 = 64, .mut = 0.02, .dup = 0.05, .season_amp = 0, .season_period = 4000, .soc_thr = 0.05, .seed = 1,
                 .repro = 0, .rep_steps = 8192, .rep_cost = 2, .rep_energy = 70, .rep_p = 1, .rep_mut = 0.005,
                 .pred = 0, .pred_dmg = 25, .pred_every = 2, .osmo = 0,
                 .forces = 0, .expand_every = 0 };
    return P;
}

static int usage(void) {
    fprintf(stderr, "usage : terrarium_run run --dir D [--islands K] [--w W --h H] [--max-pop N] [--regrow R] [--pop0 N]\n"
                    "                       [--ticks T (0 = sans fin)] [--sample S] [--ck-every C] [--migrate M (0 = îles isolées)]\n"
                    "                       [--seed N] [--mut x --dup x] [--channel on|off] [--founders seeds|random|handmade|soup:FICHIER]\n"
                    "                       [--repro split|copy] [--rep-steps N] [--rep-cost E] [--rep-energy E] [--rep-p p] [--rep-mut x]\n"
                    "                       [--pred N (prédateurs par île)] [--pred-dmg E] [--pred-every T] [--osmo U (absorption passive)]\n"
                    "                       [--season-amp a --season-period p] [--soc-thr bits]\n"
                    "                       [--forces 0|1] [--expand-every T (0 = K fixe, multiple de --sample)]\n"
                    "        reprise : --ticks, et chocs --season-amp / --pred / --regrow / --forces / --expand-every\n"
                    "        terrarium_run report DIR [DIR...]   (résumé par quarts)\n"
                    "        terrarium_run selftest\n");
    return 2;
}

static int cmd_run(int argc, char **argv) {
    Params P = defaults(); const char *dir = NULL; long ticks = 20000;
    int got_season_amp = 0, got_season_period = 0, got_pred = 0, got_pred_dmg = 0, got_pred_every = 0, got_regrow = 0;
    int got_forces = 0, got_expand = 0;
    for (int i = 2; i < argc; i++) {
        const char *a = argv[i], *v = i + 1 < argc ? argv[i + 1] : NULL;
        if (!v) return usage();
        if (!strcmp(a, "--dir")) dir = v;
        else if (!strcmp(a, "--islands")) P.K = atoi(v);
        else if (!strcmp(a, "--w")) P.w = atoi(v);
        else if (!strcmp(a, "--h")) P.h = atoi(v);
        else if (!strcmp(a, "--max-pop")) P.max_pop = atoi(v);
        else if (!strcmp(a, "--regrow")) { P.regrow = atoi(v); got_regrow = 1; }
        else if (!strcmp(a, "--pop0")) P.pop0 = atoi(v);
        else if (!strcmp(a, "--ticks")) ticks = atol(v);
        else if (!strcmp(a, "--sample")) P.sample = atoi(v);
        else if (!strcmp(a, "--ck-every")) P.ck_every = atoi(v);
        else if (!strcmp(a, "--migrate")) P.migrate = atoi(v);
        else if (!strcmp(a, "--seed")) P.seed = strtoull(v, NULL, 0);
        else if (!strcmp(a, "--mut")) P.mut = atof(v);
        else if (!strcmp(a, "--dup")) P.dup = atof(v);
        else if (!strcmp(a, "--channel")) P.channel = strcmp(v, "off") != 0;
        else if (!strcmp(a, "--founders")) {
            if (!strcmp(v, "seeds")) P.founders = F_SEEDS;
            else if (!strcmp(v, "random")) P.founders = F_RANDOM;
            else if (!strcmp(v, "handmade")) P.founders = F_HANDMADE;
            else if (!strncmp(v, "soup:", 5) && strlen(v + 5) < sizeof P.ffile) { P.founders = F_SOUP; snprintf(P.ffile, sizeof P.ffile, "%s", v + 5); }
            else return usage();
        }
        else if (!strcmp(a, "--repro")) { if (!strcmp(v, "copy")) P.repro = 1; else if (!strcmp(v, "split")) P.repro = 0; else return usage(); }
        else if (!strcmp(a, "--rep-steps")) P.rep_steps = atoi(v);
        else if (!strcmp(a, "--rep-cost")) P.rep_cost = atoi(v);
        else if (!strcmp(a, "--rep-energy")) P.rep_energy = atoi(v);
        else if (!strcmp(a, "--rep-p")) P.rep_p = atof(v);
        else if (!strcmp(a, "--rep-mut")) P.rep_mut = atof(v);
        else if (!strcmp(a, "--pred")) { P.pred = atoi(v); got_pred = 1; }
        else if (!strcmp(a, "--pred-dmg")) { P.pred_dmg = atoi(v); got_pred_dmg = 1; }
        else if (!strcmp(a, "--pred-every")) { P.pred_every = atoi(v); got_pred_every = 1; }
        else if (!strcmp(a, "--osmo")) P.osmo = atoi(v);
        else if (!strcmp(a, "--season-amp")) { P.season_amp = atof(v); got_season_amp = 1; }
        else if (!strcmp(a, "--season-period")) { P.season_period = atoi(v); got_season_period = 1; }
        else if (!strcmp(a, "--soc-thr")) P.soc_thr = atof(v);
        else if (!strcmp(a, "--forces")) { P.forces = atoi(v); got_forces = 1; }
        else if (!strcmp(a, "--expand-every")) { P.expand_every = atoi(v); got_expand = 1; }
        else return usage();
        i++;
    }
    if (!dir || P.K < 1 || P.K > KMAX || P.sample < 1 || P.ck_every % P.sample || P.w < 4 || P.h < 4 || P.w > GRID_W || P.h > GRID_H ||
        P.max_pop < 1 || P.max_pop > CMAX - 24 || P.pop0 < 1 || P.pop0 > P.max_pop || P.season_period < 1 ||
        (P.repro && P.founders == F_SEEDS) || P.pred < 0 || P.pred > 16 || P.pred_every < 1 || P.rep_steps < 1 || P.rep_energy < 1 ||
        (P.forces != 0 && P.forces != 1) || P.expand_every < 0 || (P.expand_every > 0 && P.expand_every % P.sample)) return usage();
    if (P.founders == F_SOUP && !load_soup(P.ffile)) { fprintf(stderr, "soupe %s illisible (N × 64 octets attendus)\n", P.ffile); return 1; }
    mkdir(dir, 0755);
    static Terra T; char ck[600]; snprintf(ck, sizeof ck, "%s/state.ck", dir);
    int resumed = 0;
    if (access(ck, F_OK) == 0) {
        if (!load(&T, ck)) { fprintf(stderr, "%s illisible\n", ck); return 1; }
        resumed = 1;
        /* choc : ces flags écrasent le checkpoint. Le reste (repro, osmo, seed, mondes) reste. */
        if (got_season_amp) T.P.season_amp = P.season_amp;
        if (got_season_period) T.P.season_period = P.season_period;
        if (got_regrow) {
            T.P.regrow = P.regrow;
            for (int k = 0; k < T.P.K; k++) T.I[k].w->cfg.food_regrow = P.regrow;
        }
        if (got_forces) T.P.forces = P.forces;
        if (got_expand) T.P.expand_every = P.expand_every;
        if (got_pred_dmg) T.P.pred_dmg = P.pred_dmg;
        if (got_pred_every) T.P.pred_every = P.pred_every;
        if (got_pred) {
            T.P.pred = P.pred;
            for (int k = 0; k < T.P.K; k++) {
                Island *I = &T.I[k];
                I->npred = P.pred;
                for (int q = 0; q < P.pred; q++) {
                    I->px[q] = (int)rng_below(&I->rr, (uint32_t)T.P.w);
                    I->py[q] = (int)rng_below(&I->rr, (uint32_t)T.P.h);
                }
            }
        }
        fprintf(stderr, "reprise de %s au tick %ld", ck, T.t);
        if (got_season_amp || got_pred || got_regrow || got_forces || got_expand)
            fprintf(stderr, " (choc : season_amp=%.2f pred=%d regrow=%d forces=%d expand_every=%d)",
                    T.P.season_amp, T.P.pred, T.P.regrow, T.P.forces, T.P.expand_every);
        else
            fprintf(stderr, " (paramètres du checkpoint ; --ticks et les chocs --season-amp/--pred/--regrow/--forces/--expand-every comptent)");
        fprintf(stderr, "\n");
    } else {
        terra_new(&T, &P);
        if (P.repro || P.founders >= F_SOUP) {           /* part de copieurs vérifiés parmi les fondateurs */
            SoupCfg sc = { .addr_b = 1, .no_store = 0, .steps = P.rep_steps }; int n = 0, cop = 0;
            for (int k = 0; k < P.K; k++) for (int i = 0; i < T.I[k].w->ncs; i++, n++) cop += soup_copier(T.I[k].w->cs[i]->germ, &sc) > 0;
            fprintf(stderr, "fondateurs : %d, copieurs vérifiés (budget %d pas) : %d (%.1f %%)\n", n, P.rep_steps, cop, n ? 100.0 * cop / n : 0);
        }
    }
    snprintf(T.dir, sizeof T.dir, "%s", dir);
    if (!open_outputs(&T, resumed)) { fprintf(stderr, "sorties impossibles dans %s\n", dir); return 1; }
    signal(SIGINT, on_sig); signal(SIGTERM, on_sig);
    int rc = terra_run(&T, ticks, 0);
    fprintf(stderr, "fin : t=%ld pop=%d événements=%ld (checkpoint %s)\n", T.t, total_pop(&T), T.n_events, ck);
    fclose(T.series); fclose(T.events); if (T.forces_out) fclose(T.forces_out); terra_free(&T);
    return rc;
}

/* ---- selftest ---- */
static uint64_t terra_hash(const Terra *T) {
    uint64_t h = FNV_INIT;
    for (int k = 0; k < T->P.K; k++) { uint64_t x = world_hash(T->I[k].w); h = fnv1a(&x, 8, h); }
    return h;
}
static char *slurp(const char *p) {
    FILE *f = fopen(p, "rb"); if (!f) return NULL; fseek(f, 0, SEEK_END); long n = ftell(f); rewind(f);
    char *s = malloc((size_t)n + 1); if (fread(s, 1, (size_t)n, f) != (size_t)n) n = 0; s[n] = 0; fclose(f); return s;
}
static int resume_check(const Params *P, long t1, long t2, const char *label, int *pop_end, long *births);
static int selftest(void) {
    int ok = 1;
    /* 1. codeur social : flux dépendant → excès net ; flux indépendant → excès ~ 0 */
    Tab tb = { calloc(TMASK + 1, sizeof(Slot)), malloc(sizeof(uint32_t) * (TMASK + 1)), 0 };
    Rng r; rng_seed(&r, 42);
    for (int mode = 0; mode < 2; mode++) {
        Soc S = { 0 }; int prev = 0;
        for (int i = 0; i < 20000; i++) {
            int s = (int)rng_below(&r, 16), n = (int)rng_below(&r, 16);
            int a = mode == 0 && rng_double(&r) < 0.5 ? s * 8 : (int)rng_below(&r, 4) * 8 + (prev & 1);
            soc_code(&tb, &S, a, prev, s, n); prev = a + 1;
        }
        double ex = ((S.L0 - S.Lr) - (S.L0 - S.Ln)) / S.pairs;
        int good = mode == 0 ? ex > 0.5 : fabs(ex) < 0.02;
        printf("selftest codeur social : flux %s → excès %.4f bits/paire %s\n", mode == 0 ? "dépendant" : "indépendant", ex, good ? "OK" : "ÉCHEC");
        ok &= good; tab_clear(&tb);
    }
    free(tb.t); free(tb.used);
    /* 2. déterminisme + reprise depuis checkpoint = run d'un seul tenant (mode d'origine) */
    Params P = defaults(); P.K = 2; P.w = 32; P.h = 16; P.max_pop = 300; P.regrow = 12; P.sample = 100; P.ck_every = 500; P.migrate = 300; P.seed = 7;
    int pe; long nb;
    ok &= resume_check(&P, 1500, 3000, "", &pe, &nb);
    /* 3. soup-terrarium : copie réelle (réplicateur conçu), osmotrophie, prédateurs, ombre composant — même exigence */
    P.repro = 1; P.founders = F_HANDMADE; P.osmo = 1; P.pred = 2; P.ck_every = 100; P.migrate = 100; P.rep_steps = 2048;
    int g = resume_check(&P, 100, 300, " (copie+prédateurs)", &pe, &nb) && nb > 0;
    printf("selftest copie réelle : %ld naissances par copie (soup_exec), pop finale %d → %s\n", nb, pe, g ? "OK" : "ÉCHEC");
    ok &= g;
    /* 4. forces : ATTACK opcode vs ATTACK en immédiat d'un STORE */
    {
        uint8_t a[] = { OP_NOP, OP_ATTACK }, b[] = { OP_STORE, OP_ATTACK };
        int ha = tape_has_attack(a, 2), hb = tape_has_attack(b, 2);
        printf("selftest forces : opcode ATTACK %s, immédiat %s\n", ha ? "détecté" : "MANQUE", hb ? "FAUX POSITIF" : "ignoré");
        ok &= ha && !hb;
    }
    return ok ? 0 : 1;
}

/* ---- report : résumé d'un ou plusieurs dossiers (par quarts, social poolé, événements) ---- */
typedef struct { int nc, n, cap; char name[40][32]; double *v; } Tsv;
static int tsv_col(const Tsv *S, const char *k) { for (int i = 0; i < S->nc; i++) if (!strcmp(S->name[i], k)) return i; return -1; }
static double V(const Tsv *S, int r, const char *k) { int c = tsv_col(S, k); return c < 0 ? NAN : S->v[(size_t)r * S->nc + c]; }
static int tsv_load(Tsv *S, const char *path) {
    FILE *f = fopen(path, "r"); if (!f) return 0;
    char line[4096]; memset(S, 0, sizeof *S);
    if (!fgets(line, sizeof line, f)) { fclose(f); return 0; }
    for (char *t = strtok(line, "\t\n"); t && S->nc < 40; t = strtok(NULL, "\t\n")) snprintf(S->name[S->nc++], 32, "%s", t);
    while (fgets(line, sizeof line, f)) {
        if (S->n == S->cap) { S->cap = S->cap ? S->cap * 2 : 1024; S->v = realloc(S->v, sizeof(double) * (size_t)S->cap * S->nc); }
        char *e = line;
        for (int i = 0; i < S->nc; i++) { S->v[(size_t)S->n * S->nc + i] = strtod(e, &e); if (*e == '\t') e++; }
        S->n++;
    }
    fclose(f); return S->n > 0;
}
static int qrow(int n, double q) { int r = (int)lround(q * n) - 1; return r < 0 ? 0 : r >= n ? n - 1 : r; }
static void pooled(const Tsv *S, int a, int b, double *P, double *mi, double *mn) {
    *P = *mi = *mn = 0;
    for (int r = a; r < b; r++) { double p = V(S, r, "soc_pairs"); *P += p; *mi += p * V(S, r, "soc_mi"); *mn += p * V(S, r, "soc_null"); }
    if (*P > 0) { *mi /= *P; *mn /= *P; } else *mi = *mn = NAN;
}
static int report_one(const char *dir) {
    char path[600]; Tsv S; snprintf(path, sizeof path, "%s/series.tsv", dir);
    if (!tsv_load(&S, path)) { fprintf(stderr, "report : %s illisible\n", path); return 1; }
    const char *nm = strrchr(dir, '/'); nm = nm && nm[1] ? nm + 1 : dir;
    int n = S.n; double qs[4] = { .25, .5, .75, 1 };
    printf("== %s : %.0f ticks, %d échantillons\n  quart              ", nm, V(&S, n - 1, "tick"), n);
    for (int q = 0; q < 4; q++) printf("%10.0f", V(&S, qrow(n, qs[q]), "tick"));
    printf("\n");
    const char *ks[] = { "pop", "n_geno", "H_geno", "n_comp", "comp_cum", "comp_alive", "scomp_cum", "scomp_alive", "persist_cum", "shadow_cum",
                         "len_mean", "coding_mean", "stable_niches", "H_behav", "copier_frac", "min_ess" };
    int nks = 16;
    if (tsv_col(&S, "scomp_cum") < 0) { ks[6] = ks[8]; ks[7] = ks[9]; ks[8] = ks[10]; ks[9] = ks[11]; ks[10] = ks[12]; ks[11] = ks[13]; nks = 12; }
    for (int k = 0; k < nks; k++) {
        printf("  %-19s", ks[k]);
        int dec = !strcmp(ks[k], "H_geno") || !strcmp(ks[k], "H_behav") || !strcmp(ks[k], "copier_frac") ? 2 : !strcmp(ks[k], "len_mean") || !strcmp(ks[k], "coding_mean") ? 1 : 0;
        for (int q = 0; q < 4; q++) printf("%10.*f", dec, V(&S, qrow(n, qs[q]), ks[k]));
        printf("\n");
    }
    printf("  comp_new/10k ticks ");
    int pr = 0;
    for (int q = 0; q < 4; q++) { int r = qrow(n, qs[q]); double dt = V(&S, r, "tick") - V(&S, pr, "tick");
        printf("%10.0f", dt > 0 ? (V(&S, r, "comp_cum") - V(&S, pr, "comp_cum")) / dt * 1e4 : NAN); pr = r; }
    if (tsv_col(&S, "scomp_cum") >= 0) {
        printf("\n  ombre_new/10k      "); pr = 0;
        for (int q = 0; q < 4; q++) { int r = qrow(n, qs[q]); double dt = V(&S, r, "tick") - V(&S, pr, "tick");
            printf("%10.0f", dt > 0 ? (V(&S, r, "scomp_cum") - V(&S, pr, "scomp_cum")) / dt * 1e4 : NAN); pr = r; }
        printf("\n  naissances/10k     "); 
        for (int q = 0; q < 4; q++) { double b = 0, t0 = V(&S, q * n / 4, "tick"), t1 = V(&S, (q + 1) * n / 4 - 1, "tick");
            for (int r = q * n / 4; r < (q + 1) * n / 4; r++) b += V(&S, r, "births");
            printf("%10.0f", t1 > t0 ? b / (t1 - t0 + V(&S, 0, "tick")) * 1e4 : NAN); }
    }
    printf("\n  nouv. normalisée   ");          /* nouveaux composants / (naissances × octets codants) × 1e4, par quart */
    for (int q = 0; q < 4; q++) { double a = 0, d = 0;
        for (int r = q * n / 4; r < (q + 1) * n / 4; r++) { a += V(&S, r, "comp_new"); d += V(&S, r, "births") * V(&S, r, "coding_mean"); }
        printf("%10.2f", d > 0 ? a / d * 1e4 : 0); }
    printf("\n");
    struct { const char *l; int a, b; } part[3] = { { "tout", 0, n }, { "Q1", 0, n / 4 }, { "Q4", 3 * n / 4, n } };
    for (int i = 0; i < 3; i++) { double P, mi, mn; pooled(&S, part[i].a, part[i].b, &P, &mi, &mn);
        printf("  social %-4s paires=%9.0f MI_réel=%+.4f MI_nul=%+.4f excès=%+.4f bits/paire\n", part[i].l, P, mi, mn, mi - mn); }
    double *ex = malloc(sizeof(double) * (size_t)n); int m = 0, big = 0; double t0 = -1, t1 = -1;
    for (int r = 0; r < n; r++) if (V(&S, r, "soc_pairs") >= 1000) {
        double e = V(&S, r, "soc_excess"); ex[m++] = e;
        if (e > 0.05) { big++; if (t0 < 0) t0 = V(&S, r, "tick"); t1 = V(&S, r, "tick"); } }
    if (m) {
        for (int i = 1; i < m; i++) for (int j = i; j > 0 && ex[j] < ex[j - 1]; j--) { double t = ex[j]; ex[j] = ex[j - 1]; ex[j - 1] = t; }
        printf("  excès par fenêtre (paires≥1000, n=%d) : médiane %+.4f  p95 %+.4f  max %+.4f ; fenêtres > 0.05 : %d", m,
               ex[m / 2], ex[(int)(.95 * (m - 1))], ex[m - 1], big);
        if (big) printf(" (t ∈ [%.0f, %.0f])", t0, t1);
        printf("\n");
    }
    free(ex); free(S.v);
    snprintf(path, sizeof path, "%s/events.tsv", dir);
    FILE *f = fopen(path, "r"); char line[1024]; char types[8][16]; int cnt[8] = { 0 }, nt = 0, ns = 0; long ns0 = -1, ns1 = -1;
    if (f && fgets(line, sizeof line, f)) {
        while (fgets(line, sizeof line, f)) {
            long t = strtol(line, NULL, 10); char *ty = strchr(line, '\t'); if (!ty) continue; ty++;
            char *det = strchr(ty, '\t'); if (!det) continue; *det++ = 0; det[strcspn(det, "\n")] = 0;
            int k; for (k = 0; k < nt && strcmp(types[k], ty); k++) {}
            if (k == nt && nt < 8) snprintf(types[nt++], 16, "%s", ty);
            if (k < 8) cnt[k]++;
            if (!strcmp(ty, "social") || !strcmp(ty, "cycle") || !strcmp(ty, "extinction")) printf("    %8ld %-11s %s\n", t, ty, det);
            if (!strcmp(ty, "niche_stable")) { ns++; if (ns0 < 0) ns0 = t; ns1 = t; }
        }
    }
    if (f) fclose(f);
    printf("  événements :"); if (!nt) printf(" aucun");
    for (int k = 0; k < nt; k++) printf(" %s=%d", types[k], cnt[k]);
    printf("\n");
    if (ns) printf("    niches stables : %d apparitions, première à t=%ld, dernière à t=%ld\n", ns, ns0, ns1);
    printf("\n");
    return 0;
}
static int cmd_report(int argc, char **argv) {
    int rc = 0; if (argc < 3) return usage();
    for (int i = 2; i < argc; i++) rc |= report_one(argv[i]);
    return rc;
}

/* déterminisme + reprise depuis checkpoint = run d'un seul tenant ; renvoie 1 si OK (*pop_end, *births_end : fin du run A) */
static int resume_check(const Params *P, long t1, long t2, const char *label, int *pop_end, long *births) {
    const char *da = "/tmp/terrarium-selftest-a", *db = "/tmp/terrarium-selftest-b";
    char cmd[256]; snprintf(cmd, sizeof cmd, "rm -rf %s %s", da, db); if (system(cmd)) {}
    mkdir(da, 0755); mkdir(db, 0755);
    static Terra A, B;
    terra_new(&A, P); snprintf(A.dir, sizeof A.dir, "%s", da); open_outputs(&A, 0); terra_run(&A, t2, 1);
    uint64_t ha = terra_hash(&A); *pop_end = total_pop(&A); fclose(A.series); fclose(A.events); if (A.forces_out) fclose(A.forces_out); terra_free(&A);
    terra_new(&B, P); snprintf(B.dir, sizeof B.dir, "%s", db); open_outputs(&B, 0); terra_run(&B, t1, 1);
    fclose(B.series); fclose(B.events); if (B.forces_out) fclose(B.forces_out); terra_free(&B);
    { char p[600]; snprintf(p, sizeof p, "%s/state.ck", db); memset(&B, 0, sizeof B);
      if (!load(&B, p)) { printf("selftest : checkpoint illisible\n"); return 0; } }
    snprintf(B.dir, sizeof B.dir, "%s", db); open_outputs(&B, 1); terra_run(&B, t2, 1);
    uint64_t hb = terra_hash(&B); fclose(B.series); fclose(B.events); if (B.forces_out) fclose(B.forces_out); terra_free(&B);
    char pa[600], pb[600]; snprintf(pa, sizeof pa, "%s/series.tsv", da); snprintf(pb, sizeof pb, "%s/series.tsv", db);
    char *sa = slurp(pa), *sb = slurp(pb);
    int same = ha == hb && sa && sb && !strcmp(sa, sb) && strlen(sa) > 100;
    *births = 0;
    if (sa) { Tsv S; if (tsv_load(&S, pa)) { for (int r = 0; r < S.n; r++) *births += (long)V(&S, r, "births"); free(S.v); } }
    printf("selftest reprise%s : hash %016llx vs %016llx, series.tsv %s → %s\n", label, (unsigned long long)ha, (unsigned long long)hb,
           sa && sb && !strcmp(sa, sb) ? "identiques" : "DIFFÉRENTES", same ? "OK" : "ÉCHEC");
    free(sa); free(sb);
    if (system(cmd)) {}
    return same;
}

int main(int argc, char **argv) {
    if (argc >= 2 && !strcmp(argv[1], "run")) return cmd_run(argc, argv);
    if (argc >= 2 && !strcmp(argv[1], "selftest")) return selftest();
    if (argc >= 2 && !strcmp(argv[1], "report")) return cmd_report(argc, argv);
    return usage();
}
