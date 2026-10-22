/* Recherche d'univers v2 (experiments/universe-search/v2) : fitness ROBUSTE + origine à grand N + dérive lente.
 *
 * Réutilise tel quel le moteur de la v1 (universe_search.c : catalogue fermé, VM, soupe, copieur vérifié,
 * cristaux exclus, garde-fou anti-triche) : ce fichier l'inclut et n'y change rien.
 *
 *   race   : sélection vs témoin aléatoire, MÊME procédure de racing (successive halving) dans les deux bras :
 *            étape 0 : chaque univers évalué sur 2 graines à N0 (sélection = GA ; témoin = univers frais) ;
 *            étape 1 : top K1 de chaque archive, +4 graines neuves à N0 ;
 *            étape 2 : top K2, 4 graines neuves à N=4096 ;
 *            finalistes (top K3) revalidés sur V graines JAMAIS vues à N=4096 ; Fisher exact + permutation.
 *            Score de classement = taux d'émergence lissé (nem+0.1)/(nev+2), départage par la fitness façonnée v1.
 *   origin : univers d'origine (soupe b) et finalistes sauvegardés, à N donné (16384), graines neuves.
 *   drift  : population établie d'un finaliste, physique qui dérive d'UNE entrée de table / un paramètre toutes
 *            les K époques ; témoin = saut brutal vers le MÊME point d'arrivée, et témoin sans dérive.
 */
#define main us1_main
#include "universe_search.c"
#undef main

/* ---------------- statistiques ---------------- */
static double lchoose(int n, int k) { return lgamma(n + 1.0) - lgamma(k + 1.0) - lgamma(n - k + 1.0); }
/* Fisher exact unilatéral : P(X >= a) ; table [a b ; c d], ligne 1 = sélection, colonne 1 = émergence */
static double fisher_greater(int a, int b, int c, int d) {
    int r1 = a + b, c1 = a + c, n = a + b + c + d; double p = 0;
    int hi = r1 < c1 ? r1 : c1;
    for (int x = a; x <= hi; x++) { if (c1 - x > c + d) continue; p += exp(lchoose(r1, x) + lchoose(n - r1, c1 - x) - lchoose(n, c1)); }
    return p > 1 ? 1 : p;
}
static double fisher_two(int a, int b, int c, int d) {
    int r1 = a + b, c1 = a + c, n = a + b + c + d; double p0 = exp(lchoose(r1, a) + lchoose(n - r1, c1 - a) - lchoose(n, c1)), p = 0;
    int lo = c1 - (n - r1) > 0 ? c1 - (n - r1) : 0, hi = r1 < c1 ? r1 : c1;
    for (int x = lo; x <= hi; x++) { double q = exp(lchoose(r1, x) + lchoose(n - r1, c1 - x) - lchoose(n, c1)); if (q <= p0 * (1 + 1e-9)) p += q; }
    return p > 1 ? 1 : p;
}
/* permutation unilatérale sur la moyenne des taux par univers (unité = univers, pas l'essai) */
static double perm_test(const double *x, int nx, const double *y, int ny) {
    double all[256]; int n = nx + ny; double sx = 0, obs;
    for (int i = 0; i < nx; i++) { all[i] = x[i]; sx += x[i]; }
    for (int i = 0; i < ny; i++) all[nx + i] = y[i];
    double sy = 0; for (int i = 0; i < ny; i++) sy += y[i];
    obs = sx / nx - sy / ny;
    Rng r; rng_seed(&r, 0x9E37); int ge = 0, R = 20000;
    for (int it = 0; it < R; it++) {
        for (int i = n - 1; i > 0; i--) { int j = (int)rng_below(&r, (uint32_t)(i + 1)); double t = all[i]; all[i] = all[j]; all[j] = t; }
        double a = 0, b = 0; for (int i = 0; i < nx; i++) a += all[i]; for (int i = nx; i < n; i++) b += all[i];
        ge += a / nx - b / ny >= obs - 1e-12;
    }
    return (ge + 1.0) / (R + 1.0);
}
static void wilson(int k, int n, double *lo, double *hi) {
    if (!n) { *lo = 0; *hi = 1; return; }
    double z = 1.96, p = (double)k / n, d = 1 + z * z / n, c = p + z * z / (2 * n), w = z * sqrt(p * (1 - p) / n + z * z / (4.0 * n * n));
    *lo = (c - w) / d; *hi = (c + w) / d;
}

/* ---------------- archive de candidats ---------------- */
typedef struct {
    Univ u; uint64_t h;
    int nev, nem;          /* étapes 0-1 (N0) */
    int nev4, nem4;        /* étape 2 (N=4096) */
    int nvv, nvm;          /* revalidation (graines jamais vues) */
    double fs; SimRes best; int has_best;
} Cand;
static double c_score(const Cand *c) { return (c->nem + 0.1) / (c->nev + 2.0); }
static double c_tie(const Cand *c) { return c->nev ? c->fs / c->nev : 0; }
static int cmp_cand(const void *a, const void *b) {
    const Cand *x = *(Cand *const *)a, *y = *(Cand *const *)b;
    double sx = c_score(x), sy = c_score(y);
    if (sx != sy) return sx < sy ? 1 : -1;
    sx = c_tie(x); sy = c_tie(y); return sx < sy ? 1 : sx > sy ? -1 : 0;
}
static double c_score4(const Cand *c) { return (c->nem4 + 0.1) / (c->nev4 + 2.0) + 1e-3 * c_score(c); }
static int cmp_cand4(const void *a, const void *b) {
    const Cand *x = *(Cand *const *)a, *y = *(Cand *const *)b; double sx = c_score4(x), sy = c_score4(y);
    return sx < sy ? 1 : sx > sy ? -1 : 0;
}
typedef struct { Cand *c; int n, cap; } Arch;
static int arch_get(Arch *A, const Univ *u) {
    uint64_t h = u_hash(u);
    for (int i = 0; i < A->n; i++) if (A->c[i].h == h) return i;
    if (A->n == A->cap) { A->cap = A->cap ? 2 * A->cap : 256; A->c = realloc(A->c, sizeof *A->c * (size_t)A->cap); }
    memset(&A->c[A->n], 0, sizeof A->c[0]); A->c[A->n].u = *u; A->c[A->n].h = h;
    return A->n++;
}
/* évalue la liste idx (avec répétitions possibles), ns graines chacune, à N ; stage = 0/1 (N0), 2 (4096), 3 (reval) */
static long g_sims[2];
static void eval_list(Arch *A, const int *idx, int n, int ns, int N, int E, int E1, uint64_t salt, int stage, int arm) {
    Job *J = calloc((size_t)(n * ns), sizeof *J);
    for (int i = 0; i < n; i++) for (int s = 0; s < ns; s++) {
        Job *j = &J[i * ns + s]; j->u = &A->c[idx[i]].u; memset(&j->c, 0, sizeof j->c);
        j->c.N = N; j->c.E = E; j->c.E1 = E1; j->c.seed = mix(salt, (uint64_t)i * 4096 + (uint64_t)s);
    }
    run_jobs(J, n * ns); g_sims[arm] += n * ns;
    for (int i = 0; i < n; i++) for (int s = 0; s < ns; s++) {
        Cand *c = &A->c[idx[i]]; SimRes *r = &J[i * ns + s].r;
        if (stage <= 1) { c->nev++; c->nem += r->emerged; c->fs += r->fitness; }
        else if (stage == 2) { c->nev4++; c->nem4 += r->emerged; }
        else { c->nvv++; c->nvm += r->emerged; }
        g_cry[arm] += r->crystal;
        if (r->emerged && r->has_dom && r->ess >= 0 && (!c->has_best || stage >= 2)) { c->best = *r; c->has_best = 1; }
    }
    free(J);
}
static Cand **ranked(Arch *A, int which) {
    Cand **p = malloc(sizeof *p * (size_t)A->n);
    for (int i = 0; i < A->n; i++) p[i] = &A->c[i];
    qsort(p, (size_t)A->n, sizeof *p, which ? cmp_cand4 : cmp_cand);
    return p;
}
static int c_idx(Arch *A, Cand *c) { return (int)(c - A->c); }

static double secs(struct timespec *t0) { struct timespec t1; clock_gettime(CLOCK_MONOTONIC, &t1); return (t1.tv_sec - t0->tv_sec) + (t1.tv_nsec - t0->tv_nsec) * 1e-9; }

static int cmd_race(int argc, char **argv) {
    int N0 = 1024, E = 1024, E1 = 128, pop = 48, gens = 12, K1 = 48, K2 = 12, K3 = 6, V = 12, N4 = 4096; uint64_t seed = 1;
    const char *save = NULL;
    for (int i = 2; i + 1 < argc; i += 2) {
        if (!strcmp(argv[i], "--seed")) seed = strtoull(argv[i + 1], NULL, 0);
        else if (!strcmp(argv[i], "--pop")) pop = atoi(argv[i + 1]);
        else if (!strcmp(argv[i], "--gens")) gens = atoi(argv[i + 1]);
        else if (!strcmp(argv[i], "--N0")) N0 = atoi(argv[i + 1]);
        else if (!strcmp(argv[i], "--N4")) N4 = atoi(argv[i + 1]);
        else if (!strcmp(argv[i], "--epochs")) E = atoi(argv[i + 1]);
        else if (!strcmp(argv[i], "--K1")) K1 = atoi(argv[i + 1]);
        else if (!strcmp(argv[i], "--K2")) K2 = atoi(argv[i + 1]);
        else if (!strcmp(argv[i], "--K3")) K3 = atoi(argv[i + 1]);
        else if (!strcmp(argv[i], "--V")) V = atoi(argv[i + 1]);
        else if (!strcmp(argv[i], "--threads")) g_threads = atoi(argv[i + 1]);
        else if (!strcmp(argv[i], "--save")) save = argv[i + 1];
        else { fprintf(stderr, "option inconnue %s\n", argv[i]); return 2; }
    }
    FILE *o = stdout; struct timespec t0; clock_gettime(CLOCK_MONOTONIC, &t0);
    printf("# universe_search2 race seed=%llu pop=%d gens=%d N0=%d N4=%d E=%d K1=%d K2=%d K3=%d V=%d threads=%d\n",
           (unsigned long long)seed, pop, gens, N0, N4, E, K1, K2, K3, V, g_threads);
    Arch A[2] = { { 0 } }; Cand *fin[2][64]; int nf[2];
    for (int arm = 0; arm < 2; arm++) {
        Rng rng; rng_seed(&rng, mix(seed, 0xB000 + (uint64_t)arm));
        int *cur = malloc(sizeof(int) * (size_t)pop), *nxt = malloc(sizeof(int) * (size_t)pop);
        for (int i = 0; i < pop; i++) { Univ u; u_random(&u, &rng); cur[i] = arch_get(&A[arm], &u); }
        fprintf(o, "\n## %s — étape 0 : %d × %d générations, 2 graines par évaluation à N=%d, E=%d\n",
                arm ? "TÉMOIN aléatoire" : "SÉLECTION", pop, gens, N0, E);
        fprintf(o, "# gen  émergences/(2×%d)  archive  univers_fertiles(≥1)  meilleur_score(nem/nev)  t\n", pop);
        for (int g = 0; g < gens; g++) {
            int em0 = 0; for (int i = 0; i < pop; i++) em0 += A[arm].c[cur[i]].nem;
            eval_list(&A[arm], cur, pop, 2, N0, E, E1, mix(seed, ((uint64_t)arm << 44) ^ ((uint64_t)g << 24) ^ 0x10), 0, arm);
            int em1 = 0; for (int i = 0; i < pop; i++) em1 += A[arm].c[cur[i]].nem;
            /* dédoublonnage approximatif : em1-em0 compte chaque doublon une fois de plus, sans gravité pour l'affichage */
            Cand **rk = ranked(&A[arm], 0); int fert = 0; for (int i = 0; i < A[arm].n; i++) fert += A[arm].c[i].nem > 0;
            fprintf(o, "%4d  %3d  %4d  %3d  %d/%d  %.0fs\n", g, em1 - em0, A[arm].n, fert, rk[0]->nem, rk[0]->nev, secs(&t0)); fflush(o);
            if (g == gens - 1) { free(rk); break; }
            if (arm == 1) { for (int i = 0; i < pop; i++) { Univ u; u_random(&u, &rng); nxt[i] = arch_get(&A[arm], &u); } }
            else {
                /* parents = meilleurs univers de la population courante (classés par score robuste) */
                Cand **pp = malloc(sizeof *pp * (size_t)pop); int np = 0;
                for (int i = 0; i < pop; i++) { Cand *c = &A[arm].c[cur[i]]; int d = 0; for (int k = 0; k < np; k++) d |= pp[k] == c; if (!d) pp[np++] = c; }
                qsort(pp, (size_t)np, sizeof *pp, cmp_cand);
                int elite = pop / 4 < np ? pop / 4 : np;
                for (int i = 0; i < elite; i++) nxt[i] = c_idx(&A[arm], pp[i]);   /* élites : +2 graines à chaque génération (racing) */
                int half = np / 2 > 1 ? np / 2 : 1;
                Univ PU[256]; for (int i = 0; i < half; i++) PU[i] = pp[i]->u;   /* copie : l'archive peut être réallouée */
                for (int i = elite; i < pop; i++) {
                    Univ u; int a = (int)rng_below(&rng, (uint32_t)half), b = (int)rng_below(&rng, (uint32_t)half);
                    if (rng_double(&rng) < 0.5) u_cross(&u, &PU[a], &PU[b], &rng); else u = PU[a];
                    u_mutate(&u, &rng, 0);
                    nxt[i] = arch_get(&A[arm], &u);
                }
                free(pp);
            }
            free(rk);
            int *t = cur; cur = nxt; nxt = t;
        }
        long s0 = g_sims[arm];
        /* étape 1 */
        Cand **rk = ranked(&A[arm], 0); int k1 = K1 < A[arm].n ? K1 : A[arm].n; int idx[512];
        for (int i = 0; i < k1; i++) idx[i] = c_idx(&A[arm], rk[i]);
        free(rk);
        eval_list(&A[arm], idx, k1, 4, N0, E, E1, mix(seed, 0xC100 + (uint64_t)arm), 1, arm);
        int e1 = 0, n1 = 0; for (int i = 0; i < k1; i++) { e1 += A[arm].c[idx[i]].nem; n1 += A[arm].c[idx[i]].nev; }
        fprintf(o, "étape 1 : top %d de l'archive (%d univers), +4 graines à N=%d → cumul %d/%d émergences chez ces %d  (%.0fs)\n", k1, A[arm].n, N0, e1, n1, k1, secs(&t0));
        /* étape 2 */
        rk = ranked(&A[arm], 0); int k2 = K2 < k1 ? K2 : k1;
        for (int i = 0; i < k2; i++) idx[i] = c_idx(&A[arm], rk[i]);
        free(rk);
        eval_list(&A[arm], idx, k2, 4, N4, E, E1, mix(seed, 0xC200 + (uint64_t)arm), 2, arm);
        int e2 = 0; for (int i = 0; i < k2; i++) e2 += A[arm].c[idx[i]].nem4;
        fprintf(o, "étape 2 : top %d, 4 graines à N=%d → %d/%d émergences ; par univers :", k2, N4, e2, 4 * k2);
        for (int i = 0; i < k2; i++) fprintf(o, " %d", A[arm].c[idx[i]].nem4);
        fprintf(o, "  (%.0fs)\n", secs(&t0));
        /* finalistes : classés par N=4096 parmi les k2 */
        Cand *pk[512]; for (int i = 0; i < k2; i++) pk[i] = &A[arm].c[idx[i]];
        qsort(pk, (size_t)k2, sizeof *pk, cmp_cand4);
        nf[arm] = K3 < k2 ? K3 : k2;
        for (int i = 0; i < nf[arm]; i++) { idx[i] = c_idx(&A[arm], pk[i]); }
        eval_list(&A[arm], idx, nf[arm], V, N4, E, E1, mix(seed, 0xD000 + (uint64_t)arm), 3, arm);
        for (int i = 0; i < nf[arm]; i++) fin[arm][i] = &A[arm].c[idx[i]];
        fprintf(o, "budget : %ld simulations à N0 (étape 0 : %ld), %d à N=%d hors revalidation ; cristaux rencontrés : %d\n",
                s0 + 4L * k1, s0, 4 * k2, N4, g_cry[arm]);
        free(cur); free(nxt);
    }
    /* revalidation : graines jamais vues, N4 */
    fprintf(o, "\n## Revalidation des finalistes : %d graines JAMAIS VUES par univers, N=%d, E=%d\n", V, N4, E);
    int tot[2] = { 0, 0 }, tr[2]; double rate[2][64];
    for (int arm = 0; arm < 2; arm++) {
        tr[arm] = nf[arm] * V;
        fprintf(o, "%s : par univers (reval/%d | étape2/4 | N0 nem/nev) :", arm ? "témoin   " : "sélection", V);
        for (int i = 0; i < nf[arm]; i++) { Cand *c = fin[arm][i]; tot[arm] += c->nvm; rate[arm][i] = (double)c->nvm / V; fprintf(o, "  %d|%d|%d/%d", c->nvm, c->nem4, c->nem, c->nev); }
        double lo, hi; wilson(tot[arm], tr[arm], &lo, &hi);
        fprintf(o, "\n   total %d/%d = %.1f %%  [IC95 %.1f–%.1f %%]\n", tot[arm], tr[arm], 100.0 * tot[arm] / tr[arm], 100 * lo, 100 * hi);
    }
    double pf1 = fisher_greater(tot[0], tr[0] - tot[0], tot[1], tr[1] - tot[1]), pf2 = fisher_two(tot[0], tr[0] - tot[0], tot[1], tr[1] - tot[1]);
    double pp = perm_test(rate[0], nf[0], rate[1], nf[1]);
    fprintf(o, "Fisher exact (essais poolés) : unilatéral p = %.3g, bilatéral p = %.3g ; permutation par univers (moyenne des taux, unilatéral) p = %.3g\n", pf1, pf2, pp);
    /* origine à N4, mêmes conditions */
    {
        Univ og; u_origin(&og, 8192); Arch OA = { 0 }; int oi = arch_get(&OA, &og);
        eval_list(&OA, &oi, 1, V, N4, E, E1, mix(seed, 0xD0D0), 3, 0); g_sims[0] -= V;
        fprintf(o, "origine (soupe b, 8192 pas) à N=%d : %d/%d  | Fisher sélection > origine p = %.3g ; témoin > origine p = %.3g\n", N4, OA.c[0].nvm, V,
                fisher_greater(tot[0], tr[0] - tot[0], OA.c[0].nvm, V - OA.c[0].nvm), fisher_greater(tot[1], tr[1] - tot[1], OA.c[0].nvm, V - OA.c[0].nvm));
        free(OA.c);
    }
    fprintf(o, "\n## Profil des finalistes\n");
    for (int arm = 0; arm < 2; arm++) for (int i = 0; i < nf[arm]; i++) {
        char tag[64]; snprintf(tag, sizeof tag, "%s#%d (reval %d/%d)", arm ? "T" : "S", i, fin[arm][i]->nvm, V);
        print_univ(o, tag, &fin[arm][i]->u, fin[arm][i]->has_best ? &fin[arm][i]->best : NULL);
    }
    if (save) {
        FILE *f = fopen(save, "wb");
        if (f) { for (int arm = 0; arm < 2; arm++) { fwrite(&nf[arm], sizeof(int), 1, f); for (int i = 0; i < nf[arm]; i++) fwrite(fin[arm][i], sizeof(Cand), 1, f); } fclose(f); }
    }
    fprintf(o, "\ntemps total %.0fs\n", secs(&t0));
    free(A[0].c); free(A[1].c);
    return 0;
}

static int load_fin(const char *path, Cand fin[2][64], int nf[2]) {
    FILE *f = fopen(path, "rb"); if (!f) return -1;
    for (int arm = 0; arm < 2; arm++) { if (fread(&nf[arm], sizeof(int), 1, f) != 1) { fclose(f); return -1; } for (int i = 0; i < nf[arm]; i++) if (fread(&fin[arm][i], sizeof(Cand), 1, f) != 1) { fclose(f); return -1; } }
    fclose(f); return 0;
}

/* ---------------- volet 2 : origine à grand N ---------------- */
static int cmd_origin(int argc, char **argv) {
    int N = 16384, E = 1024, ns = 8, nsel = 2; const char *load = NULL; uint64_t seed = 1;
    for (int i = 2; i + 1 < argc; i += 2) {
        if (!strcmp(argv[i], "--N")) N = atoi(argv[i + 1]);
        else if (!strcmp(argv[i], "--epochs")) E = atoi(argv[i + 1]);
        else if (!strcmp(argv[i], "--seeds")) ns = atoi(argv[i + 1]);
        else if (!strcmp(argv[i], "--nsel")) nsel = atoi(argv[i + 1]);
        else if (!strcmp(argv[i], "--load")) load = argv[i + 1];
        else if (!strcmp(argv[i], "--seed")) seed = strtoull(argv[i + 1], NULL, 0);
        else if (!strcmp(argv[i], "--threads")) g_threads = atoi(argv[i + 1]);
        else { fprintf(stderr, "option inconnue %s\n", argv[i]); return 2; }
    }
    static Cand fin[2][64]; int nf[2] = { 0, 0 };
    if (load && load_fin(load, fin, nf)) { fprintf(stderr, "lecture %s impossible\n", load); return 1; }
    Univ U[16]; char lab[16][32]; int nu = 0;
    u_origin(&U[nu], 8192); snprintf(lab[nu++], 32, "origine-8192");
    for (int i = 0; i < nf[0] && i < nsel; i++) { U[nu] = fin[0][i].u; snprintf(lab[nu++], 32, "S#%d", i); }
    for (int i = 0; i < nf[1] && i < nsel; i++) { U[nu] = fin[1][i].u; snprintf(lab[nu++], 32, "T#%d", i); }
    printf("# universe_search2 origin N=%d E=%d graines=%d (neuves, sel %llu)\n", N, E, ns, (unsigned long long)seed);
    struct timespec t0; clock_gettime(CLOCK_MONOTONIC, &t0);
    Job *J = calloc((size_t)(nu * ns), sizeof *J);
    for (int i = 0; i < nu; i++) for (int s = 0; s < ns; s++) {
        Job *j = &J[i * ns + s]; j->u = &U[i]; memset(&j->c, 0, sizeof j->c);
        j->c.N = N; j->c.E = E; j->c.E1 = 0; j->c.seed = mix(seed ^ 0x16384ULL, (uint64_t)s);   /* mêmes graines pour tous : comparaison appariée */
    }
    run_jobs(J, nu * ns);
    int em[16];
    for (int i = 0; i < nu; i++) {
        em[i] = 0; printf("%-13s :", lab[i]);
        for (int s = 0; s < ns; s++) { SimRes *r = &J[i * ns + s].r; em[i] += r->emerged; if (r->emerged) printf(" e@%d(%.2f,ess%d)", r->t_emerge, r->cfrac_final, r->ess); else printf(" -%s(b%ld)", r->any_emerged ? "cristal" : "", r->births); }
        printf("   → %d/%d\n", em[i], ns);
    }
    for (int i = 1; i < nu; i++) printf("Fisher %s > origine : p = %.3g ; origine > %s : p = %.3g\n", lab[i], fisher_greater(em[i], ns - em[i], em[0], ns - em[0]), lab[i], fisher_greater(em[0], ns - em[0], em[i], ns - em[i]));
    for (int i = 0; i < nu; i++) for (int s = 0; s < ns; s++) if (J[i * ns + s].r.emerged) { char t[64]; snprintf(t, 64, "%s graine %d", lab[i], s); print_univ(stdout, t, &U[i], &J[i * ns + s].r); break; }
    printf("temps %.0fs\n", secs(&t0));
    free(J); return 0;
}

/* ---------------- volet 3 : dérive lente de la physique ---------------- */
/* un pas de dérive : 90 % une entrée de table (q < kper), 10 % un paramètre (rcost, wcost, steps, mut) ; L, kper, addr fixés */
static int drift_step(Univ *u, Rng *r, char *what, size_t cap) {
    if (rng_double(r) < 0.9) {
        int q = (int)rng_below(r, u->kper), b = (int)rng_below(r, 256), p = (int)rng_below(r, P_COUNT);
        snprintf(what, cap, "tab[%d][%02x] %s→%s", q, b, PN[u->tab[q][b]], PN[p]);
        u->tab[q][b] = (uint8_t)p; for (int k = u->kper; k < 4; k++) memcpy(u->tab[k], u->tab[k % u->kper], 256);
        return b;
    }
    switch (rng_below(r, 4)) {
    case 0: u->rcost = (uint8_t)COSTS[rng_below(r, 3)]; snprintf(what, cap, "rcost=%d", u->rcost); break;
    case 1: u->wcost = (uint8_t)COSTS[rng_below(r, 3)]; snprintf(what, cap, "wcost=%d", u->wcost); break;
    case 2: u->steps = (uint16_t)STEPS[rng_below(r, 4)]; snprintf(what, cap, "steps=%d", u->steps); break;
    default: u->mut_i = (uint8_t)rng_below(r, 5); snprintf(what, cap, "mut=%g", MUTS[u->mut_i]); break;
    }
    return -1;
}
static int u_tabdist(const Univ *a, const Univ *b) { int d = 0; for (int q = 0; q < 4; q++) for (int x = 0; x < 256; x++) d += a->tab[q][x] != b->tab[q][x]; return d / (4 / a->kper); }

typedef struct {
    Univ base; uint8_t rep[LMAX]; int N, Eest, Edrift, K, mode; uint64_t seed;   /* mode 0 dérive, 1 saut brutal vers end, 2 sans dérive */
    Univ end;                          /* mode 1 : cible ; sortie mode 0 : point d'arrivée */
    /* sorties */
    int established; double cf_est, cf_end, cf_min; int alive, t_death, nchanges, ness_hits, rep_still, dom_ess, dom_ham, ess_changed, dom_has; double curve[80]; int ncurve;
    uint8_t dom[LMAX], ess0[LMAX], essE[LMAX]; int ess0n;
} DriftJob;

static void drift_run(DriftJob *D) {
    Univ u = D->base; const int L = u.L, N = D->N & ~1, fid = fidof(L);
    uint8_t *s = malloc((size_t)N * L); int *perm = malloc(sizeof(int) * (size_t)N);
    Rng rng, dr; rng_seed(&rng, D->seed ^ 0xD81F7ULL); rng_seed(&dr, D->seed ^ 0x5EEDULL ^ ((uint64_t)D->K << 32));
    for (size_t i = 0; i < (size_t)N * L; i++) s[i] = (uint8_t)rng_next(&rng);
    for (int k = 0; k < N / 8; k++) memcpy(s + (size_t)k * L, D->rep, L);     /* 1/8 de la soupe ensemencée, puis établissement */
    for (int i = 0; i < N; i++) perm[i] = i;
    D->ess0n = u_ablate(D->rep, &u, D->ess0);
    int Etot = D->Eest + D->Edrift; D->t_death = -1; D->cf_min = 1; D->ncurve = 0;
    for (int e = 1; e <= Etot; e++) {
        int de = e - D->Eest;
        if (de == 1 && D->mode == 1) u = D->end;
        if (de > 0 && D->mode == 0 && de % D->K == 0) { char w[64]; int b = drift_step(&u, &dr, w, sizeof w); D->nchanges++; if (b >= 0) for (int i = 0; i < L; i++) if (D->ess0[i] && D->rep[i] == b) { D->ness_hits++; break; } }
        for (int i = N - 1; i > 0; i--) { int j = (int)rng_below(&rng, (uint32_t)(i + 1)), t = perm[i]; perm[i] = perm[j]; perm[j] = t; }
        for (int i = 0; i < N; i += 2) {
            uint8_t tape[TMAX]; uint8_t *pa = s + (size_t)perm[i] * L, *pb = s + (size_t)perm[i + 1] * L;
            memcpy(tape, pa, L); memcpy(tape + L, pb, L);
            u_exec(tape, &u, NULL, 0, 0);
            memcpy(pa, tape, L); memcpy(pb, tape + L, L);
        }
        double pmut = MUTS[u.mut_i];
        if (pmut > 0) { double lq = log(1.0 - pmut); size_t tot = (size_t)N * L, i = 0; for (;;) { i += (size_t)(log(1.0 - rng_double(&rng)) / lq); if (i >= tot) break; s[i] = (uint8_t)rng_next(&rng); i++; } }
        if (e % CK == 0 || e == Etot) {
            double cf = sample_copiers(s, N, &u, D->seed * 31 + (uint64_t)e, NULL, NULL);
            if (D->ncurve < 80) D->curve[D->ncurve++] = cf;
            if (e == D->Eest) { D->cf_est = cf; D->established = cf >= 0.25; }
            if (de > 0) { if (cf < D->cf_min) D->cf_min = cf; if (cf < 0.05 && D->t_death < 0) D->t_death = de; }
        }
    }
    (void)fid;
    D->cf_end = sample_copiers(s, N, &u, D->seed * 31 + 9999, D->dom, &D->dom_ess);
    D->dom_has = D->dom_ess > 0;
    D->alive = D->cf_end >= 0.25;
    D->rep_still = u_copier(D->rep, &u) != 0;
    D->ess_changed = -1; D->dom_ham = -1;
    if (D->dom_has) {
        D->dom_ham = L - same(D->dom, D->rep, L);
        int n = u_ablate(D->dom, &u, D->essE); D->dom_ess = n;
        if (n >= 0 && D->ess0n >= 0) { int c = 0; for (int i = 0; i < L; i++) c += D->essE[i] != D->ess0[i] || (D->essE[i] && D->dom[i] != D->rep[i]); D->ess_changed = c; }
    }
    if (D->mode != 1) D->end = u;   /* mode 2 : u == base (distance 0) */
    free(s); free(perm);
}
static void *drift_worker(void *arg) {
    void **a = arg; DriftJob *J = a[0]; int n = *(int *)a[1]; int *next = a[2]; pthread_mutex_t *mu = a[3];
    for (;;) { pthread_mutex_lock(mu); int i = (*next)++; pthread_mutex_unlock(mu); if (i >= n) return NULL; drift_run(&J[i]); }
}
static void run_drift(DriftJob *J, int n) {
    int next = 0; pthread_mutex_t mu = PTHREAD_MUTEX_INITIALIZER; void *a[4] = { J, &n, &next, &mu };
    pthread_t th[64]; int nt = g_threads < 64 ? g_threads : 64;
    for (int i = 0; i < nt; i++) pthread_create(&th[i], NULL, drift_worker, a);
    for (int i = 0; i < nt; i++) pthread_join(th[i], NULL);
}

static int cmd_drift(int argc, char **argv) {
    int N = 1024, Eest = 512, Ed = 4096, reps = 4, nsrc = 2; int from = 0; const char *load = NULL; int Ks[8] = { 512, 128, 32, 8 }, nK = 4; uint64_t seed = 7;
    for (int i = 2; i + 1 < argc; i += 2) {
        if (!strcmp(argv[i], "--N")) N = atoi(argv[i + 1]);
        else if (!strcmp(argv[i], "--est")) Eest = atoi(argv[i + 1]);
        else if (!strcmp(argv[i], "--drift")) Ed = atoi(argv[i + 1]);
        else if (!strcmp(argv[i], "--reps")) reps = atoi(argv[i + 1]);
        else if (!strcmp(argv[i], "--nsrc")) nsrc = atoi(argv[i + 1]);
        else if (!strcmp(argv[i], "--from")) from = atoi(argv[i + 1]);   /* 0 = finalistes sélection puis témoin ; 1 = témoin seulement */
        else if (!strcmp(argv[i], "--load")) load = argv[i + 1];
        else if (!strcmp(argv[i], "--seed")) seed = strtoull(argv[i + 1], NULL, 0);
        else if (!strcmp(argv[i], "--threads")) g_threads = atoi(argv[i + 1]);
        else if (!strcmp(argv[i], "--K")) { nK = 0; char *p = argv[i + 1]; while (*p && nK < 8) { Ks[nK++] = (int)strtol(p, &p, 10); if (*p == ',') p++; } }
        else { fprintf(stderr, "option inconnue %s\n", argv[i]); return 2; }
    }
    static Cand fin[2][64]; int nf[2];
    if (!load || load_fin(load, fin, nf)) { fprintf(stderr, "--load finalistes requis\n"); return 1; }
    /* sources : finalistes (sélection d'abord, puis témoin) ayant un réplicateur dominant vérifié */
    Cand *src[8]; int ns = 0;
    for (int arm = from; arm < 2 && ns < nsrc; arm++) for (int i = 0; i < nf[arm] && ns < nsrc; i++) if (fin[arm][i].has_best && u_copier(fin[arm][i].best.dom, &fin[arm][i].u)) src[ns++] = &fin[arm][i];
    printf("# universe_search2 drift N=%d établissement=%d époques, dérive=%d époques, K=", N, Eest, Ed);
    for (int k = 0; k < nK; k++) printf("%d%s", Ks[k], k + 1 < nK ? "," : "");
    printf(" ; %d sources × %d répliques\n", ns, reps);
    struct timespec t0; clock_gettime(CLOCK_MONOTONIC, &t0);
    /* phase A : dérive (mode 0) pour chaque K + sans dérive (mode 2) */
    int nA = ns * reps * (nK + 1); DriftJob *A = calloc((size_t)nA, sizeof *A);
    for (int si = 0; si < ns; si++) for (int r = 0; r < reps; r++) for (int k = 0; k <= nK; k++) {
        DriftJob *D = &A[(si * reps + r) * (nK + 1) + k];
        D->base = src[si]->u; memcpy(D->rep, src[si]->best.dom, LMAX); D->N = N; D->Eest = Eest; D->Edrift = Ed;
        D->K = k < nK ? Ks[k] : 0; D->mode = k < nK ? 0 : 2; D->seed = mix(seed, (uint64_t)(si * 100 + r));   /* même soupe initiale pour tous les K */
    }
    run_drift(A, nA);
    /* phase B : saut brutal vers le même point d'arrivée que chaque trajectoire de dérive */
    int nB = ns * reps * nK; DriftJob *B = calloc((size_t)nB, sizeof *B);
    for (int si = 0; si < ns; si++) for (int r = 0; r < reps; r++) for (int k = 0; k < nK; k++) {
        DriftJob *D = &B[(si * reps + r) * nK + k], *S = &A[(si * reps + r) * (nK + 1) + k];
        *D = *S; D->mode = 1; D->end = S->end; D->nchanges = 0; D->ness_hits = 0;
    }
    run_drift(B, nB);
    for (int si = 0; si < ns; si++) {
        char d[160]; u_desc(&src[si]->u, d, sizeof d);
        printf("\n## source %d : %s ; réplicateur %d octets essentiels\n", si, d, A[si * reps * (nK + 1)].ess0n);
        printf("# condition        vivants/%d  établis  copieurs_fin(moy)  min(moy)  changements  coups_sur_essentiels  dist_table  réplicateur_initial_encore_copieur  dominant: dist_Hamming / octets_ess_changés (médiane)\n", reps);
        for (int k = 0; k <= nK; k++) for (int mode = 0; mode < (k < nK ? 2 : 1); mode++) {
            int alive = 0, est = 0, still = 0, ch = 0, hits = 0, dist = 0; double cf = 0, mn = 0; int hams[64], nh = 0, essc[64], ne = 0;
            for (int r = 0; r < reps; r++) {
                DriftJob *D = mode == 0 ? &A[(si * reps + r) * (nK + 1) + k] : &B[(si * reps + r) * nK + k];
                DriftJob *S = &A[(si * reps + r) * (nK + 1) + k];
                alive += D->alive; est += D->established; still += D->rep_still; ch += S->nchanges; hits += S->ness_hits; cf += D->cf_end; mn += D->cf_min;
                dist += u_tabdist(&D->base, &S->end);
                if (D->alive && D->dom_ham >= 0) { hams[nh++] = D->dom_ham; if (D->ess_changed >= 0) essc[ne++] = D->ess_changed; }
            }
            for (int a = 0; a < nh; a++) for (int b = a + 1; b < nh; b++) if (hams[b] < hams[a]) { int t = hams[a]; hams[a] = hams[b]; hams[b] = t; }
            for (int a = 0; a < ne; a++) for (int b = a + 1; b < ne; b++) if (essc[b] < essc[a]) { int t = essc[a]; essc[a] = essc[b]; essc[b] = t; }
            char lab[32]; if (k == nK) snprintf(lab, 32, "sans dérive"); else snprintf(lab, 32, "%s K=%d", mode ? "SAUT  " : "dérive", Ks[k]);
            printf("%-16s  %d/%d  %d/%d  %.2f  %.2f  %.1f  %.1f  %.1f  %d/%d  %d / %d  (n=%d)\n", lab, alive, reps, est, reps, cf / reps, mn / reps, (double)ch / reps, (double)hits / reps, (double)dist / reps,
                   still, reps, nh ? hams[nh / 2] : -1, ne ? essc[ne / 2] : -1, nh);
        }
        /* détail : courbes et réplicateur adapté */
        for (int k = 0; k < nK; k++) for (int r = 0; r < reps; r++) {
            DriftJob *D = &A[(si * reps + r) * (nK + 1) + k];
            if (D->alive && !D->rep_still) {
                printf("  ADAPTATION K=%d rép %d : réplicateur initial n'est plus copieur sous la physique finale, la population si (copieurs %.2f) ; dominant à %d octets, %d essentiels :\n    ", Ks[k], r, D->cf_end, D->dom_ham, D->dom_ess);
                for (int i = 0; i < D->base.L; i++) printf("%02x", D->dom[i]);
                printf("\n    ess init :"); for (int i = 0; i < D->base.L; i++) if (D->ess0[i]) printf(" %d:%02x=%s", i, D->rep[i], PN[D->base.tab[i & (D->base.kper - 1)][D->rep[i]]]);
                printf("\n    ess fin  :"); for (int i = 0; i < D->base.L; i++) if (D->essE[i]) printf(" %d:%02x=%s", i, D->dom[i], PN[D->end.tab[i & (D->end.kper - 1)][D->dom[i]]]);
                printf("\n");
            }
        }
        for (int k = 0; k <= nK; k++) {
            DriftJob *D = &A[(si * reps) * (nK + 1) + k];
            printf("  courbe copieurs rép 0 %s : ", k < nK ? "dérive" : "sans dérive"); if (k < nK) printf("K=%d ", Ks[k]);
            for (int c = 0; c < D->ncurve; c += 4) printf("%.2f ", D->curve[c]);
            printf("\n");
        }
    }
    printf("\n## bilan poolé (%d sources × %d répliques) : vivants en fin, dérive vs saut vers le même point d'arrivée\n", ns, reps);
    int a2 = 0; for (int si = 0; si < ns; si++) for (int r = 0; r < reps; r++) a2 += A[(si * reps + r) * (nK + 1) + nK].alive;
    printf("sans dérive : %d/%d\n", a2, ns * reps);
    int ta = 0, tb = 0;
    for (int k = 0; k < nK; k++) {
        int a = 0, b = 0, n = ns * reps;
        for (int si = 0; si < ns; si++) for (int r = 0; r < reps; r++) { a += A[(si * reps + r) * (nK + 1) + k].alive; b += B[(si * reps + r) * nK + k].alive; }
        ta += a; tb += b;
        printf("K=%-4d dérive %2d/%d  saut %2d/%d  Fisher bilatéral p = %.3g ; dérive vs sans dérive p = %.3g\n", Ks[k], a, n, b, n, fisher_two(a, n - a, b, n - b), fisher_two(a, n - a, a2, n - a2));
    }
    printf("tous K : dérive %d/%d  saut %d/%d  Fisher bilatéral p = %.3g (unilatéral dérive > saut p = %.3g)\n", ta, nK * ns * reps, tb, nK * ns * reps, fisher_two(ta, nK * ns * reps - ta, tb, nK * ns * reps - tb), fisher_greater(ta, nK * ns * reps - ta, tb, nK * ns * reps - tb));
    printf("temps %.0fs\n", secs(&t0));
    free(A); free(B); return 0;
}

/* selftest v2 : stats, dérive sans écriture multiple, saut = dérive quand K très grand */
static int selftest2(void) {
    int fail = us1_main(2, (char *[]){ "x", "selftest", NULL });
    double p = fisher_greater(8, 2, 1, 9); int ok = p > 0.002 && p < 0.003;     /* valeur de référence 0.00274 */
    double q = fisher_two(3, 1, 1, 3); ok &= q > 0.48 && q < 0.49;               /* 0.4857 */
    printf("selftest2 Fisher : %.5f (réf 0.00274), bilatéral %.4f (réf 0.4857) %s\n", p, q, ok ? "OK" : "ÉCHEC"); fail |= !ok;
    Rng r; rng_seed(&r, 99); Univ u; u_random(&u, &r); long viol = 0;
    for (int it = 0; it < 2000; it++) { char w[64]; drift_step(&u, &r, w, sizeof w); ok = u_valid(&u); if (!ok) viol++;
        uint8_t t[TMAX]; for (int i = 0; i < 2 * u.L; i++) t[i] = (uint8_t)rng_next(&r); if (u_exec(t, &u, NULL, 0, 1) <= -1000000) viol++; }
    printf("selftest2 dérive : 2000 pas, %ld violations (hors catalogue ou > 1 octet écrit) %s\n", viol, viol ? "ÉCHEC" : "OK"); fail |= viol != 0;
    printf("universe_search2 selftest : %s\n", fail ? "ÉCHEC" : "OK");
    return fail;
}

#ifndef US2_NO_MAIN   /* k8_survivor.c inclut ce fichier sans son main */
int main(int argc, char **argv) {
    if (argc >= 2 && !strcmp(argv[1], "selftest")) return selftest2();
    if (argc >= 2 && !strcmp(argv[1], "race")) return cmd_race(argc, argv);
    if (argc >= 2 && !strcmp(argv[1], "origin")) return cmd_origin(argc, argv);
    if (argc >= 2 && !strcmp(argv[1], "drift")) return cmd_drift(argc, argv);
    fprintf(stderr, "usage : universe_search2 selftest | race [--seed S --pop 48 --gens 12 --K1 48 --K2 12 --K3 6 --V 12 --save f] | origin --load f [--N 16384 --epochs 1024 --seeds 8] | drift --load f [--K 512,128,32,8 --reps 4]\n");
    return 2;
}
#endif
