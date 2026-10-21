/* Tests Octet-ADN (C pur, assert). Lancer : make test */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "octet.h"

int hamming128(const uint8_t *a, int la, const uint8_t *b, int lb);
int dialog_reply(int s);
static int passed;
#define TEST(name) static void name(void)
#define RUN(name) do { name(); printf("ok  %s\n", #name); passed++; } while (0)

TEST(test_legacy_opcodes_preserved) {
    const char *legacy[16] = { "NOP", "INC_A", "DEC_A", "INC_B", "DEC_B", "CLR_A", "STORE", "LOAD",
                               "JMP", "JZ", "SENSE", "TURN", "STEP", "EAT", "SPLIT", "YIELD" };
    for (int i = 0; i < 16; i++) assert(!strcmp(op_name((uint8_t)i), legacy[i]));
    assert(!strcmp(op_name(0x13), "NEURAL") && !strcmp(op_name(0x14), "EMIT") && !strcmp(op_name(0x1F), "AND"));
}

TEST(test_decode_bytes) {
    /* EAT ; STORE 19 ; NEURAL 0x80 ; EMIT ; LOADI 0xAC ; LOOP */
    const uint8_t raw[] = { 0x0d, 0x06, 0x13, 0x13, 0x80, 0x14, 0x1c, 0xac, 0x1e };
    char out[1024];
    disasm(raw, sizeof raw, 0, out, sizeof out);
    assert(strstr(out, "0000  0d     EAT\n"));
    assert(strstr(out, "0001  06 13  STORE 19\n"));
    assert(strstr(out, "0003  13 80  NEURAL 128\n"));
    assert(strstr(out, "0005  14     EMIT\n"));
    assert(strstr(out, "0006  1c ac  LOADI 172\n"));
    assert(strstr(out, "0008  1e     LOOP\n"));
}

TEST(test_introns) {
    assert(!op_valid(0xff) && !strcmp(op_name(0xff), "INTRON") && op_valid(0x1f) && !op_valid(0x20));
    const uint8_t raw[] = { 0xff };
    char out[64]; disasm(raw, 1, 0, out, sizeof out);
    assert(strstr(out, "INTRON:ff"));
}

TEST(test_seeds_embedded_match_files) {
    for (int i = 0; i < N_SEEDS; i++) {
        const SeedBlob *s = seed_at(i);
        char p[256]; snprintf(p, sizeof p, "seeds/%s.adn", s->name);
        FILE *f = fopen(p, "rb"); assert(f);
        uint8_t buf[MAX_LEN]; size_t n = fread(buf, 1, sizeof buf, f); fclose(f);
        assert((int)n == s->len && !memcmp(buf, s->p, n));
    }
    assert(seed_get("forager")->len == 256);
}

static World *mkworld(uint64_t seed, int n, Rng *r) {
    PoolEntry g[8];
    rng_seed(r, seed);
    for (int i = 0; i < n; i++) seed_entry(&g[i], "forager", 0);
    WorldConfig c = cfg_default();
    return world_new(g, n, r, &c, NULL);
}

TEST(test_tick_deterministic) {
    Rng ra, rb, rc;
    World *a = mkworld(7, 6, &ra), *b = mkworld(7, 6, &rb), *c = mkworld(8, 6, &rc);
    for (int t = 0; t < 60; t++) { world_step(a); world_step(b); world_step(c); }
    assert(world_hash(a) == world_hash(b));
    assert(world_hash(a) != world_hash(c));
    world_free(a); world_free(b); world_free(c);
}

TEST(test_store_rewrites_byte_and_counts_useful) {
    /* LOADI 0x0E ; STORE 6 ; JMP +0 ; [6] NOP ; YIELD — le slot 6 devient SPLIT puis est exécuté */
    uint8_t g[16] = { 0x1c, 0x0e, 0x06, 0x06, 0x08, 0x00, 0x00, 0x0f };
    memset(g + 8, 0xff, 8);
    Rng r; rng_seed(&r, 0);
    WorldConfig cfg = cfg_default();
    World *w = world_new(NULL, 0, &r, &cfg, NULL);
    Creature *c = creature_new(5, 5, g, 16, 200, 0, 0, -1);
    w->cs[w->ncs++] = c; w->occ[5][5]++;
    assert(c->dna[6] == 0x00);
    vm_run(c, w);
    assert(c->dna[6] == 0x0E);
    assert(c->stores == 1 && c->useful_stores == 1);
    assert(w->nnb == 1);                       /* le SPLIT réécrit a bien été exécuté */
    world_free(w);
}

TEST(test_neural_identity_on_stimulus) {
    PoolEntry g; seed_entry(&g, "neural", 0);  /* NEURAL syn0 (poids stimulus = 16) ; EMIT */
    Rng r; rng_seed(&r, 0);
    WorldConfig cfg = cfg_default();
    World *w = world_new(&g, 1, &r, &cfg, NULL);
    w->stimulus = 9;
    vm_run(w->cs[0], w);
    assert(w->nem >= 1 && w->em[0].v == 9);
    world_free(w);
}

TEST(test_mutation_bounds_and_determinism) {
    PoolEntry g; seed_entry(&g, "forager", 0);
    uint8_t m1[MAX_LEN + 16], m2[MAX_LEN + 16];
    Rng a, b; rng_seed(&a, 3); rng_seed(&b, 3);
    int n1 = mutate(g.dna, g.len, &a, 0.2, 1.0, m1), n2 = mutate(g.dna, g.len, &b, 0.2, 1.0, m2);
    assert(n1 == n2 && !memcmp(m1, m2, (size_t)n1));
    assert(n1 >= MIN_LEN && n1 <= MAX_LEN);
    assert(n1 != g.len || memcmp(m1, g.dna, (size_t)n1));
}

TEST(test_duplication_and_crossover) {
    uint8_t buf[64]; for (int i = 0; i < 32; i++) buf[i] = (uint8_t)i;
    Rng r; rng_seed(&r, 1);
    assert(duplicate_block(buf, 32, &r) > 32);
    uint8_t a[16], b[16], x[16]; memset(a, 1, 16); memset(b, 2, 16);
    int n = crossover(a, 16, b, 16, &r, x);
    assert(n == 16 && memchr(x, 1, 16) && memchr(x, 2, 16));
    assert(hamming128(a, 16, b, 16) == 16 && hamming128(a, 16, a, 10) == 6);
}

TEST(test_ten_labs_registered) {
    const char *names[N_LABS] = { "org-seeds", "max-pop", "survival", "energy-eff", "self-rewrite",
                                  "emit-echo", "emit-dialog", "predict-walk", "predict-stock", "predator-prey" };
    for (int i = 0; i < N_LABS; i++) assert(lab_index(names[i]) == i);
    assert(lab_index("nope") == -1);
    assert(dialog_reply(0x00) == 0xAC && dialog_reply(0x0F) == 0x00 && dialog_reply(5) == 6);
}

TEST(test_emit_echo_small_sim) {
    int id = lab_index("emit-echo");
    Lab l; lab_init(&l, id, 60);
    PoolEntry pool[POOL_MAX]; int n = LABS[id].seeds(pool);
    Rng r; rng_seed(&r, 1);
    static CycleResult res;
    run_cycle(&l, pool, n, &r, 60, &res);
    assert(l.total > 0);
    assert((double)l.correct / l.total > 0.9);   /* la seed fait l'écho */
    assert(res.fitness > 0.5 && res.nnew > 0);
    printf("    emit-echo 60 ticks : accuracy=%.4f fitness=%.4f\n", (double)l.correct / l.total, res.fitness);
    world_free(res.w); lab_free(&l);
}

TEST(test_runner_dry_run_two_labs) {
    RunOpts o = { .ticks = 50, .dry_run = 1, .quiet = 1, .data_dir = "unused" };
    o.labs[o.nlabs++] = lab_index("emit-echo"); o.labs[o.nlabs++] = lab_index("max-pop");
    MetricRec rows[4];
    int n = runner(&o, rows, 4);
    assert(n == 2 && rows[0].lab_id == 5 && rows[1].lab_id == 1);
    assert(rows[0].ticks == 50 && rows[1].ticks == 50);
    assert(access("unused", F_OK) != 0);        /* dry-run : rien d'écrit */
}

TEST(test_runner_persists_and_is_deterministic) {
    char d1[] = "/tmp/oadn-test-XXXXXX", d2[] = "/tmp/oadn-test-XXXXXX";
    assert(mkdtemp(d1) && mkdtemp(d2));
    MetricRec r1[4], r2[4];
    const char *dirs[2] = { d1, d2 };
    for (int k = 0; k < 2; k++) {
        RunOpts o = { .ticks = 50, .cycles = 2, .quiet = 1, .no_reload = 1, .data_dir = dirs[k] };
        o.labs[o.nlabs++] = lab_index("survival"); o.labs[o.nlabs++] = lab_index("predict-stock");
        assert(runner(&o, k ? r2 : r1, 4) == 4);
    }
    for (int i = 0; i < 4; i++) assert(r1[i].state_hash == r2[i].state_hash && r1[i].fitness == r2[i].fitness);
    char p[512]; Checkpoint ck; MetricRec *v;
    snprintf(p, sizeof p, "%s/survival/checkpoints/latest.ck", d1);
    assert(ck_load(p, &ck) == 0 && ck.cycle == 2 && ck.npool > 0);
    snprintf(p, sizeof p, "%s/predict-stock/metrics.bin", d1);
    assert(met_read(p, &v) == 2); free(v);
    snprintf(p, sizeof p, "%s/status.md", d1);
    FILE *f = fopen(p, "r"); assert(f);
    char buf[8192]; size_t n = fread(buf, 1, sizeof buf - 1, f); buf[n] = 0; fclose(f);
    assert(strstr(buf, "predict-stock"));
    char cmd[1200]; snprintf(cmd, sizeof cmd, "rm -rf %s %s", d1, d2);
    assert(system(cmd) == 0);
}

TEST(test_checkpoint_roundtrip) {
    static Checkpoint a, b;
    a.lab_id = 3; a.cycle = 42; a.best_fitness = 1.5; a.engine = 0x1234; a.has_best = 1;
    seed_entry(&a.best, "predator", 1);
    a.npool = 2; seed_entry(&a.pool[0], "forager", 0); seed_entry(&a.pool[1], "echo", 1);
    char p[] = "/tmp/oadn-ck-XXXXXX"; int fd = mkstemp(p); close(fd);
    assert(ck_save(p, &a) == 0 && ck_load(p, &b) == 0);
    assert(b.lab_id == 3 && b.cycle == 42 && b.best_fitness == 1.5 && b.engine == 0x1234 && b.npool == 2);
    assert(b.pool[1].species == 1 && !memcmp(b.pool[1].dna, a.pool[1].dna, 256) && b.best.species == 1);
    unlink(p);
}

TEST(test_pp_predators_are_mortal) {
    /* Régression : un prédateur qui ne fait que YIELD était « immortel » à l'échelle d'un cycle
     * (220 d'énergie, ATTACK vole 30 pour 1). Avec la config predator-prey il meurt à PP_PRED_MAX_AGE. */
    const uint8_t g[8] = { 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f };
    int id = lab_index("predator-prey");
    for (int pp = 0; pp < 2; pp++) {
        Rng r; rng_seed(&r, 5);
        WorldConfig cfg = cfg_default();
        if (pp) LABS[id].config(&cfg);
        assert(pp ? cfg.pred_max_age == PP_PRED_MAX_AGE : cfg.pred_max_age == 0);
        World *w = world_new(NULL, 0, &r, &cfg, NULL);
        Creature *c = creature_new(3, 3, g, 8, 220, 0, 1, -1);
        w->cs[w->ncs++] = c; w->occ[3][3]++;
        int t = 0;
        while (w->ncs && t < 150) { world_step(w); t++; }
        if (pp) assert(w->ncs == 0 && t <= PP_PRED_MAX_AGE);
        else assert(w->ncs == 1);                   /* moteur par défaut : inchangé pour les autres labos */
        world_free(w);
    }
    /* ATTACK coûte cfg.attack_cost */
    Rng r; rng_seed(&r, 6);
    WorldConfig cfg = cfg_default(); cfg.attack_cost = 5;
    World *w = world_new(NULL, 0, &r, &cfg, NULL);
    const uint8_t a[2] = { 0x1a, 0x0f };            /* ATTACK ; YIELD, seul : rien à voler */
    Creature *c = creature_new(3, 3, a, 2, 100, 0, 1, -1);
    w->cs[w->ncs++] = c; w->occ[3][3]++;
    vm_run(c, w);
    assert(c->e == 100 - 5 - 1);
    world_free(w);
}

TEST(test_pp_selection_not_frozen) {
    /* Régression : avant, un pic chanceux (fitness 1) puis la règle des 90 % gelait le pool et la
     * fitness restait à 0.4 (mean_pred == 2 pile, 0 acceptation). On exige des acceptations
     * après le cycle 5 et une population de prédateurs qui varie. */
    char d[] = "/tmp/oadn-pp-XXXXXX"; assert(mkdtemp(d));
    RunOpts o = { .ticks = 200, .cycles = 20, .quiet = 1, .no_reload = 1, .data_dir = d };
    o.labs[o.nlabs++] = lab_index("predator-prey");
    MetricRec rows[20];
    assert(runner(&o, rows, 20) == 20);
    int acc_late = 0, pred_is_2 = 0; double sum = 0;
    for (int i = 5; i < 20; i++) { acc_late += rows[i].accepted; pred_is_2 += rows[i].extra[1] == 2.0; }
    for (int i = 10; i < 20; i++) sum += rows[i].fitness;
    /* seuils calibrés (déterministe, graine 0) : ancien moteur = 0.40 figé ; nouveau moteur + règle des
     * 90 % = 2 acceptations, 0.29 ; nouveau moteur + sélection appariée = 4 acceptations, 0.61 */
    assert(acc_late >= 3 && pred_is_2 < 15 && sum / 10 >= 0.45);
    printf("    predator-prey 20 cycles : %d acceptations (c6-20), fitness moy. c11-20 = %.3f\n", acc_late, sum / 10);
    char cmd[600]; snprintf(cmd, sizeof cmd, "rm -rf %s", d);
    assert(system(cmd) == 0);
}

/* emis par un individu walk seul (sans labo) pour un stimulus donné, après insertion optionnelle d'un octet */
static int walk_emit(int stim, int insert_at) {
    PoolEntry g; seed_entry(&g, "walk", 0);
    if (insert_at >= 0) {
        memmove(g.dna + insert_at + 1, g.dna + insert_at, (size_t)(g.len - insert_at)); g.dna[insert_at] = 0xff; g.len++;
    }
    Rng r; rng_seed(&r, 0);
    WorldConfig cfg = cfg_default();
    World *w = world_new(&g, 1, &r, &cfg, NULL);
    w->stimulus = stim;
    int v = -1;
    for (int t = 0; t < 3 && v < 0; t++) { vm_run(w->cs[0], w); if (w->nem) v = w->em[0].v; }
    world_free(w);
    return v;
}

TEST(test_walk_seed_shrinks_and_survives_indels) {
    /* La seed walk prédit floor(s/2)+2 (régression vers la moyenne : optimum ≈ 0.358 contre 0.332 pour
     * la persistance) et ses poids NEURAL sont aux octets 0..4 : une insertion après le code ne les
     * décale plus (la seed neural, poids à 128, tombait à 0.24 car presque chaque enfant les perdait). */
    const int want[8] = { 2, 2, 3, 3, 4, 4, 5, 5 };
    for (int s = 0; s < 8; s++) {
        assert(walk_emit(s, -1) == want[s]);
        assert(walk_emit(s, 0x40) == want[s]);
        assert(walk_emit(s, 0xb0) == want[s]);
    }
}

TEST(test_predict_walk_beats_persistence) {
    /* Régression predict-walk : après 40 cycles (graine 0), le pool doit battre « répéter la dernière
     * valeur » sur 8 graines tenues à l'écart, même série pour les deux. Avant : -0.003 (graine 0),
     * -0.070 (graine 1). Mesuré ici : +0.027. Plafond théorique sur cette série ≈ +0.027 (bins 0..7). */
    char d[] = "/tmp/oadn-pw-XXXXXX"; assert(mkdtemp(d));
    RunOpts o = { .ticks = 200, .cycles = 40, .quiet = 1, .no_reload = 1, .data_dir = d };
    int id = lab_index("predict-walk");
    o.labs[o.nlabs++] = id;
    assert(runner(&o, NULL, 0) == 40);
    char p[512]; snprintf(p, sizeof p, "%s/predict-walk/checkpoints/latest.ck", d);
    static Checkpoint ck; assert(ck_load(p, &ck) == 0 && ck.npool > 0);
    double gap = 0, ex[N_EXTRA];
    for (int j = 0; j < 8; j++) {
        char key[64]; snprintf(key, sizeof key, "holdout:predict-walk:%d", j);
        double f = pool_eval_x(id, ck.pool, ck.npool, key, 200, NULL, 0, ex);
        assert(ex[1] >= 1000);                      /* le pool prédit vraiment (pas d'EMIT abandonné) */
        gap += (f - ex[3]) / 8;
    }
    printf("    predict-walk 40 cycles : écart moyen à la persistance = %+.4f\n", gap);
    assert(gap > 0.015);
    char cmd[600]; snprintf(cmd, sizeof cmd, "rm -rf %s", d);
    assert(system(cmd) == 0);
}

TEST(test_reseed_on_engine_change) {
    /* Un pool en place qui n'émet plus (cas du runner 24/7 : fitness 0.02) est remplacé par la seed
     * au premier cycle d'un nouveau moteur, si elle fait nettement mieux sur les mêmes graines. */
    char d[] = "/tmp/oadn-rs-XXXXXX"; assert(mkdtemp(d));
    int id = lab_index("predict-walk");
    static Checkpoint ck; memset(&ck, 0, sizeof ck);
    ck.lab_id = id; ck.cycle = 7; ck.engine = 1;    /* moteur « ancien » factice */
    seed_entry(&ck.pool[0], "forager", 0); ck.npool = 1;   /* fourrageur muet : jamais d'EMIT */
    char p[512]; snprintf(p, sizeof p, "%s/predict-walk", d); mkdir(p, 0755);
    snprintf(p, sizeof p, "%s/predict-walk/checkpoints", d); mkdir(p, 0755);
    snprintf(p, sizeof p, "%s/predict-walk/checkpoints/latest.ck", d);
    assert(ck_save(p, &ck) == 0);
    RunOpts o = { .ticks = 200, .cycles = 1, .quiet = 1, .no_reload = 1, .data_dir = d };
    o.labs[o.nlabs++] = id;
    MetricRec r; assert(runner(&o, &r, 1) == 1);
    assert(r.cycle == 8 && r.extra[1] >= 1000 && r.fitness > 0.25);
    char cmd[600]; snprintf(cmd, sizeof cmd, "rm -rf %s", d);
    assert(system(cmd) == 0);
}

int main(void) {
    RUN(test_legacy_opcodes_preserved);
    RUN(test_decode_bytes);
    RUN(test_introns);
    RUN(test_seeds_embedded_match_files);
    RUN(test_tick_deterministic);
    RUN(test_store_rewrites_byte_and_counts_useful);
    RUN(test_neural_identity_on_stimulus);
    RUN(test_mutation_bounds_and_determinism);
    RUN(test_duplication_and_crossover);
    RUN(test_ten_labs_registered);
    RUN(test_emit_echo_small_sim);
    RUN(test_runner_dry_run_two_labs);
    RUN(test_runner_persists_and_is_deterministic);
    RUN(test_checkpoint_roundtrip);
    RUN(test_pp_predators_are_mortal);
    RUN(test_pp_selection_not_frozen);
    RUN(test_walk_seed_shrinks_and_survives_indels);
    RUN(test_predict_walk_beats_persistence);
    RUN(test_reseed_on_engine_change);
    printf("\n%d/%d tests OK\n", passed, 19);
    return 0;
}
