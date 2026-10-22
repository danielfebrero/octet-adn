/* lifeprobe.h — banc commun des détecteurs de vie « aveugles au code » (mi-detector, metabolic-order).
 * Header-only, ne touche pas au cœur : un génome est posé seul dans un monde par défaut (aucun labo),
 * et le détecteur ne lit que ce qui est OBSERVABLE de l'extérieur (déplacements, repas, EMIT, énergie).
 *
 * Contrôles construits ici, identiques pour les deux détecteurs :
 *   S seeds du dépôt (16, embarquées)                    E évolués : pools des checkpoints latest.ck
 *   N bruit : octets uniformes, longueurs tirées sur E   B ELF : fenêtres d'un vrai binaire (défaut /bin/ls)
 *   X évolués mélangés : même composition d'octets que E, ordre détruit (contrôle « composition »)
 */
#ifndef LIFEPROBE_H
#define LIFEPROBE_H
#include "octet.h"
#include <dirent.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct { char name[72]; char grp; PoolEntry g; } Probe;
#define LP_GROUPS "SENBX"
static const char *lp_group_name(char g) {
    switch (g) { case 'S': return "seeds"; case 'E': return "évolués"; case 'N': return "bruit";
                 case 'B': return "ELF"; case 'X': return "évolués-mélangés"; default: return "?"; }
}

static int lp_cmpname(const void *a, const void *b) { return strcmp(*(char *const *)a, *(char *const *)b); }

/* Construit les contrôles. Retourne le nombre de sondes (≤ max). n_rand : taille de N, B et X. */
static int lp_controls(Probe *out, int max, const char *evolved_dir, const char *elf_path, int n_rand, uint64_t seed) {
    int n = 0, ne0;
    for (int i = 0; i < N_SEEDS && n < max; i++) {
        const SeedBlob *s = seed_at(i);
        Probe *p = &out[n++]; snprintf(p->name, sizeof p->name, "seed:%s", s->name); p->grp = 'S';
        p->g.species = 0; p->g.len = s->len; memcpy(p->g.dna, s->p, (size_t)s->len);
    }
    ne0 = n;
    DIR *d = opendir(evolved_dir);
    char *names[64]; int nn = 0;
    if (d) {
        struct dirent *e;
        while ((e = readdir(d)) && nn < 64) {
            size_t L = strlen(e->d_name);
            if (L > 3 && !strcmp(e->d_name + L - 3, ".ck")) names[nn++] = strdup(e->d_name);
        }
        closedir(d);
    } else fprintf(stderr, "lifeprobe : %s introuvable (évolués vides)\n", evolved_dir);
    qsort(names, (size_t)nn, sizeof *names, lp_cmpname);
    for (int f = 0; f < nn; f++) {
        char path[4096]; Checkpoint ck;
        snprintf(path, sizeof path, "%s/%s", evolved_dir, names[f]);
        if (ck_load(path, &ck)) { fprintf(stderr, "lifeprobe : %s illisible\n", path); free(names[f]); continue; }
        names[f][strlen(names[f]) - 3] = 0;
        for (int i = 0; i < ck.npool && n < max; i++) {
            int dup = 0;
            for (int j = ne0; j < n && !dup; j++)
                dup = out[j].g.len == ck.pool[i].len && !memcmp(out[j].g.dna, ck.pool[i].dna, (size_t)ck.pool[i].len);
            if (dup) continue;
            Probe *p = &out[n++]; snprintf(p->name, sizeof p->name, "evo:%s#%d", names[f], i); p->grp = 'E';
            p->g = ck.pool[i]; p->g.species = 0;
        }
        free(names[f]);
    }
    int ne = n - ne0;
    Rng r; rng_seed(&r, seed);
    for (int i = 0; i < n_rand && n < max && ne > 0; i++) {           /* N : bruit */
        Probe *p = &out[n++]; p->grp = 'N'; p->g.species = 0;
        p->g.len = out[ne0 + (int)rng_below(&r, (uint32_t)ne)].g.len;
        for (int k = 0; k < p->g.len; k++) p->g.dna[k] = (uint8_t)rng_below(&r, 256);
        snprintf(p->name, sizeof p->name, "noise#%d", i);
    }
    FILE *f = fopen(elf_path, "rb"); long fl = 0; uint8_t *elf = NULL;
    if (f) {
        fseek(f, 0, SEEK_END); fl = ftell(f); fseek(f, 0, SEEK_SET); elf = malloc((size_t)fl);
        if (fread(elf, 1, (size_t)fl, f) != (size_t)fl) fl = 0;
        fclose(f);
    } else fprintf(stderr, "lifeprobe : %s introuvable (ELF vide)\n", elf_path);
    for (int i = 0; i < n_rand && n < max && ne > 0 && fl > MAX_LEN; i++) {   /* B : fenêtres ELF */
        Probe *p = &out[n++]; p->grp = 'B'; p->g.species = 0;
        p->g.len = out[ne0 + (int)rng_below(&r, (uint32_t)ne)].g.len;
        long off = i == 0 ? 0 : (long)rng_below(&r, (uint32_t)(fl - p->g.len));   /* #0 = l'en-tête ELF */
        memcpy(p->g.dna, elf + off, (size_t)p->g.len);
        snprintf(p->name, sizeof p->name, "elf@0x%lx", off);
    }
    free(elf);
    for (int i = 0; i < n_rand && n < max && ne > 0; i++) {           /* X : évolués mélangés */
        int src = ne0 + i % ne;
        Probe *p = &out[n++]; p->grp = 'X'; p->g = out[src].g;
        for (int k = p->g.len - 1; k > 0; k--) {
            int j = (int)rng_below(&r, (uint32_t)(k + 1)); uint8_t t = p->g.dna[k]; p->g.dna[k] = p->g.dna[j]; p->g.dna[j] = t;
        }
        snprintf(p->name, sizeof p->name, "shuf:%s", out[src].name + 4);
    }
    return n;
}

/* Lit un génome isolé : .ck (best sinon pool[0]) ou octets bruts. 0 si OK. */
static int lp_load_one(const char *path, PoolEntry *g) {
    Checkpoint ck;
    if (ck_load(path, &ck) == 0) { *g = ck.has_best ? ck.best : ck.pool[0]; g->species = 0; return 0; }
    FILE *f = fopen(path, "rb"); if (!f) return -1;
    g->len = (int)fread(g->dna, 1, MAX_LEN, f); g->species = 0; fclose(f);
    return g->len >= 1 ? 0 : -1;
}

/* Monde témoin : réglages par défaut, aucun labo, `copies` exemplaires du génome. */
static World *lp_arena(const PoolEntry *g, int copies, Rng *rng) {
    PoolEntry v[POOL_MAX];
    if (copies > POOL_MAX) copies = POOL_MAX;
    for (int i = 0; i < copies; i++) { v[i] = *g; v[i].species = 0; }
    WorldConfig cfg = cfg_default();
    return world_new(v, copies, rng, &cfg, NULL);
}

/* ---- statistiques ---- */
static int lp_cmpd(const void *a, const void *b) { double x = *(const double *)a, y = *(const double *)b; return (x > y) - (x < y); }
static double lp_q(double *v, int n, double q) {         /* v trié */
    if (n <= 0) return NAN;
    double i = q * (n - 1); int lo = (int)i; double fr = i - lo;
    return lo + 1 < n ? v[lo] * (1 - fr) + v[lo + 1] * fr : v[lo];
}
/* AUC = P(score(a) > score(b)) + ½ P(=) : 1 = séparation parfaite, 0.5 = hasard. */
static double lp_auc(const double *a, int na, const double *b, int nb) {
    if (!na || !nb) return NAN;
    double s = 0;
    for (int i = 0; i < na; i++) for (int j = 0; j < nb; j++) s += a[i] > b[j] ? 1 : a[i] == b[j] ? 0.5 : 0;
    return s / ((double)na * nb);
}
static int lp_pick(const Probe *p, const double *sc, int n, char g, double *out) {
    int k = 0; for (int i = 0; i < n; i++) if (p[i].grp == g) out[k++] = sc[i]; return k;
}
/* Résumé par groupe + AUC des évolués contre chaque contrôle + taux au-dessus du p95 du bruit. */
static void lp_report(FILE *o, const Probe *p, const double *sc, int n, const char *unit) {
    double *v = malloc(sizeof(double) * (size_t)n), *e = malloc(sizeof(double) * (size_t)n), *noise = malloc(sizeof(double) * (size_t)n);
    int ne = lp_pick(p, sc, n, 'E', e), nn = lp_pick(p, sc, n, 'N', noise);
    qsort(noise, (size_t)nn, sizeof(double), lp_cmpd);
    double thr = lp_q(noise, nn, 0.95);
    fprintf(o, "%-17s %4s %9s %9s %9s %9s %9s %9s  (%s)\n", "groupe", "n", "moyenne", "p10", "médiane", "p90", "max", ">p95bruit", unit);
    for (const char *g = LP_GROUPS; *g; g++) {
        int k = lp_pick(p, sc, n, *g, v); if (!k) continue;
        double m = 0; int above = 0;
        for (int i = 0; i < k; i++) { m += v[i]; above += v[i] > thr; }
        qsort(v, (size_t)k, sizeof(double), lp_cmpd);
        fprintf(o, "%-17s %4d %9.4f %9.4f %9.4f %9.4f %9.4f %8.1f%%\n", lp_group_name(*g), k, m / k,
                lp_q(v, k, .1), lp_q(v, k, .5), lp_q(v, k, .9), v[k - 1], 100.0 * above / k);
    }
    fprintf(o, "AUC évolués >");
    for (const char *g = "SNBX"; *g; g++) { int k = lp_pick(p, sc, n, *g, v); fprintf(o, "  %s %.3f", lp_group_name(*g), lp_auc(e, ne, v, k)); }
    fprintf(o, "   (seuil p95 bruit = %.4f)\n", thr);
    free(v); free(e); free(noise);
}
#endif
