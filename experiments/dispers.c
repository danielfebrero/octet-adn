/* Ensemble dispersé : fenêtres hors-grille, écarts non constants, plus de graines.
 * Lois fixes. src/, include/ et seeds/ ne bougent pas.
 *
 *   ./experiments/dispers selftest
 *   ./experiments/dispers run
 */
#define main us1_main
#include "universe_search.c"
#undef main

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

enum { DS_N = 128, DS_E = 64, DS_WIN = 64, DS_ABL = 8, DS_SEED = 32, DS_NOISE = 16, DS_CRYS = 8, DS_LAG = 32 };

static const char *DS_FOUNDER =
    "051d03021c1fff0701030903c906901e051d03021c1fff0701030903c906901e"
    "051d03021c1fff0701030903c906901e051d03021c1fff0701030903c906901e";
static const char *DS_CRYSTAL = "00112233445566778899aabbccddeeff";

static void ds_hex(uint8_t *p, int n, const char *s) {
    for (int i = 0; i < n; i++) { unsigned x; sscanf(s + 2 * i, "%2x", &x); p[i] = (uint8_t)x; }
}

static int ds_period(const uint8_t *tape, int n, double *out) {
    for (int d = 1; d <= DS_LAG && d < n; d++) {
        int m = 0;
        for (int i = 0; i < n - d; i++) m += tape[i] == tape[i + d];
        double r = (double)m / (double)(n - d);
        if (r >= 0.5) { if (out) *out = r; return d; }
    }
    if (out) *out = 0;
    return 0;
}

static int ds_consensus_cop(const uint8_t *tape, int n, int p, const Univ *u) {
    int cnt[DS_LAG][256];
    memset(cnt, 0, sizeof cnt);
    uint8_t g[LMAX];
    for (int i = 0; i < n; i++) cnt[i % p][tape[i]]++;
    for (int i = 0; i < u->L; i++) {
        int k = i % p, best = 0, bv = 0;
        for (int v = 0; v < 256; v++) if (cnt[k][v] > bv) { bv = cnt[k][v]; best = v; }
        g[i] = (uint8_t)best;
    }
    return u_copier(g, u) != 0;
}

typedef struct {
    int hits, off, n_ess, dispersed, pcop, period;
    double r;
    char set[80], spine[80], gaps[80];
    int births, t_take;
    uint64_t hash;
} DS;

static void ds_scan(const uint8_t *tape, int n, const Univ *u, Rng *rng, DS *J) {
    J->hits = J->off = J->n_ess = J->dispersed = 0;
    J->set[0] = J->spine[0] = J->gaps[0] = 0;
    char spines[DS_ABL][80], gaps[DS_ABL][80], sets[DS_ABL][80];
    int cnt[DS_ABL], ns = 0;
    memset(cnt, 0, sizeof cnt);
    int limit = n - u->L;
    if (limit < 1) return;
    for (int w = 0; w < DS_WIN; w++) {
        int off = (int)rng_below(rng, (uint32_t)limit);
        const uint8_t *win = tape + off;
        if (!u_copier(win, u)) continue;
        J->hits++;
        if (off % u->L) J->off++;
        if (J->hits > DS_ABL) continue;
        uint8_t ess[LMAX];
        int ne = u_ablate(win, u, ess);
        if (ne < 1) continue;
        char sp[80], gp[80], st[80];
        int sl = 0, gl = 0, ul = 0, prev = -1;
        uint8_t uniq[64]; int nu = 0;
        int gapv[64], ng = 0, same = 1;
        for (int i = 0; i < u->L; i++) if (ess[i]) {
            sl += snprintf(sp + sl, sizeof sp - sl, "%02x", win[i]);
            if (prev >= 0) {
                int g = i - prev;
                gapv[ng++] = g;
                if (ng > 1 && g != gapv[0]) same = 0;
                gl += snprintf(gp + gl, sizeof gp - gl, "%s%d", gl ? "." : "", g);
            }
            prev = i;
            int seen = 0;
            for (int k = 0; k < nu; k++) if (uniq[k] == win[i]) seen = 1;
            if (!seen) uniq[nu++] = win[i];
        }
        for (int a = 1; a < nu; a++) {
            uint8_t v = uniq[a]; int b = a;
            while (b > 0 && uniq[b - 1] > v) { uniq[b] = uniq[b - 1]; b--; }
            uniq[b] = v;
        }
        for (int k = 0; k < nu; k++) ul += snprintf(st + ul, sizeof st - ul, "%02x", uniq[k]);
        sp[sl] = gp[gl] = st[ul] = 0;
        int slot = -1;
        for (int s = 0; s < ns; s++) if (strcmp(spines[s], sp) == 0) { slot = s; break; }
        if (slot < 0 && ns < DS_ABL) {
            slot = ns++;
            snprintf(spines[slot], sizeof spines[slot], "%s", sp);
            snprintf(gaps[slot], sizeof gaps[slot], "%s", gp);
            snprintf(sets[slot], sizeof sets[slot], "%s", st);
        }
        if (slot >= 0) cnt[slot]++;
        (void)same; (void)ng;
    }
    int best = -1;
    for (int s = 0; s < ns; s++) if (best < 0 || cnt[s] > cnt[best]) best = s;
    if (best >= 0) {
        snprintf(J->spine, sizeof J->spine, "%s", spines[best]);
        snprintf(J->gaps, sizeof J->gaps, "%s", gaps[best]);
        snprintf(J->set, sizeof J->set, "%s", sets[best]);
        J->n_ess = (int)strlen(J->spine) / 2;
        int same = 1, first = -1, npart = 0;
        for (const char *p = J->gaps; *p; ) {
            int g = 0;
            while (*p && *p != '.') { g = g * 10 + (*p - '0'); p++; }
            if (*p == '.') p++;
            if (first < 0) first = g; else if (g != first) same = 0;
            npart++;
        }
        J->dispersed = J->n_ess >= 4 && J->n_ess <= 16 && npart >= 1 && !same;
    }
}

static void ds_evolve(uint8_t *s, int inject, int crystal, int seed, int epochs, DS *J) {
    const int N = DS_N, L = LMAX;
    Univ u; u_origin(&u, 8192);
    uint64_t sd = 0xD1500001ULL ^ (uint64_t)seed * 0x9E3779B97F4A7C15ULL;
    if (!inject) sd ^= 0xB0A5ULL;
    if (crystal) sd ^= 0xC275ULL;
    Rng init, pairs, mut, sample;
    rng_seed(&init, sd); rng_seed(&pairs, sd ^ 1); rng_seed(&mut, sd ^ 3); rng_seed(&sample, sd ^ 7);
    uint8_t founder[LMAX], crystalb[16];
    ds_hex(founder, L, DS_FOUNDER);
    ds_hex(crystalb, 16, DS_CRYSTAL);
    for (int i = 0; i < N * L; i++) s[i] = (uint8_t)rng_next(&init);
    if (crystal) for (int i = 0; i < N * L; i++) s[i] = crystalb[i % 16];
    if (inject) {
        int pos = (int)rng_below(&init, (uint32_t)N);
        memcpy(s + (size_t)pos * L, founder, L);
    }
    J->births = 0; J->t_take = 0;
    const double pmut = crystal ? 0 : MUTS[u.mut_i];
    for (int e = 1; e <= epochs; e++) {
        int perm[DS_N];
        for (int i = 0; i < N; i++) perm[i] = i;
        for (int i = N - 1; i > 0; i--) {
            int j = (int)rng_below(&pairs, (uint32_t)(i + 1)), t = perm[i];
            perm[i] = perm[j]; perm[j] = t;
        }
        for (int i = 0; i < N; i += 2) {
            int ia = perm[i], ib = perm[i + 1];
            uint8_t tape[TMAX], olda[LMAX], oldb[LMAX];
            memcpy(olda, s + (size_t)ia * L, L);
            memcpy(oldb, s + (size_t)ib * L, L);
            memcpy(tape, olda, L); memcpy(tape + L, oldb, L);
            u_exec(tape, &u, NULL, 0, 0);
            int ch = 0; for (int k = 0; k < L; k++) ch += tape[L + k] != oldb[k];
            if (ch >= 8 && same(tape + L, olda, L) >= fidof(L) && same(oldb, olda, L) < fidof(L)) J->births++;
            memcpy(s + (size_t)ia * L, tape, L);
            memcpy(s + (size_t)ib * L, tape + L, L);
        }
        if (pmut > 0) {
            double lq = log(1.0 - pmut); size_t i = 0;
            for (;;) {
                double uu = rng_double(&mut); if (uu > 0.999999) uu = 0.999999;
                i += (size_t)(log(1.0 - uu) / lq);
                if (i >= (size_t)N * L) break;
                s[i++] = (uint8_t)rng_next(&mut);
            }
        }
        if (e % 32 == 0 && !J->t_take) {
            int ok = 0;
            for (int k = 0; k < 16; k++) ok += u_copier(s + (size_t)rng_below(&sample, (uint32_t)N) * L, &u) != 0;
            if (ok * 2 >= 16) J->t_take = e;
        }
    }
    double r = 0;
    J->period = ds_period(s, N * L, &r);
    J->r = r;
    J->pcop = J->period && ds_consensus_cop(s, N * L, J->period, &u);
    J->hash = fnv1a(s, (size_t)N * L, FNV_INIT);
    Rng scan; rng_seed(&scan, sd ^ 9);
    Univ u2; u_origin(&u2, 8192);
    ds_scan(s, N * L, &u2, &scan, J);
}

static void ds_print(int seed, const char *bras, const DS *j) {
    printf("%d\t%s\t%d\t%.3f\t%d\t%d\t%d\t%d\t%d\t%d\t%s\t%s\t%s\n",
           seed, bras, j->period, j->r, j->pcop, j->hits, j->off, j->n_ess, j->dispersed,
           j->t_take, j->set, j->spine, j->gaps);
}

static int ds_selftest(void) {
    Univ u; u_origin(&u, 8192);
    uint8_t f[LMAX], rot[LMAX], c[LMAX], mot[16];
    ds_hex(f, LMAX, DS_FOUNDER);
    ds_hex(mot, 16, DS_CRYSTAL);
    for (int i = 0; i < LMAX; i++) c[i] = mot[i % 16];
    for (int i = 0; i < LMAX; i++) rot[i] = f[(i + 1) % LMAX];
    uint8_t rot8[LMAX];
    for (int i = 0; i < LMAX; i++) rot8[i] = f[(i + 8) % LMAX];
    int cop = u_copier(f, &u) && u_copier(rot, &u) && !u_copier(rot8, &u) && !u_copier(c, &u);
    uint8_t ess[LMAX];
    int ne = u_ablate(f, &u, ess);
    int prev = -1, same = 1, first = -1, ng = 0, npos = 0;
    for (int i = 0; i < LMAX; i++) if (ess[i]) {
        npos++;
        if (prev >= 0) {
            int g = i - prev;
            if (first < 0) first = g; else if (g != first) same = 0;
            ng++;
        }
        prev = i;
    }
    int disp = ne >= 4 && ne <= 16 && ng >= 1 && !same && npos == ne;
    uint8_t *tape = malloc((size_t)DS_N * LMAX);
    Rng r; rng_seed(&r, 3);
    for (int i = 0; i < DS_N * LMAX; i++) tape[i] = (uint8_t)rng_next(&r);
    double rr = 0;
    int noise = ds_period(tape, DS_N * LMAX, &rr) == 0;
    DS a = {0}, b = {0};
    ds_evolve(tape, 1, 0, 0, 8, &a);
    ds_evolve(tape, 1, 0, 0, 8, &b);
    int det = a.hash == b.hash && a.births == b.births;
    uint8_t raw[TMAX], saved[TMAX];
    rng_seed(&r, 1);
    for (int i = 0; i < TMAX; i++) saved[i] = raw[i] = (uint8_t)rng_next(&r);
    u_exec(raw, &u, NULL, 1, 0);
    int nostore = memcmp(raw, saved, TMAX) == 0;
    free(tape);
    printf("selftest rotations et cristal : %s\n", cop ? "OK" : "ÉCHEC");
    printf("selftest écarts dispersés du fondateur : %s  n=%d\n", disp ? "OK" : "ÉCHEC", ne);
    printf("selftest bruit sans période : %s\n", noise ? "OK" : "ÉCHEC");
    printf("selftest sans écriture : %s\n", nostore ? "OK" : "ÉCHEC");
    printf("selftest déterminisme (8 ép.) : %s\n", det ? "OK" : "ÉCHEC");
    return cop && disp && noise && nostore && det;
}

int main(int argc, char **argv) {
    if (argc > 1 && strcmp(argv[1], "selftest") == 0) return ds_selftest() ? 0 : 1;
    struct timespec t0, t1; clock_gettime(CLOCK_MONOTONIC, &t0);
    uint8_t *s = malloc((size_t)DS_N * LMAX);
    if (!s) { fprintf(stderr, "allocation impossible\n"); return 1; }
    printf("seed\tbras\tP\tr\tpcop\thits\thors\tess\tdisp\tprise\tensemble\tepine\tecarts\n");
    for (int seed = 0; seed < DS_SEED; seed++) {
        DS j; ds_evolve(s, 1, 0, seed, DS_E, &j);
        ds_print(seed, "fondateur", &j);
        fflush(stdout);
    }
    for (int seed = 0; seed < DS_NOISE; seed++) {
        DS j; ds_evolve(s, 0, 0, seed, DS_E, &j);
        ds_print(seed, "bruit", &j);
        fflush(stdout);
    }
    for (int seed = 0; seed < DS_CRYS; seed++) {
        DS j; ds_evolve(s, 0, 1, seed, 0, &j);
        ds_print(seed, "cristal", &j);
        fflush(stdout);
    }
    free(s);
    clock_gettime(CLOCK_MONOTONIC, &t1);
    fprintf(stderr, "dispers : %.1f s\n", (t1.tv_sec - t0.tv_sec) + (t1.tv_nsec - t0.tv_nsec) / 1e9);
    return 0;
}
