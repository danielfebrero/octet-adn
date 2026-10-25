/* Sonde causale par relecture d'une sequence complete. Voir pre-enregistrement. */
#include "octet.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void world_move(World *, Creature *);
void world_eat(World *, Creature *);
void world_split(World *, Creature *);
void world_attack(World *, Creature *);
void world_emit(World *, Creature *, int);

enum { HORIZON = 512, MAX_EVENTS = OPS_PER_TICK, N_SHIFT = 4, N_SEEDS_RUN = 32 };
static const int shifts[N_SHIFT] = { 64, 128, 256, 384 };
typedef struct { uint8_t op, arg; } Event;
typedef struct { int n; Event ev[MAX_EVENTS]; } Tick;
typedef struct {
    int alive, x, y, dir, energy, age, steps, eaten, splits;
    long tick, born, dead, death_age, ate, attacks, crossovers, emits;
    uint8_t food[GRID_H][GRID_W];
    int occ[GRID_H][GRID_W];
    Rng rng;
} Physical;
typedef struct { Tick ticks[HORIZON]; Physical states[HORIZON]; } Trace;
typedef struct { long ate; int alive; uint64_t hash; } Outcome;
typedef struct {
    Outcome source, exact, shifted[N_SHIFT];
    long events, prescribed[N_SHIFT];
    int exact_ok, passed, alive_all;
} Result;
static Trace *observed;
static const Trace *played;
static int tick_index, shift_index;
static World *playing_world;
static int trace_overflow;

static int physical_action(int op) {
    return op == OP_TURN || op == OP_STEP || op == OP_EAT || op == OP_SPLIT ||
           op == OP_ATTACK || op == OP_EMIT;
}
static void record_action(Creature *c, uint8_t op) {
    if (!physical_action(op) || (op == OP_EMIT && c->emitted >= 0)) return;
    Tick *t = &observed->ticks[tick_index];
    if (t->n >= MAX_EVENTS) { trace_overflow = 1; return; }
    t->ev[t->n++] = (Event){ op, c->a };
}
static void replay_actions(Creature *c, uint8_t op) {
    /* The YIELD is observed after vm_run increments age and resets emitted,
     * exactly where the first source instruction would start. */
    assert(op == OP_YIELD);
    const Tick *t = &played->ticks[(tick_index + shift_index) % HORIZON];
    for (int i = 0; i < t->n && c->e > 0; i++) {
        Event e = t->ev[i];
        switch (e.op) {
        case OP_TURN: c->d = e.arg % 4; break;
        case OP_STEP: world_move(playing_world, c); break;
        case OP_EAT: world_eat(playing_world, c); break;
        case OP_SPLIT: world_split(playing_world, c); break;
        case OP_ATTACK: world_attack(playing_world, c); break;
        case OP_EMIT:
            c->emitted = e.arg;
            world_emit(playing_world, c, e.arg);
            break;
        default: abort();
        }
    }
}
static Physical snapshot(const World *w) {
    Physical p;
    memset(&p, 0, sizeof p);
    p.alive = w->ncs;
    p.tick = w->t;
    p.born = w->st.born; p.dead = w->st.dead;
    p.death_age = w->st.death_age_sum; p.ate = w->st.energy_eaten;
    p.attacks = w->st.attacks; p.crossovers = w->st.crossovers; p.emits = w->st.emits;
    memcpy(p.food, w->food, sizeof p.food);
    memcpy(p.occ, w->occ, sizeof p.occ);
    p.rng = *w->rng;
    if (w->ncs) {
        const Creature *c = w->cs[0];
        p.x = c->x; p.y = c->y; p.dir = c->d; p.energy = c->e;
        p.age = c->age; p.steps = c->steps; p.eaten = c->eaten; p.splits = c->splits;
    }
    return p;
}
static World *arena(const PoolEntry *g, Rng *r, uint64_t seed, int replay) {
    rng_seed(r, seed);
    WorldConfig cfg = cfg_default();
    cfg.max_pop = 1;
    World *w = world_new(g, 1, r, &cfg, NULL);
    if (replay) {
        Creature *c = w->cs[0];
        c->len = 1; c->dna[0] = OP_YIELD; c->ip = 0;
    }
    return w;
}
static Outcome finish(const World *w) {
    Physical p = snapshot(w);
    Outcome o;
    memset(&o, 0, sizeof o);
    o.ate = w->st.energy_eaten;
    o.alive = w->ncs;
    o.hash = fnv1a(&p, sizeof p, FNV_INIT);
    return o;
}
static Outcome collect(const PoolEntry *g, uint64_t seed, Trace *tr) {
    Rng r;
    World *w = arena(g, &r, seed, 0);
    memset(tr, 0, sizeof *tr);
    observed = tr; trace_overflow = 0;
    vm_set_watch(record_action);
    for (tick_index = 0; tick_index < HORIZON; tick_index++) {
        world_step(w);
        tr->states[tick_index] = snapshot(w);
    }
    vm_set_watch(NULL); observed = NULL;
    if (trace_overflow) { fputs("trace overflow\n", stderr); abort(); }
    Outcome o = finish(w);
    world_free(w);
    return o;
}
static Outcome replay(const PoolEntry *g, uint64_t seed, const Trace *tr,
                      int shift, int *exact_ok) {
    Rng r;
    World *w = arena(g, &r, seed, 1);
    played = tr; playing_world = w; shift_index = shift;
    vm_set_watch(replay_actions);
    for (tick_index = 0; tick_index < HORIZON; tick_index++) {
        world_step(w);
        if (!shift && exact_ok) {
            Physical p = snapshot(w);
            if (memcmp(&p, &tr->states[tick_index], sizeof p)) *exact_ok = 0;
        }
    }
    vm_set_watch(NULL); played = NULL; playing_world = NULL;
    Outcome o = finish(w);
    world_free(w);
    return o;
}
static int seed_forager(PoolEntry *g) {
    for (int i = 0; i < N_SEEDS; i++) {
        const SeedBlob *s = seed_at(i);
        if (strcmp(s->name, "forager")) continue;
        memset(g, 0, sizeof *g);
        g->len = s->len; memcpy(g->dna, s->p, (size_t)s->len);
        return 1;
    }
    return 0;
}
static PoolEntry genome(int group, int s) {
    PoolEntry g;
    if (!seed_forager(&g)) { fputs("seed forager missing\n", stderr); abort(); }
    if (group == 1) {
        Rng r; rng_seed(&r, UINT64_C(0xA0996A2CD0000000) + (uint64_t)s);
        for (int k = 0; k < g.len; k++) g.dna[k] = (uint8_t)rng_below(&r, 256);
    } else if (group == 2) memset(g.dna, 0x11, (size_t)g.len);
    return g;
}
static Result measure(const PoolEntry *g, uint64_t seed) {
    Result v;
    memset(&v, 0, sizeof v);
    Trace *tr = xcalloc(1, sizeof *tr);
    v.source = collect(g, seed, tr);
    v.exact_ok = 1;
    v.exact = replay(g, seed, tr, 0, &v.exact_ok);
    long shuffled_total = 0;
    v.alive_all = v.source.alive;
    for (int t = 0; t < HORIZON; t++) v.events += tr->ticks[t].n;
    for (int i = 0; i < N_SHIFT; i++) {
        for (int t = 0; t < HORIZON; t++)
            v.prescribed[i] += tr->ticks[(t + shifts[i]) % HORIZON].n;
        v.shifted[i] = replay(g, seed, tr, shifts[i], &v.exact_ok);
        shuffled_total += v.shifted[i].ate;
        v.alive_all &= v.shifted[i].alive;
    }
    /* Integer form of source >= 1.10 * mean(shifts), avoiding rounding. */
    v.passed = v.exact_ok && v.alive_all && v.source.ate > 0 &&
               v.source.ate * (10 * N_SHIFT) >= 11 * shuffled_total;
    free(tr);
    return v;
}
static int same_outcome(const Outcome *a, const Outcome *b) {
    return a->ate == b->ate && a->alive == b->alive && a->hash == b->hash;
}
static int same_result(const Result *a, const Result *b) {
    /* Ne pas comparer le padding des structures, indéterminé selon le compilateur. */
    if (!same_outcome(&a->source, &b->source) ||
        !same_outcome(&a->exact, &b->exact) || a->events != b->events ||
        a->exact_ok != b->exact_ok || a->passed != b->passed ||
        a->alive_all != b->alive_all) return 0;
    for (int i = 0; i < N_SHIFT; i++)
        if (!same_outcome(&a->shifted[i], &b->shifted[i]) ||
            a->prescribed[i] != b->prescribed[i]) return 0;
    return 1;
}
static int selftest(void) {
    PoolEntry g = genome(0, 0);
    Result a = measure(&g, UINT64_C(0x5300996));
    Result b = measure(&g, UINT64_C(0x5300996));
    if (!a.exact_ok || !same_result(&a, &b)) return 1;
    for (int i = 0; i < N_SHIFT; i++) if (a.events != a.prescribed[i]) return 2;
    for (int s = 0; s < 4; s++) {
        g = genome(1, 100 + s);
        Result n = measure(&g, UINT64_C(0x5300996) + (uint64_t)s);
        if (!n.exact_ok) return 3;
        for (int i = 0; i < N_SHIFT; i++) if (n.events != n.prescribed[i]) return 4;
    }
    /* Constant action stream: exactly EAT,YIELD every tick, no sensory choice.
     * Large initial reserve is not offered: the normal world rules apply. */
    memset(&g, 0, sizeof g); g.len = 8;
    memset(g.dna, OP_LOOP, (size_t)g.len);
    g.dna[0] = OP_EAT; g.dna[1] = OP_YIELD;
    g.dna[2] = OP_LOOP;
    Result constant = measure(&g, UINT64_C(0x5300997));
    if (!constant.exact_ok) return 5;
    if (constant.passed) return 6;
    /* A prescribed constant action stream is invariant under all rotations,
     * including when the executor dies. Do not rotate a truncated recording
     * here: its empty suffix would introduce an artificial time dependence. */
    Trace *fixed = xcalloc(1, sizeof *fixed);
    for (int t = 0; t < HORIZON; t++) {
        fixed->ticks[t].n = 1;
        fixed->ticks[t].ev[0] = (Event){ OP_EAT, 0 };
    }
    Outcome unshifted = replay(&g, UINT64_C(0x5300997), fixed, 0, NULL);
    for (int i = 0; i < N_SHIFT; i++) {
        Outcome shifted = replay(&g, UINT64_C(0x5300997), fixed, shifts[i], NULL);
        if (unshifted.ate != shifted.ate || unshifted.alive != shifted.alive ||
            unshifted.hash != shifted.hash) { free(fixed); return 8; }
    }
    free(fixed);
    /* Sensor-conditional fixture: IF food ahead, step; then eat. */
    memset(&g, 0xff, sizeof g); g.species = 0; g.len = 8;
    const uint8_t fixture[8] = { OP_SENSE, OP_JZ, 1, OP_STEP, OP_EAT, OP_YIELD, OP_LOOP, OP_NOP };
    memcpy(g.dna, fixture, sizeof fixture);
    Result sensor = measure(&g, UINT64_C(0x5300998));
    if (!sensor.exact_ok) return 7;
    puts("perception-sequence selftest OK: exact replay, determinism, rotations, constant, sensor fixture");
    return 0;
}
static int run(void) {
    const char *names[3] = { "forager", "bruit", "cristal" };
    int passed[3] = {0}, alive[3] = {0}, exact[3] = {0};
    long source_sum[3] = {0}, shifted_sum[3] = {0};
    puts("perception-sequence H=512 seeds=32 shifts=64,128,256,384 max_pop=1");
    puts("bras graine source_vivant decales_vivants recolte_source recoltes_decales exact passe hash_source");
    for (int arm = 0; arm < 3; arm++) for (int s = 0; s < N_SEEDS_RUN; s++) {
        PoolEntry g = genome(arm, s);
        Result v = measure(&g, UINT64_C(0x99000000) + (uint64_t)s);
        int nlive = 0;
        for (int i = 0; i < N_SHIFT; i++) nlive += v.shifted[i].alive;
        printf("%s %d %d %d %ld %ld,%ld,%ld,%ld %d %d %016llx\n", names[arm], s,
               v.source.alive, nlive, v.source.ate, v.shifted[0].ate, v.shifted[1].ate,
               v.shifted[2].ate, v.shifted[3].ate, v.exact_ok, v.passed,
               (unsigned long long)v.source.hash);
        exact[arm] += v.exact_ok; passed[arm] += v.passed; alive[arm] += v.alive_all;
        source_sum[arm] += v.source.ate;
        for (int i = 0; i < N_SHIFT; i++) shifted_sum[arm] += v.shifted[i].ate;
        if (!v.exact_ok) { fputs("INVALID: exact replay mismatch\n", stderr); return 2; }
        for (int i = 0; i < N_SHIFT; i++) if (v.events != v.prescribed[i]) return 3;
    }
    puts("comptes bras graines cinq_survivants passent exacts recolte_source_somme recolte_decalee_somme");
    for (int arm = 0; arm < 3; arm++)
        printf("compte %s %d %d %d %d %ld %ld\n", names[arm], N_SEEDS_RUN,
               alive[arm], passed[arm], exact[arm], source_sum[arm], shifted_sum[arm]);
    int yes = passed[0] >= 24 && passed[1] <= 1 && passed[2] <= 1;
    printf("verdict %s\n", yes ? "oui" : "non");
    return 0;
}
int main(int argc, char **argv) {
    if (argc == 2 && !strcmp(argv[1], "selftest")) return selftest();
    if (argc == 2 && !strcmp(argv[1], "run")) return run();
    fprintf(stderr, "usage: %s selftest|run\n", argv[0]);
    return 1;
}
