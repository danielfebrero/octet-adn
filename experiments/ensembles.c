/* Ensembles : lois fixes, période sans connaître les coupures, génome déduit.
 * src/, include/ et seeds/ ne bougent pas.
 *
 *   ./experiments/ensembles selftest
 *   ./experiments/ensembles run
 */
#define main us1_main
#include "universe_search.c"
#undef main

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

enum { EN_N = 128, EN_E = 128, EN_SEEDS = 4, EN_LAG = 32 };

static const char *EN_FOUNDER =
    "051d03021c1fff0701030903c906901e051d03021c1fff0701030903c906901e"
    "051d03021c1fff0701030903c906901e051d03021c1fff0701030903c906901e";
/* 16 octets périodiques, choisis pour n'être pas un copieur (vérifié au selftest). */
static const char *EN_CRYSTAL = "00112233445566778899aabbccddeeff";

static void en_hex(uint8_t *p, int n, const char *s) {
    for (int i = 0; i < n; i++) { unsigned x; sscanf(s + 2 * i, "%2x", &x); p[i] = (uint8_t)x; }
}

/* Plus petit décalage dont la bande se répète au moins à moitié. 0 = pas d'ensemble. */
static int en_period(const uint8_t *tape, int n, double *out) {
    for (int d = 1; d <= EN_LAG && d < n; d++) {
        int m = 0;
        for (int i = 0; i < n - d; i++) m += tape[i] == tape[i + d];
        double r = (double)m / (double)(n - d);
        if (r >= 0.5) { if (out) *out = r; return d; }
    }
    if (out) *out = 0;
    return 0;
}

static int en_consensus(const uint8_t *tape, int n, int p, uint8_t *g, const Univ *u) {
    int cnt[EN_LAG][256];
    memset(cnt, 0, sizeof cnt);
    for (int i = 0; i < n; i++) cnt[i % p][tape[i]]++;
    uint8_t mot[EN_LAG];
    for (int k = 0; k < p; k++) {
        int best = 0, bv = 0;
        for (int v = 0; v < 256; v++) if (cnt[k][v] > bv) { bv = cnt[k][v]; best = v; }
        mot[k] = (uint8_t)best;
    }
    for (int i = 0; i < u->L; i++) g[i] = mot[i % p];
    return u_copier(g, u) != 0;
}

typedef struct {
    int seed, inject, epochs, crystal;
    int period, cop, ess, t_take, births;
    double r;
    uint8_t genome[LMAX];
    uint64_t hash;
} EN;

static void en_run(EN *J) {
    const int N = EN_N, L = LMAX;
    uint8_t *s = malloc((size_t)N * L);
    if (!s) { fprintf(stderr, "allocation impossible\n"); exit(1); }
    Univ u; u_origin(&u, 8192);
    uint64_t sd = 0xE55E0001ULL ^ (uint64_t)J->seed * 0x9E3779B97F4A7C15ULL;
    if (!J->inject) sd ^= 0xB0A5ULL;
    if (J->crystal) sd ^= 0xC275ULL;
    Rng init, pairs, mut, sample;
    rng_seed(&init, sd); rng_seed(&pairs, sd ^ 1); rng_seed(&mut, sd ^ 3); rng_seed(&sample, sd ^ 7);
    uint8_t founder[LMAX], crystal[16];
    en_hex(founder, L, EN_FOUNDER);
    en_hex(crystal, 16, EN_CRYSTAL);
    for (int i = 0; i < N * L; i++) s[i] = (uint8_t)rng_next(&init);
    if (J->crystal) {
        for (int i = 0; i < N * L; i++) s[i] = crystal[i % 16];
    }
    if (J->inject) {
        int pos = (int)rng_below(&init, (uint32_t)N);
        memcpy(s + (size_t)pos * L, founder, L);
    }
    J->births = 0; J->t_take = 0;
    const double pmut = MUTS[u.mut_i];
    for (int e = 1; e <= J->epochs; e++) {
        int perm[EN_N];
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
            for (int k = 0; k < 16; k++) {
                int ix = (int)rng_below(&sample, (uint32_t)N);
                ok += u_copier(s + (size_t)ix * L, &u) != 0;
            }
            if (ok * 2 >= 16) J->t_take = e;
        }
    }
    double r = 0;
    J->period = en_period(s, N * L, &r);
    J->r = r;
    J->cop = 0; J->ess = -1;
    memset(J->genome, 0, L);
    if (J->period) {
        J->cop = en_consensus(s, N * L, J->period, J->genome, &u);
        if (J->cop) {
            uint8_t ess[LMAX];
            J->ess = u_ablate(J->genome, &u, ess);
        }
    }
    J->hash = fnv1a(s, (size_t)N * L, FNV_INIT);
    free(s);
}

static void en_print(const char *bras, const EN *j) {
    printf("%d\t%s\t%d\t%.3f\t%d\t%d\t%d\t%ld\t", j->seed, bras, j->period, j->r, j->cop, j->ess, j->t_take, (long)j->births);
    if (j->period) for (int i = 0; i < j->period && i < 32; i++) printf("%02x", j->genome[i]);
    printf("\n");
}

static int en_selftest(void) {
    Univ u; u_origin(&u, 8192);
    uint8_t f[LMAX], c[LMAX], mot[16];
    en_hex(f, LMAX, EN_FOUNDER);
    en_hex(mot, 16, EN_CRYSTAL);
    for (int i = 0; i < LMAX; i++) c[i] = mot[i % 16];
    int cop_f = u_copier(f, &u) != 0;
    int cop_c = u_copier(c, &u) == 0;
    uint8_t tape[EN_N * LMAX];
    for (int i = 0; i < EN_N * LMAX; i++) tape[i] = f[i % LMAX];
    double r = 0;
    int p = en_period(tape, EN_N * LMAX, &r);
    uint8_t g[LMAX];
    int back = p == 16 && en_consensus(tape, EN_N * LMAX, p, g, &u) && memcmp(g, f, 16) == 0;
    for (int i = 0; i < EN_N * LMAX; i++) tape[i] = c[i % 16];
    int pc = en_period(tape, EN_N * LMAX, &r);
    int crystal_ok = pc > 0 && !en_consensus(tape, EN_N * LMAX, pc, g, &u);
    Rng rng; rng_seed(&rng, 9);
    for (int i = 0; i < EN_N * LMAX; i++) tape[i] = (uint8_t)rng_next(&rng);
    int noise_ok = en_period(tape, EN_N * LMAX, &r) == 0;
    Univ w = u; uint8_t raw[TMAX], saved[TMAX];
    rng_seed(&rng, 1);
    for (int i = 0; i < TMAX; i++) saved[i] = raw[i] = (uint8_t)rng_next(&rng);
    u_exec(raw, &w, NULL, 1, 0);
    int nostore = memcmp(raw, saved, TMAX) == 0;
    EN a = { .seed = 0, .inject = 1, .epochs = 16 }, b = a;
    en_run(&a); en_run(&b);
    int det = a.hash == b.hash && a.births == b.births;
    printf("selftest fondateur copieur : %s\n", cop_f ? "OK" : "ÉCHEC");
    printf("selftest cristal non copieur : %s\n", cop_c ? "OK" : "ÉCHEC");
    printf("selftest période 16 et consensus : %s\n", back ? "OK" : "ÉCHEC");
    printf("selftest cristal rejeté : %s\n", crystal_ok ? "OK" : "ÉCHEC");
    printf("selftest bruit sans période : %s\n", noise_ok ? "OK" : "ÉCHEC");
    printf("selftest sans écriture : %s\n", nostore ? "OK" : "ÉCHEC");
    printf("selftest déterminisme (16 ép.) : %s\n", det ? "OK" : "ÉCHEC");
    return cop_f && cop_c && back && crystal_ok && noise_ok && nostore && det;
}

int main(int argc, char **argv) {
    if (argc > 1 && strcmp(argv[1], "selftest") == 0) return en_selftest() ? 0 : 1;
    struct timespec t0, t1; clock_gettime(CLOCK_MONOTONIC, &t0);
    printf("seed\tbras\tP\tr\tcopieur\tess\tprise\tnaissances\tconsensus\n");
    Univ u; u_origin(&u, 8192);
    uint8_t f[LMAX], g[LMAX];
    en_hex(f, LMAX, EN_FOUNDER);
    uint8_t *pure = malloc((size_t)EN_N * LMAX);
    for (int i = 0; i < EN_N * LMAX; i++) pure[i] = f[i % LMAX];
    double r = 0; int p = en_period(pure, EN_N * LMAX, &r);
    int cop = p && en_consensus(pure, EN_N * LMAX, p, g, &u);
    int ess = -1; if (cop) { uint8_t em[LMAX]; ess = u_ablate(g, &u, em); }
    printf("-\tpur\t%d\t%.3f\t%d\t%d\t0\t0\t", p, r, cop, ess);
    for (int i = 0; i < p && i < 32; i++) printf("%02x", g[i]);
    printf("\n");
    uint8_t mot[16]; en_hex(mot, 16, EN_CRYSTAL);
    for (int i = 0; i < EN_N * LMAX; i++) pure[i] = mot[i % 16];
    p = en_period(pure, EN_N * LMAX, &r);
    cop = p && en_consensus(pure, EN_N * LMAX, p, g, &u);
    printf("-\tcristal\t%d\t%.3f\t%d\t-1\t0\t0\t", p, r, cop);
    for (int i = 0; i < p && i < 32; i++) printf("%02x", g[i]);
    printf("\n");
    Rng rng; rng_seed(&rng, 1);
    for (int i = 0; i < EN_N * LMAX; i++) pure[i] = (uint8_t)rng_next(&rng);
    p = en_period(pure, EN_N * LMAX, &r);
    printf("-\tbruit-pur\t%d\t%.3f\t0\t-1\t0\t0\t\n", p, r);
    free(pure);
    fflush(stdout);
    for (int s = 0; s < EN_SEEDS; s++) {
        EN j = { .seed = s, .inject = 1, .epochs = EN_E };
        en_run(&j);
        en_print("evolue", &j);
        fflush(stdout);
    }
    EN n = { .seed = 0, .inject = 0, .epochs = EN_E };
    en_run(&n);
    en_print("bruit", &n);
    clock_gettime(CLOCK_MONOTONIC, &t1);
    fprintf(stderr, "ensembles : %.1f s\n", (t1.tv_sec - t0.tv_sec) + (t1.tv_nsec - t0.tv_nsec) / 1e9);
    return 0;
}
