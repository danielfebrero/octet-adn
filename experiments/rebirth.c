/* La vie qui renaît après la mort est-elle plus persistante ? (experiments/universe-search/rebirth)
 *
 * Inclut k8_survivor.c (donc universe_search2.c et v1) sans en changer la logique : même soupe, même dérive,
 * même copieur vérifié, même garde-fou anti-triche (aucune primitive nouvelle ; u_exec inchangé).
 * life_run() est une copie de drift_run() qui consomme EXACTEMENT les mêmes tirages tant que ses options valent
 * celles de drift_run (selftest), et ajoute : continuité STRICTE de lignée (0 copieur vérifié dans la population
 * ENTIÈRE à un point de contrôle = mort), cycles mort/renaissance, effacement de la soupe à la mort (générateur à
 * part), taux de mutation imposé, bifurcation de la graine de dérive à une époque donnée.
 * La physique d'une dérive ne dépend que de son générateur de dérive (indépendant de la soupe) : phys_at() la
 * reconstruit sans simuler la soupe (selftest : == drift_run).
 *
 *   ./experiments/rebirth pool    --load fin2.bin --save pool.bin   # émergences depuis le bruit (bases, fins, milieux de dérive)
 *   ./experiments/rebirth persist --load fin2.bin --pool pool.bin   # Q1 né-de-nouveau vs premier-né
 *   ./experiments/rebirth cycles  --load fin2.bin                   # Q2 cycles mort/renaissance, avec/sans effacement
 *   ./experiments/rebirth features --pool pool.bin                  # Q3 fertile vs stérile : prédicteur simple
 *   ./experiments/rebirth selftest
 */
#define K8_NO_MAIN
#include "k8_survivor.c"

/* ---------------- boucle parallèle générique ---------------- */
typedef void (*PFn)(int i, void *ctx);
typedef struct { PFn f; void *ctx; int n, next; pthread_mutex_t mu; } PFor;
static void *pfor_w(void *a) { PFor *P = a; for (;;) { pthread_mutex_lock(&P->mu); int i = P->next++; pthread_mutex_unlock(&P->mu); if (i >= P->n) return NULL; P->f(i, P->ctx); } }
static void pfor(int n, PFn f, void *ctx) {
    PFor P = { f, ctx, n, 0, PTHREAD_MUTEX_INITIALIZER }; pthread_t th[64]; int nt = g_threads < 64 ? g_threads : 64;
    for (int i = 0; i < nt; i++) pthread_create(&th[i], NULL, pfor_w, &P);
    for (int i = 0; i < nt; i++) pthread_join(th[i], NULL);
}

/* physique d'une dérive à l'époque de dérive de (pas aux de multiples de K, comme drift_run) */
static void phys_replay(const Univ *base, uint64_t seed, int K, int de, Univ *u) {
    *u = *base; Rng dr; rng_seed(&dr, seed ^ 0x5EEDULL ^ ((uint64_t)K << 32));
    for (int d = K; d <= de; d += K) { char w[64]; drift_step(u, &dr, w, sizeof w); }
}
static void phys_at(int src, int rep, int de, Univ *u) { DriftJob D; setup_job(&D, src, rep, DK, 0); phys_replay(&D.base, D.seed, DK, de, u); }

/* ---------------- soupe de vie ---------------- */
#define MAXG 256
typedef struct { int tb, td, hprev, wait, nrelic; uint8_t db[LMAX], dl[LMAX]; } Gen;
typedef struct {
    Univ base; uint8_t rep[LMAX]; int inject, N, Eest, Edrift, K, drift; uint64_t seed;
    int fork_at; uint64_t fseed;            /* à l'époque fork_at, le générateur de dérive est re-semé avec fseed (0 = jamais) */
    double pmut;                            /* < 0 : celui de la physique courante */
    int wipe, stop_death, thr;              /* thr : copieurs (pop entière) pour déclarer une naissance */
    int snap_at; Univ snap;
    /* sorties */
    int t_death;                            /* 1re époque de dérive (de>0) à 0 copieur ; -1 jamais ; -2 mort avant la dérive */
    int ngen, ndeath, nwipe, ncurve, epochs; Gen g[MAXG]; double curve[80], cf_end; Univ end; uint8_t dom[LMAX];
} Life;
static int count_cop(const uint8_t *s, int N, const Univ *u, int lim, int *first) {
    int n = 0; *first = -1;
    for (int i = 0; i < N; i++) if (u_copier(s + (size_t)i * u->L, u)) { if (*first < 0) *first = i; if (++n >= lim) break; }
    return n;
}
static void life_run(Life *X) {
    Univ u = X->base; const int L = u.L, N = X->N & ~1;
    uint8_t *s = malloc((size_t)N * L); int *perm = malloc(sizeof(int) * (size_t)N);
    Rng rng, dr, wr; rng_seed(&rng, X->seed ^ 0xD81F7ULL); rng_seed(&dr, X->seed ^ 0x5EEDULL ^ ((uint64_t)X->K << 32)); rng_seed(&wr, X->seed ^ 0x57495045ULL);
    for (size_t i = 0; i < (size_t)N * L; i++) s[i] = (uint8_t)rng_next(&rng);
    if (X->inject) for (int k = 0; k < N / 8; k++) memcpy(s + (size_t)k * L, X->rep, L);
    for (int i = 0; i < N; i++) perm[i] = i;
    int Etot = X->Eest + X->Edrift, alive = 0; X->t_death = -1; X->ngen = X->ndeath = X->nwipe = X->ncurve = 0;
    if (X->inject) { Gen *G = &X->g[0]; memset(G, 0, sizeof *G); G->td = -1; G->hprev = -1; memcpy(G->db, X->rep, L); memcpy(G->dl, X->rep, L); X->ngen = 1; alive = 1; }
    int e;
    for (e = 1; e <= Etot; e++) {
        int de = e - X->Eest;
        if (X->fork_at && e == X->fork_at) rng_seed(&dr, X->fseed ^ 0x5EEDULL ^ ((uint64_t)X->K << 32));
        if (X->drift && de > 0 && de % X->K == 0) { char w[64]; drift_step(&u, &dr, w, sizeof w); }
        if (X->snap_at == e) X->snap = u;
        for (int i = N - 1; i > 0; i--) { int j = (int)rng_below(&rng, (uint32_t)(i + 1)), t = perm[i]; perm[i] = perm[j]; perm[j] = t; }
        for (int i = 0; i < N; i += 2) {
            uint8_t tape[TMAX]; uint8_t *pa = s + (size_t)perm[i] * L, *pb = s + (size_t)perm[i + 1] * L;
            memcpy(tape, pa, L); memcpy(tape + L, pb, L);
            u_exec(tape, &u, NULL, 0, 0);
            memcpy(pa, tape, L); memcpy(pb, tape + L, L);
        }
        double pmut = X->pmut >= 0 ? X->pmut : MUTS[u.mut_i];
        if (pmut > 0) { double lq = log(1.0 - pmut); size_t tot = (size_t)N * L, i = 0; for (;;) { i += (size_t)(log(1.0 - rng_double(&rng)) / lq); if (i >= tot) break; s[i] = (uint8_t)rng_next(&rng); i++; } }
        if (e % CK == 0 || e == Etot) {
            uint8_t sd[LMAX]; int dc = 0;
            double cf = sample_copiers(s, N, &u, X->seed * 31 + (uint64_t)e, sd, &dc);
            if (X->ncurve < 80) X->curve[X->ncurve++] = cf;
            int first, n = count_cop(s, N, &u, alive ? 1 : X->thr, &first), hd = 0; uint8_t dom[LMAX];
            if (dc > 0) { memcpy(dom, sd, L); hd = 1; } else if (first >= 0) { memcpy(dom, s + (size_t)first * L, L); hd = 1; }
            if (alive) {
                Gen *G = &X->g[X->ngen - 1];
                if (n == 0) {
                    alive = 0; G->td = e; X->ndeath++;
                    if (X->t_death == -1) X->t_death = de > 0 ? de : -2;
                    for (int i = 0; i < N; i++) G->nrelic += ham(s + (size_t)i * L, G->dl, L) <= 2;
                    if (X->stop_death) break;
                    if (X->wipe) { for (size_t i = 0; i < (size_t)N * L; i++) s[i] = (uint8_t)rng_next(&wr); X->nwipe++; }
                } else if (hd) memcpy(G->dl, dom, L);
            } else if (n >= X->thr && hd && X->ngen < MAXG) {
                Gen *G = &X->g[X->ngen]; memset(G, 0, sizeof *G);
                G->tb = e; G->td = -1; memcpy(G->db, dom, L); memcpy(G->dl, dom, L);
                G->hprev = X->ngen ? ham(dom, X->g[X->ngen - 1].dl, L) : -1; G->wait = X->ngen ? e - X->g[X->ngen - 1].td : e;
                X->ngen++; alive = 1;
            }
        }
    }
    X->epochs = e > Etot ? Etot : e;
    int dc; X->cf_end = sample_copiers(s, N, &u, X->seed * 31 + 9999, X->dom, &dc);
    X->end = u; free(s); free(perm);
}
static void life_w(int i, void *ctx) { life_run(&((Life *)ctx)[i]); }

/* ---------------- statistiques de rang ---------------- */
static void ranks(const double *x, int n, double *r) {
    for (int i = 0; i < n; i++) { double lt = 0, eq = 0; for (int j = 0; j < n; j++) { lt += x[j] < x[i]; eq += x[j] == x[i]; } r[i] = lt + (eq + 1) / 2.0; }
}
static double spearman(const double *x, const double *y, int n) {
    if (n < 3) { return 0; }
    double rx[512], ry[512], mx = 0, my = 0, sxy = 0, sxx = 0, syy = 0; ranks(x, n, rx); ranks(y, n, ry);
    for (int i = 0; i < n; i++) { mx += rx[i]; my += ry[i]; } mx /= n; my /= n;
    for (int i = 0; i < n; i++) { sxy += (rx[i] - mx) * (ry[i] - my); sxx += (rx[i] - mx) * (rx[i] - mx); syy += (ry[i] - my) * (ry[i] - my); }
    return sxx > 0 && syy > 0 ? sxy / sqrt(sxx * syy) : 0;
}
/* Kendall stratifié : S = Σ_strates Σ_{i<j} sign(i-j)·sign(y_i-y_j) (index de génération croissant) ; p par permutation intra-strate */
static long kendall_S(const double *y, const int *st, int n) { long S = 0; for (int i = 0; i < n; i++) for (int j = i + 1; j < n; j++) if (st[i] == st[j]) S += (y[j] > y[i]) - (y[j] < y[i]); return S; }
static void kendall_strat(const double *y0, const int *st, int n, long *S, double *tau, double *pup, double *pdn) {
    double y[1024]; memcpy(y, y0, sizeof(double) * (size_t)n); *S = kendall_S(y, st, n);
    long np = 0; for (int i = 0; i < n; i++) for (int j = i + 1; j < n; j++) np += st[i] == st[j];
    *tau = np ? (double)*S / np : 0;
    Rng r; rng_seed(&r, 0x7A0); int ge = 0, le = 0, R = 20000;
    for (int it = 0; it < R; it++) {
        for (int a = 0; a < n;) { int b = a; while (b < n && st[b] == st[a]) b++; for (int i = b - 1; i > a; i--) { int j = a + (int)rng_below(&r, (uint32_t)(i - a + 1)); double t = y[i]; y[i] = y[j]; y[j] = t; } a = b; }
        long s2 = kendall_S(y, st, n); ge += s2 >= *S; le += s2 <= *S;
    }
    *pup = (ge + 1.0) / (R + 1.0); *pdn = (le + 1.0) / (R + 1.0);
}
static double auc(const double *x, const int *lab, int n) {
    double a = 0; long np = 0;
    for (int i = 0; i < n; i++) if (lab[i]) for (int j = 0; j < n; j++) if (!lab[j]) { a += x[i] > x[j] ? 1 : x[i] == x[j] ? 0.5 : 0; np++; }
    return np ? a / np : 0.5;
}
static double p2(const double *x, int nx, const double *y, int ny) { double a = perm_test(x, nx, y, ny), b = perm_test(y, ny, x, nx); double p = 2 * (a < b ? a : b); return p > 1 ? 1 : p; }
static double mean(const double *x, int n) { double s = 0; for (int i = 0; i < n; i++) s += x[i]; return n ? s / n : 0; }

/* ---------------- réservoir d'émergences depuis le bruit ---------------- */
#define PS 48
enum { G_BASE, G_END, G_MID };
static const char *GN[3] = { "départ", "fin de dérive", "milieu de dérive" };
typedef struct { Univ u; int src, rep, de, grp, ns, nem, ncr; uint8_t em[PS], dom[PS][LMAX]; int tem[PS]; } PoolE;
#define POOLMAX 160
static PoolE g_pool[POOLMAX]; static int g_np;
static const int MIDS[3] = { 1024, 2048, 3072 };
static int cmd_pool(const char *save) {
    g_np = 0;
    for (int s = 0; s < 4; s++) { PoolE *p = &g_pool[g_np++]; memset(p, 0, sizeof *p); p->u = g_src[s]->u; p->src = s; p->rep = -1; p->de = 0; p->grp = G_BASE; p->ns = 12; }
    for (int s = 0; s < 4; s++) for (int r = 0; r < 8; r++) { PoolE *p = &g_pool[g_np++]; memset(p, 0, sizeof *p); phys_at(s, r, 4096, &p->u); p->src = s; p->rep = r; p->de = 4096; p->grp = G_END; p->ns = (s == 0 && r == 5) ? PS : 8; }
    for (int s = 0; s < 4; s++) for (int r = 0; r < 8; r++) for (int m = 0; m < 3; m++) { PoolE *p = &g_pool[g_np++]; memset(p, 0, sizeof *p); phys_at(s, r, MIDS[m], &p->u); p->src = s; p->rep = r; p->de = MIDS[m]; p->grp = G_MID; p->ns = 3; }
    int nj = 0; for (int i = 0; i < g_np; i++) nj += g_pool[i].ns;
    Job *J = calloc((size_t)nj, sizeof *J); int k = 0;
    for (int i = 0; i < g_np; i++) for (int s = 0; s < g_pool[i].ns; s++, k++) { J[k].u = &g_pool[i].u; J[k].c.N = 1024; J[k].c.E = 4096; J[k].c.E1 = 0; J[k].c.seed = mix(0xB0B1, (uint64_t)(i * 1000 + s)); }
    fprintf(stderr, "pool : %d physiques, %d soupes de bruit\n", g_np, nj);
    run_jobs(J, nj); k = 0;
    for (int i = 0; i < g_np; i++) for (int s = 0; s < g_pool[i].ns; s++, k++) {
        PoolE *p = &g_pool[i]; SimRes *R = &J[k].r;
        p->em[s] = R->emerged && R->has_dom && u_copier(R->dom, &p->u); p->tem[s] = R->t_emerge; p->nem += p->em[s]; p->ncr += R->crystal;
        memcpy(p->dom[s], R->dom, LMAX);
    }
    free(J);
    printf("# réservoir : émergence depuis le bruit pur (N=1024, E=4096, émergence = copieurs ≥ 0.25, cristaux exclus, dominant vérifié)\n");
    for (int i = 0; i < g_np; i++) { PoolE *p = &g_pool[i]; if (p->grp == G_MID) continue; printf("%-4s %-14s rép %2d de %4d : %2d/%-2d émergences", g_lab[p->src], GN[p->grp], p->rep, p->de, p->nem, p->ns);
        for (int s = 0; s < p->ns; s++) if (p->em[s]) { printf("  ex. "); hex(stdout, p->dom[s], p->u.L); printf(" (t=%d)", p->tem[s]); break; }
        printf("\n"); }
    int mf = 0, mt = 0; for (int i = 0; i < g_np; i++) if (g_pool[i].grp == G_MID) { mf += g_pool[i].nem > 0; mt++; }
    printf("milieux de dérive (de 1024/2048/3072, 3 graines) : %d/%d physiques fertiles\n", mf, mt);
    if (save) { FILE *f = fopen(save, "wb"); if (!f) return 1; fwrite("RBP1", 1, 4, f); fwrite(&g_np, sizeof g_np, 1, f); fwrite(g_pool, sizeof(PoolE), (size_t)g_np, f); fclose(f); }
    return 0;
}
static int load_pool(const char *path) {
    FILE *f = path ? fopen(path, "rb") : NULL; char m[4]; if (!f) return -1;
    int ok = fread(m, 1, 4, f) == 4 && !memcmp(m, "RBP1", 4) && fread(&g_np, sizeof g_np, 1, f) == 1 && g_np <= POOLMAX && fread(g_pool, sizeof(PoolE), (size_t)g_np, f) == (size_t)g_np;
    fclose(f); return ok ? 0 : -1;
}

/* ---------------- Q1 : persistance ---------------- */
#define RMAX 64
enum { C_FIRST, C_SURV, C_REBORN, C_DEP, C_END };
static const char *CN[5] = { "premier injecté S#0", "renaissant survivant", "renaissance indépendante (S#0 r5 finale)", "premier-né, physique de départ", "premier-né, physique de fin de dérive" };
typedef struct { uint8_t g[LMAX]; Univ u; int cat, src, born_end; char lab[48];
    double rob[4], mrob, life, surv, msurv[4]; int ess; } Rep;
static Rep g_rep[RMAX]; static int g_nr;
static const int RN[4] = { 8, 32, 128, 352 };
static const double PM[4] = { -1, 1e-3, 4e-3, 1.6e-2 };
#define DS 8
#define MSD 2
static int add_rep(const uint8_t *g, const Univ *u, int cat, int src, const char *lab) {
    if (g_nr >= RMAX || !u_copier(g, u)) return 0;
    for (int i = 0; i < g_nr; i++) if (!memcmp(&g_rep[i].u, u, sizeof *u) && !memcmp(g_rep[i].g, g, u->L)) return 0;
    Rep *R = &g_rep[g_nr++]; memset(R, 0, sizeof *R); memcpy(R->g, g, u->L); R->u = *u; R->cat = cat; R->src = src; R->born_end = cat == C_SURV || cat == C_REBORN || cat == C_END;
    snprintf(R->lab, sizeof R->lab, "%s", lab); return 1;
}
static void rob_w(int i, void *ctx) {
    (void)ctx; Rep *R = &g_rep[i]; Rng r; rng_seed(&r, mix(0xB0B, (uint64_t)i));
    for (int k = 0; k < 4; k++) { int ok = 0; for (int t = 0; t < 200; t++) { Univ v = R->u; perturb(&v, RN[k], &r); ok += u_copier(R->g, &v) != 0; } R->rob[k] = ok / 200.0; }
    int ok = 0, tot = 0; uint8_t m[LMAX]; memcpy(m, R->g, R->u.L);
    for (int p = 0; p < R->u.L; p++) { for (int v = 0; v < 256; v++) if (v != R->g[p]) { m[p] = (uint8_t)v; ok += u_copier(m, &R->u) != 0; tot++; } m[p] = R->g[p]; }
    R->mrob = (double)ok / tot; uint8_t ess[LMAX]; R->ess = u_ablate(R->g, &R->u, ess);
}
static int cmd_persist(void) {
    const uint8_t SURV[16] = { 0x35, 0, 0, 0, 0xf1, 0x86, 0x52, 0xc6, 0xc6, 0x3b, 0x86, 0xf1, 0, 0, 0, 0x35 };
    PoolE *E5 = NULL; for (int i = 0; i < g_np; i++) if (g_pool[i].grp == G_END && g_pool[i].src == 0 && g_pool[i].rep == 5) E5 = &g_pool[i];
    g_nr = 0; char b[48];
    add_rep(g_src[0]->best.dom, &g_src[0]->u, C_FIRST, 0, "S#0 injecté");
    if (!add_rep(SURV, &E5->u, C_SURV, 0, "survivant 3500…0035")) { fprintf(stderr, "survivant non copieur sous la physique reconstruite\n"); return 1; }
    { int n = 0; for (int s = 0; s < E5->ns && n < 8; s++) if (E5->em[s]) { snprintf(b, sizeof b, "S#0r5fin g%d", s); n += add_rep(E5->dom[s], &E5->u, C_REBORN, 0, b); } }
    for (int s = 1; s < 4; s++) { snprintf(b, sizeof b, "%s injecté", g_lab[s]); add_rep(g_src[s]->best.dom, &g_src[s]->u, C_DEP, s, b); }
    for (int i = 0; i < g_np; i++) { PoolE *p = &g_pool[i]; if (p->grp == G_MID || p == E5) continue; int n = 0;
        for (int s = 0; s < p->ns && n < 3; s++) if (p->em[s]) { if (p->grp == G_BASE) snprintf(b, sizeof b, "%s base g%d", g_lab[p->src], s); else snprintf(b, sizeof b, "%s r%d fin g%d", g_lab[p->src], p->rep, s);
            n += add_rep(p->dom[s], &p->u, p->grp == G_BASE ? C_DEP : C_END, p->src, b); } }
    fprintf(stderr, "persist : %d réplicateurs\n", g_nr);
    pfor(g_nr, rob_w, NULL);
    /* nouvelle dérive K=8, continuité stricte */
    Life *X = calloc((size_t)g_nr * DS, sizeof *X);
    for (int i = 0; i < g_nr; i++) for (int d = 0; d < DS; d++) { Life *x = &X[i * DS + d]; x->base = g_rep[i].u; memcpy(x->rep, g_rep[i].g, LMAX); x->inject = 1; x->N = 1024; x->Eest = 512; x->Edrift = 4096; x->K = 8; x->drift = 1;
        x->seed = mix(0xD21F7, (uint64_t)(i * 64 + d)); x->pmut = -1; x->stop_death = 1; x->thr = 1; }
    pfor(g_nr * DS, life_w, X);
    for (int i = 0; i < g_nr; i++) { double s = 0; int sv = 0; for (int d = 0; d < DS; d++) { int t = X[i * DS + d].t_death; double l = t == -1 ? 4096 : t == -2 ? 0 : t; s += l; sv += t == -1; } g_rep[i].life = s / DS; g_rep[i].surv = (double)sv / DS; }
    free(X);
    /* taux de mutation imposé, sans dérive */
    X = calloc((size_t)g_nr * 4 * MSD, sizeof *X);
    for (int i = 0; i < g_nr; i++) for (int m = 0; m < 4; m++) for (int d = 0; d < MSD; d++) { Life *x = &X[(i * 4 + m) * MSD + d]; x->base = g_rep[i].u; memcpy(x->rep, g_rep[i].g, LMAX); x->inject = 1; x->N = 1024; x->Eest = 0; x->Edrift = 2048; x->K = 8; x->drift = 0;
        x->seed = mix(0x307A, (uint64_t)(i * 64 + m * 8 + d)); x->pmut = PM[m]; x->stop_death = 1; x->thr = 1; }
    pfor(g_nr * 4 * MSD, life_w, X);
    for (int i = 0; i < g_nr; i++) for (int m = 0; m < 4; m++) { int sv = 0; for (int d = 0; d < MSD; d++) sv += X[(i * 4 + m) * MSD + d].t_death == -1; g_rep[i].msurv[m] = (double)sv / MSD; }
    free(X);
    printf("# Q1 persistance, même protocole pour chaque réplicateur, dans SA physique de naissance\n");
    printf("#  rob@n : part de 200 tables à n entrées re-tirées où le génome copie encore ; mrob : part des L×255 mutants ponctuels copieurs\n");
    printf("#  dérive : nouvelle dérive K=8 depuis sa physique, %d graines, ensemencé 1/8, établissement 512, dérive 4096 ; mort = 0 copieur vérifié dans la pop ENTIÈRE (contrôle /64 ép.) ;\n", DS);
    printf("#           vie = époques de dérive avant la mort (4096 = censuré vivant), surv = part des graines vivantes sans interruption à 4096\n");
    printf("#  mut : sans dérive, 2048 ép., %d graines, survie continue au taux natif / 1e-3 / 4e-3 / 1.6e-2\n", MSD);
    printf("%-22s %-5s %-34s mut  L kp ess | rob@8 rob@32 rob@128 rob@352 | mrob  | vie   surv | mut: nat 1e-3 4e-3 1.6e-2\n", "label", "née", "génome");
    for (int i = 0; i < g_nr; i++) { Rep *R = &g_rep[i]; char hx[140]; for (int k = 0; k < R->u.L && k < 64; k++) sprintf(hx + 2 * k, "%02x", R->g[k]); if (R->u.L > 16) strcpy(hx + 30, "…");
        printf("%-22s %-5s %-34s %.0e %2d %d %3d | %5.3f %5.3f  %5.3f   %5.3f  | %5.3f | %5.0f %4.2f | %4.1f %4.1f %4.1f %4.1f\n", R->lab, R->born_end ? "fin" : "dép", hx, MUTS[R->u.mut_i], R->u.L, R->u.kper, R->ess,
               R->rob[0], R->rob[1], R->rob[2], R->rob[3], R->mrob, R->life, R->surv, R->msurv[0], R->msurv[1], R->msurv[2], R->msurv[3]); }
    printf("\n# moyennes par catégorie (unité = réplicateur)\n");
    for (int c = 0; c < 5; c++) { int n = 0; double a[12] = { 0 }; for (int i = 0; i < g_nr; i++) if (g_rep[i].cat == c) { Rep *R = &g_rep[i]; n++; for (int k = 0; k < 4; k++) a[k] += R->rob[k]; a[4] += R->mrob; a[5] += R->life; a[6] += R->surv; for (int m = 0; m < 4; m++) a[7 + m] += R->msurv[m]; }
        if (!n) { continue; }
        printf("%-42s n=%2d | rob@32 %.3f rob@352 %.3f | mrob %.3f | vie %5.0f surv %.2f | mut nat %.2f 4e-3 %.2f 1.6e-2 %.2f\n", CN[c], n, a[1] / n, a[3] / n, a[4] / n, a[5] / n, a[6] / n, a[7] / n, a[9] / n, a[10] / n); }
    /* tests */
    const char *MN[5] = { "rob@32", "rob@352", "mrob", "vie dérive", "survie mut 4e-3" };
    printf("\n# tests de permutation bilatéraux (20000), unité = réplicateur\n");
    for (int pass = 0; pass < 3; pass++) {
        double x[RMAX][5], y[RMAX][5]; int nx = 0, ny = 0; const char *t = "";
        for (int i = 0; i < g_nr; i++) { Rep *R = &g_rep[i]; int inx, iny;
            if (pass == 0) { t = "renaissants S#0 (survivant + indépendants, physique S#0 r5 finale) vs premiers-nés S#0 (injecté + bruit sous S#0 base)"; inx = R->src == 0 && R->born_end; iny = R->src == 0 && !R->born_end; }
            else if (pass == 1) { t = "nés en physique de FIN de dérive vs nés en physique de DÉPART, toutes sources"; inx = R->born_end; iny = !R->born_end; }
            else { t = "idem, sources T#0/T#1 seulement (les seules à avoir des fins fertiles hors S#0)"; inx = R->born_end && R->src >= 2; iny = !R->born_end && R->src >= 2; }
            double v[5] = { R->rob[1], R->rob[3], R->mrob, R->life, R->msurv[2] };
            if (inx) { memcpy(x[nx++], v, sizeof v); } if (iny) { memcpy(y[ny++], v, sizeof v); } }
        printf("%s : n=%d vs %d\n", t, nx, ny);
        for (int m = 0; m < 5; m++) { double a[RMAX], c[RMAX]; for (int i = 0; i < nx; i++) a[i] = x[i][m]; for (int i = 0; i < ny; i++) c[i] = y[i][m];
            printf("   %-16s %7.3f vs %7.3f   p=%.4f\n", MN[m], mean(a, nx), mean(c, ny), nx && ny ? p2(a, nx, c, ny) : 1.0); }
    }
    { Rep *A = &g_rep[0], *B = &g_rep[1]; printf("\n# face-à-face : premier injecté vs renaissant survivant : vie %.0f vs %.0f (surv %.2f vs %.2f), rob@352 %.3f vs %.3f, mrob %.3f vs %.3f\n",
        A->life, B->life, A->surv, B->surv, A->rob[3], B->rob[3], A->mrob, B->mrob); }
    return 0;
}

/* ---------------- Q2 : cycles mort/renaissance ---------------- */
#define NF 8
#define ELONG 32768
static int cmd_cycles(void) {
    DriftJob D; setup_job(&D, 0, 5, DK, 0);
    Life *X = calloc(2 * NF, sizeof *X);
    for (int w = 0; w < 2; w++) for (int f = 0; f < NF; f++) { Life *x = &X[w * NF + f]; x->base = D.base; memcpy(x->rep, D.rep, LMAX); x->inject = 1; x->N = 1024; x->Eest = 512; x->Edrift = ELONG; x->K = DK; x->drift = 1; x->seed = D.seed;
        x->fork_at = f ? 512 + 4096 + 1 : 0; x->fseed = mix(0xF0CC, (uint64_t)f); x->pmut = -1; x->wipe = w; x->thr = 51; x->snap_at = 512 + 4096; }
    pfor(2 * NF, life_w, X);
    Univ E5; phys_at(0, 5, 4096, &E5);
    printf("# Q2 univers S#0 rép5 (graine mix(7,5)) sous dérive K=8 continue : 512 ép. d'établissement + %d ép. de dérive (%d changements de loi)\n", ELONG, ELONG / DK);
    printf("#  run 0 = trajectoire d'origine prolongée (identique à la dérive k8 jusqu'à de=4096 : physique à de=4096 == fin reconstruite : %s) ;\n", !memcmp(&X[0].snap, &E5, sizeof E5) ? "OUI" : "NON");
    printf("#  runs 1..%d = même histoire jusqu'à de=4096, puis graine de dérive bifurquée. Mort = 0 copieur vérifié dans la pop entière ; naissance = ≥51 copieurs (5%%).\n", NF - 1);
    printf("#  « effacement » : même suite de physiques (le générateur de dérive est indépendant de la soupe), mais la soupe est remplacée par du bruit à chaque mort.\n");
    double ly[2][1024]; int lst[2][1024], nl[2] = { 0, 0 }; double hp[2][1024]; int nh[2] = { 0, 0 }; double wt[2][1024]; int nwt[2] = { 0, 0 };
    double alivef[2][NF], nb[2][NF];
    for (int w = 0; w < 2; w++) {
        printf("\n## %s\n", w ? "AVEC effacement de la soupe à chaque mort" : "SANS effacement (la soupe garde ses restes)");
        for (int f = 0; f < NF; f++) { Life *x = &X[w * NF + f]; int Et = x->Eest + x->Edrift; double al = 0;
            printf("run %d : %d générations, %d morts. ", f, x->ngen, x->ndeath);
            for (int k = 0; k < x->ngen; k++) { Gen *G = &x->g[k]; int td = G->td < 0 ? Et : G->td; al += td - G->tb;
                if (k < 12) printf("[%d→%s%d%s]", G->tb, G->td < 0 ? "" : "", td, G->td < 0 ? "+" : "");
                if (G->td >= 0 && nl[w] < 1024) { ly[w][nl[w]] = td - G->tb; lst[w][nl[w]++] = f; }
                if (k > 0) { if (nh[w] < 1024) hp[w][nh[w]++] = G->hprev; if (nwt[w] < 1024) wt[w][nwt[w]++] = G->wait; } }
            if (x->ngen > 12) printf("…");
            alivef[w][f] = al / Et; nb[w][f] = x->ngen - 1;
            printf("  vivant %.0f%% du temps\n", 100 * al / Et);
            for (int k = 1; k < x->ngen && k < 6; k++) { Gen *G = &x->g[k]; printf("    renaissance %d @%d : attente %d, H(→dernier dominant précédent)=%d, reliques (H≤2) à la mort précédente=%d, dominant ", k, G->tb, G->wait, G->hprev, x->g[k - 1].nrelic); hex(stdout, G->db, 16); printf("\n"); }
        }
        long S; double tau, pu, pd; kendall_strat(ly[w], lst[w], nl[w], &S, &tau, &pu, &pd);
        double rho_sum = 0; int nr = 0;
        for (int f = 0; f < NF; f++) { double gx[512], gy[512]; int n = 0; for (int i = 0; i < nl[w]; i++) if (lst[w][i] == f) { gx[n] = n; gy[n] = ly[w][i]; n++; } if (n >= 3) { rho_sum += spearman(gx, gy, n); nr++; } }
        printf("durées de vie des générations achevées : n=%d, moyenne %.0f ép. ; tendance vs rang de génération (Kendall stratifié par run) τ=%.3f, P(hausse)=%.4f, P(baisse)=%.4f ; Spearman moyen intra-run %.3f (%d runs ≥3 gén.)\n",
               nl[w], mean(ly[w], nl[w]), tau, pu, pd, nr ? rho_sum / nr : 0, nr);
        double first[NF], rest[1024]; int nfi = 0, nre = 0;
        for (int i = 0; i < nl[w]; i++) { int isfirst = i == 0 || lst[w][i] != lst[w][i - 1]; if (isfirst) first[nfi++] = ly[w][i]; else rest[nre++] = ly[w][i]; }
        printf("vie de la 1re génération (injectée) %.0f vs renaissances %.0f (p=%.4f)\n", mean(first, nfi), mean(rest, nre), nfi && nre ? p2(first, nfi, rest, nre) : 1.0);
        int close = 0; for (int i = 0; i < nh[w]; i++) close += hp[w][i] <= 4;
        printf("héritage : H(renaissant → dernier dominant de la vie précédente) moyenne %.2f / 16 ; ≤4 dans %d/%d ; attente moyenne mort→renaissance %.0f ép.\n", mean(hp[w], nh[w]), close, nh[w], mean(wt[w], nwt[w]));
    }
    printf("\n## comparaison appariée (même suite de physiques) : renaissances par run sans/avec effacement : ");
    for (int f = 0; f < NF; f++) printf("%.0f/%.0f ", nb[0][f], nb[1][f]);
    printf("\n   part du temps vivant : sans %.3f, avec %.3f (p=%.4f) ; H moyen renaissant→précédent : sans %.2f (n=%d), avec %.2f (n=%d), p=%.4f ; attente : sans %.0f, avec %.0f, p=%.4f\n",
           mean(alivef[0], NF), mean(alivef[1], NF), p2(alivef[0], NF, alivef[1], NF), mean(hp[0], nh[0]), nh[0], mean(hp[1], nh[1]), nh[1], nh[0] && nh[1] ? p2(hp[0], nh[0], hp[1], nh[1]) : 1.0,
           mean(wt[0], nwt[0]), mean(wt[1], nwt[1]), nwt[0] && nwt[1] ? p2(wt[0], nwt[0], wt[1], nwt[1]) : 1.0);
    /* fertilité de la physique le long de la trajectoire d'origine prolongée (bruit pur, 4 graines par point) */
    int npt = ELONG / 2048; Job *J = calloc((size_t)npt * 4, sizeof *J); Univ *U = calloc((size_t)npt, sizeof *U);
    for (int k = 0; k < npt; k++) { phys_replay(&D.base, D.seed, DK, (k + 1) * 2048, &U[k]); for (int s = 0; s < 4; s++) { Job *j = &J[k * 4 + s]; j->u = &U[k]; j->c.N = 1024; j->c.E = 4096; j->c.seed = mix(0xCC1, (uint64_t)(k * 10 + s)); } }
    run_jobs(J, npt * 4);
    printf("\n## fertilité (émergence depuis le bruit, 4 graines) de la physique du run 0 tous les 2048 ép. de dérive :\n   ");
    for (int k = 0; k < npt; k++) { int em = 0; for (int s = 0; s < 4; s++) em += J[k * 4 + s].r.emerged; printf("de%d:%d/4 ", (k + 1) * 2048, em); }
    printf("\n"); free(J); free(U); free(X);
    return 0;
}

/* ---------------- Q3 : qu'est-ce qui rend une physique fertile ? ---------------- */
#define NFEAT (P_COUNT + 13)
static const char *FN_X[13] = { "contrôle (JMP/JZ/JNZ/LOOP)", "têtes H0/H1 (±)", "READ_H+WRITE_H", "LOAD+STORE", "kit copie min(READ_H,WRITE_H,LOOP|JNZ,H±)", "octet 00 = NOP (part des q)",
    "adressage (0 abs,1 b,2 ip)", "kper", "steps", "rcost", "wcost", "taux de mutation", "L" };
static void features(const Univ *u, double *f) {
    double c[P_COUNT] = { 0 }; int ne = 256 * u->kper;
    for (int q = 0; q < u->kper; q++) for (int b = 0; b < 256; b++) c[u->tab[q][b]]++;
    for (int p = 0; p < P_COUNT; p++) f[p] = c[p] / ne;
    double *x = f + P_COUNT;
    x[0] = (c[P_JMP] + c[P_JZ] + c[P_JNZ] + c[P_LOOP]) / ne; x[1] = (c[P_H0_INC] + c[P_H0_DEC] + c[P_H1_INC] + c[P_H1_DEC]) / ne; x[2] = (c[P_READ_H] + c[P_WRITE_H]) / ne; x[3] = (c[P_LOAD] + c[P_STORE]) / ne;
    double m = c[P_READ_H]; if (c[P_WRITE_H] < m) m = c[P_WRITE_H]; if (c[P_LOOP] + c[P_JNZ] < m) m = c[P_LOOP] + c[P_JNZ]; if (c[P_H0_INC] + c[P_H0_DEC] + c[P_H1_INC] + c[P_H1_DEC] < m) m = c[P_H0_INC] + c[P_H0_DEC] + c[P_H1_INC] + c[P_H1_DEC]; x[4] = m / ne;
    int z = 0; for (int q = 0; q < u->kper; q++) z += u->tab[q][0] == P_NOP; x[5] = (double)z / u->kper;
    x[6] = u->addr; x[7] = u->kper; x[8] = u->steps; x[9] = u->rcost; x[10] = u->wcost; x[11] = MUTS[u->mut_i]; x[12] = u->L;
}
static const char *fname(int k) { return k < P_COUNT ? PN[k] : FN_X[k - P_COUNT]; }
static int best_feat(double F[][NFEAT], const int *lab, const int *use, int n, double *bauc) {
    double x[POOLMAX]; int l[POOLMAX], best = 0; double bd = -1;
    for (int k = 0; k < NFEAT; k++) { int m = 0; for (int i = 0; i < n; i++) if (use[i]) { x[m] = F[i][k]; l[m++] = lab[i]; } double a = auc(x, l, m); if (fabs(a - 0.5) > bd) { bd = fabs(a - 0.5); best = k; *bauc = a; } }
    return best;
}
static int cmd_features(void) {
    static double F[POOLMAX][NFEAT]; int lab[POOLMAX], all[POOLMAX], n = g_np, nf = 0;
    for (int i = 0; i < n; i++) { features(&g_pool[i].u, F[i]); lab[i] = g_pool[i].nem > 0; all[i] = 1; nf += lab[i]; }
    printf("# Q3 %d physiques (4 départs, 32 fins de dérive, 96 milieux de dérive) ; fertile = ≥1 émergence depuis le bruit (8–48 graines aux bases/fins, 3 aux milieux) ; %d fertiles\n", n, nf);
    printf("# AUC de chaque caractéristique (1 = plus grande chez les fertiles, 0 = plus petite) ; triées par |AUC−0.5|\n");
    double A[NFEAT]; int ord[NFEAT];
    for (int k = 0; k < NFEAT; k++) { double x[POOLMAX]; for (int i = 0; i < n; i++) x[i] = F[i][k]; A[k] = auc(x, lab, n); ord[k] = k; }
    for (int a = 0; a < NFEAT; a++) for (int b = a + 1; b < NFEAT; b++) if (fabs(A[ord[b]] - 0.5) > fabs(A[ord[a]] - 0.5)) { int t = ord[a]; ord[a] = ord[b]; ord[b] = t; }
    for (int r = 0; r < 12; r++) { int k = ord[r]; double mf = 0, ms = 0; for (int i = 0; i < n; i++) { if (lab[i]) mf += F[i][k]; else ms += F[i][k]; }
        printf("  %-44s AUC %.3f   moyenne fertiles %.4f / stériles %.4f\n", fname(k), A[k], mf / nf, ms / (n - nf)); }
    /* p familial : max |AUC−0.5| sous permutation des étiquettes */
    double obs = fabs(A[ord[0]] - 0.5); Rng r; rng_seed(&r, 0xFEA7); int ge = 0, R = 2000, pl[POOLMAX];
    for (int it = 0; it < R; it++) { memcpy(pl, lab, sizeof(int) * (size_t)n); for (int i = n - 1; i > 0; i--) { int j = (int)rng_below(&r, (uint32_t)(i + 1)), t = pl[i]; pl[i] = pl[j]; pl[j] = t; } double ba; best_feat(F, pl, all, n, &ba); ge += fabs(ba - 0.5) >= obs - 1e-12; }
    printf("p familial (max sur %d caractéristiques, %d permutations) = %.4f\n", NFEAT, R, (ge + 1.0) / (R + 1.0));
    /* validation croisée par source : caractéristique + seuil choisis sur 3 sources, testés sur la 4e */
    int tp = 0, tn = 0, P = 0, Nn = 0;
    for (int s = 0; s < 4; s++) {
        int tr[POOLMAX]; for (int i = 0; i < n; i++) tr[i] = g_pool[i].src != s;
        double ba; int k = best_feat(F, lab, tr, n, &ba); int dir = ba >= 0.5;
        double bt = 0, bacc = -1;
        for (int c = 0; c < n; c++) if (tr[c]) { double th = F[c][k]; int a = 0, b = 0, pa = 0, nb2 = 0;
            for (int i = 0; i < n; i++) if (tr[i]) { int pr = dir ? F[i][k] >= th : F[i][k] <= th; if (lab[i]) { pa++; a += pr; } else { nb2++; b += !pr; } }
            double acc = 0.5 * ((pa ? (double)a / pa : 0) + (nb2 ? (double)b / nb2 : 0)); if (acc > bacc) { bacc = acc; bt = th; } }
        int a = 0, b = 0, pa = 0, nb2 = 0;
        for (int i = 0; i < n; i++) if (!tr[i]) { int pr = dir ? F[i][k] >= bt : F[i][k] <= bt; if (lab[i]) { pa++; a += pr; } else { nb2++; b += !pr; } }
        printf("  test %s : règle « %s %s %.4f » (apprise sur les 3 autres) → fertiles reconnues %d/%d, stériles %d/%d\n", g_lab[s], fname(k), dir ? "≥" : "≤", bt, a, pa, b, nb2);
        tp += a; P += pa; tn += b; Nn += nb2;
    }
    printf("validation croisée par source : sensibilité %d/%d, spécificité %d/%d, exactitude équilibrée %.3f\n", tp, P, tn, Nn, 0.5 * ((P ? (double)tp / P : 0) + (Nn ? (double)tn / Nn : 0)));
    /* par source : la fertilité est-elle surtout une affaire de source ? */
    for (int s = 0; s < 4; s++) { int f = 0, t = 0; for (int i = 0; i < n; i++) if (g_pool[i].src == s) { f += lab[i]; t++; } printf("  %s : %d/%d physiques fertiles (L=%d kper=%d addr=%s)\n", g_lab[s], f, t, g_src[s]->u.L, g_src[s]->u.kper, ADN[g_src[s]->u.addr]); }
    /* prédicteurs non compositionnels : distance de dérive, et lois « d'ancêtres » encore présentes */
    printf("\n# fertilité selon l'âge de dérive : ");
    { const int DEs[5] = { 0, 1024, 2048, 3072, 4096 }; for (int d = 0; d < 5; d++) { int f = 0, t = 0; for (int i = 0; i < n; i++) if (g_pool[i].de == DEs[d]) { f += lab[i]; t++; } printf("de%d %d/%d  ", DEs[d], f, t); } printf("\n"); }
    { double xr[POOLMAX], xk[POOLMAX];
      for (int i = 0; i < n; i++) { const Univ *u = &g_pool[i].u; xr[i] = u_copier(g_src[g_pool[i].src]->best.dom, u) != 0; int k = 0, t = 0;
          for (int j = 0; j < n; j++) if (j != i && g_pool[j].u.L == u->L && memcmp(&g_pool[j].u, u, sizeof *u)) for (int s = 0; s < g_pool[j].ns; s++) if (g_pool[j].em[s]) { t++; k += u_copier(g_pool[j].dom[s], u) != 0; }
          xk[i] = t ? (double)k / t : 0; }
      double kf = 0, ks = 0; for (int i = 0; i < n; i++) { if (lab[i]) kf += xk[i]; else ks += xk[i]; }
      int a = 0, b = 0, c = 0, d = 0; for (int i = 0; i < n; i++) { if (xr[i]) { if (lab[i]) a++; else b++; } else { if (lab[i]) c++; else d++; } }
      printf("« le réplicateur injecté de la source copie encore sous cette physique » : fertiles %d/%d, stériles %d/%d (Fisher bilatéral p=%.4f)\n", a, a + c, b, b + d, fisher_two(a, b, c, d));
      printf("« part des réplicateurs nés ailleurs (même L, autres physiques) qui copient sous cette physique » : AUC %.3f, moyenne fertiles %.3f / stériles %.3f\n", auc(xk, lab, n), kf / (a + c), ks / (b + d));
    }
    return 0;
}

static int selftest4(void) {
    int fail = 0; Rng r; rng_seed(&r, 9);
    DriftJob D; memset(&D, 0, sizeof D); u_random(&D.base, &r); D.base.L = 16;
    for (int i = 0; i < LMAX; i++) D.rep[i] = (uint8_t)rng_next(&r);
    D.N = 128; D.Eest = 64; D.Edrift = 256; D.K = 8; D.mode = 0; D.seed = 77;
    Life X; memset(&X, 0, sizeof X); X.base = D.base; memcpy(X.rep, D.rep, LMAX); X.inject = 1; X.N = D.N; X.Eest = D.Eest; X.Edrift = D.Edrift; X.K = D.K; X.drift = 1; X.seed = D.seed; X.pmut = -1; X.thr = 1;
    drift_run(&D); life_run(&X);
    int ok = !memcmp(&X.end, &D.end, sizeof(Univ)) && X.cf_end == D.cf_end && X.ncurve == D.ncurve && !memcmp(X.curve, D.curve, sizeof D.curve);
    printf("rebirth selftest : life_run rejoue drift_run à l'identique %s\n", ok ? "OK" : "ÉCHEC"); fail |= !ok;
    Univ u; phys_replay(&D.base, D.seed, D.K, D.Edrift, &u); ok = !memcmp(&u, &D.end, sizeof u);
    printf("rebirth selftest : phys_replay reconstruit la physique finale sans soupe %s\n", ok ? "OK" : "ÉCHEC"); fail |= !ok;
    Life Y = X; Y.fork_at = 100000; Y.fseed = 3; life_run(&Y); ok = !memcmp(&Y.end, &X.end, sizeof(Univ)) && Y.cf_end == X.cf_end;
    Life Z = X; Z.fork_at = 65; Z.fseed = 3; life_run(&Z); ok &= memcmp(&Z.end, &X.end, sizeof(Univ)) != 0;
    printf("rebirth selftest : bifurcation de dérive (hors fenêtre = neutre, dans la fenêtre = autre physique) %s\n", ok ? "OK" : "ÉCHEC"); fail |= !ok;
    double y[6] = { 1, 2, 3, 4, 5, 6 }, x[6] = { 0, 1, 2, 3, 4, 5 }; int st[6] = { 0, 0, 0, 1, 1, 1 }, lb[6] = { 0, 0, 0, 1, 1, 1 };
    long S; double tau, pu, pd; kendall_strat(y, st, 6, &S, &tau, &pu, &pd);
    ok = S == 6 && fabs(tau - 1) < 1e-12 && fabs(spearman(x, y, 6) - 1) < 1e-12 && auc(y, lb, 6) == 1.0;
    printf("rebirth selftest : statistiques de rang (Kendall stratifié, Spearman, AUC) %s\n", ok ? "OK" : "ÉCHEC"); fail |= !ok;
    return fail;
}

int main(int argc, char **argv) {
    const char *load = NULL, *save = NULL, *pool = NULL;
    (void)selftest2; (void)cmd_drift; (void)cmd_origin; (void)cmd_race; (void)selftest3; (void)cmd_replay; (void)cmd_trace; (void)cmd_robust; (void)cmd_fresh; (void)cmd_fertile; (void)cmd_knock; (void)run_traces;
    for (int i = 2; i + 1 < argc; i += 2) {
        if (!strcmp(argv[i], "--load")) load = argv[i + 1];
        else if (!strcmp(argv[i], "--save")) save = argv[i + 1];
        else if (!strcmp(argv[i], "--pool")) pool = argv[i + 1];
        else if (!strcmp(argv[i], "--threads")) g_threads = atoi(argv[i + 1]);
        else { fprintf(stderr, "option inconnue %s\n", argv[i]); return 2; }
    }
    if (argc >= 2 && !strcmp(argv[1], "selftest")) return selftest4();
    const char *use = "usage : rebirth selftest | pool --load fin2.bin --save pool.bin | persist --load fin2.bin --pool pool.bin | cycles --load fin2.bin | features --load fin2.bin --pool pool.bin\n";
    if (argc < 2 || load_sources(load)) { fprintf(stderr, "%s(fin2.bin : universe_search2 race --seed 2 --save fin2.bin)\n", use); return 2; }
    if (!strcmp(argv[1], "pool")) return cmd_pool(save);
    if (!strcmp(argv[1], "cycles")) return cmd_cycles();
    if (load_pool(pool)) { fprintf(stderr, "%s", use); return 2; }
    if (!strcmp(argv[1], "persist")) return cmd_persist();
    if (!strcmp(argv[1], "features")) return cmd_features();
    return 2;
}
