/* Monde torique : nourriture, énergie, voisinage, stimulus externe, prédation. */
#include <stdlib.h>
#include <string.h>
#include "octet.h"

static const int DX[4] = { 0, 1, 0, -1 }, DY[4] = { -1, 0, 1, 0 };
void lab_pre(struct Lab *l, World *w);
void lab_post(struct Lab *l, World *w);

WorldConfig cfg_default(void) {
    WorldConfig c = { .w = 48, .h = 20, .food_density = 0.35, .food_regrow = 4, .max_pop = 120,
                      .split_energy = 70, .start_energy = 160, .mut_rate = 0.02, .dup_rate = 0.05,
                      .crossover = 0, .pred_eat_factor = 0.2, .attack_steal = 30,
                      .attack_cost = 1, .pred_metab = 0, .pred_max_age = 0, .eat_guard = 0 };
    return c;
}

Creature *creature_new(int x, int y, const uint8_t *dna, int len, int e, int id, int species, int parent) {
    Creature *c = calloc(1, sizeof *c);
    c->x = x; c->y = y; c->e = e; c->id = id; c->species = species; c->parent = parent;
    c->len = len; c->germ_len = len; c->emitted = -1;
    if (len > 0) { memcpy(c->dna, dna, (size_t)len); memcpy(c->germ, dna, (size_t)len); }
    return c;
}

static void add(World *w, Creature *c) {
    if (w->nnb >= CMAX) { free(c); return; }
    w->nb[w->nnb++] = c;
    w->occ[c->y][c->x]++;
}
void world_spawn(World *w, Creature *c) { add(w, c); }   /* labos : injecter une créature (pathogène, hôte...) */

World *world_new(const PoolEntry *g, int n, Rng *rng, const WorldConfig *cfg, struct Lab *lab) {
    World *w = calloc(1, sizeof *w);
    w->cfg = cfg ? *cfg : cfg_default();
    if (w->cfg.w > GRID_W) w->cfg.w = GRID_W;
    if (w->cfg.h > GRID_H) w->cfg.h = GRID_H;
    w->rng = rng; w->lab = lab;
    for (int y = 0; y < w->cfg.h; y++)
        for (int x = 0; x < w->cfg.w; x++)
            w->food[y][x] = rng_double(rng) < w->cfg.food_density ? (uint8_t)rng_int(rng, 4, 14) : 0;
    for (int i = 0; i < n; i++) {
        int x = (int)rng_below(rng, (uint32_t)w->cfg.w), y = (int)rng_below(rng, (uint32_t)w->cfg.h);
        add(w, creature_new(x, y, g[i].dna, g[i].len, w->cfg.start_energy, w->nid++, g[i].species, -1));
    }
    memcpy(w->cs, w->nb, sizeof(Creature *) * (size_t)w->nnb);
    w->ncs = w->nnb; w->nnb = 0;
    w->st.born = w->ncs;
    return w;
}

void world_free(World *w) {
    if (!w) return;
    for (int i = 0; i < w->ncs; i++) free(w->cs[i]);
    for (int i = 0; i < w->nnb; i++) free(w->nb[i]);
    free(w);
}

static void ahead(const World *w, const Creature *c, int *x, int *y) {
    *x = (c->x + DX[c->d & 3] + w->cfg.w) % w->cfg.w;
    *y = (c->y + DY[c->d & 3] + w->cfg.h) % w->cfg.h;
}
int food_ahead(const World *w, const Creature *c) { int x, y; ahead(w, c, &x, &y); return w->food[y][x]; }
int neighbors(const World *w, const Creature *c) {
    int n = 0, W = w->cfg.w, H = w->cfg.h;
    for (int dy = -1; dy <= 1; dy++)
        for (int dx = -1; dx <= 1; dx++)
            if (dx || dy) n += w->occ[(c->y + dy + H) % H][(c->x + dx + W) % W];
    return n;
}
int history_at(const World *w, int k) {
    if (k >= w->hist_n) return 0;
    return w->hist[(w->hist_pos - 1 - k + 2 * HIST_N) % HIST_N];
}

void world_move(World *w, Creature *c) {
    w->occ[c->y][c->x]--;
    ahead(w, c, &c->x, &c->y);
    w->occ[c->y][c->x]++;
    c->e -= 1;
    c->steps++;
}
void world_eat(World *w, Creature *c) {
    int before = w->food[c->y][c->x];
    int take = before;
    if (take > 6) take = 6;
    if (!take) return;
    w->food[c->y][c->x] -= (uint8_t)take;
    int gain = c->species == 0 ? take * 5 : (int)(take * 5 * w->cfg.pred_eat_factor);
    c->e = c->e + gain > 220 ? 220 : c->e + gain;
    c->eaten += gain;
    w->st.energy_eaten += gain;
    /* commons seulement : la seed ne mange qu'une case ≥ 8. Un glouton qui vide ou
     * entame une case plus basse marque son génome, la file des parents le voit. */
    if (w->cfg.eat_guard && c->species == 0 && (before < 8 || w->food[c->y][c->x] == 0))
        c->lscore -= 1;
}
static int near(const Creature *a, const Creature *b) { return abs(a->x - b->x) <= 1 && abs(a->y - b->y) <= 1; }
void world_split(World *w, Creature *c) {
    if (c->e < w->cfg.split_energy || w->ncs + w->nnb >= w->cfg.max_pop) return;
    uint8_t tmp[MAX_LEN], child[MAX_LEN + 16];
    EvoParams ep = w->cfg.self_evo ? evo_params(c->dna, c->len) : (EvoParams){ 0, 0, 0, 0 };
    /* general : muter le génome de naissance. Le soma (STORE) ne part pas dans l'enfant.
     * germline à 0 : même source qu'avant, donc les mêmes tirages pour les labos historiques. */
    int n = c->len;
    const uint8_t *src = c->dna;
    if (w->cfg.germline && c->germ_len > 0) { n = c->germ_len; src = c->germ; }
    memcpy(tmp, src, (size_t)n);
    if (w->cfg.self_evo ? (ep.xover) : w->cfg.crossover) {
        for (int i = 0; i < w->ncs; i++) {
            Creature *o = w->cs[i];
            if (o != c && o->species == c->species && o->e > 0 && near(o, c)) {
                const uint8_t *ob = o->dna;
                int ol = o->len;
                if (w->cfg.germline && o->germ_len > 0) { ob = o->germ; ol = o->germ_len; }
                n = crossover(src, n, ob, ol, w->rng, tmp);
                w->st.crossovers++;
                break;
            }
        }
    }
    int m;
    if (w->cfg.self_evo) m = mutate_evo(tmp, n, w->rng, &ep, child);
    else m = mutate(tmp, n, w->rng, w->cfg.mut_rate, w->cfg.dup_rate, child);
    /* general seulement. delay_rate à 0 : aucun tirage, les labos historiques ne bougent pas. */
    if (w->cfg.delay_rate > 0) {
        if (rng_double(w->rng) < w->cfg.delay_rate) stagger_cell(child, m);
        if (rng_double(w->rng) < w->cfg.delay_rate) retarget_prev(child, m);
    }
    if (w->cfg.unmix_rate > 0 && rng_double(w->rng) < w->cfg.unmix_rate) unmix_prev(child, m);   /* general seulement */
    int d = (c->d + 1) & 3, give = c->e / 3;
    int born_before = w->nnb;
    add(w, creature_new((c->x + DX[d] + w->cfg.w) % w->cfg.w, (c->y + DY[d] + w->cfg.h) % w->cfg.h,
                        child, m, give, w->nid++, c->species, c->id));
    if (w->nnb > born_before) c->splits++;
    c->e -= give + 8 + ep.prot / 4;                      /* self-evolve : la protection coûte (0 ailleurs) */
    w->st.born++;
}
void world_attack(World *w, Creature *c) {
    c->e -= w->cfg.attack_cost;
    for (int i = 0; i < w->ncs; i++) {
        Creature *o = w->cs[i];
        if (o->species != c->species && o->e > 0 && near(o, c)) {
            int s = o->e < w->cfg.attack_steal ? o->e : w->cfg.attack_steal;
            o->e -= s;
            c->e = c->e + s > 220 ? 220 : c->e + s;
            w->st.attacks++;
            return;
        }
    }
}
void world_emit(World *w, Creature *c, int v) {
    if (w->nem < CMAX) { w->em[w->nem].c = c; w->em[w->nem].v = v; w->nem++; }
    w->st.emits++;
}

static void bury(World *w, Creature *c, int drop_food) {
    w->st.dead++;
    w->st.death_age_sum += c->age;
    w->occ[c->y][c->x]--;
    if (drop_food) w->food[c->y][c->x] = (uint8_t)(w->food[c->y][c->x] + 4 > 14 ? 14 : w->food[c->y][c->x] + 4);
}

void world_step(World *w) {
    w->t++;
    for (int i = 0; i < w->cfg.food_regrow; i++) {
        int x = (int)rng_below(w->rng, (uint32_t)w->cfg.w), y = (int)rng_below(w->rng, (uint32_t)w->cfg.h);
        int f = w->food[y][x] + rng_int(w->rng, 1, 4);
        w->food[y][x] = (uint8_t)(f > 14 ? 14 : f);
    }
    if (w->lab) lab_pre(w->lab, w);
    w->nem = 0;
    static __thread Creature *live[CMAX], *grave[CMAX];
    int nl = 0, ng = 0;
    for (int i = 0; i < w->ncs; i++) {
        Creature *c = w->cs[i];
        if (w->before_vm) w->before_vm(w, c);
        int ok = vm_run(c, w);
        if (ok && c->species != 0) {                 /* prédateurs : coût de vie + vieillissement */
            c->e -= w->cfg.pred_metab;
            if (w->cfg.pred_max_age > 0 && c->age >= w->cfg.pred_max_age) c->e = 0;
        }
        if (ok && c->e > 0) live[nl++] = c;
        else { bury(w, c, 1); grave[ng++] = c; }
    }
    if (w->lab) lab_post(w->lab, w);
    int na = 0;
    for (int i = 0; i < nl; i++) {                 /* tués après leur tour (ATTACK, pénalités) */
        Creature *c = live[i];
        if (c->e > 0) w->cs[na++] = c;
        else { bury(w, c, 0); grave[ng++] = c; }
    }
    for (int i = 0; i < w->nnb; i++) {
        Creature *c = w->nb[i];
        if (c->e > 0 && na < CMAX) w->cs[na++] = c;
        else { w->occ[c->y][c->x]--; free(c); }
    }
    w->ncs = na; w->nnb = 0;
    for (int i = 0; i < ng; i++) free(grave[i]);
    w->nem = 0;
    w->hist[w->hist_pos] = (uint8_t)w->stimulus;
    w->hist_pos = (w->hist_pos + 1) % HIST_N;
    if (w->hist_n < HIST_N) w->hist_n++;
}

uint64_t world_hash(const World *w) {
    uint64_t h = FNV_INIT;
    for (int i = 0; i < w->ncs; i++) {
        const Creature *c = w->cs[i];
        uint8_t b[9] = { (uint8_t)c->x, (uint8_t)c->y, (uint8_t)(c->d & 3), c->a, c->b,
                         (uint8_t)c->e, (uint8_t)(c->e >> 8), (uint8_t)(c->e >> 16), (uint8_t)(c->e >> 24) };
        h = fnv1a(b, sizeof b, h);
        h = fnv1a(c->dna, (size_t)c->len, h);
    }
    for (int y = 0; y < w->cfg.h; y++) h = fnv1a(w->food[y], (size_t)w->cfg.w, h);
    return h;
}
