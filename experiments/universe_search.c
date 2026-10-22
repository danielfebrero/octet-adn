/* Recherche d'univers (experiments/universe-search) : on fait évoluer la PHYSIQUE, pas la vie.
 *
 * Un « univers » est un génome tiré d'une graine :
 *  - une table chimie[pos % kper][octet] -> primitive ÉLÉMENTAIRE (catalogue fermé ci-dessous),
 *    kper ∈ {1,2,4} : le sens d'un octet peut dépendre de sa position dans le programme ;
 *  - des lois du monde : longueur L des programmes (bande = 2L), mode d'adressage de LOAD/STORE k
 *    (abs : k ; b : B+k ; ip : position de l'instruction + k), coût en pas d'une lecture / écriture,
 *    budget de pas par interaction, taux de mutation de fond.
 * Dans chaque univers on lance une soupe sans SPLIT, protocole de src/soup.c généralisé à L : N
 * programmes appariés au hasard, concaténés, exécutés depuis 0, recoupés. Une naissance = une moitié
 * réécrite (≥ L/8 octets écrits ET changés) en copie fidèle (≥ L − L/8) de l'autre — détectée après coup.
 * Un réplicateur vérifié = copie fidèle sur 3 bruits sur 4, et sa copie aussi (comme soup_copier).
 *
 * GARDE-FOU ANTI-TRICHE : le catalogue ne contient que des opérations sur UN octet ou un registre.
 * Aucune primitive de copie de bloc, duplication ou SPLIT ; une instruction écrit au plus un octet de
 * la bande (vérifié dynamiquement dans le selftest, sur des univers aléatoires, mutés et croisés).
 * Le moteur des labos (vm.c, soup.c) n'est pas touché ; ce fichier a sa propre VM.
 *
 *   ./experiments/universe_search selftest
 *   ./experiments/universe_search run [--seed S] [--pop 32] [--gens 12] [--N 1024] [--epochs 512]
 *                                     [--fixL 0|16|32|64] [--threads 8] [--migrate 1]
 */
#define _GNU_SOURCE
#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "octet.h"

/* ---------------- catalogue de primitives (fermé) ---------------- */
enum {
    P_NOP, P_HALT, P_INC_A, P_DEC_A, P_INC_B, P_DEC_B, P_CLR_A, P_CLR_B, P_MOV_BA, P_MOV_AB,
    P_ADD, P_SUB, P_XOR, P_LOADI, P_AND, P_CLRI, P_LOAD, P_STORE, P_JMP, P_JZ, P_JNZ, P_LOOP,
    P_READ_H, P_WRITE_H, P_H0_INC, P_H0_DEC, P_H1_INC, P_H1_DEC, P_COUNT
};
static const char *PN[P_COUNT] = {
    "NOP", "HALT", "INC_A", "DEC_A", "INC_B", "DEC_B", "CLR_A", "CLR_B", "MOV_BA", "MOV_AB",
    "ADD", "SUB", "XOR", "LOADI", "AND", "CLRI", "LOAD", "STORE", "JMP", "JZ", "JNZ", "LOOP",
    "READ_H", "WRITE_H", "H0_INC", "H0_DEC", "H1_INC", "H1_DEC" };
/* classe de chaque primitive : ce qu'elle a le droit de toucher (sert au selftest anti-triche) */
enum { K_CTRL, K_REG, K_READ1, K_WRITE1 };
static const int PKIND[P_COUNT] = {
    K_CTRL, K_CTRL, K_REG, K_REG, K_REG, K_REG, K_REG, K_REG, K_REG, K_REG,
    K_REG, K_REG, K_REG, K_REG, K_REG, K_REG, K_READ1, K_WRITE1, K_CTRL, K_CTRL, K_CTRL, K_CTRL,
    K_READ1, K_WRITE1, K_REG, K_REG, K_REG, K_REG };
static int p_imm(int p) { return p == P_LOADI || p == P_AND || p == P_CLRI || p == P_LOAD || p == P_STORE || p == P_JMP || p == P_JZ || p == P_JNZ; }

enum { AD_ABS, AD_B, AD_IP, AD_COUNT };
static const char *ADN[AD_COUNT] = { "abs", "b", "ip" };
#define LMAX 64
#define TMAX 128
static const int LS[3] = { 16, 32, 64 };
static const int STEPS[4] = { 256, 512, 1024, 2048 };
static const int COSTS[3] = { 1, 2, 4 };
static const double MUTS[5] = { 0, 1e-4, 2.4e-4, 1e-3, 4e-3 };

typedef struct {
    uint8_t tab[4][256];
    uint8_t kper, L, addr, rcost, wcost, mut_i;
    uint16_t steps;
} Univ;

static int u_valid(const Univ *u) {
    if (!(u->kper == 1 || u->kper == 2 || u->kper == 4) || u->addr >= AD_COUNT || u->mut_i >= 5) return 0;
    if (!(u->L == 16 || u->L == 32 || u->L == 64) || u->rcost < 1 || u->wcost < 1 || u->steps < 1) return 0;
    for (int r = 0; r < 4; r++) for (int b = 0; b < 256; b++) if (u->tab[r][b] >= P_COUNT) return 0;
    return 1;
}

/* exécute la bande (2L octets) ; written marque les cases écrites ; check = selftest : ≤ 1 octet changé par instruction */
static inline int u_exec(uint8_t *t, const Univ *u, uint8_t *wr, int nostore, int check) {
    const int T = 2 * u->L, budget = u->steps, km = u->kper - 1;
    int ip = 0, steps = 0, h0 = 0, h1 = 0, k, ad;
    uint8_t a = 0, b = 0, before[TMAX];
    while (steps < budget && ip < T) {
        int pos = ip, p = u->tab[pos & km][t[ip++]];
        steps++;
        if (p_imm(p)) { if (ip >= T) break; k = t[ip++]; } else k = 0;
        if (check) memcpy(before, t, T);
        switch (p) {
        case P_HALT: return steps;
        case P_INC_A: a++; break;
        case P_DEC_A: a--; break;
        case P_INC_B: b++; break;
        case P_DEC_B: b--; break;
        case P_CLR_A: case P_CLRI: a = 0; break;
        case P_CLR_B: b = 0; break;
        case P_MOV_BA: b = a; break;
        case P_MOV_AB: a = b; break;
        case P_ADD: a += b; break;
        case P_SUB: a -= b; break;
        case P_XOR: a ^= b; break;
        case P_LOADI: a = (uint8_t)k; break;
        case P_AND: a &= (uint8_t)k; break;
        case P_LOAD:
            ad = (u->addr == AD_ABS ? k : u->addr == AD_B ? b + k : pos + k) % T;
            a = t[ad]; steps += u->rcost - 1; break;
        case P_STORE:
            ad = (u->addr == AD_ABS ? k : u->addr == AD_B ? b + k : pos + k) % T;
            if (!nostore) { t[ad] = a; if (wr) wr[ad] = 1; }
            steps += u->wcost - 1; break;
        case P_JMP: ip = (ip + k) % T; break;
        case P_JZ: if (a == 0) ip = (ip + k) % T; break;
        case P_JNZ: if (a != 0) ip = (ip + k) % T; break;
        case P_LOOP: ip = 0; break;
        case P_READ_H: a = t[h0]; steps += u->rcost - 1; break;
        case P_WRITE_H: if (!nostore) { t[h1] = a; if (wr) wr[h1] = 1; } steps += u->wcost - 1; break;
        case P_H0_INC: h0 = (h0 + 1) % T; break;
        case P_H0_DEC: h0 = (h0 + T - 1) % T; break;
        case P_H1_INC: h1 = (h1 + 1) % T; break;
        case P_H1_DEC: h1 = (h1 + T - 1) % T; break;
        default: break;
        }
        if (check) {
            int ch = 0; for (int i = 0; i < T; i++) ch += before[i] != t[i];
            if (ch > (PKIND[p] == K_WRITE1 ? 1 : 0)) return -1000000 - p;   /* violation */
        }
    }
    return steps;
}

/* ---------------- génome d'univers : tirage, mutation, croisement ---------------- */
static void u_random(Univ *u, Rng *r) {
    memset(u, 0, sizeof *u);
    u->kper = (uint8_t)(1 << rng_below(r, 3));
    for (int b = 0; b < 256; b++) for (int q = 0; q < 4; q++) u->tab[q][b] = (uint8_t)rng_below(r, P_COUNT);
    for (int q = u->kper; q < 4; q++) memcpy(u->tab[q], u->tab[q % u->kper], 256);
    u->L = (uint8_t)LS[rng_below(r, 3)];
    u->addr = (uint8_t)rng_below(r, AD_COUNT);
    u->rcost = (uint8_t)COSTS[rng_below(r, 3)];
    u->wcost = (uint8_t)COSTS[rng_below(r, 3)];
    u->steps = (uint16_t)STEPS[rng_below(r, 4)];
    u->mut_i = (uint8_t)rng_below(r, 5);
}
static void u_set_kper(Univ *u, int k) {
    int old = u->kper;
    if (k > old) for (int q = old; q < k; q++) memcpy(u->tab[q], u->tab[q % old], 256);
    u->kper = (uint8_t)k;
    for (int q = k; q < 4; q++) memcpy(u->tab[q], u->tab[q % k], 256);
}
static void u_mutate(Univ *u, Rng *r, int fixL) {
    int n = 1; while (n < 8 && rng_double(r) < 0.6) n++;
    for (int i = 0; i < n; i++) u->tab[rng_below(r, u->kper)][rng_below(r, 256)] = (uint8_t)rng_below(r, P_COUNT);
    for (int q = u->kper; q < 4; q++) memcpy(u->tab[q], u->tab[q % u->kper], 256);
    if (rng_double(r) < 0.3) {
        switch (rng_below(r, 7)) {
        case 0: u_set_kper(u, 1 << rng_below(r, 3)); break;
        case 1: if (!fixL) u->L = (uint8_t)LS[rng_below(r, 3)]; break;
        case 2: u->addr = (uint8_t)rng_below(r, AD_COUNT); break;
        case 3: u->rcost = (uint8_t)COSTS[rng_below(r, 3)]; break;
        case 4: u->wcost = (uint8_t)COSTS[rng_below(r, 3)]; break;
        case 5: u->steps = (uint16_t)STEPS[rng_below(r, 4)]; break;
        default: u->mut_i = (uint8_t)rng_below(r, 5); break;
        }
    }
}
/* croisement : deux points sur l'axe des octets (des blocs de « chimie » voisins restent liés), lois tirées chez l'un ou l'autre */
static void u_cross(Univ *c, const Univ *x, const Univ *y, Rng *r) {
    *c = *x;
    int i = (int)rng_below(r, 256), j = (int)rng_below(r, 256);
    if (i > j) { int t = i; i = j; j = t; }
    for (int q = 0; q < c->kper; q++) for (int b = i; b <= j; b++) c->tab[q][b] = y->tab[q % y->kper][b];
    for (int q = c->kper; q < 4; q++) memcpy(c->tab[q], c->tab[q % c->kper], 256);
    if (rng_below(r, 2)) c->L = y->L;
    if (rng_below(r, 2)) c->addr = y->addr;
    if (rng_below(r, 2)) c->rcost = y->rcost;
    if (rng_below(r, 2)) c->wcost = y->wcost;
    if (rng_below(r, 2)) c->steps = y->steps;
    if (rng_below(r, 2)) c->mut_i = y->mut_i;
}
/* univers d'origine : la soupe de src/soup.c en adressage b (0x13 NEURAL, seul écart, devient CLRI) */
static void u_origin(Univ *u, int steps) {
    memset(u, 0, sizeof *u);
    static const uint8_t m[32] = {
        P_NOP, P_INC_A, P_DEC_A, P_INC_B, P_DEC_B, P_CLR_A, P_STORE, P_LOAD,
        P_JMP, P_JZ, P_CLR_A, P_NOP, P_NOP, P_NOP, P_NOP, P_HALT,
        P_CLR_A, P_CLR_A, P_CLR_A, P_CLRI, P_NOP, P_MOV_BA, P_MOV_AB, P_ADD,
        P_SUB, P_XOR, P_NOP, P_JNZ, P_LOADI, P_CLRI, P_LOOP, P_AND };
    for (int q = 0; q < 4; q++) memcpy(u->tab[q], m, 32);
    u->kper = 1; u->L = 64; u->addr = AD_B; u->rcost = u->wcost = 1; u->steps = (uint16_t)steps; u->mut_i = 2;
}
static void u_desc(const Univ *u, char *o, size_t cap) {
    snprintf(o, cap, "L=%d kper=%d addr=%s rcost=%d wcost=%d steps=%d mut=%g", u->L, u->kper, ADN[u->addr], u->rcost, u->wcost, u->steps, MUTS[u->mut_i]);
}
static uint64_t u_hash(const Univ *u) {
    uint64_t h = fnv1a(u->tab, (size_t)256 * u->kper, FNV_INIT);
    uint8_t p[7] = { u->kper, u->L, u->addr, u->rcost, u->wcost, u->mut_i, (uint8_t)(u->steps >> 8) };
    return fnv1a(p, 7, h);
}

/* ---------------- réplicateurs : copie, vérification, ablation ---------------- */
static int same(const uint8_t *x, const uint8_t *y, int L) { int m = 0; for (int i = 0; i < L; i++) m += x[i] == y[i]; return m; }
static int fidof(int L) { return L - L / 8; }
static void noise(uint8_t *o, int L, int t) { Rng r; rng_seed(&r, 0x50555045ULL + (uint64_t)t); for (int i = 0; i < L; i++) o[i] = (uint8_t)rng_next(&r); }
static int copy_once(const uint8_t *p, int ord, int t, const Univ *u, uint8_t *child) {
    int L = u->L; uint8_t tape[TMAX];
    memcpy(tape + ord * L, p, L);
    noise(tape + (1 - ord) * L, L, t);
    u_exec(tape, u, NULL, 0, 0);
    if (child) memcpy(child, tape + (1 - ord) * L, L);
    return same(tape + (1 - ord) * L, p, L) >= fidof(L);
}
static int copies(const uint8_t *p, int ord, int t0, const Univ *u, uint8_t *child) {
    int ok = 0;
    for (int t = 0; t < 4; t++) {
        ok += copy_once(p, ord, t0 + t, u, t == 0 ? child : NULL);
        if (ok + (3 - t) < 3) return 0;
    }
    return ok >= 3 && (!child || same(child, p, u->L) >= fidof(u->L));
}
static int u_copier(const uint8_t *p, const Univ *u) {
    for (int ord = 0; ord < 2; ord++) {
        uint8_t ch[LMAX];
        if (copies(p, ord, 0, u, ch) && copies(ch, ord, 4, u, NULL)) return 1 + ord;
    }
    return 0;
}
static int u_ablate(const uint8_t *p, const Univ *u, uint8_t *ess) {
    if (!u_copier(p, u)) return -1;
    int n = 0, L = u->L;
    Rng r; rng_seed(&r, 0xAB1A7EULL);
    for (int i = 0; i < L; i++) {
        int broke = 0;
        for (int j = 0; j < 4; j++) {
            uint8_t q[LMAX]; memcpy(q, p, L);
            q[i] = (uint8_t)(p[i] + 1 + rng_below(&r, 255));
            broke += !u_copier(q, u);
        }
        ess[i] = broke >= 3; n += ess[i];
    }
    return n;
}

/* ---------------- une soupe dans un univers ---------------- */
#define SAMPLE 64
#define CK 64
#define MAXCK 64
typedef struct {
    int N, E, E1;          /* programmes, époques max, époque du tri précoce (0 = pas de tri) */
    int full;              /* 1 = aller jusqu'à E même après émergence */
    uint64_t seed;
    const uint8_t *inject; int ninject;
    int nostore;
} SimCfg;
typedef struct {
    long births, fbirths; int fb_checks, crystal, any_emerged; int emerged, t_emerge, epochs_run, early_stop;
    double cfrac_final, cfrac_max, pc;
    int ncurve; float curve[MAXCK];
    uint8_t dom[LMAX]; int has_dom, ess, dom_count;
    uint8_t ess_mask[LMAX];
    double frac_inj;        /* migration : part de l'échantillon final à ≥ L/2 octets de l'injecté */
    uint64_t hash;
    double fitness;
} SimRes;

static double sample_copiers(const uint8_t *s, int N, const Univ *u, uint64_t sd, uint8_t *dom, int *dcount) {
    Rng r; rng_seed(&r, sd);
    int L = u->L, nc = 0, best = 0, idx[SAMPLE], isc[SAMPLE];
    for (int i = 0; i < SAMPLE; i++) { idx[i] = (int)rng_below(&r, (uint32_t)N); isc[i] = u_copier(s + (size_t)idx[i] * L, u) != 0; nc += isc[i]; }
    if (dom) for (int i = 0; i < SAMPLE; i++) if (isc[i]) {
        int c = 0; for (int j = 0; j < SAMPLE; j++) c += !memcmp(s + (size_t)idx[i] * L, s + (size_t)idx[j] * L, L);
        if (c > best) { best = c; memcpy(dom, s + (size_t)idx[i] * L, L); }
    }
    if (dcount) *dcount = best;
    return (double)nc / SAMPLE;
}

static void sim(const Univ *u, const SimCfg *c, SimRes *R) {
    const int L = u->L, T = 2 * L, N = c->N & ~1, fid = fidof(L), nwmin = L / 8;
    memset(R, 0, sizeof *R); R->t_emerge = -1; R->ess = -1;
    uint8_t *s = malloc((size_t)N * L); int *perm = malloc(sizeof(int) * (size_t)N);
    Rng rng; rng_seed(&rng, c->seed ^ 0x554E4956ULL);
    for (size_t i = 0; i < (size_t)N * L; i++) s[i] = (uint8_t)rng_next(&rng);
    for (int k = 0; k < c->ninject && k < N; k++) memcpy(s + (size_t)k * L, c->inject, L);
    for (int i = 0; i < N; i++) perm[i] = i;
    double pmut = MUTS[u->mut_i], pcsum = 0;
    for (int e = 1; e <= c->E; e++) {
        for (int i = N - 1; i > 0; i--) { int j = (int)rng_below(&rng, (uint32_t)(i + 1)), t = perm[i]; perm[i] = perm[j]; perm[j] = t; }
        int pcmax = 0;
        for (int i = 0; i < N; i += 2) {
            uint8_t tape[TMAX], old[TMAX], wr[TMAX];
            memset(wr, 0, T);
            uint8_t *pa = s + (size_t)perm[i] * L, *pb = s + (size_t)perm[i + 1] * L;
            memcpy(tape, pa, L); memcpy(tape + L, pb, L); memcpy(old, tape, T);
            u_exec(tape, u, wr, c->nostore, 0);
            for (int h = 0; h < 2; h++) {
                const uint8_t *dst = tape + h * L, *src = old + (1 - h) * L, *d0 = old + h * L;
                int nw = 0, pcn = 0;
                for (int j = 0; j < L; j++) if (wr[h * L + j] && dst[j] != d0[j]) { nw++; pcn += dst[j] == src[j]; }
                if (pcn > pcmax) pcmax = pcn;
                if (nw >= nwmin && same(dst, src, L) >= fid && same(d0, src, L) < fid) {
                    R->births++;
                    if (R->fb_checks < 1500) { R->fb_checks++; R->fbirths += u_copier(src, u) != 0; }   /* naissance par un copieur vérifié */
                }
            }
            memcpy(pa, tape, L); memcpy(pb, tape + L, L);
        }
        pcsum += (double)pcmax / L;
        if (pmut > 0) {
            double lq = log(1.0 - pmut); size_t tot = (size_t)N * L, i = 0;
            for (;;) { i += (size_t)(log(1.0 - rng_double(&rng)) / lq); if (i >= tot) break; s[i] = (uint8_t)rng_next(&rng); i++; }
        }
        R->epochs_run = e;
        if (e % CK == 0 || e == c->E) {
            double cf = sample_copiers(s, N, u, c->seed * 31 + (uint64_t)e, NULL, NULL);
            if (R->ncurve < MAXCK) R->curve[R->ncurve++] = (float)cf;
            R->cfrac_final = cf; if (cf > R->cfrac_max) R->cfrac_max = cf;
            if (cf >= 0.25 && !R->emerged) { R->emerged = 1; R->t_emerge = e; }
            if (R->emerged && !c->full && e >= R->t_emerge + 2 * CK) break;
        }
        if (c->E1 && e == c->E1 && R->births == 0 && !c->full) { R->early_stop = 1; break; }
    }
    R->pc = pcsum / (R->epochs_run ? R->epochs_run : 1);
    double cf = sample_copiers(s, N, u, c->seed * 31 + 7777, R->dom, &R->dom_count);
    R->has_dom = R->dom_count > 0;
    (void)cf;
    if (R->has_dom && R->emerged) R->ess = u_ablate(R->dom, u, R->ess_mask);
    if (R->emerged && R->has_dom) {           /* cristal : réplicateur homopolymère (≤ 2 valeurs d'octet distinctes), sans information */
        int seen[256] = { 0 }, nd = 0; for (int i = 0; i < L; i++) if (!seen[R->dom[i]]++) nd++;
        R->crystal = nd <= 2;
    }
    R->any_emerged = R->emerged;
    if (R->crystal) R->emerged = 0;           /* « émergence » = réplicateur informationnel uniquement */
    if (c->ninject) {
        Rng r; rng_seed(&r, c->seed + 99); int m = 0;
        for (int i = 0; i < SAMPLE; i++) m += same(s + (size_t)rng_below(&r, (uint32_t)N) * L, c->inject, L) >= L / 2;
        R->frac_inj = (double)m / SAMPLE;
    }
    R->hash = fnv1a(s, (size_t)N * L, FNV_INIT);
    if (R->emerged) R->fitness = 3.0 + R->cfrac_final + (1.0 - (double)R->t_emerge / c->E);
    else R->fitness = (R->fbirths > 0 && !R->crystal ? 1.5 + 0.5 * fmin(1.0, log1p((double)R->fbirths) / log(65.0)) : 0.0) + 0.5 * fmin(1.0, log1p((double)R->births) / log1p((double)N)) + 0.4 * R->pc;
    free(s); free(perm);
}

/* ---------------- pool de threads ---------------- */
typedef struct { const Univ *u; SimCfg c; SimRes r; } Job;
typedef struct { Job *jobs; int n, next; pthread_mutex_t mu; } JobQ;
static void *worker(void *arg) {
    JobQ *q = arg;
    for (;;) {
        pthread_mutex_lock(&q->mu); int i = q->next++; pthread_mutex_unlock(&q->mu);
        if (i >= q->n) return NULL;
        sim(q->jobs[i].u, &q->jobs[i].c, &q->jobs[i].r);
    }
}
static int g_threads = 8;
static int g_cry[2];
static void run_jobs(Job *jobs, int n) {
    JobQ q = { jobs, n, 0, PTHREAD_MUTEX_INITIALIZER };
    pthread_t th[64]; int nt = g_threads < 64 ? g_threads : 64;
    for (int i = 0; i < nt; i++) pthread_create(&th[i], NULL, worker, &q);
    for (int i = 0; i < nt; i++) pthread_join(th[i], NULL);
}

/* ---------------- profil d'un univers ---------------- */
static void print_univ(FILE *o, const char *tag, const Univ *u, const SimRes *r) {
    char d[160]; u_desc(u, d, sizeof d);
    fprintf(o, "%s %s  [hash %016llx]\n", tag, d, (unsigned long long)u_hash(u));
    int cnt[P_COUNT] = { 0 };
    for (int q = 0; q < u->kper; q++) for (int b = 0; b < 256; b++) cnt[u->tab[q][b]]++;
    fprintf(o, "   table (entrées / %d) :", 256 * u->kper);
    for (int p = 0; p < P_COUNT; p++) if (cnt[p]) fprintf(o, " %s:%d", PN[p], cnt[p]);
    fprintf(o, "\n");
    if (r && r->has_dom && r->ess >= 0) {
        fprintf(o, "   réplicateur dominant (%d/%d de l'échantillon) : %d octets essentiels sur %d\n     ", r->dom_count, SAMPLE, r->ess, u->L);
        for (int i = 0; i < u->L; i++) fprintf(o, "%02x", r->dom[i]);
        fprintf(o, "\n     essentiels :");
        for (int i = 0; i < u->L; i++) if (r->ess_mask[i]) fprintf(o, " %d:%02x=%s", i, r->dom[i], PN[u->tab[i & (u->kper - 1)][r->dom[i]]]);
        fprintf(o, "\n");
    }
}

/* ---------------- recherche ---------------- */
typedef struct { Univ u; double fsum; int nev, nem; SimRes last; } Ind;
static int cmp_ind(const void *a, const void *b) {
    const Ind *x = a, *y = b; double fx = x->fsum / x->nev, fy = y->fsum / y->nev;
    return fx < fy ? 1 : fx > fy ? -1 : 0;
}
typedef struct { int N, E, E1, pop, gens, fixL; uint64_t seed; } Params;

static void set_cfg(SimCfg *c, const Params *P, uint64_t seed) {
    memset(c, 0, sizeof *c); c->N = P->N; c->E = P->E; c->E1 = P->E1; c->seed = seed;
}
static uint64_t mix(uint64_t a, uint64_t b) { Rng r; rng_seed(&r, a * 0x9E3779B97F4A7C15ULL ^ b); return rng_next(&r); }

/* arm 0 = sélection, 1 = témoin (même nombre d'évaluations, univers tirés au hasard sans sélection) */
static Ind *run_arm(const Params *P, int arm, FILE *o, int *nev_out, int *nem_out, Ind **all_out, int *nall) {
    Rng rng; rng_seed(&rng, mix(P->seed, 0xA000 + (uint64_t)arm));
    int pop = P->pop, total = pop * P->gens, nall_ = 0;
    Ind *cur = calloc((size_t)pop, sizeof *cur), *nxt = calloc((size_t)pop, sizeof *nxt);
    Ind *all = calloc((size_t)total, sizeof *all);
    Job *jobs = calloc((size_t)pop, sizeof *jobs);
    int nev = 0, nem = 0;
    for (int i = 0; i < pop; i++) { u_random(&cur[i].u, &rng); if (P->fixL) cur[i].u.L = (uint8_t)P->fixL; }
    fprintf(o, "\n## %s (pop %d × %d générations = %d évaluations, N=%d, E=%d, tri précoce à %d)\n", arm ? "TÉMOIN : univers aléatoires" : "SÉLECTION d'univers", pop, P->gens, total, P->N, P->E, P->E1);
    fprintf(o, "# gen  émergences/%d  fitness_moy  meilleure_moy(≥2 éval)  univers_fertiles_cumulés  arrêts_précoces  t\n", pop);
    struct timespec t0; clock_gettime(CLOCK_MONOTONIC, &t0);
    int cum_fertile = 0;
    uint64_t *seen = calloc((size_t)total, sizeof *seen); int nseen = 0;
    for (int g = 0; g < P->gens; g++) {
        for (int i = 0; i < pop; i++) { jobs[i].u = &cur[i].u; set_cfg(&jobs[i].c, P, mix(P->seed, ((uint64_t)arm << 40) ^ ((uint64_t)g << 20) ^ (uint64_t)i)); }
        run_jobs(jobs, pop);
        int em = 0, early = 0, cr = 0; double fs = 0;
        for (int i = 0; i < pop; i++) {
            SimRes *r = &jobs[i].r;
            cur[i].fsum += r->fitness; cur[i].nev++; cur[i].nem += r->emerged; cur[i].last = *r;
            em += r->emerged; early += r->early_stop; fs += r->fitness; cr += r->crystal; g_cry[arm] += r->crystal;
            nev++; nem += r->emerged;
            uint64_t h = u_hash(&cur[i].u); int found = -1;
            for (int k = 0; k < nseen; k++) if (seen[k] == h) { found = k; break; }
            if (found < 0) { seen[nseen++] = h; all[nall_] = cur[i]; nall_++; if (r->emerged) cum_fertile++; }
            else { for (int k = 0; k < nall_; k++) if (u_hash(&all[k].u) == h) { int was = all[k].nem > 0; all[k] = cur[i]; if (!was && cur[i].nem) cum_fertile++; break; } }
        }
        qsort(cur, (size_t)pop, sizeof *cur, cmp_ind);
        double best = 0; for (int i = 0; i < pop; i++) if (cur[i].nev >= 2) { best = cur[i].fsum / cur[i].nev; break; }
        struct timespec t1; clock_gettime(CLOCK_MONOTONIC, &t1);
        fprintf(o, "%4d  %2d (+%d cristaux)  %.3f  %.3f  %d  %d  %.0fs\n", g, em, cr, fs / pop, best, cum_fertile, early, (t1.tv_sec - t0.tv_sec) + (t1.tv_nsec - t0.tv_nsec) * 1e-9);
        fflush(o);
        if (g == P->gens - 1) break;
        if (arm == 1) { for (int i = 0; i < pop; i++) { memset(&nxt[i], 0, sizeof nxt[i]); u_random(&nxt[i].u, &rng); if (P->fixL) nxt[i].u.L = (uint8_t)P->fixL; } }
        else {
            int elite = pop / 4;
            for (int i = 0; i < elite; i++) nxt[i] = cur[i];               /* élites réévaluées sur une nouvelle graine (moyenne) */
            for (int i = elite; i < pop; i++) {
                memset(&nxt[i], 0, sizeof nxt[i]);
                int a = (int)rng_below(&rng, (uint32_t)(pop / 2)), b = (int)rng_below(&rng, (uint32_t)(pop / 2));
                if (rng_double(&rng) < 0.5) u_cross(&nxt[i].u, &cur[a].u, &cur[b].u, &rng); else nxt[i].u = cur[a].u;
                u_mutate(&nxt[i].u, &rng, P->fixL);
            }
        }
        Ind *t = cur; cur = nxt; nxt = t;
    }
    *nev_out = nev; *nem_out = nem;
    *all_out = all; *nall = nall_;
    free(nxt); free(jobs); free(seen);
    return cur;
}

/* revalidation : univers candidats sur des graines fraîches */
static void validate(const Params *P, Univ *U, int n, int nseeds, uint64_t salt, int *nem, SimRes *keep) {
    Job *jobs = calloc((size_t)(n * nseeds), sizeof *jobs);
    for (int i = 0; i < n; i++) for (int s = 0; s < nseeds; s++) {
        Job *j = &jobs[i * nseeds + s]; j->u = &U[i]; set_cfg(&j->c, P, mix(salt, (uint64_t)(i * 1000 + s)));
    }
    run_jobs(jobs, n * nseeds);
    for (int i = 0; i < n; i++) {
        nem[i] = 0; int got = 0;
        for (int s = 0; s < nseeds; s++) { SimRes *r = &jobs[i * nseeds + s].r; nem[i] += r->emerged; if (r->emerged && !got && keep) { keep[i] = *r; got = 1; } }
        if (!got && keep) keep[i] = jobs[i * nseeds].r;
    }
    free(jobs);
}

static int pick_fertile(Ind *all, int nall, Univ *out, int maxn) {
    /* univers distincts ayant émergé au moins une fois, triés par fitness moyenne */
    qsort(all, (size_t)nall, sizeof *all, cmp_ind);
    int n = 0;
    for (int i = 0; i < nall && n < maxn; i++) if (all[i].nem > 0) out[n++] = all[i].u;
    return n;
}

static void fit_to_L(const uint8_t *src, int Ls, uint8_t *dst, int Ld, uint64_t sd) {
    Rng r; rng_seed(&r, sd);
    for (int i = 0; i < Ld; i++) dst[i] = i < Ls ? src[i] : (uint8_t)rng_next(&r);
}

static int cmd_run(int argc, char **argv) {
    Params P = { .N = 1024, .E = 512, .E1 = 128, .pop = 32, .gens = 12, .fixL = 0, .seed = 1 };
    int nval = 4, migrate = 1;
    for (int i = 2; i + 1 < argc; i += 2) {
        if (!strcmp(argv[i], "--seed")) P.seed = strtoull(argv[i + 1], NULL, 0);
        else if (!strcmp(argv[i], "--pop")) P.pop = atoi(argv[i + 1]);
        else if (!strcmp(argv[i], "--gens")) P.gens = atoi(argv[i + 1]);
        else if (!strcmp(argv[i], "--N")) P.N = atoi(argv[i + 1]);
        else if (!strcmp(argv[i], "--epochs")) P.E = atoi(argv[i + 1]);
        else if (!strcmp(argv[i], "--early")) P.E1 = atoi(argv[i + 1]);
        else if (!strcmp(argv[i], "--fixL")) P.fixL = atoi(argv[i + 1]);
        else if (!strcmp(argv[i], "--threads")) g_threads = atoi(argv[i + 1]);
        else if (!strcmp(argv[i], "--val")) nval = atoi(argv[i + 1]);
        else if (!strcmp(argv[i], "--migrate")) migrate = atoi(argv[i + 1]);
        else { fprintf(stderr, "option inconnue %s\n", argv[i]); return 2; }
    }
    FILE *o = stdout;
    fprintf(o, "# universe_search run seed=%llu pop=%d gens=%d N=%d E=%d E1=%d fixL=%d threads=%d\n",
            (unsigned long long)P.seed, P.pop, P.gens, P.N, P.E, P.E1, P.fixL, g_threads);
    int nev[2], nem[2], nall[2]; Ind *all[2], *fin[2];
    for (int arm = 0; arm < 2; arm++) fin[arm] = run_arm(&P, arm, o, &nev[arm], &nem[arm], &all[arm], &nall[arm]);
    int fert_distinct[2] = { 0, 0 };
    for (int arm = 0; arm < 2; arm++) for (int i = 0; i < nall[arm]; i++) fert_distinct[arm] += all[arm][i].nem > 0;
    fprintf(o, "\n## Bilan brut\n");
    for (int arm = 0; arm < 2; arm++)
        fprintf(o, "%s : %d/%d évaluations avec émergence informationnelle (%.1f %%) + %d cristaux homopolymères, %d univers distincts évalués, %d fertiles au moins une fois\n",
                arm ? "témoin   " : "sélection", nem[arm], nev[arm], 100.0 * nem[arm] / nev[arm], g_cry[arm], nall[arm], fert_distinct[arm]);

    /* revalidation sur graines fraîches : 8 meilleurs fertiles de chaque bras + origine */
    enum { KV = 8 };
    Univ V[2][KV]; int nv[2]; int vem[2][KV]; SimRes vres[2][KV];
    fprintf(o, "\n## Revalidation : %d graines fraîches par univers (même échelle N=%d, E=%d)\n", nval, P.N, P.E);
    for (int arm = 0; arm < 2; arm++) {
        nv[arm] = pick_fertile(all[arm], nall[arm], V[arm], KV);
        validate(&P, V[arm], nv[arm], nval, mix(P.seed, 0xBEEF + (uint64_t)arm), vem[arm], vres[arm]);
        int tot = 0; for (int i = 0; i < nv[arm]; i++) tot += vem[arm][i];
        fprintf(o, "%s : %d univers fertiles retenus, émergence sur graines fraîches %d/%d (%.1f %%) ; par univers :",
                arm ? "témoin   " : "sélection", nv[arm], tot, nv[arm] * nval, nv[arm] ? 100.0 * tot / (nv[arm] * nval) : 0.0);
        for (int i = 0; i < nv[arm]; i++) fprintf(o, " %d", vem[arm][i]);
        fprintf(o, "\n");
    }
    Univ orig[2]; u_origin(&orig[0], 8192); u_origin(&orig[1], 2048);
    int oem[2]; SimRes ores[2];
    validate(&P, orig, 2, 2 * nval, mix(P.seed, 0x0816), oem, ores);
    fprintf(o, "origine (soup b, steps 8192) : %d/%d ; origine (steps 2048) : %d/%d   (à N=%d, E=%d)\n", oem[0], 2 * nval, oem[1], 2 * nval, P.N, P.E);

    fprintf(o, "\n## Profil des univers fertiles revalidés\n");
    for (int arm = 0; arm < 2; arm++) for (int i = 0; i < nv[arm]; i++) {
        char tag[64]; snprintf(tag, sizeof tag, "%s#%d (%d/%d)", arm ? "T" : "S", i, vem[arm][i], nval);
        print_univ(o, tag, &V[arm][i], &vres[arm][i]);
    }
    /* bilan agrégé des lois chez les fertiles (tous bras, ≥ 1 émergence en revalidation) vs tous les univers évalués du témoin */
    {
        int cL[3] = { 0 }, cA[AD_COUNT] = { 0 }, cK[3] = { 0 }, cS[4] = { 0 }, cM[5] = { 0 }, cR[3] = { 0 }, cW[3] = { 0 }, nf = 0;
        long prim[P_COUNT] = { 0 }, essp[P_COUNT] = { 0 }; long tot = 0, etot = 0; int esz[64], ne = 0;
        for (int arm = 0; arm < 2; arm++) for (int i = 0; i < nv[arm]; i++) if (vem[arm][i] > 0) {
            Univ *u = &V[arm][i]; nf++;
            cL[u->L == 16 ? 0 : u->L == 32 ? 1 : 2]++; cA[u->addr]++; cK[u->kper == 1 ? 0 : u->kper == 2 ? 1 : 2]++;
            for (int k = 0; k < 4; k++) if (STEPS[k] == u->steps) cS[k]++;
            cM[u->mut_i]++; for (int k = 0; k < 3; k++) { if (COSTS[k] == u->rcost) cR[k]++; if (COSTS[k] == u->wcost) cW[k]++; }
            for (int q = 0; q < u->kper; q++) for (int b = 0; b < 256; b++) { prim[u->tab[q][b]]++; tot++; }
            SimRes *r = &vres[arm][i];
            if (r->ess >= 0) { if (ne < 64) esz[ne++] = r->ess; for (int j = 0; j < u->L; j++) if (r->ess_mask[j]) { essp[u->tab[j & (u->kper - 1)][r->dom[j]]]++; etot++; } }
        }
        fprintf(o, "\nFertiles revalidés : %d\n  L 16/32/64 : %d/%d/%d   addr abs/b/ip : %d/%d/%d   kper 1/2/4 : %d/%d/%d\n", nf, cL[0], cL[1], cL[2], cA[0], cA[1], cA[2], cK[0], cK[1], cK[2]);
        fprintf(o, "  steps 256/512/1024/2048 : %d/%d/%d/%d   mut 0/1e-4/2.4e-4/1e-3/4e-3 : %d/%d/%d/%d/%d   rcost 1/2/4 : %d/%d/%d   wcost 1/2/4 : %d/%d/%d\n",
                cS[0], cS[1], cS[2], cS[3], cM[0], cM[1], cM[2], cM[3], cM[4], cR[0], cR[1], cR[2], cW[0], cW[1], cW[2]);
        fprintf(o, "  part des primitives dans la table (uniforme = %.3f) :", 1.0 / P_COUNT);
        for (int p = 0; p < P_COUNT; p++) fprintf(o, " %s=%.3f", PN[p], tot ? (double)prim[p] / tot : 0);
        fprintf(o, "\n  primitives des octets ESSENTIELS des réplicateurs (%ld octets) :", etot);
        for (int p = 0; p < P_COUNT; p++) if (essp[p]) fprintf(o, " %s=%ld", PN[p], essp[p]);
        fprintf(o, "\n  tailles des réplicateurs (octets essentiels) :");
        for (int i = 0; i < ne; i++) fprintf(o, " %d", esz[i]);
        fprintf(o, "\n");
    }

    if (migrate) {
        /* migration : le réplicateur dominant d'un univers fertile, 32 copies injectées dans le bruit d'un autre univers */
        Univ M[12]; SimRes MR[12]; char ml[12][24]; int nm = 0;
        for (int arm = 0; arm < 2; arm++) for (int i = 0; i < nv[arm] && nm < 6 + 2 * arm; i++)
            if (vem[arm][i] > 0 && vres[arm][i].has_dom && vres[arm][i].ess >= 0) { M[nm] = V[arm][i]; MR[nm] = vres[arm][i]; snprintf(ml[nm], 24, "%s#%d", arm ? "T" : "S", i); nm++; }
        int nsrc = nm;
        Rng rr; rng_seed(&rr, mix(P.seed, 0x5151));
        M[nm] = orig[0]; memset(&MR[nm], 0, sizeof MR[nm]); snprintf(ml[nm++], 24, "origine");
        for (int k = 0; k < 2 && nm < 12; k++) { u_random(&M[nm], &rr); memset(&MR[nm], 0, sizeof MR[nm]); snprintf(ml[nm++], 24, "aléa%d", k); }
        fprintf(o, "\n## Migration (%d sources × %d cibles) : 32 copies du réplicateur dominant de la source dans une soupe de bruit de la cible\n", nsrc, nm);
        fprintf(o, "# cellule = direct/invasion/témoin : direct = l'immigré est un copieur vérifié sous la physique cible (1/0) ;\n"
                   "# invasion = copieurs ≥ 25 %% et ≥ 25 %% de l'échantillon à ≥ L/2 octets de l'immigré après %d époques ;\n"
                   "# témoin = émergence spontanée dans la cible sans injection (même graine). Longueurs différentes : tronqué / complété de bruit.\n", P.E / 2);
        int nj = nsrc * nm; Job *jobs = calloc((size_t)(2 * nj), sizeof *jobs); uint8_t (*inj)[LMAX] = calloc((size_t)nj, LMAX);
        for (int s = 0; s < nsrc; s++) for (int t = 0; t < nm; t++) {
            int j = s * nm + t; fit_to_L(MR[s].dom, M[s].L, inj[j], M[t].L, (uint64_t)j);
            for (int w = 0; w < 2; w++) {
                Job *J = &jobs[2 * j + w]; J->u = &M[t]; memset(&J->c, 0, sizeof J->c);
                J->c.N = P.N; J->c.E = P.E / 2; J->c.full = 1; J->c.seed = mix(P.seed, 0x3000 + (uint64_t)j);
                if (w == 0) { J->c.inject = inj[j]; J->c.ninject = 32; }
            }
        }
        run_jobs(jobs, 2 * nj);
        fprintf(o, "%-10s", "src\\cible"); for (int t = 0; t < nm; t++) fprintf(o, " %10s", ml[t]); fprintf(o, "\n");
        int cross_ok = 0, cross_n = 0, self_ok = 0, direct_x = 0;
        for (int s = 0; s < nsrc; s++) {
            fprintf(o, "%-10s", ml[s]);
            for (int t = 0; t < nm; t++) {
                int j = s * nm + t; SimRes *ri = &jobs[2 * j].r, *rc = &jobs[2 * j + 1].r;
                int direct = u_copier(inj[j], &M[t]) != 0;
                int inv = ri->cfrac_final >= 0.25 && ri->frac_inj >= 0.25;
                fprintf(o, "      %d/%d/%d", direct, inv, rc->emerged);
                if (s == t) self_ok += inv; else { cross_n++; cross_ok += inv; direct_x += direct; }
            }
            fprintf(o, "\n");
        }
        fprintf(o, "auto-migration (témoin positif) : %d/%d envahissent ; migrations croisées : %d/%d envahissent, %d/%d immigrés copieurs directs\n",
                self_ok, nsrc, cross_ok, cross_n, direct_x, cross_n);
        free(jobs); free(inj);
    }
    free(fin[0]); free(fin[1]); free(all[0]); free(all[1]);
    return 0;
}

/* ---------------- selftest ---------------- */
static int selftest(void) {
    int fail = 0;
    Rng r; rng_seed(&r, 12345);
    /* 1. catalogue fermé : seules STORE et WRITE_H écrivent, chacune UN octet ; aucune primitive de bloc/duplication/SPLIT */
    {
        int nw = 0, bad = 0;
        for (int p = 0; p < P_COUNT; p++) {
            nw += PKIND[p] == K_WRITE1;
            const char *n = PN[p];
            if (strstr(n, "COPY") || strstr(n, "DUP") || strstr(n, "SPLIT") || strstr(n, "BLOCK") || strstr(n, "CLONE")) bad++;
        }
        int ok = nw == 2 && bad == 0 && PKIND[P_STORE] == K_WRITE1 && PKIND[P_WRITE_H] == K_WRITE1;
        printf("selftest catalogue : %d primitives, %d écrivent (1 octet), %d de bloc %s\n", P_COUNT, nw, bad, ok ? "OK" : "ÉCHEC");
        fail |= !ok;
    }
    /* 2. dynamique : ≤ 1 octet changé par instruction, sur univers aléatoires, mutés, croisés et tables d'écriture saturées */
    {
        long viol = 0, inval = 0, instr = 0;
        Univ a, b, c;
        for (int it = 0; it < 600; it++) {
            u_random(&a, &r); u_random(&b, &r); u_cross(&c, &a, &b, &r); for (int m = 0; m < 5; m++) u_mutate(&c, &r, 0);
            Univ *us[3] = { &a, &b, &c };
            if (it % 50 == 0) { for (int q = 0; q < 4; q++) for (int x = 0; x < 256; x++) c.tab[q][x] = (uint8_t)(x & 1 ? P_WRITE_H : (x & 2 ? P_H1_INC : P_STORE)); }
            for (int k = 0; k < 3; k++) {
                inval += !u_valid(us[k]);
                for (int t = 0; t < 4; t++) {
                    uint8_t tape[TMAX]; for (int i = 0; i < 2 * us[k]->L; i++) tape[i] = (uint8_t)rng_next(&r);
                    int st = u_exec(tape, us[k], NULL, 0, 1);
                    if (st <= -1000000) viol++; else instr += st;
                }
            }
        }
        int ok = viol == 0 && inval == 0;
        printf("selftest anti-triche : 1800 univers (aléatoires/croisés/mutés), %ld pas exécutés, %ld instructions écrivant > 1 octet, %ld génomes hors catalogue %s\n", instr, viol, inval, ok ? "OK" : "ÉCHEC");
        fail |= !ok;
    }
    /* 3. l'univers d'origine reproduit soup_exec (adressage b), hors 0x13 NEURAL */
    {
        Univ o; u_origin(&o, 8192); int diff = 0, excused = 0;
        SoupCfg sc = { .addr_b = 1, .steps = 8192 };
        for (int it = 0; it < 3000; it++) {
            uint8_t t1[TMAX], t2[TMAX], w1[TMAX] = { 0 }, w2[TMAX] = { 0 };
            for (int i = 0; i < TMAX; i++) { uint8_t x = (uint8_t)rng_next(&r); if (it & 1) x &= 0x1F; if (x == 0x13) x = 0; t1[i] = t2[i] = x; }
            int s1 = soup_exec(t1, &sc, w1), s2 = u_exec(t2, &o, w2, 0, 0);
            if (s1 != s2 || memcmp(t1, t2, TMAX) || memcmp(w1, w2, TMAX)) { if (memchr(t1, 0x13, TMAX) || memchr(t2, 0x13, TMAX)) excused++; else diff++; }
        }
        uint8_t h[SOUP_PROG], z[SOUP_PROG]; soup_handmade(h, 1); noise(z, 64, 777);
        int c1 = soup_copier(h, &sc) != 0, c2 = u_copier(h, &o) != 0, c3 = u_copier(z, &o) != 0;
        int ok = diff == 0 && c1 && c2 && !c3;
        printf("selftest origine = soup b : %d divergences sur 3000 bandes (%d où STORE a fabriqué un 0x13 NEURAL, écart documenté) ; copieur fait main reconnu %d/%d ; bruit copieur %d %s\n", diff, excused, c1, c2, c3, ok ? "OK" : "ÉCHEC");
        fail |= !ok;
    }
    /* 4. témoins de la soupe généralisée : injection envahit ; no-store = 0 naissance ; déterminisme */
    {
        Univ o; u_origin(&o, 1024); uint8_t h[SOUP_PROG]; soup_handmade(h, 1);
        SimCfg c = { .N = 512, .E = 128, .full = 1, .seed = 3, .inject = h, .ninject = 16 };
        SimRes a, b, n;
        sim(&o, &c, &a); sim(&o, &c, &b);
        c.nostore = 1; sim(&o, &c, &n);
        int ok = a.emerged && a.births > 0 && a.hash == b.hash && n.births == 0 && !n.emerged;
        printf("selftest soupe : injection émerge=%d (copieurs %.2f, %ld naissances), déterministe=%d, no-store naissances=%ld %s\n",
               a.emerged, a.cfrac_final, a.births, a.hash == b.hash, n.births, ok ? "OK" : "ÉCHEC");
        fail |= !ok;
    }
    printf("universe_search selftest : %s\n", fail ? "ÉCHEC" : "OK");
    return fail;
}

int main(int argc, char **argv) {
    if (argc >= 2 && !strcmp(argv[1], "selftest")) return selftest();
    if (argc >= 2 && !strcmp(argv[1], "run")) return cmd_run(argc, argv);
    fprintf(stderr, "usage : universe_search selftest | run [--seed S] [--pop 32] [--gens 12] [--N 1024] [--epochs 512] [--early 128] [--fixL 0|16|32|64] [--threads 8] [--val 4] [--migrate 1]\n");
    return 2;
}
