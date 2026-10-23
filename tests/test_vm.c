/* Tests Octet-ADN (C pur, assert). Lancer : make test */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <math.h>
#include "octet.h"

int hamming128(const uint8_t *a, int la, const uint8_t *b, int lb);
int dialog_reply(int s);
void hl_se_init(Lab *l, World *w);
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
    const char *names[N_CORE_LABS] = { "org-seeds", "max-pop", "survival", "energy-eff", "self-rewrite",
                                  "emit-echo", "emit-dialog", "predict-walk", "predict-stock", "predator-prey" };
    for (int i = 0; i < N_CORE_LABS; i++) assert(lab_index(names[i]) == i);
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

/* ======================= labos humanité ======================= */
static double ev4(const char *lab, const PoolEntry *p, int n, double *ex_mean) {
    int id = lab_index(lab); double s = 0, ex[N_EXTRA];
    if (ex_mean) memset(ex_mean, 0, sizeof(double) * N_EXTRA);
    for (int j = 0; j < 4; j++) {
        char k[64]; snprintf(k, sizeof k, "test:%s:%d", lab, j);
        s += pool_eval_x(id, p, n, k, 200, NULL, 0, ex) / 4;
        if (ex_mean) for (int i = 0; i < N_EXTRA; i++) ex_mean[i] += ex[i] / 4;
    }
    return s;
}
static int raw_pool(PoolEntry *p, const char *hex) {
    for (int k = 0; k < 4; k++) {
        int L = 0;
        for (const char *h = hex; h[0] && h[1]; h += 2) { unsigned v; sscanf(h, "%2x", &v); p[k].dna[L++] = (uint8_t)v; }
        while (L < 8) p[k].dna[L++] = 0xff;
        p[k].len = L; p[k].species = 0;
    }
    return 4;
}

TEST(test_sixteen_labs_and_core_untouched) {
    const char *nw[6] = { "fold", "antibiotic", "commons", "pandemic", "code-repair", "self-evolve" };
    for (int i = 0; i < 6; i++) assert(lab_index(nw[i]) == N_CORE_LABS + i);
    assert(N_LABS == 19 && cfg_default().self_evo == 0);
    Rng r; rng_seed(&r, 0); WorldConfig cfg = cfg_default();
    World *w = world_new(NULL, 0, &r, &cfg, NULL);
    assert(w->before_vm == NULL);
    /* LOADI 7 ; STORE 0xE0 ; YIELD : sans fenêtre, écrit dans le génome (0xE0 % 16 = 0) ; avec, dans win */
    uint8_t g[16] = { 0x1c, 0x07, 0x06, 0xe0, 0x0f }; memset(g + 5, 0xff, 11);
    for (int win = 0; win < 2; win++) {
        Creature *c = creature_new(1, 1, g, 16, 100, 0, 0, -1);
        if (win) c->win_len = 8;
        vm_run(c, w);
        if (win) assert(c->dna[0] == 0x1c && c->win[0] == 7);
        else assert(c->dna[0] == 7);
        free(c);
    }
    world_free(w);
}

TEST(test_evo_params_bounded) {
    uint8_t g[40]; memset(g, 0xff, sizeof g);
    EvoParams p = evo_params(g, 40);
    assert(p.mut <= EVO_MUT_MAX + 1e-12 && p.indel <= EVO_INDEL_MAX + 1e-12 && p.prot <= EVO_PROT_MAX && p.xover == 1);
    p = evo_params(g, 20); assert(p.prot <= 10);
    uint8_t out[MAX_LEN + 16]; Rng r; rng_seed(&r, 1);
    EvoParams z = { 0, 0, 0, 0 };
    for (int i = 0; i < 40; i++) g[i] = (uint8_t)i;
    assert(mutate_evo(g, 40, &r, &z, out) == 40 && !memcmp(out, g, 2) && !memcmp(out + 6, g + 6, 34));   /* taux 0 = copie, sauf paramètres */
    int moved = 0;                                                            /* les octets 2..5 dérivent toujours (méta) */
    EvoParams pr = { 0, 0, 0, 16 };
    for (int k = 0; k < 200; k++) { mutate_evo(g, 40, &r, &pr, out); moved += memcmp(out + 2, g + 2, 4) != 0; assert(!memcmp(out + 6, g + 6, 10)); }
    assert(moved > 0);
    EvoParams all = { 1.0, 0, 0, 12 };
    mutate_evo(g, 40, &r, &all, out);
    assert(!memcmp(out, g, 2) && !memcmp(out + 6, g + 6, 6) && memcmp(out + 12, g + 12, 28));   /* zone protégée intacte (hors paramètres) */
}

TEST(test_new_labs_beat_naive_baselines) {
    PoolEntry p[POOL_MAX], f4[4]; double ex[N_EXTRA]; int n;
    for (int k = 0; k < 4; k++) seed_entry(&f4[k], "forager", 0);
    n = LABS[lab_index("fold")].seeds(p);        double fo = ev4("fold", p, n, ex);       assert(fo > ex[2]);   /* > marche aléatoire */
    n = LABS[lab_index("antibiotic")].seeds(p);  double ab = ev4("antibiotic", p, n, ex); assert(ab > ex[4] + 0.2);
    n = LABS[lab_index("commons")].seeds(p);     double cm = ev4("commons", p, n, ex);    assert(cm > ex[3] + 0.05);
    n = LABS[lab_index("pandemic")].seeds(p);    double pd = ev4("pandemic", p, n, ex);   double pdf = ev4("pandemic", f4, 4, NULL); assert(pd > 0.2 && pd > pdf);
    n = LABS[lab_index("code-repair")].seeds(p); double cr = ev4("code-repair", p, n, ex); assert(cr > ex[4] + 0.1);
    n = LABS[lab_index("self-evolve")].seeds(p); double se = ev4("self-evolve", p, n, ex);
    assert(ex[1] > 0);                                                      /* bat le moteur fixe 2 % / 5 % */
    for (int k = 0; k < n; k++) { p[k].dna[2] = 0; p[k].dna[3] = 0; }
    double gel = ev4("self-evolve", p, n, NULL);
    assert(se > gel);                                                       /* geler mut et indel fait perdre */
    printf("    fold=%.3f antibiotic=%.3f commons=%.3f pandemic=%.3f code-repair=%.3f self-evolve=%.3f vs=%+.3f gel=%.3f\n", fo, ab, cm, pd, cr, se, ex[1], gel);
}

TEST(test_new_labs_exploits_closed) {
    PoolEntry p[POOL_MAX]; double ex[N_EXTRA];
    const char *inert = "0f0f0f0f0f0f0f0f";
    assert(ev4("fold", p, raw_pool(p, inert), NULL) == 0);                     /* tout droit = 0 contact */
    assert(ev4("commons", p, raw_pool(p, inert), NULL) == 0);                  /* ne rien faire = s'éteindre */
    assert(ev4("code-repair", p, raw_pool(p, inert), NULL) == 0);
    assert(ev4("pandemic", p, raw_pool(p, inert), NULL) == 0);                 /* aléa commun : sans action, paires identiques */
    assert(ev4("antibiotic", p, raw_pool(p, "1a1a0d0a09030c0e0f010b0c1a0e0f1e"), ex) < 0.05);   /* tuer les hôtes */
    double spam = ev4("antibiotic", p, raw_pool(p, "01140d0a09030c0e0f010b0c0d0e0f1e"), NULL);   /* tous les médicaments */
    int n = LABS[lab_index("antibiotic")].seeds(p);
    assert(spam < ev4("antibiotic", p, n, NULL));
    double sweep = ev4("self-evolve", p, raw_pool(p, "080400000000011400140d0a09030c0e0f010b0c0e0f1e"), NULL);
    n = LABS[lab_index("self-evolve")].seeds(p);
    assert(sweep < ev4("self-evolve", p, n, NULL));                            /* balayer 0..15 ne paie pas */
}

TEST(test_resume_legacy_checkpoints_with_16_labs) {
    char d[] = "/tmp/oadn-rs16-XXXXXX"; assert(mkdtemp(d));
    RunOpts o = { .ticks = 50, .cycles = 1, .quiet = 1, .no_reload = 1, .data_dir = d };
    o.labs[o.nlabs++] = lab_index("max-pop");
    assert(runner(&o, NULL, 0) == 1);
    RunOpts a = { .ticks = 50, .cycles = 1, .quiet = 1, .no_reload = 1, .data_dir = d };
    for (int i = 0; i < N_LABS; i++) a.labs[a.nlabs++] = i;
    MetricRec rows[N_LABS];
    assert(runner(&a, rows, N_LABS) == N_LABS);
    assert(rows[1].cycle == 2);                                                /* repris */
    for (int i = N_CORE_LABS; i < N_LABS; i++) assert(rows[i].cycle == 1 && rows[i].lab_id == i);
    char cmd[600]; snprintf(cmd, sizeof cmd, "rm -rf %s", d);
    assert(system(cmd) == 0);
}

static PoolEntry cycle_top(const char *lab, PoolEntry *pool, int n, int ticks) {
    int id = lab_index(lab);
    Lab l; lab_init(&l, id, ticks);
    Rng r; rng_seed(&r, 1);
    static CycleResult res;
    run_cycle(&l, pool, n, &r, ticks, &res);
    assert(res.nnew > 0);
    PoolEntry top = res.newpool[0];
    world_free(res.w); lab_free(&l);
    return top;
}

TEST(test_contribution_outranks_energy) {
    /* Le glouton (forager) et le muet énergétique ne doivent plus passer devant
     * la seed qui fait le travail du labo. */
    PoolEntry pool[4], commons, fora, anti, top;
    seed_entry(&commons, "commons", 0); seed_entry(&fora, "forager", 0); seed_entry(&anti, "antibiotic", 0);
    for (int i = 0; i < 2; i++) pool[i] = fora;
    for (int i = 2; i < 4; i++) pool[i] = commons;
    top = cycle_top("commons", pool, 4, 16);   /* ×5 : 80 ticks, la population est encore là */
    assert(hamming128(top.dna, top.len, commons.dna, commons.len) < hamming128(top.dna, top.len, fora.dna, fora.len));
    for (int i = 0; i < 2; i++) pool[i] = fora;
    for (int i = 2; i < 4; i++) pool[i] = anti;
    top = cycle_top("antibiotic", pool, 4, 80);
    assert(hamming128(top.dna, top.len, anti.dna, anti.len) < hamming128(top.dna, top.len, fora.dna, fora.len));
    PoolEntry pand; seed_entry(&pand, "pandemic", 0);
    for (int i = 0; i < 2; i++) pool[i] = fora;
    for (int i = 2; i < 4; i++) pool[i] = pand;
    top = cycle_top("pandemic", pool, 4, 80);
    assert(hamming128(top.dna, top.len, pand.dna, pand.len) < hamming128(top.dna, top.len, fora.dna, fora.len));
}

TEST(test_selfevo_seed_keeps_a_mutation_rate) {
    /* Geler mut et indel à 0 perd sous le mètre actuel (mort à 60, dérive saturée).
     * La seed porte un taux non nul et ne le réécrit pas à chaque tick. */
    PoolEntry s; seed_entry(&s, "selfevo", 0);
    assert(s.dna[2] != 0);
    for (int i = 0; i + 1 < s.len; i++)
        assert(!(s.dna[i] == OP_STORE && (s.dna[i + 1] == 2 || s.dna[i + 1] == 3)));
}

TEST(test_foreign_admits_only_positive_gap) {
    char d[] = "/tmp/oadn-fr-XXXXXX"; assert(mkdtemp(d));
    char cmd[800];
    snprintf(cmd, sizeof cmd, "mkdir -p %s/max-pop/checkpoints %s/fold/checkpoints %s/code-repair/checkpoints", d, d, d);
    assert(system(cmd) == 0);
    Checkpoint ck; memset(&ck, 0, sizeof ck); ck.has_best = 1; ck.cycle = 1;
    seed_entry(&ck.best, "forager", 0); ck.lab_id = lab_index("max-pop");
    char p[512]; snprintf(p, sizeof p, "%s/max-pop/checkpoints/latest.ck", d); assert(ck_save(p, &ck) == 0);
    seed_entry(&ck.best, "fold", 0); ck.lab_id = lab_index("fold");
    snprintf(p, sizeof p, "%s/fold/checkpoints/latest.ck", d); assert(ck_save(p, &ck) == 0);
    seed_entry(&ck.best, "repair", 0); ck.lab_id = lab_index("code-repair");
    snprintf(p, sizeof p, "%s/code-repair/checkpoints/latest.ck", d); assert(ck_save(p, &ck) == 0);
    MetricRec rec; memset(&rec, 0, sizeof rec);
    rec.lab_id = (uint8_t)lab_index("fold"); rec.extra[3] = -0.4;
    snprintf(p, sizeof p, "%s/fold/metrics.bin", d); assert(met_append(p, &rec) == 0);
    memset(&rec, 0, sizeof rec);
    rec.lab_id = (uint8_t)lab_index("code-repair"); rec.extra[5] = 0.2;
    snprintf(p, sizeof p, "%s/code-repair/metrics.bin", d); assert(met_append(p, &rec) == 0);
    PoolEntry out[16], repair, fold, fora;
    seed_entry(&repair, "repair", 0); seed_entry(&fold, "fold", 0); seed_entry(&fora, "forager", 0);
    int n = collect_foreign(d, out);
    assert(n == 2);
    assert(!memcmp(out[0].dna, fora.dna, (size_t)fora.len));
    assert(!memcmp(out[1].dna, repair.dna, (size_t)repair.len));
    assert(memcmp(out[1].dna, fold.dna, 8));
    /* Le sommet d'un cycle refusé (best) ne passe pas devant le pool mesuré. */
    seed_entry(&ck.best, "forager", 0);
    seed_entry(&ck.pool[0], "repair", 0); ck.npool = 1; ck.has_best = 1;
    snprintf(p, sizeof p, "%s/code-repair/checkpoints/latest.ck", d); assert(ck_save(p, &ck) == 0);
    n = collect_foreign(d, out);
    assert(n == 2 && !memcmp(out[1].dna, repair.dna, (size_t)repair.len));
    assert(memcmp(out[1].dna, fora.dna, 8));
    /* écart négatif : le champion humanité reste dehors */
    rec.extra[5] = -0.05; snprintf(p, sizeof p, "%s/code-repair/metrics.bin", d); unlink(p); assert(met_append(p, &rec) == 0);
    n = collect_foreign(d, out);
    assert(n == 1 && !memcmp(out[0].dna, fora.dna, (size_t)fora.len));
    snprintf(cmd, sizeof cmd, "rm -rf %s", d); assert(system(cmd) == 0);
}

TEST(test_code_repair_pool_beats_flips) {
    /* Après 12 cycles, le pool bat les flips aléatoires sur 8 graines tenues à l'écart.
     * La sélection peut garder la seed : l'écart mesuré est celui du pool réel. */
    char d[] = "/tmp/oadn-cr-XXXXXX"; assert(mkdtemp(d));
    RunOpts o = { .ticks = 200, .cycles = 12, .quiet = 1, .no_reload = 1, .data_dir = d };
    int id = lab_index("code-repair");
    o.labs[o.nlabs++] = id;
    assert(runner(&o, NULL, 0) == 12);
    char p[512]; snprintf(p, sizeof p, "%s/code-repair/checkpoints/latest.ck", d);
    static Checkpoint ck; assert(ck_load(p, &ck) == 0 && ck.npool > 0);
    PoolEntry seed[4]; int ns = LABS[id].seeds(seed);
    int replaced = ck.npool != ns || memcmp(ck.pool[0].dna, seed[0].dna, (size_t)seed[0].len);
    double gap = 0;
    for (int j = 0; j < 8; j++) {
        char key[64]; snprintf(key, sizeof key, "holdout:code-repair:%d", j);
        double ex[N_EXTRA];
        pool_eval_x(id, ck.pool, ck.npool, key, 200, NULL, 0, ex);
        gap += ex[5] / 8;
    }
    printf("    code-repair 12 cycles : vs_baseline moyen = %+.4f  pool_remplace=%d\n", gap, replaced);
    assert(gap > 0);
    char cmd[600]; snprintf(cmd, sizeof cmd, "rm -rf %s", d);
    assert(system(cmd) == 0);
}

/* 1 cycle, 50 ticks, graine 0. Relevés sur origin/main 9b57fb9. */
TEST(test_core_state_hash_matches_main) {
    static const struct { const char *name; uint64_t hash; } want[] = {
        { "org-seeds",     0xa0fa0608f5856113ULL },
        { "max-pop",       0x5c214ea538538273ULL },
        { "survival",      0x12191814d6e72613ULL },
        { "energy-eff",    0x45073d51311ff435ULL },
        { "self-rewrite",  0xc02ecc32b6826a19ULL },
        { "emit-echo",     0x40a3793d144cd485ULL },
        { "emit-dialog",   0xfa6b54894526b4b0ULL },
        { "predict-walk",  0x06a9b0fb1ac9fd67ULL },
        { "predict-stock", 0xdb8a47db549d815cULL },
        { "predator-prey", 0x7cdfa8ae1a89366eULL },
    };
    RunOpts o = { .ticks = 50, .cycles = 1, .quiet = 1, .no_reload = 1, .dry_run = 1,
                  .data_dir = "/tmp/unused-hash" };
    for (int i = 0; i < 10; i++) o.labs[o.nlabs++] = lab_index(want[i].name);
    MetricRec rows[10];
    assert(runner(&o, rows, 10) == 10);
    for (int i = 0; i < 10; i++) {
        assert(rows[i].lab_id == o.labs[i]);
        assert(rows[i].state_hash == want[i].hash);
    }
}

/* La seed ne contient pas la politique. Sur 200 ticks (mutation 5 %) :
 * copier < incrémenter < ((marque + 1) XOR 4). Le calcul complet se sert
 * du stimulus. Seuils mesurés, avec de la marge. */
TEST(test_diagnose_reads_and_computes) {
    PoolEntry seed[POOL_MAX], copy[4], step[4], full[4];
    int n = LABS[lab_index("diagnose")].seeds(seed);
    assert(n == 4 && lab_index("diagnose") == N_CORE_LABS + 6);
    for (int i = 0; i < n; i++)
        for (int k = 0; k < seed[i].len; k++)
            assert(seed[i].dna[k] != OP_SENSE_STIM && seed[i].dna[k] != OP_EMIT);
    double lo = ev4("diagnose", seed, n, NULL);
    for (int i = 0; i < 4; i++) {
        copy[i] = seed[0];
        copy[i].dna[0] = OP_SENSE_STIM; copy[i].dna[1] = OP_EMIT;
        step[i] = seed[0];
        step[i].dna[0] = OP_SENSE_STIM; step[i].dna[1] = OP_INC_A; step[i].dna[2] = OP_EMIT;
        full[i] = seed[0];
        full[i].dna[0] = OP_SENSE_STIM; full[i].dna[1] = OP_INC_A; full[i].dna[2] = OP_MOV_BA;
        full[i].dna[3] = OP_LOADI; full[i].dna[4] = 4; full[i].dna[5] = OP_XOR; full[i].dna[6] = OP_EMIT;
    }
    double mid = ev4("diagnose", copy, 4, NULL);
    double hi = ev4("diagnose", step, 4, NULL);
    double ex[N_EXTRA];
    double top = ev4("diagnose", full, 4, ex);
    assert(hi > mid + 0.10);
    assert(top > 0.58 && top > hi + 0.12);
    assert(ex[4] > 0.18);
    printf("    diagnose seed=%.3f copy=%.3f step=%.3f full=%.3f vs_blind=%+.3f\n", lo, mid, hi, top, ex[4]);
}

double gn_eval(const PoolEntry *g, int n, int held, int level, uint64_t seed, int ntask, int mode, int fam, double *curve);
double gn_train(const PoolEntry *g, int n, uint64_t seed);
/* Ligne à retard : confirme T[décalage] si la récompense est juste, sinon incrémente la conjecture.
 * Absente de la seed. Données en 0x20 (introns). lag2 clé = p2, lag3 clé = p3. */
static const uint8_t LAG2[172] = {
    32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 18, 21, 31, 8, 9, 27,
    28, 32, 6, 0, 6, 1, 6, 2, 6, 3, 6, 4, 6, 5, 6, 6,
    6, 7, 6, 8, 6, 9, 28, 0, 20, 8, 95, 7, 9, 31, 7, 9,
    74, 2, 9, 71, 2, 9, 68, 22, 31, 4, 9, 22, 7, 2, 31, 3,
    21, 28, 5, 23, 6, 79, 7, 4, 31, 3, 21, 28, 36, 23, 6, 0,
    8, 41, 7, 2, 31, 3, 21, 28, 5, 23, 6, 93, 7, 0, 31, 4,
    27, 25, 7, 4, 1, 31, 3, 21, 28, 32, 23, 6, 4, 7, 2, 31,
    3, 21, 28, 5, 23, 6, 122, 7, 4, 6, 0, 7, 1, 31, 3, 21,
    28, 5, 23, 6, 134, 7, 0, 31, 3, 20, 21, 28, 32, 23, 6, 4,
    7, 2, 6, 3, 7, 1, 6, 2, 7, 0, 6, 1, 18, 31, 3, 21,
    28, 32, 23, 6, 0, 7, 9, 1, 6, 9, 14, 15,
};
static const uint8_t LAG3[175] = {
    32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 18, 21, 31, 8, 9, 27,
    28, 32, 6, 0, 6, 1, 6, 2, 6, 3, 6, 4, 6, 5, 6, 6,
    6, 7, 6, 8, 6, 9, 28, 0, 20, 8, 98, 7, 9, 31, 7, 9,
    77, 2, 9, 74, 2, 9, 71, 2, 9, 68, 22, 31, 4, 9, 22, 7,
    3, 31, 3, 21, 28, 5, 23, 6, 82, 7, 4, 31, 3, 21, 28, 36,
    23, 6, 0, 8, 41, 7, 3, 31, 3, 21, 28, 5, 23, 6, 96, 7,
    0, 31, 4, 27, 25, 7, 4, 1, 31, 3, 21, 28, 32, 23, 6, 4,
    7, 3, 31, 3, 21, 28, 5, 23, 6, 125, 7, 4, 6, 0, 7, 2,
    31, 3, 21, 28, 5, 23, 6, 137, 7, 0, 31, 3, 20, 21, 28, 32,
    23, 6, 4, 7, 2, 6, 3, 7, 1, 6, 2, 7, 0, 6, 1, 18,
    31, 3, 21, 28, 32, 23, 6, 0, 7, 9, 1, 6, 9, 14, 15,
};
static PoolEntry gn_prog(const uint8_t *dna, int len) {
    PoolEntry g; memset(&g, 0, sizeof g); g.len = len; memcpy(g.dna, dna, (size_t)len); return g;
}
/* general : la seed apprend les familles qu'elle sait écrire. La note affichée est la mémoire
 * longue, absente de la seed. Une ligne à retard la résout et s'effondre gelée ou sans récompense. */
TEST(test_general_learns_and_holds_out) {
    PoolEntry s[POOL_MAX]; int n = LABS[lab_index("general")].seeds(s);
    assert(n == 4 && lab_index("general") == N_CORE_LABS + 7 && LABS[lab_index("general")].show_extra == 1);
    double cv[2], h = gn_eval(s, 1, 1, 0, 0, 40, 0, -1, cv), fz = gn_eval(s, 1, 1, 0, 0, 40, 1, -1, NULL);
    double nr = gn_eval(s, 1, 1, 0, 0, 40, 2, -1, NULL), tr = gn_eval(s, 1, 0, 0, 7, 32, 0, -1, NULL), best_c = 0;
    for (int v = 0; v < 4; v++) { double c = gn_eval(s, 1, 1, 0, 0, 40, 4 + v, -1, NULL); if (c > best_c) best_c = c; }
    double m2 = gn_eval(s, 1, 1, 0, 0, 40, 0, 6, NULL), m3 = gn_eval(s, 1, 1, 0, 0, 40, 0, 7, NULL);
    double mix = gn_train(s, 1, 7);
    PoolEntry p2 = gn_prog(LAG2, 172), p3 = gn_prog(LAG3, 175);
    assert(memcmp(p2.dna, s[0].dna, 172) != 0);
    double r2 = gn_eval(&p2, 1, 1, 0, 0, 40, 0, 6, NULL), r2f = gn_eval(&p2, 1, 1, 0, 0, 40, 1, 6, NULL);
    double r2n = gn_eval(&p2, 1, 1, 0, 0, 40, 2, 6, NULL), r2x = gn_eval(&p2, 1, 1, 0, 0, 40, 0, 7, NULL);
    double r3 = gn_eval(&p3, 1, 1, 0, 0, 40, 0, 7, NULL), r3f = gn_eval(&p3, 1, 1, 0, 0, 40, 1, 7, NULL);
    double r3n = gn_eval(&p3, 1, 1, 0, 0, 40, 2, 7, NULL);
    printf("    general seed known=%.3f L0=%.3f gelé=%.3f sans_récompense=%.3f cte=%.3f m2=%.3f m3=%.3f mix=%.3f début/fin=%.3f/%.3f\n",
           h, tr, fz, nr, best_c, m2, m3, mix, cv[0], cv[1]);
    printf("    general ligne2=%.3f gelé=%.3f sans_récompense=%.3f autre_lag=%.3f  ligne3=%.3f gelé=%.3f sans_récompense=%.3f\n",
           r2, r2f, r2n, r2x, r3, r3f, r3n);
    assert(h > fz + 0.25 && h > best_c + 0.3 && nr < 0.3 && cv[1] > cv[0] + 0.1 && tr > 0.9);
    assert(m2 < 0.45 && m3 < 0.48 && mix > 0.45 && mix < 0.65);   /* la seed ne sait pas la mémoire longue */
    assert(r2 > 0.90 && r2 > r2f + 0.50 && r2n < 0.40 && r2x < 0.55);
    assert(r3 > 0.90 && r3 > r3f + 0.50 && r3n < 0.40);
    double ex[N_EXTRA]; double f = pool_eval_x(lab_index("general"), s, n, "gn-test", 100, NULL, 0, ex);
    printf("    general sélection f=%.3f (train, non saturé)\n", f);
    assert(f == ex[1] && fabs(f - gn_train(s, 4 < n ? 4 : n, 0)) < 1 && f > 0.40 && f < 0.65);
    RunOpts o = { .ticks = 60, .cycles = 1, .quiet = 1, .no_reload = 1, .dry_run = 1, .data_dir = "/tmp/unused-gn" };
    o.labs[o.nlabs++] = lab_index("general");
    MetricRec r; assert(runner(&o, &r, 1) == 1 && r.fitness == r.extra[0] && r.best_fitness == r.extra[0] && r.extra[1] != r.extra[0]);
    assert(r.extra[0] < 0.55 && r.extra[4] > 0.55);               /* affiché = lag ; known reste haut */
}

/* Critère d'émergence : seul, ≥ 0.95 sur une mémoire longue, et cette mémoire retombe d'au moins
 * 0.5 gelée et sans récompense. La seed ne le passe pas ; les lignes à retard du test, oui. */
TEST(test_general_check_needs_learning) {
    PoolEntry s; seed_entry(&s, "general", 0);
    PoolEntry p2 = gn_prog(LAG2, 172), p3 = gn_prog(LAG3, 175);
    double v[GN_CHECK_N];
    assert(gn_check(&s, v) == 0 && v[0] < 0.45 && v[1] < 0.48);
    assert(gn_check(&p2, v) == 2 && v[0] > 0.95 && v[2] < 0.4 && v[4] < 0.4);
    assert(gn_check(&p3, v) == 3 && v[1] > 0.95 && v[3] < 0.4 && v[5] < 0.4);
    PoolEntry r = p2; r.dna[r.len++] = 0x0f;      /* un génome qui récite sans apprendre : rien */
    for (int i = 0; i < r.len; i++) if (r.dna[i] == OP_STORE) r.dna[i] = OP_NOP;
    assert(gn_check(&r, v) == 0);
    printf("    general check : seed 0, ligne2 2, ligne3 3, sans STORE 0\n");
}

void hl_gn_cfg(WorldConfig *c);
int hl_gn_sel(Lab *l, World *w, PoolEntry *out);
void world_split(World *w, Creature *c);
/* general : l'enfant reçoit le génome de naissance. Le soma réécrit par STORE ne part pas.
 * germline à 0 : l'enfant reçoit le soma, comme les labos historiques. */
TEST(test_general_germline_not_soma) {
    WorldConfig gc = cfg_default();
    hl_gn_cfg(&gc);
    assert(gc.germline == 1 && gc.ops_per_tick == 192);
    const uint8_t prog[] = { 0x1c, 0x21, 0x06, 0x00, 0x0e, 0x0f };   /* LOADI 33 ; STORE 0 ; SPLIT ; YIELD */
    for (int arm = 0; arm < 2; arm++) {
        Rng r; rng_seed(&r, 3);
        WorldConfig c = cfg_default();
        c.germline = arm; c.mut_rate = 0; c.dup_rate = 0; c.max_pop = 8; c.split_energy = 10;
        World *w = world_new(NULL, 0, &r, &c, NULL);
        Creature *p = creature_new(1, 1, prog, 6, 200, 1, 0, -1);
        world_spawn(w, p);
        assert(vm_run(p, w) == 1 && w->nnb == 2);
        Creature *ch = w->nb[1];
        assert(p->dna[0] == 0x21 && p->germ[0] == 0x1c && p->germ_len == 6);
        assert(ch->len == 6 && ch->germ_len == 6 && ch->dna[0] == ch->germ[0]);
        assert(ch->dna[0] == (arm ? 0x1c : 0x21));
        world_free(w);
    }
    PoolEntry seed; seed_entry(&seed, "general", 0);
    PoolEntry bad; memset(&bad, 0, sizeof bad); bad.len = 8;
    Rng r; rng_seed(&r, 9);
    WorldConfig c = cfg_default(); c.germline = 1; c.max_pop = 4;
    World *w = world_new(NULL, 0, &r, &c, NULL);
    Creature *p = creature_new(2, 2, seed.dna, seed.len, 200, 2, 0, -1);
    memset(p->dna, 0xff, (size_t)p->len);          /* soma illisible ; la lignée reste la seed */
    p->lscore = 1;
    w->cs[0] = p; w->ncs = 1;
    Lab l; memset(&l, 0, sizeof l);
    l.pool0 = &bad; l.npool0 = 1; rng_seed(&l.rng0, 1);
    PoolEntry out[POOL_MAX]; int k = hl_gn_sel(&l, w, out);
    int found = 0;
    for (int i = 0; i < k; i++) {
        if (out[i].len == seed.len && !memcmp(out[i].dna, seed.dna, (size_t)seed.len)) found = 1;
        if (out[i].len == seed.len) assert(out[i].dna[0] != 0xff);
    }
    assert(k > 0 && found);
    w->ncs = 0; free(p); world_free(w);
    printf("    germline: enfant = naissance, sélection = lignée\n");
}

double gn_mem(const PoolEntry *g, int n, uint64_t seed);
/* Niche mémoire : deux octets de la seed (0x6b : LOAD 20 au lieu de SENSE_STIM ; AND 3) clés la table
 * par indice sur l'essai précédent. T[p1] monte, l'association tombe : la sélection du curriculum le
 * jette, l'échelle mémoire le met en tête du pool, devant la seed. */
TEST(test_general_memory_niche) {
    PoolEntry s; seed_entry(&s, "general", 0);
    PoolEntry r = s; r.dna[0x6b] = OP_LOAD; r.dna[0x6c] = 20;
    double m1s = gn_eval(&s, 1, 1, 0, 0, 40, 0, 3, NULL), m1r = gn_eval(&r, 1, 1, 0, 0, 40, 0, 3, NULL);
    double ms = gn_mem(&s, 1, 5), mr = gn_mem(&r, 1, 5), ts = gn_train(&s, 1, 5), tr = gn_train(&r, 1, 5);
    printf("    niche : mémoire-1 seed=%.3f deux_octets=%.3f  échelle %.3f/%.3f  sélection %.3f/%.3f\n", m1s, m1r, ms, mr, ts, tr);
    assert(m1s < 0.6 && m1r > 0.85 && mr > ms + 0.1 && tr < ts);
    WorldConfig c = cfg_default(); c.germline = 1; c.max_pop = 4;
    Rng rg; rng_seed(&rg, 4);
    World *w = world_new(NULL, 0, &rg, &c, NULL);
    Creature *a = creature_new(1, 1, s.dna, s.len, 200, 1, 0, -1), *b = creature_new(3, 3, r.dna, r.len, 200, 2, 0, -1);
    a->lscore = 1; b->lscore = 0;                   /* le monde préfère la seed ; la niche, non */
    w->cs[0] = a; w->cs[1] = b; w->ncs = 2;
    Lab l; memset(&l, 0, sizeof l);
    l.pool0 = &s; l.npool0 = 1; rng_seed(&l.rng0, 2);
    PoolEntry out[POOL_MAX]; int k = hl_gn_sel(&l, w, out);
    assert(k == 2 && out[0].len == r.len && !memcmp(out[0].dna, r.dna, (size_t)r.len));
    assert(!memcmp(out[1].dna, s.dna, (size_t)s.len));
    w->ncs = 0; free(a); free(b); world_free(w);
}

int stagger_cell(uint8_t *dna, int n);
int retarget_prev(uint8_t *dna, int n);
/* Le second registre est une réécriture de même longueur, puis un LOAD qui lit la case d'avant.
 * Mémoire-1 du décalage seul = 1. Mémoire-2 du re-ciblage monte, mémoire-1 tombe : l'échelle
 * le garde, la moyenne des trois barreaux le jetterait. */
TEST(test_general_second_rung_kept) {
    PoolEntry s; seed_entry(&s, "general", 0);
    PoolEntry reg = s; reg.dna[0x6b] = OP_LOAD; reg.dna[0x6c] = 20;
    PoolEntry stag = reg;
    assert(stagger_cell(stag.dna, stag.len) == 1);
    assert(stagger_cell(stag.dna, stag.len) == 0);          /* déjà décalé : rien à refaire */
    assert(stag.dna[0x8b] == OP_MOV_BA && stag.dna[0x8c] == OP_LOAD && stag.dna[0x8d] == 20);
    assert(stag.dna[0x8e] == OP_STORE && stag.dna[0x8f] == 19);
    double p1 = gn_eval(&stag, 1, 1, 0, 0, 16, 0, 3, NULL);
    PoolEntry prev = stag;
    assert(retarget_prev(prev.dna, prev.len) == 1);
    assert(prev.dna[0x6c] == 19 && prev.dna[0x78] == 20 && prev.dna[0x8d] == 20);
    PoolEntry both = prev;                                  /* un second passage re-cible l'autre LOAD */
    assert(retarget_prev(both.dna, both.len) == 1 && both.dna[0x78] == 19);
    assert(retarget_prev(both.dna, both.len) == 0);
    double p2s = gn_eval(&stag, 1, 1, 0, 0, 24, 0, 6, NULL);
    double p2r = gn_eval(&prev, 1, 1, 0, 0, 24, 0, 6, NULL);
    double ms = 0, mr = 0;
    for (int i = 0; i < 4; i++) { ms += gn_mem(&stag, 1, (uint64_t)i); mr += gn_mem(&prev, 1, (uint64_t)i); }
    printf("    second barreau : décalage p1=%.3f  re-cible p2=%.3f (avant %.3f)  échelle %.3f > %.3f\n",
           p1, p2r, p2s, mr / 4, ms / 4);
    assert(p1 > 0.95 && p2r > p2s + 0.08 && mr > ms + 0.8);
    WorldConfig c = cfg_default(); c.germline = 1; c.max_pop = 4;
    Rng rg; rng_seed(&rg, 4);
    World *w = world_new(NULL, 0, &rg, &c, NULL);
    Creature *a = creature_new(1, 1, stag.dna, stag.len, 200, 1, 0, -1);
    Creature *b = creature_new(3, 3, prev.dna, prev.len, 200, 2, 0, -1);
    a->lscore = 1; b->lscore = 0;                           /* le monde préfère le registre parfait */
    w->cs[0] = a; w->cs[1] = b; w->ncs = 2;
    Lab l; memset(&l, 0, sizeof l);
    l.pool0 = &stag; l.npool0 = 1; rng_seed(&l.rng0, 3);
    PoolEntry out[POOL_MAX]; int k = hl_gn_sel(&l, w, out);
    int kept = 0;
    for (int i = 0; i < k; i++) if (out[i].len == prev.len && !memcmp(out[i].dna, prev.dna, (size_t)prev.len)) kept = 1;
    assert(k >= 2 && kept);
    w->ncs = 0; free(a); free(b); world_free(w);
}

/* L'ADD qui suit le LOAD de la case retardée mélange l'indice courant à p2. À la naissance,
 * unmix_prev en fait un intron (0xcf). Sur la lignée graine 2 (mémoire-2 0,530, code 3),
 * ce seul geste donne le critère. Sans décalage, rien à réécrire ; taux 0 hors general. */
TEST(test_general_unmix_at_birth) {
    PoolEntry s; seed_entry(&s, "general", 0);
    PoolEntry plain = s;
    assert(unmix_prev(plain.dna, plain.len) == 0 && !memcmp(plain.dna, s.dna, (size_t)s.len));
    PoolEntry g; memset(&g, 0, sizeof g);
    FILE *f = fopen("experiments/general-transfer/seed2-pool3.adn", "rb");
    assert(f); g.len = (int)fread(g.dna, 1, MAX_LEN, f); fclose(f);
    assert(g.len == 172 && g.dna[0x78] == 19 && g.dna[0x79] == OP_ADD);
    double v[GN_CHECK_N];
    assert(gn_check(&g, v) == 0);
    assert(unmix_prev(g.dna, g.len) == 1 && g.dna[0x79] == 0xcf);
    assert(unmix_prev(g.dna, g.len) == 0);                  /* déjà démêlé */
    assert(gn_check(&g, v) == 2 && v[0] > 0.95 && v[2] < v[0] - 0.5 && v[4] < v[0] - 0.5);
    WorldConfig gc = cfg_default(); assert(gc.unmix_rate == 0);
    hl_gn_cfg(&gc); assert(gc.unmix_rate > 0 && gc.unmix_rate <= 0.2);
    printf("    démêlage : graine 2 + intron 0x79  m2=%.3f gelé=%.3f sans_récompense=%.3f\n", v[0], v[2], v[4]);
}

/* Second étage à la naissance. La case 0x10 (intron, hors des adresses 0..7 que
 * la vie réécrit) reçoit l'ancienne case 19 avant qu'elle soit écrasée. La clé
 * la lit. Sans le motif, rien ne change. La graine 0 du pool a un AND 0 à la
 * place du AND 3 : pas de capture de l'indice, donc pas de réécriture. */
TEST(test_general_deepen_at_birth) {
    PoolEntry s; seed_entry(&s, "general", 0);
    PoolEntry plain = s;
    assert(deepen_prev(plain.dna, plain.len) == 0 && !memcmp(plain.dna, s.dna, (size_t)s.len));
    PoolEntry z; memset(&z, 0, sizeof z);
    FILE *fz = fopen("experiments/general-transfer/seed0-pool12.adn", "rb");
    assert(fz); z.len = (int)fread(z.dna, 1, MAX_LEN, fz); fclose(fz);
    PoolEntry z0 = z;
    assert(z.len == 172 && deepen_prev(z.dna, z.len) == 0 && !memcmp(z.dna, z0.dna, (size_t)z.len));
    PoolEntry g; memset(&g, 0, sizeof g);
    FILE *f = fopen("experiments/general-transfer/seed2-pool3.adn", "rb");
    assert(f); g.len = (int)fread(g.dna, 1, MAX_LEN, f); fclose(f);
    assert(g.len == 172 && g.dna[0x10] >= OP_COUNT && g.dna[0x78] == 19);
    assert(deepen_prev(g.dna, g.len) == 1 && g.dna[0x78] == 0x10 && g.dna[0x79] == 0xcf);
    assert(g.dna[0x6e] == OP_MOV_BA && g.dna[0x7c] == OP_INC_A && g.len == 172);
    assert(deepen_prev(g.dna, g.len) == 0);
    double v[GN_CHECK_N];
    assert(gn_check(&g, v) == 3 && v[1] > 0.95 && v[3] < v[1] - 0.5 && v[5] < v[1] - 0.5);
    WorldConfig gc = cfg_default(); assert(gc.deepen_rate == 0);
    hl_gn_cfg(&gc); assert(gc.deepen_rate > 0 && gc.deepen_rate <= 0.2);
    printf("    second étage : graine 2  m3=%.3f gelé=%.3f sans_récompense=%.3f\n", v[1], v[3], v[5]);
}

TEST(test_general_regs_push_peek) {
    /* LOADI 7 ; RPUSH 12 ; LOADI 9 ; RPUSH 13 ; RPEEK 1 ; STORE 14 ; YIELD. Avec cfg.regs, RPUSH écrit
     * comme STORE et pousse A ; RPEEK 1 relit l'avant-dernier poussé. Sans cfg.regs, F0/F1 sont des introns. */
    uint8_t g[16] = { 0x1c, 0x07, 0xf0, 0x0c, 0x1c, 0x09, 0xf0, 0x0d, 0xf1, 0x05, 0x06, 0x0e, 0x0f, 0, 0, 0 };
    for (int regs = 0; regs <= 1; regs++) {
        Rng r; rng_seed(&r, 0);
        WorldConfig cfg = cfg_default(); cfg.regs = regs;
        World *w = world_new(NULL, 0, &r, &cfg, NULL);
        Creature *c = creature_new(5, 5, g, 16, 200, 0, 0, -1);
        w->cs[w->ncs++] = c; w->occ[5][5]++;
        vm_run(c, w);
        if (regs) assert(c->dna[12] == 7 && c->dna[13] == 9 && c->dna[14] == 7 && c->r[0] == 9 && c->r[1] == 7);
        else assert(c->dna[14] != 7 && c->r[0] == 0 && c->r[1] == 0);
        world_free(w);
    }
    WorldConfig gc = cfg_default(); hl_gn_cfg(&gc);
    assert(gc.regs == 1 && cfg_default().regs == 0);
}

TEST(test_novelty_map_elites) {
    /* experiments/novelty-map-elites : descripteurs bornés et déterministes, cases dans la grille,
     * archive reproductible, grille 1 case = hill-climbing élitiste (une seule élite). */
    PoolEntry g; seed_entry(&g, "general", 0);
    double d[GN_BEH_N], e[GN_BEH_N];
    gn_behavior(&g, d); gn_behavior(&g, e);
    assert(!memcmp(d, e, sizeof d));
    assert(d[0] >= 0 && d[0] <= log2(5.0) + 1e-9 && d[1] >= 0 && d[1] <= 1);
    for (int L = 0; L < 4; L++) assert(d[2 + L] >= -1e-9 && d[2 + L] <= 2 + 1e-9);
    for (int gr = 0; gr < 3; gr++) { int c = nov_cell(gr, d); assert(c >= 0 && c < nov_ncells(gr)); }
    NovRun r = { .grid = 1, .iters = 12, .report = 12, .init_mut = 4, .seed = 3 };
    NovResult a, b;
    assert(nov_run(&r, NULL, &a) == 0 && nov_run(&r, NULL, &b) == 0);
    assert(a.hash == b.hash && a.cells >= 1 && a.inserts >= 1 && a.max_m2 > 0.3 && a.passers == 0);
    r.grid = 0;
    assert(nov_run(&r, NULL, &a) == 0 && a.cells == 1 && a.ncells == 1);
}

TEST(test_soup_detector_controls) {
    /* experiments/soup-replicator : les deux réplicateurs faits main sont reconnus comme copieurs,
     * l'ablation retrouve leur cœur (22 octets en adressage d'origine, 9 en adressage B),
     * le bruit et le témoin sans STORE ne copient pas, et une soupe est déterministe. */
    static const int ess_want[2] = { 22, 9 };
    for (int m = 0; m < 2; m++) {
        uint8_t p[SOUP_PROG], e[SOUP_PROG]; SoupCfg c = { m, 0, 0 }, ns = { m, 1, 0 };
        soup_handmade(p, m);
        assert(soup_copier(p, &c) != 0 && soup_copier(p, &ns) == 0);
        assert(soup_ablate(p, &c, e) == ess_want[m] && e[2] && !e[40]);
        Rng r; rng_seed(&r, 7); for (int i = 0; i < SOUP_PROG; i++) p[i] = (uint8_t)rng_next(&r);
        assert(soup_copier(p, &c) == 0);
    }
    SoupRun r = { .nprog = 256, .epochs = 64, .report = 64, .inject = 8, .mut = 0.00024 };
    SoupResult a, b;
    assert(soup_run(&r, NULL, &a) == 0 && soup_run(&r, NULL, &b) == 0);
    assert(a.hash == b.hash && a.births > 0 && a.copiers_final > 128 && a.t_first == 64);
    r.vm.no_store = 1;
    assert(soup_run(&r, NULL, &a) == 0 && a.births == 0 && a.copiers_final == 0 && a.t_first == -1);
}

/* dialog-mem : la seed-amorce (emit-dialog, s_t + 1) ne contient pas la mémoire ; un génome
 * qui garde s_{t-1} + 1 dans A et l'émet avant de relire le stimulus passe le plafond sans mémoire,
 * et retombe au hasard quand on remplace s_{t-1} par un faux (baseline). Seuils mesurés, avec marge. */
TEST(test_dialog_mem_needs_memory) {
    PoolEntry seed[POOL_MAX], mem[4];
    int id = lab_index("dialog-mem"), n = LABS[id].seeds(seed);
    assert(id == N_LABS - 1 && n == 4);
    double es[N_EXTRA], em[N_EXTRA];
    double fs = ev4("dialog-mem", seed, n, es);
    raw_pool(mem, "0d1412010e0f1e");          /* EAT EMIT SENSE_STIM INC_A SPLIT YIELD LOOP */
    double fm = ev4("dialog-mem", mem, 4, em);
    printf("    dialog-mem : seed %.3f (exact %.3f)  mémoire %.3f (exact %.3f, baseline %.3f)\n", fs, es[1], fm, em[1], em[3]);
    assert(fs < 0.40 && es[1] < 0.25);
    assert(fm > 0.90 && em[1] > 0.90 && em[3] < 0.40);
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
    RUN(test_sixteen_labs_and_core_untouched);
    RUN(test_evo_params_bounded);
    RUN(test_new_labs_beat_naive_baselines);
    RUN(test_new_labs_exploits_closed);
    RUN(test_resume_legacy_checkpoints_with_16_labs);
    RUN(test_contribution_outranks_energy);
    RUN(test_selfevo_seed_keeps_a_mutation_rate);
    RUN(test_foreign_admits_only_positive_gap);
    RUN(test_code_repair_pool_beats_flips);
    RUN(test_core_state_hash_matches_main);
    RUN(test_diagnose_reads_and_computes);
    RUN(test_general_learns_and_holds_out);
    RUN(test_general_germline_not_soma);
    RUN(test_general_check_needs_learning);
    RUN(test_general_memory_niche);
    RUN(test_general_second_rung_kept);
    RUN(test_general_unmix_at_birth);
    RUN(test_general_deepen_at_birth);
    RUN(test_general_regs_push_peek);
    RUN(test_novelty_map_elites);
    RUN(test_soup_detector_controls);
    RUN(test_dialog_mem_needs_memory);
    printf("\n%d/%d tests OK\n", passed, 41);
    return 0;
}
