/* Monde v0 : réplicateur de soupe, lois locales à dérive lente, îles, nourriture passive, greffe d'une loi.
 * src/, include/ et seeds/ ne bougent pas. Pas de WRITE_LAW, pas de SPLIT.
 *
 *   ./experiments/monde_v0 selftest
 *   ./experiments/monde_v0 run
 */
#define main us1_main
#include "universe_search.c"
#undef main

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <time.h>

enum {
    MV_ISLES = 4, MV_CAP = 64, MV_START = 16, MV_TAB = 256,
    MV_EPOCHS = 1024, MV_SAMPLE = 64, MV_STREAK = 4, MV_MINCAR = 3,
    MV_MAP = 2048, MV_SEEDS = 4, MV_REGROW = 8, MV_FCAP = 32, MV_K = 512
};

static const char *MV_FOUNDER =
    "051d03021c1fff0701030903c906901e051d03021c1fff0701030903c906901e"
    "051d03021c1fff0701030903c906901e051d03021c1fff0701030903c906901e";

typedef struct {
    uint8_t g[LMAX];
    uint8_t law[MV_TAB];
    uint8_t sh[MV_TAB];
    unsigned char alive;
} Slot;

typedef struct { uint64_t h; int streak; unsigned char used, present, counted; } Sp;

typedef struct {
    int seed, inject, epochs, regrow;
    int t_take, cop_end, living_end, isles_cop;
    int real_cum, sh_cum;
    int isle_cop[MV_ISLES], isle_n[MV_ISLES];
    int curve[24], ncurve;
    long inherit, grafts, births, migs;
    double dist_r, dist_s;
    uint64_t hash;
} MV;

static void mv_founder(uint8_t *p) {
    for (int i = 0; i < LMAX; i++) { unsigned x; sscanf(MV_FOUNDER + 2 * i, "%2x", &x); p[i] = (uint8_t)x; }
}

static void mv_bind(Univ *u, const Univ *skel, const uint8_t *tab) {
    *u = *skel;
    for (int q = 0; q < 4; q++) memcpy(u->tab[q], tab, MV_TAB);
    u->kper = 1;
}

static Sp *sp_get(Sp *m, uint64_t h) {
    uint32_t i = (uint32_t)h & (MV_MAP - 1);
    for (int n = 0; n < MV_MAP; n++) {
        if (!m[i].used) { m[i].used = 1; m[i].h = h; m[i].streak = 0; return &m[i]; }
        if (m[i].h == h) return &m[i];
        i = (i + 1) & (MV_MAP - 1);
    }
    return NULL;
}

static int sp_sample(Sp *m, const uint8_t (*tab)[MV_TAB], int n, uint64_t origin) {
    uint64_t ord[MV_ISLES * MV_CAP];
    for (int i = 0; i < n; i++) ord[i] = fnv1a(tab[i], MV_TAB, FNV_INIT);
    for (int i = 0; i < MV_MAP; i++) m[i].present = 0;
    for (int i = 1; i < n; i++) {
        uint64_t v = ord[i]; int j = i;
        while (j > 0 && ord[j - 1] > v) { ord[j] = ord[j - 1]; j--; }
        ord[j] = v;
    }
    for (int i = 0; i < n; ) {
        int k = i + 1; while (k < n && ord[k] == ord[i]) k++;
        if (k - i >= MV_MINCAR) { Sp *s = sp_get(m, ord[i]); if (s) s->present = 1; }
        i = k;
    }
    int fresh = 0;
    for (int i = 0; i < MV_MAP; i++) if (m[i].used) {
        if (m[i].present) m[i].streak++; else m[i].streak = 0;
        if (m[i].streak >= MV_STREAK && !m[i].counted && m[i].h != origin) { m[i].counted = 1; fresh++; }
    }
    return fresh;
}

static int sp_cum(const Sp *m) { int c = 0; for (int i = 0; i < MV_MAP; i++) c += m[i].counted; return c; }

/* Transmet la table source, puis une entrée peut revenir à la table écrasée. */
static int mv_pass(uint8_t *dst, const uint8_t *src, const uint8_t *old, Rng *r, double p) {
    memcpy(dst, src, MV_TAB);
    if (rng_double(r) < p) {
        int b = (int)rng_below(r, MV_TAB);
        dst[b] = old[b];
        return 1;
    }
    return 0;
}

static int mv_migrate(Slot isle[][MV_CAP], Rng *r, double p) {
    int order[MV_ISLES * MV_CAP], n = 0, moves = 0;
    for (int q = 0; q < MV_ISLES; q++) for (int i = 0; i < MV_CAP; i++) if (isle[q][i].alive) order[n++] = q * MV_CAP + i;
    for (int i = n - 1; i > 0; i--) {
        int j = (int)rng_below(r, (uint32_t)(i + 1)), t = order[i]; order[i] = order[j]; order[j] = t;
    }
    for (int k = 0; k < n; k++) {
        if (rng_double(r) >= p) continue;
        int q = order[k] / MV_CAP, i = order[k] % MV_CAP;
        if (!isle[q][i].alive) continue;
        int dest = (q + 1) % MV_ISLES, empty[MV_CAP], ne = 0;
        for (int j = 0; j < MV_CAP; j++) if (!isle[dest][j].alive) empty[ne++] = j;
        if (!ne) continue;
        int j = empty[rng_below(r, (uint32_t)ne)];
        isle[dest][j] = isle[q][i];
        isle[q][i].alive = 0;
        moves++;
    }
    return moves;
}

static int mv_living(const Slot isle[][MV_CAP], int *ids) {
    int n = 0;
    for (int q = 0; q < MV_ISLES; q++) for (int i = 0; i < MV_CAP; i++) if (isle[q][i].alive) ids[n++] = q * MV_CAP + i;
    return n;
}

static void mv_run(MV *J) {
    Slot (*isle)[MV_CAP] = calloc((size_t)MV_ISLES * MV_CAP, sizeof(Slot));
    Sp *mr = calloc(MV_MAP, sizeof(Sp)), *ms = calloc(MV_MAP, sizeof(Sp));
    uint8_t (*laws)[MV_TAB] = malloc((size_t)MV_ISLES * MV_CAP * MV_TAB);
    uint8_t (*shs)[MV_TAB] = malloc((size_t)MV_ISLES * MV_CAP * MV_TAB);
    if (!isle || !mr || !ms || !laws || !shs) { fprintf(stderr, "allocation impossible\n"); exit(1); }
    Univ skel; u_origin(&skel, 8192);
    uint8_t origin[MV_TAB]; memcpy(origin, skel.tab[0], MV_TAB);
    uint64_t oh = fnv1a(origin, MV_TAB, FNV_INIT);
    uint64_t sd = 0x30E0E000ULL ^ (uint64_t)J->seed * 0x9E3779B97F4A7C15ULL ^ (J->inject ? 0 : 0xB0A5ULL);
    Rng init, pairs, drift, mut, mig, graft, shr, sample;
    rng_seed(&init, sd); rng_seed(&pairs, sd ^ 1); rng_seed(&drift, sd ^ 2); rng_seed(&mut, sd ^ 3);
    rng_seed(&mig, sd ^ 4); rng_seed(&graft, sd ^ 5); rng_seed(&shr, sd ^ 6); rng_seed(&sample, sd ^ 7);
    for (int q = 0; q < MV_ISLES; q++) for (int i = 0; i < MV_START; i++) {
        Slot *s = &isle[q][i];
        s->alive = 1;
        for (int b = 0; b < LMAX; b++) s->g[b] = (uint8_t)rng_next(&init);
        memcpy(s->law, origin, MV_TAB); memcpy(s->sh, origin, MV_TAB);
    }
    if (J->inject) mv_founder(isle[0][0].g);
    int food[MV_ISLES] = {0, 0, 0, 0};
    int regrow = J->regrow;
    J->t_take = 0; J->inherit = 0; J->grafts = 0; J->births = 0; J->migs = 0; J->real_cum = 0; J->sh_cum = 0;
    const double pd = 0.9 / MV_K, lqd = log(1.0 - pd), pgraft = 0.25, pmig = 0.02;
    for (int e = 1; e <= J->epochs; e++) {
        for (int q = 0; q < MV_ISLES; q++) {
            food[q] += regrow; if (food[q] > MV_FCAP) food[q] = MV_FCAP;
        }
        int ids[MV_ISLES * MV_CAP], nalive = mv_living(isle, ids);
        if (pd > 0 && nalive) {
            for (size_t i = 0;;) {
                double u = rng_double(&drift); if (u > 0.999999) u = 0.999999;
                i += (size_t)(log(1.0 - u) / lqd);
                if (i >= (size_t)nalive) break;
                int id = ids[i], b = (int)rng_below(&drift, MV_TAB), p = (int)rng_below(&drift, P_COUNT);
                isle[id / MV_CAP][id % MV_CAP].law[b] = isle[id / MV_CAP][id % MV_CAP].sh[b] = (uint8_t)p;
                i++;
            }
        }
        if (e % MV_K == 0 && rng_double(&drift) < 0.1) switch (rng_below(&drift, 4)) {
            case 0: skel.rcost = (uint8_t)COSTS[rng_below(&drift, 3)]; break;
            case 1: skel.wcost = (uint8_t)COSTS[rng_below(&drift, 3)]; break;
            case 2: skel.steps = (uint16_t)STEPS[rng_below(&drift, 4)]; break;
            default: skel.mut_i = (uint8_t)rng_below(&drift, 5); break;
        }
        for (int q = 0; q < MV_ISLES; q++) {
            int loc[MV_CAP], n = 0;
            for (int i = 0; i < MV_CAP; i++) if (isle[q][i].alive) loc[n++] = i;
            for (int i = n - 1; i > 0; i--) {
                int j = (int)rng_below(&pairs, (uint32_t)(i + 1)), t = loc[i]; loc[i] = loc[j]; loc[j] = t;
            }
            int can = food[q] / 2; if (can > n / 2) can = n / 2;
            food[q] -= can * 2;
            for (int p = 0; p < can; p++) {
                int ia = loc[2 * p], ib = loc[2 * p + 1];
                Slot *a = &isle[q][ia], *b = &isle[q][ib];
                Univ w; mv_bind(&w, &skel, a->law);
                uint8_t tape[TMAX], olda[LMAX], oldb[LMAX], oldlaw[MV_TAB], oldsh[MV_TAB];
                memcpy(olda, a->g, LMAX); memcpy(oldb, b->g, LMAX);
                memcpy(oldlaw, b->law, MV_TAB); memcpy(oldsh, b->sh, MV_TAB);
                memcpy(tape, olda, LMAX); memcpy(tape + LMAX, oldb, LMAX);
                u_exec(tape, &w, NULL, 0, 0);
                int ch = 0; for (int k = 0; k < LMAX; k++) ch += tape[LMAX + k] != oldb[k];
                if (ch >= 8 && same(tape + LMAX, olda, LMAX) >= fidof(LMAX) && same(oldb, olda, LMAX) < fidof(LMAX)) J->births++;
                memcpy(a->g, tape, LMAX); memcpy(b->g, tape + LMAX, LMAX);
                if (2 * ch >= LMAX) {
                    int mates[MV_CAP], nm = 0;
                    for (int i = 0; i < MV_CAP; i++) if (isle[q][i].alive) mates[nm++] = i;
                    int donor = mates[rng_below(&shr, (uint32_t)nm)];
                    J->grafts += mv_pass(b->law, a->law, oldlaw, &graft, pgraft);
                    J->grafts += mv_pass(b->sh, isle[q][donor].sh, oldsh, &graft, pgraft);
                    J->inherit++;
                }
            }
        }
        double pmut = MUTS[skel.mut_i];
        if (pmut > 0 && nalive) {
            double lq = log(1.0 - pmut); size_t i = 0; size_t nbytes = (size_t)nalive * LMAX;
            for (;;) {
                double u = rng_double(&mut); if (u > 0.999999) u = 0.999999;
                i += (size_t)(log(1.0 - u) / lq);
                if (i >= nbytes) break;
                int id = ids[i / LMAX];
                isle[id / MV_CAP][id % MV_CAP].g[i % LMAX] = (uint8_t)rng_next(&mut);
                i++;
            }
        }
        J->migs += mv_migrate(isle, &mig, pmig);
        if (e % MV_SAMPLE == 0) {
            nalive = mv_living(isle, ids);
            int nr = 0;
            for (int i = 0; i < nalive; i++) {
                int id = ids[i];
                memcpy(laws[nr], isle[id / MV_CAP][id % MV_CAP].law, MV_TAB);
                memcpy(shs[nr], isle[id / MV_CAP][id % MV_CAP].sh, MV_TAB);
                nr++;
            }
            J->real_cum += sp_sample(mr, laws, nr, oh);
            J->sh_cum += sp_sample(ms, shs, nr, oh);
            int ok = 0, ntry = nalive < 16 ? nalive : 16;
            for (int k = 0; k < ntry; k++) {
                int id = ids[rng_below(&sample, (uint32_t)nalive)];
                Slot *s = &isle[id / MV_CAP][id % MV_CAP];
                Univ w; mv_bind(&w, &skel, s->law);
                ok += u_copier(s->g, &w) != 0;
            }
            if (!J->t_take && ntry && ok * 2 >= ntry) J->t_take = e;
            if (J->ncurve < 24) J->curve[J->ncurve++] = ok;
        }
    }
    int ids[MV_ISLES * MV_CAP], nalive = mv_living(isle, ids);
    J->living_end = nalive; J->cop_end = 0;
    for (int q = 0; q < MV_ISLES; q++) { J->isle_cop[q] = 0; J->isle_n[q] = 0; }
    long dr = 0, ds = 0;
    for (int i = 0; i < nalive; i++) {
        int id = ids[i], q = id / MV_CAP;
        Slot *s = &isle[q][id % MV_CAP];
        Univ w; mv_bind(&w, &skel, s->law);
        int cop = u_copier(s->g, &w) != 0;
        J->cop_end += cop; J->isle_n[q]++; J->isle_cop[q] += cop;
        for (int b = 0; b < MV_TAB; b++) { dr += s->law[b] != origin[b]; ds += s->sh[b] != origin[b]; }
    }
    J->isles_cop = 0; for (int q = 0; q < MV_ISLES; q++) J->isles_cop += J->isle_cop[q] > 0;
    J->dist_r = nalive ? (double)dr / nalive : 0; J->dist_s = nalive ? (double)ds / nalive : 0;
    J->real_cum = sp_cum(mr); J->sh_cum = sp_cum(ms);
    uint64_t h = FNV_INIT;
    for (int q = 0; q < MV_ISLES; q++) for (int i = 0; i < MV_CAP; i++) if (isle[q][i].alive) {
        uint8_t tag = (uint8_t)q;
        h = fnv1a(&tag, 1, h);
        h = fnv1a(isle[q][i].g, LMAX, h);
        h = fnv1a(isle[q][i].law, MV_TAB, h);
        h = fnv1a(isle[q][i].sh, MV_TAB, h);
    }
    J->hash = h;
    free(isle); free(mr); free(ms); free(laws); free(shs);
}

static int mv_selftest(void) {
    uint8_t g[LMAX]; mv_founder(g);
    Univ o; u_origin(&o, 8192);
    int cop = u_copier(g, &o);
    uint8_t dst[MV_TAB], src[MV_TAB], old[MV_TAB];
    memset(src, 1, MV_TAB); memset(old, 2, MV_TAB);
    Rng r; rng_seed(&r, 1);
    int grafted = mv_pass(dst, src, old, &r, 1.0);
    int n2 = 0; for (int i = 0; i < MV_TAB; i++) n2 += dst[i] == 2;
    int graft_ok = grafted == 1 && n2 == 1;
    Slot isle[MV_ISLES][MV_CAP];
    memset(isle, 0, sizeof isle);
    isle[0][0].alive = 1; mv_founder(isle[0][0].g);
    rng_seed(&r, 2);
    int moves = mv_migrate(isle, &r, 1.0);
    int mig_ok = moves == 1 && !isle[0][0].alive;
    int found = 0;
    for (int i = 0; i < MV_CAP; i++) if (isle[1][i].alive && memcmp(isle[1][i].g, g, LMAX) == 0) found++;
    mig_ok = mig_ok && found == 1;
    MV a = { .seed = 0, .inject = 1, .epochs = 32, .regrow = MV_REGROW }, b = a;
    mv_run(&a); mv_run(&b);
    int det = a.hash == b.hash && a.inherit == b.inherit && a.births == b.births && a.migs == b.migs;
    MV z = { .seed = 0, .inject = 1, .epochs = 32, .regrow = 0 };
    mv_run(&z);
    int starve = z.births == 0 && z.inherit == 0;
    Univ w = o; uint8_t tape[TMAX], noiseb[TMAX];
    rng_seed(&r, 1);
    for (int i = 0; i < TMAX; i++) noiseb[i] = tape[i] = (uint8_t)rng_next(&r);
    u_exec(tape, &w, NULL, 1, 0);
    int nostore = memcmp(tape, noiseb, TMAX) == 0;
    printf("selftest fondateur copieur : %s\n", cop ? "OK" : "ÉCHEC");
    printf("selftest greffe d'une entrée : %s\n", graft_ok ? "OK" : "ÉCHEC");
    printf("selftest migration : %s\n", mig_ok ? "OK" : "ÉCHEC");
    printf("selftest déterminisme (32 ép.) : %s  hash=%016llx\n", det ? "OK" : "ÉCHEC", (unsigned long long)a.hash);
    printf("selftest sans nourriture : %s  naissances=%ld\n", starve ? "OK" : "ÉCHEC", z.births);
    printf("selftest sans écriture : %s\n", nostore ? "OK" : "ÉCHEC");
    return cop && graft_ok && mig_ok && det && starve && nostore;
}

int main(int argc, char **argv) {
    if (argc > 1 && strcmp(argv[1], "selftest") == 0) return mv_selftest() ? 0 : 1;
    struct timespec t0, t1; clock_gettime(CLOCK_MONOTONIC, &t0);
    printf("seed\tbras\tcop_fin\tvivants\tiles\tt_take\treal_cum\tsh_cum\tdist_r\tdist_s\théritages\tgreffes\tnaissances\tmigs\tcop_iles\tcourbe\n");
    for (int s = 0; s < MV_SEEDS; s++) {
        MV j = { .seed = s, .inject = 1, .epochs = MV_EPOCHS, .regrow = MV_REGROW };
        mv_run(&j);
        printf("%d\tfondateur\t%d\t%d\t%d\t%d\t%d\t%d\t%.2f\t%.2f\t%ld\t%ld\t%ld\t%ld\t%d/%d/%d/%d\t",
               s, j.cop_end, j.living_end, j.isles_cop, j.t_take, j.real_cum, j.sh_cum,
               j.dist_r, j.dist_s, j.inherit, j.grafts, j.births, j.migs,
               j.isle_cop[0], j.isle_cop[1], j.isle_cop[2], j.isle_cop[3]);
        for (int c = 0; c < j.ncurve; c++) printf("%s%d", c ? "," : "", j.curve[c]);
        printf("\n");
        fflush(stdout);
    }
    MV n = { .seed = 0, .inject = 0, .epochs = MV_EPOCHS, .regrow = MV_REGROW };
    mv_run(&n);
    printf("0\tbruit\t%d\t%d\t%d\t%d\t%d\t%d\t%.2f\t%.2f\t%ld\t%ld\t%ld\t%ld\t%d/%d/%d/%d\t",
           n.cop_end, n.living_end, n.isles_cop, n.t_take, n.real_cum, n.sh_cum,
           n.dist_r, n.dist_s, n.inherit, n.grafts, n.births, n.migs,
           n.isle_cop[0], n.isle_cop[1], n.isle_cop[2], n.isle_cop[3]);
    for (int c = 0; c < n.ncurve; c++) printf("%s%d", c ? "," : "", n.curve[c]);
    printf("\n");
    clock_gettime(CLOCK_MONOTONIC, &t1);
    fprintf(stderr, "monde_v0 : %.1f s\n", (t1.tv_sec - t0.tv_sec) + (t1.tv_nsec - t0.tv_nsec) / 1e9);
    return 0;
}
