/* Banc C borné, aucun monde de l'hôte, aucun lancement de garde snapshot.
 * stdout est la mesure ; rien n'est écrit dans un répertoire de vie. */
#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <time.h>
#include "octet.h"
#include "../rencontres-ligne/rendezvous.h"
#include "../rencontres-ligne/naissance.h"
#define main garde_cli_non_executee
#include "../rencontres-ligne/garde.c"
#undef main

static int old_capable(const World *w, const Creature *c) {
    int sensor = 0, step = 0;
    for (int i = 0; i < c->len;) {
        uint8_t op = c->dna[i++];
        if (op == OP_SENSE || op == OP_LOAD || op == OP_SENSE_NB ||
            op == OP_SENSE_EN || op == OP_SENSE_STIM || op == OP_SENSE_HIST) sensor = 1;
        if (op == OP_STEP) step = 1;
        if (op_has_imm(op) || (w->cfg.regs &&
            (op == OPX_RPUSH || op == OPX_RPEEK))) i++;
    }
    return sensor && step;
}

static World *fixture(uint64_t seed, int n) {
    World *w = calloc(1, sizeof *w); assert(w);
    w->rng = malloc(sizeof *w->rng); assert(w->rng); rng_seed(w->rng, seed);
    w->cfg = cfg_default(); w->cfg.w = 32; w->cfg.h = 32;
    w->cfg.max_pop = CMAX; memset(w->food, 14, sizeof w->food);
    for (int i = 0; i < n; i++) {
        const SeedBlob *g = seed_at(i % N_SEEDS);
        int x = i % 32, y = (i / 32) % 32;
        w->cs[w->ncs++] = creature_new(x, y, g->p, g->len, 220, i, 0, -1);
        w->occ[y][x]++; w->st.born++; w->nid++;
    }
    return w;
}
static void discard(World *w) { Rng *r = w->rng; world_free(w); free(r); }

static void gate_measure(void) {
    World *w = fixture(0x0996a2cd, 1);
    Creature c = *w->cs[0];
    int old = 0, behavior = 0;
    for (int i = 0; i < N_SEEDS; i++) {
        const SeedBlob *g = seed_at(i);
        c.len = g->len; memcpy(c.dna, g->p, (size_t)c.len);
        c.ip = 0; c.a = c.b = 0;
        int a = old_capable(w, &c), b = rendezvous_capable(w, &c, 1);
        old += a; behavior += b;
        printf("seed %s statique=%d comportement=%d\n", g->name, a, b);
    }
    printf("seeds total statique=%d/%d comportement=%d/%d\n", old, N_SEEDS, behavior, N_SEEDS);
    Creature lines[16]; assert(N_SEEDS == 16);
    Rng drift; rng_seed(&drift, 0x0996A2CD60ULL);
    for (int i = 0; i < N_SEEDS; i++) {
        lines[i] = c; const SeedBlob *g = seed_at(i);
        lines[i].len = g->len; memcpy(lines[i].dna, g->p, (size_t)g->len);
    }
    int sumold = 0, sumbehavior = 0, minold = 16, maxold = 0, minbehavior = 16, maxbehavior = 0;
    for (int generation = 0; generation <= 60; generation++) {
        old = behavior = 0;
        for (int i = 0; i < N_SEEDS; i++) {
            old += old_capable(w, &lines[i]); behavior += rendezvous_capable(w, &lines[i], 1);
        }
        if (old < minold) minold = old;
        if (old > maxold) maxold = old;
        if (behavior < minbehavior) minbehavior = behavior;
        if (behavior > maxbehavior) maxbehavior = behavior;
        sumold += old; sumbehavior += behavior;
        if (generation % 10 == 0) printf("derive generation=%d statique=%d/16 comportement=%d/16\n", generation, old, behavior);
        if (generation < 60) for (int i = 0; i < N_SEEDS; i++) {
            uint8_t child[MAX_LEN + 16];
            lines[i].len = mutate(lines[i].dna, lines[i].len, &drift, 0.02, 0.05, child);
            memcpy(lines[i].dna, child, (size_t)lines[i].len);
        }
    }
    printf("derive total=%d/976 comportement=%d/976 plages=%d..%d/%d..%d\n", sumold, sumbehavior, minold, maxold, minbehavior, maxbehavior);
    discard(w);
}

static int observer_calls;
static void observer(Creature *c, uint8_t op) { (void)c; (void)op; observer_calls++; }
static void isolation(void) {
    World *w = fixture(0x15, 16), *saved = malloc(sizeof *saved); assert(saved);
    Creature copies[16]; for (int i = 0; i < 16; i++) copies[i] = *w->cs[i];
    *saved = *w; Rng rng = *w->rng;
    vm_set_watch(observer); observer_calls = 0;
    for (int i = 0; i < 16; i++) (void)rendezvous_capable(w, w->cs[i], 1);
    assert(!observer_calls && !memcmp(saved, w, sizeof *w) && !memcmp(&rng, w->rng, sizeof rng));
    for (int i = 0; i < 16; i++) assert(!memcmp(&copies[i], w->cs[i], sizeof copies[i]));
    /* Le vrai watcher est toujours branché après les sondes. */
    vm_run(w->cs[0], w); assert(observer_calls > 0); vm_set_watch(NULL);
    free(saved); discard(w);
    puts("isolation monde+creatures+RNG+watcher 16/16 OK");
    w = fixture(0x16, 2);
    uint8_t code[] = { OP_SPLIT, OP_SENSE, OP_STEP, OP_YIELD };
    memcpy(w->cs[0]->dna, code, sizeof code);
    w->cs[0]->ip = 0;
    double mutation_sum = 0; long mutation_n = 0;
    world_take_birth_mut(&mutation_sum, &mutation_n); /* vider le banc précédent */
    mutation_sum = 0; mutation_n = 0;
    world_set_mut_scale(3);
    assert(rendezvous_capable(w, w->cs[0], 1));
    world_take_birth_mut(&mutation_sum, &mutation_n);
    assert(mutation_sum == 0 && mutation_n == 0);
    vm_run(w->cs[0], w);
    world_take_birth_mut(&mutation_sum, &mutation_n);
    assert(mutation_n == 1 && mutation_sum > 0.059 && mutation_sum < 0.061);
    world_set_mut_scale(0); discard(w);
    puts("isolation TLS mutation OK : sonde 0 naissance réelle, vrai tick taux 0.06 conservé");
}

static void ceiling_measure(void) {
    int passed = 0; long caps = 0, places = 0, hatched = 0, paid = 0;
    for (unsigned seed = 1; seed <= 32; seed++) {
        World *w = fixture(seed, CMAX);
        /* Le dispositif de place ne doit pas dépendre d'une dynamique VM. */
        for (int i = 0; i < w->ncs; i++) w->cs[i]->dna[0] = OP_YIELD;
        NaissanceState *s = calloc(1, sizeof *s); assert(s); naissance_init(s, seed);
        NaissanceConfig cfg = { .enabled = 1, .period = 1, .cost = 10,
            .food_cost = 3, .delay = 1, .cap = CMAX };
        naissance_step(w, &cfg, s); assert(s->eggs == 1);
        cfg.period = 100000; w->t = 1; naissance_step(w, &cfg, s);
        assert(s->refused_cap == 1 && !s->hatched);
        int ex = s->pending[0].x, ey = s->pending[0].y;
        int fx = (ex + 16) % 32, fy = (ey + 16) % 32;
        int removed = fy * 32 + fx;
        Creature *dead = w->cs[removed];
        w->occ[fy][fx]--; free(dead);
        w->cs[removed] = w->cs[--w->ncs];
        w->t = 2; naissance_step(w, &cfg, s);
        assert(s->refused_place == 1 && !s->hatched);
        int nx = (ex + 1) % 32, ny = ey;
        for (int i = 0; i < w->ncs; i++) if (w->cs[i]->x == nx && w->cs[i]->y == ny) {
            w->occ[ny][nx]--; w->occ[fy][fx]++;
            w->cs[i]->x = fx; w->cs[i]->y = fy;
        }
        w->t = 3; naissance_step(w, &cfg, s);
        int ok = s->hatched == 1 && !s->npending && w->ncs == CMAX && s->energy_paid == 20 && s->food_paid == 3;
        assert(ok); passed += ok;
        caps += s->refused_cap; places += s->refused_place; hatched += s->hatched; paid += s->energy_paid;
        free(s); discard(w);
    }
    printf("nurserie CMAX=%d graines=%d refus_plafond=%ld refus_place=%ld eclosions=%ld energie_payee=%ld\n", CMAX, passed, caps, places, hatched, paid);
}

static double seconds(void) { struct timespec t; assert(!clock_gettime(CLOCK_MONOTONIC, &t)); return t.tv_sec + t.tv_nsec * 1e-9; }
static void benchmark(void) {
    const int counts[] = {1, 128, CMAX};
    for (int k = 0; k < 3; k++) {
        World *w = fixture(99, counts[k]); int accepted = 0;
        double begin = seconds();
        for (int j = 0; j < 4096; j++) accepted += rendezvous_capable(w, w->cs[j % w->ncs], 1);
        double elapsed = seconds() - begin;
        printf("cout population=%d sondes=4096 acceptees=%d secondes=%.6f us_par_sonde=%.3f\n", counts[k], accepted, elapsed, elapsed * 1e6 / 4096);
        discard(w);
    }
}

static void protection_measure(void) {
    int pass = 0;
    for (int i = 0; i < NW; i++) {
        char p[256]; snprintf(p, sizeof p, "/tmp/inexistant/%s///", names[i]);
        assert(world_index(p) == i); pass++;
        snprintf(p, sizeof p, "/tmp/inexistant/%s-essai", names[i]);
        assert(world_index(p) == -1); pass++;
    }
    assert(world_index("/tmp/vie-rencontres-b129054c/enfant") == -1);
    printf("protection noms=%d chemins_exacts_et_suffixes=%d/%d OK\n", NW, pass, NW * 2);

    /* Faux /proc privé. exe est un simple fichier de fixture, jamais un ELF
     * vivant. On exerce scan/load sans appeler la commande snapshot. */
    char root[] = "/tmp/octet-garde-0996-XXXXXX"; assert(mkdtemp(root));
    for (int i = 0; i < NW; i++) {
        char dir[256], path[300]; snprintf(dir, sizeof dir, "%s/%d", root, 100 + i);
        assert(!mkdir(dir, 0700)); snprintf(path, sizeof path, "%s/cmdline", dir);
        FILE *f = fopen(path, "wb"); assert(f);
        const char args[] = "fixture\0--dir\0";
        assert(fwrite(args, 1, sizeof args - 1, f) == sizeof args - 1);
        fprintf(f, "/tmp/fiction/%s", names[i]); fputc(0, f); assert(!fclose(f));
        snprintf(path, sizeof path, "%s/stat", dir); f = fopen(path, "w"); assert(f);
        fprintf(f, "%d (fixture) S 1", 100 + i);
        for (int j = 0; j < 17; j++) fputs(" 0", f);
        fprintf(f, " %d\n", 10000 + i); assert(!fclose(f));
        snprintf(path, sizeof path, "%s/exe", dir); f = fopen(path, "w"); assert(f);
        fputs("image de fixture", f); assert(!fclose(f));
    }
    Record rows[NW] = {{0}}, readback[NW] = {{0}}; assert(!scan(root, rows));
    char proof[256]; snprintf(proof, sizeof proof, "%s/preuve-fixture.txt", root);
    FILE *f = fopen(proof, "w"); assert(f);
    fprintf(f, "OCTET_GARDE_V1\t%d\n", NW);
    for (int i = 0; i < NW; i++) print_record(f, i, &rows[i]);
    assert(!fclose(f)); assert(!load_file(proof, readback));
    for (int i = 0; i < NW; i++) assert(same(&rows[i], &readback[i]));
    for (int i = 12; i < NW; i++) {
        char path[300]; snprintf(path, sizeof path, "%s/%d/cmdline", root, 100 + i);
        f = fopen(path, "wb"); assert(f); assert(!fclose(f));
        assert(scan(root, readback) != 0);
        /* Restaurer après l'absence ciblée pour isoler le nom suivant. */
        f = fopen(path, "wb"); assert(f);
        const char args[] = "fixture\0--dir\0";
        assert(fwrite(args, 1, sizeof args - 1, f) == sizeof args - 1);
        fprintf(f, "/tmp/fiction/%s", names[i]); fputc(0, f); assert(!fclose(f));
    }
    assert(!unlink(proof));
    for (int i = 0; i < NW; i++) {
        char dir[256], path[300]; snprintf(dir, sizeof dir, "%s/%d", root, 100 + i);
        const char *parts[] = {"cmdline", "stat", "exe"};
        for (int j = 0; j < 3; j++) { snprintf(path, sizeof path, "%s/%s", dir, parts[j]); assert(!unlink(path)); }
        assert(!rmdir(dir));
    }
    assert(!rmdir(root));
    printf("garde faux proc %d/%d lus, preuve %d/%d relue, deux mondes rencontres absents chacun refusés OK\n", NW, NW, NW, NW);
}

int main(int argc, char **argv) {
    if (argc != 2 || (strcmp(argv[1], "selftest") && strcmp(argv[1], "run"))) {
        fprintf(stderr, "usage: corrections selftest | run\n"); return 2;
    }
    assert(!rendezvous_selftest()); assert(!naissance_selftest());
    isolation(); protection_measure(); ceiling_measure();
    if (!strcmp(argv[1], "run")) { gate_measure(); benchmark(); }
    puts("corrections selftest OK"); return 0;
}
