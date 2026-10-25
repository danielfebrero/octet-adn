/* Ordre 0996a2cd : sélection d'un métabolisme à deux catalyseurs.
 * VM historique et mondes strictement locaux ; aucun format de checkpoint. */
#include "octet.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NPAIR 64
#define GLEN 32
#define NGEN 512
#define NTRAIN 16
#define NTEST 128
#define NSEED 32
#define DENOM (NPAIR * NTEST)

typedef struct { uint8_t dna[GLEN]; int8_t response[NTRAIN]; int valid; } Gene;
typedef struct { Gene member[2]; } Pair;
typedef struct { Pair pair[NPAIR]; } Population;
typedef struct { int products, alone, ablated, train, emitting, unique; uint64_t hash; } Result;
enum { PACKET, SHUFFLE, NOISE, CRYSTAL, NARM };
static const char *names[NARM] = { "paquet", "brassage", "bruit", "cristal" };

/* Un seul sandbox réutilisé, toujours remis à vide. Aucune adresse de monde
 * extérieur n'entre ici. SPLIT est refusé par ncs=max_pop=1. */
static int response(World *sandbox, const uint8_t dna[GLEN], unsigned stimulus) {
    Creature c;
    memset(&c, 0, sizeof c);
    c.x = c.y = 1; c.e = 128; c.len = GLEN; c.emitted = -1;
    memcpy(c.dna, dna, GLEN);
    for (int y = 0; y < 4; y++) {
        memset(sandbox->occ[y], 0, 4 * sizeof sandbox->occ[y][0]);
        memset(sandbox->food[y], 0, 4);
    }
    sandbox->occ[1][1] = 1;
    sandbox->ncs = 1; sandbox->cs[0] = &c;
    sandbox->nnb = sandbox->nem = 0;
    sandbox->st = (Stats){0}; sandbox->stimulus = (int)stimulus;
    int alive = vm_run(&c, sandbox);
    assert(sandbox->nnb == 0);
    return alive && c.emitted >= 0 ? c.emitted & 3 : -1;
}

static World *new_sandbox(Rng *rng) {
    World *w = calloc(1, sizeof *w);
    if (!w) { perror("calloc sandbox"); exit(1); }
    w->cfg = cfg_default(); w->cfg.w = w->cfg.h = 4;
    w->cfg.food_density = 0; w->cfg.food_regrow = 0;
    w->cfg.max_pop = 1; w->cfg.split_energy = INT_MAX;
    w->rng = rng;
    return w;
}

static int product(int a, int b, unsigned s) {
    int x = (int)(s & 3), y = (x + 1) & 3;
    return (a == x && b == y) || (a == y && b == x);
}

static void cache(World *w, Gene *g) {
    if (g->valid) return;
    for (unsigned s = 0; s < NTRAIN; s++)
        g->response[s] = (int8_t)response(w, g->dna, s);
    g->valid = 1;
}

static int training(World *w, Pair *p) {
    for (int j = 0; j < 2; j++) cache(w, &p->member[j]);
    int score = 0;
    for (unsigned s = 0; s < NTRAIN; s++)
        score += product(p->member[0].response[s], p->member[1].response[s], s);
    return score;
}

static void mutate_gene(Gene *g, Rng *r) {
    if (rng_below(r, 2)) {
        unsigned at = rng_below(r, GLEN);
        uint8_t v = (uint8_t)rng_below(r, 256);
        if (g->dna[at] != v) { g->dna[at] = v; g->valid = 0; }
    }
}

static void reshuffle(Population *p, Rng *r) {
    Gene flat[2 * NPAIR];
    for (int i = 0; i < NPAIR; i++)
        for (int j = 0; j < 2; j++) flat[2 * i + j] = p->pair[i].member[j];
    for (unsigned i = 2 * NPAIR - 1; i > 0; i--) {
        unsigned j = rng_below(r, i + 1);
        Gene g = flat[i]; flat[i] = flat[j]; flat[j] = g;
    }
    for (int i = 0; i < NPAIR; i++)
        for (int j = 0; j < 2; j++) p->pair[i].member[j] = flat[2 * i + j];
}

static void evolve(World *w, Population *pop, int arm, unsigned seed) {
    if (arm == CRYSTAL) return;
    /* Streams séparés : le brassage ne consomme aucun tirage de mutation. */
    Rng selection, mutation, grouping;
    rng_seed(&selection, 0x73656c656374ULL + seed);
    rng_seed(&mutation, 0x6d7574617465ULL + seed);
    rng_seed(&grouping, 0x67726f7570ULL + seed);
    for (int generation = 0; generation < NGEN; generation++) {
        unsigned cumulative[NPAIR], total = 0;
        for (int i = 0; i < NPAIR; i++) {
            total += 1 + (arm == NOISE ? 0 : (unsigned)training(w, &pop->pair[i]));
            cumulative[i] = total;
        }
        Population child;
        for (int i = 0; i < NPAIR; i++) {
            unsigned ticket = rng_below(&selection, total), parent = 0;
            while (ticket >= cumulative[parent]) parent++;
            child.pair[i] = pop->pair[parent];
            if (rng_below(&grouping, 2)) {
                Gene g = child.pair[i].member[0];
                child.pair[i].member[0] = child.pair[i].member[1];
                child.pair[i].member[1] = g;
            }
            for (int j = 0; j < 2; j++) mutate_gene(&child.pair[i].member[j], &mutation);
        }
        if (arm == SHUFFLE) reshuffle(&child, &grouping);
        *pop = child;
    }
}

static void initialize(Population *p, unsigned seed, int crystal) {
    Rng r; rng_seed(&r, 0x696e697469616cULL + seed);
    memset(p, 0, sizeof *p);
    for (int i = 0; i < NPAIR; i++)
        for (int j = 0; j < 2; j++)
            for (int k = 0; k < GLEN; k++)
                p->pair[i].member[j].dna[k] = crystal ? OP_EMIT : (uint8_t)rng_below(&r, 256);
}

static Result evaluate(World *w, Population *p, unsigned seed) {
    Rng test; rng_seed(&test, 0x746573742d6e6577ULL + seed);
    unsigned stimuli[NTEST];
    for (int i = 0; i < NTEST; i++) stimuli[i] = 16 + rng_below(&test, 240);
    uint8_t inert[GLEN]; memset(inert, OP_YIELD, sizeof inert);
    Result result = {0}; result.hash = FNV_INIT;
    for (int i = 0; i < NPAIR; i++) {
        Pair *pair = &p->pair[i]; result.train += training(w, pair);
        for (int j = 0; j < 2; j++) {
            result.hash = fnv1a(pair->member[j].dna, GLEN, result.hash);
            int previous = 0;
            for (int k = 0; k < 2 * i + j; k++)
                if (!memcmp(pair->member[j].dna, p->pair[k / 2].member[k % 2].dna, GLEN)) previous = 1;
            result.unique += !previous;
        }
        for (int t = 0; t < NTEST; t++) {
            unsigned s = stimuli[t];
            int a = response(w, pair->member[0].dna, s);
            int b = response(w, pair->member[1].dna, s);
            int absent = -1, stopped = response(w, inert, s);
            result.emitting += (a >= 0) + (b >= 0);
            result.products += product(a, b, s);
            result.alone += product(a, absent, s);
            result.ablated += product(a, stopped, s);
        }
    }
    return result;
}

static void positive(Pair *pair) {
    memset(pair, 0, sizeof *pair);
    for (int j = 0; j < 2; j++) memset(pair->member[j].dna, 0xff, GLEN);
    uint8_t a[] = { OP_SENSE_STIM, OP_EMIT, OP_YIELD };
    uint8_t b[] = { OP_SENSE_STIM, OP_INC_A, OP_EMIT, OP_YIELD };
    memcpy(pair->member[0].dna, a, sizeof a);
    memcpy(pair->member[1].dna, b, sizeof b);
}

static void selftest(void) {
    Rng rng; rng_seed(&rng, 3); World *w = new_sandbox(&rng);
    Rng outside_rng = rng; World *outside = new_sandbox(&outside_rng);
    uint64_t outside_before = world_hash(outside);
    Population *p = malloc(sizeof *p), *copy = malloc(sizeof *copy);
    assert(p && copy);
    Pair pos; positive(&pos);
    for (int i = 0; i < NPAIR; i++) p->pair[i] = pos;
    Result good = evaluate(w, p, 0);
    assert(good.products == DENOM && good.alone == 0 && good.ablated == 0);
    assert(good.train == NPAIR * NTRAIN);
    initialize(p, 0, 1); Result crystal = evaluate(w, p, 0);
    assert(crystal.products == 0);
    initialize(p, 7, 0); *copy = *p;
    Rng before = rng;
    uint64_t input = fnv1a(p, sizeof *p, FNV_INIT);
    (void)evaluate(w, p, 7);
    /* Cache est la seule écriture permise : ADN et RNG du bac restent identiques. */
    for (int i = 0; i < NPAIR; i++) for (int j = 0; j < 2; j++)
        assert(!memcmp(p->pair[i].member[j].dna, copy->pair[i].member[j].dna, GLEN));
    assert(!memcmp(&before, &rng, sizeof rng));
    *p = *copy; assert(input == fnv1a(p, sizeof *p, FNV_INIT));
    uint8_t writes[GLEN]; memset(writes, 0xff, sizeof writes);
    writes[0] = OP_LOADI; writes[1] = OP_YIELD;
    writes[2] = OP_STORE; writes[3] = 0; writes[4] = OP_EMIT;
    uint8_t saved[GLEN]; memcpy(saved, writes, sizeof writes);
    (void)response(w, writes, 0); assert(!memcmp(saved, writes, sizeof writes));
    /* Deux évaluations sans mutation extérieure déterministes. */
    initialize(p, 11, 0); *copy = *p;
    Result a = evaluate(w, p, 11), b = evaluate(w, copy, 11);
    assert(a.products == b.products && a.hash == b.hash);
    reshuffle(copy, &rng);
    int seen[2 * NPAIR] = {0};
    for (int i = 0; i < NPAIR; i++) for (int j = 0; j < 2; j++) {
        int found = -1;
        for (int k = 0; k < 2 * NPAIR; k++)
            if (!seen[k] && !memcmp(copy->pair[i].member[j].dna,
                                   p->pair[k / 2].member[k % 2].dna, GLEN)) { found = k; break; }
        assert(found >= 0); seen[found] = 1;
    }
    assert(outside_before == world_hash(outside));
    free(outside); free(p); free(copy); free(w);
    puts("selftest symbiose-paquets OK : positif 8192/8192, cristal/ablation/seul 0, ADN immuable, RNG intact, déterminisme, permutation conserve 128 membres");
}

static void run(void) {
    int passes = 0, noise_high = 0, crystal_high = 0;
    puts("# symbiose-paquets preenregistre : 32 graines, 64 paires ADN32, 512 generations, 16 substrats selection, 128 tests reserves");
    puts("# graine bras produits total rendement seul ablation produits_entrainement emissions genotypes hash");
    for (unsigned seed = 0; seed < NSEED; seed++) {
        Result results[NARM];
        for (int arm = 0; arm < NARM; arm++) {
            Rng rng; rng_seed(&rng, seed); World *w = new_sandbox(&rng);
            Population *pop = malloc(sizeof *pop);
            if (!pop) { perror("malloc population"); exit(1); }
            initialize(pop, seed, arm == CRYSTAL);
            evolve(w, pop, arm, seed);
            Result r = evaluate(w, pop, seed); results[arm] = r;
            printf("%u %s %d %d %.8f %d %d %d %d %d %016llx\n", seed, names[arm],
                   r.products, DENOM, (double)r.products / DENOM, r.alone, r.ablated,
                   r.train, r.emitting, r.unique, (unsigned long long)r.hash);
            if (arm == PACKET || arm == SHUFFLE) {
                int best = 0, score = -1;
                for (int i = 0; i < NPAIR; i++) {
                    int k = training(w, &pop->pair[i]); if (k > score) { best = i; score = k; }
                }
                printf("# paire %u %s entrainement %d/16 ADN", seed, names[arm], score);
                for (int j = 0; j < 2; j++) {
                    putchar(' ');
                    for (int k = 0; k < GLEN; k++) printf("%02x", pop->pair[best].member[j].dna[k]);
                }
                putchar('\n');
            }
            fflush(stdout); free(pop); free(w);
        }
        int pass = results[PACKET].products * 10 >= 9 * DENOM &&
            (results[PACKET].products - results[SHUFFLE].products) * 5 >= DENOM &&
            results[PACKET].alone * 10 <= DENOM && results[PACKET].ablated * 10 <= DENOM;
        passes += pass;
        noise_high += results[NOISE].products * 10 >= 9 * DENOM;
        crystal_high += results[CRYSTAL].products * 10 >= 9 * DENOM;
        printf("# decision graine %u passe %d\n", seed, pass);
    }
    printf("# total passent %d/32 bruit_haut %d/32 cristal_haut %d/32 verdict %s\n", passes,
           noise_high, crystal_high, noise_high > 2 || crystal_high > 2 ? "INVALIDE" : passes >= 24 ? "OUI" : "NON");
}

int main(int argc, char **argv) {
    if (argc != 2 || (strcmp(argv[1], "selftest") && strcmp(argv[1], "run"))) {
        fprintf(stderr, "usage: %s selftest|run\n", argv[0]); return 2;
    }
    if (!strcmp(argv[1], "selftest")) selftest(); else run();
    return 0;
}
