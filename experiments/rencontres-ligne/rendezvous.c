#include "rendezvous.h"

#include <stdlib.h>
#include <string.h>

enum { RENDEZVOUS_RADIUS = 4, RENDEZVOUS_TTL = 24 };

/* Le pas standard maintient occupation, compteur et coût de base ensemble. */
void world_move(World *w, Creature *c);

void rendezvous_init(RendezvousState *s, uint64_t seed) {
    memset(s, 0, sizeof *s);
    s->seed = seed;
    s->last_fields_tick = -1;
}

static int static_capable(const World *w, const Creature *c) {
    int sensor = 0, step = 0;
    if (c->len <= 0 || c->len > MAX_LEN) return 0;
    for (int i = 0; i < c->len;) {
        uint8_t op = c->dna[i++];
        if (op == OP_SENSE || op == OP_LOAD || op == OP_SENSE_NB ||
            op == OP_SENSE_EN || op == OP_SENSE_STIM || op == OP_SENSE_HIST)
            sensor = 1;
        if (op == OP_STEP) step = 1;
        if (op_has_imm(op) || (w->cfg.regs &&
            (op == OPX_RPUSH || op == OPX_RPEEK))) i++;
    }
    return sensor && step;
}

static __thread int witness_sensor, witness_step;
static void witness_watch(Creature *c, uint8_t op) {
    (void)c;
    if (op == OP_SENSE || op == OP_LOAD || op == OP_SENSE_NB ||
        op == OP_SENSE_EN || op == OP_SENSE_STIM || op == OP_SENSE_HIST)
        witness_sensor = 1;
    if (op == OP_STEP) witness_step = 1;
}

int rendezvous_capable(const World *w, const Creature *c, int behavioral) {
    if (!behavioral) return static_capable(w, c);
    if (c->len <= 0 || c->len > MAX_LEN || c->e <= 0 ||
        w->ncs < 0 || w->ncs > CMAX || w->nnb < 0 || w->nnb > CMAX) return 0;
    /* Pas de préfiltre statique : STORE peut créer l'opcode exécuté ensuite.
     * Tous les pointeurs accessibles par ATTACK/crossover sont privés. */
    World *p = xmalloc(sizeof *p); *p = *w;
    int n = w->ncs + w->nnb;
    Creature *copies = xmalloc((size_t)(n + 1) * sizeof *copies), *subject = NULL;
    for (int i = 0; i < w->ncs; i++) {
        copies[i] = *w->cs[i]; p->cs[i] = &copies[i];
        if (w->cs[i] == c) subject = p->cs[i];
    }
    for (int i = 0; i < w->nnb; i++) {
        copies[w->ncs + i] = *w->nb[i]; p->nb[i] = &copies[w->ncs + i];
        if (w->nb[i] == c) subject = p->nb[i];
    }
    if (!subject) { copies[n] = *c; subject = &copies[n]; }
    Rng r = w->rng ? *w->rng : (Rng){{1,2,3,4}};
    p->rng = &r; p->lab = NULL; p->before_vm = NULL;
    /* Les anciennes émissions ne sont jamais lues par vm_run ; aucune adresse
     * du monde n'est laissée dans cette file privée. */
    memset(p->em, 0, sizeof p->em); p->nem = 0;
    WorldThreadState thread = world_thread_state();
    VmWatch watch = vm_get_watch();
    witness_sensor = witness_step = 0;
    vm_set_watch(witness_watch);
    vm_run(subject, p);
    int result = witness_sensor && witness_step;
    vm_set_watch(watch);
    world_thread_restore(thread);
    /* SPLIT a pu allouer de vrais enfants dans la copie. */
    for (int i = w->nnb; i < p->nnb; i++) free(p->nb[i]);
    free(copies); free(p);
    return result;
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
    if (!rendezvous_capable(w, c, cfg->enabled == 2)) return;

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
    /* Présence inatteignable, puis opcode créé par STORE : le tick prime sur
     * la simple grammaire. L'ip courant et YIELD sont effectivement respectés. */
    memset(c.dna, 0xff, sizeof c.dna); c.len = 8; c.ip = 0; c.e = 200;
    c.dna[0] = OP_YIELD; c.dna[1] = OP_SENSE; c.dna[2] = OP_STEP;
    CHECK(rendezvous_capable(w, &c, 0) && !rendezvous_capable(w, &c, 1));
    uint8_t created[] = { OP_LOADI, OP_STEP, OP_STORE, 6, OP_SENSE, OP_NOP, OP_NOP, OP_YIELD };
    memcpy(c.dna, created, sizeof created);
    CHECK(!rendezvous_capable(w, &c, 0) && rendezvous_capable(w, &c, 1));

    /* ATTACK, EAT, STORE, EMIT et SPLIT frappent seulement les copies ; même
     * le cumul TLS de mutation et l'observateur déjà installé sont restaurés. */
    w->cfg = cfg_default(); w->cfg.w = GRID_W; w->cfg.h = GRID_H;
    c.x = 3; c.y = 3; c.d = 1; c.len = 16; c.ip = 0; c.e = 200; c.species = 0;
    uint8_t hostile[] = { OP_SENSE, OP_STEP, OP_EAT, OP_LOADI, 49, OP_STORE, 14,
                          OP_ATTACK, OP_SPLIT, OP_EMIT, OP_YIELD };
    memset(c.dna, 0xff, sizeof c.dna); memcpy(c.dna, hostile, sizeof hostile);
    Creature victim = c; victim.id = 1; victim.species = 1; victim.x = 5;
    Creature pending = victim; pending.id = 2;
    w->cs[0] = &c; w->cs[1] = &victim; w->ncs = 2;
    w->nb[0] = &pending; w->nnb = 1; w->rng = &rng;
    memset(w->occ, 0, sizeof w->occ); w->occ[3][3] = 1; w->occ[3][5] = 2;
    memset(w->food, 14, sizeof w->food);
    *before = *w; original = c;
    Creature vcopy = victim, pcopy = pending; rng_before = rng;
    WorldThreadState old_thread = world_thread_state();
    WorldThreadState requested = { 1.75, 123.5, 9 };
    world_thread_restore(requested);
    VmWatch old_watch = vm_get_watch(); vm_set_watch(witness_watch);
    CHECK(rendezvous_capable(w, &c, 1));
    WorldThreadState after_thread = world_thread_state();
    CHECK(after_thread.scale == requested.scale && after_thread.sum == requested.sum && after_thread.n == requested.n);
    CHECK(vm_get_watch() == witness_watch);
    CHECK(!memcmp(before, w, sizeof *w) && !memcmp(&c, &original, sizeof c));
    CHECK(!memcmp(&victim, &vcopy, sizeof victim) && !memcmp(&pending, &pcopy, sizeof pending));
    CHECK(!memcmp(&rng, &rng_before, sizeof rng));
    vm_set_watch(old_watch); world_thread_restore(old_thread);
    free(before); free(w);
#undef CHECK
    return failures;
}
