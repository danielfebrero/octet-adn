/* Joint : réplicateur né du bruit + lois locales héritées + ombre neutre.
 * src/, include/ et seeds/ ne bougent pas. Pas de WRITE_LAW, pas de nouveau taux.
 *
 *   ./experiments/join_wheel selftest
 *   ./experiments/join_wheel run
 */
#define main us1_main
#include "universe_search.c"
#undef main

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <time.h>

enum { JW_N = 512, JW_TAB = 256, JW_SAMPLE = 64, JW_STREAK = 4, JW_MINCAR = 3, JW_E = 1024, JW_K = 8, JW_MAP = 2048, JW_SEEDS = 4 };

/* big_b_0, soupe graine 0, archivé tel quel (experiments/soup-replicator). */
static const char *JW_FOUNDER =
    "051d03021c1fff0701030903c906901e051d03021c1fff0701030903c906901e"
    "051d03021c1fff0701030903c906901e051d03021c1fff0701030903c906901e";

static void jw_founder(uint8_t *p) {
    for (int i = 0; i < LMAX; i++) { unsigned x; sscanf(JW_FOUNDER + 2 * i, "%2x", &x); p[i] = (uint8_t)x; }
}

static void jw_bind(Univ *u, const Univ *skel, const uint8_t *tab) {
    *u = *skel;
    for (int q = 0; q < 4; q++) memcpy(u->tab[q], tab, JW_TAB);
    u->kper = 1;
}

typedef struct { uint64_t h; int streak; unsigned char used, present, counted; } Sp;

static Sp *sp_get(Sp *m, uint64_t h) {
    uint32_t i = (uint32_t)h & (JW_MAP - 1);
    for (int n = 0; n < JW_MAP; n++) {
        if (!m[i].used) { m[i].used = 1; m[i].h = h; m[i].streak = 0; return &m[i]; }
        if (m[i].h == h) return &m[i];
        i = (i + 1) & (JW_MAP - 1);
    }
    return NULL;
}

static int sp_sample(Sp *m, const uint8_t (*tab)[JW_TAB], int n, uint64_t origin) {
    uint64_t hs[JW_N];
    for (int i = 0; i < n; i++) hs[i] = fnv1a(tab[i], JW_TAB, FNV_INIT);
    for (int i = 0; i < JW_MAP; i++) m[i].present = 0;
    /* comptage par tri : une table compte si ≥ JW_MINCAR porteurs */
    uint64_t ord[JW_N];
    memcpy(ord, hs, (size_t)n * sizeof *ord);
    for (int i = 1; i < n; i++) {
        uint64_t v = ord[i]; int j = i;
        while (j > 0 && ord[j - 1] > v) { ord[j] = ord[j - 1]; j--; }
        ord[j] = v;
    }
    for (int i = 0; i < n; ) {
        int k = i + 1; while (k < n && ord[k] == ord[i]) k++;
        if (k - i >= JW_MINCAR) { Sp *s = sp_get(m, ord[i]); if (s) s->present = 1; }
        i = k;
    }
    int fresh = 0;
    for (int i = 0; i < JW_MAP; i++) if (m[i].used) {
        if (m[i].present) m[i].streak++; else m[i].streak = 0;
        if (m[i].streak >= JW_STREAK && !m[i].counted && m[i].h != origin) { m[i].counted = 1; fresh++; }
    }
    return fresh;
}

static int sp_cum(const Sp *m) { int c = 0; for (int i = 0; i < JW_MAP; i++) c += m[i].counted; return c; }

static int sp_distinct(const uint8_t (*tab)[JW_TAB], int n) {
    uint64_t hs[JW_N];
    for (int i = 0; i < n; i++) hs[i] = fnv1a(tab[i], JW_TAB, FNV_INIT);
    int d = 0;
    for (int i = 0; i < n; i++) {
        int seen = 0; for (int j = 0; j < i; j++) if (hs[j] == hs[i]) { seen = 1; break; }
        d += !seen;
    }
    return d;
}

typedef struct {
    int seed, inject, epochs, est;
    int t_take, samples, cop_end_n, cop_end_ok, cop_est_ok, cop_est_n;
    int real_cum, sh_cum;
    int distinct_r, distinct_s;
    int curve[24], ncurve;
    long inherit, births;
    double dist_r, dist_s;
    uint64_t hash;
} JW;

static double jw_dist(const uint8_t (*tab)[JW_TAB], const uint8_t *origin, int n) {
    long d = 0; for (int i = 0; i < n; i++) for (int b = 0; b < JW_TAB; b++) d += tab[i][b] != origin[b];
    return (double)d / n;
}

static void jw_run(JW *J) {
    const int N = JW_N, L = LMAX;
    uint8_t *s = malloc((size_t)N * L);
    uint8_t (*real)[JW_TAB] = malloc((size_t)N * JW_TAB);
    uint8_t (*sh)[JW_TAB] = malloc((size_t)N * JW_TAB);
    Sp *mr = calloc(JW_MAP, sizeof(Sp)), *ms = calloc(JW_MAP, sizeof(Sp));
    int *perm = malloc(sizeof(int) * (size_t)N);
    if (!s || !real || !sh || !mr || !ms || !perm) { fprintf(stderr, "allocation impossible\n"); exit(1); }
    Univ skel; u_origin(&skel, 8192);
    uint8_t origin[JW_TAB]; memcpy(origin, skel.tab[0], JW_TAB);
    uint64_t oh = fnv1a(origin, JW_TAB, FNV_INIT);
    uint64_t sd = 0x301A7E11ULL ^ (uint64_t)J->seed * 0x9E3779B97F4A7C15ULL ^ (J->inject ? 0 : 0xB0A5ULL);
    Rng init, pairs, drift, mut, shr;
    rng_seed(&init, sd); rng_seed(&pairs, sd ^ 1); rng_seed(&drift, sd ^ 2); rng_seed(&mut, sd ^ 3); rng_seed(&shr, sd ^ 4);
    for (int i = 0; i < N * L; i++) s[i] = (uint8_t)rng_next(&init);
    for (int i = 0; i < N; i++) { memcpy(real[i], origin, JW_TAB); memcpy(sh[i], origin, JW_TAB); }
    if (J->inject) { int pos = (int)rng_below(&init, (uint32_t)N); jw_founder(s + (size_t)pos * L); }
    J->t_take = 0; J->samples = 0; J->inherit = 0; J->births = 0; J->real_cum = 0; J->sh_cum = 0;
    const double pd = 0.9 / JW_K, lqd = log(1.0 - pd);
    for (int e = 1; e <= J->epochs; e++) {
        if (e > J->est) {
            for (size_t i = 0;;) {
                double u = rng_double(&drift); if (u > 0.999999) u = 0.999999;
                i += (size_t)(log(1.0 - u) / lqd);
                if (i >= (size_t)N) break;
                int b = (int)rng_below(&drift, JW_TAB), p = (int)rng_below(&drift, P_COUNT);
                real[i][b] = sh[i][b] = (uint8_t)p;
                i++;
            }
            if (e % JW_K == 0 && rng_double(&drift) < 0.1) switch (rng_below(&drift, 4)) {
                case 0: skel.rcost = (uint8_t)COSTS[rng_below(&drift, 3)]; break;
                case 1: skel.wcost = (uint8_t)COSTS[rng_below(&drift, 3)]; break;
                case 2: skel.steps = (uint16_t)STEPS[rng_below(&drift, 4)]; break;
                default: skel.mut_i = (uint8_t)rng_below(&drift, 5); break;
            }
        }
        for (int i = 0; i < N; i++) perm[i] = i;
        for (int i = N - 1; i > 0; i--) { int j = (int)rng_below(&pairs, (uint32_t)(i + 1)), t = perm[i]; perm[i] = perm[j]; perm[j] = t; }
        for (int i = 0; i < N; i += 2) {
            int ia = perm[i], ib = perm[i + 1];
            Univ w; jw_bind(&w, &skel, real[ia]);
            uint8_t tape[TMAX], olda[LMAX], oldb[LMAX];
            memcpy(olda, s + (size_t)ia * L, L); memcpy(oldb, s + (size_t)ib * L, L);
            memcpy(tape, olda, L); memcpy(tape + L, oldb, L);
            u_exec(tape, &w, NULL, 0, 0);
            int ch = 0; for (int k = 0; k < L; k++) ch += tape[L + k] != oldb[k];
            if (ch >= 8 && same(tape + L, olda, L) >= fidof(L) && same(oldb, olda, L) < fidof(L)) J->births++;
            memcpy(s + (size_t)ia * L, tape, L); memcpy(s + (size_t)ib * L, tape + L, L);
            if (e > J->est && 2 * ch >= L) {
                memcpy(real[ib], real[ia], JW_TAB);
                int donor = (int)rng_below(&shr, (uint32_t)N);
                memcpy(sh[ib], sh[donor], JW_TAB);
                J->inherit++;
            }
        }
        double pmut = MUTS[skel.mut_i];
        if (pmut > 0) {
            double lq = log(1.0 - pmut); size_t i = 0;
            for (;;) {
                double u = rng_double(&mut); if (u > 0.999999) u = 0.999999;
                i += (size_t)(log(1.0 - u) / lq);
                if (i >= (size_t)N * L) break;
                s[i++] = (uint8_t)rng_next(&mut);
            }
        }
        if (e % JW_SAMPLE == 0) {
            J->real_cum += sp_sample(mr, real, N, oh);
            J->sh_cum += sp_sample(ms, sh, N, oh);
            J->samples++;
            int ok = 0, ntry = 16;
            for (int k = 0; k < ntry; k++) {
                int ix = (int)rng_below(&pairs, (uint32_t)N);
                Univ w; jw_bind(&w, &skel, real[ix]);
                ok += u_copier(s + (size_t)ix * L, &w) != 0;
            }
            if (!J->t_take && ok * 2 >= ntry) J->t_take = e;
            if (J->ncurve < 24) J->curve[J->ncurve++] = ok;
            if (e == J->est) { J->cop_est_n = ntry; J->cop_est_ok = ok; }
        }
    }
    J->cop_end_n = 64; J->cop_end_ok = 0;
    for (int k = 0; k < J->cop_end_n; k++) {
        int ix = (int)rng_below(&pairs, (uint32_t)N);
        Univ w; jw_bind(&w, &skel, real[ix]);
        J->cop_end_ok += u_copier(s + (size_t)ix * L, &w) != 0;
    }
    J->distinct_r = sp_distinct(real, N); J->distinct_s = sp_distinct(sh, N);
    J->dist_r = jw_dist(real, origin, N); J->dist_s = jw_dist(sh, origin, N);
    J->real_cum = sp_cum(mr); J->sh_cum = sp_cum(ms);
    J->hash = fnv1a(s, (size_t)N * L, fnv1a(real, (size_t)N * JW_TAB, FNV_INIT));
    free(s); free(real); free(sh); free(mr); free(ms); free(perm);
}

static int jw_selftest(void) {
    uint8_t g[LMAX]; jw_founder(g);
    Univ o; u_origin(&o, 8192);
    int cop = u_copier(g, &o);
    JW a = { .seed = 0, .inject = 1, .epochs = 64 }, b = a;
    jw_run(&a); jw_run(&b);
    int det = a.hash == b.hash && a.inherit == b.inherit && a.births == b.births;
    /* sans écriture : aucune moitié ne change, donc aucun héritage */
    Univ w = o; uint8_t tape[TMAX], z[TMAX];
    for (int i = 0; i < TMAX; i++) tape[i] = z[i] = (uint8_t)(i * 17 + 3);
    jw_founder(tape); u_exec(tape, &w, NULL, 1, 0);
    int nostore = memcmp(tape, z, TMAX) != 0; /* le fondateur est en tête : on compare après une bande bruit+bruit */
    uint8_t noise[TMAX]; Rng r; rng_seed(&r, 1);
    for (int i = 0; i < TMAX; i++) noise[i] = tape[i] = (uint8_t)rng_next(&r);
    u_exec(tape, &w, NULL, 1, 0);
    nostore = memcmp(tape, noise, TMAX) == 0;
    printf("selftest fondateur copieur : %s\n", cop ? "OK" : "ÉCHEC");
    printf("selftest déterminisme (64 ép.) : %s  hash=%016llx héritages=%ld naissances=%ld\n",
           det ? "OK" : "ÉCHEC", (unsigned long long)a.hash, a.inherit, a.births);
    printf("selftest sans écriture : %s\n", nostore ? "OK" : "ÉCHEC");
    return cop && det && nostore;
}

int main(int argc, char **argv) {
    if (argc > 1 && strcmp(argv[1], "selftest") == 0) return jw_selftest() ? 0 : 1;
    clock_t t0 = clock();
    int est = (argc > 1 && strcmp(argv[1], "run-est") == 0) ? 256 : 0;
    int epochs = est ? est + JW_E : JW_E;
    printf("seed\tbras\tcop_est\tcop_fin\tt_take\treal_cum\tsh_cum\tdist_r\tdist_s\tdistinct_r\tdistinct_s\théritages\tnaissances\tcourbe\n");
    for (int s = 0; s < JW_SEEDS; s++) {
        JW j = { .seed = s, .inject = 1, .epochs = epochs, .est = est };
        jw_run(&j);
        printf("%d\t%s\t%d/%d\t%d/%d\t%d\t%d\t%d\t%.2f\t%.2f\t%d\t%d\t%ld\t%ld\t",
               s, est ? "apres" : "ensemble", j.cop_est_ok, j.cop_est_n ? j.cop_est_n : 16,
               j.cop_end_ok, j.cop_end_n, j.t_take, j.real_cum, j.sh_cum, j.dist_r, j.dist_s,
               j.distinct_r, j.distinct_s, j.inherit, j.births);
        for (int c = 0; c < j.ncurve; c++) printf("%s%d", c ? "," : "", j.curve[c]);
        printf("\n");
        fflush(stdout);
    }
    JW n = { .seed = 0, .inject = 0, .epochs = est ? est + 512 : 512, .est = est };
    jw_run(&n);
    printf("0\tbruit\t%d/%d\t%d/%d\t%d\t%d\t%d\t%.2f\t%.2f\t%d\t%d\t%ld\t%ld\t",
           n.cop_est_ok, n.cop_est_n ? n.cop_est_n : 16, n.cop_end_ok, n.cop_end_n, n.t_take,
           n.real_cum, n.sh_cum, n.dist_r, n.dist_s, n.distinct_r, n.distinct_s, n.inherit, n.births);
    for (int c = 0; c < n.ncurve; c++) printf("%s%d", c ? "," : "", n.curve[c]);
    printf("\n");
    fprintf(stderr, "join_wheel : %.1f s\n", (double)(clock() - t0) / CLOCKS_PER_SEC);
    return 0;
}
