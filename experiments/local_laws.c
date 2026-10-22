/* Lois LOCALES héritées : la sélection fait-elle émerger des gardiens de lois ? (experiments/universe-search/local-laws)
 *
 * Suite de law_keepers.c (inclus sans son main, garde LK_NO_MAIN ; même VM lx_exec, mêmes primitives READ_LAW /
 * WRITE_LAW, même anti-triche). Différence : chaque programme porte SA table des lois (kper×256 octets, la table
 * entière) et s'exécute avec elle. Règles physiques, sans oracle :
 *   exécution  : la paire (pa,pb) tourne avec les lois de pa (c'est pa qui démarre en ip=0) ;
 *   héritage   : si l'exécution a changé au moins L/2 octets de la moitié pb, pb reçoit une copie de la table de pa
 *                (critère mécanique : « qui écrit l'autre lui transmet ses lois », aucun test de fidélité) ;
 *   dérive     : chaque individu voit sa table frappée avec probabilité 0.9/K par époque (K=8), soit le même débit
 *                par lignée que la dérive globale de v2 (90 % entrées) ; les paramètres (coûts, pas, mutation) restent
 *                globaux (10 % des pas de dérive, tous les K) ;
 *   jeton      : chaque frappe de dérive donne à l'individu un jeton de réécriture (non cumulable), comme en
 *                law_keepers : la vie écrit au plus au débit de la dérive ;
 *   WRITE_LAW  : écrit 1 octet de la table PROPRE de pa (redir : position aléatoire de la table propre).
 * Bras : A-local, A-local-jet (écritures jetées), A-local-redir, local-sans-W (primitives de loi inertes : dérive
 * + héritage seuls), et en référence A global et v2 (lrun de law_keepers.c, mêmes graines).
 *
 *   ./experiments/local_laws selftest
 *   ./experiments/local_laws run --load fin2.bin [--edrift 8192] [--seeds 8] [--fert 1]
 */
#define LK_NO_MAIN
#include "law_keepers.c"

enum { LA_A, LA_JET, LA_REDIR, LA_NOW, NLA };
static const char *LAS[NLA] = { "A-local", "A-local-jet", "A-local-redir", "local-sans-W" };
static int la_arm(int a) { return a == LA_A ? AR_A : a == LA_JET ? AR_DISC : a == LA_REDIR ? AR_REDIR : AR_NONE; }

static inline void ld_tab(Univ *w, const Univ *P, const uint8_t *t, int n0) { *w = *P; memcpy(w->tab, t, (size_t)n0); }
static int tdist(const uint8_t *a, const uint8_t *b, int n0) { int d = 0; for (int i = 0; i < n0; i++) d += a[i] != b[i]; return d; }

/* la paire (pa,pb) tourne avec les lois ta de pa ; WRITE_LAW agit sur ta ; pb hérite de ta s'il a été réécrit à ≥ L/2 */
static int ll_pair(uint8_t *pa, uint8_t *pb, uint8_t *ta, uint8_t *tbp, const Univ *P, LX *c) {
    int L = P->L, n0 = P->kper * 256, ch = 0; Univ w; ld_tab(&w, P, ta, n0); uint8_t tape[TMAX];
    memcpy(tape, pa, L); memcpy(tape + L, pb, L);
    c->has_w = 0; lx_exec(tape, &w, c, 0);
    if (c->has_w) memcpy(ta, w.tab, (size_t)n0);
    for (int i = 0; i < L; i++) ch += tape[L + i] != pb[i];
    memcpy(pa, tape, L); memcpy(pb, tape + L, L);
    if (2 * ch >= L) { memcpy(tbp, ta, (size_t)n0); return 1; }
    return 0;
}

/* score de réparation d'un génome (sémantique A, écritures actives) : on casse une entrée qu'il utilise (2 valeurs
 * au hasard par entrée), on garde les cas où il ne copie plus, on lui donne un jeton et UNE auto-exécution
 * (lui en pa, bruit en pb) : répare-t-il sa table au point de copier de nouveau ? */
static void repair_score(const uint8_t *g, const uint8_t *t, const Univ *P, uint64_t sd, long *brk, long *fix) {
    int n0 = P->kper * 256, L = P->L; Univ w; ld_tab(&w, P, t, n0); uint8_t mark[1024];
    if (!lx_used(g, &w, AR_A, mark, NULL)) return;
    Rng r; rng_seed(&r, sd);
    for (int i = 0; i < n0; i++) if (mark[i]) for (int k = 0; k < 2; k++) {
        int v = (int)rng_below(&r, NPL); if (v == t[i]) continue;
        Univ w2; ld_tab(&w2, P, t, n0); set_entry(&w2, i / 256, i % 256, v);
        if (lx_copier(g, &w2, AR_A)) continue;
        (*brk)++;
        uint8_t tape[TMAX]; memcpy(tape, g, L); noise(tape + L, L, 7); LX c; memset(&c, 0, sizeof c); c.arm = AR_A; c.token = 1;
        lx_exec(tape, &w2, &c, 0);
        if (c.has_w && lx_copier(g, &w2, AR_A)) (*fix)++;
    }
}

typedef struct {
    int src, rep, arm; uint64_t seed; int N, Eest, Edrift, K, thr, fert, evdiv;
    const uint8_t *inj_g, *inj_t;
    /* sorties */
    int est, ndeath, nbirth, alive_end, ck_alive, ck_tot, fertile, t_steril;
    long ninherit, nwrite, nattempt, ndrift;
    double sp_ntab, sp_pair, sp_dcop, sp_dall; int sp_n; int last_ntab; double last_pair;
    int end_dom_dist; double end_neutral;
    long cu_eq, cu_tot, cn_eq, cn_tot;
    long ev_n, ev_cop, ev_used, ev_comp, ev_compu, ev_eff, ev_effu, ev_effc, ev_effcu, ev_fix, ev_brk, ev_noop, ev_rbase;
    double ev_expu, ev_expc, ev_expeffu, ev_expeffc;
    long rs_brk, rs_fix;
    int kc_has, kc_e, kc_i, kc_old, kc_new, kc_used; double kc_sc; uint8_t kc_g[LMAX], kc_t[1024];
    uint8_t domg[LMAX], domt[1024];
} LL;

static void mkcomp_g(const uint8_t *g, int L, int kper, const uint8_t *used, uint8_t *comp, int *ncomp, int *nusedc) { mkcomp(g, L, kper, used, comp, ncomp, nusedc); }

static void llrun(LL *X) {
    Univ P = g_base[X->src]; const Univ base = P; const int L = P.L, N = X->N & ~1, n0 = P.kper * 256, arm = la_arm(X->arm);
    uint8_t *s = malloc((size_t)N * L), *T = malloc((size_t)N * n0), *tok = calloc((size_t)N, 1); int *perm = malloc(sizeof(int) * (size_t)N);
    Rng rng, dr, rev; rng_seed(&rng, X->seed ^ 0xD81F7ULL); rng_seed(&dr, X->seed ^ 0x5EEDULL ^ ((uint64_t)X->K << 32)); rng_seed(&rev, X->seed ^ 0xE7E7ULL);
    LX c; memset(&c, 0, sizeof c); c.arm = arm; rng_seed(&c.xr, X->seed ^ 0x7ED1AULL);
    const uint8_t *g0 = X->inj_g ? X->inj_g : g_src[X->src]->best.dom; const uint8_t *t0 = X->inj_t ? X->inj_t : &base.tab[0][0];
    for (size_t i = 0; i < (size_t)N * L; i++) s[i] = (uint8_t)rng_next(&rng);
    for (int k = 0; k < N / 8; k++) memcpy(s + (size_t)k * L, g0, L);
    for (int i = 0; i < N; i++) { memcpy(T + (size_t)i * n0, t0, (size_t)n0); perm[i] = i; }
    memcpy(X->domg, g0, L); memcpy(X->domt, t0, (size_t)n0);
    const double pd = 0.9 / X->K, lqd = log(1.0 - pd);
    int Etot = X->Eest + X->Edrift, alive = 1, last_td = -1;
    for (int e = 1; e <= Etot; e++) {
        int de = e - X->Eest;
        if (de > 0) {
            for (size_t i = 0;;) {
                i += (size_t)(log(1.0 - rng_double(&dr)) / lqd); if (i >= (size_t)N) break;
                int q = (int)rng_below(&dr, P.kper), b = (int)rng_below(&dr, 256), p = (int)rng_below(&dr, NPL);
                T[i * n0 + (size_t)(q * 256 + b)] = (uint8_t)p; tok[i] = 1; X->ndrift++; i++;
            }
            if (de % X->K == 0 && rng_double(&dr) < 0.1) switch (rng_below(&dr, 4)) {
                case 0: P.rcost = (uint8_t)COSTS[rng_below(&dr, 3)]; break;
                case 1: P.wcost = (uint8_t)COSTS[rng_below(&dr, 3)]; break;
                case 2: P.steps = (uint16_t)STEPS[rng_below(&dr, 4)]; break;
                default: P.mut_i = (uint8_t)rng_below(&dr, 5); break;
            }
        }
        for (int i = N - 1; i > 0; i--) { int j = (int)rng_below(&rng, (uint32_t)(i + 1)), t = perm[i]; perm[i] = perm[j]; perm[j] = t; }
        for (int i = 0; i < N; i += 2) {
            int ia = perm[i], ib = perm[i + 1]; uint8_t *pa = s + (size_t)ia * L, *pb = s + (size_t)ib * L, *ta = T + (size_t)ia * n0;
            uint8_t pre[LMAX], tbef[1024]; int samp = 0;
            if (tok[ia] && (arm == AR_A || arm == AR_REDIR) && rng_below(&rev, (uint32_t)X->evdiv) == 0) { samp = 1; memcpy(pre, pa, L); memcpy(tbef, ta, (size_t)n0); }
            c.token = tok[ia];
            int inh = ll_pair(pa, pb, ta, T + (size_t)ib * n0, &P, &c);
            tok[ia] = (uint8_t)c.token; if (inh) tok[ib] = 0;
            X->ninherit += inh;
            if (c.has_w) {
                X->nwrite++;
                if (samp) {
                    int ix = c.wq * 256 + c.wb; Univ wb_, wa_; ld_tab(&wb_, &P, tbef, n0); ld_tab(&wa_, &P, ta, n0);
                    int c0 = lx_copier(pre, &wb_, arm) != 0, c1 = lx_copier(pre, &wa_, arm) != 0;
                    X->ev_n++; X->ev_noop += c.wold == c.wnew; X->ev_rbase += c.wnew == base.tab[c.wq][c.wb] && c.wold != c.wnew;
                    if (c0 || c1) {
                        uint8_t mark[1024], comp[1024]; int nu = lx_used(pre, c0 ? &wb_ : &wa_, arm, mark, NULL), ncomp, nusedc;
                        mkcomp_g(pre, L, P.kper, mark, comp, &ncomp, &nusedc);
                        X->ev_cop++; X->ev_used += mark[ix]; X->ev_expu += (double)nu / n0;
                        if (comp[ix]) { X->ev_comp++; X->ev_compu += mark[ix]; X->ev_expc += (double)nusedc / ncomp; }
                        if (c.wold != c.wnew) {
                            X->ev_eff++; X->ev_effu += mark[ix]; X->ev_expeffu += (double)nu / n0;
                            if (comp[ix]) { X->ev_effc++; X->ev_effcu += mark[ix]; X->ev_expeffc += (double)nusedc / ncomp; }
                        }
                        X->ev_fix += !c0 && c1; X->ev_brk += c0 && !c1;
                        if (!c0 && c1) {
                            double sc = 1 + mark[ix] + (c.wnew == base.tab[c.wq][c.wb]);
                            if (sc > X->kc_sc) { X->kc_sc = sc; X->kc_has = 1; X->kc_e = e; X->kc_i = ix; X->kc_old = c.wold; X->kc_new = c.wnew; X->kc_used = mark[ix]; memcpy(X->kc_g, pre, L); memcpy(X->kc_t, tbef, (size_t)n0); }
                        }
                    }
                }
            }
        }
        double pmut = MUTS[P.mut_i];
        if (pmut > 0) { double lq = log(1.0 - pmut); size_t tot = (size_t)N * L, i = 0; for (;;) { i += (size_t)(log(1.0 - rng_double(&rng)) / lq); if (i >= tot) break; s[i] = (uint8_t)rng_next(&rng); i++; } }
        if (e % CK == 0 || e == Etot) {
            Rng r; rng_seed(&r, X->seed * 31 + (uint64_t)e); int idx[SAMPLE], cop[SAMPLE], nc = 0;
            for (int k = 0; k < SAMPLE; k++) { idx[k] = (int)rng_below(&r, (uint32_t)N); Univ w; ld_tab(&w, &P, T + (size_t)idx[k] * n0, n0); if (lx_copier(s + (size_t)idx[k] * L, &w, arm)) cop[nc++] = idx[k]; }
            int n = 0, first = -1, lim = alive ? 1 : X->thr;
            if (nc) { n = nc >= lim ? lim : nc; first = cop[0]; }
            if (n < lim) { n = 0; for (int i = 0; i < N && n < lim; i++) { Univ w; ld_tab(&w, &P, T + (size_t)i * n0, n0); if (lx_copier(s + (size_t)i * L, &w, arm)) { if (first < 0) first = i; n++; } } }
            if (alive && n == 0) { alive = 0; X->ndeath++; last_td = e; }
            else if (!alive && n >= X->thr) { alive = 1; X->nbirth++; }
            if (alive && n > 0) {
                int bi = first, best = 0;
                for (int k = 0; k < nc; k++) { int cc = 0; for (int j = 0; j < nc; j++) cc += !memcmp(s + (size_t)cop[k] * L, s + (size_t)cop[j] * L, L); if (cc > best) { best = cc; bi = cop[k]; } }
                memcpy(X->domg, s + (size_t)bi * L, L); memcpy(X->domt, T + (size_t)bi * n0, (size_t)n0);
                if (de > 0) {
                    if (nc >= 2) {
                        int nt = 0; double pr = 0; int np = 0; double dc = 0, da = 0;
                        for (int k = 0; k < nc; k++) { int nw = 1; for (int j = 0; j < k; j++) if (!memcmp(T + (size_t)cop[k] * n0, T + (size_t)cop[j] * n0, (size_t)n0)) { nw = 0; break; } nt += nw;
                            for (int j = 0; j < k; j++) { pr += tdist(T + (size_t)cop[k] * n0, T + (size_t)cop[j] * n0, n0); np++; }
                            dc += tdist(T + (size_t)cop[k] * n0, &base.tab[0][0], n0); }
                        for (int k = 0; k < SAMPLE; k++) da += tdist(T + (size_t)idx[k] * n0, &base.tab[0][0], n0);
                        X->sp_ntab += nt; X->sp_pair += pr / np; X->sp_dcop += dc / nc; X->sp_dall += da / SAMPLE; X->sp_n++; X->last_ntab = nt; X->last_pair = pr / np;
                    }
                    Univ w; ld_tab(&w, &P, X->domt, n0); uint8_t mark[1024];
                    if (lx_used(X->domg, &w, arm, mark, NULL)) for (int i = 0; i < n0; i++) { int eq = X->domt[i] == base.tab[i / 256][i % 256]; if (mark[i]) { X->cu_tot++; X->cu_eq += eq; } else { X->cn_tot++; X->cn_eq += eq; } }
                    if ((e / CK) % 2 == 0) repair_score(X->domg, X->domt, &P, X->seed ^ (uint64_t)e, &X->rs_brk, &X->rs_fix);
                }
            }
            if (e == X->Eest) X->est = alive && X->ndeath == 0;
            if (de > 0) { X->ck_tot++; X->ck_alive += alive; }
        }
    }
    X->alive_end = alive; X->t_steril = alive ? -1 : last_td - X->Eest; X->nattempt = c.nattempt;
    X->end_dom_dist = tdist(X->domt, &base.tab[0][0], n0);
    X->end_neutral = n0 * (29.0 / 30.0) * (1.0 - pow(1.0 - 1.0 / n0, X->Edrift * pd));
    if (X->fert) { Univ w; ld_tab(&w, &P, X->domt, n0); X->fertile = lfert(&w, arm, X->seed ^ 0xF0F0); } else X->fertile = -1;
    free(s); free(T); free(tok); free(perm);
}
static void llrun_w(int i, void *ctx) { llrun(&((LL *)ctx)[i]); }

static int ll_cmd(void) {
    struct timespec t0; clock_gettime(CLOCK_MONOTONIC, &t0);
    for (int s = 0; s < 4; s++) if (make_base(s) < 0) { fprintf(stderr, "table de départ impossible %s\n", g_lab[s]); return 1; }
    int nv = 4 * g_seeds, nl = nv * NLA, ng = nv * 2;
    LL *R = calloc((size_t)nl, sizeof *R); LRun *G = calloc((size_t)ng, sizeof *G);
    for (int s = 0; s < 4; s++) for (int g = 0; g < g_seeds; g++) {
        DriftJob D; setup_job(&D, s, g, DK, 0); int k = s * g_seeds + g;
        for (int a = 0; a < NLA; a++) { LL *X = &R[k * NLA + a]; X->src = s; X->rep = g; X->arm = a; X->seed = D.seed; X->N = 1024; X->Eest = 512; X->Edrift = g_edrift; X->K = DK; X->thr = 51; X->fert = g_fert; X->evdiv = 8; }
        for (int a = 0; a < 2; a++) { LRun *X = &G[k * 2 + a]; X->src = s; X->rep = g; X->arm = a ? AR_A : AR_NONE; X->seed = D.seed; X->N = 1024; X->Eest = 512; X->Edrift = g_edrift; X->K = DK; X->thr = 51; X->fert = g_fert; }
    }
    pfor(nl, llrun_w, R); pfor(ng, lrun_w, G);
    printf("# %d runs locaux (4 sources × %d graines × %d bras) + %d runs globaux (v2, A global) ; établissement 512 + dérive %d (%d pas de dérive par lignée) ; N=1024 ; %.0f s\n",
           nl, g_seeds, NLA, ng, g_edrift, g_edrift / DK, secs(&t0));
    printf("# continuité stricte = établi et jamais 0 copieur (pop entière, contrôle /64 ép.) ; vivant = part des contrôles de dérive vivants ; fertile = émergence depuis le bruit (2048 ép.) sous la table du dernier dominant (local) / table finale (global)\n");
    printf("bras            établis  continuité  vivant  morts  naiss.  morte-fin  stéril.(méd.)  fertile  dist-table-dom(neutre)  tables≠/64  dist-entre-lignées  héritages/ép  écr/ép\n");
    double va[NLA + 2][512];
    for (int a = 0; a < NLA + 2; a++) {
        int est = 0, cont = 0, dead = 0, fert = 0; double vf = 0, md = 0, mb = 0, dd = 0, dn = 0, nt = 0, pr = 0, inh = 0, wr = 0; double st[512]; int nst = 0, nsp = 0;
        for (int k = 0; k < nv; k++) {
            if (a < NLA) {
                LL *X = &R[k * NLA + a]; est += X->est; cont += X->est && !X->ndeath; dead += !X->alive_end; fert += X->fertile > 0;
                va[a][k] = (double)X->ck_alive / X->ck_tot; md += X->ndeath; mb += X->nbirth; dd += X->end_dom_dist; dn += X->end_neutral;
                if (X->sp_n) { nt += X->sp_ntab / X->sp_n; pr += X->sp_pair / X->sp_n; nsp++; }
                inh += (double)X->ninherit / (X->Eest + X->Edrift); wr += (double)X->nwrite / (X->Eest + X->Edrift); if (!X->alive_end) st[nst++] = X->t_steril;
            } else {
                LRun *X = &G[k * 2 + (a - NLA)]; est += X->est; cont += X->est && !X->ndeath; dead += !X->alive_end; fert += X->fertile > 0;
                va[a][k] = (double)X->ck_alive / X->ck_tot; md += X->ndeath; mb += X->nbirth; dd += X->dist_end; wr += (double)X->nwrite / (X->Eest + X->Edrift); if (!X->alive_end) st[nst++] = X->t_steril;
            }
        }
        for (int i = 1; i < nst; i++) for (int j = i; j > 0 && st[j] < st[j - 1]; j--) { double t = st[j]; st[j] = st[j - 1]; st[j - 1] = t; }
        const char *nm = a < NLA ? LAS[a] : a == NLA ? "v2 (global)" : "A global";
        if (a < NLA) printf("%-15s %3d/%-3d  %3d/%-3d     %.3f  %5.2f  %5.2f   %3d/%-3d   %6.0f       %3d/%-3d   %5.1f (%5.1f)           %5.2f       %6.1f              %6.1f        %6.2f\n", nm, est, nv, cont, est, vf = mean(va[a], nv), md / nv, mb / nv, dead, nv, nst ? st[nst / 2] : -1.0, fert, nv, dd / nv, dn / nv, nsp ? nt / nsp : 0, nsp ? pr / nsp : 0, inh / nv, wr / nv);
        else printf("%-15s %3d/%-3d  %3d/%-3d     %.3f  %5.2f  %5.2f   %3d/%-3d   %6.0f       %3d/%-3d   %5.1f (table unique)    1            0                    —        %6.2f\n", nm, est, nv, cont, est, mean(va[a], nv), md / nv, mb / nv, dead, nv, nst ? st[nst / 2] : -1.0, fert, nv, dd / nv, wr / nv);
    }
    printf("\n# par source : part vivante moyenne\n");
    for (int s = 0; s < 4; s++) { printf("%-4s", g_lab[s]); for (int a = 0; a < NLA + 2; a++) { double m = 0; for (int g = 0; g < g_seeds; g++) m += va[a][s * g_seeds + g]; printf("  %s %.2f", a < NLA ? LAS[a] : a == NLA ? "v2" : "A-glob", m / g_seeds); } printf("\n"); }
    printf("\n# comparaisons appariées sur la part vivante (mêmes graines ; les bras locaux partagent la même suite de dérive), permutation à signes\n");
    int cm[6][2] = { { LA_A, LA_JET }, { LA_A, LA_REDIR }, { LA_A, LA_NOW }, { LA_NOW, NLA }, { LA_A, NLA + 1 }, { LA_JET, LA_NOW } };
    for (int k = 0; k < 6; k++) { int x = cm[k][0], y = cm[k][1], w = 0, l = 0; for (int i = 0; i < nv; i++) { w += va[x][i] > va[y][i]; l += va[x][i] < va[y][i]; }
        printf("  %-13s vs %-13s : %.3f vs %.3f (meilleur %d, pire %d)  p=%.4f\n", x < NLA ? LAS[x] : x == NLA ? "v2" : "A global", y < NLA ? LAS[y] : y == NLA ? "v2" : "A global", mean(va[x], nv), mean(va[y], nv), w, l, paired_p(va[x], va[y], nv)); }

    printf("\n# sélection entre tables héritées : conservation des entrées que le dominant UTILISE vs celles qu'il n'utilise pas (part restée égale à la table de départ, contrôles vivants)\n");
    for (int a = 0; a < NLA; a++) { long ue = 0, ut = 0, ne = 0, nt = 0; double dc = 0, da = 0; int n = 0; for (int k = 0; k < nv; k++) { LL *X = &R[k * NLA + a]; ue += X->cu_eq; ut += X->cu_tot; ne += X->cn_eq; nt += X->cn_tot; if (X->sp_n) { dc += X->sp_dcop / X->sp_n; da += X->sp_dall / X->sp_n; n++; } }
        printf("  %-13s utilisées %.3f (%ld) ; non utilisées %.3f (%ld) ; distance à la table de départ : copieurs %.1f, individus tirés au hasard %.1f\n", LAS[a], ut ? (double)ue / ut : 0, ut, nt ? (double)ne / nt : 0, nt, n ? dc / n : 0, n ? da / n : 0); }

    printf("\n# ciblage de WRITE_LAW (écritures échantillonnées 1/8, écrivain copieur avant ou après) : nul uniforme = Σ nused/n ; nul composition = cible (phase, octet présent dans l'écrivain)\n");
    for (int a = LA_A; a <= LA_REDIR; a += 2) {
        long n = 0, cop = 0, u = 0, cp = 0, cpu = 0, ef = 0, efu = 0, efc = 0, efcu = 0, fx = 0, bk = 0, nop = 0, rb = 0; double eu = 0, ec = 0, eeu = 0, eec = 0;
        for (int k = 0; k < nv; k++) { LL *X = &R[k * NLA + a]; n += X->ev_n; cop += X->ev_cop; u += X->ev_used; cp += X->ev_comp; cpu += X->ev_compu; ef += X->ev_eff; efu += X->ev_effu; efc += X->ev_effc; efcu += X->ev_effcu; fx += X->ev_fix; bk += X->ev_brk; nop += X->ev_noop; rb += X->ev_rbase; eu += X->ev_expu; ec += X->ev_expc; eeu += X->ev_expeffu; eec += X->ev_expeffc; }
        printf("  %-13s écritures %ld (sans effet %ld, retour à la loi de départ %ld) ; par un copieur %ld : sur entrée utilisée %ld (uniforme %.1f, ×%.2f)\n", LAS[a], n, nop, rb, cop, u, eu, eu > 0 ? u / eu : 0);
        printf("                composition : %ld cibles, utilisées %ld (attendu %.1f, ×%.2f, z=%.1f) ; EFFECTIVES %ld : utilisées %ld (uniforme %.1f) ; dans la composition %ld : utilisées %ld (attendu %.1f, ×%.2f, z=%.1f)\n",
               cp, cpu, ec, ec > 0 ? cpu / ec : 0, ec > 0 ? (cpu - ec) / sqrt(ec) : 0, ef, efu, eeu, efc, efcu, eec, eec > 0 ? efcu / eec : 0, eec > 0 ? (efcu - eec) / sqrt(eec) : 0);
        printf("                cassent l'écrivain %ld, le réparent %ld\n", bk, fx);
    }
    printf("\n# score de réparation des dominants (sémantique A, 1 jeton, 1 auto-exécution après casse d'une entrée utilisée) : réparées / cassées\n");
    double rsa[NLA][512];
    for (int a = 0; a < NLA; a++) { long b = 0, f = 0; for (int k = 0; k < nv; k++) { LL *X = &R[k * NLA + a]; b += X->rs_brk; f += X->rs_fix; rsa[a][k] = X->rs_brk ? (double)X->rs_fix / X->rs_brk : 0; } printf("  %-13s %ld / %ld = %.4f\n", LAS[a], f, b, b ? (double)f / b : 0); }
    printf("  A-local vs A-local-jet (par run, apparié) p=%.4f ; A-local vs local-sans-W p=%.4f\n", paired_p(rsa[LA_A], rsa[LA_JET], nv), paired_p(rsa[LA_A], rsa[LA_NOW], nv));

    /* gardien candidat : écriture qui a rendu l'écrivain copieur, meilleur score ; ablation sur le MÊME génome */
    int bk = -1; double bs = 0; for (int k = 0; k < nv; k++) { LL *X = &R[k * NLA + LA_A]; if (X->kc_has && X->kc_sc > bs) { bs = X->kc_sc; bk = k; } }
    printf("\n# gardien : écriture (A-local) qui rend l'écrivain de nouveau copieur, puis ablation sur le même génome\n");
    if (bk < 0) printf("  aucun cas.\n");
    else {
        LL *X = &R[bk * NLA + LA_A]; int L = g_base[X->src].L;
        printf("  %s graine %d, époque %d (dérive %d) : sa loi [%d][%02x] %s → %s (entrée utilisée=%d) ; génome ", g_lab[X->src], X->rep, X->kc_e, X->kc_e - X->Eest, X->kc_i / 256, X->kc_i % 256, pname(X->kc_old), pname(X->kc_new), X->kc_used);
        hex(stdout, X->kc_g, L); printf("\n");
        { Univ P = g_base[X->src]; long b = 0, f = 0; Univ w; ld_tab(&w, &P, X->kc_t, P.kper * 256); set_entry(&w, X->kc_i / 256, X->kc_i % 256, X->kc_new);
          uint8_t t1[1024]; memcpy(t1, w.tab, (size_t)P.kper * 256); repair_score(X->kc_g, t1, &P, 99, &b, &f);
          printf("  score de réparation du génome (sa table réparée, écritures actives) : %ld/%ld ; écritures retirées : 0/%ld par construction\n", f, b, b); }
        int M = 8; LL *Ab = calloc((size_t)(2 * M), sizeof *Ab);
        uint8_t t1[1024]; { Univ w = g_base[X->src]; memcpy(w.tab, X->kc_t, 1024 > 0 ? (size_t)w.kper * 256 : 0); set_entry(&w, X->kc_i / 256, X->kc_i % 256, X->kc_new); memcpy(t1, w.tab, (size_t)w.kper * 256); }
        for (int m = 0; m < 2 * M; m++) { LL *Y = &Ab[m]; Y->src = X->src; Y->rep = m % M; Y->arm = m < M ? LA_A : LA_JET; Y->seed = mix(0xAB1A, (uint64_t)(m % M)); Y->N = 1024; Y->Eest = 512; Y->Edrift = 4096; Y->K = DK; Y->thr = 51; Y->evdiv = 1 << 30; Y->inj_g = X->kc_g; Y->inj_t = t1; }
        pfor(2 * M, llrun_w, Ab);
        double fa[16], fj[16]; int ea = 0, ej = 0, za = 0, zj = 0;
        for (int m = 0; m < M; m++) { fa[m] = (double)Ab[m].ck_alive / Ab[m].ck_tot; fj[m] = (double)Ab[M + m].ck_alive / Ab[M + m].ck_tot; ea += Ab[m].est; ej += Ab[M + m].est; za += Ab[m].alive_end; zj += Ab[M + m].alive_end; }
        printf("  ablation (soupe ensemencée par ce génome et sa table, 8 graines, 512 + 4096 ép.) : écritures actives établi %d/8, part vivante %.3f, vivant en fin %d/8 ; écritures jetées établi %d/8, %.3f, %d/8 ; p apparié=%.4f\n",
               ea, mean(fa, M), za, ej, mean(fj, M), zj, paired_p(fa, fj, M));
        free(Ab);
    }
    free(R); free(G);
    printf("\ntemps total %.0f s\n", secs(&t0));
    return 0;
}

static int ll_selftest(void) {
    int fail = 0, ok; Rng r; rng_seed(&r, 0x10CA1);
    /* 1. anti-triche sur tables locales, y compris tables triviales (une seule loi partout) */
    long viol = 0, steps = 0;
    for (int it = 0; it < 1800; it++) {
        Univ u; u_random(&u, &r); int mode = it % 3, p1 = (int)rng_below(&r, NPL);
        for (int q = 0; q < 4; q++) for (int b = 0; b < 256; b++) u.tab[q][b] = (uint8_t)(mode == 0 ? p1 : mode == 1 ? (int)rng_below(&r, NPL) : (rng_below(&r, 2) ? p1 : (int)rng_below(&r, NPL)));
        for (int q = u.kper; q < 4; q++) memcpy(u.tab[q], u.tab[q % u.kper], 256);
        uint8_t t[TMAX]; for (int i = 0; i < 2 * u.L; i++) t[i] = (uint8_t)rng_next(&r);
        LX c; memset(&c, 0, sizeof c); c.arm = it % 2 ? AR_A : AR_REDIR; c.token = 1; rng_seed(&c.xr, (uint64_t)it);
        int st = lx_exec(t, &u, &c, 2); viol += st < 0; steps += st > 0 ? st : 0;
    }
    ok = viol == 0; printf("local_laws selftest : anti-triche sur tables locales (1800, dont 600 tables à loi unique ; %ld pas) 0 instruction > 1 octet %s\n", steps, ok ? "OK" : "ÉCHEC"); fail |= !ok;
    /* 2. une table à loi unique ne fait jamais de copieur (30 lois × 4 univers × 50 génomes, + génome fait main) */
    int ncop = 0; uint8_t h[SOUP_PROG]; soup_handmade(h, 1);
    for (int p = 0; p < NPL; p++) for (int k = 0; k < 4; k++) {
        Univ u; u_random(&u, &r); for (int q = 0; q < 4; q++) memset(u.tab[q], p, 256);
        for (int g = 0; g < 50; g++) { uint8_t x[LMAX]; for (int i = 0; i < u.L; i++) x[i] = (uint8_t)rng_next(&r); ncop += lx_copier(x, &u, AR_A) != 0; }
        Univ o; u_origin(&o, 1024); for (int q = 0; q < 4; q++) memset(o.tab[q], p, 256); ncop += lx_copier(h, &o, AR_A) != 0;
    }
    ok = ncop == 0; printf("local_laws selftest : aucune table à loi unique ne produit de copieur (%d trouvés) %s\n", ncop, ok ? "OK" : "ÉCHEC"); fail |= !ok;
    /* 3. héritage : le copieur fait main transmet sa table à la moitié qu'il réécrit ; un programme inerte ne transmet rien ;
     *    WRITE_LAW ne touche que la table de pa */
    {
        Univ o; u_origin(&o, 1024); int n0 = 256; uint8_t ta[1024], tb[1024], pa[LMAX], pb[LMAX];
        memcpy(ta, o.tab[0], 256); ta[255] = P_NOP; memcpy(tb, o.tab[0], 256); tb[254] = P_INC_A;
        memcpy(pa, h, 64); for (int i = 0; i < 64; i++) pb[i] = (uint8_t)rng_next(&r);
        LX c; memset(&c, 0, sizeof c); c.arm = AR_A;
        int cop = lx_copier(h, &o, AR_A) != 0, inh = ll_pair(pa, pb, ta, tb, &o, &c), same_t = !memcmp(ta, tb, (size_t)n0);
        uint8_t ta2[1024], tb2[1024]; memcpy(ta2, o.tab[0], 256); memcpy(tb2, o.tab[0], 256); tb2[3] = P_INC_A; uint8_t z[LMAX] = { 0 }, y[LMAX];
        for (int i = 0; i < 64; i++) y[i] = (uint8_t)rng_next(&r);
        int inh2 = ll_pair(z, y, ta2, tb2, &o, &c);  /* génome nul : octet 0 = NOP, n'écrit rien */
        /* WRITE_LAW partout dans la table de pa, jeton : seule ta3 change, d'au plus 1 entrée */
        Univ u = o; for (int b = 0; b < 256; b++) u.tab[0][b] = (uint8_t)(b % 3 ? P_WLAW : P_INC_B);
        uint8_t ta3[1024], tb3[1024], q1[LMAX], q2[LMAX]; memcpy(ta3, u.tab[0], 256); memcpy(tb3, o.tab[0], 256);
        for (int i = 0; i < 64; i++) { q1[i] = (uint8_t)rng_next(&r); q2[i] = (uint8_t)rng_next(&r); }
        uint8_t ta3b[1024], tb3b[1024]; memcpy(ta3b, ta3, 256); memcpy(tb3b, tb3, 256);
        c.token = 1; int inh3 = ll_pair(q1, q2, ta3, tb3, &u, &c);
        int okw = tdist(ta3, ta3b, 256) <= 1 && tdist(ta3, ta3b, 256) == c.has_w && (inh3 || !memcmp(tb3, tb3b, 256));
        ok = cop && inh && same_t && !inh2 && tb2[3] == P_INC_A && okw;
        printf("local_laws selftest : héritage (copieur fait main=%d transmet=%d, inerte transmet=%d) ; WRITE_LAW sur la table propre seulement %s\n", cop, inh && same_t, inh2, ok ? "OK" : "ÉCHEC"); fail |= !ok;
    }
    return fail;
}

int main(int argc, char **argv) {
    const char *load = NULL;
    (void)lk_run; (void)lk_selftest;
    (void)selftest2; (void)cmd_drift; (void)cmd_origin; (void)cmd_race; (void)selftest3; (void)cmd_replay; (void)cmd_trace; (void)cmd_robust; (void)cmd_fresh; (void)cmd_fertile; (void)cmd_knock;
    (void)selftest4; (void)cmd_pool; (void)cmd_cycles; (void)cmd_persist; (void)cmd_features; (void)load_pool;
    for (int i = 2; i + 1 < argc; i += 2) {
        if (!strcmp(argv[i], "--load")) load = argv[i + 1];
        else if (!strcmp(argv[i], "--edrift")) g_edrift = atoi(argv[i + 1]);
        else if (!strcmp(argv[i], "--seeds")) g_seeds = atoi(argv[i + 1]);
        else if (!strcmp(argv[i], "--fert")) g_fert = atoi(argv[i + 1]);
        else if (!strcmp(argv[i], "--threads")) g_threads = atoi(argv[i + 1]);
        else { fprintf(stderr, "option inconnue %s\n", argv[i]); return 2; }
    }
    if (argc >= 2 && !strcmp(argv[1], "selftest")) return ll_selftest();
    if (argc < 2 || strcmp(argv[1], "run") || load_sources(load)) { fprintf(stderr, "usage : local_laws selftest | run --load fin2.bin [--edrift 8192] [--seeds 8] [--fert 1]\n"); return 2; }
    return ll_cmd();
}
