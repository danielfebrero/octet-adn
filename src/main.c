/* octet-adn — point d'entrée. Aucun interpréteur : ce binaire EST le ribosome. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "octet.h"

static void usage(void) {
    puts("usage : octet-adn [run] [options]        runner des éprouvettes (défaut)\n"
         "        octet-adn labs                    liste les 10 labos et leur fitness\n"
         "        octet-adn disasm FICHIER.adn      désassemble un génome\n"
         "        octet-adn dump FICHIER.bin        métriques binaires → texte (une ligne / cycle)\n"
         "        octet-adn hexdump FICHIER         octets bruts en hexadécimal\n"
         "        octet-adn ck FICHIER.ck           inspecte un checkpoint\n"
         "        octet-adn seeds DOSSIER           exporte les seeds embarquées en .adn\n"
         "\noptions run :\n"
         "  --labs all|a,b,c        (défaut all)\n"
         "  --ticks-per-cycle N     (défaut 200)\n"
         "  --cycles N              0 = boucle infinie (défaut 0)\n"
         "  --data-dir DIR          (défaut labs_data)\n"
         "  --seed N                graine RNG globale (défaut 0)\n"
         "  --sleep S               pause entre cycles (s)\n"
         "  --stock-csv FICHIER     série locale (colonne close) pour predict-stock ; aucun réseau\n"
         "  --no-reload             pas de ré-exécution quand le binaire change\n"
         "  --dry-run               1 cycle, rien n'est écrit");
}

static uint8_t *slurp(const char *path, long *n) {
    FILE *f = fopen(path, "rb");
    if (!f) { perror(path); return NULL; }
    fseek(f, 0, SEEK_END); *n = ftell(f); fseek(f, 0, SEEK_SET);
    uint8_t *b = malloc((size_t)*n + 1);
    if (fread(b, 1, (size_t)*n, f) != (size_t)*n) { fclose(f); free(b); return NULL; }
    fclose(f); b[*n] = 0;
    return b;
}

static double *load_csv(const char *path, int *n) {
    long len; char *s = (char *)slurp(path, &len);
    if (!s) return NULL;
    int col = -1, c = 0, cap = 1024; *n = 0;
    double *v = malloc(sizeof(double) * (size_t)cap);
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
                if (*n == cap) v = realloc(v, sizeof(double) * (size_t)(cap *= 2));
                v[(*n)++] = strtod(t, NULL);
            }
    }
    free(s);
    return v;
}

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
            size_t cap = (size_t)n * 40 + 64; char *o = malloc(cap);
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
