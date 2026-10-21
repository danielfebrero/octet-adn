/* Orchestrateur continu des 10 éprouvettes.
 * Chaque cycle : reprend le checkpoint de chaque labo, fait vivre son pool, mesure la
 * fitness, sélectionne, sauvegarde (binaire), ajoute un enregistrement à metrics.bin et
 * régénère status.md. Si le binaire est recompilé pendant que le runner tourne, il se
 * ré-exécute (execv) : la nouvelle « popote » s'applique à tous les labos au cycle suivant. */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include "octet.h"

uint64_t engine_version(void) {
    static uint64_t cached;
    if (cached) return cached;
    FILE *f = fopen("/proc/self/exe", "rb");
    uint64_t h = FNV_INIT;
    if (f) {
        uint8_t buf[65536]; size_t n;
        while ((n = fread(buf, 1, sizeof buf, f)) > 0) h = fnv1a(buf, n, h);
        fclose(f);
    }
    return cached = h;
}

static int mkdirs(const char *p) {
    char b[4096]; snprintf(b, sizeof b, "%s", p);
    for (char *s = b + 1; *s; s++)
        if (*s == '/') { *s = 0; if (mkdir(b, 0755) && errno != EEXIST) return -1; *s = '/'; }
    return (mkdir(b, 0755) && errno != EEXIST) ? -1 : 0;
}
static double now_s(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec + t.tv_nsec * 1e-9; }
static int write_file(const char *path, const uint8_t *p, int n) {
    FILE *f = fopen(path, "wb");
    if (!f) return -1;
    size_t w = fwrite(p, 1, (size_t)n, f);
    return (fclose(f) == 0 && w == (size_t)n) ? 0 : -1;
}

/* Écart à la baseline du dernier cycle enregistré. Pandemic : la fitness EST l'écart. */
static double humanity_gap(int id, const MetricRec *r) {
    const char *n = LABS[id].name;
    if (!strcmp(n, "fold")) return r->extra[3];
    if (!strcmp(n, "antibiotic")) return r->extra[5];
    if (!strcmp(n, "commons")) return r->extra[4];
    if (!strcmp(n, "pandemic")) return r->fitness;
    if (!strcmp(n, "code-repair")) return r->extra[5];
    if (!strcmp(n, "self-evolve")) return r->extra[1];
    if (!strcmp(n, "diagnose")) return r->extra[4];
    return 0;
}

int collect_foreign(const char *dir, PoolEntry *out) {
    int n = 0;
    for (int i = 1; i < N_CORE_LABS; i++) {            /* labos historiques sauf org-seeds, ordre du registre */
        char p[4096]; Checkpoint ck;
        snprintf(p, sizeof p, "%s/%s/checkpoints/latest.ck", dir, LABS[i].name);
        if (ck_load(p, &ck) == 0 && ck.has_best) out[n++] = ck.best;
    }
    /* Un champion humanité n'entre que si son dernier vs_baseline est positif.
     * Le génome importé est celui du pool (celui que la métrique a mesuré), pas le
     * sommet d'un cycle que la sélection appariée a refusé. Il prend la 8e place. */
    PoolEntry champ; int have = 0; double best_gap = 0;
    for (int i = N_CORE_LABS; i < N_LABS; i++) {
        char p[4096]; Checkpoint ck;
        snprintf(p, sizeof p, "%s/%s/checkpoints/latest.ck", dir, LABS[i].name);
        if (ck_load(p, &ck) || !ck.has_best) continue;
        snprintf(p, sizeof p, "%s/%s/metrics.bin", dir, LABS[i].name);
        MetricRec *v = NULL; int m = met_read(p, &v);
        double gap = (m > 0) ? humanity_gap(i, &v[m - 1]) : 0;
        free(v);
        if (gap > best_gap) {
            best_gap = gap; have = 1;
            champ = ck.npool > 0 ? ck.pool[0] : ck.best;
        }
    }
    if (have) {
        if (n >= 8) out[7] = champ;
        else out[n++] = champ;
    }
    return n;
}

/* Fitness d'un pool sur une graine donnée (cycle complet, monde jetable). */
double pool_eval_x(int id, const PoolEntry *pool, int n, const char *key, int ticks, PoolEntry *foreign, int nf, double *extra) {
    Rng rng; rng_seed(&rng, fnv1a(key, strlen(key), FNV_INIT));
    Lab lab; lab_init(&lab, id, ticks);
    lab.foreign = foreign; lab.nforeign = nf;
    CycleResult *res = malloc(sizeof *res);
    run_cycle(&lab, pool, n, &rng, ticks, res);
    double f = res->fitness;
    if (extra) memcpy(extra, lab.extra, sizeof lab.extra);
    world_free(res->w); lab_free(&lab); free(res);
    return f;
}
double pool_eval(int id, const PoolEntry *pool, int n, const char *key, int ticks, PoolEntry *foreign, int nf) {
    return pool_eval_x(id, pool, n, key, ticks, foreign, nf, NULL);
}

static double g_eval_new, g_eval_old;   /* dernière réévaluation (affichage) */

static MetricRec run_lab_cycle(int id, const RunOpts *o, PoolEntry *foreign, int nforeign, int persist) {
    static Checkpoint ck;
    char dir[4096], p[4096];
    snprintf(dir, sizeof dir, "%s/%s", o->data_dir ? o->data_dir : ".", LABS[id].name);
    snprintf(p, sizeof p, "%s/checkpoints/latest.ck", dir);
    if (!persist || ck_load(p, &ck) || ck.npool == 0) {
        memset(&ck, 0, sizeof ck);
        ck.lab_id = id;
        ck.npool = LABS[id].seeds(ck.pool);
    }
    uint64_t eng = engine_version();
    if (persist && ck.engine && ck.engine != eng && !o->quiet)
        printf("[popote] %s : moteur %010llx → %010llx (amélioration appliquée)\n", LABS[id].name,
               (unsigned long long)(ck.engine & 0xffffffffffULL), (unsigned long long)(eng & 0xffffffffffULL));
    if (persist && ck.engine && ck.engine != eng) {
        /* Réensemencement au changement de moteur : si la seed du labo (peut-être améliorée par la
         * nouvelle popote) bat le pool en place sur les mêmes SEL_EVALS graines, elle le remplace.
         * Sinon une seed corrigée ne profiterait jamais aux labos qui tournent déjà. */
        static PoolEntry sp[POOL_MAX]; int ns = LABS[id].seeds(sp);
        double fs = 0, fo = 0;
        for (int j = 0; j < SEL_EVALS; j++) {
            char rk[300]; snprintf(rk, sizeof rk, "%s:reseed:%010llx:%d", LABS[id].name, (unsigned long long)(eng & 0xffffffffffULL), j);
            fs += pool_eval(id, sp, ns, rk, o->ticks, foreign, nforeign) / SEL_EVALS;
            fo += pool_eval(id, ck.pool, ck.npool, rk, o->ticks, foreign, nforeign) / SEL_EVALS;
        }
        if (fs > 1.05 * fo) {                          /* marge : pas de reset sur du bruit */
            memcpy(ck.pool, sp, sizeof(PoolEntry) * (size_t)ns); ck.npool = ns;
            if (!o->quiet) printf("[popote] %s : réensemencé (seed %.4g > pool en place %.4g)\n", LABS[id].name, fs, fo);
        }
    }
    uint32_t cycle = ck.cycle + 1;
    char key[256];
    snprintf(key, sizeof key, "%s:%u:%llu", LABS[id].name, cycle, (unsigned long long)o->seed);
    Rng rng; rng_seed(&rng, fnv1a(key, strlen(key), FNV_INIT));   /* déterministe par (labo, cycle, seed) */
    Lab lab; lab_init(&lab, id, o->ticks);
    lab.foreign = foreign; lab.nforeign = nforeign;
    static CycleResult res;
    double t0 = now_s();
    run_cycle(&lab, ck.pool, ck.npool, &rng, o->ticks, &res);
    double fit = res.fitness;
    int accepted = 0;
    g_eval_new = g_eval_old = 0;
    if (o->sel_legacy) accepted = res.nnew > 0 && fit >= ACCEPT_RATIO * ck.best_fitness;
    else if (res.nnew > 0) {
        /* Sélection appariée, robuste au bruit : le nouveau pool et le pool en place sont
         * réévalués sur les MÊMES SEL_EVALS graines (indépendantes de celle du cycle) ; on garde
         * le nouveau s'il fait au moins aussi bien en moyenne. Plus de comparaison à un pic
         * historique chanceux, donc plus de pool gelé. */
        for (int j = 0; j < SEL_EVALS; j++) {
            char ek[300]; snprintf(ek, sizeof ek, "%s:eval%d", key, j);
            g_eval_new += pool_eval(id, res.newpool, res.nnew, ek, o->ticks, foreign, nforeign) / SEL_EVALS;
            g_eval_old += pool_eval(id, ck.pool, ck.npool, ek, o->ticks, foreign, nforeign) / SEL_EVALS;
        }
        accepted = g_eval_new >= g_eval_old;
    }
    if (accepted) { memcpy(ck.pool, res.newpool, sizeof(PoolEntry) * (size_t)res.nnew); ck.npool = res.nnew; }
    if (fit > ck.best_fitness && res.nnew > 0) { ck.best_fitness = fit; ck.best = res.newpool[0]; ck.has_best = 1; }
    ck.cycle = cycle; ck.engine = eng;

    MetricRec r; memset(&r, 0, sizeof r);
    if (LABS[id].show_extra) fit = lab.extra[LABS[id].show_extra - 1];   /* general : affiche le jeu tenu à l'écart */
    time_t now = time(NULL); struct tm lt; localtime_r(&now, &lt);
    r.ts = now; r.tz_off = (int32_t)lt.tm_gmtoff;
    r.cycle = cycle; r.ticks = (uint32_t)o->ticks; r.survived = (uint32_t)res.survived;
    r.fitness = fit; r.best_fitness = ck.best_fitness; r.accepted = (uint8_t)accepted; r.lab_id = (uint8_t)id;
    World *w = res.w;
    long lens = 0; for (int i = 0; i < w->ncs; i++) lens += w->cs[i]->len;
    r.pop_final = (uint16_t)w->ncs;
    long sp = 0; for (int i = 0; i < lab.npops; i++) sp += lab.pops[i];
    r.pop_mean = lab.npops ? (double)sp / lab.npops : 0;
    r.born = (uint32_t)w->st.born; r.dead = (uint32_t)w->st.dead;
    r.store_writes = (uint32_t)w->st.store_writes; r.useful_stores = (uint32_t)w->st.useful_stores;
    r.emits = (uint32_t)w->st.emits;
    r.genome_len_mean = w->ncs ? (double)lens / w->ncs : 0;
    r.engine = eng; r.state_hash = world_hash(w);
    r.elapsed_s = now_s() - t0;
    memcpy(r.extra, lab.extra, sizeof r.extra);
    world_free(w); lab_free(&lab);

    if (persist) {
        snprintf(p, sizeof p, "%s/checkpoints", dir);
        mkdirs(p);
        snprintf(p, sizeof p, "%s/checkpoints/latest.ck", dir);
        if (ck_save(p, &ck)) fprintf(stderr, "erreur : checkpoint %s\n", p);
        if (ck.has_best) { snprintf(p, sizeof p, "%s/champion.adn", dir); write_file(p, ck.best.dna, ck.best.len); }
        snprintf(p, sizeof p, "%s/metrics.bin", dir);
        if (met_append(p, &r)) fprintf(stderr, "erreur : metrics %s\n", p);
    }
    return r;
}

static int exe_changed(const char *path, struct stat *ref) {
    struct stat st;
    if (!path || stat(path, &st)) return 0;
    return st.st_ino != ref->st_ino || st.st_mtim.tv_sec != ref->st_mtim.tv_sec || st.st_size != ref->st_size;
}

int runner(RunOpts *o, MetricRec *rows, int cap) {
    int cycles = o->dry_run ? 1 : o->cycles, persist = !o->dry_run, nrows = 0;
    struct stat ref; int have_ref = o->exe_path && !stat(o->exe_path, &ref);
    static PoolEntry foreign[N_LABS];
    if (persist) mkdirs(o->data_dir);
    for (int k = o->cycles_done; cycles == 0 || k < cycles; k++) {
        if (!o->no_reload && have_ref && k > o->cycles_done && exe_changed(o->exe_path, &ref) && o->argv) {
            char done[32]; snprintf(done, sizeof done, "--cycles-done=%d", k);
            int n = 0; while (o->argv[n]) n++;
            char **av = calloc((size_t)n + 2, sizeof *av);
            for (int i = 0; i < n; i++) av[i] = strncmp(o->argv[i], "--cycles-done=", 14) ? o->argv[i] : done;
            int has = 0; for (int i = 0; i < n; i++) if (av[i] == done) has = 1;
            if (!has) av[n++] = done;
            printf("[popote] nouveau binaire détecté → ré-exécution (cycle %d)\n", k + 1);
            fflush(stdout);
            execv(o->exe_path, av);
            perror("execv");                           /* échec : on continue avec l'ancien moteur */
            free(av); have_ref = 0;
        }
        int nf = persist ? collect_foreign(o->data_dir, foreign) : 0;
        for (int i = 0; i < o->nlabs; i++) {
            MetricRec r = run_lab_cycle(o->labs[i], o, foreign, nf, persist);
            if (rows && nrows < cap) rows[nrows] = r;
            nrows++;
            if (!o->quiet)
                printf("[%13s] cycle %4u  fitness=%-9.4g best=%-9.4g pop=%3u  %s  eval nouveau/en place=%.4g/%.4g  %.3fs\n",
                       LABS[r.lab_id].name, r.cycle, r.fitness, r.best_fitness, r.pop_final, r.accepted ? "✓" : "·",
                       g_eval_new, g_eval_old, r.elapsed_s);
            fflush(stdout);
        }
        if (persist) write_status(o->data_dir, o->labs, o->nlabs);
        if (o->sleep_s > 0 && (cycles == 0 || k + 1 < cycles)) {
            struct timespec ts = { (time_t)o->sleep_s, (long)((o->sleep_s - (time_t)o->sleep_s) * 1e9) };
            nanosleep(&ts, NULL);
        }
    }
    return nrows;
}
