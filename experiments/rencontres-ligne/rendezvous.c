#include "rendezvous.h"

#include <stdlib.h>
#include <string.h>

enum { RENDEZVOUS_RADIUS = 4, RENDEZVOUS_TTL = 24 };

/* Même interprète, symboles privés : sa sonde ne remplace jamais le watcher
 * d'attention/traits posé autour du vrai tick. Aucun opcode ni src/ modifié. */
#define vm_run rendezvous_probe_run
#define vm_set_watch rendezvous_probe_watch
#define cfg_default rendezvous_probe_cfg_default
#define creature_new rendezvous_probe_creature_new
#define world_set_mut_scale rendezvous_probe_set_mut_scale
#define world_take_birth_mut rendezvous_probe_take_birth_mut
#define world_spawn rendezvous_probe_spawn
#define world_new rendezvous_probe_new
#define world_free rendezvous_probe_free
#define food_ahead rendezvous_probe_food_ahead
#define neighbors rendezvous_probe_neighbors
#define history_at rendezvous_probe_history_at
#define world_move rendezvous_probe_move
#define world_eat rendezvous_probe_eat
#define world_split rendezvous_probe_split
#define world_attack rendezvous_probe_attack
#define world_emit rendezvous_probe_emit
#define world_step rendezvous_probe_step
#define world_hash rendezvous_probe_hash
int rendezvous_probe_run(Creature *c, World *w);
#include "../../src/world.c"
#include "../../src/vm.c"
#undef vm_run
#undef vm_set_watch
#undef cfg_default
#undef creature_new
#undef world_set_mut_scale
#undef world_take_birth_mut
#undef world_spawn
#undef world_new
#undef world_free
#undef food_ahead
#undef neighbors
#undef history_at
#undef world_move
#undef world_eat
#undef world_split
#undef world_attack
#undef world_emit
#undef world_step
#undef world_hash

static __thread int probe_sensor, probe_step;
static void probe_watch(Creature *c, uint8_t op) {
    (void)c;
    if (op == OP_SENSE || op == OP_LOAD || op == OP_SENSE_NB ||
        op == OP_SENSE_EN || op == OP_SENSE_STIM || op == OP_SENSE_HIST ||
        op == OP_NEURAL) probe_sensor = 1;
    if (op == OP_STEP && probe_sensor) probe_step = 1;
}

/* Le pas standard maintient occupation, compteur et coût de base ensemble. */
void world_move(World *w, Creature *c);

void rendezvous_init(RendezvousState *s, uint64_t seed) {
    memset(s, 0, sizeof *s);
    s->seed = seed;
    s->last_fields_tick = -1;
}

int rendezvous_capable(const World *w, const Creature *c) {
    if (c->len <= 0 || c->len > MAX_LEN) return 0;
    if (!w->rng || w->ncs < 0 || w->ncs > CMAX || w->nnb < 0 ||
        w->nnb > CMAX || w->cfg.w <= 0 || w->cfg.h <= 0) return 0;
    /* Recopier chaque état à chaque appel. ATTACK, STORE, SPLIT et EMIT ne
     * touchent ainsi aucun vivant,
     * pointeur de créature, compteur, nourriture ni RNG du monde original. */
    World *sandbox = malloc(sizeof *sandbox +
        (size_t)(w->ncs + w->nnb) * sizeof(Creature));
    if (!sandbox) return 0;
    Creature *residents = (Creature *)(sandbox + 1), *newborns = residents + w->ncs;
    *sandbox = *w;
    Rng rng = *w->rng;
    sandbox->rng = &rng;
    sandbox->lab = NULL;
    sandbox->before_vm = NULL;
    for (int i = 0; i < w->ncs; i++) {
        residents[i] = *w->cs[i]; sandbox->cs[i] = &residents[i];
    }
    for (int i = 0; i < w->nnb; i++) {
        newborns[i] = *w->nb[i]; sandbox->nb[i] = &newborns[i];
    }
    Creature candidate = *c;
    /* La vraie identité de c doit aussi être celle vue par ATTACK/crossover. */
    Creature *probe = &candidate;
    for (int i = 0; i < w->ncs; i++)
        if (w->cs[i] == c) { probe = sandbox->cs[i]; break; }
    probe_sensor = probe_step = 0;
    rendezvous_probe_watch(probe_watch);
    int existing = sandbox->nnb;
    /* world_split et son TLS sont eux aussi privés ; aucun compteur de
     * mutation du vrai cœur ne reçoit les naissances du tick témoin. */
    rendezvous_probe_run(probe, sandbox);
    rendezvous_probe_watch(NULL);
    for (int i = existing; i < sandbox->nnb; i++) free(sandbox->nb[i]);
    free(sandbox);
    return probe_step;
}

static unsigned mark_at(const RendezvousState *s,
                        const uint8_t marks[GRID_H][GRID_W], int x, int y) {
    unsigned v = marks ? marks[y][x] & 15u : 0;
    return v ? v : s->marks[y][x] & 15u;
}

static int wrap(int x, int width) {
    int r = x % width;
    return r < 0 ? r + width : r;
}

void rendezvous_before(World *w, Creature *c, const RendezvousConfig *cfg,
                       RendezvousState *s,
                       const uint8_t marks[GRID_H][GRID_W]) {
    if (!cfg->enabled || c->e <= 0) return;
    int W = w->cfg.w, H = w->cfg.h;
    if (W <= 0 || H <= 0 || W > GRID_W || H > GRID_H) return;
    int period = cfg->period > 0 ? cfg->period : 8;
    uint64_t phase = (uint64_t)w->t + (uint64_t)(unsigned)c->id + s->seed;
    if (phase % (unsigned)period) return;

    unsigned value = mark_at(s, marks, c->x, c->y);
    int tx = 0, ty = 0, distance = RENDEZVOUS_RADIUS + 1;
    if (value) distance = 0;
    /* Rotation déterministe du balayage pour ne pas privilégier constamment
     * le nord-ouest lors des égalités ; aucun tirage d'aléa supplémentaire. */
    unsigned turn = (unsigned)((phase / (unsigned)period) & 3u);
    for (int radius = 1; distance > RENDEZVOUS_RADIUS &&
                         radius <= RENDEZVOUS_RADIUS; radius++) {
        for (int dy = -radius; dy <= radius; dy++) {
            for (int dx = -radius; dx <= radius; dx++) {
                if (abs(dx) + abs(dy) != radius) continue;
                int rx = dx, ry = dy;
                for (unsigned r = 0; r < turn; r++) {
                    int save = rx; rx = -ry; ry = save;
                }
                unsigned v = mark_at(s, marks, wrap(c->x + rx, W),
                                      wrap(c->y + ry, H));
                if (!v) continue;
                value = v; tx = rx; ty = ry; distance = radius;
                break;
            }
            if (distance == radius) break;
        }
    }
    if (distance > RENDEZVOUS_RADIUS) return;
    if (!rendezvous_capable(w, c)) return;

    s->reads++;
    c->a = (uint8_t)((c->a & 15u) | (value << 4));
    if (!distance) return;
    int cost = cfg->cost > 0 ? cfg->cost : 1;
    if (c->e <= cost) { s->blocked++; return; }

    /* Un seul pas vers la cible, même si elle est à quatre cases. Le monde
     * autorise le partage d'une case : aucune collision inventée ici. */
    if (tx && (!ty || (phase & 1u))) c->d = tx > 0 ? 1 : 3;
    else c->d = ty > 0 ? 2 : 0;
    world_move(w, c);
    c->e -= cost - 1;
    s->moves++;
    s->paid += (uint64_t)cost;
}

void rendezvous_fields(World *w, const RendezvousConfig *cfg,
                       RendezvousState *s,
                       uint8_t out_marks[GRID_H][GRID_W]) {
    if (!cfg->enabled) return;
    int W = w->cfg.w, H = w->cfg.h;
    if (W <= 0 || H <= 0 || W > GRID_W || H > GRID_H) return;
    if (s->last_fields_tick != w->t) {
        int64_t elapsed = s->last_fields_tick < 0 ? 1 : w->t - s->last_fields_tick;
        if (elapsed < 1) elapsed = 1;
        for (int y = 0; y < H; y++) {
            for (int x = 0; x < W; x++) {
                if (!s->ttl[y][x]) continue;
                if (elapsed >= s->ttl[y][x]) {
                    s->ttl[y][x] = 0; s->marks[y][x] = 0; s->expired++;
                } else s->ttl[y][x] -= (uint8_t)elapsed;
            }
        }
        for (int i = 0; i < w->ncs; i++) {
            const Creature *c = w->cs[i];
            if (c->e <= 0 || c->emitted < 0) continue;
            unsigned value = (unsigned)c->emitted & 15u;
            if (!value) continue;
            s->marks[c->y][c->x] = (uint8_t)value;
            s->ttl[c->y][c->x] = RENDEZVOUS_TTL;
            s->deposited++;
        }
        s->last_fields_tick = w->t;
    }
    if (out_marks) memcpy(out_marks, s->marks, sizeof s->marks);
}

int rendezvous_selftest(void) {
    int failures = 0;
#define CHECK(c) do { if (!(c)) failures++; } while (0)
    World *w = calloc(1, sizeof *w), *before = calloc(1, sizeof *before);
    if (!w || !before) { free(w); free(before); return 1; }
    Rng rng = {{ 1, 2, 3, 4 }}, rng_before = rng;
    w->rng = &rng; w->cfg.w = GRID_W; w->cfg.h = GRID_H;
    RendezvousState s, saved;
    RendezvousConfig cfg = { 0, 1, 3 };
    uint8_t marks[GRID_H][GRID_W] = {{0}};
    Creature c = { .x = GRID_W - 1, .y = 5, .e = 100, .len = 8, .emitted = -1 };
    c.dna[0] = OP_SENSE_STIM; c.dna[1] = OP_STEP; c.dna[2] = OP_YIELD;
    w->occ[c.y][c.x] = 1; w->cs[0] = &c; w->ncs = 1;
    marks[5][2] = 2;
    rendezvous_init(&s, 17);
    saved = s; *before = *w; Creature original = c;
    rendezvous_before(w, &c, &cfg, &s, marks);
    rendezvous_fields(w, &cfg, &s, marks);
    CHECK(!memcmp(&saved, &s, sizeof s) && !memcmp(before, w, sizeof *w));
    CHECK(!memcmp(&original, &c, sizeof c));

    cfg.enabled = 1;
    rendezvous_before(w, &c, &cfg, &s, marks);
    CHECK(c.x == 0 && c.y == 5 && c.d == 1 && c.steps == 1 && c.e == 97);
    CHECK(w->occ[5][GRID_W - 1] == 0 && w->occ[5][0] == 1);
    CHECK(s.reads == 1 && s.moves == 1 && s.paid == 3 && (c.a >> 4) == 2);
    CHECK(!memcmp(&rng, &rng_before, sizeof rng));

    /* Même état initial => même action, sans la moindre lecture de RNG. */
    RendezvousState replay;
    rendezvous_init(&replay, 17);
    Creature again = original;
    rendezvous_before(before, &again, &cfg, &replay, marks);
    CHECK(!memcmp(&again, &c, sizeof c) && !memcmp(&replay, &s, sizeof s));

    memset(marks, 0, sizeof marks);
    original = c; saved = s;
    rendezvous_before(w, &c, &cfg, &s, marks);
    CHECK(!memcmp(&original, &c, sizeof c) && !memcmp(&saved, &s, sizeof s));
    marks[5][5] = 7; /* cinq cases : hors de portée depuis x=0 */
    rendezvous_before(w, &c, &cfg, &s, marks);
    CHECK(!memcmp(&original, &c, sizeof c) && !memcmp(&saved, &s, sizeof s));

    memset(marks, 0, sizeof marks); marks[5][2] = 2;
    c.dna[0] = OP_LOADI; c.dna[1] = OP_SENSE; c.dna[2] = OP_STEP;
    c.dna[3] = OP_YIELD; original = c; saved = s;
    rendezvous_before(w, &c, &cfg, &s, marks);
    CHECK(!memcmp(&original, &c, sizeof c) && !memcmp(&saved, &s, sizeof s));
    c.dna[0] = OP_SENSE; c.dna[1] = OP_LOADI; c.dna[2] = OP_STEP;
    c.dna[3] = OP_YIELD; original = c;
    rendezvous_before(w, &c, &cfg, &s, marks);
    CHECK(!memcmp(&original, &c, sizeof c)); /* STEP immédiat, pas instruction */

    c.dna[0] = OP_LOAD; c.dna[1] = OP_STEP; c.dna[2] = OP_STEP;
    c.dna[3] = OP_YIELD; c.e = 3; original = c;
    rendezvous_before(w, &c, &cfg, &s, marks);
    CHECK(c.x == original.x && c.y == original.y && c.e == 3 && s.blocked == 1);
    c.e = 30; marks[c.y][c.x] = 4; original = c;
    rendezvous_before(w, &c, &cfg, &s, marks);
    CHECK(c.x == original.x && c.y == original.y && c.e == original.e);
    CHECK((c.a >> 4) == 4); /* arrivé : lire et rester */

    c.emitted = 7;
    rendezvous_fields(w, &cfg, &s, NULL);
    CHECK(s.marks[c.y][c.x] == 7 && s.ttl[c.y][c.x] == 24 && s.deposited == 1);
    rendezvous_fields(w, &cfg, &s, NULL);
    CHECK(s.deposited == 1); /* pas de double dépôt dans le même tick */
    c.emitted = -1; w->t = 24;
    rendezvous_fields(w, &cfg, &s, NULL);
    CHECK(!s.marks[c.y][c.x] && !s.ttl[c.y][c.x] && s.expired == 1);
    CHECK(!memcmp(&rng, &rng_before, sizeof rng));

    /* Un capteur et STEP dans une queue morte ne donnent plus un passe-droit.
     * Les sauts, l'ip courant et l'ordre effectivement exécuté comptent. */
    c.e = 100; c.ip = 0; memset(c.dna, OP_NOP, (size_t)c.len);
    c.dna[0] = OP_YIELD; c.dna[1] = OP_SENSE; c.dna[2] = OP_STEP;
    CHECK(!rendezvous_capable(w, &c));
    c.dna[0] = OP_STEP; c.dna[1] = OP_SENSE; c.dna[2] = OP_YIELD;
    CHECK(!rendezvous_capable(w, &c));
    c.dna[0] = OP_SENSE; c.dna[1] = OP_STEP; c.dna[2] = OP_YIELD;
    CHECK(rendezvous_capable(w, &c));
    c.ip = 2; CHECK(!rendezvous_capable(w, &c)); c.ip = 0;
    /* STORE, EAT, SPLIT, ATTACK et EMIT s'exécutent seulement sur les copies.
     * Le voisin attaquable appartient lui aussi à la copie profonde. */
    Creature neighbor = c;
    neighbor.id = c.id + 1; neighbor.species = c.species + 1;
    w->cs[1] = &neighbor; w->ncs = 2; w->cfg.max_pop = 10;
    w->cfg.split_energy = 10; w->cfg.attack_steal = 5;
    w->cfg.attack_cost = 1; w->cfg.pred_eat_factor = 0.2;
    c.len = 16; c.e = 200; c.a = 7;
    uint8_t effects[] = { OP_STORE, 15, OP_EAT, OP_SPLIT, OP_ATTACK,
        OP_EMIT, OP_SENSE, OP_STEP, OP_YIELD };
    memset(c.dna, OP_NOP, (size_t)c.len); memcpy(c.dna, effects, sizeof effects);
    w->food[c.y][c.x] = 14;
    *before = *w; original = c; Creature original_neighbor = neighbor;
    CHECK(rendezvous_capable(w, &c));
    CHECK(!memcmp(before, w, sizeof *w) && !memcmp(&original, &c, sizeof c));
    CHECK(!memcmp(&original_neighbor, &neighbor, sizeof neighbor));
    CHECK(!memcmp(&rng, &rng_before, sizeof rng));
    free(before); free(w);
#undef CHECK
    return failures;
}
