/* Symbiose métabolique, protocole fixé dans symbiose-metabolique/README.md.
 * La VM native reste intacte. Cette éprouvette remplace seulement les effets
 * du milieu de deux actions : EAT n'a pas de ressource, EMIT demande une
 * conversion chimique unitaire. Aucun nouveau codon ni nouvelle division.
 */
#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "octet.h"

/* Réutiliser le monde natif sans écrire dans src/ ni produire d'objet partagé
 * avec les mondes vivants. Les appels de vm.c arrivent dans les deux wrappers
 * ci-dessous ; les définitions natives renommées restent disponibles.
 */
#define world_emit symbiose_native_emit
#define world_eat symbiose_native_eat
#include "../src/world.c"
#undef world_emit
#undef world_eat

enum { CHEM_REAL, CHEM_SINK, CHEM_SILENT };
enum { FOUNDERS = 32, DNA_LEN = 64, HORIZON = 2048, ASSAY = 512,
       NS = 32, NC = 8, STOCK = 64, GAIN = 4 };

typedef struct {
    World *w;
    Rng rng;
    int mode, suppress, stock[2], sink;
    uint64_t reactions[2], births[2], supplied[2], received[2];
} Chem;

typedef struct {
    int pop[2], candidate, causal[2], pass;
    double intact[2], alone[2];
    uint64_t hash;
} Result;

static __thread Chem *current;

static int enzyme(const Creature *c) {
    return c->germ_len > 0 ? c->germ[c->germ_len - 1] & 1 : 0;
}

void world_eat(World *w, Creature *c) {
    if (!current) symbiose_native_eat(w, c);
}

void world_emit(World *w, Creature *c, int v) {
    symbiose_native_emit(w, c, v);
    Chem *q = current;
    if (!q || q->w != w || q->mode == CHEM_SILENT) return;
    int e = enzyme(c);
    if (q->stock[e] == 0) return;
    q->stock[e]--;
    if (q->mode == CHEM_SINK) q->sink++;
    else q->stock[1 - e]++;
    int before = c->e;
    c->e = c->e + GAIN > 220 ? 220 : c->e + GAIN;
    q->reactions[e]++;
    q->supplied[e] += GAIN;
    q->received[e] += (uint64_t)(c->e - before);
    assert(q->stock[0] >= 0 && q->stock[1] >= 0 && q->sink >= 0);
    assert(q->stock[0] + q->stock[1] + q->sink == STOCK);
}

static void before_vm(World *w, Creature *c) {
    assert(current && current->w == w);
    int n = current->stock[enzyme(c)];
    w->stimulus = n > 255 ? 255 : n;
}

static void remove_suppressed(Chem *q) {
    if (q->suppress < 0) return;
    World *w = q->w;
    int n = 0;
    for (int i = 0; i < w->ncs; i++) {
        Creature *c = w->cs[i];
        if (enzyme(c) == q->suppress) {
            w->occ[c->y][c->x]--;
            free(c);
        } else w->cs[n++] = c;
    }
    w->ncs = n;
}

static void chem_step(Chem *q) {
    current = q;
    remove_suppressed(q);
    memset(q->w->food, 0, sizeof q->w->food);
    int first_child = q->w->nid;
    world_step(q->w);
    for (int i = 0; i < q->w->ncs; i++) {
        Creature *c = q->w->cs[i];
        if (c->id >= first_child) q->births[enzyme(c)]++;
    }
    remove_suppressed(q);
    assert(q->stock[0] + q->stock[1] + q->sink == STOCK);
    assert(q->w->ncs <= 64);
    current = NULL;
}

/* kind 0 : bruit. 1 : contrôle positif prédéfini. 2 : cristal. */
static Chem *chem_new(int seed, int mode, int kind) {
    Chem *q = xcalloc(1, sizeof *q);
    q->mode = mode;
    q->suppress = -1;
    q->stock[0] = q->stock[1] = STOCK / 2;
    Rng genes;
    rng_seed(&genes, 0x0996a2cdB000ULL + (uint64_t)seed);
    PoolEntry p[FOUNDERS];
    memset(p, 0, sizeof p);
    for (int i = 0; i < FOUNDERS; i++) {
        p[i].len = DNA_LEN;
        for (int k = 0; k < DNA_LEN; k++)
            p[i].dna[k] = kind == 0 ? (uint8_t)rng_next(&genes) : kind == 1 ? 0xff : 0x11;
        if (kind == 1) {
            p[i].dna[0] = OP_EMIT;
            p[i].dna[1] = OP_SPLIT;
            p[i].dna[2] = OP_YIELD;
            p[i].dna[3] = OP_LOOP;
        }
        p[i].dna[DNA_LEN - 1] = (uint8_t)((p[i].dna[DNA_LEN - 1] & 0xfe) | (i & 1));
    }
    rng_seed(&q->rng, 0x0996a2cdB100ULL + (uint64_t)seed);
    WorldConfig cfg = cfg_default();
    cfg.w = 16; cfg.h = 8; cfg.max_pop = 64;
    cfg.start_energy = 48; cfg.split_energy = 70;
    cfg.food_density = 0; cfg.food_regrow = 0;
    cfg.mut_rate = 0.005; cfg.dup_rate = 0.01;
    cfg.crossover = 0;
    q->w = world_new(p, FOUNDERS, &q->rng, &cfg, NULL);
    q->w->before_vm = before_vm;
    return q;
}

static Chem *chem_clone(const Chem *q) {
    Chem *c = xcalloc(1, sizeof *c);
    *c = *q;
    c->w = xcalloc(1, sizeof *c->w);
    *c->w = *q->w;
    c->w->rng = &c->rng;
    assert(q->w->nnb == 0 && q->w->nem == 0);
    for (int i = 0; i < c->w->ncs; i++) {
        c->w->cs[i] = xcalloc(1, sizeof(Creature));
        *c->w->cs[i] = *q->w->cs[i];
    }
    return c;
}

static void chem_free(Chem *q) {
    if (!q) return;
    assert(current != q);
    world_free(q->w);
    free(q);
}

static uint64_t chem_hash(const Chem *q) {
    uint64_t h = world_hash(q->w);
    h = fnv1a(&q->mode, sizeof q->mode, h);
    h = fnv1a(&q->suppress, sizeof q->suppress, h);
    h = fnv1a(&q->w->t, sizeof q->w->t, h);
    h = fnv1a(&q->w->nid, sizeof q->w->nid, h);
    h = fnv1a(&q->w->st, sizeof q->w->st, h);
    h = fnv1a(&q->w->ncs, sizeof q->w->ncs, h);
    h = fnv1a(&q->w->hist_n, sizeof q->w->hist_n, h);
    h = fnv1a(&q->w->hist_pos, sizeof q->w->hist_pos, h);
    h = fnv1a(q->w->hist, sizeof q->w->hist, h);
    h = fnv1a(q->w->occ, sizeof q->w->occ, h);
    h = fnv1a(&q->rng, sizeof q->rng, h);
    h = fnv1a(q->stock, sizeof q->stock, h);
    h = fnv1a(&q->sink, sizeof q->sink, h);
    h = fnv1a(q->births, sizeof q->births, h);
    h = fnv1a(q->reactions, sizeof q->reactions, h);
    h = fnv1a(q->supplied, sizeof q->supplied, h);
    h = fnv1a(q->received, sizeof q->received, h);
    for (int i = 0; i < q->w->ncs; i++) {
        const Creature *c = q->w->cs[i];
        /* Creature ne contient pas de pointeur et naît de xcalloc : inclure
         * aussi les registres, dirty, ascendance et état de naissance. */
        h = fnv1a(c, sizeof *c, h);
    }
    return h;
}

static void biomass(const Chem *q, double out[2]) {
    out[0] = out[1] = 0;
    for (int i = 0; i < q->w->ncs; i++) {
        const Creature *c = q->w->cs[i];
        if (c->e > 0) out[enzyme(c)] += c->e;
    }
}

static Result inspect(const Chem *q) {
    Result r = {0};
    r.hash = chem_hash(q);
    for (int i = 0; i < q->w->ncs; i++) r.pop[enzyme(q->w->cs[i])]++;
    r.candidate = r.pop[0] >= 8 && r.pop[1] >= 8 &&
        q->births[0] >= 16 && q->births[1] >= 16 &&
        q->reactions[0] >= 128 && q->reactions[1] >= 128;
    if (!r.candidate) return r;
    Chem *copies[3];
    for (int j = 0; j < 3; j++) {
        copies[j] = chem_clone(q);
        copies[j]->w->cfg.mut_rate = 0;
        copies[j]->w->cfg.dup_rate = 0;
        copies[j]->suppress = j - 1; /* intact, sans 0, sans 1 */
    }
    double sums[3][2] = {{0}};
    for (int t = 0; t < ASSAY; t++) for (int j = 0; j < 3; j++) {
        chem_step(copies[j]);
        if (t >= ASSAY / 2) {
            double b[2];
            biomass(copies[j], b);
            sums[j][0] += b[0]; sums[j][1] += b[1];
        }
    }
    for (int e = 0; e < 2; e++) {
        r.intact[e] = sums[0][e] / (ASSAY / 2);
        r.alone[e] = sums[2 - e][e] / (ASSAY / 2);
        r.causal[e] = r.intact[e] > 0 && r.alone[e] < 0.5 * r.intact[e];
    }
    r.pass = r.causal[0] && r.causal[1];
    for (int j = 0; j < 3; j++) chem_free(copies[j]);
    assert(chem_hash(q) == r.hash); /* la sonde ne touche pas le parent */
    return r;
}

static void advance(Chem *q, int n) {
    for (int t = 0; t < n; t++) chem_step(q);
}

static int selftest(void) {
    Chem *q = chem_new(0, CHEM_REAL, 1);
    Creature *c = q->w->cs[0];
    int before = c->e;
    current = q;
    world_emit(q->w, c, 3);
    current = NULL;
    assert(q->stock[0] == 31 && q->stock[1] == 33 && c->e == before + GAIN);
    assert(q->reactions[0] == 1 && q->supplied[0] == GAIN);
    q->w->nem = 0;
    q->w->food[c->y][c->x] = 14;
    current = q;
    before = c->e;
    world_eat(q->w, c);
    current = NULL;
    assert(c->e == before && q->w->food[c->y][c->x] == 14);
    puts("réaction unitaire, énergie extérieure, EAT sans ressource : OK");
    chem_free(q);

    q = chem_new(1, CHEM_SINK, 1);
    advance(q, HORIZON);
    assert(q->stock[0] + q->stock[1] + q->sink == STOCK && q->sink == STOCK);
    assert(q->w->ncs == 0 && !inspect(q).pass);
    puts("puits conservatif et épuisement sans recyclage : OK");
    chem_free(q);

    q = chem_new(0, CHEM_REAL, 1);
    Chem *d = chem_new(0, CHEM_REAL, 1);
    advance(q, HORIZON); advance(d, HORIZON);
    assert(chem_hash(q) == chem_hash(d));
    Result a = inspect(q), b = inspect(d);
    assert(a.pass && b.pass && a.hash == b.hash);
    assert(a.alone[0] == 0 && a.alone[1] == 0);
    assert(a.intact[0] > 0 && a.intact[1] > 0);
    assert(chem_hash(q) == chem_hash(d));
    puts("positif reproducteur, dépendance réciproque, clones isolés, déterminisme : OK");
    chem_free(q); chem_free(d);

    q = chem_new(2, CHEM_SILENT, 1);
    advance(q, HORIZON);
    assert(q->w->ncs == 0 && q->reactions[0] == 0 && q->reactions[1] == 0);
    chem_free(q);
    q = chem_new(2, CHEM_REAL, 2);
    advance(q, HORIZON);
    assert(q->w->ncs == 0 && q->reactions[0] == 0 && q->reactions[1] == 0);
    chem_free(q);
    puts("EMIT neutralisé et cristal sans gain : OK");
    puts("selftest symbiose : OK");
    return 0;
}

static int run(void) {
    const char *names[5] = {"R", "P", "S", "cristal", "positif"};
    int passed[5] = {0}, candidates[5] = {0}, both[5] = {0};
    uint64_t births[5][2] = {{0}}, reactions[5][2] = {{0}}, supplied[5][2] = {{0}};
    puts("bras graine pop0 pop1 naiss0 naiss1 reactions0 reactions1 A B puits candidat causal0 causal1 intact0 intact1 seul0 seul1 passe hash");
    for (int arm = 0; arm < 5; arm++) {
        int n = arm < 3 ? NS : NC;
        for (int s = 0; s < n; s++) {
            int mode = arm == 1 ? CHEM_SINK : arm == 2 ? CHEM_SILENT : CHEM_REAL;
            int kind = arm == 3 ? 2 : arm == 4 ? 1 : 0;
            Chem *q = chem_new(s, mode, kind);
            advance(q, HORIZON);
            Result r = inspect(q);
            passed[arm] += r.pass;
            candidates[arm] += r.candidate;
            both[arm] += r.pop[0] > 0 && r.pop[1] > 0;
            for (int e = 0; e < 2; e++) {
                births[arm][e] += q->births[e];
                reactions[arm][e] += q->reactions[e];
                supplied[arm][e] += q->supplied[e];
            }
            printf("%s %d %d %d %" PRIu64 " %" PRIu64 " %" PRIu64 " %" PRIu64
                   " %d %d %d %d %d %d %.6f %.6f %.6f %.6f %d %016" PRIx64 "\n",
                   names[arm], s, r.pop[0], r.pop[1], q->births[0], q->births[1],
                   q->reactions[0], q->reactions[1], q->stock[0], q->stock[1], q->sink,
                   r.candidate, r.causal[0], r.causal[1], r.intact[0], r.intact[1],
                   r.alone[0], r.alone[1], r.pass, r.hash);
            chem_free(q);
        }
    }
    puts("\ncomptes bras graines coexistence candidats passent naiss0 naiss1 reactions0 reactions1 energie_externe0 energie_externe1");
    for (int a = 0; a < 5; a++)
        printf("comptes %s %d %d %d %d %" PRIu64 " %" PRIu64 " %" PRIu64 " %" PRIu64 " %" PRIu64 " %" PRIu64 "\n",
               names[a], a < 3 ? NS : NC, both[a], candidates[a], passed[a],
               births[a][0], births[a][1], reactions[a][0], reactions[a][1], supplied[a][0], supplied[a][1]);
    int valid = passed[4] >= 6;
    int yes = valid && passed[0] >= 8 && passed[1] == 0 && passed[2] == 0 &&
        passed[3] == 0 && reactions[2][0] == 0 && reactions[2][1] == 0 &&
        reactions[3][0] == 0 && reactions[3][1] == 0;
    printf("verdict : %s\n", !valid ? "dispositif invalide" : yes ? "oui" : "non");
    return valid ? 0 : 2;
}

int main(int argc, char **argv) {
    if (argc == 2 && !strcmp(argv[1], "selftest")) return selftest();
    if (argc == 2 && !strcmp(argv[1], "run")) return run();
    fprintf(stderr, "usage : %s selftest|run\n", argv[0]);
    return 1;
}
