/* Soupe spatiale : chaque programme ne rencontre que ses quatre voisins sur un tore.
 * Physique figée (univers d'origine, adressage B). Le bras mélangé est la fonction sim du cadre,
 * inchangée. Le bras grille est la même boucle, seul l'appariement change (pavage en dominos).
 * src/, include/ et seeds/ ne bougent pas.
 *
 *   ./experiments/soupe_spatiale selftest
 *   ./experiments/soupe_spatiale run [--threads 32]
 */
#define main us1_main
#include "universe_search.c"
#undef main

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

enum { SS_N = 4096, SS_E = 16384, SS_SEEDS = 32, SS_NOISE = 8, SS_POS = 8, SS_POS_E = 2048, SS_POS_INJ = 16 };
enum { ARM_M, ARM_G };
#define SS_CKMAX (SS_E / CK)

static const char *SS_FOUNDER =
    "051d03021c1fff0701030903c906901e051d03021c1fff0701030903c906901e"
    "051d03021c1fff0701030903c906901e051d03021c1fff0701030903c906901e";
static const char *SS_CRYSTAL = "00112233445566778899aabbccddeeff";

static void ss_hex(uint8_t *p, int n, const char *s) {
    for (int i = 0; i < n; i++) { unsigned x; sscanf(s + 2 * (i % (int)(strlen(s) / 2)), "%2x", &x); p[i] = (uint8_t)x; }
}

/* Pavage en dominos du tore W × W : orientation o (0 horizontale, 1 verticale), parité ph.
 * Écrit dans pa/pb les deux cases de chaque paire ; chaque case apparaît une fois. */
static int ss_tiling(int W, int o, int ph, int *pa, int *pb) {
    int n = 0;
    for (int y = 0; y < W; y++) for (int x = 0; x < W; x++) {
        int c = o ? y : x;
        if (((c - ph) & 1) != 0) continue;
        int x2 = o ? x : (x + 1) % W, y2 = o ? (y + 1) % W : y;
        pa[n] = y * W + x; pb[n] = y2 * W + x2; n++;
    }
    return n;
}

typedef struct { SimRes r; int ncurve; float curve[SS_CKMAX]; } GRes;

/* La boucle sim du cadre, appariement local. Seules les lignes de l'appariement diffèrent. */
static void sim_grid(const Univ *u, const SimCfg *c, GRes *G) {
    SimRes *R = &G->r;
    const int L = u->L, T = 2 * L, N = c->N & ~1, fid = fidof(L), nwmin = L / 8;
    int W = 1; while (W * W < N) W++;
    memset(G, 0, sizeof *G); R->t_emerge = -1; R->ess = -1;
    uint8_t *s = malloc((size_t)N * L);
    int *pa = malloc(sizeof(int) * (size_t)N), *pb = malloc(sizeof(int) * (size_t)N);
    Rng rng; rng_seed(&rng, c->seed ^ 0x554E4956ULL);
    for (size_t i = 0; i < (size_t)N * L; i++) s[i] = (uint8_t)rng_next(&rng);
    for (int k = 0; k < c->ninject && k < N; k++) memcpy(s + (size_t)k * L, c->inject, L);
    double pmut = MUTS[u->mut_i], pcsum = 0;
    for (int e = 1; e <= c->E; e++) {
        int o = (int)rng_below(&rng, 2), ph = (int)rng_below(&rng, 2);
        int np = ss_tiling(W, o, ph, pa, pb);
        int pcmax = 0;
        for (int i = 0; i < np; i++) {
            int x = pa[i], y = pb[i];
            if (rng_below(&rng, 2)) { int t = x; x = y; y = t; }
            uint8_t tape[TMAX], old[TMAX], wr[TMAX];
            memset(wr, 0, T);
            uint8_t *qa = s + (size_t)x * L, *qb = s + (size_t)y * L;
            memcpy(tape, qa, L); memcpy(tape + L, qb, L); memcpy(old, tape, T);
            u_exec(tape, u, wr, c->nostore, 0);
            for (int h = 0; h < 2; h++) {
                const uint8_t *dst = tape + h * L, *src = old + (1 - h) * L, *d0 = old + h * L;
                int nw = 0, pcn = 0;
                for (int j = 0; j < L; j++) if (wr[h * L + j] && dst[j] != d0[j]) { nw++; pcn += dst[j] == src[j]; }
                if (pcn > pcmax) pcmax = pcn;
                if (nw >= nwmin && same(dst, src, L) >= fid && same(d0, src, L) < fid) {
                    R->births++;
                    if (R->fb_checks < 1500) { R->fb_checks++; R->fbirths += u_copier(src, u) != 0; }
                }
            }
            memcpy(qa, tape, L); memcpy(qb, tape + L, L);
        }
        pcsum += (double)pcmax / L;
        if (pmut > 0) {
            double lq = log(1.0 - pmut); size_t tot = (size_t)N * L, i = 0;
            for (;;) { i += (size_t)(log(1.0 - rng_double(&rng)) / lq); if (i >= tot) break; s[i] = (uint8_t)rng_next(&rng); i++; }
        }
        R->epochs_run = e;
        if (e % CK == 0 || e == c->E) {
            double cf = sample_copiers(s, N, u, c->seed * 31 + (uint64_t)e, NULL, NULL);
            if (G->ncurve < SS_CKMAX) G->curve[G->ncurve++] = (float)cf;
            R->cfrac_final = cf; if (cf > R->cfrac_max) R->cfrac_max = cf;
            if (cf >= 0.25 && !R->emerged) { R->emerged = 1; R->t_emerge = e; }
            if (R->emerged && !c->full && e >= R->t_emerge + 2 * CK) break;
        }
    }
    R->pc = pcsum / (R->epochs_run ? R->epochs_run : 1);
    sample_copiers(s, N, u, c->seed * 31 + 7777, R->dom, &R->dom_count);
    R->has_dom = R->dom_count > 0;
    if (R->has_dom && R->emerged) R->ess = u_ablate(R->dom, u, R->ess_mask);
    if (R->emerged && R->has_dom) {
        int seen[256] = { 0 }, nd = 0; for (int i = 0; i < L; i++) if (!seen[R->dom[i]]++) nd++;
        R->crystal = nd <= 2;
    }
    R->any_emerged = R->emerged;
    if (R->crystal) R->emerged = 0;
    R->hash = fnv1a(s, (size_t)N * L, FNV_INIT);
    free(s); free(pa); free(pb);
}

/* ---------------- travaux parallèles ---------------- */
typedef struct { const char *bras; int arm, seed; Univ u; SimCfg c; GRes g; double secs; } SJob;
typedef struct { SJob *j; int n, next; pthread_mutex_t mu; } SQ;
static void *ss_worker(void *arg) {
    SQ *q = arg;
    for (;;) {
        pthread_mutex_lock(&q->mu); int i = q->next++; pthread_mutex_unlock(&q->mu);
        if (i >= q->n) return NULL;
        SJob *j = &q->j[i];
        struct timespec t0, t1; clock_gettime(CLOCK_MONOTONIC, &t0);
        if (j->arm == ARM_G) sim_grid(&j->u, &j->c, &j->g);
        else { memset(&j->g, 0, sizeof j->g); sim(&j->u, &j->c, &j->g.r); }
        clock_gettime(CLOCK_MONOTONIC, &t1);
        j->secs = (t1.tv_sec - t0.tv_sec) + (t1.tv_nsec - t0.tv_nsec) * 1e-9;
        fprintf(stderr, "fini %s graine %d : %.0f s\n", j->bras, j->seed, j->secs);
    }
}
static void ss_run(SJob *j, int n, int nt) {
    SQ q = { j, n, 0, PTHREAD_MUTEX_INITIALIZER };
    pthread_t th[64]; if (nt > 64) nt = 64;
    for (int i = 0; i < nt; i++) pthread_create(&th[i], NULL, ss_worker, &q);
    for (int i = 0; i < nt; i++) pthread_join(th[i], NULL);
}

static int ss_pass(const SimRes *r) { return r->emerged && !r->crystal && r->cfrac_final >= 0.25; }

static void ss_print(const SJob *j) {
    const SimRes *r = &j->g.r;
    printf("%s\t%d\t%d\t%d\t%d\t%.3f\t%.3f\t%ld\t%ld/%d\t%d\t%d\t%d\t",
           j->bras, j->seed, ss_pass(r), r->any_emerged, r->t_emerge, r->cfrac_max, r->cfrac_final,
           r->births, r->fbirths, r->fb_checks, r->crystal, r->ess, r->epochs_run);
    if (r->has_dom) for (int i = 0; i < 64; i++) printf("%02x", r->dom[i]); else printf("-");
    printf("\t%016llx\t%.0f\n", (unsigned long long)r->hash, j->secs);
    if (j->arm == ARM_G && j->g.ncurve) {
        printf("#courbe %s %d :", j->bras, j->seed);
        for (int k = 15; k < j->g.ncurve; k += 16) printf(" %.2f", j->g.curve[k]);
        printf("\n");
    }
}

/* ---------------- selftest ---------------- */
static int ss_selftest(void) {
    int ok = 1;
    /* 1. pavage : chaque case une fois, partenaire voisin, pour les 4 tirages */
    {
        int W = 64, pa[4096], pb[4096], good = 1;
        for (int o = 0; o < 2; o++) for (int ph = 0; ph < 2; ph++) {
            int seen[4096] = { 0 }, n = ss_tiling(W, o, ph, pa, pb);
            if (n != W * W / 2) good = 0;
            for (int i = 0; i < n; i++) {
                seen[pa[i]]++; seen[pb[i]]++;
                int ax = pa[i] % W, ay = pa[i] / W, bx = pb[i] % W, by = pb[i] / W;
                int dx = (bx - ax + W) % W, dy = (by - ay + W) % W;
                if (!((dx == 1 && dy == 0) || (dx == 0 && dy == 1))) good = 0;
            }
            for (int k = 0; k < W * W; k++) if (seen[k] != 1) good = 0;
        }
        printf("selftest pavage : 4 tirages, chaque case une fois avec une voisine %s\n", good ? "OK" : "ÉCHEC");
        ok &= good;
    }
    Univ u; u_origin(&u, 8192);
    uint8_t f[64], cr[64];
    ss_hex(f, 64, SS_FOUNDER); ss_hex(cr, 64, SS_CRYSTAL);
    /* 2. détecteur : fondateur copieur, cristal non */
    {
        int a = u_copier(f, &u) != 0, b = u_copier(cr, &u) != 0;
        printf("selftest détecteur : big_b_0 copieur=%d, cristal 00 11 … ff copieur=%d %s\n", a, b, a && !b ? "OK" : "ÉCHEC");
        ok &= a && !b;
    }
    /* 3. déterminisme, sans STORE : zéro naissance, injection : invasion */
    {
        SimCfg c; memset(&c, 0, sizeof c); c.N = 256; c.E = 256; c.full = 1; c.seed = 5;
        GRes *g1 = malloc(sizeof *g1), *g2 = malloc(sizeof *g2), *g3 = malloc(sizeof *g3), *g4 = malloc(sizeof *g4);
        sim_grid(&u, &c, g1); sim_grid(&u, &c, g2);
        int det = g1->r.hash == g2->r.hash && g1->r.births == g2->r.births;
        SimCfg cn = c; cn.nostore = 1; sim_grid(&u, &cn, g3);
        SimCfg ci = c; ci.inject = f; ci.ninject = 16; sim_grid(&u, &ci, g4);
        int good = det && g3->r.births == 0 && g4->r.emerged && g4->r.cfrac_final >= 0.25;
        printf("selftest soupe grille : déterministe=%d, sans STORE naissances=%ld, injection 16/256 copieurs fin %.2f (%ld naissances) %s\n",
               det, g3->r.births, g4->r.cfrac_final, g4->r.births, good ? "OK" : "ÉCHEC");
        ok &= good;
        free(g1); free(g2); free(g3); free(g4);
    }
    return ok;
}

int main(int argc, char **argv) {
    if (argc > 1 && strcmp(argv[1], "selftest") == 0) return ss_selftest() ? 0 : 1;
    if (argc < 2 || strcmp(argv[1], "run")) { fprintf(stderr, "usage : %s selftest | run [--threads N]\n", argv[0]); return 2; }
    int nt = 32;
    for (int i = 2; i + 1 < argc; i++) if (!strcmp(argv[i], "--threads")) nt = atoi(argv[i + 1]);
    struct timespec t0, t1; clock_gettime(CLOCK_MONOTONIC, &t0);
    static uint8_t founder[64]; ss_hex(founder, 64, SS_FOUNDER);
    int n = 2 * SS_SEEDS + SS_NOISE + SS_POS, k = 0;
    SJob *jobs = calloc((size_t)n, sizeof *jobs);
    /* les plus longs d'abord : contrôle positif à la fin */
    for (int s = 0; s < SS_SEEDS; s++) for (int arm = ARM_G; arm >= ARM_M; arm--) {
        SJob *j = &jobs[k++]; j->bras = arm == ARM_G ? "G" : "M"; j->arm = arm; j->seed = s;
        u_origin(&j->u, 8192); j->c.N = SS_N; j->c.E = SS_E; j->c.full = 1; j->c.seed = 0x5050A7ULL + (uint64_t)s;
    }
    for (int s = 0; s < SS_NOISE; s++) {
        SJob *j = &jobs[k++]; j->bras = "bruit"; j->arm = ARM_G; j->seed = s;
        u_origin(&j->u, 8192); j->c.N = SS_N; j->c.E = SS_E; j->c.full = 1; j->c.nostore = 1; j->c.seed = 0x5050A7ULL + (uint64_t)s;
    }
    for (int s = 0; s < SS_POS; s++) {
        SJob *j = &jobs[k++]; j->bras = "positif"; j->arm = ARM_G; j->seed = s;
        u_origin(&j->u, 8192); j->c.N = SS_N; j->c.E = SS_POS_E; j->c.full = 1; j->c.seed = 0x5050A7ULL + (uint64_t)s;
        j->c.inject = founder; j->c.ninject = SS_POS_INJ;
    }
    ss_run(jobs, n, nt);
    printf("# soupe spatiale : univers d'origine (adressage B, L 64, 8192 pas, mutation 2,4e-4), N=%d, E=%d\n", SS_N, SS_E);
    printf("# passe = réplicateur non cristallin >= 0,25 à un point de contrôle ET encore >= 0,25 à la fin\n");
    printf("bras\tgraine\tpasse\temerge\tt_emerge\tcop_max\tcop_fin\tnaissances\tnaiss_par_copieur\tcristal\tessentiels\tepoques\tdominant\thash\tsecondes\n");
    int pass[4] = { 0 }; long births_noise = 0; double cop_noise = 0;
    for (int i = 0; i < n; i++) {
        ss_print(&jobs[i]);
        const SimRes *r = &jobs[i].g.r;
        if (!strcmp(jobs[i].bras, "G")) pass[0] += ss_pass(r);
        else if (!strcmp(jobs[i].bras, "M")) pass[1] += ss_pass(r);
        else if (!strcmp(jobs[i].bras, "bruit")) { births_noise += r->births; if (r->cfrac_max > cop_noise) cop_noise = r->cfrac_max; }
        else pass[3] += r->cfrac_final >= 0.25;
    }
    Univ u; u_origin(&u, 8192); uint8_t cr[64]; ss_hex(cr, 64, SS_CRYSTAL);
    int crys = u_copier(cr, &u) != 0;
    printf("\n# comptes\n");
    printf("G (grille) passe : %d/%d   seuil >= 8\n", pass[0], SS_SEEDS);
    printf("M (mélange) passe : %d/%d   seuil <= 2\n", pass[1], SS_SEEDS);
    printf("bruit (G sans STORE) : %ld naissances, copieurs max %.3f sur %d graines   seuil 0\n", births_noise, cop_noise, SS_NOISE);
    printf("cristal 00 11 … ff : copieur=%d   seuil 0\n", crys);
    printf("contrôle positif (G, %d copies, %d époques) : invasion %d/%d   seuil >= 6\n", SS_POS_INJ, SS_POS_E, pass[3], SS_POS);
    int yes = pass[0] >= 8 && pass[1] <= 2 && births_noise == 0 && cop_noise == 0 && !crys && pass[3] >= 6;
    printf("verdict : %s\n", yes ? "OUI" : "NON");
    clock_gettime(CLOCK_MONOTONIC, &t1);
    fprintf(stderr, "soupe_spatiale : %.1f s\n", (t1.tv_sec - t0.tv_sec) + (t1.tv_nsec - t0.tv_nsec) / 1e9);
    free(jobs);
    return 0;
}
