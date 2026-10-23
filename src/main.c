/* octet-adn — point d'entrée. Aucun interpréteur : ce binaire EST le ribosome. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include "octet.h"

static void usage(void) {
    puts("usage : octet-adn [run] [options]        runner des éprouvettes (défaut)\n"
         "        octet-adn labs                    liste les labos et leur fitness\n"
         "        octet-adn disasm FICHIER.adn      désassemble un génome\n"
         "        octet-adn dump FICHIER.bin        métriques binaires → texte (une ligne / cycle)\n"
         "        octet-adn hexdump FICHIER         octets bruts en hexadécimal\n"
         "        octet-adn ck FICHIER.ck           inspecte un checkpoint\n"
         "        octet-adn seeds DOSSIER           exporte les seeds embarquées en .adn\n"
         "        octet-adn eval DATA_DIR [N] [TICKS] fitness des pools (checkpoints) sur N graines tenues à l'écart\n"
         "        octet-adn general FICHIER.adn|.ck  general : rejoue chaque génome seul (mémoire 2 et 3, gelé, sans récompense)\n"
         "        octet-adn novelty [options]       MAP-Elites sur general (nouveauté de comportement, hors sélection historique)\n"
         "            --grid none|agn|mem (défaut agn)  --iters N (défaut 20000)  --seed N  --report N (défaut 1000)  --init N (défaut 32)\n"
         "        octet-adn soup [options]          soupe d'octets sans SPLIT : un réplicateur émerge-t-il du bruit ?\n"
         "            --ticks|--epochs N (défaut 4096)  --seed N  --programs N (défaut 4096)  --mut P (défaut 0.00024)\n"
         "            --addr abs|b (défaut abs)  --steps N (défaut 8192)  --report N (défaut 256)  --inject K (témoin : K copies du réplicateur fait main)  --no-store\n"
         "            --dump FICHIER (soupe finale, N × 64 octets bruts : fondateurs de terrarium_run --founders soup:FICHIER)\n"
         "\noptions run :\n"
         "  --labs all|a,b,c        (défaut all)\n"
         "  --ticks-per-cycle N     (défaut 200)\n"
         "  --cycles N              0 = boucle infinie (défaut 0)\n"
         "  --data-dir DIR          (défaut labs_data)\n"
         "  --seed N                graine RNG globale (défaut 0)\n"
         "  --sleep S               pause entre cycles (s)\n"
         "  --stock-csv FICHIER     série locale (colonne close) pour predict-stock ; aucun réseau\n"
         "  --select paired|legacy  sélection : réévaluation appariée (défaut) ou règle des 90 %\n"
         "  --no-reload             pas de ré-exécution quand le binaire change\n"
         "  --dry-run               1 cycle, rien n'est écrit");
}

static uint8_t *slurp(const char *path, long *n) {
    FILE *f = fopen(path, "rb");
    if (!f) { perror(path); return NULL; }
    fseek(f, 0, SEEK_END); *n = ftell(f); fseek(f, 0, SEEK_SET);
    uint8_t *b = xmalloc((size_t)*n + 1);
    if (fread(b, 1, (size_t)*n, f) != (size_t)*n) { fclose(f); free(b); return NULL; }
    fclose(f); b[*n] = 0;
    return b;
}

static double *load_csv(const char *path, int *n) {
    long len; char *s = (char *)slurp(path, &len);
    if (!s) return NULL;
    int col = -1, c = 0, cap = 1024; *n = 0;
    double *v = xmalloc(sizeof(double) * (size_t)cap);
    char *save = NULL;
    for (char *line = strtok_r(s, "\r\n", &save); line; line = strtok_r(NULL, "\r\n", &save)) {
        char *sv = NULL; c = 0;
        if (col < 0) {                                   /* entête : trouver la colonne close */
            for (char *t = strtok_r(line, ",", &sv); t; t = strtok_r(NULL, ",", &sv), c++)
                if (!strcmp(t, "close") || !strcmp(t, "Close")) col = c;
            if (col < 0) { fprintf(stderr, "%s : pas de colonne 'close'\n", path); free(s); free(v); return NULL; }
            continue;
        }
        for (char *t = strtok_r(line, ",", &sv); t; t = strtok_r(NULL, ",", &sv), c++)
            if (c == col) {
                if (*n == cap) v = xrealloc(v, sizeof(double) * (size_t)(cap *= 2));
                v[(*n)++] = strtod(t, NULL);
            }
    }
    free(s);
    return v;
}

#define ARGS(name) (!strcmp(a, name) && nx && (i++, 1))
int main(int argc, char **argv) {
    const char *cmd = argc > 1 && argv[1][0] != '-' ? argv[1] : "run";
    if (!strcmp(cmd, "help") || (argc > 1 && (!strcmp(argv[1], "-h") || !strcmp(argv[1], "--help")))) { usage(); return 0; }
    if (!strcmp(cmd, "labs")) {
        for (int i = 0; i < N_LABS; i++) printf("%2d  %-13s  %s\n    fitness : %s\n", i + 1, LABS[i].name, LABS[i].title, LABS[i].fitness_doc);
        return 0;
    }
    if (!strcmp(cmd, "disasm") || !strcmp(cmd, "hexdump")) {
        if (argc < 3) { usage(); return 2; }
        long n; uint8_t *b = slurp(argv[2], &n);
        if (!b) return 1;
        if (cmd[0] == 'd') {
            size_t cap = (size_t)n * 40 + 64; char *o = xmalloc(cap);
            disasm(b, (size_t)n, 1, o, cap);
            printf("# %s : %ld octets\n%s", argv[2], n, o); free(o);
        } else for (long i = 0; i < n; i++) printf("%02x%s", b[i], (i % 16 == 15 || i == n - 1) ? "\n" : " ");
        free(b); return 0;
    }
    if (!strcmp(cmd, "dump")) {
        if (argc < 3) { usage(); return 2; }
        MetricRec *v; int n = met_read(argv[2], &v);
        if (n < 0) { fprintf(stderr, "%s : pas un fichier " MET_MAGIC "\n", argv[2]); return 1; }
        char line[2048];
        for (int i = 0; i < n; i++) { met_format(&v[i], line, sizeof line); puts(line); }
        free(v); return 0;
    }
    if (!strcmp(cmd, "ck")) {
        if (argc < 3) { usage(); return 2; }
        static Checkpoint ck;
        if (ck_load(argv[2], &ck)) { fprintf(stderr, "%s : checkpoint invalide\n", argv[2]); return 1; }
        printf("lab=%s cycle=%u best_fitness=%.5g engine=%010llx pool=%d\n", ck.lab_id < N_LABS ? LABS[ck.lab_id].name : "?",
               ck.cycle, ck.best_fitness, (unsigned long long)(ck.engine & 0xffffffffffULL), ck.npool);
        for (int i = 0; i < ck.npool; i++) printf("  [%2d] espèce %d, %d octets\n", i, ck.pool[i].species, ck.pool[i].len);
        return 0;
    }
    if (!strcmp(cmd, "seeds")) {
        if (argc < 3) { usage(); return 2; }
        for (int i = 0; i < N_SEEDS; i++) {
            const SeedBlob *s = seed_at(i); char p[4096];
            snprintf(p, sizeof p, "%s/%s.adn", argv[2], s->name);
            FILE *f = fopen(p, "wb"); if (!f) { perror(p); return 1; }
            fwrite(s->p, 1, (size_t)s->len, f); fclose(f);
            printf("%s (%d octets)\n", p, s->len);
        }
        return 0;
    }
    if (!strcmp(cmd, "eval")) {                         /* qualité des pools : graines tenues à l'écart */
        if (argc < 3) { usage(); return 2; }
        int n = argc > 3 ? atoi(argv[3]) : 8, ticks = argc > 4 ? atoi(argv[4]) : 200;
        if (n <= 0 || ticks <= 0) { usage(); return 2; }
        static PoolEntry foreign[N_LABS]; static Checkpoint ck;
        int nf = collect_foreign(argv[2], foreign);
        for (int id = 0; id < N_LABS; id++) {
            char p[4096]; snprintf(p, sizeof p, "%s/%s/checkpoints/latest.ck", argv[2], LABS[id].name);
            if (ck_load(p, &ck) || !ck.npool) continue;
            double s = 0, s2 = 0, sb = 0, ex[N_EXTRA];
            int bi = -1;                               /* labos de prédiction : colonne « baseline » */
            for (int k = 0; k < N_EXTRA && LABS[id].extra_names[k]; k++) if (!strcmp(LABS[id].extra_names[k], "baseline")) bi = k;
            for (int j = 0; j < n; j++) {
                char key[128]; snprintf(key, sizeof key, "holdout:%s:%d", LABS[id].name, j);
                double f = pool_eval_x(id, ck.pool, ck.npool, key, ticks, foreign, nf, ex);
                if (LABS[id].show_extra) f = ex[LABS[id].show_extra - 1];
                s += f; s2 += f * f; if (bi >= 0) sb += ex[bi];
            }
            double m = s / n, sd = n > 1 ? sqrt(fmax(0, (s2 - n * m * m) / (n - 1))) : 0;
            printf("%-13s cycle=%u  holdout_mean=%.4f  sd=%.4f  n=%d", LABS[id].name, ck.cycle, m, sd, n);
            if (bi >= 0) printf("  baseline=%.4f  écart=%+.4f", sb / n, m - sb / n);
            putchar('\n');
        }
        return 0;
    }
    if (!strcmp(cmd, "general")) {                      /* émergence : chaque génome rejoué seul */
        if (argc < 3) { usage(); return 2; }
        PoolEntry seed; seed_entry(&seed, "general", 0);
        int found = 0;
        for (int a = 2; a < argc; a++) {
            static Checkpoint ck; static PoolEntry g[POOL_MAX + 1]; int n = 0;
            if (ck_load(argv[a], &ck) == 0) {
                for (int i = 0; i < ck.npool; i++) g[n++] = ck.pool[i];
                if (ck.has_best) g[n++] = ck.best;
            } else {
                long len; uint8_t *b = slurp(argv[a], &len);
                if (!b) return 1;
                if (len < 1 || len > MAX_LEN) { fprintf(stderr, "%s : %ld octets\n", argv[a], len); free(b); return 1; }
                memset(&g[0], 0, sizeof g[0]); g[0].len = (int)len; memcpy(g[0].dna, b, (size_t)len); n = 1; free(b);
            }
            for (int i = 0; i < n; i++) {
                double v[GN_CHECK_N]; int e = gn_check(&g[i], v);
                int d = abs(g[i].len - seed.len);
                for (int k = 0; k < g[i].len && k < seed.len; k++) d += g[i].dna[k] != seed.dna[k];
                printf("%s[%d] %d o, %d o ≠ seed  m2=%.3f m3=%.3f  gelé %.3f/%.3f  sans_récompense %.3f/%.3f  known=%.3f train=%.3f%s\n",
                       argv[a], i, g[i].len, d, v[0], v[1], v[2], v[3], v[4], v[5], v[6], v[7],
                       e ? (e == 2 ? "  ÉMERGENCE mémoire-2" : "  ÉMERGENCE mémoire-3") : "");
                found |= e != 0;
            }
        }
        printf("émergence : %s\n", found ? "oui" : "non");
        return found ? 0 : 3;
    }
    if (!strcmp(cmd, "novelty")) {                      /* experiments/novelty-map-elites */
        NovRun r = { .grid = 1, .iters = 20000, .report = 1000, .init_mut = 32 };
        for (int i = 2; i < argc; i++) {
            const char *a = argv[i], *nx = i + 1 < argc ? argv[i + 1] : NULL;
#define ARGN(name) (!strcmp(a, name) && nx && (i++, 1))
            if (ARGN("--iters")) r.iters = atoi(nx);
            else if (ARGN("--seed")) r.seed = strtoull(nx, NULL, 10);
            else if (ARGN("--report")) r.report = atoi(nx);
            else if (ARGN("--init")) r.init_mut = atoi(nx);
            else if (ARGN("--grid")) {
                if (!strcmp(nx, "none")) r.grid = 0; else if (!strcmp(nx, "agn")) r.grid = 1;
                else if (!strcmp(nx, "mem")) r.grid = 2; else { fprintf(stderr, "--grid : none|agn|mem\n"); return 2; }
            }
            else { fprintf(stderr, "option inconnue : %s\n", a); usage(); return 2; }
#undef ARGN
        }
        if (r.iters < 0 || r.report <= 0 || r.init_mut < 0) { usage(); return 2; }
        NovResult res;
        if (nov_run(&r, stdout, &res)) return 2;
        printf("verdict grid=%d seed=%llu : couverture %d/%d  qd=%.3f  best_q=%.4f  max_m2=%.3f  max_m3=%.3f  passeurs=%d  hash=%016llx\n",
               r.grid, (unsigned long long)r.seed, res.cells, res.ncells, res.qd, res.best_q, res.max_m2, res.max_m3, res.passers,
               (unsigned long long)res.hash);
        return 0;
    }
    if (!strcmp(cmd, "soup")) {                         /* experiments/soup-replicator */
        SoupRun r = { .nprog = 4096, .epochs = 4096, .report = 256, .mut = 0.00024 };
        for (int i = 2; i < argc; i++) {
            const char *a = argv[i], *nx = i + 1 < argc ? argv[i + 1] : NULL;
            if (ARGS("--ticks") || ARGS("--epochs")) r.epochs = atoi(nx);
            else if (ARGS("--seed")) r.seed = strtoull(nx, NULL, 10);
            else if (ARGS("--programs")) r.nprog = atoi(nx);
            else if (ARGS("--mut")) r.mut = atof(nx);
            else if (ARGS("--steps")) r.vm.steps = atoi(nx);
            else if (ARGS("--report")) r.report = atoi(nx);
            else if (ARGS("--addr")) {
                if (!strcmp(nx, "b")) r.vm.addr_b = 1;
                else if (strcmp(nx, "abs")) { fprintf(stderr, "--addr : abs|b\n"); return 2; }
            }
            else if (ARGS("--inject")) r.inject = atoi(nx);
            else if (ARGS("--dump")) r.dump = nx;
            else if (!strcmp(a, "--no-store")) r.vm.no_store = 1;
            else { fprintf(stderr, "option inconnue : %s\n", a); usage(); return 2; }
        }
        if (r.epochs <= 0 || r.report <= 0 || r.nprog < 2 || r.mut < 0 || r.mut >= 1) { usage(); return 2; }
        SoupResult res;
        if (soup_run(&r, stdout, &res)) return 2;
        printf("verdict seed=%llu addr=%s : réplicateur %s  t_first=%ld t_half=%ld  min_essentiels=%d  copieurs_fin=%d  naissances=%ld  hash=%016llx\n",
               (unsigned long long)r.seed, r.vm.addr_b ? "b" : "abs", res.t_first >= 0 ? "OUI" : "non",
               res.t_first, res.t_half, res.min_ess, res.copiers_final, res.births, (unsigned long long)res.hash);
        return 0;
    }
    if (strcmp(cmd, "run")) { usage(); return 2; }

    RunOpts o = { .ticks = 200, .data_dir = "labs_data", .argv = argv };
    static char exe[4096];
    ssize_t el = readlink("/proc/self/exe", exe, sizeof exe - 1);
    if (el > 0) { exe[el] = 0; o.exe_path = exe; }
    const char *labs = "all";
    for (int i = 1; i < argc; i++) {
        const char *a = argv[i];
        const char *nx = i + 1 < argc ? argv[i + 1] : NULL;
#define ARG(name) (!strcmp(a, name) && nx && (i++, 1))
        if (!strcmp(a, "run")) continue;
        else if (ARG("--labs")) labs = nx;
        else if (ARG("--ticks-per-cycle")) o.ticks = atoi(nx);
        else if (ARG("--cycles")) o.cycles = atoi(nx);
        else if (ARG("--data-dir")) o.data_dir = nx;
        else if (ARG("--seed")) o.seed = strtoull(nx, NULL, 10);
        else if (ARG("--sleep")) o.sleep_s = atof(nx);
        else if (ARG("--stock-csv")) {
            int n; double *v = load_csv(nx, &n);
            if (!v || n < 2) { fprintf(stderr, "--stock-csv : série invalide\n"); return 2; }
            stock_set_csv(v, n);
        }
        else if (!strncmp(a, "--cycles-done=", 14)) o.cycles_done = atoi(a + 14);
        else if (ARG("--select")) {
            if (!strcmp(nx, "legacy")) o.sel_legacy = 1;
            else if (strcmp(nx, "paired")) { fprintf(stderr, "--select : paired|legacy\n"); return 2; }
        }
        else if (!strcmp(a, "--no-reload")) o.no_reload = 1;
        else if (!strcmp(a, "--dry-run")) o.dry_run = 1;
        else { fprintf(stderr, "option inconnue : %s\n", a); usage(); return 2; }
    }
    if (o.ticks <= 0) { fprintf(stderr, "--ticks-per-cycle doit être > 0\n"); return 2; }
    if (!strcmp(labs, "all")) { for (int i = 0; i < N_LABS; i++) o.labs[o.nlabs++] = i; }
    else {
        char buf[1024]; snprintf(buf, sizeof buf, "%s", labs); char *sv = NULL;
        for (char *t = strtok_r(buf, ",", &sv); t; t = strtok_r(NULL, ",", &sv)) {
            int id = lab_index(t);
            if (id < 0 || o.nlabs >= N_LABS) {
                fprintf(stderr, "labo inconnu : %s (dispo :", t);
                for (int k = 0; k < N_LABS; k++) fprintf(stderr, " %s", LABS[k].name);
                fprintf(stderr, ")\n"); return 2;
            }
            o.labs[o.nlabs++] = id;
        }
    }
    runner(&o, NULL, 0);
    return 0;
}
