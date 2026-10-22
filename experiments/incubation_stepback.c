/* Une seule condition : durée de contact des partenaires, sans exécution offerte.
 * Moteur et dérive existants, sans modifier src/, include/ ou les seeds. */
#define US2_NO_MAIN
#include "universe_search2.c"

enum { IN = 1024, NSEED = 8, NSOURCE = 2, NARM = 3 };
static const int CONTACT[NARM] = { 1, 8, 8 };
static const char *ARM[NARM] = { "brassage", "contact8", "sans-ecritures" };
/* Octets archivés, aucune reconstruction ni optimisation du fondateur. */
static const char *FOUNDER[NSOURCE] = {
    "051d03021c1fff0701030903c906901e051d03021c1fff0701030903c906901e"
    "051d03021c1fff0701030903c906901e051d03021c1fff0701030903c906901e",
    "0412d288eae460074000c40600f33d106de8cd136d344c4d3f003ffd1ec658ba00a4"
    "049e9e0286027a025147a102eaf77a021eb923f1afcc34f98a0ba9d92200"
};
static void founder(int src, uint8_t *p) {
    for (int i = 0; i < LMAX; i++) { unsigned x; sscanf(FOUNDER[src] + 2 * i, "%2x", &x); p[i] = (uint8_t)x; }
}
typedef struct {
    int source, seed, arm, horizon, frozen;
    int life, censored, generation, peak, final_live;
    long births;
    uint64_t hash, phys_hash;
} IncJob;

/* Observation seule : reprendre le critère de naissance de soup_run, puis
 * transporter la généalogie. Aucun test de naissance n'agit sur les octets. */
static void lineage(const uint8_t *old, const uint8_t *t, const uint8_t *wr,
                    const int *before, int *after, long *births) {
    for (int h = 0; h < 2; h++) {
        int nw = 0;
        for (int i = 0; i < LMAX; i++) nw += wr[h * LMAX + i] && t[h * LMAX + i] != old[h * LMAX + i];
        if (nw >= 8 && same(t + h * LMAX, old + (1 - h) * LMAX, LMAX) >= fidof(LMAX)
                    && same(old + h * LMAX, old + (1 - h) * LMAX, LMAX) < fidof(LMAX)) {
            after[h] = before[1 - h] < 0 ? -1 : before[1 - h] + 1;
            *births += before[1 - h] >= 0;
        }
    }
}

static void inc_run(IncJob *J) {
    uint8_t *s = malloc(IN * LMAX);
    int *gen = malloc(IN * sizeof *gen), perm[IN], candidate[IN];
    if (!s || !gen) { fprintf(stderr, "allocation impossible\n"); exit(1); }
    Univ u; u_origin(&u, 8192);
    Rng init, pairs, roles, mutations, drift;
    uint64_t sd = mix(0x1AC0BA7E, (uint64_t)(J->source * NSEED + J->seed));
    rng_seed(&init, sd); rng_seed(&pairs, sd ^ 1); rng_seed(&roles, sd ^ 2);
    rng_seed(&mutations, sd ^ 3); rng_seed(&drift, sd ^ 4);
    for (int i = 0; i < IN * LMAX; i++) s[i] = (uint8_t)rng_next(&init);
    for (int i = 0; i < IN; i++) gen[i] = -1;
    int pos = (int)rng_below(&init, IN); founder(J->source, s + pos * LMAX); gen[pos] = 0;
    J->life = 0; J->births = 0; J->censored = 0; J->generation = 0; J->peak = 1;
    for (int e = 1; e <= J->horizon; e++) {
        if (!J->frozen && e % 8 == 0) { char what[64]; drift_step(&u, &drift, what, sizeof what); }
        for (int i = 0; i < IN; i++) candidate[i] = i;
        for (int i = IN - 1; i > 0; i--) {
            int j = (int)rng_below(&pairs, (uint32_t)(i + 1)), t = candidate[i];
            candidate[i] = candidate[j]; candidate[j] = t;
        }
        if ((e - 1) % CONTACT[J->arm] == 0) memcpy(perm, candidate, sizeof perm);
        for (int i = 0; i < IN; i += 2) {
            int flip = (int)rng_below(&roles, 2), a = perm[i + flip], b = perm[i + 1 - flip];
            uint8_t old[TMAX], tape[TMAX], wr[TMAX] = { 0 };
            memcpy(old, s + a * LMAX, LMAX); memcpy(old + LMAX, s + b * LMAX, LMAX);
            memcpy(tape, old, sizeof tape);
            u_exec(tape, &u, wr, J->arm == 2, 0);
            int before[2] = { gen[a], gen[b] }, after[2] = { gen[a], gen[b] };
            lineage(old, tape, wr, before, after, &J->births);
            gen[a] = after[0]; gen[b] = after[1];
            memcpy(s + a * LMAX, tape, LMAX); memcpy(s + b * LMAX, tape + LMAX, LMAX);
        }
        double pmut = MUTS[u.mut_i];
        if (pmut > 0) {
            double lq = log(1.0 - pmut); size_t i = 0;
            for (;;) {
                i += (size_t)(log(1.0 - rng_double(&mutations)) / lq);
                if (i >= IN * LMAX) break;
                s[i++] = (uint8_t)rng_next(&mutations);
            }
        }
        int live = 0;
        for (int i = 0; i < IN; i++) if (gen[i] >= 0 && u_copier(s + i * LMAX, &u)) {
            live++;
            if (gen[i] > J->generation) J->generation = gen[i];
        }
        J->final_live = live;
        if (live > J->peak) J->peak = live;
        if (!live) break; /* arrêt à la PREMIÈRE extinction : aucune renaissance ne compte */
        J->life = e;
        if (e == J->horizon) J->censored = 1;
    }
    J->hash = fnv1a(s, IN * LMAX, FNV_INIT); J->phys_hash = u_hash(&u);
    free(s); free(gen);
}

static int inc_selftest(void) {
    int fail = 0; Univ u; u_origin(&u, 8192);
    for (int src = 0; src < NSOURCE; src++) {
        uint8_t p[LMAX]; founder(src, p);
        SoupCfg cfg = { .addr_b = 1, .steps = 8192 };
        int ok = strlen(FOUNDER[src]) == 2 * LMAX && soup_copier(p, &cfg) && u_copier(p, &u);
        printf("incubation selftest : fondateur archivé %d copieur sous soupe ET univers %s\n", src, ok ? "OK" : "ÉCHEC"); fail |= !ok;
    }
    /* La généalogie doit reconnaître une copie, sans déplacer un seul octet. */
    uint8_t old[TMAX], tape[TMAX], wr[TMAX] = { 0 }; soup_handmade(old, 1); noise(old + LMAX, LMAX, 19);
    memcpy(tape, old, TMAX); u_exec(tape, &u, wr, 0, 0);
    int before[2] = { 0, -1 }, after[2] = { 0, -1 }; long births = 0;
    uint64_t h = fnv1a(tape, TMAX, FNV_INIT);
    lineage(old, tape, wr, before, after, &births);
    int ok = after[1] == 1 && births == 1 && h == fnv1a(tape, TMAX, FNV_INIT);
    printf("incubation selftest : naissance causale et observation inerte %s\n", ok ? "OK" : "ÉCHEC"); fail |= !ok;
    IncJob a = { .source = 0, .seed = 3, .arm = 1, .horizon = 32, .frozen = 1 }, b = a;
    inc_run(&a); inc_run(&b);
    ok = a.hash == b.hash && a.life == b.life && a.births == b.births;
    printf("incubation selftest : déterminisme %s\n", ok ? "OK" : "ÉCHEC"); fail |= !ok;
    b.arm = 2; inc_run(&b); ok = b.births == 0 && b.generation == 0;
    printf("incubation selftest : sans écritures, zéro descendance %s\n", ok ? "OK" : "ÉCHEC"); fail |= !ok;
    return fail;
}

typedef struct { IncJob *jobs; int next, n; pthread_mutex_t lock; } IncQueue;
static void *inc_worker(void *arg) {
    IncQueue *q = arg;
    for (;;) {
        pthread_mutex_lock(&q->lock); int i = q->next++; pthread_mutex_unlock(&q->lock);
        if (i >= q->n) return NULL;
        inc_run(&q->jobs[i]);
    }
}
int main(int argc, char **argv) {
    (void)selftest2; (void)cmd_race; (void)cmd_origin; (void)cmd_drift;
    if (argc == 2 && !strcmp(argv[1], "selftest")) return inc_selftest();
    int horizon = 4096;
    if (argc == 4 && !strcmp(argv[2], "--epochs")) horizon = atoi(argv[3]);
    else if (argc != 2) horizon = 0;
    if (horizon < 8 || argc < 2 || strcmp(argv[1], "run")) {
        fprintf(stderr, "usage : incubation_stepback selftest | run [--epochs 4096]\n"); return 2;
    }
    IncJob jobs[NSOURCE * NSEED * NARM] = { 0 };
    for (int s = 0; s < NSOURCE; s++) for (int g = 0; g < NSEED; g++) for (int a = 0; a < NARM; a++) {
        IncJob *j = &jobs[(s * NSEED + g) * NARM + a];
        j->source = s; j->seed = g; j->arm = a; j->horizon = horizon;
    }
    struct timespec t0; clock_gettime(CLOCK_MONOTONIC, &t0);
    IncQueue q = { .jobs = jobs, .n = NSOURCE * NSEED * NARM, .lock = PTHREAD_MUTEX_INITIALIZER };
    pthread_t threads[8];
    for (int t = 0; t < 8; t++) pthread_create(&threads[t], NULL, inc_worker, &q);
    for (int t = 0; t < 8; t++) pthread_join(threads[t], NULL);
    pthread_mutex_destroy(&q.lock);
    printf("# N=%d une copie initiale, horizon=%d K=8 ; arrêt première extinction, test exhaustif chaque époque\n", IN, horizon);
    printf("# source seed bras vie(censurée à horizon) censuré naissances_desc génération_max pic_copieurs fin_copieurs hash physique\n");
    double sum[NARM] = { 0 }; int cens[NARM] = { 0 }, wins = 0; long neg = 0;
    for (int i = 0; i < q.n; i++) {
        IncJob *j = &jobs[i]; sum[j->arm] += j->life; cens[j->arm] += j->censored;
        if (j->arm == 2) neg += j->births;
        if (j->arm == 1) wins += j->life > jobs[i - 1].life;
        printf("%d %d %s %d %d %ld %d %d %d %016llx %016llx\n", j->source, j->seed, ARM[j->arm], j->life,
               j->censored, j->births, j->generation, j->peak, j->final_live,
               (unsigned long long)j->hash, (unsigned long long)j->phys_hash);
    }
    double ratio = sum[0] ? sum[1] / sum[0] : 0;
    const char *verdict = neg ? "invalide" : (ratio >= 2 && wins >= 13) ? "oui" :
                          (cens[0] && cens[1]) ? "non concluant (censure)" : "non";
    printf("# moyenne brassage=%.3f contact8=%.3f ratio=%.6f gains=%d/16 censures=%d/%d négatif=%ld verdict=%s temps=%.2fs\n",
           sum[0] / 16, sum[1] / 16, ratio, wins, cens[0], cens[1], neg, verdict, secs(&t0));
    return neg ? 1 : 0;
}
