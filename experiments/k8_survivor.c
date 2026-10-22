/* Autopsie du survivant de la dérive K=8 (experiments/universe-search/k8-survivor).
 *
 * Inclut universe_search2.c (donc v1) sans en changer la logique : même soupe, même dérive, même copieur vérifié,
 * même garde-fou anti-triche. trace_run() est une copie instrumentée de drift_run() : elle consomme EXACTEMENT les
 * mêmes tirages aléatoires (les mesures ajoutées utilisent des générateurs à part), donc elle rejoue la trajectoire
 * à l'identique ; `replay` le vérifie en comparant avec drift_run().
 *
 *   ./experiments/k8_survivor replay   --load fin2.bin               # 4 sources × 8 répliques, K=8, dérive + saut
 *   ./experiments/k8_survivor trace    --load fin2.bin --src 0 --rep 5 [--mode 0|1]
 *   ./experiments/k8_survivor robust   --load fin2.bin               # (b) tables perturbées au hasard
 *   ./experiments/k8_survivor fresh    --load fin2.bin               # (e) émergence depuis le bruit sous la physique finale
 *   ./experiments/k8_survivor knockout --load fin2.bin --src 0 --rep 5 --at E --h H   # (e) contrôle causal de lignée
 *   ./experiments/k8_survivor selftest
 */
#define US2_NO_MAIN
#include "universe_search2.c"

static Cand g_fin[2][64]; static int g_nf[2];
static Cand *g_src[4];
static const char *g_lab[4] = { "S#0", "S#1", "T#0", "T#1" };
#define DK 8

/* sources exactement comme cmd_drift : 2 premiers finalistes (avec réplicateur vérifié) de chaque bras ;
   le témoin a été lancé avec --from 1, donc ses indices de graine repartent de 0 */
static int load_sources(const char *path) {
    if (!path || load_fin(path, g_fin, g_nf)) return -1;
    for (int arm = 0; arm < 2; arm++) {
        int ns = 0;
        for (int i = 0; i < g_nf[arm] && ns < 2; i++)
            if (g_fin[arm][i].has_best && u_copier(g_fin[arm][i].best.dom, &g_fin[arm][i].u)) g_src[arm * 2 + ns++] = &g_fin[arm][i];
        if (ns < 2) return -1;
    }
    return 0;
}
static void setup_job(DriftJob *D, int src, int rep, int K, int mode) {
    memset(D, 0, sizeof *D);
    D->base = g_src[src]->u; memcpy(D->rep, g_src[src]->best.dom, LMAX);
    D->N = 1024; D->Eest = 512; D->Edrift = 4096; D->K = K; D->mode = mode;
    D->seed = mix(7, (uint64_t)((src % 2) * 100 + rep));
}
static int ham(const uint8_t *x, const uint8_t *y, int L) { return L - same(x, y, L); }
static void hex(FILE *o, const uint8_t *g, int L) { for (int i = 0; i < L; i++) fprintf(o, "%02x", g[i]); }
static void pr_ess(FILE *o, const uint8_t *g, const uint8_t *ess, const Univ *u) {
    for (int i = 0; i < u->L; i++) if (ess[i]) fprintf(o, " %d:%02x=%s", i, g[i], PN[u->tab[i & (u->kper - 1)][g[i]]]);
}
/* entrées de table (q, octet) utilisées aux positions essentielles d'un génome */
static int ess_entries(const uint8_t *g, const uint8_t *ess, const Univ *u, int *q, int *b) {
    int n = 0;
    for (int i = 0; i < u->L; i++) if (ess[i]) {
        int qq = i & (u->kper - 1), d = 0;
        for (int k = 0; k < n; k++) d |= q[k] == qq && b[k] == g[i];
        if (!d) { q[n] = qq; b[n] = g[i]; n++; }
    }
    return n;
}

/* ---------------- trace instrumentée ---------------- */
typedef struct { int de, q, b, oldp, newp, param, used, ess, ok_before, ok_after; char what[64]; } Chg;
#define MAXT 80
typedef struct {
    int e; double cf64, cfull; int ncop, ncg, domcnt, ness, rep_ok, dom_ok_base, ham_rep, ham_prev, near_rep, near_fin, mind_fin, tabd, ndist;
    uint8_t dom[LMAX], ess[LMAX]; Univ u;
} Ck;
static int cmp_prog(const void *a, const void *b, void *arg) { return memcmp(*(uint8_t *const *)a, *(uint8_t *const *)b, (size_t)*(int *)arg); }

static void pop_stats(const uint8_t *s, int N, const Univ *u, const Univ *base, const uint8_t *rep, const uint8_t *fin, Ck *c, const Ck *prev) {
    int L = u->L; uint8_t **cp = malloc(sizeof *cp * (size_t)N), **all = malloc(sizeof *all * (size_t)N); int nc = 0;
    for (int i = 0; i < N; i++) { all[i] = (uint8_t *)s + (size_t)i * L; if (u_copier(all[i], u)) cp[nc++] = all[i]; }
    c->ncop = nc; c->cfull = (double)nc / N; c->domcnt = 0; c->ncg = 0;
    qsort_r(cp, (size_t)nc, sizeof *cp, cmp_prog, &L);
    for (int i = 0; i < nc;) { int j = i; while (j < nc && !memcmp(cp[i], cp[j], L)) j++; c->ncg++; if (j - i > c->domcnt) { c->domcnt = j - i; memcpy(c->dom, cp[i], L); } i = j; }
    qsort_r(all, (size_t)N, sizeof *all, cmp_prog, &L);
    c->ndist = 0; for (int i = 0; i < N;) { int j = i; while (j < N && !memcmp(all[i], all[j], L)) j++; c->ndist++; i = j; }
    c->near_rep = 0; c->near_fin = 0; c->mind_fin = L;
    for (int i = 0; i < N; i++) { const uint8_t *p = s + (size_t)i * L; c->near_rep += ham(p, rep, L) <= 2; if (fin) { int h = ham(p, fin, L); c->near_fin += h <= 2; if (h < c->mind_fin) c->mind_fin = h; } }
    c->rep_ok = u_copier(rep, u) != 0; c->tabd = u_tabdist(base, u); c->u = *u;
    c->ness = -1; c->dom_ok_base = -1; c->ham_rep = -1; c->ham_prev = -1;
    if (nc) {
        c->ness = u_ablate(c->dom, u, c->ess); c->dom_ok_base = u_copier(c->dom, base) != 0; c->ham_rep = ham(c->dom, rep, L);
        if (prev && prev->domcnt) c->ham_prev = ham(c->dom, prev->dom, L);
    } else memset(c->ess, 0, LMAX);
    free(cp); free(all);
}

/* copie de drift_run (mêmes appels RNG dans le même ordre) + mesures ; knock_at>0 : à cette époque, les programmes à
   distance ≤ knock_h du dominant de lignée (ref) sont remplacés par du bruit tiré d'un générateur À PART */
typedef struct { DriftJob D; Ck ck[MAXT]; int nck; Chg *ch; int nch; const uint8_t *fin; int knock_at, knock_h, knocked; uint8_t knock_ref[LMAX]; int light; } Trace;
static void trace_run(Trace *T) {
    DriftJob *D = &T->D; Univ u = D->base; const int L = u.L, N = D->N & ~1;
    uint8_t *s = malloc((size_t)N * L); int *perm = malloc(sizeof(int) * (size_t)N);
    Rng rng, dr; rng_seed(&rng, D->seed ^ 0xD81F7ULL); rng_seed(&dr, D->seed ^ 0x5EEDULL ^ ((uint64_t)D->K << 32));
    for (size_t i = 0; i < (size_t)N * L; i++) s[i] = (uint8_t)rng_next(&rng);
    for (int k = 0; k < N / 8; k++) memcpy(s + (size_t)k * L, D->rep, L);
    for (int i = 0; i < N; i++) perm[i] = i;
    D->ess0n = u_ablate(D->rep, &u, D->ess0);
    int Etot = D->Eest + D->Edrift; D->t_death = -1; D->cf_min = 1; D->ncurve = 0; T->nck = 0; T->nch = 0;
    uint8_t curdom[LMAX], curess[LMAX]; int havedom = 1; memcpy(curdom, D->rep, LMAX); memcpy(curess, D->ess0, LMAX);
    for (int e = 1; e <= Etot; e++) {
        int de = e - D->Eest;
        if (de == 1 && D->mode == 1) u = D->end;
        if (de > 0 && D->mode == 0 && de % D->K == 0) {
            Univ before = u; char w[64]; int okb = havedom ? u_copier(curdom, &u) != 0 : 0;
            int b = drift_step(&u, &dr, w, sizeof w); D->nchanges++;
            if (b >= 0) for (int i = 0; i < L; i++) if (D->ess0[i] && D->rep[i] == b) { D->ness_hits++; break; }
            if (T->ch) {
                Chg *c = &T->ch[T->nch++]; memset(c, 0, sizeof *c); c->de = de; snprintf(c->what, sizeof c->what, "%s", w); c->param = b < 0; c->q = -1; c->b = b;
                if (b >= 0) for (int q = 0; q < u.kper; q++) if (before.tab[q][b] != u.tab[q][b]) { c->q = q; c->oldp = before.tab[q][b]; c->newp = u.tab[q][b]; }
                if (havedom && c->q >= 0) for (int i = 0; i < L; i++) if ((i & (u.kper - 1)) == c->q && curdom[i] == b) { c->used = 1; c->ess |= curess[i]; }
                c->ok_before = okb; c->ok_after = havedom ? u_copier(curdom, &u) != 0 : 0;
            }
        }
        for (int i = N - 1; i > 0; i--) { int j = (int)rng_below(&rng, (uint32_t)(i + 1)), t = perm[i]; perm[i] = perm[j]; perm[j] = t; }
        for (int i = 0; i < N; i += 2) {
            uint8_t tape[TMAX]; uint8_t *pa = s + (size_t)perm[i] * L, *pb = s + (size_t)perm[i + 1] * L;
            memcpy(tape, pa, L); memcpy(tape + L, pb, L);
            u_exec(tape, &u, NULL, 0, 0);
            memcpy(pa, tape, L); memcpy(pb, tape + L, L);
        }
        double pmut = MUTS[u.mut_i];
        if (pmut > 0) { double lq = log(1.0 - pmut); size_t tot = (size_t)N * L, i = 0; for (;;) { i += (size_t)(log(1.0 - rng_double(&rng)) / lq); if (i >= tot) break; s[i] = (uint8_t)rng_next(&rng); i++; } }
        if (T->knock_at && e == T->knock_at) {
            Rng kr; rng_seed(&kr, 0x4B4E4F43ULL ^ D->seed); int n = 0;
            for (int i = 0; i < N; i++) { uint8_t *p = s + (size_t)i * L; if (ham(p, T->knock_ref, L) <= T->knock_h) { for (int j = 0; j < L; j++) p[j] = (uint8_t)rng_next(&kr); n++; } }
            T->knocked = n;
        }
        if (e % CK == 0 || e == Etot) {
            double cf = sample_copiers(s, N, &u, D->seed * 31 + (uint64_t)e, NULL, NULL);
            if (D->ncurve < 80) D->curve[D->ncurve++] = cf;
            if (e == D->Eest) { D->cf_est = cf; D->established = cf >= 0.25; }
            if (de > 0) { if (cf < D->cf_min) D->cf_min = cf; if (cf < 0.05 && D->t_death < 0) D->t_death = de; }
            if (T->nck < MAXT) {
                Ck *c = &T->ck[T->nck]; c->e = e; c->cf64 = cf;
                if (!T->light) pop_stats(s, N, &u, &D->base, D->rep, T->fin, c, T->nck ? &T->ck[T->nck - 1] : NULL);
                else { int nc = 0; for (int i = 0; i < N; i++) nc += u_copier(s + (size_t)i * L, &u) != 0; c->ncop = nc; c->cfull = (double)nc / N; c->tabd = u_tabdist(&D->base, &u); c->domcnt = 0; }
                if (!T->light && c->ncop) { memcpy(curdom, c->dom, LMAX); memcpy(curess, c->ess, LMAX); havedom = 1; }
                T->nck++;
            }
        }
    }
    D->cf_end = sample_copiers(s, N, &u, D->seed * 31 + 9999, D->dom, &D->dom_ess);
    D->dom_has = D->dom_ess > 0; D->alive = D->cf_end >= 0.25; D->rep_still = u_copier(D->rep, &u) != 0;
    D->ess_changed = -1; D->dom_ham = -1;
    if (D->dom_has) { D->dom_ham = L - same(D->dom, D->rep, L); D->dom_ess = u_ablate(D->dom, &u, D->essE); }
    if (D->mode != 1) D->end = u;
    free(s); free(perm);
}
static void *trace_worker(void *arg) {
    void **a = arg; Trace *T = a[0]; int n = *(int *)a[1]; int *next = a[2]; pthread_mutex_t *mu = a[3];
    for (;;) { pthread_mutex_lock(mu); int i = (*next)++; pthread_mutex_unlock(mu); if (i >= n) return NULL; trace_run(&T[i]); }
}
static void run_traces(Trace *T, int n) {
    int next = 0; pthread_mutex_t mu = PTHREAD_MUTEX_INITIALIZER; void *a[4] = { T, &n, &next, &mu };
    pthread_t th[64]; int nt = g_threads < 64 ? g_threads : 64;
    for (int i = 0; i < nt; i++) pthread_create(&th[i], NULL, trace_worker, a);
    for (int i = 0; i < nt; i++) pthread_join(th[i], NULL);
}

/* ---------------- replay : les 32 trajectoires K=8 (dérive puis saut) ---------------- */
static DriftJob g_A[4][8], g_B[4][8];
static void replay_all(void) {
    DriftJob *A = malloc(sizeof *A * 32), *B = malloc(sizeof *B * 32);
    for (int s = 0; s < 4; s++) for (int r = 0; r < 8; r++) setup_job(&A[s * 8 + r], s, r, DK, 0);
    run_drift(A, 32);
    for (int i = 0; i < 32; i++) { B[i] = A[i]; B[i].mode = 1; B[i].nchanges = 0; B[i].ness_hits = 0; }
    run_drift(B, 32);
    for (int s = 0; s < 4; s++) for (int r = 0; r < 8; r++) { g_A[s][r] = A[s * 8 + r]; g_B[s][r] = B[s * 8 + r]; }
    free(A); free(B);
}
static int cmd_replay(void) {
    replay_all();
    printf("# replay K=%d : 4 sources × 8 répliques, N=1024, établissement 512, dérive 4096 (graine maîtresse 7, comme v2)\n", DK);
    printf("# src rép | DÉRIVE vivant cf_fin cf_min t_mort | SAUT vivant cf_fin cf_min t_mort | dist_table | essentiels_init: entrées distinctes / réassignées en fin / coups en route (q,octet exacts)\n");
    int ta = 0, tb = 0, sa[4] = { 0 }, sb[4] = { 0 }; double sd[4] = { 0 };
    for (int s = 0; s < 4; s++) for (int r = 0; r < 8; r++) {
        DriftJob *A = &g_A[s][r], *B = &g_B[s][r]; int q[LMAX], b[LMAX], ne = ess_entries(A->rep, A->ess0, &A->base, q, b), nr = 0;
        for (int k = 0; k < ne; k++) nr += A->base.tab[q[k]][b[k]] != A->end.tab[q[k]][b[k]];
        int d = u_tabdist(&A->base, &A->end); sd[s] += d;
        ta += A->alive; tb += B->alive; sa[s] += A->alive; sb[s] += B->alive;
        printf("%s %d | %s %.2f %.2f %5d | %s %.2f %.2f %5d | %3d | %2d / %2d / %2d%s\n", g_lab[s], r, A->alive ? "VIVANT" : "mort  ", A->cf_end, A->cf_min, A->t_death,
               B->alive ? "VIVANT" : "mort  ", B->cf_end, B->cf_min, B->t_death, d, ne, nr, A->ness_hits, (A->alive || B->alive) ? "   <==" : "");
    }
    for (int s = 0; s < 4; s++) printf("%s : dérive %d/8, saut %d/8, dist_table moyenne %.1f\n", g_lab[s], sa[s], sb[s], sd[s] / 8);
    printf("TOTAL K=%d : dérive %d/32, saut %d/32  (v2 publié : 1/32 et 3/32 ; S#0 1/8|1/8, S#1 0|0, T#0 0|2, T#1 0|0 ; dist 351.5 / 359.5 / 205.0 / 295.8)\n", DK, ta, tb);
    /* déterminisme : trace_run() redonne exactement drift_run() */
    for (int s = 0; s < 4; s++) for (int r = 0; r < 8; r++) if (g_A[s][r].alive || g_B[s][r].alive) {
        for (int mode = 0; mode < 2; mode++) {
            DriftJob *R = mode ? &g_B[s][r] : &g_A[s][r]; Trace *T = calloc(1, sizeof *T);
            setup_job(&T->D, s, r, DK, mode); if (mode) T->D.end = g_A[s][r].end; T->light = 1; trace_run(T);
            int ok = T->D.alive == R->alive && T->D.cf_end == R->cf_end && !memcmp(T->D.dom, R->dom, LMAX) && T->D.ncurve == R->ncurve && !memcmp(T->D.curve, R->curve, sizeof R->curve) && !memcmp(&T->D.end, &R->end, sizeof(Univ));
            printf("déterminisme %s rép %d %s : trace_run == drift_run %s (vivant %d, cf_fin %.2f, dominant ", g_lab[s], r, mode ? "saut  " : "dérive", ok ? "OUI" : "NON", T->D.alive, T->D.cf_end);
            hex(stdout, T->D.dom, T->D.base.L); printf(")\n");
            free(T);
        }
    }
    return 0;
}

/* ---------------- trace détaillée ---------------- */
static int cmd_trace(int src, int rep, int mode) {
    replay_all();
    DriftJob *R = mode ? &g_B[src][rep] : &g_A[src][rep];
    Trace *T = calloc(1, sizeof *T); T->ch = calloc(1024, sizeof(Chg));
    setup_job(&T->D, src, rep, DK, mode); if (mode) T->D.end = g_A[src][rep].end; T->fin = R->dom;
    trace_run(T);
    DriftJob *D = &T->D; const int L = D->base.L;
    char d[160]; u_desc(&D->base, d, sizeof d);
    printf("# trace %s rép %d %s K=%d : %s ; graine mix(7,%d)\n", g_lab[src], rep, mode ? "SAUT" : "DÉRIVE", DK, d, (src % 2) * 100 + rep);
    printf("# vivant=%d cf_fin=%.2f (drift_run : %d %.2f) ; réplicateur injecté ", D->alive, D->cf_end, R->alive, R->cf_end); hex(stdout, D->rep, L);
    printf(" (%d essentiels :", D->ess0n); pr_ess(stdout, D->rep, D->ess0, &D->base); printf(")\n");
    printf("# dominant final (échantillon) "); hex(stdout, D->dom, L); printf(" ; %d essentiels sous la physique finale :", D->dom_ess); pr_ess(stdout, D->dom, D->essE, &D->end); printf("\n");
    printf("# ép  dé  | cf64 copieurs/1024 génotypes_copieurs distincts_tous | dominant(pop) n ess | init_copie? dom_copie_sous_base? H(dom,init) H(dom,préc) | proches(≤2) init / dom_final, minH→final | dist_table steps rc wc mut | chgts: n utilisés essentiels létaux\n");
    int ci = 0;
    for (int k = 0; k < T->nck; k++) {
        Ck *c = &T->ck[k]; int de = c->e - D->Eest, n = 0, nu = 0, nes = 0, nl = 0;
        for (; ci < T->nch && T->ch[ci].de <= de; ci++) { Chg *h = &T->ch[ci]; n++; nu += h->used; nes += h->ess; nl += h->ok_before && !h->ok_after; }
        printf("%4d %5d | %.2f %4d %3d %4d | ", c->e, de, c->cf64, c->ncop, c->ncg, c->ndist);
        if (c->ncop) { hex(stdout, c->dom, L); printf(" %3d %2d", c->domcnt, c->ness); } else printf("%-*s", 2 * L + 7, "-");
        printf(" | %d %2d %2d %2d | %3d %3d %2d | %3d %4d %d %d %g | %d %d %d %d\n", c->rep_ok, c->dom_ok_base, c->ham_rep, c->ham_prev, c->near_rep, c->near_fin, c->mind_fin,
               c->tabd, c->u.steps, c->u.rcost, c->u.wcost, MUTS[c->u.mut_i], n, nu, nes, nl);
    }
    printf("\n## essentiels du dominant à intervalles réguliers (ablation sous la physique du moment)\n");
    for (int k = 0; k < T->nck; k++) { Ck *c = &T->ck[k]; if ((c->e % 512) && k != T->nck - 1) continue; if (!c->ncop) { printf("ép %4d : aucun copieur\n", c->e); continue; }
        printf("ép %4d : ", c->e); hex(stdout, c->dom, L); printf(" (%d/1024) %d ess :", c->domcnt, c->ness); pr_ess(stdout, c->dom, c->ess, &c->u); printf("\n"); }
    if (T->nch) {
        int nu = 0, nes = 0, nl = 0, np = 0, nlr = 0; for (int i = 0; i < T->nch; i++) { nu += T->ch[i].used; nes += T->ch[i].ess; nl += T->ch[i].ok_before && !T->ch[i].ok_after; np += T->ch[i].param; }
        printf("\n## %d changements (%d de paramètre) ; %d touchent une entrée utilisée par le dominant du moment, %d une position essentielle ; %d cassent le dominant du moment (copieur → non-copieur)\n", T->nch, np, nu, nes, nl);
        (void)nlr;
        printf("# détail des changements qui touchent le dominant du moment (ou un paramètre) :\n");
        for (int i = 0; i < T->nch; i++) { Chg *h = &T->ch[i]; if (!h->used && !h->param && !(h->ok_before && !h->ok_after)) continue;
            printf("  dé %4d  %-28s utilisé=%d essentiel=%d dominant copieur %d→%d\n", h->de, h->what, h->used, h->ess, h->ok_before, h->ok_after); }
    }
    /* (c) l'ancien et le nouveau réplicateur sous les deux physiques */
    printf("\n## (c) croisement réplicateur × physique\n");
    printf("injecté sous base : %d ; injecté sous finale : %d ; dominant final sous finale : %d ; dominant final sous base : %d\n",
           u_copier(D->rep, &D->base) != 0, u_copier(D->rep, &D->end) != 0, u_copier(D->dom, &D->end) != 0, u_copier(D->dom, &D->base) != 0);
    /* (a) quelles entrées essentielles de l'injecté ont été réassignées en fin */
    int q[LMAX], b[LMAX], ne = ess_entries(D->rep, D->ess0, &D->base, q, b);
    printf("\n## (a) entrées (q,octet) essentielles de l'injecté : %d ; état en fin de trajectoire\n", ne);
    int nr = 0; for (int k = 0; k < ne; k++) { int ch = D->base.tab[q[k]][b[k]] != D->end.tab[q[k]][b[k]]; nr += ch; printf("  tab[%d][%02x] %s → %s%s\n", q[k], b[k], PN[D->base.tab[q[k]][b[k]]], PN[D->end.tab[q[k]][b[k]]], ch ? "  (RÉASSIGNÉE)" : ""); }
    printf("  → %d/%d réassignées\n", nr, ne);
    ne = ess_entries(D->dom, D->essE, &D->end, q, b);
    printf("## entrées essentielles du dominant final : %d ; d'où viennent-elles ?\n", ne);
    nr = 0; for (int k = 0; k < ne; k++) { int ch = D->base.tab[q[k]][b[k]] != D->end.tab[q[k]][b[k]]; nr += ch; printf("  tab[%d][%02x] base %s → finale %s%s\n", q[k], b[k], PN[D->base.tab[q[k]][b[k]]], PN[D->end.tab[q[k]][b[k]]], ch ? "  (nouvelle loi créée par la dérive)" : ""); }
    printf("  → %d/%d de ses lois essentielles n'existaient pas dans la physique de départ\n", nr, ne);
    free(T->ch); free(T);
    return 0;
}

/* ---------------- (b) robustesse intrinsèque ---------------- */
static void perturb(Univ *u, int n, Rng *r) {
    for (int i = 0; i < n; i++) { int q = (int)rng_below(r, u->kper), b = (int)rng_below(r, 256), p = (int)rng_below(r, P_COUNT); u->tab[q][b] = (uint8_t)p; }
    for (int k = u->kper; k < 4; k++) memcpy(u->tab[k], u->tab[k % u->kper], 256);
}
/* un mutant ponctuel copie-t-il sous u ? (capacité d'une population à se réparer en un pas) */
static int rescue1(const uint8_t *g, const Univ *u) {
    uint8_t m[LMAX]; memcpy(m, g, u->L);
    for (int i = 0; i < u->L; i++) { for (int v = 0; v < 256; v++) { if (v == g[i]) continue; m[i] = (uint8_t)v; if (u_copier(m, u)) return 1; } m[i] = g[i]; }
    return 0;
}
typedef struct { const uint8_t *g; Univ u; int n, trials, resc; uint64_t sd; int ok, okr; } RobJob;
static void *rob_worker(void *arg) {
    void **a = arg; RobJob *J = a[0]; int n = *(int *)a[1]; int *next = a[2]; pthread_mutex_t *mu = a[3];
    for (;;) {
        pthread_mutex_lock(mu); int i = (*next)++; pthread_mutex_unlock(mu); if (i >= n) return NULL;
        RobJob *j = &J[i]; Rng r; rng_seed(&r, j->sd); j->ok = 0; j->okr = 0;
        for (int t = 0; t < j->trials; t++) { Univ v = j->u; perturb(&v, j->n, &r); int c = u_copier(j->g, &v) != 0; j->ok += c; if (j->resc && t < j->resc) j->okr += c || rescue1(j->g, &v); }
    }
}
static int cmd_robust(void) {
    replay_all();
    static const int NS[] = { 1, 2, 4, 8, 16, 32, 64, 128, 256, 352 }; const int nn = 10, TR = 400, RS = 40;
    const uint8_t *G[6]; Univ U[6]; const char *lab[6]; int ng = 0;
    for (int s = 0; s < 4; s++) { G[ng] = g_src[s]->best.dom; U[ng] = g_src[s]->u; lab[ng++] = g_lab[s]; }
    G[ng] = g_A[0][5].dom; U[ng] = g_A[0][5].end; lab[ng++] = "survivant S#0 rép5 dérive (dominant final, physique finale)";
    printf("# (b) robustesse : n entrées de table re-tirées au hasard (même loi que drift_step), %d tables par n ; u_copier du génome inchangé\n", TR);
    printf("#     colonne « +1mut » : part des %d premières tables où le génome OU un de ses %s mutants ponctuels copie (réparation en un pas)\n", RS, "15×255");
    RobJob *J = calloc((size_t)(ng * nn), sizeof *J);
    for (int g = 0; g < ng; g++) for (int k = 0; k < nn; k++) { RobJob *j = &J[g * nn + k]; j->g = G[g]; j->u = U[g]; j->n = NS[k]; j->trials = TR; j->resc = RS; j->sd = mix(0x0B0B, (uint64_t)(g * 100 + k)); }
    int n = ng * nn, next = 0; pthread_mutex_t mu = PTHREAD_MUTEX_INITIALIZER; void *a[4] = { J, &n, &next, &mu };
    pthread_t th[64]; for (int i = 0; i < g_threads; i++) pthread_create(&th[i], NULL, rob_worker, a); for (int i = 0; i < g_threads; i++) pthread_join(th[i], NULL);
    printf("# génome                    ess  entrées_ess |");
    for (int k = 0; k < nn; k++) printf(" n=%-4d", NS[k]);
    printf("\n");
    for (int g = 0; g < ng; g++) {
        uint8_t ess[LMAX]; int ne = u_ablate(G[g], &U[g], ess), q[LMAX], b[LMAX], nent = ess_entries(G[g], ess, &U[g], q, b);
        printf("%-26.26s %3d  %3d/%-4d |", lab[g], ne, nent, 256 * U[g].kper);
        for (int k = 0; k < nn; k++) printf(" %5.3f", (double)J[g * nn + k].ok / TR);
        printf("\n%-26s %13s +1mut|", "", ""); for (int k = 0; k < nn; k++) printf(" %5.2f", (double)J[g * nn + k].okr / RS);
        printf("\n%-26s %13s attendu|", "", ""); for (int k = 0; k < nn; k++) { double pe = pow(1.0 - (P_COUNT - 1.0) / P_COUNT / (256.0 * U[g].kper), NS[k]); double pp = 1; for (int i = 0; i < nent; i++) pp *= pe; printf(" %5.3f", pp); }
        printf("   (attendu = aucune entrée essentielle réellement changée)\n");
    }
    free(J); return 0;
}

/* ---------------- (e) émergence fraîche sous la physique finale ---------------- */
static int cmd_fresh(int ns) {
    replay_all();
    Univ U[6]; const char *lab[6]; int nu = 0;
    U[nu] = g_A[0][5].base; lab[nu++] = "S#0 base";
    U[nu] = g_A[0][5].end; lab[nu++] = "S#0 rép5 finale (dérive, survivant)";
    for (int s = 0; s < 4; s++) for (int r = 0; r < 8; r++) if (g_B[s][r].alive && nu < 6) { static char b[4][64]; snprintf(b[nu - 2], 64, "%s rép%d finale (saut, survivant)", g_lab[s], r); U[nu] = g_A[s][r].end; lab[nu] = b[nu - 2]; nu++; }
    printf("# (e) soupe de bruit pur (aucune injection), N=1024, E=4096, %d graines neuves par univers ; émergence = copieurs ≥0,25 (cristaux exclus)\n", ns);
    Job *J = calloc((size_t)(nu * ns), sizeof *J);
    for (int i = 0; i < nu; i++) for (int s = 0; s < ns; s++) { Job *j = &J[i * ns + s]; j->u = &U[i]; j->c.N = 1024; j->c.E = 4096; j->c.E1 = 0; j->c.seed = mix(0xF2E54, (uint64_t)s); }
    run_jobs(J, nu * ns);
    for (int i = 0; i < nu; i++) {
        int em = 0, cr = 0; printf("%-40s :", lab[i]);
        for (int s = 0; s < ns; s++) { SimRes *r = &J[i * ns + s].r; em += r->emerged; cr += r->crystal; if (r->emerged) printf(" e@%d", r->t_emerge); }
        printf("  → %d/%d (cristaux %d)\n", em, ns, cr);
        for (int s = 0; s < ns; s++) { SimRes *r = &J[i * ns + s].r; if (r->emerged && r->has_dom) { printf("     ex. dominant graine %d : ", s); hex(stdout, r->dom, U[i].L); printf(" H(→dominant final du survivant)=%d\n", ham(r->dom, g_A[0][5].dom, U[i].L)); break; } }
    }
    free(J); return 0;
}

/* ---------------- (e) contrôle causal : on efface la lignée au creux et on regarde si la vie revient ---------------- */
static int cmd_knock(int src, int rep, int mode, int at, int h, int nref) {
    replay_all();
    DriftJob *R = mode ? &g_B[src][rep] : &g_A[src][rep];
    /* référence de lignée : dominant (pop entière) au point de contrôle juste avant `at` (trace complète) */
    Trace *T0 = calloc(1, sizeof *T0); setup_job(&T0->D, src, rep, DK, mode); if (mode) T0->D.end = g_A[src][rep].end; T0->fin = R->dom; trace_run(T0);
    int kref = -1; for (int k = 0; k < T0->nck; k++) if (T0->ck[k].e <= at && T0->ck[k].ncop) kref = k;
    if (nref >= 0) kref = nref;
    printf("# knockout %s rép %d %s : à l'époque %d, tout programme à H≤%d du génome de référence est remplacé par du bruit\n", g_lab[src], rep, mode ? "SAUT" : "DÉRIVE", at, h);
    Trace *T = calloc(2, sizeof *T);
    for (int v = 0; v < 2; v++) {
        setup_job(&T[v].D, src, rep, DK, mode); if (mode) T[v].D.end = g_A[src][rep].end; T[v].light = 1; T[v].knock_at = at; T[v].knock_h = h;
        if (v == 0) memcpy(T[v].knock_ref, kref >= 0 ? T0->ck[kref].dom : R->dom, LMAX);
        else memcpy(T[v].knock_ref, R->dom, LMAX);
    }
    run_traces(T, 2);
    const char *nm[2] = { "réf = dominant de lignée avant le creux", "réf = dominant FINAL" };
    for (int v = 0; v < 2; v++) {
        printf("%-42s ", nm[v]); hex(stdout, T[v].knock_ref, T[v].D.base.L);
        printf(" : %d programmes effacés ; fin vivant=%d cf=%.2f ; copieurs/1024 après :", T[v].knocked, T[v].D.alive, T[v].D.cf_end);
        for (int k = 0; k < T[v].nck; k++) if (T[v].ck[k].e >= at - 64) printf(" %d@%d", T[v].ck[k].ncop, T[v].ck[k].e);
        printf("\n");
    }
    printf("témoin sans knockout : vivant=%d cf=%.2f\n", R->alive, R->cf_end);
    free(T0); free(T); return 0;
}

/* (e) fertilité : émergence spontanée (bruit pur, sans injection) sous la physique FINALE de chacune des 32 dérives K=8 ;
   les fins de trajectoire des survivants sont-elles plus fertiles que celles des morts ? */
static int cmd_fertile(int ns) {
    replay_all();
    int nu = 32; Job *J = calloc((size_t)(nu * ns), sizeof *J);
    for (int i = 0; i < nu; i++) for (int s = 0; s < ns; s++) { Job *j = &J[i * ns + s]; j->u = &g_A[i / 8][i % 8].end; j->c.N = 1024; j->c.E = 4096; j->c.E1 = 0; j->c.seed = mix(0xFE27, (uint64_t)(i * 1000 + s)); }
    run_jobs(J, nu * ns);
    printf("# (e) fertilité de la physique finale de chaque dérive K=8 : soupe de bruit pur, N=1024, E=4096, %d graines ; émergences/graines\n", ns);
    int tot[2] = { 0, 0 }, cnt[2] = { 0, 0 }, tsrc[4][2] = { { 0 } }, csrc[4][2] = { { 0 } };
    for (int i = 0; i < nu; i++) {
        int em = 0; for (int s = 0; s < ns; s++) em += J[i * ns + s].r.emerged;
        int sv = g_A[i / 8][i % 8].alive || g_B[i / 8][i % 8].alive;
        printf("%s rép %d : %2d/%d  (dérive %s, saut %s)\n", g_lab[i / 8], i % 8, em, ns, g_A[i / 8][i % 8].alive ? "VIVANT" : "mort", g_B[i / 8][i % 8].alive ? "VIVANT" : "mort");
        tot[sv] += em; cnt[sv] += ns; tsrc[i / 8][sv] += em; csrc[i / 8][sv] += ns;
    }
    for (int s = 0; s < 4; s++) printf("%s : fins mortes %d/%d, fins survivantes %d/%d\n", g_lab[s], tsrc[s][0], csrc[s][0], tsrc[s][1], csrc[s][1]);
    printf("TOTAL : physiques finales sans survivant %d/%d = %.3f ; avec survivant (dérive ou saut) %d/%d = %.3f\n", tot[0], cnt[0], (double)tot[0] / cnt[0], tot[1], cnt[1], cnt[1] ? (double)tot[1] / cnt[1] : 0);
    free(J); return 0;
}

static int selftest3(void) {
    int fail = 0;
    /* trace_run sans dérive (mode 2) == drift_run sur un univers aléatoire avec un « réplicateur » quelconque */
    Rng r; rng_seed(&r, 5); DriftJob D; memset(&D, 0, sizeof D); u_random(&D.base, &r); D.base.L = 16;
    for (int i = 0; i < LMAX; i++) D.rep[i] = (uint8_t)rng_next(&r);
    D.N = 128; D.Eest = 64; D.Edrift = 256; D.K = 8; D.mode = 0; D.seed = 42;
    Trace *T = calloc(1, sizeof *T); T->D = D; T->light = 1; drift_run(&D); trace_run(T);
    int ok = !memcmp(&T->D.end, &D.end, sizeof(Univ)) && T->D.cf_end == D.cf_end && T->D.ncurve == D.ncurve && !memcmp(T->D.curve, D.curve, sizeof D.curve);
    printf("k8_survivor selftest : trace_run rejoue drift_run à l'identique %s\n", ok ? "OK" : "ÉCHEC"); fail |= !ok; free(T);
    return fail;
}

#ifndef K8_NO_MAIN   /* rebirth.c inclut ce fichier sans son main */
int main(int argc, char **argv) {
    const char *load = NULL; int src = 0, rep = 5, mode = 0, at = 0, h = 2, ns = 16, nref = -1;
    (void)selftest2; (void)cmd_drift; (void)cmd_origin; (void)cmd_race; /* fonctions de universe_search2.c non utilisées ici */
    for (int i = 2; i + 1 < argc; i += 2) {
        if (!strcmp(argv[i], "--load")) load = argv[i + 1];
        else if (!strcmp(argv[i], "--src")) src = atoi(argv[i + 1]);
        else if (!strcmp(argv[i], "--rep")) rep = atoi(argv[i + 1]);
        else if (!strcmp(argv[i], "--mode")) mode = atoi(argv[i + 1]);
        else if (!strcmp(argv[i], "--at")) at = atoi(argv[i + 1]);
        else if (!strcmp(argv[i], "--h")) h = atoi(argv[i + 1]);
        else if (!strcmp(argv[i], "--ref")) nref = atoi(argv[i + 1]);
        else if (!strcmp(argv[i], "--seeds")) ns = atoi(argv[i + 1]);
        else if (!strcmp(argv[i], "--threads")) g_threads = atoi(argv[i + 1]);
        else { fprintf(stderr, "option inconnue %s\n", argv[i]); return 2; }
    }
    if (argc >= 2 && !strcmp(argv[1], "selftest")) return selftest3();
    if (argc < 2 || load_sources(load)) { fprintf(stderr, "usage : k8_survivor selftest | replay|trace|robust|fresh|fertile|knockout --load fin2.bin (régénérer : universe_search2 race --seed 2 --save fin2.bin)\n"); return 2; }
    if (!strcmp(argv[1], "replay")) return cmd_replay();
    if (!strcmp(argv[1], "trace")) return cmd_trace(src, rep, mode);
    if (!strcmp(argv[1], "robust")) return cmd_robust();
    if (!strcmp(argv[1], "fresh")) return cmd_fresh(ns);
    if (!strcmp(argv[1], "fertile")) return cmd_fertile(ns);
    if (!strcmp(argv[1], "knockout")) return cmd_knock(src, rep, mode, at, h, nref);
    return 2;
}
#endif
