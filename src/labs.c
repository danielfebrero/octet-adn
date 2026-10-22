/* Les 10 éprouvettes : même moteur, objectifs (fitness) différents. */
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "octet.h"

/* ---------------- seeds embarquées ---------------- */
#define EXT(s) extern const uint8_t s[], s##_end[];
EXT(adn_forager) EXT(adn_neural) EXT(adn_echo) EXT(adn_dialog) EXT(adn_stock) EXT(adn_social) EXT(adn_predator) EXT(adn_walk)
EXT(adn_fold) EXT(adn_antibiotic) EXT(adn_commons) EXT(adn_pandemic) EXT(adn_repair) EXT(adn_selfevo) EXT(adn_diagnose) EXT(adn_general)
#define BLOB(n, s) { n, s, 0 }
static SeedBlob seeds_tab[] = {
    BLOB("forager", adn_forager), BLOB("neural", adn_neural), BLOB("echo", adn_echo),
    BLOB("dialog", adn_dialog), BLOB("stock", adn_stock), BLOB("social", adn_social),
    BLOB("predator", adn_predator), BLOB("walk", adn_walk),
    BLOB("fold", adn_fold), BLOB("antibiotic", adn_antibiotic), BLOB("commons", adn_commons), BLOB("pandemic", adn_pandemic), BLOB("repair", adn_repair), BLOB("selfevo", adn_selfevo),
    BLOB("diagnose", adn_diagnose), BLOB("general", adn_general),
};
static const uint8_t *seed_ends[] = { adn_forager_end, adn_neural_end, adn_echo_end, adn_dialog_end,
                                      adn_stock_end, adn_social_end, adn_predator_end, adn_walk_end,
                                      adn_fold_end, adn_antibiotic_end, adn_commons_end, adn_pandemic_end, adn_repair_end, adn_selfevo_end,
                                      adn_diagnose_end, adn_general_end };
const int N_SEEDS = sizeof seeds_tab / sizeof seeds_tab[0];
const SeedBlob *seed_at(int i) { return (i >= 0 && i < N_SEEDS) ? seed_get(seeds_tab[i].name) : NULL; }
const SeedBlob *seed_get(const char *name) {
    for (int i = 0; i < N_SEEDS; i++)
        if (!strcmp(seeds_tab[i].name, name)) {
            seeds_tab[i].len = (int)(seed_ends[i] - seeds_tab[i].p);
            return &seeds_tab[i];
        }
    return NULL;
}
void seed_entry(PoolEntry *e, const char *name, int species) {
    const SeedBlob *s = seed_get(name);
    e->species = species; e->len = s->len;
    memcpy(e->dna, s->p, (size_t)s->len);
}

/* ---------------- helpers ---------------- */
static double pop_ticks(const Lab *l) {
    long s = 0;
    for (int i = 0; i < l->npops; i++) s += l->pops[i];
    return s > 0 ? (double)s : 1.0;
}
static double mean_pop(const Lab *l) {
    long s = 0;
    for (int i = 0; i < l->npops; i++) s += l->pops[i];
    return l->npops ? (double)s / l->npops : 0.0;
}
static void rank_default(const Creature *c, double k[2]) { k[0] = c->e; k[1] = c->age; }
static void sort_by_rank(Creature **v, int n, void (*rank)(const Creature *, double[2])) {
    double (*key)[2] = malloc(sizeof(double[2]) * (size_t)(n ? n : 1));
    for (int i = 0; i < n; i++) rank(v[i], key[i]);
    for (int i = 1; i < n; i++) {               /* tri par insertion stable, décroissant */
        Creature *c = v[i]; double k0 = key[i][0], k1 = key[i][1]; int j = i - 1;
        while (j >= 0 && (key[j][0] < k0 || (key[j][0] == k0 && key[j][1] < k1))) {
            v[j + 1] = v[j]; key[j + 1][0] = key[j][0]; key[j + 1][1] = key[j][1]; j--;
        }
        v[j + 1] = c; key[j + 1][0] = k0; key[j + 1][1] = k1;
    }
    free(key);
}
static void entry_from(PoolEntry *e, const Creature *c) {
    e->species = c->species; e->len = c->len; memcpy(e->dna, c->dna, (size_t)c->len);
}
static int same(const uint8_t *a, int la, const uint8_t *b, int lb) {
    return la == lb && !memcmp(a, b, (size_t)la);
}
int hamming128(const uint8_t *a, int la, const uint8_t *b, int lb) {
    int na = la < 128 ? la : 128, nb = lb < 128 ? lb : 128, n = na < nb ? na : nb, d = 0;
    for (int i = 0; i < n; i++) d += a[i] != b[i];
    return d + abs(na - nb);
}
static int one_seed(PoolEntry *o, const char *n) { seed_entry(o, n, 0); return 1; }

/* ---------------- configs ---------------- */
static void cfg_poor(WorldConfig *c) { c->food_density = 0.2; c->food_regrow = 2; }
static void cfg_org(WorldConfig *c) { c->crossover = 1; c->mut_rate = 0.025; }
/* Prédateurs mortels : sans ça deux prédateurs « immortels » (ATTACK vole 30, coûte 1) bloquent
 * la fitness à 0.4 sans jamais se reproduire. Ils paient un métabolisme, vieillissent et ATTACK coûte. */
static void cfg_pp(WorldConfig *c) { c->pred_metab = PP_PRED_METAB; c->pred_max_age = PP_PRED_MAX_AGE; c->attack_cost = PP_ATTACK_COST; }

/* ---------------- seeds par labo ---------------- */
static int s_forager(PoolEntry *o) { return one_seed(o, "forager"); }
static int s_echo(PoolEntry *o) { return one_seed(o, "echo"); }
static int s_dialog(PoolEntry *o) { return one_seed(o, "dialog"); }
static int s_walk(PoolEntry *o) { return one_seed(o, "walk"); }
static int s_stock(PoolEntry *o) { return one_seed(o, "stock"); }
static int s_org(PoolEntry *o) {
    const char *n[] = { "forager", "neural", "echo", "social", "predator" };
    for (int i = 0; i < 5; i++) seed_entry(&o[i], n[i], 0);
    return 5;
}
static int s_pp(PoolEntry *o) {
    for (int i = 0; i < 3; i++) seed_entry(&o[i], "forager", 0);
    seed_entry(&o[3], "predator", 1);
    return 4;
}

/* ---------------- max-pop / survival / energy-eff / self-rewrite ---------------- */
static double f_maxpop(Lab *l, World *w, int t) {
    (void)w; (void)t;
    int peak = 0;
    for (int i = 0; i < l->npops; i++) if (l->pops[i] > peak) peak = l->pops[i];
    l->extra[0] = peak;
    return mean_pop(l) + 0.1 * peak;
}
static int cmp_int(const void *a, const void *b) { return *(const int *)a - *(const int *)b; }
static double f_survival(Lab *l, World *w, int ticks) {
    double alive_frac = fmin(1.0, (double)l->npops / (ticks > 0 ? ticks : 1));
    double med = 0; long age_alive = 0;
    if (w->ncs) {
        int *ages = malloc(sizeof(int) * (size_t)w->ncs);
        for (int i = 0; i < w->ncs; i++) { ages[i] = w->cs[i]->age; age_alive += ages[i]; }
        qsort(ages, (size_t)w->ncs, sizeof(int), cmp_int);
        med = w->ncs & 1 ? ages[w->ncs / 2] : (ages[w->ncs / 2 - 1] + ages[w->ncs / 2]) / 2.0;
        free(ages);
    }
    double lifespan = (double)(w->st.death_age_sum + age_alive) / (w->st.born > 0 ? w->st.born : 1);
    l->extra[0] = med;
    l->extra[1] = w->st.dead ? (double)w->st.death_age_sum / w->st.dead : 0;
    l->extra[2] = lifespan;
    return lifespan * alive_frac;
}
static void r_age(const Creature *c, double k[2]) { k[0] = c->age; k[1] = c->e; }
static double f_energy(Lab *l, World *w, int t) {
    (void)t; l->extra[0] = (double)w->st.energy_eaten;
    return w->st.energy_eaten / pop_ticks(l);
}
static void r_energy(const Creature *c, double k[2]) { k[0] = (double)c->eaten / (c->age > 1 ? c->age : 1); k[1] = c->e; }
static double f_rewrite(Lab *l, World *w, int t) {
    (void)t; l->extra[0] = (double)w->st.store_writes; l->extra[1] = (double)w->st.useful_stores;
    return w->st.useful_stores / pop_ticks(l);
}
static void r_rewrite(const Creature *c, double k[2]) { k[0] = c->useful_stores; k[1] = c->e; }

/* ---------------- labos de communication (EMIT récompensé en énergie) ---------------- */
static void emit_score(Lab *l, World *w, int target, int reward, int penalty) {
    for (int i = 0; i < w->nem; i++) {
        Creature *c = w->em[i].c;
        l->total++;
        if (w->em[i].v == target) { l->correct++; c->e = c->e + reward > 220 ? 220 : c->e + reward; }
        else c->e -= penalty;
    }
}
static double f_emit(Lab *l, World *w, int t) {
    (void)w; (void)t;
    l->extra[0] = (double)l->correct; l->extra[1] = (double)l->total;
    l->extra[2] = (double)l->correct / (l->total ? l->total : 1);
    return l->correct / pop_ticks(l);                 /* précision × couverture */
}
static void pre_echo(Lab *l, World *w) {
    (void)l;
    if (w->t % 4 == 1) w->stimulus = (int)rng_below(w->rng, 16);   /* nouveau symbole / 4 ticks */
}
static void post_echo(Lab *l, World *w) { emit_score(l, w, w->stimulus, 6, 2); }
/* Protocole : hello 0x00 → ACK 0xAC ; s (1..14) → s+1 ; bye 0x0F → 0x00. */
int dialog_reply(int s) { return s == 0x00 ? 0xAC : s == 0x0F ? 0x00 : s + 1; }
static void pre_dialog(Lab *l, World *w) {
    if (l->seq_pos >= l->nseq) {
        l->nseq = 0; l->seq_pos = 0;
        l->seq[l->nseq++] = 0x00;
        for (int i = 0; i < 5; i++) l->seq[l->nseq++] = (uint8_t)rng_int(w->rng, 1, 14);
        l->seq[l->nseq++] = 0x0F;
    }
    if (w->t % 3 == 1) w->stimulus = l->seq[l->seq_pos++];
}
static void post_dialog(Lab *l, World *w) { emit_score(l, w, dialog_reply(w->stimulus), 6, 2); }

/* ---------------- labos de prédiction : EMIT au tick t = prédiction de t+1 ---------------- */
static const double *g_csv; static int g_ncsv;
void stock_set_csv(const double *closes, int n) { g_csv = closes; g_ncsv = n; }

static int next_walk(Lab *l) {                         /* AR(2) bruitée, 8 bins */
    double x = 0.75 * l->ar0 - 0.2 * l->ar1 + rng_gauss(&l->series, 0, 1.0);
    l->ar1 = l->ar0; l->ar0 = x;
    int b = (int)lround(x + 3.5);
    return b < 0 ? 0 : b > 7 ? 7 : b;
}
static int next_stock(Lab *l) {                        /* GBM synthétique ou CSV local */
    double ret;
    if (l->csv && l->ncsv > 1) {
        int i = l->csv_i % (l->ncsv - 1);
        l->csv_i++;
        ret = log(l->csv[i + 1] / l->csv[i]);
    } else ret = rng_gauss(&l->series, 0.0002, 0.01);
    l->px *= exp(ret);
    return ret > 0 ? 1 : 0;
}
static void predict_pre(Lab *l, World *w, int stock) {
    if (!l->series_init) { rng_seed(&l->series, rng_next(w->rng)); l->series_init = 1; l->px = 100.0;
                           l->csv = g_csv; l->ncsv = g_ncsv; }
    int v = stock ? next_stock(l) : next_walk(l);
    int reward = stock ? 3 : 4, penalty = stock ? 3 : 2;
    for (int i = 0; i < l->npend; i++) {
        int pred = l->pend_v[i];
        int ok = stock ? ((pred & 1) == v) : (pred == v);
        Creature *c = NULL;
        for (int j = 0; j < w->ncs; j++) if (w->cs[j]->id == l->pend_id[i]) { c = w->cs[j]; break; }
        l->total++;
        if (ok) { l->correct++; if (c) c->e = c->e + reward > 220 ? 220 : c->e + reward; }
        else if (c) c->e -= penalty;
    }
    if (l->has_prev) {                                 /* baseline : persistance / momentum */
        l->base_total++;
        l->base_correct += stock ? ((l->prev & 1) == v) : (l->prev == v);
    }
    w->stimulus = v;
    l->prev = v; l->has_prev = 1;
    l->npend = 0;
}
static void pre_walk(Lab *l, World *w) { predict_pre(l, w, 0); }
static void pre_stock(Lab *l, World *w) { predict_pre(l, w, 1); }
static void post_predict(Lab *l, World *w) {
    l->npend = 0;
    for (int i = 0; i < w->nem && i < CMAX; i++) {
        l->pend_id[l->npend] = w->em[i].c->id; l->pend_v[l->npend] = w->em[i].v; l->npend++;
    }
}
static double f_predict(Lab *l, World *w, int t) {
    (void)w; (void)t;
    double acc = (double)l->correct / (l->total ? l->total : 1);
    l->extra[0] = (double)l->correct; l->extra[1] = (double)l->total; l->extra[2] = acc;
    l->extra[3] = (double)l->base_correct / (l->base_total ? l->base_total : 1);
    l->extra[4] = (l->csv && l->ncsv > 1) ? 1 : 0;
    l->extra[5] = acc - l->extra[3];                  /* > 0 : bat la persistance (répéter la dernière valeur) */
    return l->total >= 30 ? acc : 0.0;
}

/* ---------------- predator-prey ---------------- */
static void post_pp(Lab *l, World *w) {
    int p = 0, d = 0;
    for (int i = 0; i < w->ncs; i++)
        if (w->cs[i]->e > 0) { if (w->cs[i]->species == 0) p++; else d++; }
    if (l->prey) { l->prey[w->t - 1] = p; l->pred[w->t - 1] = d; }
}
static double f_pp(Lab *l, World *w, int ticks) {
    int n = ticks > 0 ? ticks : 1, both = 0; long sp = 0, sd = 0;
    for (int i = 0; i < l->npops; i++) { both += l->prey[i] > 0 && l->pred[i] > 0; sp += l->prey[i]; sd += l->pred[i]; }
    double mp = (double)sp / n, md = (double)sd / n;
    l->extra[0] = mp; l->extra[1] = md; l->extra[2] = (double)w->st.attacks;
    return (double)both / n * fmin(1.0, mp / 20) * fmin(1.0, md / 5);
}
static int sel_pp(Lab *l, World *w, PoolEntry *out) {
    (void)l;
    Creature *prey[CMAX], *pred[CMAX]; int np = 0, nd = 0, n = 0;
    for (int i = 0; i < w->ncs; i++) (w->cs[i]->species == 0 ? (prey[np++] = w->cs[i]) : (pred[nd++] = w->cs[i]));
    sort_by_rank(prey, np, rank_default); sort_by_rank(pred, nd, rank_default);
    int k = nd < POOL_SIZE / 4 ? nd : POOL_SIZE / 4;
    for (int i = 0; i < k; i++) entry_from(&out[n++], pred[i]);
    for (int i = 0; i < np && i < POOL_SIZE - k; i++) entry_from(&out[n++], prey[i]);
    while (k < 2 && n < POOL_MAX) { seed_entry(&out[n++], "predator", 1); k++; }   /* immigration */
    for (int i = np; i < 4 && n < POOL_MAX; i++) seed_entry(&out[n++], "forager", 0);   /* immigration proies (< 4) */
    return n;
}

/* ---------------- org-seeds : méta-labo qui croise les champions des autres ---------------- */
static double f_org(Lab *l, World *w, int t) {
    (void)t;
    if (!w->ncs) return 0.0;
    Creature *reps[CMAX]; int nr = 0, uniq = 0;
    for (int i = 0; i < w->ncs; i++) {
        Creature *c = w->cs[i]; int novel = 1, dup = 0;
        for (int j = 0; j < nr; j++) if (hamming128(c->dna, c->len, reps[j]->dna, reps[j]->len) <= 6) { novel = 0; break; }
        if (novel) reps[nr++] = c;
        for (int j = 0; j < i; j++) if (same(c->dna, c->len, w->cs[j]->dna, w->cs[j]->len)) { dup = 1; break; }
        uniq += !dup;
    }
    l->extra[0] = nr; l->extra[1] = uniq; l->extra[2] = (double)w->st.crossovers;
    return nr * ((double)uniq / w->ncs);
}
static int sel_org(Lab *l, World *w, PoolEntry *out) {
    (void)l;
    Creature *v[CMAX]; int n = w->ncs, k = 0;
    memcpy(v, w->cs, sizeof(Creature *) * (size_t)n);
    sort_by_rank(v, n, rank_default);
    for (int i = 0; i < n && k < POOL_SIZE; i++) {     /* un représentant par lignée d'abord */
        int novel = 1;
        for (int j = 0; j < k; j++) if (hamming128(v[i]->dna, v[i]->len, out[j].dna, out[j].len) <= 6) { novel = 0; break; }
        if (novel) { entry_from(&out[k], v[i]); out[k].species = 0; k++; }
    }
    for (int i = 0; i < n && k < POOL_SIZE; i++) {
        int dup = 0;
        for (int j = 0; j < k; j++) if (same(v[i]->dna, v[i]->len, out[j].dna, out[j].len)) { dup = 1; break; }
        if (!dup) { entry_from(&out[k], v[i]); out[k].species = 0; k++; }
    }
    return k;
}

/* labos humanité (src/labs_h.c) */
void hl_rank_score(const Creature *c, double k[2]);
void hl_fold_cfg(WorldConfig *c); int hl_fold_seeds(PoolEntry *o); double hl_fold_fit(Lab *l, World *w, int t); void hl_fold_initp(Lab *l, World *w);
void hl_ab_cfg(WorldConfig *c); int hl_ab_seeds(PoolEntry *o); void hl_ab_init(Lab *l, World *w); void hl_ab_pre(Lab *l, World *w);
void hl_ab_post(Lab *l, World *w); double hl_ab_fit(Lab *l, World *w, int t); int hl_ab_sel(Lab *l, World *w, PoolEntry *out);
void hl_cm_cfg(WorldConfig *c); int hl_cm_seeds(PoolEntry *o); void hl_cm_init(Lab *l, World *w); void hl_cm_pre(Lab *l, World *w); double hl_cm_fit(Lab *l, World *w, int t);
int hl_pd_seeds(PoolEntry *o); void hl_pd_rank(const Creature *c, double k[2]); void hl_pd_init(Lab *l, World *w); void hl_pd_post(Lab *l, World *w); double hl_pd_fit(Lab *l, World *w, int t);
int hl_cr_seeds(PoolEntry *o); void hl_cr_init(Lab *l, World *w); double hl_cr_fit(Lab *l, World *w, int t); void hl_cr_rank(const Creature *c, double k[2]);
void hl_se_cfg(WorldConfig *c); int hl_se_seeds(PoolEntry *o); void hl_se_init(Lab *l, World *w); void hl_se_pre(Lab *l, World *w);
void hl_se_post(Lab *l, World *w); double hl_se_fit(Lab *l, World *w, int t);
void hl_dg_cfg(WorldConfig *c); int hl_dg_seeds(PoolEntry *o); void hl_dg_init(Lab *l, World *w); void hl_dg_pre(Lab *l, World *w);
void hl_dg_post(Lab *l, World *w); double hl_dg_fit(Lab *l, World *w, int t); int hl_dg_sel(Lab *l, World *w, PoolEntry *out);
void hl_gn_cfg(WorldConfig *c); int hl_gn_seeds(PoolEntry *o); void hl_gn_init(Lab *l, World *w); void hl_gn_pre(Lab *l, World *w); double hl_gn_fit(Lab *l, World *w, int t); int hl_gn_sel(Lab *l, World *w, PoolEntry *out);

/* ---------------- registre ---------------- */
const LabDef LABS[N_LABS] = {
    { "org-seeds", "Organiser d'autres seeds",
      "lignées distinctes viables (clusters Hamming) × diversité (génomes uniques / pop)",
      { "clusters", "unique", "crossovers" }, cfg_org, s_org, NULL, NULL, f_org, rank_default, sel_org, NULL, 0, 0 },
    { "max-pop", "Population la plus haute", "population moyenne sur le cycle (+10 % du pic)",
      { "peak" }, NULL, s_forager, NULL, NULL, f_maxpop, rank_default, NULL, NULL, 0, 0 },
    { "survival", "Longévité", "espérance de vie moyenne × fraction du cycle survécue (monde pauvre)",
      { "median_age", "mean_death_age", "mean_lifespan" }, cfg_poor, s_forager, NULL, NULL, f_survival, r_age, NULL, NULL, 0, 0 },
    { "energy-eff", "Efficacité métabolique", "énergie mangée / créature-tick",
      { "energy_eaten" }, NULL, s_forager, NULL, NULL, f_energy, r_energy, NULL, NULL, 0, 0 },
    { "self-rewrite", "Auto-réécriture utile", "STORE utiles (codon réécrit PUIS exécuté) / créature-tick",
      { "store_writes", "useful_stores" }, NULL, s_forager, NULL, NULL, f_rewrite, r_rewrite, NULL, NULL, 0, 0 },
    { "emit-echo", "Communiquer : écho d'un stimulus", "EMIT == stimulus (0..15) ; corrects / créature-tick",
      { "correct", "emits", "accuracy" }, NULL, s_echo, pre_echo, post_echo, f_emit, rank_default, NULL, NULL, 0, 0 },
    { "emit-dialog", "Dialogue : protocole stimulus → réponse",
      "hello 00→AC, s→s+1, bye 0F→00 ; corrects / créature-tick",
      { "correct", "emits", "accuracy" }, NULL, s_dialog, pre_dialog, post_dialog, f_emit, rank_default, NULL, NULL, 0, 0 },
    { "predict-walk", "Prédire une série AR(2) bruitée", "précision sur le bin (0..7) du prochain point ; baseline = répéter la dernière valeur",
      { "correct", "emits", "accuracy", "baseline", "csv", "vs_baseline" }, NULL, s_walk, pre_walk, post_predict, f_predict, rank_default, NULL, NULL, 0, 0 },
    { "predict-stock", "Direction d'un prix (proxy GBM / CSV local)",
      "précision hausse/baisse du pas suivant (A&1) ; hasard = 50 %",
      { "correct", "emits", "accuracy", "baseline", "csv", "vs_baseline" }, NULL, s_stock, pre_stock, post_predict, f_predict, rank_default, NULL, NULL, 0, 0 },
    { "predator-prey", "Proies (espèce 0) et prédateurs (espèce 1, ATTACK)",
      "fraction des ticks de coexistence × équilibre min(proies/20, préd/5)",
      { "mean_prey", "mean_pred", "attacks" }, cfg_pp, s_pp, NULL, post_pp, f_pp, rank_default, sel_pp, NULL, 0, 0 },
    /* ---- humanité ---- */
    { "fold", "Repliement de protéine (modèle HP 2D)",
      "contacts H-H / optimum connu, moyenne sur 6 benchmarks ; baseline = marche auto-évitante aléatoire",
      { "mean_score", "max_score", "baseline", "vs_baseline" }, hl_fold_cfg, hl_fold_seeds, NULL, NULL, hl_fold_fit, hl_rank_score, NULL, hl_fold_initp, 0, 0 },
    { "antibiotic", "Antibiotique contre pathogène co-évolutif",
      "ticks avant échappement (> 60 pathogènes) / ticks, ×1.25 si éradiqué, × hôtes vivants, × (1 − dégâts médicament) ; baseline = sans traitement",
      { "t_ctrl", "eradicated", "hosts_alive", "drug_damage", "baseline", "vs_baseline" }, hl_ab_cfg, hl_ab_seeds, hl_ab_pre, hl_ab_post, hl_ab_fit, rank_default, hl_ab_sel, hl_ab_init, 0, 0 },
    { "commons", "Biens communs : ressource à repousse lente",
      "population moyenne (/100) sur la 2e moitié d'un horizon de 5 × ticks × fraction de capacité restante ; vider une case la dégrade pour toujours ; baseline = fourrageur glouton",
      { "pop_late", "resource_frac", "dead_cells", "baseline", "vs_baseline" }, hl_cm_cfg, hl_cm_seeds, hl_cm_pre, NULL, hl_cm_fit, hl_rank_score, NULL, hl_cm_init, 5, 0 },
    { "pandemic", "Pandémie : contagion SEIR entre agents",
      "moyenne sur 4 paires appariées (nombres aléatoires communs) de : morts évitées / max(morts sans actions, 10) × maintien pop − énergie des actions / mangée ; baseline = même pool, même graine, actions sans effet",
      { "deaths", "pop_mean", "deaths_no_action", "cost", "vaccinated", "isolations" }, NULL, hl_pd_seeds, NULL, hl_pd_post, hl_pd_fit, hl_pd_rank, NULL, hl_pd_init, 0, 0 },
    { "code-repair", "Réparer un programme cassé via STORE",
      "score moyen par patient résolu : 1 si les 256 entrées redonnent la sortie de référence (+0.5 × patch minimal), 0 sinon ; baseline = flips aléatoires sans retour arrière",
      { "repaired", "resolved", "success_rate", "patch_bonus", "baseline", "vs_baseline" }, NULL, hl_cr_seeds, NULL, NULL, hl_cr_fit, hl_cr_rank, NULL, hl_cr_init, 0, 0 },
    { "self-evolve", "La seed pilote sa propre évolution",
      "Σ(1 − écart/4)⁺ des EMIT / créature-tick, moyenne sur les régimes (cible 0..15 qui change toutes les 50 ticks) ; paramètres d'évolution lus dans le génome (octets 2..5) ; baseline = paramètres fixes du moteur",
      { "baseline", "vs_baseline", "mut_rate", "indel_rate", "crossover", "protected" }, hl_se_cfg, hl_se_seeds, hl_se_pre, hl_se_post, hl_se_fit, hl_rank_score, NULL, hl_se_init, 0, 0 },
    { "diagnose", "Calculer un médicament que le stimulus ne donne pas",
      "justesse des EMIT : le médicament est ((marque + 1) XOR 4) mod 8, la marque est le stimulus ; baseline = même pool, stimulus renversé",
      { "accuracy", "emits", "best_rate", "blind", "vs_blind" }, hl_dg_cfg, hl_dg_seeds, hl_dg_pre, hl_dg_post, hl_dg_fit, hl_rank_score, hl_dg_sel, hl_dg_init, 0, 0 },
    { "general", "Apprendre pendant sa vie, généraliser à des tâches jamais vues",
      "note affichée = justesse des essais 32..63 sur la mémoire 2 et la mémoire 3 (T[indice d'il y a 2 ou 3 essais], tables réservées) ; pool en deux moitiés : niche mémoire (places 0-7, échelle T[p1], T[p2], T[p3] sur d'autres tables ; note et champion) et curriculum (places 8-15) ; la réévaluation appariée note les deux ; le pool garde le génome de naissance, pas les STORE de la vie ; known = familles déjà dans la seed ; baseline = mêmes génomes, génome gelé",
      { "lag", "train", "frozen", "vs_frozen", "known", "level" }, hl_gn_cfg, hl_gn_seeds, hl_gn_pre, NULL, hl_gn_fit, hl_rank_score, hl_gn_sel, hl_gn_init, 0, 1 },
};

int lab_index(const char *name) {
    for (int i = 0; i < N_LABS; i++) if (!strcmp(LABS[i].name, name)) return i;
    return -1;
}
void lab_init(Lab *l, int id, int max_ticks) {
    memset(l, 0, sizeof *l);
    l->id = id; l->def = &LABS[id];
    if (LABS[id].ticks_mult > 1) max_ticks *= LABS[id].ticks_mult;
    l->pops = calloc((size_t)max_ticks + 1, sizeof(int));
    l->prey = calloc((size_t)max_ticks + 1, sizeof(int));
    l->pred = calloc((size_t)max_ticks + 1, sizeof(int));
}
void lab_free(Lab *l) { if (l->ext_free) l->ext_free(l); free(l->ext); l->ext = NULL; free(l->pops); free(l->prey); free(l->pred); l->pops = l->prey = l->pred = NULL; }
void lab_pre(Lab *l, World *w) { if (l->def->pre) l->def->pre(l, w); }
void lab_post(Lab *l, World *w) { if (l->def->post) l->def->post(l, w); }

void run_cycle(Lab *l, const PoolEntry *pool, int n, Rng *rng, int ticks, CycleResult *res) {
    if (l->def->ticks_mult > 1) ticks *= l->def->ticks_mult;
    l->pool0 = pool; l->npool0 = n; l->rng0 = *rng;      /* pour les rejeux contrefactuels */
    PoolEntry *g = malloc(sizeof(PoolEntry) * (POOL_MAX + 8 + POOL_SIZE));
    int k = 0;
    for (int i = 0; i < n && k < POOL_MAX; i++) g[k++] = pool[i];
    if (l->id == 0)                                    /* org-seeds : + 8 champions étrangers */
        for (int i = 0; i < l->nforeign && i < 8; i++) { g[k] = l->foreign[i]; g[k].species = 0; k++; }
    int base = k;
    while (k < POOL_SIZE && base) { g[k] = g[k % base]; k++; }
    WorldConfig cfg = cfg_default();
    if (l->def->config) l->def->config(&cfg);
    World *w = world_new(g, k, rng, &cfg, l);
    if (l->def->init) l->def->init(l, w);
    free(g);
    int survived = 0;
    for (int t = 0; t < ticks; t++) {
        world_step(w);
        l->pops[l->npops++] = w->ncs;
        if (!w->ncs) break;
        survived++;
    }
    res->w = w;
    res->fitness = l->def->fitness(l, w, ticks);
    res->survived = survived;
    if (l->def->select) res->nnew = l->def->select(l, w, res->newpool);
    else {
        Creature *v[CMAX]; int m = w->ncs;
        memcpy(v, w->cs, sizeof(Creature *) * (size_t)m);
        sort_by_rank(v, m, l->def->rank);
        res->nnew = 0;
        for (int i = 0; i < m && i < POOL_SIZE; i++) entry_from(&res->newpool[res->nnew++], v[i]);
    }
}
