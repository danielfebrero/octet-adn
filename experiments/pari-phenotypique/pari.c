/* Pari phénotypique, mission 0996a2cd. Modèle indépendant, libc + libm. */
#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define POP 48
#define ELITE 4
#define GENERATIONS 128
#define TRAIN_REPS 4
#define TRAIN_STEPS 256
#define VALID_REPS 16
#define VALID_STEPS 4096
#define SEEDS 32

typedef struct { uint64_t state; } Rng;
typedef struct { uint8_t g[2]; } Genome;
typedef struct { double selected, noise, crystal, synchronous; int pass; } Result;

static uint64_t next64(Rng *r) {
    uint64_t z = (r->state += UINT64_C(0x9e3779b97f4a7c15));
    z = (z ^ (z >> 30)) * UINT64_C(0xbf58476d1ce4e5b9);
    z = (z ^ (z >> 27)) * UINT64_C(0x94d049bb133111eb);
    return z ^ (z >> 31);
}
static unsigned below(Rng *r, unsigned n) {
    uint64_t x, cutoff = (uint64_t)(-((uint64_t)n)) % n;
    do { x = next64(r); } while (x < cutoff);
    return (unsigned)(x % n);
}
static uint64_t key(unsigned seed, unsigned phase, unsigned generation) {
    Rng r = { UINT64_C(0xa83d6a4dd0996a2c) ^ ((uint64_t)seed << 32)
        ^ ((uint64_t)phase << 16) ^ generation };
    return next64(&r);
}
static void environment(uint8_t *env, size_t n, Rng *r) {
    unsigned state = below(r, 2);
    for (size_t t = 0; t < n; ++t) {
        if (t && below(r, 32) == 0) state ^= 1;
        env[t] = (uint8_t)state;
    }
}
/* Fraction après le changement stochastique individuel, avant croissance. */
static double transition(double x0, Genome g) {
    return x0 * (1.0 - g.g[0] / 256.0) + (1.0 - x0) * g.g[1] / 256.0;
}
static double growth(Genome g, const uint8_t *env, size_t n, int synchronous) {
    double x0 = 0.5, total = 0.0;
    for (size_t t = 0; t < n; ++t) {
        double y0 = transition(x0, g);
        double f0 = env[t] == 0 ? 1.5 : 0.05;
        double f1 = env[t] == 1 ? 1.5 : 0.05;
        if (synchronous) {
            /* Espérance du log : tout le clone porte un seul état. */
            total += y0 * log(f0) + (1.0 - y0) * log(f1);
            x0 = y0;
        } else {
            double factor = y0 * f0 + (1.0 - y0) * f1;
            total += log(factor);
            x0 = y0 * f0 / factor;
        }
    }
    return total / (double)n;
}
static double constant_growth(unsigned state, const uint8_t *env, size_t n) {
    double total = 0;
    for (size_t t = 0; t < n; ++t)
        total += log(env[t] == state ? 1.5 : 0.05);
    return total / (double)n;
}
static double training_score(Genome g, const uint8_t *env) {
    double sum = 0;
    for (int j = 0; j < TRAIN_REPS; ++j)
        sum += growth(g, env + j * TRAIN_STEPS, TRAIN_STEPS, 0);
    return sum / TRAIN_REPS;
}
static Genome mutated(Genome g, Rng *r) {
    unsigned locus = below(r, 2), op = below(r, 5);
    unsigned replacement = below(r, 256); /* tirage commun, même si inutilisé */
    static const int delta[4] = {-8, -1, 1, 8};
    int value = op == 4 ? (int)replacement : g.g[locus] + delta[op];
    if (value < 0) value = 0;
    if (value > 255) value = 255;
    g.g[locus] = (uint8_t)value;
    return g;
}
static void breed(const Genome *pop, Genome *child, const double *score,
                  Rng *r, int selected) {
    unsigned rank[POP];
    for (unsigned i = 0; i < POP; ++i) rank[i] = i;
    /* Tri stable : score égal => ordre d'origine, pas d'aléa caché. */
    for (unsigned i = 1; i < POP; ++i) {
        unsigned v = rank[i], j = i;
        while (j && score[v] > score[rank[j - 1]]) { rank[j] = rank[j-1]; --j; }
        rank[j] = v;
    }
    for (unsigned i = 0; i < POP; ++i) {
        unsigned a = below(r, POP), b = below(r, POP), c = below(r, POP);
        unsigned parent = a;
        if (selected) {
            if (i < ELITE) parent = rank[i];
            else {
                if (score[b] > score[parent]) parent = b;
                if (score[c] > score[parent]) parent = c;
            }
        }
        child[i] = pop[parent];
        if (i >= ELITE) child[i] = mutated(child[i], r);
    }
}
static void evolve(unsigned seed, unsigned generations, Genome *selected, Genome *noise) {
    Genome pop[2][POP], child[2][POP];
    double score[2][POP];
    uint8_t env[TRAIN_REPS][TRAIN_STEPS];
    Rng init = {key(seed, 1, 0)};
    Rng mutation[2] = {{key(seed, 2, 0)}, {key(seed, 2, 0)}};
    for (unsigned i = 0; i < POP; ++i) {
        pop[0][i].g[0] = (uint8_t)below(&init, 256);
        pop[0][i].g[1] = (uint8_t)below(&init, 256);
        pop[1][i] = pop[0][i];
    }
    for (unsigned generation = 0; generation < generations; ++generation) {
        Rng er = {key(seed, 3, generation)};
        for (unsigned j = 0; j < TRAIN_REPS; ++j)
            environment(env[j], TRAIN_STEPS, &er);
        for (unsigned arm = 0; arm < 2; ++arm) {
            for (unsigned i = 0; i < POP; ++i)
                score[arm][i] = training_score(pop[arm][i], &env[0][0]);
            breed(pop[arm], child[arm], score[arm], &mutation[arm], arm == 0);
            memcpy(pop[arm], child[arm], sizeof pop[arm]);
        }
        /* Opérateurs et budget aléatoire identiques, sauf choix parental. */
        assert(mutation[0].state == mutation[1].state);
    }
    *selected = pop[0][0];
    *noise = pop[1][0];
}
static Result validate(unsigned seed, Genome selected, Genome noise) {
    Result out = {0};
    double c0 = 0, c1 = 0;
    uint8_t env[VALID_STEPS];
    for (unsigned j = 0; j < VALID_REPS; ++j) {
        Rng er = {key(seed, 4, j)};
        environment(env, VALID_STEPS, &er);
        out.selected += growth(selected, env, VALID_STEPS, 0) / VALID_REPS;
        out.noise += growth(noise, env, VALID_STEPS, 0) / VALID_REPS;
        out.synchronous += growth(selected, env, VALID_STEPS, 1) / VALID_REPS;
        c0 += constant_growth(0, env, VALID_STEPS) / VALID_REPS;
        c1 += constant_growth(1, env, VALID_STEPS) / VALID_REPS;
    }
    out.crystal = c0 > c1 ? c0 : c1;
    out.pass = selected.g[0] > 0 && selected.g[0] < 255
        && selected.g[1] > 0 && selected.g[1] < 255
        && out.selected - out.noise >= 0.10
        && out.selected - out.crystal >= 0.10
        && out.selected - out.synchronous >= 0.10;
    return out;
}
static void close_to(double a, double b) { assert(fabs(a-b) < 1e-11); }
static void selftest(void) {
    Genome zero = {{0, 0}}, all = {{128, 128}}, turn = {{255, 255}};
    uint8_t env[] = {0, 1};
    close_to(transition(1, zero), 1);
    close_to(transition(0, zero), 0);
    for (unsigned k = 0; k <= 100; ++k) {
        double x = transition(k / 100.0, turn);
        assert(x >= 0 && x <= 1);
        close_to(x + (1-x), 1);
    }
    close_to(growth(all, env, 1, 0), log(0.775));
    close_to(growth(all, env, 2, 0), log(0.775));
    close_to(growth(zero, env, 2, 1), (log(1.5)+log(0.05))/2);
    close_to(constant_growth(0, env, 2), constant_growth(1, env, 2));
    assert(growth(all, env, 2, 0) > growth(all, env, 2, 1));
    uint8_t e0[1024], e1[1024];
    Rng r0 = {key(9, 3, 0)}, r1 = {key(9, 3, 0)};
    environment(e0, sizeof e0, &r0); environment(e1, sizeof e1, &r1);
    assert(!memcmp(e0, e1, sizeof e0));
    assert(key(0, 3, 1) != key(0, 4, 1));
    Genome s0, n0, s1, n1;
    evolve(17, 3, &s0, &n0); evolve(17, 3, &s1, &n1);
    assert(!memcmp(&s0, &s1, sizeof s0) && !memcmp(&n0, &n1, sizeof n0));
    Result a = validate(17, s0, n0), b = validate(17, s0, n0);
    close_to(a.selected, b.selected); close_to(a.noise, b.noise);
    close_to(a.crystal, b.crystal); close_to(a.synchronous, b.synchronous);
    puts("pari-phenotypique selftest: OK (conservation, calcul, témoins, déterminisme)");
}
static int run(void) {
    int pass = 0;
    double sums[4] = {0};
    puts("seed a b bruit_a bruit_b selection bruit cristal synchrone delta_bruit passe");
    for (unsigned seed = 0; seed < SEEDS; ++seed) {
        Genome s, n; evolve(seed, GENERATIONS, &s, &n);
        Result r = validate(seed, s, n);
        printf("%u %u %u %u %u %.9f %.9f %.9f %.9f %.9f %d\n", seed,
               s.g[0], s.g[1], n.g[0], n.g[1], r.selected, r.noise,
               r.crystal, r.synchronous, r.selected-r.noise, r.pass);
        fflush(stdout);
        pass += r.pass;
        sums[0] += r.selected; sums[1] += r.noise;
        sums[2] += r.crystal; sums[3] += r.synchronous;
    }
    printf("MOYENNES selection=%.9f bruit=%.9f cristal=%.9f synchrone=%.9f\n",
        sums[0]/SEEDS, sums[1]/SEEDS, sums[2]/SEEDS, sums[3]/SEEDS);
    printf("VERDICT %s %d/%d seuil=24/%d\n", pass >= 24 ? "OUI" : "NON", pass, SEEDS, SEEDS);
    return 0; /* Un non expérimental est une exécution réussie. */
}
int main(int argc, char **argv) {
    if (argc != 2) { fprintf(stderr, "usage: %s selftest|run\n", argv[0]); return 2; }
    if (!strcmp(argv[1], "selftest")) { selftest(); return 0; }
    if (!strcmp(argv[1], "run")) return run();
    fprintf(stderr, "commande inconnue: %s\n", argv[1]); return 2;
}
