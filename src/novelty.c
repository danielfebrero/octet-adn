/* experiments/novelty-map-elites : MAP-Elites sur le labo general.
 * La sélection historique (runner, labs) n'est pas touchée : c'est une boucle à part,
 * appelée par « octet-adn novelty ». Qualité = gn_train(g, 1, 0) (tâches d'entraînement,
 * jamais le jeu tenu à l'écart). Cellule = descripteurs de gn_behavior (aucune note de tâche).
 * Mesure seulement : chaque élite inséré est rejoué sur mémoire 2 et 3 tenues à l'écart
 * (gn_eval held, 40 tâches) ; s'il atteint 0.95, gn_check complet (gelé, sans récompense). */
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "octet.h"

static int bin_stores(double s) {
    if (s <= 0) return 0;
    int b = 1 + (int)floor(log2(s)) + 4;
    return b < 1 ? 1 : b > 7 ? 7 : b;
}
static int bin_drift(double x) {
    static const double th[7] = { 0, 1, 2, 4, 8, 16, 32 };
    int b = 0; while (b < 7 && x > th[b]) b++;
    return b;
}
int nov_ncells(int grid) { return grid == 1 ? 512 : grid == 2 ? 320 : 1; }
int nov_cell(int grid, const double *d) {
    if (grid == 0) return 0;
    if (grid == 1) {                    /* agnostique : entropie de réponse × STORE / essai × dérive du soma */
        int b0 = (int)(d[0] / log2(5.0) * 8); if (b0 > 7) b0 = 7; if (b0 < 0) b0 = 0;
        return (b0 * 8 + bin_stores(d[6])) * 8 + bin_drift(d[7]);
    }
    /* mem : retard dominant de la dépendance réponse/indice (aucun, 0..3) × force (bits) × STORE / essai */
    int L = 0; double m = d[2];
    for (int k = 1; k < 4; k++) if (d[2 + k] > m) { m = d[2 + k]; L = k; }
    int lag = m < 0.05 ? 0 : 1 + L, b1 = (int)(m * 8); if (b1 > 7) b1 = 7;
    return (lag * 8 + b1) * 8 + bin_stores(d[6]);
}

typedef struct { PoolEntry g; double q, m2, m3; int on; } Elite;

int nov_run(const NovRun *r, FILE *out, NovResult *res) {
    int nc = nov_ncells(r->grid);
    Elite *a = calloc((size_t)nc, sizeof *a);
    int *occ = malloc(sizeof(int) * (size_t)nc), nocc = 0;
    if (!a || !occ) { free(a); free(occ); return -1; }
    Rng rng; rng_seed(&rng, r->seed ^ 0x4E4F5645ULL);
    memset(res, 0, sizeof *res); res->ncells = nc;
    PoolEntry seed; seed_entry(&seed, "general", 0);
    static const char *GN[3] = { "1case", "agn", "mem" };
    if (out) fprintf(out, "# novelty grid=%s (%d cases) seed=%llu iters=%d init=%d  qualité = gn_train(·,1,0)\n"
                          "# iter cases couverture qd best_q archive_max_m2 archive_max_m3 max_m2_jamais max_m3_jamais insertions passeurs\n",
                     GN[r->grid], nc, (unsigned long long)r->seed, r->iters, r->init_mut);
    PoolEntry ch;
    for (long it = 0; it <= r->iters; it++) {
        const PoolEntry *par;
        if (it == 0) par = NULL;                                   /* la seed elle-même */
        else if (it <= r->init_mut || !nocc) par = &seed;          /* initialisation : mutants de la seed */
        else par = &a[occ[rng_below(&rng, (uint32_t)nocc)]].g;
        if (!par) ch = seed;
        else {
            memset(&ch, 0, sizeof ch);
            ch.len = mutate(par->dna, par->len, &rng, 0.005, 0.01, ch.dna);   /* taux de hl_gn_cfg */
            if (ch.len < 1) ch.len = 1;
        }
        if (!par || ch.len != par->len || memcmp(ch.dna, par->dna, (size_t)ch.len)) {
            double d[GN_BEH_N]; gn_behavior(&ch, d);
            int c = nov_cell(r->grid, d);
            double q = gn_train(&ch, 1, 0);
            if (c >= 0 && (!a[c].on || q > a[c].q)) {
                if (!a[c].on) { occ[nocc++] = c; a[c].on = 1; }
                a[c].g = ch; a[c].q = q; res->inserts++;
                a[c].m2 = gn_eval(&ch, 1, 1, 0, 0, 40, 0, 6, NULL);
                a[c].m3 = gn_eval(&ch, 1, 1, 0, 0, 40, 0, 7, NULL);
                if (a[c].m2 > res->max_m2) res->max_m2 = a[c].m2;
                if (a[c].m3 > res->max_m3) res->max_m3 = a[c].m3;
                if (a[c].m2 >= 0.95 || a[c].m3 >= 0.95) {
                    double v[GN_CHECK_N];
                    if (gn_check(&ch, v)) res->passers++;
                }
            }
        }
        if ((it % r->report) && it != r->iters) continue;
        double qd = 0, bq = 0, am2 = 0, am3 = 0;
        for (int k = 0; k < nocc; k++) {
            const Elite *e = &a[occ[k]];
            qd += e->q; if (e->q > bq) bq = e->q; if (e->m2 > am2) am2 = e->m2; if (e->m3 > am3) am3 = e->m3;
        }
        res->cells = nocc; res->qd = qd; res->best_q = bq;
        if (out) { fprintf(out, "%ld %d %.4f %.3f %.4f %.3f %.3f %.3f %.3f %ld %d\n", it, nocc, (double)nocc / nc, qd, bq,
                           am2, am3, res->max_m2, res->max_m3, res->inserts, res->passers); fflush(out); }
    }
    uint64_t h = FNV_INIT;
    for (int c = 0; c < nc; c++) if (a[c].on) { h = fnv1a(&c, sizeof c, h); h = fnv1a(a[c].g.dna, (size_t)a[c].g.len, h); }
    res->hash = h;
    if (out) {   /* l'élite de meilleure qualité et celui de meilleure mémoire held-out */
        int bi = -1, mi = -1;
        for (int k = 0; k < nocc; k++) {
            int c = occ[k];
            if (bi < 0 || a[c].q > a[bi].q) bi = c;
            if (mi < 0 || fmax(a[c].m2, a[c].m3) > fmax(a[mi].m2, a[mi].m3)) mi = c;
        }
        if (bi >= 0) fprintf(out, "# meilleure qualité : case %d q=%.4f m2=%.3f m3=%.3f len=%d\n", bi, a[bi].q, a[bi].m2, a[bi].m3, a[bi].g.len);
        if (mi >= 0) fprintf(out, "# meilleure mémoire held-out : case %d q=%.4f m2=%.3f m3=%.3f len=%d\n", mi, a[mi].q, a[mi].m2, a[mi].m3, a[mi].g.len);
    }
    free(a); free(occ);
    return 0;
}
