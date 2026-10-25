/* Ordre 0996a2cd : sélection lexicase hors runner, protocole préenregistré. */
#include "octet.h"
#include <assert.h>
#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define POP 16
#define CAND (POP * 2)
#define CASES 4
#define GENS 128
#define SEEDS 32
#define ARMS 3
#define JOBS (SEEDS * ARMS)

typedef struct {
    int joint, m2, m3;
    double best_min, best_m2, best_m3;
    uint64_t hash;
} Result;
static Result results[JOBS];
static int next_job;
static pthread_mutex_t jobs_mutex = PTHREAD_MUTEX_INITIALIZER;
static int training_calls, held_calls; /* seulement le selftest, pas les workers */

static uint64_t address(uint64_t seed, unsigned gen, unsigned episode) {
    uint64_t key[3] = { seed, gen, episode };
    return fnv1a(key, sizeof key, FNV_INIT ^ UINT64_C(0x0996A2CD));
}
static void shuffle(int *v, int n, Rng *r) {
    for (int i = 0; i < n; i++) v[i] = i;
    for (int i = n - 1; i > 0; i--) {
        int j = (int)rng_below(r, (uint32_t)(i + 1));
        int t = v[i]; v[i] = v[j]; v[j] = t;
    }
}
static int lex_pick(const double score[CAND][CASES], int n, Rng *r) {
    int alive[CAND], order[CASES], left = n;
    for (int i = 0; i < n; i++) alive[i] = i;
    shuffle(order, CASES, r);
    for (int k = 0; k < CASES && left > 1; k++) {
        double best = -1;
        for (int i = 0; i < left; i++)
            if (score[alive[i]][order[k]] > best) best = score[alive[i]][order[k]];
        int kept = 0;
        for (int i = 0; i < left; i++)
            if (score[alive[i]][order[k]] >= best - 1.0 / 32.0)
                alive[kept++] = alive[i];
        assert(kept > 0); left = kept;
    }
    return alive[rng_below(r, (uint32_t)left)];
}
static void mean_order(const double score[CAND][CASES], int n, Rng *r, int *order) {
    double mean[CAND];
    for (int i = 0; i < n; i++) {
        mean[i] = 0;
        for (int k = 0; k < CASES; k++) mean[i] += score[i][k];
    }
    shuffle(order, n, r);
    /* Tri stable : l'ordre aléatoire initial départage les égalités. */
    for (int i = 1; i < n; i++) {
        int v = order[i], j = i;
        while (j > 0 && mean[order[j - 1]] < mean[v]) {
            order[j] = order[j - 1]; j--;
        }
        order[j] = v;
    }
}
static int is_joint(const double v[GN_CHECK_N]) {
    for (int k = 0; k < 2; k++)
        if (!isfinite(v[k]) || !isfinite(v[2 + k]) || !isfinite(v[4 + k]) ||
            v[k] < .95 || v[2 + k] > v[k] - .5 || v[4 + k] > v[k] - .5) return 0;
    return 1;
}
static void eval_cases(const PoolEntry *p, int n, double scores[CAND][CASES],
                       uint64_t seed, int gen, int count_calls) {
    for (int i = 0; i < n; i++) for (int k = 0; k < CASES; k++) {
        scores[i][k] = gn_eval(&p[i], 1, 0, 0, address(seed, (unsigned)gen, (unsigned)k),
                               1, 0, 6 + k / 2, NULL);
        if (count_calls) training_calls++;
    }
}
static void evolve(int arm, unsigned seed, int gens, PoolEntry *pool, int count_calls) {
    Rng mutation, selection;
    rng_seed(&mutation, UINT64_C(0xE0996A2CD) + seed);
    rng_seed(&selection, UINT64_C(0x50996A2CD) + seed);
    for (int i = 0; i < POP; i++) seed_entry(&pool[i], "general", 0);
    PoolEntry *cand = xcalloc(CAND, sizeof *cand), *next = xcalloc(POP, sizeof *next);
    for (int gen = 0; gen < gens; gen++) {
        memcpy(cand, pool, POP * sizeof *cand);
        for (int i = 0; i < POP; i++) {
            int parent = (int)rng_below(&mutation, POP);
            memset(&cand[POP + i], 0, sizeof *cand);
            cand[POP + i].len = mutate(pool[parent].dna, pool[parent].len, &mutation,
                                       .005, .01, cand[POP + i].dna);
        }
        double scores[CAND][CASES];
        eval_cases(cand, CAND, scores, seed, gen, count_calls);
        int order[CAND];
        if (arm == 1) mean_order(scores, CAND, &selection, order);
        for (int i = 0; i < POP; i++) {
            int take = arm == 0 ? lex_pick(scores, CAND, &selection)
                       : arm == 1 ? order[i] : (int)rng_below(&selection, CAND);
            next[i] = cand[take];
        }
        memcpy(pool, next, POP * sizeof *pool);
    }
    free(cand); free(next);
}
static uint64_t pool_hash(const PoolEntry *pool) {
    uint64_t h = FNV_INIT;
    for (int i = 0; i < POP; i++) {
        h = fnv1a(&pool[i].len, sizeof pool[i].len, h);
        h = fnv1a(pool[i].dna, (size_t)pool[i].len, h);
    }
    return h;
}
static Result measure(const PoolEntry *pool, int n, int count_calls) {
    Result out = {0};
    for (int i = 0; i < n; i++) {
        /* held reste hors evolve(). Ne jamais sélectionner sur ces résultats. */
        double v[GN_CHECK_N]; gn_check(&pool[i], v);
        if (count_calls) held_calls++;
        out.joint += is_joint(v);
        for (int k = 0; k < 2; k++) {
            int pass = v[k] >= .95 && v[2+k] <= v[k]-.5 && v[4+k] <= v[k]-.5;
            if (k == 0) out.m2 += pass; else out.m3 += pass;
        }
        double joint_min = fmin(v[0], v[1]);
        if (joint_min > out.best_min) out.best_min = joint_min;
        if (v[0] > out.best_m2) out.best_m2 = v[0];
        if (v[1] > out.best_m3) out.best_m3 = v[1];
    }
    return out;
}
static void *worker(void *unused) {
    (void)unused;
    PoolEntry *pool = xcalloc(POP, sizeof *pool);
    for (;;) {
        pthread_mutex_lock(&jobs_mutex);
        int job = next_job++;
        pthread_mutex_unlock(&jobs_mutex);
        if (job >= JOBS) break;
        evolve(job / SEEDS, (unsigned)(job % SEEDS), GENS, pool, 0);
        results[job] = measure(pool, POP, 0);
        results[job].hash = pool_hash(pool);
    }
    free(pool); return NULL;
}
static void selftest(void) {
    double scores[CAND][CASES] = {{0}};
    /* Le spécialiste de chaque moitié est exclu par le généraliste à la moyenne,
       et retenu par lexicase lorsque sa propre moitié arrive en premier. */
    scores[0][0] = scores[0][1] = 1;
    scores[1][2] = scores[1][3] = 1;
    for (int k = 0; k < CASES; k++) scores[2][k] = .6;
    Rng r; rng_seed(&r, 1); int seen[3] = {0}, order[CAND];
    mean_order(scores, 3, &r, order); assert(order[0] == 2);
    for (int i = 0; i < 256; i++) seen[lex_pick(scores, 3, &r)]++;
    assert(seen[0] > 0 && seen[1] > 0 && seen[2] == 0);
    double v[GN_CHECK_N] = {.96,.96,.3,.3,.3,.3,0,0};
    assert(is_joint(v)); v[1] = .6; assert(!is_joint(v));
    v[1] = .96; v[5] = .9; assert(!is_joint(v));
    v[5] = .3; v[0] = NAN; assert(!is_joint(v));
    PoolEntry *a = xcalloc(POP, sizeof *a), *b = xcalloc(POP, sizeof *b);
    for (int arm = 0; arm < ARMS; arm++) {
        training_calls = held_calls = 0;
        evolve(arm, 7, 2, a, 1);
        assert(training_calls == 2 * CAND * CASES && held_calls == 0);
        evolve(arm, 7, 2, b, 0); assert(pool_hash(a) == pool_hash(b));
    }
    free(a); free(b);
    puts("selftest selection-episodes : OK (lexicase, moyenne, joint, séparation held, déterminisme)");
}
static int run(void) {
    static const char *names[ARMS] = {"lexicase", "moyenne", "neutre"};
    printf("selection-episodes prereg=3402120 graines=32 generations=%d population=%d enfants=16 episodes=4\n", GENS, POP);
    pthread_t ts[4];
    for (int i = 0; i < 4; i++) if (pthread_create(&ts[i], NULL, worker, NULL)) return 2;
    for (int i = 0; i < 4; i++) if (pthread_join(ts[i], NULL)) return 2;
    int seeds_pass[ARMS] = {0};
    for (int arm = 0; arm < ARMS; arm++) {
        int m2 = 0, m3 = 0, joint = 0; double bm = 0, b2 = 0, b3 = 0;
        for (int seed = 0; seed < SEEDS; seed++) {
            Result *r = &results[arm * SEEDS + seed];
            printf("%s graine=%d joints=%d m2=%d m3=%d meilleur_min=%.6f max_m2=%.6f max_m3=%.6f hash=%016llx\n",
                   names[arm],seed,r->joint,r->m2,r->m3,r->best_min,r->best_m2,r->best_m3,(unsigned long long)r->hash);
            seeds_pass[arm] += r->joint > 0; joint += r->joint; m2 += r->m2; m3 += r->m3;
            if (r->best_min > bm) bm = r->best_min;
            if (r->best_m2 > b2) b2 = r->best_m2;
            if (r->best_m3 > b3) b3 = r->best_m3;
        }
        printf("compte %s graines_jointes=%d/32 genomes_joints=%d/512 m2=%d m3=%d meilleur_min=%.6f max_m2=%.6f max_m3=%.6f\n",
               names[arm],seeds_pass[arm],joint,m2,m3,bm,b2,b3);
    }
    int control_joint[2] = {0};
    for (int arm = 0; arm < 2; arm++) for (int seed = 0; seed < SEEDS; seed++) {
        PoolEntry g = {0}; g.len = 172;
        Rng r; rng_seed(&r, UINT64_C(0xB0996A2CD) + (unsigned)seed);
        for (int k = 0; k < g.len; k++) g.dna[k] = arm == 0 ? (uint8_t)rng_below(&r,256) : (uint8_t)seed;
        Result m = measure(&g, 1, 0); control_joint[arm] += m.joint;
        printf("controle %s graine=%d joint=%d min=%.6f\n",arm==0?"bruit":"cristal",seed,m.joint,m.best_min);
    }
    int valid = !control_joint[0] && !control_joint[1];
    int yes = valid && seeds_pass[0] >= 8 && seeds_pass[0] >= seeds_pass[1]+6 && seeds_pass[2] <= 1;
    printf("temoins bruit=%d/32 cristal=%d/32 dispositif=%s\n",control_joint[0],control_joint[1],valid?"valide":"INVALIDE");
    printf("verdict=%s L=%d/32 M=%d/32 N=%d/32 seuils=L>=8,L-M>=6,N<=1\n",valid?(yes?"OUI":"NON"):"INVALIDE",seeds_pass[0],seeds_pass[1],seeds_pass[2]);
    return valid ? 0 : 2;
}
int main(int argc, char **argv) {
    setvbuf(stdout,NULL,_IOLBF,0);
    if (argc == 2 && !strcmp(argv[1],"selftest")) { selftest(); return 0; }
    if (argc == 2 && !strcmp(argv[1],"run")) return run();
    fprintf(stderr,"usage: %s selftest|run\n",argv[0]); return 2;
}
