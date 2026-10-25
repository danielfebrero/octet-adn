/* Héritage écologique : ne transférer que food[][] entre deux mondes neufs.
 * Protocole et seuils : README.md, pré-enregistrés avant l'appel run.
 * Aucun changement de src/, aucun hook dans la VM. */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <assert.h>
#include "octet.h"

enum { EW = 32, EH = 16, CELLS = EW * EH, FOUNDERS = 4,
       DONOR_TICKS = 96, RECIPIENT_TICKS = 160, HALF_TICKS = 80 };
typedef struct { uint8_t v[EH][EW]; } Field;
typedef struct { int x, y, d; } Anchor;
typedef struct {
    long biomass, births, deaths;
    int final_population, final_food;
    uint64_t final_hash;
} Score;
typedef struct {
    Score h, p, v;
    int initial_food, inherited_food, donor_population;
    long donor_births, donor_deaths;
    uint64_t field_hash, donor_hash, genome_hash;
    int pass;
} Trial;

/* world.c ne les appelle jamais car tous les mondes de cet essai ont lab=NULL.
 * Une régression qui violerait ce contrat doit échouer bruyamment. */
void lab_pre(Lab *l, World *w) { (void)l; (void)w; abort(); }
void lab_post(Lab *l, World *w) { (void)l; (void)w; abort(); }

static WorldConfig config(void) {
    WorldConfig c = cfg_default();
    c.w = EW; c.h = EH; c.food_density = 0.50; c.food_regrow = 0;
    c.max_pop = 64; c.start_energy = 160; c.split_energy = 70;
    c.mut_rate = 0; c.dup_rate = 0; c.crossover = 0; c.germline = 1;
    return c;
}

static int field_sum(const Field *f) {
    int n = 0;
    for (int y = 0; y < EH; y++)
        for (int x = 0; x < EW; x++) n += f->v[y][x];
    return n;
}

static void field_get(Field *f, const World *w) {
    for (int y = 0; y < EH; y++) memcpy(f->v[y], w->food[y], EW);
}

static void field_put(World *w, const Field *f) {
    for (int y = 0; y < EH; y++) memcpy(w->food[y], f->v[y], EW);
}

static uint8_t *field_cell(Field *f, int i) { return &f->v[i / EW][i % EW]; }

static void permute(Field *out, const Field *in, uint64_t seed) {
    *out = *in;
    Rng r; rng_seed(&r, seed);
    for (int i = CELLS - 1; i > 0; i--) {
        int j = (int)rng_below(&r, (uint32_t)(i + 1));
        uint8_t *a = field_cell(out, i), *b = field_cell(out, j), t = *a;
        *a = *b; *b = t;
    }
}

/* Déplétion/ajout neutre : chaque unité modifiée choisit uniformément parmi
 * toutes les cases admissibles, sans consulter organismes ni géométrie. */
static void match_total(Field *out, const Field *initial, int target, uint64_t seed) {
    assert(target >= 0 && target <= 14 * CELLS);
    *out = *initial;
    Rng r; rng_seed(&r, seed);
    int total = field_sum(out), admissible[CELLS];
    while (total != target) {
        int down = total > target, n = 0;
        for (int i = 0; i < CELLS; i++) {
            int value = *field_cell(out, i);
            if (down ? value > 0 : value < 14) admissible[n++] = i;
        }
        assert(n > 0);
        int i = admissible[rng_below(&r, (uint32_t)n)];
        uint8_t *value = field_cell(out, i);
        if (down) { (*value)--; total--; }
        else { (*value)++; total++; }
    }
}

static World *recipient(const PoolEntry *g, const Field *f,
                        const Anchor anchor[FOUNDERS], Rng *rng, uint64_t seed) {
    rng_seed(rng, seed);
    WorldConfig cfg = config();
    World *w = world_new(NULL, 0, rng, &cfg, NULL);
    field_put(w, f);
    for (int i = 0; i < FOUNDERS; i++) {
        Creature *c = creature_new(anchor[i].x, anchor[i].y, g->dna, g->len,
                                   cfg.start_energy, w->nid++, 0, -1);
        c->d = anchor[i].d;
        world_spawn(w, c);
    }
    memcpy(w->cs, w->nb, sizeof *w->cs * FOUNDERS);
    w->ncs = w->nnb; w->nnb = 0; w->st.born = FOUNDERS;
    return w;
}

static void verify_germlines(const World *w, const PoolEntry *g) {
    for (int i = 0; i < w->ncs; i++) {
        const Creature *c = w->cs[i];
        assert(c->germ_len == g->len);
        assert(!memcmp(c->germ, g->dna, (size_t)g->len));
    }
}

static Score measure(const PoolEntry *g, const Field *f,
                     const Anchor anchor[FOUNDERS], uint64_t seed) {
    Rng rng;
    World *w = recipient(g, f, anchor, &rng, seed);
    Score s = { 0 };
    for (int t = 1; t <= RECIPIENT_TICKS; t++) {
        world_step(w);
        verify_germlines(w, g);
        if (t > HALF_TICKS)
            for (int i = 0; i < w->ncs; i++) {
                assert(w->cs[i]->e > 0);
                s.biomass += w->cs[i]->e;
            }
    }
    s.births = w->st.born - FOUNDERS; s.deaths = w->st.dead;
    s.final_population = w->ncs;
    Field final; field_get(&final, w); s.final_food = field_sum(&final);
    s.final_hash = world_hash(w);
    assert(FOUNDERS + s.births - s.deaths == s.final_population);
    world_free(w);
    return s;
}

static int passes(Score h, Score p, Score v) {
    return h.biomass > 0 && h.births > 0 &&
        10 * h.biomass >= 11 * p.biomass && 10 * h.biomass >= 11 * v.biomass &&
         5 * h.births >= 6 * p.births && 5 * h.births >= 6 * v.births;
}

static Trial trial(const PoolEntry *g, uint64_t seed) {
    Rng rng; rng_seed(&rng, seed);
    PoolEntry founders[FOUNDERS];
    for (int i = 0; i < FOUNDERS; i++) founders[i] = *g;
    WorldConfig cfg = config();
    World *donor = world_new(founders, FOUNDERS, &rng, &cfg, NULL);
    Anchor anchor[FOUNDERS];
    for (int i = 0; i < FOUNDERS; i++) {
        anchor[i].x = donor->cs[i]->x; anchor[i].y = donor->cs[i]->y;
        anchor[i].d = donor->cs[i]->d;
    }
    Field initial, inherited, shuffled, virgin;
    field_get(&initial, donor);
    for (int t = 0; t < DONOR_TICKS; t++) {
        world_step(donor);
        verify_germlines(donor, g);
    }
    field_get(&inherited, donor);
    Trial t = { 0 };
    t.initial_food = field_sum(&initial); t.inherited_food = field_sum(&inherited);
    t.donor_population = donor->ncs; t.donor_births = donor->st.born - FOUNDERS;
    t.donor_deaths = donor->st.dead; t.donor_hash = world_hash(donor);
    t.field_hash = fnv1a(&inherited, sizeof inherited, FNV_INIT);
    t.genome_hash = fnv1a(g->dna, (size_t)g->len, FNV_INIT);
    assert(FOUNDERS + t.donor_births - t.donor_deaths == t.donor_population);
    world_free(donor); /* aucun corps donneur n'existe pendant les trois mesures */

    permute(&shuffled, &inherited, seed ^ UINT64_C(0x5045524d555445));
    match_total(&virgin, &initial, t.inherited_food, seed ^ UINT64_C(0x56495247494e));
    assert(field_sum(&shuffled) == t.inherited_food);
    assert(field_sum(&virgin) == t.inherited_food);
    uint64_t recipient_seed = seed ^ UINT64_C(0x524543495049454e);
    t.h = measure(g, &inherited, anchor, recipient_seed);
    t.p = measure(g, &shuffled, anchor, recipient_seed);
    t.v = measure(g, &virgin, anchor, recipient_seed);
    t.pass = passes(t.h, t.p, t.v);
    return t;
}

static int same_score(Score a, Score b) {
    return a.biomass == b.biomass && a.births == b.births && a.deaths == b.deaths &&
        a.final_population == b.final_population && a.final_food == b.final_food &&
        a.final_hash == b.final_hash;
}

static int load_forager(PoolEntry *g) {
    memset(g, 0, sizeof *g);
    FILE *f = fopen("seeds/forager.adn", "rb");
    if (!f) { perror("seeds/forager.adn"); return 0; }
    size_t n = fread(g->dna, 1, MAX_LEN, f);
    int extra = fgetc(f), failed = ferror(f);
    fclose(f);
    if (failed || n < MIN_LEN || extra != EOF) {
        fprintf(stderr, "seed forager invalide\n"); return 0;
    }
    g->len = (int)n;
    return 1;
}

static int selftest(void) {
    Field f, p, v;
    for (int i = 0; i < CELLS; i++) *field_cell(&f, i) = (uint8_t)(i % 15);
    permute(&p, &f, UINT64_C(0x54455354));
    int hist_f[15] = { 0 }, hist_p[15] = { 0 };
    for (int i = 0; i < CELLS; i++) {
        hist_f[*field_cell(&f, i)]++; hist_p[*field_cell(&p, i)]++;
    }
    assert(!memcmp(hist_f, hist_p, sizeof hist_f));
    assert(field_sum(&f) == field_sum(&p));
    match_total(&v, &f, 1, 13); assert(field_sum(&v) == 1);
    match_total(&v, &f, 14 * CELLS - 1, 13); assert(field_sum(&v) == 14 * CELLS - 1);
    match_total(&v, &f, 0, 13); assert(field_sum(&v) == 0);
    match_total(&v, &f, 14 * CELLS, 13); assert(field_sum(&v) == 14 * CELLS);

    PoolEntry g;
    if (!load_forager(&g)) return 1;
    Anchor anchor[FOUNDERS] = { { 1, 2, 3 }, { 2, 3, 0 }, { 3, 4, 1 }, { 4, 5, 2 } };
    Rng ra, rb;
    World *a = recipient(&g, &f, anchor, &ra, 7);
    World *b = recipient(&g, &p, anchor, &rb, 7);
    assert(a->t == 0 && a->nid == FOUNDERS && a->hist_n == 0 && a->lab == NULL);
    assert(!memcmp(&ra, &rb, sizeof ra));
    assert(!memcmp(&a->st, &b->st, sizeof a->st));
    assert(a->st.born == FOUNDERS && a->st.dead == 0);
    for (int i = 0; i < FOUNDERS; i++) {
        Creature *c = a->cs[i];
        assert(!memcmp(c, b->cs[i], sizeof *c));
        assert(c->x == anchor[i].x && c->y == anchor[i].y && c->d == anchor[i].d);
        assert(c->a == 0 && c->b == 0 && c->ip == 0 && c->age == 0 && c->parent == -1);
        assert(c->e == 160 && c->stores == 0 && c->splits == 0 && c->lscore == 0);
        assert(!memcmp(c->dna, g.dna, (size_t)g.len));
    }
    world_free(a); world_free(b);

    /* Champ cristallin : aucune géométrie supplémentaire après permutation. */
    memset(&f, 7, sizeof f); permute(&p, &f, 17);
    assert(!memcmp(&f, &p, sizeof f));
    Score sa = measure(&g, &f, anchor, 41), sb = measure(&g, &p, anchor, 41);
    assert(same_score(sa, sb)); assert(!passes(sa, sb, sb));
    Score zero = { 0 }, h = { .biomass = 110, .births = 6 };
    Score control = { .biomass = 100, .births = 5 };
    assert(passes(h, control, control));
    assert(!passes(zero, zero, zero));
    h.biomass = 109; assert(!passes(h, control, control));
    h.biomass = 110; h.births = 5; assert(!passes(h, control, control));
    h.births = 6; control.biomass = 101; assert(!passes(h, control, zero));

    Trial t1 = trial(&g, UINT64_C(0x544553540001));
    Trial t2 = trial(&g, UINT64_C(0x544553540001));
    assert(same_score(t1.h, t2.h) && same_score(t1.p, t2.p) && same_score(t1.v, t2.v));
    assert(t1.field_hash == t2.field_hash && t1.donor_hash == t2.donor_hash && t1.pass == t2.pass);
    puts("selftest heritage-milieu : conservation et histogramme OK ; recepteurs frais et apparies OK ; genealogies fixes OK ; cristal sans avantage OK ; seuils exacts OK ; determinisme OK");
    return 0;
}

static void print_trial(const char *group, int s, uint64_t seed, Trial t) {
    printf("%s %d %" PRIu64 " %016" PRIx64 " %d %d %ld %ld %d "
           "%ld %ld %ld %d %d %016" PRIx64 " "
           "%ld %ld %ld %d %d %016" PRIx64 " "
           "%ld %ld %ld %d %d %016" PRIx64 " %016" PRIx64 " %016" PRIx64 " %d\n",
           group, s, seed, t.genome_hash, t.initial_food, t.inherited_food,
           t.donor_births, t.donor_deaths, t.donor_population,
           t.h.biomass, t.h.births, t.h.deaths, t.h.final_population, t.h.final_food, t.h.final_hash,
           t.p.biomass, t.p.births, t.p.deaths, t.p.final_population, t.p.final_food, t.p.final_hash,
           t.v.biomass, t.v.births, t.v.deaths, t.v.final_population, t.v.final_food, t.v.final_hash,
           t.field_hash, t.donor_hash, t.pass);
}

static int run(void) {
    PoolEntry forager;
    if (!load_forager(&forager)) return 1;
    int totals[3] = { 0 };
    puts("# heritage-milieu ordre0996a2cd : H/P/V ; biomasse=sum energie ticks81..160 ; births exclut4fondateurs");
    puts("# groupe graine monde_seed genome_hash nourriture_initiale nourriture_transmise donneur_naissances donneur_morts donneur_pop H_biomasse H_naissances H_morts H_pop H_food H_hash P_biomasse P_naissances P_morts P_pop P_food P_hash V_biomasse V_naissances V_morts V_pop V_food V_hash field_hash donor_hash passe");
    const char *names[] = { "forager", "bruit", "cristal" };
    const uint64_t base[] = { UINT64_C(0x45434f000000), UINT64_C(0x425255000000), UINT64_C(0x435259000000) };
    for (int group = 0; group < 3; group++) {
        int count = group == 0 ? 32 : 16;
        for (int s = 1; s <= count; s++) {
            PoolEntry g = forager;
            if (group == 1) {
                Rng r; rng_seed(&r, UINT64_C(0x47454e4f000000) + (uint64_t)s);
                g.len = 256;
                for (int i = 0; i < g.len; i++) g.dna[i] = (uint8_t)rng_below(&r, 256);
            } else if (group == 2) { g.len = 256; memset(g.dna, 0x11, (size_t)g.len); }
            uint64_t seed = base[group] + (uint64_t)s;
            Trial t = trial(&g, seed);
            print_trial(names[group], s, seed, t);
            totals[group] += t.pass;
        }
    }
    printf("# comptes forager %d/32 seuil>=24 ; bruit %d/16 seuil<=2 ; cristal %d/16 seuil=0\n",
           totals[0], totals[1], totals[2]);
    printf("# verdict %s\n", totals[0] >= 24 && totals[1] <= 2 && totals[2] == 0 ? "oui" : "non");
    return 0; /* un non scientifique est une mesure réussie, pas une erreur d'exécution */
}

int main(int argc, char **argv) {
    setvbuf(stdout, NULL, _IOLBF, 0);
    if (argc == 2 && !strcmp(argv[1], "selftest")) return selftest();
    if (argc == 2 && !strcmp(argv[1], "run")) return run();
    fprintf(stderr, "usage: %s selftest|run (depuis la racine du depot)\n", argv[0]);
    return 2;
}
