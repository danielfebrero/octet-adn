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
 * --forces 3 : EMIT pose une brique (recette = YIELD/XOR/ATTACK/LOOP/NEURAL/SENSE).
 * 64 briques ouvrent une île vide (KMAX 32). Un 2×2 de recettes distinctes irrigue.
 * --stress 1 : à la naissance, taux = base × (1 + 7×stress). stress = moyenne
 * (énergie basse, case pauvre, voisins denses). Calme → le taux d'avant. L'espèce
 * est le masque ATTACK/YIELD/EMIT/XOR/AND/SUB/NEURAL/ADD. societies.tsv sépare
 * l'excès social intra et inter. Stress 0 : mêmes tirages, même series.tsv.
 * --attention 1 : le sol garde une chaleur (0..15) et l'écho du dernier choix.
 * Le processus (opcodes avant le premier STEP/TURN/EAT/SPLIT/EMIT/ATTACK/YIELD)
 * dépose cette chaleur à la case de départ. Un choix sur une case chaude la
 * dépense : STEP pose 1 nourriture à l'arrivée, TURN pousse 1 chaleur devant,
 * EAT laisse 1 nourriture derrière, EMIT réchauffe les 4 voisins, SPLIT offre
 * 2 à la case de l'enfant, ATTACK refroidit de moitié, YIELD n'écrit que l'écho.
 * Un réflexe sur sol froid ne reçoit rien. Le quartet haut du stimulus devient
 * l'écho s'il y en a un, sinon la chaleur. Attention 0 : aucun relevé, mêmes tirages.
 * --traits 1 : une caractéristique n'est pas déclarée. Chaque tick note les
 * opcodes vraiment exécutés, et six relations (hésite, cultive, entre siens,
 * entre autres, énergie basse, case nue). Un fait devient caractéristique
 * s'il sépare la population (5 % à 95 %), s'il n'est pas la copie d'un fait
 * déjà retenu, et s'il le reste PERSIST échantillons. La niche est le masque
 * de ces caractéristiques, à la place de la grille fixe bouge/mange/tourne/
 * divise/émet. --traits 0 : pas de relevé, mêmes tirages, checkpoint OADNTER7.
 * Traits tient hors de Params. L'écriture avec traits est OADNTER8.
 * --depense 1 : archipel plein (KMAX), un 2×2 mixte n'irrigue plus et,
 * une fois par relevé et par île, retire une brique. --causales 1 : le
 * signal aux siens déplace une nourriture vers un voisin de l'espèce ;
 * le signal aux autres pose une marque lisible dans le stimulus.
 * --saison 1 : le facteur de saison gate la ferme et la technologie
 * (creux seulement ; amplitude 0 = toutes les cases). --tronc 1 : au
 * relevé, charge les .so neufs de <dir>/lois (loi.h). Ces quatre
 * drapeaux sont hors Params. 0 = aucun effet, aucun tirage. Dès que
 * l'un est allumé, l'écriture est OADNTER9.
 * CMAX 4096 : le plafond par île se lève via --max-pop (choc à la reprise).
 * Les checkpoints OADNTER4 (World à 1024), OADNTER5 et OADNTER6 se relisent ;
 * l'écriture sans traits est OADNTER7 (chaleur et écho par case). Attention
 * tient dans le bourrage de Params : un OADNTER6 relu garde attention = 0.
 *
 *   ./experiments/terrarium_run run --dir D [options]   (reprend tout seul depuis D/state.ck)
 *   ./experiments/terrarium_run report D [D...]          (résumé des séries)
 *   ./experiments/terrarium_run selftest */
#define _GNU_SOURCE
#include <dirent.h>
#include <dlfcn.h>
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
#include "tronc-ligne/loi.h"

/* Points d'extension à la compilation pour un ELF expérimental séparé.
 * Sans définition par l'incluant : aucun état, appel, tirage ni octet de checkpoint. */
#ifndef TERRA_BEFORE_WORLD
#define TERRA_BEFORE_WORLD(I) ((void)0)
#endif
#ifndef TERRA_AFTER_WORLD
#define TERRA_AFTER_WORLD(I) ((void)0)
#endif
#ifndef TERRA_AFTER_HOOK
#define TERRA_AFTER_HOOK(I, c) ((void)0)
#endif
#ifndef TERRA_AFTER_SAMPLE
#define TERRA_AFTER_SAMPLE(T) ((void)0)
#endif
#ifndef TERRA_SAVE_EXTRA
#define TERRA_SAVE_EXTRA(f, T) 1
#endif
#ifndef TERRA_LOAD_EXTRA
#define TERRA_LOAD_EXTRA(f, T) 1
#endif

void world_eat(World *w, Creature *c);

#define KMAX    32
#define BRK_NEED 64         /* briques sur une île pour ouvrir un nouvel espace */
#define BRK_PAY  32         /* coût en briques (on les consomme) */
#define PERSIST 10          /* échantillons consécutifs pour « persister » */
#define COMPK   5           /* longueur des composants (k-grammes d'opcodes) : 3 sature l'espace (~30k), 5 ≈ 28M */
#define CLONES  3           /* … et au moins CLONES porteurs simultanés à un moment (pas un seul individu qui vit vieux) */
#define RING    96
#define NNICHE  243         /* 3^5 : bouge, mange, tourne, divise, émet — grille fixe */
#define NREL    6           /* relations qui ne sont pas un opcode */
#define NFACT   (OP_COUNT + NREL)
#define NKEEP   8           /* caractéristiques tenues en même temps : le masque de niche tient sur 8 bits */
#define DNICHE  256
#define TRAIT_LO 0.05
#define TRAIT_HI 0.95
#define TRAIT_CORR 0.80
_Static_assert(NKEEP <= 8, "masque de niche");
_Static_assert(OP_COUNT == 32, "un bit par opcode");
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
    int forces;                 /* 0 héritage ; 1 = espèce ATTACK ; 2 = + causes ; 3 = + briques (EMIT pose, recettes, espace) */
    int expand_every;           /* 0 = K fixe ; N = +1 île vide tous les N ticks (horloge), jusqu'à KMAX */
    int stress;                 /* OADNTER6, en queue. 0 = loi d'avant ; 1 = mutation ∝ stress, espèce = masque */
    int attention;              /* OADNTER7, dans le bourrage. 0 = loi d'avant ; 1 = le monde répond au choix */
} Params;
_Static_assert(offsetof(Params, stress) == 408, "stress bouge");
_Static_assert(offsetof(Params, attention) == 412, "attention hors bourrage");
_Static_assert(sizeof(Params) == 416, "Params a grandi");
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
/* une paire : action a, action précédente p, signal livré s, signal non livré n (0..15).
 * base 0 = canal poolé (selftest). base 4 = intra-espèce, base 8 = inter. Même table, autres niveaux. */
static void soc_code_at(Tab *T, Soc *S, int a, int p, int s, int n, int base) {
    uint32_t *n0 = tab_get(T, KEY(base + 0, 0, a)), *N0 = tab_get(T, KEY(base + 0, 0, 4095));
    double P0 = (*n0 + 0.5) / (*N0 + 0.5 * ASYM);
    uint32_t *n1 = tab_get(T, KEY(base + 1, p, a)), *N1 = tab_get(T, KEY(base + 1, p, 4095));
    double P1 = (*n1 + CONC * P0) / (*N1 + CONC);
    uint32_t *nr = tab_get(T, KEY(base + 2, p * 16 + s, a)), *Nr = tab_get(T, KEY(base + 2, p * 16 + s, 4095));
    uint32_t *nn = tab_get(T, KEY(base + 3, p * 16 + n, a)), *Nn = tab_get(T, KEY(base + 3, p * 16 + n, 4095));
    double Pr = (*nr + CONC * P1) / (*Nr + CONC), Pn = (*nn + CONC * P1) / (*Nn + CONC);
    S->L0 -= log2(P1); S->Lr -= log2(Pr); S->Ln -= log2(Pn); S->pairs++;
    (*n0)++; (*N0)++; (*n1)++; (*N1)++; (*nr)++; (*Nr)++; (*nn)++; (*Nn)++;
}
static void soc_code(Tab *T, Soc *S, int a, int p, int s, int n) { soc_code_at(T, S, a, p, s, n, 0); }

/* ---- île ---- */
typedef struct {
    World *w; Rng wr, tr; int env, idx;
    Tab tab; Soc soc;
    long births, deaths, novel, emits0;
    int em_n[GRID_H][GRID_W]; uint8_t em_v[GRID_H][GRID_W][4]; int em_id[GRID_H][GRID_W][4];
    uint8_t em_sp[GRID_H][GRID_W][4];             /* espèce de l'émetteur, même case que em_v (pas de tirage) */
    Soc soc_in, soc_out;                          /* excès intra / inter, niveaux 4 et 8 de la même table */
    double mut_sum; long mut_n;                   /* taux appliqué aux naissances de la fenêtre */
    int snap_id[CMAX]; uint64_t snap_fh[CMAX]; int nsnap;
    uint64_t *sh; int nsh; uint64_t sh_next;         /* ombre neutre : une étiquette par individu */
    /* ombre neutre au niveau composant : un génome par individu, mêmes naissances/morts, même mutation, sans sélection */
    struct SG { int species, len; uint8_t dna[MAX_LEN]; } *sg; int nsg; Rng sr;
    Rng rr;                                           /* copie réelle + prédateurs (jamais tiré dans le mode d'origine) */
    int npred, px[16], py[16];
    long rep_try, rep_ok, pred_hits;
    long cause_yield, cause_attr, cause_rep, cause_drain;
    int celln[GRID_H][GRID_W]; Creature *cellv[GRID_H][GRID_W][4];
    uint8_t brick[GRID_H][GRID_W];                    /* bit0 pose, 1 YIELD, 2 XOR, 3 ATTACK, 4 LOOP, 5 NEURAL, 6 SENSE */
    long brick_place, brick_farm, brick_expand, brick_tech;
    long brick_spend, partage, marques;
    uint8_t att[GRID_H][GRID_W];                      /* chaleur déposée par le processus de choix, 0..15 */
    uint8_t echo[GRID_H][GRID_W];                     /* opcode du dernier choix, 0 si hésitation */
    uint8_t mark[GRID_H][GRID_W];                     /* causales : quartet de l'autre espèce, 0 si rien */
    long attn_depth, attn_choice, attn_hes, attn_yield;
    const Params *P;
    int traits;                                   /* copie de Terra.traits pour le thread. 0 = loi d'avant */
    int depense, causales, saison;
    long t_end;
} Island;

static __thread Island *cur;

/* Creux de saison : 1 si la case reçoit la pompe. Amplitude 0 ou drapeau
 * éteint : toujours 1. Au-dessus de 1, la pompe ne dépasse pas son plein. */
static int pompe_passe(const Island *I, int x, int y) {
    if (!I->saison) return 1;
    const Params *P = I->P;
    if (P->season_amp <= 0 || P->season_period < 1) return 1;
    double f = 1.0 + P->season_amp * sin(2.0 * M_PI * (double)I->w->t / (double)P->season_period);
    if (f < 0) f = 0;
    int milli = (int)lround(f * 1000.0);
    if (milli >= 1000) return 1;
    if (milli <= 0) return 0;
    int h = (x * 17 + y * 31 + (int)I->w->t) % 1000;
    if (h < 0) h += 1000;
    return h < milli;
}

/* Signal du tick. Aux siens : une nourriture se déplace vers un voisin
 * de l'espèce. Aux autres : une marque (1..15) sur la case. Aucun tirage. */
static void causales_appliquer(Island *I, Creature *c, int sig, int esp) {
    World *w = I->w;
    if (!sig || c->species < 0 || c->species > 255) return;
    if (esp == c->species) {
        int W = w->cfg.w, H = w->cfg.h;
        if (w->food[c->y][c->x] <= 0) return;
        for (int d = 0; d < 4; d++) {
            int x = (c->x + TDX[d] + W) % W, y = (c->y + TDY[d] + H) % H;
            int hit = 0;
            for (int q = 0; q < I->celln[y][x]; q++) {
                Creature *o = I->cellv[y][x][q];
                if (o && o != c && o->species == c->species) { hit = 1; break; }
            }
            if (hit && w->food[y][x] < 14) {
                w->food[c->y][c->x]--;
                w->food[y][x]++;
                I->partage++;
                return;
            }
        }
    } else if (esp) {
        int nib = esp & 15; if (!nib) nib = 1;
        I->mark[c->y][c->x] = (uint8_t)nib;
        I->marques++;
    }
}

/* opcodes du germen (pas un immédiat) : la force naît de la seed */
static int tape_scan(const uint8_t *g, int len, int has[OP_COUNT]) {
    memset(has, 0, sizeof(int) * OP_COUNT);
    for (int i = 0; i < len; i++) {
        uint8_t op = g[i];
        if (op < OP_COUNT) has[op] = 1;
        if (op_has_imm(op) && i + 1 < len) i++;
    }
    return has[OP_ATTACK];
}
static int tape_has_attack(const uint8_t *g, int len) {
    int has[OP_COUNT];
    return tape_scan(g, len, has);
}
/* espèce = rôles réellement codés. LOOP et SENSE sont presque fixes, ils ne séparent pas. */
static int role_mask(const uint8_t *g, int len) {
    int has[OP_COUNT];
    tape_scan(g, len, has);
    int m = 0;
    if (has[OP_ATTACK]) m |= 1;
    if (has[OP_YIELD])  m |= 2;
    if (has[OP_EMIT])   m |= 4;
    if (has[OP_XOR])    m |= 8;
    if (has[OP_AND])    m |= 16;
    if (has[OP_SUB])    m |= 32;
    if (has[OP_NEURAL]) m |= 64;
    if (has[OP_ADD])    m |= 128;
    return m;
}
static double stress_scale(double stress) {
    if (stress < 0) stress = 0;
    if (stress > 1) stress = 1;
    return 1.0 + 7.0 * stress;
}
/* énergie basse, case pauvre, voisinage dense. Aucun tirage. */
static double stress_of(const World *w, const Creature *c) {
    int e = c->e; if (e < 0) e = 0; if (e > 220) e = 220;
    int f = w->food[c->y][c->x]; if (f > 14) f = 14;
    int nb = neighbors(w, c); if (nb < 0) nb = 0; if (nb > 8) nb = 8;
    double s = ((1.0 - (double)e / 220.0) + (1.0 - (double)f / 14.0) + ((double)nb / 8.0)) / 3.0;
    if (s < 0) s = 0;
    if (s > 1) s = 1;
    return s;
}
static int ham_bytes(const uint8_t *a, int na, const uint8_t *b, int nb) {
    int n = na < nb ? na : nb, d = na > nb ? na - nb : nb - na;
    for (int i = 0; i < n; i++) d += a[i] != b[i];
    return d;
}
/* N=0 E=1 S=2 W=3 — tore */
static int dir_to(int x0, int y0, int x1, int y1, int W, int H) {
    int dx = x1 - x0, dy = y1 - y0;
    if (dx >  W / 2) dx -= W;
    if (dx < -W / 2) dx += W;
    if (dy >  H / 2) dy -= H;
    if (dy < -H / 2) dy += H;
    if (abs(dx) >= abs(dy)) return dx > 0 ? 1 : dx < 0 ? 3 : (dy > 0 ? 2 : 0);
    return dy > 0 ? 2 : 0;
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

static int attn_is_commit(uint8_t op) {
    return op == OP_STEP || op == OP_TURN || op == OP_EAT || op == OP_SPLIT
        || op == OP_EMIT || op == OP_ATTACK || op == OP_YIELD;
}
/* opcodes exécutés avant le premier choix. win[15] déjà posé : la suite ne compte plus. */
static void attn_watch(Creature *c, uint8_t op) {
    if (c->win[15]) return;
    if (attn_is_commit(op)) c->win[15] = op;
    else if (c->win[14] < 255) c->win[14]++;
}
/* opcodes réellement exécutés. win0[8..11] = bits, win0[12] = relations. La VM ne les lit pas. */
static void trait_watch(Creature *c, uint8_t op) {
    if (op < OP_COUNT) {
        uint32_t bits;
        memcpy(&bits, &c->win0[8], 4);
        bits |= 1u << op;
        memcpy(&c->win0[8], &bits, 4);
    }
    if (cur && cur->P->attention) attn_watch(c, op);
}
static void hook(World *w, Creature *c) {
    Island *I = cur;
    if (I->P->attention) {                        /* relevé du processus, case de départ */
        c->win[14] = 0; c->win[15] = 0;
        c->win[26] = (uint8_t)c->x; c->win[27] = (uint8_t)c->y;
    }
    if (I->P->stress) {                               /* espèce = masque de rôle du germen */
        int sp = role_mask(c->germ, c->germ_len);
        if (c->species != sp) { c->species = sp; if (c->ls[4]) genotype(c); }
    } else if (I->P->forces) {                        /* forces : l'espèce suit le germen, pas un --pred collé */
        int sp = tape_has_attack(c->germ, c->germ_len) ? 1 : 0;
        if (c->species != sp) { c->species = sp; if (c->ls[4]) genotype(c); }
    }
    if (I->P->osmo) {                                 /* osmotrophie : avant le relevé, donc « a mangé » = EAT exécuté */
        int f = w->food[c->y][c->x], take = f < I->P->osmo ? f : I->P->osmo;
        if (take) { w->food[c->y][c->x] = (uint8_t)(f - take); c->e = c->e + take * 5 > 220 ? 220 : c->e + take * 5; }
    }
    if (I->P->forces >= 2) {                          /* causes : l'octet du germen fait un effet au monde */
        int has[OP_COUNT]; tape_scan(c->germ, c->germ_len, has);
        int W = w->cfg.w, H = w->cfg.h;
        Creature *sim = NULL, *dif = NULL; int best_s = 1 << 20, best_d = -1;
        for (int dy = -1; dy <= 1; dy++) for (int dx = -1; dx <= 1; dx++) {
            int y = (c->y + dy + H) % H, x = (c->x + dx + W) % W;
            for (int k = 0; k < I->celln[y][x]; k++) {
                Creature *o = I->cellv[y][x][k];
                if (o == c || o->e <= 0) continue;
                int h = ham_bytes(c->germ, c->germ_len, o->germ, o->germ_len);
                if (h < best_s) { best_s = h; sim = o; }
                if (h > best_d) { best_d = h; dif = o; }
            }
        }
        if (has[OP_YIELD] && c->e > 10) {
            uint8_t *f = &w->food[c->y][c->x];
            if (*f < 14) { (*f)++; c->e -= 1; I->cause_yield++; }
        }
        if ((has[OP_AND] || has[OP_ADD]) && sim) { c->d = dir_to(c->x, c->y, sim->x, sim->y, W, H); I->cause_attr++; }
        if (has[OP_XOR] && dif) { c->d = (dir_to(c->x, c->y, dif->x, dif->y, W, H) + 2) & 3; I->cause_rep++; }
        if (has[OP_SUB] && dif && dif->e > 1) {
            dif->e -= 1; c->e = c->e + 1 > 220 ? 220 : c->e + 1; I->cause_drain++;
        }
    }
    if (I->P->forces >= 3) {                          /* briques : EMIT pose une pierre ; la recette est les autres octets */
        int has[OP_COUNT]; tape_scan(c->germ, c->germ_len, has);
        if (has[OP_EMIT] && c->e > 12 && I->brick[c->y][c->x] == 0) {
            uint8_t t = 1;
            if (has[OP_YIELD]) t |= 2;
            if (has[OP_XOR]) t |= 4;
            if (has[OP_ATTACK]) t |= 8;
            if (has[OP_LOOP]) t |= 16;
            if (has[OP_NEURAL]) t |= 32;
            if (has[OP_SENSE]) t |= 64;
            I->brick[c->y][c->x] = t;
            c->e -= 2;
            I->brick_place++;
        }
        uint8_t b = I->brick[c->y][c->x];
        if ((b & 16) && c->e < 160) c->e++;           /* LOOP : sol motorisé, annule le métabolisme */
        /* NEURAL (bit 32) : puits dans island_tick, une fois par case, comme la ferme YIELD */
    }
    c->ls[0] = c->steps; c->ls[1] = c->eaten; c->ls[2] = c->splits; c->ls[3] = c->d; c->ls[5] = (int)w->t;
    int cand_v[36], cand_sp[36], m = 0, W = w->cfg.w, H = w->cfg.h;
    for (int dy = -1; dy <= 1; dy++)
        for (int dx = -1; dx <= 1; dx++) {
            int y = (c->y + dy + H) % H, x = (c->x + dx + W) % W;
            for (int k = 0; k < I->em_n[y][x]; k++)
                if (I->em_id[y][x][k] != c->id) {
                    cand_sp[m] = I->em_sp[y][x][k];
                    cand_v[m++] = I->em_v[y][x][k];
                }
        }
    int sig = 0, nsig = 0, esp = 0;
    if (m >= 1) {
        int i = (int)rng_below(&I->tr, (uint32_t)m);
        sig = cand_v[i] + 1; esp = cand_sp[i];
        if (m >= 2) { int j = (int)rng_below(&I->tr, (uint32_t)(m - 1)); if (j >= i) j++; nsig = cand_v[j] + 1; }
    }
    if (I->P->stress) c->win[31] = (uint8_t)esp;
    c->win[0] = (uint8_t)sig; c->win[1] = (uint8_t)nsig;
    int low = (I->P->channel && sig) ? sig - 1 : I->env, high = I->env;
    if (I->P->attention) {
        uint8_t heat = I->att[c->y][c->x];
        if (heat) {
            uint8_t echo = I->echo[c->y][c->x];
            high = echo ? (echo & 15) : (heat & 15);
        }
    }
    if (I->causales && I->P->stress) causales_appliquer(I, c, sig, esp);
    if (I->causales && I->mark[c->y][c->x]) high = I->mark[c->y][c->x] & 15;
    if (I->npred) {                                   /* zone de mort : Chebyshev ≤ 1 (tore) ; perçue directement (quartet haut = 15) */
        for (int q = 0; q < I->npred; q++) {
            int dx = abs(c->x - I->px[q]), dy = abs(c->y - I->py[q]);
            if (dx > W - dx) dx = W - dx;
            if (dy > H - dy) dy = H - dy;
            if (dx <= 1 && dy <= 1) { high = 15; c->e -= I->P->pred_dmg; I->pred_hits++; }
        }
    }
    w->stimulus = (high << 4) | low;
    if (I->P->stress) world_set_mut_scale(stress_scale(stress_of(w, c)));
    TERRA_AFTER_HOOK(I, c);
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
    double rate = P->rep_mut;
    if (P->stress) rate *= stress_scale(stress_of(w, c));
    for (int j = 0; j < SOUP_PROG; j++) if (rng_double(&I->rr) < rate) child[j] = (uint8_t)rng_next(&I->rr);
    int dist = 0; for (int j = 0; j < SOUP_PROG; j++) dist += child[j] != old[po + j];
    int d = (c->d + 1) & 3, give = c->e / 3;
    int sp = P->stress ? role_mask(child, SOUP_PROG) : P->forces ? (tape_has_attack(child, SOUP_PROG) ? 1 : 0) : c->species;
    Creature *k = creature_new((c->x + TDX[d] + w->cfg.w) % w->cfg.w, (c->y + TDY[d] + w->cfg.h) % w->cfg.h,
                               child, SOUP_PROG, give, w->nid++, sp, c->id);
    k->win[30] = (uint8_t)(dist > 255 ? 255 : dist);
    int nb0 = w->nnb; world_spawn(w, k);
    if (w->nnb > nb0) {
        c->splits++; c->e -= give + 8; I->rep_ok++; w->st.born++;
        if (P->stress) { I->mut_sum += rate; I->mut_n++; }
    }
}

static int find_parent(const Island *I, int id) {
    int lo = 0, hi = I->nsnap - 1;
    while (lo <= hi) { int mid = (lo + hi) / 2; if (I->snap_id[mid] == id) return mid; if (I->snap_id[mid] < id) lo = mid + 1; else hi = mid - 1; }
    return -1;
}

/* La chaleur du tick précédent est déjà lisible. On la laisse refroidir, puis le tick
 * qui vient de finir dépose la sienne. Un choix ne répond que si la case est chaude. */
static void attn_apply(Island *I) {
    World *w = I->w; int H = w->cfg.h, W = w->cfg.w;
    for (int y = 0; y < H; y++) for (int x = 0; x < W; x++)
        if (I->att[y][x] && --I->att[y][x] == 0) I->echo[y][x] = 0;
    for (int i = 0; i < w->ncs; i++) {
        Creature *c = w->cs[i];
        if (c->ls[5] != (int)w->t) continue;
        int depth = c->win[14], commit = c->win[15];
        int y = c->win[27], x = c->win[26];
        if (y < 0 || x < 0 || y >= H || x >= W) continue;
        I->attn_depth += depth;
        int heat = I->att[y][x] + (depth > 15 ? 15 : depth);
        if (heat > 15) heat = 15;
        I->att[y][x] = (uint8_t)heat;
        if (!commit) {
            if (depth) { I->echo[y][x] = 0; I->attn_hes++; }
            continue;
        }
        if (commit == OP_YIELD) {
            if (I->att[y][x]) { I->echo[y][x] = (uint8_t)OP_YIELD; I->attn_yield++; }
            continue;
        }
        if (!I->att[y][x]) continue;
        I->att[y][x]--;
        I->echo[y][x] = I->att[y][x] ? (uint8_t)commit : 0;
        I->attn_choice++;
        switch (commit) {
        case OP_STEP:
            if (w->food[c->y][c->x] < 14) w->food[c->y][c->x]++;
            break;
        case OP_TURN: {
            int nx = (x + TDX[c->d] + W) % W, ny = (y + TDY[c->d] + H) % H;
            if (I->att[ny][nx] < 15) I->att[ny][nx]++;
            break;
        }
        case OP_EAT: {
            int bd = (c->d + 2) & 3;
            int nx = (x + TDX[bd] + W) % W, ny = (y + TDY[bd] + H) % H;
            if (w->food[ny][nx] < 14) w->food[ny][nx]++;
            break;
        }
        case OP_EMIT:
            for (int d = 0; d < 4; d++) {
                int nx = (x + TDX[d] + W) % W, ny = (y + TDY[d] + H) % H;
                if (I->att[ny][nx] < 15) I->att[ny][nx]++;
            }
            break;
        case OP_SPLIT: {
            int bd = (c->d + 1) & 3;
            int nx = (x + TDX[bd] + W) % W, ny = (y + TDY[bd] + H) % H;
            int v = I->att[ny][nx] + 2; if (v > 15) v = 15;
            I->att[ny][nx] = (uint8_t)v;
            break;
        }
        case OP_ATTACK:
            I->att[y][x] = (uint8_t)(I->att[y][x] / 2);
            break;
        }
    }
}

static void briques_pompes(Island *I) {
    World *w = I->w; int W = w->cfg.w, H = w->cfg.h;
    for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) {
        uint8_t b = I->brick[y][x];
        if (!b) continue;
        if ((b & 2) && w->food[y][x] < 14 && pompe_passe(I, x, y)) { w->food[y][x]++; I->brick_farm++; }
        if ((b & 32) && w->food[y][x] < 8 && pompe_passe(I, x, y)) { w->food[y][x]++; I->brick_farm++; }
    }
    int spent = 0;
    int plein = I->depense && I->P->K >= KMAX;
    int periode = I->P->sample > 0 ? I->P->sample : 1;
    for (int y = 0; y < H - 1; y++) for (int x = 0; x < W - 1; x++) {
        uint8_t a = I->brick[y][x], b = I->brick[y][x + 1], c2 = I->brick[y + 1][x], d = I->brick[y + 1][x + 1];
        if (!a || !b || !c2 || !d) continue;
        if (a == b && a == c2 && a == d) continue;
        I->brick_tech++;
        if (plein) {
            if (!spent && (w->t % periode) == 0) {
                I->brick[y][x] = 0;
                I->brick_spend++;
                spent = 1;
            }
        } else {
            if (w->food[y][x] < 14 && pompe_passe(I, x, y)) w->food[y][x]++;
            if (w->food[y][x + 1] < 14 && pompe_passe(I, x + 1, y)) w->food[y][x + 1]++;
            if (w->food[y + 1][x] < 14 && pompe_passe(I, x, y + 1)) w->food[y + 1][x]++;
            if (w->food[y + 1][x + 1] < 14 && pompe_passe(I, x + 1, y + 1)) w->food[y + 1][x + 1]++;
        }
    }
}

static void island_tick(Island *I) {
    cur = I;
    World *w = I->w; const Params *P = I->P;
    if (rng_below(&I->tr, 50) == 0) I->env = (int)rng_below(&I->tr, P->pred ? 15 : 16);   /* 15 = danger, réservé */
    if (I->npred && w->t % P->pred_every == 0) pred_move(I);
    if (P->season_amp > 0) {
        double f = 1 + P->season_amp * sin(2 * M_PI * (double)w->t / P->season_period);
        w->cfg.food_regrow = (int)lround(P->regrow * f);
    }
    memset(I->em_n, 0, sizeof I->em_n);
    memset(I->celln, 0, sizeof I->celln);
    I->nsnap = w->ncs;
    for (int i = 0; i < w->ncs; i++) {
        Creature *c = w->cs[i];
        I->snap_id[i] = c->id; I->snap_fh[i] = FH(c);
        if (c->emitted >= 0 && I->em_n[c->y][c->x] < 4) {
            int k = I->em_n[c->y][c->x]++;
            I->em_v[c->y][c->x][k] = (uint8_t)(c->emitted & 15); I->em_id[c->y][c->x][k] = c->id;
            I->em_sp[c->y][c->x][k] = (uint8_t)(c->species < 0 ? 0 : c->species > 255 ? 255 : c->species);
        }
        if (I->celln[c->y][c->x] < 4) I->cellv[c->y][c->x][I->celln[c->y][c->x]++] = c;
    }
    int n_before = w->ncs;
    if (I->traits) vm_set_watch(trait_watch);
    else if (P->attention) vm_set_watch(attn_watch);
    TERRA_BEFORE_WORLD(I);
    world_step(w);
    TERRA_AFTER_WORLD(I);
    if (I->traits || P->attention) vm_set_watch(NULL);
    if (P->attention) attn_apply(I);
    if (P->stress) {
        double ms = 0; long mn = 0;
        world_take_birth_mut(&ms, &mn);
        I->mut_sum += ms; I->mut_n += mn;
    }
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
        if (P->stress && c->win[0] && c->win[1]) {
            int intra = c->species >= 0 && c->species < 256 && c->win[31] == (uint8_t)c->species;
            Soc *S = intra ? &I->soc_in : &I->soc_out;
            if (S->pairs < SOC_CAP)
                soc_code_at(&I->tab, S, a, (int)c->lscore, c->win[0] - 1, c->win[1] - 1, intra ? 4 : 8);
        }
        if (I->traits) {
            uint8_t rel = c->win0[12];
            if (P->attention && c->win[14] && !c->win[15]) rel |= 1;
            if (P->forces >= 3) {
                uint8_t b = I->brick[c->y][c->x];
                if (b & (2 | 32)) rel |= 2;
            }
            if (P->stress && c->win[0] && c->species >= 0 && c->species < 256)
                rel |= (uint8_t)(c->win[31] == (uint8_t)c->species ? 4 : 8);
            if (c->e < 40) rel |= 16;
            if (!w->food[c->y][c->x]) rel |= 32;
            c->win0[12] = rel;
        }
        c->lscore = a + 1;
    }
    if (P->forces >= 3) briques_pompes(I);
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
    int soc_in_streak, soc_in_on; long soc_in_events;
    int soc_out_streak, soc_out_on; long soc_out_events;
    long n_events;
    long emits_prev;
    FILE *series, *events, *forces_out, *societies, *attention, *traits_out, *lois_out; char dir[512];
    int saw_sp1, saw_attn;
    /* Découverte. Hors Params et hors du corps OADNTER7 : traits 0 n'écrit rien de plus. */
    int traits;
    int trait_hi[NFACT], trait_lo[NFACT], trait_on[NFACT], trait_slot[NFACT];
    Niche dniche[DNICHE];
    Cyc cyc_dniche[DNICHE];
    /* Stimulations et tronc. Hors Params. 0 = le chemin d'avant. */
    int depense, causales, saison, tronc;
    void *loi_h[32];
    void (*loi_fn[32])(const LoiMonde *);
    char loi_nom[32][64];
    int nloi;
    int adapter_plugin_state[7][3];
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

static const char *REL_NAME[NREL] = {
    "hesite", "cultive", "entre_siens", "entre_autres", "energie_basse", "case_nue"
};
static const char *fact_name(int i) { return i < OP_COUNT ? op_name((uint8_t)i) : REL_NAME[i - OP_COUNT]; }
static int fact_on(const Creature *c, int i) {
    if (i < OP_COUNT) {
        uint32_t bits; memcpy(&bits, &c->win0[8], 4);
        return (int)((bits >> i) & 1u);
    }
    return (c->win0[12] >> (i - OP_COUNT)) & 1;
}
static void fact_clear(Creature *c) { memset(&c->win0[8], 0, 5); }
static void trait_arm(Terra *T) {
    memset(T->trait_hi, 0, sizeof T->trait_hi);
    memset(T->trait_lo, 0, sizeof T->trait_lo);
    memset(T->trait_on, 0, sizeof T->trait_on);
    for (int i = 0; i < NFACT; i++) T->trait_slot[i] = -1;
    memset(T->dniche, 0, sizeof T->dniche);
    memset(T->cyc_dniche, 0, sizeof T->cyc_dniche);
    for (int n = 0; n < DNICHE; n++) T->dniche[n].first_stable = -1;
}
/* n11[i * NFACT + j] pour i < j : co-occurrences. Retourne le nombre retenu. */
static int trait_select(const int *cnt, const int *n11, int n, int selected[NFACT]) {
    memset(selected, 0, sizeof(int) * NFACT);
    if (n <= 0) return 0;
    int order[NFACT], nb = 0;
    for (int i = 0; i < NFACT; i++) {
        double f = (double)cnt[i] / n;
        if (f >= TRAIT_LO && f <= TRAIT_HI) order[nb++] = i;
    }
    for (int a = 1; a < nb; a++) {
        int v = order[a], b = a;
        double fa = fabs((double)cnt[v] / n - 0.5);
        while (b > 0) {
            double fb = fabs((double)cnt[order[b - 1]] / n - 0.5);
            if (fb < fa || (fb == fa && order[b - 1] < v)) break;
            order[b] = order[b - 1]; b--;
        }
        order[b] = v;
    }
    int kept = 0, keep[NKEEP];
    for (int a = 0; a < nb && kept < NKEEP; a++) {
        int i = order[a], ok = 1;
        for (int k = 0; k < kept && ok; k++) {
            int j = keep[k];
            int lo = i < j ? i : j, hi = i < j ? j : i;
            int both = n11[lo * NFACT + hi];
            int n10 = cnt[i] - both, n01 = cnt[j] - both, n00 = n - both - n10 - n01;
            double d1 = (double)(both + n10) * (both + n01) * (n10 + n00) * (n01 + n00);
            double phi = d1 > 1e-9 ? (both * (double)n00 - n10 * (double)n01) / sqrt(d1) : 0;
            if (fabs(phi) > TRAIT_CORR) ok = 0;
        }
        if (!ok) continue;
        selected[i] = 1; keep[kept++] = i;
    }
    return kept;
}
static void trait_mask_name(const Terra *T, int mask, char *out, size_t cap) {
    size_t n = 0; if (cap) out[0] = 0;
    for (int i = 0; i < NFACT; i++) {
        int s = T->trait_slot[i];
        if (!T->trait_on[i] || s < 0 || !(mask & (1 << s))) continue;
        const char *nm = fact_name(i);
        size_t L = strlen(nm);
        if (n && n + 1 < cap) out[n++] = '+';
        if (n + L >= cap) break;
        memcpy(out + n, nm, L); n += L; out[n] = 0;
    }
    if (!n && cap) snprintf(out, cap, "aucun");
}
/* Fait passer les faits retenus ce relevé. Émet trait_ne / trait_perdu. */
static void trait_commit(Terra *T, const int selected[NFACT], const double *freq) {
    int non = 0;
    for (int i = 0; i < NFACT; i++) if (T->trait_on[i]) non++;
    for (int i = 0; i < NFACT; i++) {
        if (selected[i]) { T->trait_hi[i]++; T->trait_lo[i] = 0; }
        else { T->trait_lo[i]++; T->trait_hi[i] = 0; }
        if (!T->trait_on[i] && selected[i] && T->trait_hi[i] >= PERSIST && non < NKEEP) {
            int used[NKEEP] = { 0 };
            for (int j = 0; j < NFACT; j++) if (T->trait_on[j] && T->trait_slot[j] >= 0) used[T->trait_slot[j]] = 1;
            int s = 0; while (s < NKEEP && used[s]) s++;
            if (s >= NKEEP) continue;
            T->trait_on[i] = 1; T->trait_slot[i] = s; non++;
            event(T, "trait_ne", "%s freq=%.3f", fact_name(i), freq[i]);
        }
        if (T->trait_on[i] && !selected[i] && T->trait_lo[i] >= PERSIST) {
            T->trait_on[i] = 0; T->trait_slot[i] = -1; if (non) non--;
            event(T, "trait_perdu", "%s", fact_name(i));
        }
    }
}
static void trait_sample(Terra *T, long pos, int *occ_out, int *nstable_out, double *Hb_out) {
    int cnt[NFACT] = { 0 }, n11[NFACT * NFACT]; int N = 0;
    memset(n11, 0, sizeof n11);
    for (int k = 0; k < T->P.K; k++) {
        World *w = T->I[k].w;
        for (int i = 0; i < w->ncs; i++) { N++; for (int f = 0; f < NFACT; f++) cnt[f] += fact_on(w->cs[i], f); }
    }
    int band[NFACT], nb = 0;
    if (N > 0) for (int f = 0; f < NFACT; f++) {
        double q = (double)cnt[f] / N;
        if (q >= TRAIT_LO && q <= TRAIT_HI) band[nb++] = f;
    }
    for (int k = 0; k < T->P.K; k++) {
        World *w = T->I[k].w;
        for (int i = 0; i < w->ncs; i++) {
            Creature *c = w->cs[i];
            int has[NFACT];
            memset(has, 0, sizeof has);
            for (int b = 0; b < nb; b++) has[band[b]] = fact_on(c, band[b]);
            for (int a = 0; a < nb; a++) if (has[band[a]])
                for (int b = a + 1; b < nb; b++) if (has[band[b]]) {
                    int ia = band[a], ib = band[b];
                    int lo = ia < ib ? ia : ib, hi = ia < ib ? ib : ia;
                    n11[lo * NFACT + hi]++;
                }
        }
    }
    int selected[NFACT];
    trait_select(cnt, n11, N, selected);
    double freq[NFACT];
    for (int f = 0; f < NFACT; f++) freq[f] = N ? (double)cnt[f] / N : 0;
    trait_commit(T, selected, freq);
    int nh[DNICHE] = { 0 };
    for (int k = 0; k < T->P.K; k++) {
        World *w = T->I[k].w;
        for (int i = 0; i < w->ncs; i++) {
            Creature *c = w->cs[i]; int mask = 0;
            for (int f = 0; f < NFACT; f++)
                if (T->trait_on[f] && T->trait_slot[f] >= 0 && fact_on(c, f)) mask |= 1 << T->trait_slot[f];
            nh[mask]++;
            fact_clear(c);
        }
    }
    int occ = 0, nstable = 0; double Hb = 0;
    for (int n = 0; n < DNICHE; n++) {
        Niche *z = &T->dniche[n];
        double f = N ? (double)nh[n] / N : 0;
        z->ring[pos] = f;
        if (nh[n]) { occ++; if (f > 0) Hb -= f * log2(f); }
        z->hi = f >= 0.05 ? z->hi + 1 : 0;
        z->lo = f < 0.02 ? z->lo + 1 : 0;
        if (!z->stable && z->hi >= PERSIST) {
            z->stable = 1;
            if (z->first_stable < 0) z->first_stable = T->t;
            char name[160]; trait_mask_name(T, n, name, sizeof name);
            event(T, "niche_stable", "niche=%d %s frac=%.3f", n, name, f);
        }
        if (z->stable && z->lo >= PERSIST) { z->stable = 0; event(T, "niche_perdue", "niche=%d", n); }
        nstable += z->stable;
    }
    int navail = T->sample_i + 1 < RING ? (int)T->sample_i + 1 : RING;
    double pk = 0; int per;
    for (int n = 0; n < DNICHE; n++) {
        if (!T->dniche[n].stable) continue;
        Cyc *cy = &T->cyc_dniche[n];
        if ((per = acf_cycle(T->dniche[n].ring, navail, pos + 1, &pk)) && !cy->armed) {
            event(T, "cycle", "serie=niche%d periode=%d ticks acf=%.2f", n, per * T->P.sample, pk); cy->armed = 1; cy->quiet = 0;
        } else if (!per && cy->armed && ++cy->quiet >= 32) cy->armed = 0;
    }
    if (T->traits_out) {
        int non = 0; for (int f = 0; f < NFACT; f++) non += T->trait_on[f];
        fprintf(T->traits_out, "%ld\t%d\t", T->t, non);
        int any = 0;
        for (int f = 0; f < NFACT; f++) if (T->trait_on[f]) {
            fprintf(T->traits_out, "%s%s=%.3f", any ? "," : "", fact_name(f), freq[f]); any = 1;
        }
        if (!any) fputs("-", T->traits_out);
        fputc('\t', T->traits_out); any = 0;
        for (int f = 0; f < NFACT; f++) if (selected[f]) {
            fprintf(T->traits_out, "%s%s=%.3f", any ? "," : "", fact_name(f), freq[f]); any = 1;
        }
        if (!any) fputs("-", T->traits_out);
        fputc('\n', T->traits_out);
        fflush(T->traits_out);
    }
    *occ_out = occ; *nstable_out = nstable; *Hb_out = Hb;
}

static Terra *loi_courant;
static void loi_evt(const char *type, const char *detail) {
    if (loi_courant && type && detail) event(loi_courant, type, "%s", detail);
}
static int loi_ile_ok(int ile, int x, int y, Island **out) {
    Terra *T = loi_courant;
    if (!T || ile < 0 || ile >= T->P.K) return 0;
    Island *I = &T->I[ile];
    if (!I->w || x < 0 || y < 0 || x >= I->w->cfg.w || y >= I->w->cfg.h) return 0;
    *out = I;
    return 1;
}
static int loi_lire(int ile, int x, int y) {
    Island *I; if (!loi_ile_ok(ile, x, y, &I)) return 0; return I->w->food[y][x];
}
static void loi_food(int ile, int x, int y, int delta) {
    Island *I; if (!loi_ile_ok(ile, x, y, &I)) return;
    int v = I->w->food[y][x] + delta; if (v < 0) v = 0; if (v > 14) v = 14; I->w->food[y][x] = (uint8_t)v;
}
static void loi_brique(int ile, int x, int y) {
    Island *I; if (!loi_ile_ok(ile, x, y, &I)) return; I->brick[y][x] = 0;
}
static int loi_brique_lire(int ile, int x, int y) {
    Island *I; if (!loi_ile_ok(ile, x, y, &I)) return 0; return I->brick[y][x];
}
static void loi_chaleur(int ile, int x, int y, int delta) {
    Island *I; if (!loi_ile_ok(ile, x, y, &I)) return;
    int v = I->att[y][x] + delta; if (v < 0) v = 0; if (v > 15) v = 15; I->att[y][x] = (uint8_t)v;
}
static void loi_marque(int ile, int x, int y, int valeur) {
    Island *I; if (!loi_ile_ok(ile, x, y, &I)) return; I->mark[y][x] = (uint8_t)(valeur & 15);
}
static void lois_brancher(Terra *T) {
    if (!T->tronc || !T->dir[0]) return;
    char dir[600]; snprintf(dir, sizeof dir, "%s/lois", T->dir);
    DIR *d = opendir(dir); if (!d) return;
    struct dirent *de;
    while ((de = readdir(d))) {
        const char *nm = de->d_name; size_t n = strlen(nm);
        if (n < 4 || strcmp(nm + n - 3, ".so") != 0) continue;
        int deja = 0;
        for (int i = 0; i < T->nloi; i++) if (!strcmp(T->loi_nom[i], nm)) deja = 1;
        if (deja) continue;
        if (T->nloi >= 32) { event(T, "loi_rejet", "%s : 32 lois deja chargees", nm); continue; }
        char full[700]; snprintf(full, sizeof full, "%s/%s", dir, nm);
        void *h = dlopen(full, RTLD_NOW | RTLD_LOCAL);
        if (!h) { const char *e = dlerror(); event(T, "loi_rejet", "%s", e ? e : nm); continue; }
        const Loi *(*reg)(void) = (const Loi *(*)(void))dlsym(h, "loi_registre");
        const Loi *L = reg ? reg() : NULL;
        if (!L || L->version != 1 || !L->echantillon || !L->nom) {
            event(T, "loi_rejet", "%s : registre absent ou version autre que 1", nm);
            dlclose(h); continue;
        }
        int i = T->nloi++;
        T->loi_h[i] = h; T->loi_fn[i] = L->echantillon;
        int pi=adapter_plugin_index(nm);void(*restore)(const int*)=dlsym(h,"adapter_state_set");if(pi>=0&&restore)restore(T->adapter_plugin_state[pi]);
        snprintf(T->loi_nom[i], sizeof T->loi_nom[i], "%s", nm);
        event(T, "loi", "%s branchee", L->nom);
    }
    closedir(d);
    if (!T->nloi) return;
    LoiMonde M = { .tick = T->t, .pop = total_pop(T), .K = T->P.K, .evenement = loi_evt,
                   .lire_nourriture = loi_lire, .ajouter_nourriture = loi_food, .retirer_brique = loi_brique,
                   .ajouter_chaleur = loi_chaleur, .poser_marque = loi_marque,
                   .lire_brique = loi_brique_lire };
    loi_courant = T;
    for (int i = 0; i < T->nloi; i++) {T->loi_fn[i](&M);int pi=adapter_plugin_index(T->loi_nom[i]);void(*capture)(int*)=dlsym(T->loi_h[i],"adapter_state_get");if(pi>=0&&capture)capture(T->adapter_plugin_state[pi]);}
    loi_courant = NULL;
}

static void sample(Terra *T) {
    const Params *P = &T->P;
    int N = total_pop(T);
    uint64_t *keys = malloc(sizeof(uint64_t) * (size_t)(N + 1)), *raw = malloc(sizeof(uint64_t) * (size_t)(N + 1));
    double len_sum = 0, cod_sum = 0; int len_max = 0, j = 0;
    int nh[NNICHE] = { 0 }, ncls = 0;
    long births = 0, deaths = 0, emits = 0, pairs = 0; double L0 = 0, Lr = 0, Ln = 0;
    int spc[256]; memset(spc, 0, sizeof spc);
    long pin = 0, pout = 0; double iL0 = 0, iLr = 0, iLn = 0, oL0 = 0, oLr = 0, oLn = 0;
    double mut_sum = 0; long mut_n = 0;
    long depth_s = 0, choice_s = 0, hes_s = 0, yield_s = 0; int att_sum = 0, echo_n = 0;
    for (int k = 0; k < P->K; k++) {
        Island *I = &T->I[k]; World *w = I->w;
        for (int i = 0; i < w->ncs; i++) {
            Creature *c = w->cs[i];
            keys[j] = FH(c); raw[j] = RAWH(c); j++;
            if ((unsigned)c->species < 256) spc[c->species]++;
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
        pin += I->soc_in.pairs; iL0 += I->soc_in.L0; iLr += I->soc_in.Lr; iLn += I->soc_in.Ln;
        pout += I->soc_out.pairs; oL0 += I->soc_out.L0; oLr += I->soc_out.Lr; oLn += I->soc_out.Ln;
        mut_sum += I->mut_sum; mut_n += I->mut_n; I->mut_sum = 0; I->mut_n = 0;
        depth_s += I->attn_depth; choice_s += I->attn_choice; hes_s += I->attn_hes; yield_s += I->attn_yield;
        I->attn_depth = I->attn_choice = I->attn_hes = I->attn_yield = 0;
        if (P->attention) {
            int H = w->cfg.h, W = w->cfg.w;
            for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) {
                att_sum += I->att[y][x]; echo_n += I->echo[y][x] != 0;
            }
        }
        memset(&I->soc, 0, sizeof I->soc); memset(&I->soc_in, 0, sizeof I->soc_in); memset(&I->soc_out, 0, sizeof I->soc_out);
        tab_clear(&I->tab);
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
    /* niches : grille fixe, ou masque des caractéristiques découvertes */
    long pos = T->sample_i % RING; int occ = 0, nstable = 0; double Hb = 0;
    if (T->traits) trait_sample(T, pos, &occ, &nstable, &Hb);
    else for (int n = 0; n < NNICHE; n++) {
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
    if (!T->traits) for (int n = 0; n < NNICHE; n++) {
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
    if (T->societies) {
        int nsp = 0; double Hs = 0;
        if (N > 0) for (int s = 0; s < 256; s++) if (spc[s]) { nsp++; double p = (double)spc[s] / N; Hs -= p * log2(p); }
        double ex_in = pin ? ((iL0 - iLr) - (iL0 - iLn)) / pin : 0;
        double ex_out = pout ? ((oL0 - oLr) - (oL0 - oLn)) / pout : 0;
        if (pin >= 1000 && ex_in > P->soc_thr) T->soc_in_streak++; else T->soc_in_streak = 0;
        if (T->soc_in_streak >= 3 && !T->soc_in_on) {
            T->soc_in_on = 1; T->soc_in_events++;
            event(T, "societe_intra", "exces=%.4f bits/paire paires=%ld (3 fenetres > %.3f)", ex_in, pin, P->soc_thr);
        }
        if (T->soc_in_streak == 0) T->soc_in_on = 0;
        if (pout >= 1000 && ex_out > P->soc_thr) T->soc_out_streak++; else T->soc_out_streak = 0;
        if (T->soc_out_streak >= 3 && !T->soc_out_on) {
            T->soc_out_on = 1; T->soc_out_events++;
            event(T, "societe_inter", "exces=%.4f bits/paire paires=%ld (3 fenetres > %.3f)", ex_out, pout, P->soc_thr);
        }
        if (T->soc_out_streak == 0) T->soc_out_on = 0;
        fprintf(T->societies, "%ld\t%d\t%.4f\t%.5f\t%ld\t%.5f\t%ld\t%.5f\n",
                T->t, nsp, Hs, mut_n ? mut_sum / mut_n : -1.0, pin, ex_in, pout, ex_out);
        fflush(T->societies);
    }
    if (T->attention) {
        fprintf(T->attention, "%ld\t%d\t%d\t%ld\t%ld\t%ld\t%ld\n",
                T->t, att_sum, echo_n, depth_s, choice_s, hes_s, yield_s);
        fflush(T->attention);
        if (choice_s && !T->saw_attn) {
            T->saw_attn = 1;
            event(T, "attention", "choix=%ld profondeur=%ld hesitations=%ld yields=%ld chaleur=%d",
                  choice_s, depth_s, hes_s, yield_s, att_sum);
        }
    }
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
        long yld = 0, attr = 0, rep = 0, drn = 0;
        for (int k = 0; k < P->K; k++) {
            yld += T->I[k].cause_yield; attr += T->I[k].cause_attr; rep += T->I[k].cause_rep; drn += T->I[k].cause_drain;
            T->I[k].cause_yield = T->I[k].cause_attr = T->I[k].cause_rep = T->I[k].cause_drain = 0;
        }
        long plc = 0, frm = 0, expn = 0, tec = 0; int bricks = 0;
        for (int k = 0; k < P->K; k++) {
            Island *I = &T->I[k];
            plc += I->brick_place; frm += I->brick_farm; expn += I->brick_expand; tec += I->brick_tech;
            I->brick_place = I->brick_farm = I->brick_expand = I->brick_tech = 0;
            int H = I->w->cfg.h, W = I->w->cfg.w;
            for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) bricks += I->brick[y][x] != 0;
        }
        if (P->forces >= 3)
            fprintf(T->forces_out, "%ld\t%d\t%d\t%ld\t%d\t%d\t%d\t%ld\t%ld\t%ld\t%ld\t%d\t%ld\t%ld\t%ld\t%ld\n",
                    T->t, N, sp1, attacks, P->K, isles, food, yld, attr, rep, drn, bricks, plc, frm, expn, tec);
        else if (P->forces >= 2)
            fprintf(T->forces_out, "%ld\t%d\t%d\t%ld\t%d\t%d\t%d\t%ld\t%ld\t%ld\t%ld\n",
                    T->t, N, sp1, attacks, P->K, isles, food, yld, attr, rep, drn);
        else
            fprintf(T->forces_out, "%ld\t%d\t%d\t%ld\t%d\t%d\t%d\n", T->t, N, sp1, attacks, P->K, isles, food);
        fflush(T->forces_out);
        if (sp1 && !T->saw_sp1) { T->saw_sp1 = 1; event(T, "forces", "sp1=%d pop=%d attacks=%ld K=%d", sp1, N, attacks, P->K); }
    }
    {
        long spend = 0, part = 0, marq = 0;
        for (int k = 0; k < P->K; k++) {
            spend += T->I[k].brick_spend; part += T->I[k].partage; marq += T->I[k].marques;
            T->I[k].brick_spend = T->I[k].partage = T->I[k].marques = 0;
        }
        if (T->lois_out) {
            int milli = 1000;
            if (T->saison && P->season_amp > 0 && P->season_period > 0) {
                double f = 1.0 + P->season_amp * sin(2.0 * M_PI * (double)T->t / (double)P->season_period);
                if (f < 0) f = 0;
                milli = (int)lround(f * 1000.0);
                if (milli > 1000) milli = 1000;
            }
            fprintf(T->lois_out, "%ld\t%ld\t%ld\t%ld\t%d\t%d\n", T->t, spend, part, marq, milli, T->nloi);
            fflush(T->lois_out);
        }
    }
    lois_brancher(T);
    T->sample_i++;
    TERRA_AFTER_SAMPLE(T);
}

static const char *HEADER = "tick\tpop\tn_geno\tH_geno\tn_raw\tpersist_cum\tshadow_cum\tnew_persist\tpersist_alive\tlen_mean\tlen_max\tcoding_mean"
                            "\tniches_occ\tstable_niches\tH_behav\tsoc_pairs\tsoc_mi\tsoc_null\tsoc_excess\tbirths\tdeaths\temits"
                            "\tn_comp\tcomp_per_ind\tcomp_cum\tcomp_new\tcomp_alive"
                            "\trep_try\trep_ok\tcopier_frac\tmin_ess\tpred_hits\tscomp_cum\tscomp_new\tscomp_alive\n";

/* ---- checkpoint ---- */
#define TMAGIC3 "OADNTER3"
#define TMAGIC4 "OADNTER4"
#define TMAGIC5 "OADNTER5"
#define TMAGIC6 "OADNTER6"
#define TMAGIC7 "OADNTER7"
#define TMAGIC8 "OADNTER8"
#define TMAGIC9 "OADNTER9"
#define CMAX_LEGACY 1024
typedef struct {
    WorldConfig cfg;
    Rng *rng;
    struct Lab *lab;
    uint8_t food[GRID_H][GRID_W];
    int occ[GRID_H][GRID_W];
    long t;
    int nid, stimulus;
    uint8_t hist[HIST_N];
    int hist_n, hist_pos;
    Emission em[CMAX_LEGACY]; int nem;
    Stats st;
    Creature *cs[CMAX_LEGACY]; int ncs;
    Creature *nb[CMAX_LEGACY]; int nnb;
    void (*before_vm)(struct World *w, Creature *c);
} WorldLegacy;
static int wr(FILE *f, const void *p, size_t n) { return fwrite(p, 1, n, f) == n; }
static int rd(FILE *f, void *p, size_t n) { return fread(p, 1, n, f) == n; }
static int load_world(FILE *f, World *w, int legacy) {
    memset(w, 0, sizeof *w);
    if (!legacy) return rd(f, w, sizeof(World));
    WorldLegacy old;
    if (!rd(f, &old, sizeof old)) return 0;
    w->cfg = old.cfg;
    memcpy(w->food, old.food, sizeof w->food);
    memcpy(w->occ, old.occ, sizeof w->occ);
    w->t = old.t; w->nid = old.nid; w->stimulus = old.stimulus;
    memcpy(w->hist, old.hist, sizeof w->hist);
    w->hist_n = old.hist_n; w->hist_pos = old.hist_pos;
    w->st = old.st; w->ncs = old.ncs; w->nnb = 0; w->nem = 0;
    return w->ncs >= 0 && w->ncs <= CMAX_LEGACY;
}
static int save(Terra *T) {
    char tmp[600], path[600]; snprintf(tmp, sizeof tmp, "%s/state.ck.tmp", T->dir); snprintf(path, sizeof path, "%s/state.ck", T->dir);
    FILE *f = fopen(tmp, "w+b"); if (!f) return 0;
    int lois = T->depense || T->causales || T->saison || T->tronc;
    int ok = wr(f, lois ? TMAGIC9 : T->traits ? TMAGIC8 : TMAGIC7, 8) && wr(f, &T->P, sizeof T->P) && wr(f, &T->gr, sizeof T->gr) && wr(f, &T->t, sizeof T->t) &&
             wr(f, &T->sample_i, sizeof T->sample_i) && wr(f, &T->founders_done, sizeof(int)) &&
             wr(f, T->niche, sizeof T->niche) && wr(f, T->pop_ring, sizeof T->pop_ring) && wr(f, &T->cyc_pop, sizeof T->cyc_pop) &&
             wr(f, T->cyc_niche, sizeof T->cyc_niche) && wr(f, &T->soc_streak, sizeof(int)) && wr(f, &T->soc_on, sizeof(int)) &&
             wr(f, &T->soc_events, sizeof(long)) && wr(f, &T->n_events, sizeof(long)) && wr(f, &T->emits_prev, sizeof(long)) &&
             wr(f, &T->sgr, sizeof(Rng)) && wr(f, &T->last_ess, sizeof(int)) &&
             wr(f, &T->soc_in_streak, sizeof(int)) && wr(f, &T->soc_in_on, sizeof(int)) && wr(f, &T->soc_in_events, sizeof(long)) &&
             wr(f, &T->soc_out_streak, sizeof(int)) && wr(f, &T->soc_out_on, sizeof(int)) && wr(f, &T->soc_out_events, sizeof(long));
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
        if (T->P.forces >= 3) ok = ok && wr(f, I->brick, sizeof I->brick);
        ok = ok && wr(f, I->att, sizeof I->att) && wr(f, I->echo, sizeof I->echo);
    }
    if (ok && T->traits && !lois) {
        int non = NFACT, dn = DNICHE;
        ok = wr(f, &non, sizeof non) && wr(f, T->trait_hi, sizeof T->trait_hi) && wr(f, T->trait_lo, sizeof T->trait_lo) &&
             wr(f, T->trait_on, sizeof T->trait_on) && wr(f, T->trait_slot, sizeof T->trait_slot) && wr(f, &dn, sizeof dn);
        for (int n = 0; n < DNICHE && ok; n++)
            ok = wr(f, &T->dniche[n].stable, sizeof(int)) && wr(f, &T->dniche[n].hi, sizeof(int)) &&
                 wr(f, &T->dniche[n].lo, sizeof(int)) && wr(f, &T->dniche[n].first_stable, sizeof(long));
    }
    if (ok && lois) {
        int tp = T->traits ? 1 : 0;
        int bits = (T->depense ? 1 : 0) | (T->causales ? 2 : 0) | (T->saison ? 4 : 0) | (T->tronc ? 8 : 0);
        ok = wr(f, &tp, sizeof tp);
        if (ok && tp) {
            int non = NFACT, dn = DNICHE;
            ok = wr(f, &non, sizeof non) && wr(f, T->trait_hi, sizeof T->trait_hi) && wr(f, T->trait_lo, sizeof T->trait_lo) &&
                 wr(f, T->trait_on, sizeof T->trait_on) && wr(f, T->trait_slot, sizeof T->trait_slot) && wr(f, &dn, sizeof dn);
            for (int n = 0; n < DNICHE && ok; n++)
                ok = wr(f, &T->dniche[n].stable, sizeof(int)) && wr(f, &T->dniche[n].hi, sizeof(int)) &&
                     wr(f, &T->dniche[n].lo, sizeof(int)) && wr(f, &T->dniche[n].first_stable, sizeof(long));
        }
        ok = ok && wr(f, &bits, sizeof bits);
    }
    if (ok) ok = TERRA_SAVE_EXTRA(f, T);
    ok = terra_finish(f) && ok;
    if(!ok||rename(tmp,path)) return 0;
    int fd=open(T->dir,O_RDONLY|O_DIRECTORY);if(fd<0)return 0;int synced=fsync(fd)==0;close(fd);return synced;
}
static void island_alloc(Island *I) {
    I->tab.t = calloc(TMASK + 1, sizeof(Slot)); I->tab.used = malloc(sizeof(uint32_t) * (TMASK + 1));
    I->sh = malloc(sizeof(uint64_t) * 2 * CMAX);
    I->sg = malloc(sizeof(struct SG) * 2 * CMAX);
    memset(I->brick, 0, sizeof I->brick);
    I->brick_place = I->brick_farm = I->brick_expand = I->brick_tech = 0;
    I->brick_spend = I->partage = I->marques = 0;
    memset(I->att, 0, sizeof I->att); memset(I->echo, 0, sizeof I->echo); memset(I->mark, 0, sizeof I->mark);
    I->attn_depth = I->attn_choice = I->attn_hes = I->attn_yield = 0;
}
static int load(Terra *T, const char *path) {
    FILE *f = fopen(path, "rb"); if (!f) return 0;
    char m[8]; int ok = rd(f, m, 8);
    int v9 = ok && !memcmp(m, TMAGIC9, 8), v8 = ok && !memcmp(m, TMAGIC8, 8), v7 = ok && !memcmp(m, TMAGIC7, 8), v6 = ok && !memcmp(m, TMAGIC6, 8);
    int v5 = ok && !memcmp(m, TMAGIC5, 8), v4 = ok && !memcmp(m, TMAGIC4, 8), v3 = ok && !memcmp(m, TMAGIC3, 8);
    if (!v3 && !v4 && !v5 && !v6 && !v7 && !v8 && !v9) { fclose(f); return 0; }
    if (v9 || v8 || v7 || v6) {
        ok = rd(f, &T->P, sizeof T->P);
        if (v6) T->P.attention = 0;                   /* bourrage d'OADNTER6, pas un flag */
    } else if (v5 || v4) {
        memset(&T->P, 0, sizeof T->P);
        ok = rd(f, &T->P, offsetof(Params, stress));   /* blob v5 : stress reste 0 */
    } else {
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
    if (v9 || v8 || v7 || v6) ok = ok && rd(f, &T->soc_in_streak, sizeof(int)) && rd(f, &T->soc_in_on, sizeof(int)) && rd(f, &T->soc_in_events, sizeof(long)) &&
                            rd(f, &T->soc_out_streak, sizeof(int)) && rd(f, &T->soc_out_on, sizeof(int)) && rd(f, &T->soc_out_events, sizeof(long));
    Lins *ls[4] = { &T->geno, &T->shadow, &T->comp, &T->scomp };
    for (int i = 0; i < 4 && ok; i++) {
        ok = rd(f, &ls[i]->n, sizeof(int)) && rd(f, &ls[i]->cum, sizeof(long));
        ls[i]->v = malloc(sizeof(Lin) * (size_t)(ls[i]->n + 1));
        ok = ok && rd(f, ls[i]->v, sizeof(Lin) * (size_t)ls[i]->n);
    }
    for (int k = 0; k < T->P.K && ok; k++) {
        Island *I = &T->I[k]; island_alloc(I); I->idx = k; I->P = &T->P;
        I->w = malloc(sizeof(World));
        ok = load_world(f, I->w, !(v5 || v6 || v7 || v8 || v9)) && rd(f, &I->wr, sizeof(Rng)) && rd(f, &I->tr, sizeof(Rng)) && rd(f, &I->env, sizeof(int)) &&
             rd(f, &I->nsh, sizeof(int)) && rd(f, &I->sh_next, sizeof(uint64_t)) && I->nsh <= 2 * CMAX &&
             rd(f, I->sh, sizeof(uint64_t) * (size_t)I->nsh) &&
             rd(f, &I->sr, sizeof(Rng)) && rd(f, &I->rr, sizeof(Rng)) && rd(f, &I->npred, sizeof(int)) && rd(f, I->px, sizeof I->px) &&
             rd(f, I->py, sizeof I->py) && rd(f, &I->nsg, sizeof(int)) && I->nsg <= 2 * CMAX && I->npred <= 16;
        for (int i = 0; i < I->nsg && ok; i++) ok = rd(f, &I->sg[i].species, sizeof(int)) && rd(f, &I->sg[i].len, sizeof(int)) &&
                                                   I->sg[i].len >= 0 && I->sg[i].len <= MAX_LEN && rd(f, I->sg[i].dna, (size_t)I->sg[i].len);
        World *w = I->w; w->rng = &I->wr; w->lab = NULL; w->before_vm = hook; w->nnb = 0; w->nem = 0;
        if (w->ncs < 0 || w->ncs > CMAX) ok = 0;
        for (int i = 0; i < w->ncs && ok; i++) { w->cs[i] = malloc(sizeof(Creature)); ok = rd(f, w->cs[i], sizeof(Creature)); }
        if (T->P.forces >= 3) ok = ok && rd(f, I->brick, sizeof I->brick);
        if (v7 || v8 || v9) ok = ok && rd(f, I->att, sizeof I->att) && rd(f, I->echo, sizeof I->echo);
    }
    T->traits = 0;
    if (v8) {
        int non = 0, dn = 0;
        ok = ok && rd(f, &non, sizeof non) && non == NFACT && rd(f, T->trait_hi, sizeof T->trait_hi) &&
             rd(f, T->trait_lo, sizeof T->trait_lo) && rd(f, T->trait_on, sizeof T->trait_on) &&
             rd(f, T->trait_slot, sizeof T->trait_slot) && rd(f, &dn, sizeof dn) && dn == DNICHE;
        for (int n = 0; n < DNICHE && ok; n++)
            ok = rd(f, &T->dniche[n].stable, sizeof(int)) && rd(f, &T->dniche[n].hi, sizeof(int)) &&
                 rd(f, &T->dniche[n].lo, sizeof(int)) && rd(f, &T->dniche[n].first_stable, sizeof(long));
        if (ok) T->traits = 1;
    }
    T->depense = T->causales = T->saison = T->tronc = 0;
    if (v9) {
        int tp = 0, bits = 0, non = 0, dn = 0;
        ok = ok && rd(f, &tp, sizeof tp);
        if (ok && tp) {
            ok = rd(f, &non, sizeof non) && non == NFACT && rd(f, T->trait_hi, sizeof T->trait_hi) &&
                 rd(f, T->trait_lo, sizeof T->trait_lo) && rd(f, T->trait_on, sizeof T->trait_on) &&
                 rd(f, T->trait_slot, sizeof T->trait_slot) && rd(f, &dn, sizeof dn) && dn == DNICHE;
            for (int n = 0; n < DNICHE && ok; n++)
                ok = rd(f, &T->dniche[n].stable, sizeof(int)) && rd(f, &T->dniche[n].hi, sizeof(int)) &&
                     rd(f, &T->dniche[n].lo, sizeof(int)) && rd(f, &T->dniche[n].first_stable, sizeof(long));
            if (ok) T->traits = 1;
        }
        ok = ok && rd(f, &bits, sizeof bits);
        if (ok) {
            T->depense = (bits & 1) != 0; T->causales = (bits & 2) != 0;
            T->saison = (bits & 4) != 0; T->tronc = (bits & 8) != 0;
        }
    }
    if (ok) ok = TERRA_LOAD_EXTRA(f, T);
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
    for (int i = 0; i < T->nloi; i++) if (T->loi_h[i]) dlclose(T->loi_h[i]);
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
        for (int k = 0; k < P->K; k++) {
            T->I[k].traits = T->traits; T->I[k].depense = T->depense; T->I[k].causales = T->causales; T->I[k].saison = T->saison;
            T->I[k].t_end = b; pthread_create(&th[k], NULL, island_run, &T->I[k]);
        }
        for (int k = 0; k < P->K; k++) pthread_join(th[k], NULL);
        T->t = b;
        if (P->migrate > 0 && T->t % P->migrate == 0) migrate(T);
        if (P->expand_every > 0 && T->t % P->expand_every == 0 && T->P.K < KMAX) expand_one(T);
        if (P->forces >= 3 && T->P.K < KMAX) {        /* l'espace naît des briques, pas d'une horloge */
            int best = -1, bn = 0;
            for (int k = 0; k < T->P.K; k++) {
                Island *I = &T->I[k]; int n = 0, H = I->w->cfg.h, W = I->w->cfg.w;
                for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) n += I->brick[y][x] != 0;
                if (n > bn) { bn = n; best = k; }
            }
            if (bn >= BRK_NEED && best >= 0) {
                expand_one(T);
                Island *I = &T->I[best];
                int pay = 0, H = I->w->cfg.h, W = I->w->cfg.w;
                for (int y = 0; y < H && pay < BRK_PAY; y++)
                    for (int x = 0; x < W && pay < BRK_PAY; x++)
                        if (I->brick[y][x]) { I->brick[y][x] = 0; pay++; }
                I->brick_expand++;
                event(T, "briques", "espace K=%d depuis_ile=%d briques=%d paye=%d", T->P.K, best, bn, pay);
            }
        }
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
        if (!resumed || fresh)
            fputs(T->P.forces >= 3 ? "tick\tpop\tsp1\tattacks\tK\tisles\tfood\tyield\tattr\trep\tdrain\tbricks\tplace\tfarm\texpand\ttech\n"
                  : T->P.forces >= 2 ? "tick\tpop\tsp1\tattacks\tK\tisles\tfood\tyield\tattr\trep\tdrain\n"
                                     : "tick\tpop\tsp1\tattacks\tK\tisles\tfood\n", T->forces_out);
    }
    T->societies = NULL;
    if (T->P.stress) {
        snprintf(p, sizeof p, "%s/societies.tsv", T->dir); if (resumed) truncate_tsv(p, T->t);
        int fresh = access(p, F_OK) != 0;
        T->societies = fopen(p, resumed && !fresh ? "a" : "w");
        if (!T->societies) return 0;
        if (!resumed || fresh)
            fputs("tick\tn_species\tH_species\tmut_mean\tsoc_intra_pairs\tsoc_intra_excess\tsoc_inter_pairs\tsoc_inter_excess\n", T->societies);
    }
    T->attention = NULL;
    if (T->P.attention) {
        snprintf(p, sizeof p, "%s/attention.tsv", T->dir); if (resumed) truncate_tsv(p, T->t);
        int fresh = access(p, F_OK) != 0;
        T->attention = fopen(p, resumed && !fresh ? "a" : "w");
        if (!T->attention) return 0;
        if (!resumed || fresh)
            fputs("tick\tchaleur\techos\tprofondeur\tchoix\thesitations\tyields\n", T->attention);
    }
    T->traits_out = NULL;
    if (T->traits) {
        snprintf(p, sizeof p, "%s/traits.tsv", T->dir); if (resumed) truncate_tsv(p, T->t);
        int fresh = access(p, F_OK) != 0;
        T->traits_out = fopen(p, resumed && !fresh ? "a" : "w");
        if (!T->traits_out) return 0;
        if (!resumed || fresh) fputs("tick\tn_traits\ttenues\tessayees\n", T->traits_out);
    }
    T->lois_out = NULL;
    if (T->depense || T->causales || T->saison || T->tronc) {
        snprintf(p, sizeof p, "%s/lois.tsv", T->dir); if (resumed) truncate_tsv(p, T->t);
        int fresh = access(p, F_OK) != 0;
        T->lois_out = fopen(p, resumed && !fresh ? "a" : "w");
        if (!T->lois_out) return 0;
        if (!resumed || fresh) fputs("tick\tspend\tpartage\tmarques\tpompe_milli\tlois\n", T->lois_out);
        if (T->tronc) { char ld[600]; snprintf(ld, sizeof ld, "%s/lois", T->dir); mkdir(ld, 0755); }
    }
    return 1;
}

static Params defaults(void) {
    Params P = { .K = 4, .w = 64, .h = 32, .max_pop = 1000, .regrow = 80, .sample = 200, .ck_every = 2000, .migrate = 1000,
                 .channel = 1, .founders = 0, .pop0 = 64, .mut = 0.02, .dup = 0.05, .season_amp = 0, .season_period = 4000, .soc_thr = 0.05, .seed = 1,
                 .repro = 0, .rep_steps = 8192, .rep_cost = 2, .rep_energy = 70, .rep_p = 1, .rep_mut = 0.005,
                 .pred = 0, .pred_dmg = 25, .pred_every = 2, .osmo = 0,
                 .forces = 0, .expand_every = 0, .stress = 0, .attention = 0 };
    return P;
}

static int usage(void) {
    fprintf(stderr, "usage : terrarium_run run --dir D [--islands K] [--w W --h H] [--max-pop N] [--regrow R] [--pop0 N]\n"
                    "                       [--ticks T (0 = sans fin)] [--sample S] [--ck-every C] [--migrate M (0 = îles isolées)]\n"
                    "                       [--seed N] [--mut x --dup x] [--channel on|off] [--founders seeds|random|handmade|soup:FICHIER]\n"
                    "                       [--repro split|copy] [--rep-steps N] [--rep-cost E] [--rep-energy E] [--rep-p p] [--rep-mut x]\n"
                    "                       [--pred N (prédateurs par île)] [--pred-dmg E] [--pred-every T] [--osmo U (absorption passive)]\n"
                    "                       [--season-amp a --season-period p] [--soc-thr bits]\n"
                    "                       [--forces 0|1|2|3] [--expand-every T (0 = K fixe, multiple de --sample)] [--stress 0|1]\n"
                    "                       [--attention 0|1] [--traits 0|1] [--depense 0|1] [--causales 0|1] [--saison 0|1] [--tronc 0|1]\n"
                    "        reprise : --ticks, et chocs --season-amp / --pred / --regrow / --forces / --expand-every / --stress / --attention / --traits / --depense / --causales / --saison / --tronc\n"
                    "        terrarium_run dump DIR              (germens d'un checkpoint : opcodes, consensus)\n"
                    "        terrarium_run report DIR [DIR...]   (résumé par quarts)\n"
                    "        terrarium_run selftest\n");
    return 2;
}

static int cmd_run(int argc, char **argv) {
    Params P = defaults(); const char *dir = NULL; long ticks = 20000;
    int got_season_amp = 0, got_season_period = 0, got_pred = 0, got_pred_dmg = 0, got_pred_every = 0, got_regrow = 0;
    int got_forces = 0, got_expand = 0, got_max_pop = 0, got_stress = 0, got_attention = 0, got_traits = 0;
    int got_depense = 0, got_causales = 0, got_saison = 0, got_tronc = 0;
    int traits_flag = 0, depense_flag = 0, causales_flag = 0, saison_flag = 0, tronc_flag = 0;
    for (int i = 2; i < argc; i++) {
        const char *a = argv[i], *v = i + 1 < argc ? argv[i + 1] : NULL;
        if (!v) return usage();
        if (!strcmp(a, "--dir")) dir = v;
        else if (!strcmp(a, "--islands")) P.K = atoi(v);
        else if (!strcmp(a, "--w")) P.w = atoi(v);
        else if (!strcmp(a, "--h")) P.h = atoi(v);
        else if (!strcmp(a, "--max-pop")) { P.max_pop = atoi(v); got_max_pop = 1; }
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
        else if (!strcmp(a, "--stress")) { P.stress = atoi(v); got_stress = 1; }
        else if (!strcmp(a, "--attention")) { P.attention = atoi(v); got_attention = 1; }
        else if (!strcmp(a, "--traits")) { traits_flag = atoi(v); got_traits = 1; }
        else if (!strcmp(a, "--depense")) { depense_flag = atoi(v); got_depense = 1; }
        else if (!strcmp(a, "--causales")) { causales_flag = atoi(v); got_causales = 1; }
        else if (!strcmp(a, "--saison")) { saison_flag = atoi(v); got_saison = 1; }
        else if (!strcmp(a, "--tronc")) { tronc_flag = atoi(v); got_tronc = 1; }
        else return usage();
        i++;
    }
    if (!dir || P.K < 1 || P.K > KMAX || P.sample < 1 || P.ck_every % P.sample || P.w < 4 || P.h < 4 || P.w > GRID_W || P.h > GRID_H ||
        P.max_pop < 1 || P.max_pop > CMAX - 24 || P.pop0 < 1 || P.pop0 > P.max_pop || P.season_period < 1 ||
        (P.repro && P.founders == F_SEEDS) || P.pred < 0 || P.pred > 16 || P.pred_every < 1 || P.rep_steps < 1 || P.rep_energy < 1 ||
        P.forces < 0 || P.forces > 3 || P.stress < 0 || P.stress > 1 || P.attention < 0 || P.attention > 1 ||
        traits_flag < 0 || traits_flag > 1 || depense_flag < 0 || depense_flag > 1 ||
        causales_flag < 0 || causales_flag > 1 || saison_flag < 0 || saison_flag > 1 || tronc_flag < 0 || tronc_flag > 1 ||
        P.expand_every < 0 || (P.expand_every > 0 && P.expand_every % P.sample)) return usage();
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
        if (got_stress) T.P.stress = P.stress;
        if (got_attention) T.P.attention = P.attention;
        if (got_traits) {
            if (traits_flag && !T.traits) trait_arm(&T);
            T.traits = traits_flag;
        }
        if (got_depense) T.depense = depense_flag;
        if (got_causales) T.causales = causales_flag;
        if (got_saison) T.saison = saison_flag;
        if (got_tronc) T.tronc = tronc_flag;
        if (got_max_pop) {
            T.P.max_pop = P.max_pop;
            for (int k = 0; k < T.P.K; k++) T.I[k].w->cfg.max_pop = P.max_pop;
        }
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
        if (got_season_amp || got_pred || got_regrow || got_forces || got_expand || got_max_pop || got_stress || got_attention || got_traits || got_depense || got_causales || got_saison || got_tronc)
            fprintf(stderr, " (choc : season_amp=%.2f pred=%d regrow=%d forces=%d expand_every=%d max_pop=%d stress=%d attention=%d traits=%d depense=%d causales=%d saison=%d tronc=%d)",
                    T.P.season_amp, T.P.pred, T.P.regrow, T.P.forces, T.P.expand_every, T.P.max_pop, T.P.stress, T.P.attention, T.traits, T.depense, T.causales, T.saison, T.tronc);
        else
            fprintf(stderr, " (paramètres du checkpoint ; --ticks et les chocs --season-amp/--pred/--regrow/--forces/--expand-every/--max-pop/--stress/--attention/--traits comptent)");
        fprintf(stderr, "\n");
    } else {
        terra_new(&T, &P);
        if (traits_flag) { T.traits = 1; trait_arm(&T); }
        T.depense = depense_flag; T.causales = causales_flag; T.saison = saison_flag; T.tronc = tronc_flag;
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
    fclose(T.series); fclose(T.events); if (T.forces_out) fclose(T.forces_out); if (T.societies) fclose(T.societies); if (T.attention) fclose(T.attention); if (T.traits_out) fclose(T.traits_out); if (T.lois_out) fclose(T.lois_out); terra_free(&T);
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

/* Une créature, un tick, regrow 0. food renvoyé = arrivée si elle a bougé, piste ouest si on avait
 * posé de la nourriture, sinon la case de départ. */
static int attn_probe(const uint8_t *dna, int len, int heat, int echo0, int food0,
                      int *out_food, int *out_att, int *out_echo, int *out_emit,
                      int *out_steps, int *out_x, int *out_y) {
    Params P = defaults();
    P.attention = 1; P.K = 1; P.w = 8; P.h = 8; P.max_pop = 4; P.pop0 = 1;
    P.regrow = 0; P.sample = 50; P.ck_every = 50; P.migrate = 0; P.seed = 3;
    P.channel = 1; P.forces = 0; P.stress = 0; P.pred = 0;
    static Terra T;
    terra_new(&T, &P);
    Island *I = &T.I[0]; World *w = I->w;
    w->cfg.food_regrow = 0;
    for (int y = 0; y < w->cfg.h; y++) for (int x = 0; x < w->cfg.w; x++) w->food[y][x] = 0;
    Creature *c = w->cs[0];
    w->occ[c->y][c->x]--; c->x = 3; c->y = 3; w->occ[3][3]++;
    memcpy(c->dna, dna, (size_t)len); c->len = len; c->ip = 0;
    c->e = 120; c->d = 1; c->a = 0; c->b = 0; c->emitted = -1; c->steps = 0; c->eaten = 0;
    c->ls[4] = 1;
    memset(I->att, 0, sizeof I->att); memset(I->echo, 0, sizeof I->echo);
    I->att[3][3] = (uint8_t)heat; I->echo[3][3] = (uint8_t)echo0;
    w->food[3][3] = (uint8_t)food0;
    island_tick(I);
    *out_att = I->att[3][3]; *out_echo = I->echo[3][3];
    *out_x = c->x; *out_y = c->y; *out_steps = c->steps; *out_emit = c->emitted;
    if (c->x != 3 || c->y != 3) *out_food = w->food[c->y][c->x];
    else if (food0) *out_food = w->food[3][2];        /* piste : d=1, derrière = ouest */
    else *out_food = w->food[3][3];
    int alive = c->e > 0;
    terra_free(&T);
    return alive;
}

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
        int hy[OP_COUNT], hz[OP_COUNT];
        uint8_t y[] = { OP_YIELD, OP_XOR }, z[] = { OP_LOAD, OP_YIELD };
        tape_scan(y, 2, hy); tape_scan(z, 2, hz);
        int he[OP_COUNT], hi[OP_COUNT];
        uint8_t e[] = { OP_EMIT, OP_YIELD }, im[] = { OP_STORE, OP_EMIT };
        tape_scan(e, 2, he); tape_scan(im, 2, hi);
        int hl[OP_COUNT], hn[OP_COUNT];
        uint8_t lp[] = { OP_LOOP, OP_SENSE, OP_NEURAL, 0 }, li[] = { OP_STORE, OP_LOOP };
        tape_scan(lp, 4, hl); tape_scan(li, 2, hn);
        printf("selftest forces : opcode ATTACK %s, immédiat %s ; YIELD opcode %s immédiat %s ; EMIT opcode %s immédiat %s ; LOOP/NEURAL/SENSE %s immédiat LOOP %s\n",
               ha ? "détecté" : "MANQUE", hb ? "FAUX POSITIF" : "ignoré",
               hy[OP_YIELD] ? "détecté" : "MANQUE", hz[OP_YIELD] ? "FAUX POSITIF" : "ignoré",
               he[OP_EMIT] ? "détecté" : "MANQUE", hi[OP_EMIT] ? "FAUX POSITIF" : "ignoré",
               hl[OP_LOOP] && hl[OP_NEURAL] && hl[OP_SENSE] ? "détectés" : "MANQUE",
               hn[OP_LOOP] ? "FAUX POSITIF" : "ignoré");
        ok &= ha && !hb && hy[OP_YIELD] && hy[OP_XOR] && !hz[OP_YIELD] && he[OP_EMIT] && he[OP_YIELD] && !hi[OP_EMIT]
              && hl[OP_LOOP] && hl[OP_NEURAL] && hl[OP_SENSE] && !hn[OP_LOOP];
    }
    /* 5. stress : masque (opcode ≠ immédiat), calme ×1 / plein ×8, run court qui écrit societies.tsv */
    {
        uint8_t atk[] = { OP_NOP, OP_ATTACK }, imm[] = { OP_STORE, OP_ATTACK }, both[] = { OP_ATTACK, OP_YIELD };
        int ma = role_mask(atk, 2), mi = role_mask(imm, 2), mb = role_mask(both, 2);
        int layout = offsetof(Params, stress) == 408 && sizeof(Params) == 416;
        int scale = stress_scale(0) == 1.0 && stress_scale(1) == 8.0;
        int good = ma == 1 && mi == 0 && mb == 3 && layout && scale;
        printf("selftest stress : masque ATTACK=%d immédiat=%d ATTACK|YIELD=%d échelle %.0f/%.0f layout %s → %s\n",
               ma, mi, mb, stress_scale(0), stress_scale(1), layout ? "408/416" : "CASSÉ", good ? "OK" : "ÉCHEC");
        ok &= good;
        Params Ps = defaults();
        Ps.stress = 1; Ps.K = 1; Ps.w = 16; Ps.h = 8; Ps.max_pop = 40; Ps.pop0 = 8;
        Ps.regrow = 4; Ps.sample = 50; Ps.ck_every = 100; Ps.migrate = 0; Ps.seed = 9;
        Ps.repro = 1; Ps.founders = F_HANDMADE; Ps.rep_steps = 256;
        const char *ds = "/tmp/terrarium-selftest-stress";
        char cmd[128]; snprintf(cmd, sizeof cmd, "rm -rf %s", ds); if (system(cmd)) {}
        mkdir(ds, 0755);
        static Terra S;
        terra_new(&S, &Ps); snprintf(S.dir, sizeof S.dir, "%s", ds);
        int ran = open_outputs(&S, 0) && terra_run(&S, 100, 1) == 0;
        if (S.series) fclose(S.series);
        if (S.events) fclose(S.events);
        if (S.forces_out) fclose(S.forces_out);
        if (S.societies) fclose(S.societies);
        if (S.attention) fclose(S.attention);
        if (S.traits_out) fclose(S.traits_out);
        terra_free(&S);
        char path[160]; snprintf(path, sizeof path, "%s/societies.tsv", ds);
        char *ss = slurp(path);
        int rows = 0; if (ss) for (char *q = ss; *q; q++) rows += *q == '\n';
        int file_ok = ran && ss && rows >= 2 && strstr(ss, "n_species");
        printf("selftest stress run : societies.tsv %d lignes → %s\n", rows, file_ok ? "OK" : "ÉCHEC");
        ok &= file_ok; free(ss); if (system(cmd)) {}
    }
    /* 6. attention : le processus dépose, le choix dépense, le réflexe sur sol froid ne reçoit rien */
    {
        uint8_t step[] = { OP_SENSE, OP_STEP, OP_YIELD };
        uint8_t reflex[] = { OP_STEP, OP_YIELD };
        uint8_t proc[] = { OP_SENSE, OP_SENSE_NB, OP_YIELD };
        uint8_t look[] = { OP_SENSE_STIM, OP_EMIT, OP_YIELD };
        uint8_t eat[] = { OP_SENSE, OP_EAT, OP_YIELD };
        int food = 0, att = 0, echo = 0, emit = 0, steps = 0, x = 0, y = 0, look_hi = -1;
        int a = attn_probe(step, 3, 3, 0, 0, &food, &att, &echo, &emit, &steps, &x, &y);
        int deliber = a && steps == 1 && x == 4 && y == 3 && food == 1 && att == 2 && echo == OP_STEP;
        int fa = food, aa = att, ea = echo;
        int b = attn_probe(reflex, 3, 0, 0, 0, &food, &att, &echo, &emit, &steps, &x, &y);
        int cold = b && steps == 1 && food == 0 && att == 0 && echo == 0;
        int fr = food;
        int c = attn_probe(proc, 3, 0, 0, 0, &food, &att, &echo, &emit, &steps, &x, &y);
        int hesit = c && steps == 0 && food == 0 && att == 2 && echo == OP_YIELD;
        int ap = att, ep = echo;
        int d = attn_probe(look, 3, 4, OP_STEP, 0, &food, &att, &echo, &emit, &steps, &x, &y);
        look_hi = (emit >> 4) & 15;
        int sensed = d && look_hi == (OP_STEP & 15);
        int e = attn_probe(eat, 3, 0, 0, 6, &food, &att, &echo, &emit, &steps, &x, &y);
        int grazed = e && food == 1;
        int good = deliber && cold && hesit && sensed && grazed;
        printf("selftest attention : délibéré food=%d att=%d echo=%d ; réflexe food=%d ; processus att=%d echo=%d ; stimulus %d ; piste=%d → %s\n",
               fa, aa, ea, fr, ap, ep, look_hi, grazed, good ? "OK" : "ÉCHEC");
        if (!good) printf("  détail délibéré=%d réflexe=%d processus=%d stimulus=%d piste=%d\n",
                          deliber, cold, hesit, sensed, grazed);
        ok &= good;
    }
    /* 7. traits : on retient ce qui sépare, pas la copie d'un fait déjà retenu, et le monde ne dévie pas */
    {
        enum { N = 20 };
        int cnt[NFACT] = { 0 }, n11[NFACT * NFACT];
        memset(n11, 0, sizeof n11);
        uint8_t row[N][NFACT];
        memset(row, 0, sizeof row);
        for (int c = 0; c < N; c++) {
            row[c][OP_STEP] = (c % 2) == 0;
            row[c][OP_EAT] = row[c][OP_STEP];
            row[c][OP_TURN] = 1;
            row[c][OP_SENSE] = c < 10;
            row[c][OP_YIELD] = c < 2;
        }
        for (int c = 0; c < N; c++) for (int i = 0; i < NFACT; i++) if (row[c][i]) {
            cnt[i]++;
            for (int j = i + 1; j < NFACT; j++) if (row[c][j]) n11[i * NFACT + j]++;
        }
        int sel[NFACT];
        int nk = trait_select(cnt, n11, N, sel);
        int pick = sel[OP_STEP] && sel[OP_SENSE] && sel[OP_YIELD] && !sel[OP_EAT] && !sel[OP_TURN] && nk == 3;
        static Terra Tr;
        memset(&Tr, 0, sizeof Tr);
        Tr.traits = 1; trait_arm(&Tr);
        double freq[NFACT] = { 0 };
        freq[OP_STEP] = 0.5; freq[OP_SENSE] = 0.5; freq[OP_YIELD] = 0.1;
        for (int r = 0; r < PERSIST; r++) trait_commit(&Tr, sel, freq);
        int born = Tr.trait_on[OP_STEP] && Tr.trait_on[OP_SENSE] && Tr.trait_on[OP_YIELD] && !Tr.trait_on[OP_EAT] && Tr.n_events == 3;
        printf("selftest traits : choix %d (step=%d eat=%d turn=%d sense=%d yield=%d) naissance %d → %s\n",
               nk, sel[OP_STEP], sel[OP_EAT], sel[OP_TURN], sel[OP_SENSE], sel[OP_YIELD], born, pick && born ? "OK" : "ÉCHEC");
        if (!(pick && born)) ok = 0;
        Params P = defaults();
        P.K = 1; P.pop0 = 24; P.max_pop = 48; P.sample = 100; P.ck_every = 1000; P.migrate = 0; P.seed = 3; P.regrow = 20;
        const char *du = "/tmp/terrarium-selftest-tu", *dv = "/tmp/terrarium-selftest-tv";
        char cmd[160]; snprintf(cmd, sizeof cmd, "rm -rf %s %s", du, dv); if (system(cmd)) {}
        mkdir(du, 0755); mkdir(dv, 0755);
        static Terra U, V;
        terra_new(&U, &P); snprintf(U.dir, sizeof U.dir, "%s", du); open_outputs(&U, 0); terra_run(&U, 300, 1);
        terra_new(&V, &P); V.traits = 1; trait_arm(&V); snprintf(V.dir, sizeof V.dir, "%s", dv); open_outputs(&V, 0); terra_run(&V, 300, 1);
        uint64_t cu = FNV_INIT, cv = FNV_INIT;
        for (int side = 0; side < 2; side++) {
            Terra *X = side ? &V : &U; uint64_t *h = side ? &cv : &cu;
            for (int k = 0; k < X->P.K; k++) {
                World *w = X->I[k].w;
                *h = fnv1a(w->food, sizeof w->food, *h);
                *h = fnv1a(&w->ncs, sizeof w->ncs, *h);
                *h = fnv1a(&X->I[k].wr, sizeof(Rng), *h);
                for (int i = 0; i < w->ncs; i++) {
                    Creature *c = w->cs[i];
                    int rec[6] = { c->x, c->y, c->d, c->e, c->id, c->len };
                    *h = fnv1a(rec, sizeof rec, *h);
                }
            }
        }
        char tp[160]; snprintf(tp, sizeof tp, "%s/traits.tsv", dv);
        char *ts = slurp(tp);
        int file_ok = ts && strstr(ts, "essayees") && strchr(ts, '\n') && strchr(strchr(ts, '\n') + 1, '\n');
        int same = cu == cv && file_ok;
        printf("selftest traits monde : recensement %s, traits.tsv %s → %s\n",
               cu == cv ? "identique" : "DÉVIÉ", file_ok ? "écrit" : "absent", same ? "OK" : "ÉCHEC");
        ok &= same;
        fclose(U.series); fclose(U.events); if (U.forces_out) fclose(U.forces_out); if (U.societies) fclose(U.societies); if (U.attention) fclose(U.attention); if (U.traits_out) fclose(U.traits_out); terra_free(&U);
        fclose(V.series); fclose(V.events); if (V.forces_out) fclose(V.forces_out); if (V.societies) fclose(V.societies); if (V.attention) fclose(V.attention); if (V.traits_out) fclose(V.traits_out); terra_free(&V);
        free(ts); if (system(cmd)) {}
    }
    /* 8. dépense retire une brique sans irriguer ; causales déplace ou marque ; saison muette si amplitude 0 ; un .so se branche */
    {
        Params P = defaults();
        P.K = 1; P.w = 8; P.h = 8; P.pop0 = 2; P.max_pop = 8; P.stress = 1; P.forces = 3;
        P.sample = 200; P.ck_every = 200; P.migrate = 0; P.regrow = 0; P.seed = 4; P.season_amp = 0;
        static Terra D;
        terra_new(&D, &P);
        Island *I = &D.I[0];
        I->depense = 1; I->saison = 1; D.P.K = KMAX; I->P = &D.P;
        World *w = I->w; w->t = 0;
        memset(w->food, 0, sizeof w->food);
        memset(I->brick, 0, sizeof I->brick);
        I->brick[0][0] = 1; I->brick[0][1] = 2; I->brick[1][0] = 4; I->brick[1][1] = 8;
        briques_pompes(I);
        /* ferme YIELD encore active (food 0,1) ; technologie coupée (les trois autres cases) ; une brique retirée */
        int dep = I->brick[0][0] == 0 && I->brick_spend == 1
            && w->food[0][0] == 0 && w->food[0][1] == 1 && w->food[1][0] == 0 && w->food[1][1] == 0;
        I->depense = 0; D.P.K = 1; I->brick_tech = 0;
        I->brick[0][0] = 1;
        briques_pompes(I);
        int irr = I->brick_tech == 1 && w->food[0][0] == 1 && I->brick_spend == 1;
        Creature *a = w->cs[0], *b = w->cs[1];
        a->species = b->species = 3; a->x = 1; a->y = 1; b->x = 2; b->y = 1;
        w->food[1][1] = 5; w->food[1][2] = 1;
        memset(I->celln, 0, sizeof I->celln);
        I->celln[1][2] = 1; I->cellv[1][2][0] = b;
        I->causales = 1;
        causales_appliquer(I, a, 1, 3);
        int part = w->food[1][1] == 4 && w->food[1][2] == 2 && I->partage == 1;
        causales_appliquer(I, a, 1, 9);
        int mrk = I->mark[1][1] == 9 && I->marques == 1;
        I->saison = 0; int passe = pompe_passe(I, 1, 1);
        I->saison = 1; D.P.season_amp = 1; D.P.season_period = 4; w->t = 3;
        int creux = pompe_passe(I, 0, 0) == 0;
        I->brick[2][3] = 42;
        loi_courant = &D;
        int lu = loi_brique_lire(0, 3, 2);
        int hors = loi_brique_lire(-1, 0, 0) | loi_brique_lire(0, 8, 0) | loi_brique_lire(0, 0, 8);
        loi_courant = NULL;
        I->brick[2][3] = 0;
        int lect = lu == 42 && hors == 0;
        const char *so = "experiments/tronc-ligne/exemple.so";
        const char *ld = "/tmp/terrarium-selftest-lois/lois";
        char cmd[240]; snprintf(cmd, sizeof cmd, "rm -rf /tmp/terrarium-selftest-lois && mkdir -p %s && cp %s %s/exemple.so", ld, so, ld);
        int copie = system(cmd) == 0;
        static Terra R;
        memset(&R, 0, sizeof R);
        /* K = 0 : aucune île allouée. total_pop ne doit pas déréférencer un monde absent. */
        R.tronc = 1; R.P.K = 0; R.t = 10; snprintf(R.dir, sizeof R.dir, "/tmp/terrarium-selftest-lois");
        R.events = fopen("/tmp/terrarium-selftest-lois/events.tsv", "w");
        if (R.events) fputs("tick\ttype\tdetail\n", R.events);
        if (copie) lois_brancher(&R);
        int branche = R.nloi == 1 && R.n_events >= 1;
        if (copie) lois_brancher(&R);
        int une = R.nloi == 1;
        printf("selftest lois : depense %d irrigue %d partage %d marque %d passe %d creux %d lecture %d branche %d une_fois %d → %s\n",
               dep, irr, part, mrk, passe, creux, lect, branche, une,
               dep && irr && part && mrk && passe && creux && lect && branche && une ? "OK" : "ÉCHEC");
        ok &= dep && irr && part && mrk && passe && creux && lect && branche && une;
        if (R.events) fclose(R.events);
        D.P.K = 1; R.P.K = 0;
        terra_free(&D); terra_free(&R);
        if (system("rm -rf /tmp/terrarium-selftest-lois")) {}
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
    uint64_t ha = terra_hash(&A); *pop_end = total_pop(&A); fclose(A.series); fclose(A.events); if (A.forces_out) fclose(A.forces_out); if (A.societies) fclose(A.societies); if (A.attention) fclose(A.attention); if (A.traits_out) fclose(A.traits_out); terra_free(&A);
    terra_new(&B, P); snprintf(B.dir, sizeof B.dir, "%s", db); open_outputs(&B, 0); terra_run(&B, t1, 1);
    fclose(B.series); fclose(B.events); if (B.forces_out) fclose(B.forces_out); if (B.societies) fclose(B.societies); if (B.attention) fclose(B.attention); if (B.traits_out) fclose(B.traits_out); terra_free(&B);
    { char p[600]; snprintf(p, sizeof p, "%s/state.ck", db); memset(&B, 0, sizeof B);
      if (!load(&B, p)) { printf("selftest : checkpoint illisible\n"); return 0; } }
    snprintf(B.dir, sizeof B.dir, "%s", db); open_outputs(&B, 1); terra_run(&B, t2, 1);
    uint64_t hb = terra_hash(&B); fclose(B.series); fclose(B.events); if (B.forces_out) fclose(B.forces_out); if (B.societies) fclose(B.societies); if (B.attention) fclose(B.attention); if (B.traits_out) fclose(B.traits_out); terra_free(&B);
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

static int cmd_dump(int argc, char **argv) {
    if (argc < 3) return usage();
    static Terra T; memset(&T, 0, sizeof T);
    char ck[600]; snprintf(ck, sizeof ck, "%s/state.ck", argv[2]);
    if (!load(&T, ck)) { fprintf(stderr, "%s illisible\n", ck); return 1; }
    int N = total_pop(&T), has_n[OP_COUNT] = {0}, att = 0, n64 = 0;
    int cons_n[SOUP_PROG][256]; memset(cons_n, 0, sizeof cons_n);
    printf("t=%ld K=%d pop=%d forces=%d expand_every=%d regrow=%d\n",
           T.t, T.P.K, N, T.P.forces, T.P.expand_every, T.P.regrow);
    for (int k = 0; k < T.P.K; k++) {
        World *w = T.I[k].w;
        int sp1 = 0; long atk = w->st.attacks;
        for (int i = 0; i < w->ncs; i++) {
            Creature *c = w->cs[i];
            int has[OP_COUNT]; tape_scan(c->germ, c->germ_len, has);
            for (int o = 0; o < OP_COUNT; o++) has_n[o] += has[o];
            if (has[OP_ATTACK]) att++;
            if (c->species) sp1++;
            if (c->germ_len == SOUP_PROG) {
                n64++;
                for (int b = 0; b < SOUP_PROG; b++) cons_n[b][c->germ[b]]++;
            }
        }
        int br = 0, H = w->cfg.h, W = w->cfg.w;
        int rb[7] = { 0 };
        for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) {
            uint8_t bk = T.I[k].brick[y][x];
            if (!bk) continue;
            br++;
            for (int b = 0; b < 7; b++) rb[b] += (bk >> b) & 1;
        }
        printf("  ile %d ncs=%d sp1=%d attacks_cum=%ld briques=%d YIELD=%d XOR=%d ATTACK=%d LOOP=%d NEURAL=%d SENSE=%d\n",
               k, w->ncs, sp1, atk, br, rb[1], rb[2], rb[3], rb[4], rb[5], rb[6]);
    }
    printf("porteurs d'opcode (sur %d) :\n", N);
    for (int o = 1; o < OP_COUNT; o++)
        if (has_n[o]) printf("  %-12s %5d  %5.1f%%\n", op_name(o), has_n[o], N ? 100.0 * has_n[o] / N : 0);
    printf("ATTACK porteurs %d (%.2f%%)\n", att, N ? 100.0 * att / N : 0);
    if (n64) {
        int conserved = 0;
        printf("consensus 64o (%d individus), positions ≥ 50 %% :\n", n64);
        for (int b = 0; b < SOUP_PROG; b++) {
            int best = 0, bv = 0;
            for (int v = 0; v < 256; v++) if (cons_n[b][v] > bv) { bv = cons_n[b][v]; best = v; }
            double f = (double)bv / n64;
            if (f >= 0.5) {
                conserved++;
                printf("  [%02d] %02x %-12s %5.1f%%\n", b, best, op_valid((uint8_t)best) ? op_name((uint8_t)best) : "INTRON", 100 * f);
            }
        }
        printf("positions majoritaires ≥50%% : %d / 64\n", conserved);
    }
    terra_free(&T);
    return 0;
}

int main(int argc, char **argv) {
    if (argc >= 2 && !strcmp(argv[1], "run")) return cmd_run(argc, argv);
    if (argc >= 2 && !strcmp(argv[1], "selftest")) return selftest();
    if (argc >= 2 && !strcmp(argv[1], "report")) return cmd_report(argc, argv);
    if (argc >= 2 && !strcmp(argv[1], "dump")) return cmd_dump(argc, argv);
    return usage();
}
