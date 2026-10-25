#include <limits.h>
#include <stdio.h>
#include <string.h>
#include "naissance.h"

void naissance_init(NaissanceState *s, uint64_t seed) {
    memset(s, 0, sizeof *s);
    s->version = NAISSANCE_VERSION;
    rng_seed(&s->rng, seed);
}

static int close_torus(const World *w, const Creature *a, const Creature *b) {
    int dx = abs(a->x - b->x), dy = abs(a->y - b->y);
    if (dx > w->cfg.w - dx) dx = w->cfg.w - dx;
    if (dy > w->cfg.h - dy) dy = w->cfg.h - dy;
    return dx <= 1 && dy <= 1;
}

static int cooling(const NaissanceState *s, int id) {
    for (int i = 0; i < s->ncooldown; i++) if (s->cooldown_id[i] == id) return 1;
    return 0;
}

/* The list is compacted once per conception window. Its size is bounded by CMAX,
 * rather than by the monotonically increasing creature IDs. No Creature pointer
 * survives a call: an egg remains valid if either parent subsequently dies. */
static void expire_cooldown(NaissanceState *s, long t) {
    int n = 0;
    for (int i = 0; i < s->ncooldown; i++) if (s->cooldown_until[i] > t) {
        s->cooldown_id[n] = s->cooldown_id[i];
        s->cooldown_until[n++] = s->cooldown_until[i];
    }
    s->ncooldown = n;
}

static void parent_split(World *w, int id) {
    for (int i = 0; i < w->ncs; i++) if (w->cs[i]->id == id) {
        if (w->cs[i]->e > 0) w->cs[i]->splits++;
        return;
    }
}

/* Enumerate unique neighboring cells, including in tiny 1xN fixtures. The nest's
 * own cell is excluded even if a toroidal offset aliases it. Reservoir sampling
 * makes the choice uniform among available cells, independent of loop order. */
static int hatch_place(World *w, NaissanceState *s, const NaissanceEgg *e, int *x, int *y) {
    int seen_x[8], seen_y[8], nseen = 0, found = 0;
    for (int dy = -1; dy <= 1; dy++) for (int dx = -1; dx <= 1; dx++) {
        if (!dx && !dy) continue;
        int xx = (e->x + dx + w->cfg.w) % w->cfg.w;
        int yy = (e->y + dy + w->cfg.h) % w->cfg.h;
        if (xx == e->x && yy == e->y) continue;
        int duplicate = 0;
        for (int j = 0; j < nseen; j++) if (seen_x[j] == xx && seen_y[j] == yy) duplicate = 1;
        if (duplicate) continue;
        seen_x[nseen] = xx; seen_y[nseen++] = yy;
        if (w->occ[yy][xx] != 0) continue;
        if (rng_below(&s->rng, (uint32_t)++found) == 0) { *x = xx; *y = yy; }
    }
    return found > 0;
}

static void hatch(World *w, const NaissanceConfig *cfg, NaissanceState *s) {
    int cap = cfg->cap > 0 ? cfg->cap : w->cfg.max_pop;
    if (cap > CMAX) cap = CMAX;
    int n = 0;
    for (int i = 0; i < s->npending; i++) {
        NaissanceEgg *e = &s->pending[i];
        int x = 0, y = 0, keep = 1;
        if (e->due <= w->t) {
            if (w->ncs + w->nnb >= cap || w->ncs >= CMAX) s->refused_cap++;
            else if (!hatch_place(w, s, e, &x, &y)) s->refused_place++;
            else {
                Creature *c = creature_new(x, y, e->dna, e->len, e->energy,
                                           w->nid++, e->species, e->parent_a);
                /* creature_new zeros ls, registers and the soma, and copies dna
                 * to germ. The runner will assign its genotype after this call. */
                w->cs[w->ncs++] = c;
                w->occ[y][x]++;
                w->st.born++;
                w->st.crossovers++;
                parent_split(w, e->parent_a);
                parent_split(w, e->parent_b);
                s->hatched++;
                s->crossed += e->crossed;
                keep = 0;
            }
        }
        if (keep) s->pending[n++] = *e;
    }
    s->npending = n;
}

static const uint8_t *inheritance(const World *w, const Creature *c, int *len) {
    *len = c->len;
    if (w->cfg.germline && c->germ_len > 0 && c->germ_len <= MAX_LEN) {
        *len = c->germ_len; return c->germ;
    }
    return c->dna;
}

static void recombine(World *w, const NaissanceConfig *cfg, NaissanceState *s,
                      const Creature *a, const Creature *b, NaissanceEgg *e) {
    int na, nb;
    const uint8_t *da = inheritance(w, a, &na), *db = inheritance(w, b, &nb);
    int overlap = na < nb ? na : nb;
    e->len = na;
    memcpy(e->dna, da, (size_t)na);
    /* Replace one same-address segment, never splice an insertion/deletion.
     * Reject the complete interval when possible, preserving material from A.
     * Coincident alleles can still yield a parental clone: it is not counted as
     * crossed, even if mutation later makes its DNA new. */
    int lo, hi;
    do {
        lo = (int)rng_below(&s->rng, (uint32_t)overlap);
        hi = lo + 1 + (int)rng_below(&s->rng, (uint32_t)(overlap - lo));
    } while (overlap > 1 && lo == 0 && hi == overlap);
    memcpy(e->dna + lo, db + lo, (size_t)(hi - lo));
    e->crossed = memcmp(e->dna, da, (size_t)na) != 0
                 && (na != nb || memcmp(e->dna, db, (size_t)na) != 0);
    if (cfg->mutation > 0) for (int i = 0; i < na; i++) {
        if (rng_double(&s->rng) < cfg->mutation)
            e->dna[i] ^= (uint8_t)(1u << rng_below(&s->rng, 8));
    }
}

void naissance_step(World *w, const NaissanceConfig *cfg, NaissanceState *s) {
    if (!cfg->enabled) return;    /* no state changes, allocations or RNG draws */
    if (cfg->period <= 0 || cfg->cost <= 0 || cfg->food_cost <= 0
        || cfg->mutation < 0 || cfg->mutation > 1
        || w->cfg.w <= 0 || w->cfg.w > GRID_W || w->cfg.h <= 0 || w->cfg.h > GRID_H) {
        s->refused_config++; return;
    }
    hatch(w, cfg, s);
    if (w->t % cfg->period) return;
    if (cfg->season_period > 0 && (cfg->season_open <= 0
        || w->t % cfg->season_period >= cfg->season_open)) { s->refused_season++; return; }
    if (s->npending >= NAISSANCE_EGGS) { s->refused_queue++; return; }
    expire_cooldown(s, w->t);
    if (s->ncooldown > CMAX - 2) { s->refused_queue++; return; }

    /* Both reservoirs are uniform among their eligible candidates: no first
     * species or low array index receives preferential access to a partner. */
    Creature *a = NULL, *b = NULL;
    int eligible = 0, nearby = 0, low_energy = 0, viable = 0, energetic = 0, paused = 0;
    for (int i = 0; i < w->ncs; i++) {
        Creature *c = w->cs[i];
        if (c->e <= 0 || c->len <= 0 || c->len > MAX_LEN) continue;
        viable++;
        if (c->e <= cfg->cost) continue;
        energetic++;
        if (cooling(s, c->id)) continue;
        if (rng_below(&s->rng, (uint32_t)++eligible) == 0) a = c;
    }
    if (!a) {
        if (energetic) s->refused_cooldown++;
        else if (viable) s->refused_energy++;
        else s->refused_partner++;
        return;
    }
    for (int i = 0; i < w->ncs; i++) {
        Creature *c = w->cs[i];
        if (c == a || c->id == a->id || c->e <= 0 || c->len <= 0 || c->len > MAX_LEN
            || !close_torus(w, a, c)) continue;
        if (c->e <= cfg->cost) { low_energy = 1; continue; }
        if (cooling(s, c->id)) { paused = 1; continue; }
        if (rng_below(&s->rng, (uint32_t)++nearby) == 0) b = c;
    }
    if (!b) {
        if (paused) s->refused_cooldown++;
        else if (low_energy) s->refused_energy++;
        else s->refused_partner++;
        return;
    }
    if (w->food[a->y][a->x] < cfg->food_cost) { s->refused_food++; return; }

    NaissanceEgg *e = &s->pending[s->npending++];
    memset(e, 0, sizeof *e);
    e->x = a->x; e->y = a->y;
    e->parent_a = a->id; e->parent_b = b->id; e->species = a->species;
    long delay = cfg->delay > 0 ? cfg->delay : 1;
    e->due = w->t > LONG_MAX - delay ? LONG_MAX : w->t + delay;
    e->energy = cfg->cost >= 110 ? 220 : 2 * cfg->cost;
    recombine(w, cfg, s, a, b, e);
    a->e -= cfg->cost; b->e -= cfg->cost;
    w->food[a->y][a->x] -= (uint8_t)cfg->food_cost;
    s->energy_paid += 2L * cfg->cost; s->food_paid += cfg->food_cost;
    s->eggs++;
    long pause = 2L * cfg->period;
    if (pause < delay) pause = delay;
    long until = w->t > LONG_MAX - pause ? LONG_MAX : w->t + pause;
    s->cooldown_id[s->ncooldown] = a->id; s->cooldown_until[s->ncooldown++] = until;
    s->cooldown_id[s->ncooldown] = b->id; s->cooldown_until[s->ncooldown++] = until;
}

/* Tests use private, sparse worlds and never a live run or a checkpoint. */
static World *fixture(Rng *r, int width, int height, int ax, int ay, int bx, int by) {
    World *w = xcalloc(1, sizeof *w);
    w->cfg = cfg_default(); w->cfg.w = width; w->cfg.h = height;
    w->cfg.food_regrow = 0; w->cfg.max_pop = 20; w->rng = r;
    memset(w->food, 14, sizeof w->food);
    uint8_t a[MIN_LEN], b[MIN_LEN];
    memset(a, 0x80, sizeof a); memset(b, 0x90, sizeof b);
    a[0] = b[0] = OP_YIELD;
    w->cs[w->ncs++] = creature_new(ax, ay, a, MIN_LEN, 100, w->nid++, 0, -1);
    w->cs[w->ncs++] = creature_new(bx, by, b, MIN_LEN, 100, w->nid++, 1, -1);
    w->occ[ay][ax]++; w->occ[by][bx]++; w->st.born = 2;
    return w;
}

int naissance_selftest(void) {
    int failed = 0;
#define NCHECK(test) do { if (!(test)) { fprintf(stderr, "naissance selftest: %s (line %d)\n", #test, __LINE__); failed++; } } while (0)
    Rng r; rng_seed(&r, 27);
    NaissanceState *s = xcalloc(1, sizeof *s), *copy = xcalloc(1, sizeof *copy);
    NaissanceConfig cfg = { .enabled = 1, .period = 1, .cost = 10, .food_cost = 3,
                            .delay = 2, .cap = 3, .mutation = 0 };
    World *w = fixture(&r, 5, 5, 1, 1, 2, 1);
    naissance_init(s, 42);
    *copy = *s;
    Rng saved_rng = r;
    uint64_t hash = world_hash(w);
    cfg.enabled = 0; naissance_step(w, &cfg, s); cfg.enabled = 1;
    NCHECK(memcmp(s, copy, sizeof *s) == 0 && memcmp(&r, &saved_rng, sizeof r) == 0);
    NCHECK(world_hash(w) == hash && w->st.born == 2);
    world_step(w); naissance_step(w, &cfg, s);
    NCHECK(s->eggs == 1 && s->npending == 1 && s->hatched == 0 && w->ncs == 2);
    NCHECK(w->cs[0]->e == 89 && w->cs[1]->e == 89 && s->energy_paid == 20 && s->food_paid == 3);
    if (s->npending) {
        NaissanceEgg *e = &s->pending[0];
        NCHECK(e->parent_a != e->parent_b && e->due == 3);
        NCHECK(e->crossed == (memcmp(e->dna, w->cs[0]->dna, MIN_LEN) != 0
                              && memcmp(e->dna, w->cs[1]->dna, MIN_LEN) != 0));
        for (int i = 0; i < e->len; i++) NCHECK(e->dna[i] == w->cs[0]->dna[i] || e->dna[i] == w->cs[1]->dna[i]);
    }
    cfg.period = 100;
    world_step(w); naissance_step(w, &cfg, s);
    NCHECK(w->ncs == 2 && s->hatched == 0);
    world_step(w); naissance_step(w, &cfg, s);
    NCHECK(w->ncs == 3 && w->st.born == 3 && w->st.dead == 0 && s->hatched == 1 && s->crossed == 1);
    NCHECK(s->energy_paid == 20 && s->food_paid == 3 && w->cs[0]->splits == 1 && w->cs[1]->splits == 1);
    if (w->ncs == 3) {
        Creature *c = w->cs[2]; int zero_ls[6] = { 0 };
        NCHECK(c->e == 20 && c->id == 2 && c->germ_len == c->len && !memcmp(c->dna, c->germ, (size_t)c->len));
        NCHECK(!memcmp(c->ls, zero_ls, sizeof zero_ls) && w->occ[c->y][c->x] == 1);
    }
    world_free(w);

    /* A full population blocks an already paid egg, then a higher real cap
     * permits hatching without a death and without recreating the payment. */
    w = fixture(&r, 5, 5, 1, 1, 2, 1); naissance_init(s, 43);
    cfg.period = 1; cfg.delay = 1; cfg.cap = 2;
    world_step(w); naissance_step(w, &cfg, s); cfg.period = 100;
    world_step(w); naissance_step(w, &cfg, s);
    world_step(w); naissance_step(w, &cfg, s);
    NCHECK(w->ncs == 2 && s->npending == 1 && s->refused_cap == 2);
    NCHECK(s->energy_paid == 20 && s->food_paid == 3);
    cfg.cap = 3; world_step(w); naissance_step(w, &cfg, s);
    NCHECK(w->ncs == 3 && s->hatched == 1 && w->st.dead == 0 && s->energy_paid == 20);
    world_free(w);

    /* Le vrai plafond CMAX, pas seulement un cap de fixture à deux individus.
     * Une place libre loin du nid ne suffit pas ; une place locale débloque
     * l'œuf payé, sans nouveau débit et sans changer le vrai plafond. */
    w = fixture(&r, 32, 32, 1, 1, 2, 1); naissance_init(s, 4301);
    w->cfg.max_pop = CMAX;
    while (w->ncs < CMAX) {
        int x = w->ncs % 32, y = (w->ncs / 32) % 32;
        Creature *c = creature_new(x, y, w->cs[0]->dna, MIN_LEN, 100,
                                   w->nid++, 0, -1);
        w->cs[w->ncs++] = c; w->occ[y][x]++; w->st.born++;
    }
    cfg.period = 1; cfg.delay = 1; cfg.cap = CMAX;
    naissance_step(w, &cfg, s);
    NCHECK(w->ncs == CMAX && s->eggs == 1 && s->npending == 1);
    /* Quel que soit le nid tiré, son voisinage est entièrement occupé. */
    cfg.period = 100000; w->t = 1; naissance_step(w, &cfg, s);
    NCHECK(s->refused_cap == 1 && s->hatched == 0);
    Creature *removed = w->cs[--w->ncs];
    w->occ[removed->y][removed->x]--; free(removed);
    /* CMAX=4096 sur 1024 cases : retirer un seul occupant ne vide aucune
     * case. Le refus local reste donc observable à CMAX-1. */
    int ex = s->pending[0].x, ey = s->pending[0].y;
    int fx = (ex + 16) % 32, fy = (ey + 16) % 32;
    w->t = 2; naissance_step(w, &cfg, s);
    NCHECK(w->ncs == CMAX - 1 && s->refused_place == 1 && s->hatched == 0);
    int nx = (ex + 1) % 32, ny = ey;
    for (int i = 0; i < w->ncs; i++) if (w->cs[i]->x == nx && w->cs[i]->y == ny) {
        w->occ[ny][nx]--; w->occ[fy][fx]++;
        w->cs[i]->x = fx; w->cs[i]->y = fy;
    }
    w->t = 3; naissance_step(w, &cfg, s);
    NCHECK(w->ncs == CMAX && s->hatched == 1 && s->npending == 0);
    NCHECK(s->energy_paid == 20 && s->food_paid == 3 && s->refused_cap == 1);
    world_free(w);

    /* Both axes wrap: (0,0) and (4,4) are neighbors on a 5x5 torus. */
    w = fixture(&r, 5, 5, 0, 0, 4, 4); naissance_init(s, 44); cfg.period = 1;
    world_step(w); naissance_step(w, &cfg, s);
    NCHECK(s->eggs == 1);
    world_free(w);

    /* A free global slot does not create local space on a full tiny torus. */
    w = fixture(&r, 2, 1, 0, 0, 1, 0); naissance_init(s, 45); cfg.cap = 4;
    world_step(w); naissance_step(w, &cfg, s); cfg.period = 100;
    world_step(w); naissance_step(w, &cfg, s);
    NCHECK(w->ncs == 2 && s->npending == 1 && s->refused_place == 1 && s->refused_cap == 0);
    w->cfg.w = 3; world_step(w); naissance_step(w, &cfg, s);
    NCHECK(w->ncs == 3 && s->hatched == 1 && w->cs[2]->x == 2 && w->occ[0][2] == 1);
    world_free(w);

    /* Identical parents plus mutation must never be reported as recombination. */
    w = fixture(&r, 5, 5, 1, 1, 2, 1); naissance_init(s, 46);
    memcpy(w->cs[1]->dna, w->cs[0]->dna, MIN_LEN);
    cfg.period = 1; cfg.cap = 3; cfg.mutation = 1;
    world_step(w); naissance_step(w, &cfg, s); cfg.period = 100;
    NCHECK(s->npending == 1 && s->pending[0].crossed == 0);
    world_step(w); naissance_step(w, &cfg, s);
    NCHECK(s->hatched == 1 && s->crossed == 0);
    world_free(w);

    /* A closed season pays nothing; opening it admits the next conception. */
    w = fixture(&r, 5, 5, 1, 1, 2, 1); naissance_init(s, 47);
    cfg.period = 1; cfg.mutation = 0; cfg.season_period = 4; cfg.season_open = 1;
    world_step(w); naissance_step(w, &cfg, s);
    NCHECK(s->eggs == 0 && s->refused_season == 1 && s->energy_paid == 0);
    w->t = 3; world_step(w); naissance_step(w, &cfg, s);
    NCHECK(s->eggs == 1 && s->energy_paid == 20);
    world_free(w);
    free(copy); free(s);
#undef NCHECK
    return failed;
}
