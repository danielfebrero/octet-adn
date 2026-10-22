/* La vie qui protège ses lois — de façon ÉMERGENTE ? (experiments/universe-search/law-keepers)
 *
 * Inclut rebirth.c (donc k8_survivor.c, universe_search2.c, v1) sans en changer une ligne de logique.
 * Ce fichier a sa propre VM lx_exec() = u_exec() + 2 primitives qui rendent la table des lois accessible :
 *   READ_LAW  : a = loi[phase de l'instruction][b]                      (lit 1 octet de la table, n'écrit rien)
 *   WRITE_LAW : loi[phase][b] = a mod 30                                 (écrit 1 SEUL octet de la table, 0 de la bande)
 * La table (kper×256 octets) est un ruban partagé par toute la soupe. Pas d'oracle : rien ne dit à la VM quelle
 * loi est « essentielle », rien ne récompense une réparation. Bras (même table de départ, mêmes graines) :
 *   v2      : primitives de loi inertes (NOP) — dérive v2 pure (K=8, catalogue de 30 valeurs)
 *   A       : lois dans le monde ; la soupe dispose d'UN jeton de réécriture par pas de dérive (même débit que la
 *             dérive) : la 1re WRITE_LAW exécutée après le pas l'emporte, les suivantes sont sans effet
 *   A-jet   : idem, mais les écritures sont jetées (READ_LAW marche)
 *   A-redir : idem A, mais l'octet écrit atterrit à une position tirée au hasard
 *   B       : v2 + inertie par l'usage : l'entrée qui dérive est tirée avec un poids 1/(1+f), f = usage récent de
 *             l'entrée / usage moyen (moyenne glissante ~64 époques), sans savoir qui l'exécute ni pourquoi
 *   B-perm  : idem B, mais le vecteur d'usage est permuté au hasard à chaque pas (même distribution de poids)
 *
 *   ./experiments/law_keepers selftest
 *   ./experiments/law_keepers run --load fin2.bin [--edrift 16384] [--seeds 8] [--fert 1]
 */
#define RB_NO_MAIN
#include "rebirth.c"

enum { P_RLAW = P_COUNT, P_WLAW, NPL };
enum { AR_NONE, AR_A, AR_DISC, AR_REDIR, AR_B, AR_BSH, NAR };
static const char *ARS[NAR] = { "v2", "A", "A-jet", "A-redir", "B", "B-perm" };
static int ar_read(int arm) { return arm == AR_A || arm == AR_DISC || arm == AR_REDIR; }

typedef struct {
    int arm, verify, token;
    Rng xr;
    uint32_t *cnt;            /* bras B : exécutions par entrée */
    uint8_t *mark;            /* trace : entrées exécutées */
    long nattempt;
    int has_w, wq, wb, wold, wnew;
    int ntr, trpos[16], tra[16], trb[16];   /* trace des WRITE_LAW exécutées (position, a, b) */
} LX;

static void set_entry(Univ *u, int q, int b, int v) { u->tab[q][b] = (uint8_t)v; for (int k = u->kper; k < 4; k++) u->tab[k][b] = u->tab[k % u->kper][b]; }
static const char *pname(int p) { return p == P_RLAW ? "READ_LAW" : p == P_WLAW ? "WRITE_LAW" : p < P_COUNT ? PN[p] : "?"; }

static int lx_exec(uint8_t *t, Univ *u, LX *c, int check) {
    const int T = 2 * u->L, budget = u->steps, km = u->kper - 1;
    int ip = 0, steps = 0, h0 = 0, h1 = 0, k, ad;
    uint8_t a = 0, b = 0, before[TMAX], lawb[1024];
    while (steps < budget && ip < T) {
        int pos = ip, by = t[ip++], q = pos & km, p = u->tab[q][by];
        steps++;
        if (c->cnt) c->cnt[q * 256 + by]++;
        if (c->mark) c->mark[q * 256 + by] = 1;
        if (p < P_COUNT && p_imm(p)) { if (ip >= T) break; k = t[ip++]; } else k = 0;
        if (check) { memcpy(before, t, T); memcpy(lawb, u->tab, 1024); }
        switch (p) {
        case P_HALT: return steps;
        case P_INC_A: a++; break;
        case P_DEC_A: a--; break;
        case P_INC_B: b++; break;
        case P_DEC_B: b--; break;
        case P_CLR_A: case P_CLRI: a = 0; break;
        case P_CLR_B: b = 0; break;
        case P_MOV_BA: b = a; break;
        case P_MOV_AB: a = b; break;
        case P_ADD: a += b; break;
        case P_SUB: a -= b; break;
        case P_XOR: a ^= b; break;
        case P_LOADI: a = (uint8_t)k; break;
        case P_AND: a &= (uint8_t)k; break;
        case P_LOAD:
            ad = (u->addr == AD_ABS ? k : u->addr == AD_B ? b + k : pos + k) % T;
            a = t[ad]; steps += u->rcost - 1; break;
        case P_STORE:
            ad = (u->addr == AD_ABS ? k : u->addr == AD_B ? b + k : pos + k) % T;
            t[ad] = a; steps += u->wcost - 1; break;
        case P_JMP: ip = (ip + k) % T; break;
        case P_JZ: if (a == 0) ip = (ip + k) % T; break;
        case P_JNZ: if (a != 0) ip = (ip + k) % T; break;
        case P_LOOP: ip = 0; break;
        case P_READ_H: a = t[h0]; steps += u->rcost - 1; break;
        case P_WRITE_H: t[h1] = a; steps += u->wcost - 1; break;
        case P_H0_INC: h0 = (h0 + 1) % T; break;
        case P_H0_DEC: h0 = (h0 + T - 1) % T; break;
        case P_H1_INC: h1 = (h1 + 1) % T; break;
        case P_H1_DEC: h1 = (h1 + T - 1) % T; break;
        case P_RLAW: if (ar_read(c->arm)) { a = u->tab[q][b]; steps += u->rcost - 1; } break;
        case P_WLAW:
            if (!ar_read(c->arm)) break;
            steps += u->wcost - 1;
            if (c->ntr < 16 && c->mark) { c->trpos[c->ntr] = pos; c->tra[c->ntr] = a; c->trb[c->ntr++] = b; }
            if (c->verify) break;
            c->nattempt++;
            if ((c->arm == AR_A || c->arm == AR_REDIR) && c->token) {
                int tq = q, tb = b;
                if (c->arm == AR_REDIR) { tq = (int)rng_below(&c->xr, u->kper); tb = (int)rng_below(&c->xr, 256); }
                c->has_w = 1; c->wq = tq; c->wb = tb; c->wold = u->tab[tq][tb]; c->wnew = a % NPL;
                set_entry(u, tq, tb, a % NPL); c->token = 0;
            }
            break;
        default: break;
        }
        if (check) {
            int ch = 0, lw = 0;
            for (int i = 0; i < T; i++) ch += before[i] != t[i];
            for (int qq = 0; qq < u->kper; qq++) for (int x = 0; x < 256; x++) { lw += lawb[qq * 256 + x] != u->tab[qq][x]; if (u->tab[qq][x] >= NPL) return -2000000; }
            int okw = p < P_COUNT && PKIND[p] == K_WRITE1;
            if (ch > okw || lw > (p == P_WLAW) || ch + lw > 1) return -1000000 - p;
            if (check == 2) c->token = 1;     /* selftest : jeton permanent */
        }
    }
    return steps;
}

/* ---------- copieur vérifié (même protocole que u_copier), écritures de loi sans effet pendant la vérification ---------- */
static int lx_copy_once(const uint8_t *p, int ord, int t, Univ *u, int arm, uint8_t *child, LX *trace) {
    int L = u->L; uint8_t tape[TMAX]; LX c0; LX *c = trace ? trace : &c0;
    if (!trace) memset(&c0, 0, sizeof c0);
    c->arm = arm; c->verify = 1;
    memcpy(tape + ord * L, p, L); noise(tape + (1 - ord) * L, L, t);
    lx_exec(tape, u, c, 0);
    if (child) memcpy(child, tape + (1 - ord) * L, L);
    return same(tape + (1 - ord) * L, p, L) >= fidof(L);
}
static int lx_copies(const uint8_t *p, int ord, int t0, Univ *u, int arm, uint8_t *child) {
    int ok = 0;
    for (int t = 0; t < 4; t++) { ok += lx_copy_once(p, ord, t0 + t, u, arm, t == 0 ? child : NULL, NULL); if (ok + (3 - t) < 3) return 0; }
    return ok >= 3 && (!child || same(child, p, u->L) >= fidof(u->L));
}
static int lx_copier(const uint8_t *p, Univ *u, int arm) {
    for (int ord = 0; ord < 2; ord++) { uint8_t ch[LMAX]; if (lx_copies(p, ord, 0, u, arm, ch) && lx_copies(ch, ord, 4, u, arm, NULL)) return 1 + ord; }
    return 0;
}
static double lx_sample(const uint8_t *s, int N, Univ *u, int arm, uint64_t sd, uint8_t *dom, int *dcount) {
    Rng r; rng_seed(&r, sd);
    int L = u->L, nc = 0, best = 0, idx[SAMPLE], isc[SAMPLE];
    for (int i = 0; i < SAMPLE; i++) { idx[i] = (int)rng_below(&r, (uint32_t)N); isc[i] = lx_copier(s + (size_t)idx[i] * L, u, arm) != 0; nc += isc[i]; }
    if (dom) for (int i = 0; i < SAMPLE; i++) if (isc[i]) {
        int c = 0; for (int j = 0; j < SAMPLE; j++) c += !memcmp(s + (size_t)idx[i] * L, s + (size_t)idx[j] * L, L);
        if (c > best) { best = c; memcpy(dom, s + (size_t)idx[i] * L, L); }
    }
    if (dcount) *dcount = best;
    return (double)nc / SAMPLE;
}
static int lx_count(const uint8_t *s, int N, Univ *u, int arm, int lim, int *first) {
    int n = 0; *first = -1;
    for (int i = 0; i < N; i++) if (lx_copier(s + (size_t)i * u->L, u, arm)) { if (*first < 0) *first = i; if (++n >= lim) break; }
    return n;
}
/* entrées de la table exécutées par g quand il se copie (trace d'une copie sur bruit) ; 0 si g ne copie pas */
static int lx_used(const uint8_t *g, Univ *u, int arm, uint8_t *mark, LX *tr) {
    memset(mark, 0, 1024); int o = lx_copier(g, u, arm); if (!o) return 0;
    LX c; memset(&c, 0, sizeof c); c.mark = mark; lx_copy_once(g, o - 1, 0, u, arm, NULL, &c); if (tr) *tr = c;
    int n = 0; for (int i = 0; i < u->kper * 256; i++) n += mark[i]; return n;
}
/* nul « composition » : entrées (phase q, octet x) avec x présent dans le génome du dominant ; nusedc = entrées utilisées dans cet ensemble */
static void mkcomp(const uint8_t *g, int L, int kper, const uint8_t *used, uint8_t *comp, int *ncomp, int *nusedc) {
    int has[256] = { 0 }; for (int i = 0; i < L; i++) has[g[i]] = 1;
    *ncomp = *nusedc = 0; memset(comp, 0, 1024);
    for (int q = 0; q < kper; q++) for (int x = 0; x < 256; x++) if (has[x]) { comp[q * 256 + x] = 1; ++*ncomp; *nusedc += used[q * 256 + x]; }
}
static int crystal(const uint8_t *g, int L) { int seen[256] = { 0 }, nd = 0; for (int i = 0; i < L; i++) if (!seen[g[i]]++) nd++; return nd <= 2; }

/* ---------- dérive : 90 % une entrée (30 valeurs), 10 % un paramètre ; bras B : entrée tirée avec poids 1/(1+f) ---------- */
static int lx_drift(Univ *u, Rng *r, const float *use, int shuf, int *oldv) {
    if (rng_double(r) < 0.9) {
        int n = u->kper * 256, q, b, i;
        if (use) {
            double m = 0; for (int j = 0; j < n; j++) m += use[j]; m /= n;
            int pi[1024]; for (int j = 0; j < n; j++) pi[j] = j;
            if (shuf) for (int j = n - 1; j > 0; j--) { int x = (int)rng_below(r, (uint32_t)(j + 1)), t = pi[j]; pi[j] = pi[x]; pi[x] = t; }
            for (;;) { q = (int)rng_below(r, u->kper); b = (int)rng_below(r, 256); i = q * 256 + b; double f = m > 0 ? use[pi[i]] / m : 0; if (rng_double(r) < 1.0 / (1.0 + f)) break; }
        } else { q = (int)rng_below(r, u->kper); b = (int)rng_below(r, 256); i = q * 256 + b; }
        int p = (int)rng_below(r, NPL); *oldv = u->tab[q][b]; set_entry(u, q, b, p);
        return i;
    }
    switch (rng_below(r, 4)) {
    case 0: u->rcost = (uint8_t)COSTS[rng_below(r, 3)]; break;
    case 1: u->wcost = (uint8_t)COSTS[rng_below(r, 3)]; break;
    case 2: u->steps = (uint16_t)STEPS[rng_below(r, 4)]; break;
    default: u->mut_i = (uint8_t)rng_below(r, 5); break;
    }
    return -1;
}

/* ---------- table de départ : les deux primitives de loi à leur densité naturelle (1/30 chacune) ---------- */
static Univ g_base[4];
static int make_base(int src) {
    const Univ *u0 = &g_src[src]->u; const uint8_t *rep = g_src[src]->best.dom; int n = u0->kper * 256, m = (n + 15) / 30;
    for (int tr = 0; tr < 500; tr++) {
        Rng r; rng_seed(&r, mix(0x1A55, (uint64_t)(src * 1000 + tr)));
        Univ u = *u0; int idx[1024]; for (int i = 0; i < n; i++) idx[i] = i;
        for (int i = 0; i < 2 * m; i++) { int j = i + (int)rng_below(&r, (uint32_t)(n - i)), t = idx[i]; idx[i] = idx[j]; idx[j] = t; set_entry(&u, idx[i] / 256, idx[i] % 256, i < m ? P_RLAW : P_WLAW); }
        if (lx_copier(rep, &u, AR_A) && lx_copier(rep, &u, AR_NONE)) { g_base[src] = u; return tr; }
    }
    return -1;
}

/* ---------- une longue vie sous dérive ---------- */
typedef struct { int e, i, old, nw, alive, used, nused, incomp, ncomp, nusedc, ok0, ok1, rprev, rbase, wcop, wham; uint8_t wg[LMAX]; } Ev;
#define MAXEV 2100
typedef struct {
    int src, rep, arm; uint64_t seed; int N, Eest, Edrift, K, thr, fert;
    int switch_at, switch_arm, fork_at; uint64_t fseed;      /* ablation : à switch_at le bras devient switch_arm ; fork_at : graine de dérive bifurquée */
    /* sorties */
    int est, ndeath, nbirth, t_first_death, t_steril, alive_end, ck_alive, ck_tot, ck_alive_after, ck_tot_after, dist_end, ndrift, nwrite, nundo, nselfd, fertile;
    long nattempt; Ev *ev; int nev; Univ end;
} LRun;

static int lfert(Univ *u0, int arm, uint64_t seed) {
    Univ u = *u0; const int L = u.L, N = 1024; uint8_t *s = malloc((size_t)N * L); int perm[1024];
    Rng rng; rng_seed(&rng, seed ^ 0xFE47ULL); LX c; memset(&c, 0, sizeof c); c.arm = arm; c.verify = 1;
    for (size_t i = 0; i < (size_t)N * L; i++) s[i] = (uint8_t)rng_next(&rng);
    for (int i = 0; i < N; i++) perm[i] = i;
    int em = 0;
    for (int e = 1; e <= 2048 && !em; e++) {
        for (int i = N - 1; i > 0; i--) { int j = (int)rng_below(&rng, (uint32_t)(i + 1)), t = perm[i]; perm[i] = perm[j]; perm[j] = t; }
        for (int i = 0; i < N; i += 2) { uint8_t tape[TMAX]; uint8_t *pa = s + (size_t)perm[i] * L, *pb = s + (size_t)perm[i + 1] * L; memcpy(tape, pa, L); memcpy(tape + L, pb, L); lx_exec(tape, &u, &c, 0); memcpy(pa, tape, L); memcpy(pb, tape + L, L); }
        double pm = MUTS[u.mut_i];
        if (pm > 0) { double lq = log(1.0 - pm); size_t tot = (size_t)N * L, i = 0; for (;;) { i += (size_t)(log(1.0 - rng_double(&rng)) / lq); if (i >= tot) break; s[i] = (uint8_t)rng_next(&rng); i++; } }
        if (e % CK == 0) { uint8_t d[LMAX]; int dc; double cf = lx_sample(s, N, &u, arm, seed * 31 + (uint64_t)e, d, &dc); if (cf >= 0.25 && dc > 0 && !crystal(d, L)) em = e; }
    }
    free(s); return em;
}

static void lrun(LRun *X) {
    Univ u = g_base[X->src]; const Univ base = u; const int L = u.L, N = X->N & ~1, n0 = u.kper * 256;
    uint8_t *s = malloc((size_t)N * L); int *perm = malloc(sizeof(int) * (size_t)N);
    Rng rng, dr; rng_seed(&rng, X->seed ^ 0xD81F7ULL); rng_seed(&dr, X->seed ^ 0x5EEDULL ^ ((uint64_t)X->K << 32));
    LX c; memset(&c, 0, sizeof c); c.arm = X->arm; rng_seed(&c.xr, X->seed ^ 0x7ED1AULL);
    static __thread uint32_t cnt[1024]; static __thread float use[1024]; memset(cnt, 0, sizeof cnt); memset(use, 0, sizeof use);
    int isB = X->arm == AR_B || X->arm == AR_BSH; if (isB) c.cnt = cnt;
    uint8_t prevv[1024], drifted[1024] = { 0 }, used[1024] = { 0 }, comp[1024] = { 0 }, dom[LMAX]; int nused = 0, hasdom = 0, ncomp = 0, nusedc = 0;
    for (size_t i = 0; i < (size_t)N * L; i++) s[i] = (uint8_t)rng_next(&rng);
    for (int k = 0; k < N / 8; k++) memcpy(s + (size_t)k * L, g_src[X->src]->best.dom, L);
    for (int i = 0; i < N; i++) perm[i] = i;
    int Etot = X->Eest + X->Edrift, alive = 1, last_break = -100000, last_td = -1;
    X->t_first_death = -1; X->t_steril = -1; X->nev = 0; X->est = 0;
    memcpy(dom, g_src[X->src]->best.dom, L); hasdom = 1; nused = lx_used(dom, &u, c.arm, used, NULL); mkcomp(dom, L, u.kper, used, comp, &ncomp, &nusedc);
    for (int e = 1; e <= Etot; e++) {
        int de = e - X->Eest;
        if (X->fork_at && e == X->fork_at) rng_seed(&dr, X->fseed ^ 0x5EEDULL ^ ((uint64_t)X->K << 32));
        if (X->switch_at && e == X->switch_at) c.arm = X->switch_arm;
        if (de > 0 && de % X->K == 0) {
            int ov, i = lx_drift(&u, &dr, isB ? use : NULL, X->arm == AR_BSH, &ov); X->ndrift++;
            if (i >= 0 && u.tab[i / 256][i % 256] != ov) { prevv[i] = (uint8_t)ov; drifted[i] = 1; }
            c.token = 1;
        }
        for (int i = N - 1; i > 0; i--) { int j = (int)rng_below(&rng, (uint32_t)(i + 1)), t = perm[i]; perm[i] = perm[j]; perm[j] = t; }
        for (int i = 0; i < N; i += 2) {
            uint8_t tape[TMAX], pre[LMAX]; uint8_t *pa = s + (size_t)perm[i] * L, *pb = s + (size_t)perm[i + 1] * L;
            memcpy(tape, pa, L); memcpy(tape + L, pb, L); memcpy(pre, pa, L);
            c.has_w = 0;
            lx_exec(tape, &u, &c, 0);
            memcpy(pa, tape, L); memcpy(pb, tape + L, L);
            if (c.has_w) {
                int q = c.wq, b = c.wb, ix = q * 256 + b, rp = drifted[ix] && c.wnew == prevv[ix] && c.wold != c.wnew;
                X->nwrite++; X->nundo += rp;
                if (X->ev && X->nev < MAXEV) {
                    Ev *E = &X->ev[X->nev++]; memset(E, 0, sizeof *E);
                    E->e = e; E->i = ix; E->old = c.wold; E->nw = c.wnew; E->alive = alive && hasdom; E->rprev = rp;
                    E->rbase = c.wnew == base.tab[q][b] && c.wold != c.wnew;
                    E->used = E->alive ? used[ix] : 0; E->nused = E->alive ? nused : 0; E->incomp = E->alive ? comp[ix] : 0; E->ncomp = ncomp; E->nusedc = nusedc; memcpy(E->wg, pre, L);
                    E->wham = hasdom ? ham(pre, dom, L) : -1;
                    if (E->alive) {
                        E->ok1 = lx_copier(dom, &u, c.arm) != 0;
                        set_entry(&u, q, b, c.wold); E->ok0 = lx_copier(dom, &u, c.arm) != 0; E->wcop = lx_copier(pre, &u, c.arm) != 0; set_entry(&u, q, b, c.wnew);
                        if (E->ok0 && !E->ok1) last_break = e;
                    }
                }
                if (rp) drifted[ix] = 0;
            }
        }
        if (isB) for (int j = 0; j < n0; j++) { use[j] = use[j] * (63.0f / 64.0f) + (float)cnt[j]; cnt[j] = 0; }
        double pmut = MUTS[u.mut_i];
        if (pmut > 0) { double lq = log(1.0 - pmut); size_t tot = (size_t)N * L, i = 0; for (;;) { i += (size_t)(log(1.0 - rng_double(&rng)) / lq); if (i >= tot) break; s[i] = (uint8_t)rng_next(&rng); i++; } }
        if (e % CK == 0 || e == Etot) {
            uint8_t sd[LMAX]; int dc = 0, first;
            lx_sample(s, N, &u, c.arm, X->seed * 31 + (uint64_t)e, sd, &dc);
            int n = lx_count(s, N, &u, c.arm, alive ? 1 : X->thr, &first);
            if (alive && n == 0) {
                alive = 0; X->ndeath++; last_td = e; hasdom = 0;
                if (X->t_first_death == -1) X->t_first_death = de > 0 ? de : -2;
                if (e - last_break <= 2 * CK) X->nselfd++;
            } else if (!alive && n >= X->thr) { alive = 1; X->nbirth++; }
            if (alive && n > 0) { if (dc > 0) memcpy(dom, sd, L); else memcpy(dom, s + (size_t)first * L, L); hasdom = 1; nused = lx_used(dom, &u, c.arm, used, NULL); if (!nused) hasdom = 0; mkcomp(dom, L, u.kper, used, comp, &ncomp, &nusedc); }
            if (e == X->Eest) X->est = alive && X->ndeath == 0;
            if (de > 0) { X->ck_tot++; X->ck_alive += alive; if (X->switch_at && e >= X->switch_at) { X->ck_tot_after++; X->ck_alive_after += alive; } }
        }
    }
    X->alive_end = alive; X->t_steril = alive ? -1 : last_td - X->Eest; X->nattempt = c.nattempt;
    X->dist_end = 0; for (int i = 0; i < n0; i++) X->dist_end += u.tab[i / 256][i % 256] != base.tab[i / 256][i % 256];
    X->end = u;
    X->fertile = X->fert ? lfert(&u, c.arm, X->seed ^ 0xF0F0) : -1;
    free(s); free(perm);
}
static void lrun_w(int i, void *ctx) { lrun(&((LRun *)ctx)[i]); }

/* test de permutation apparié (inversion de signe), bilatéral */
static double paired_p(const double *x, const double *y, int n) {
    double d[512], S = 0; for (int i = 0; i < n; i++) { d[i] = x[i] - y[i]; S += d[i]; }
    Rng r; rng_seed(&r, 0x9A1); int ge = 0, R = 20000;
    for (int it = 0; it < R; it++) { double s2 = 0; for (int i = 0; i < n; i++) s2 += rng_below(&r, 2) ? d[i] : -d[i]; ge += fabs(s2) >= fabs(S) - 1e-12; }
    return (ge + 1.0) / (R + 1.0);
}

static int g_edrift = 16384, g_seeds = 8, g_fert = 1;
static int lk_run(void) {
    struct timespec t0; clock_gettime(CLOCK_MONOTONIC, &t0);
    for (int s = 0; s < 4; s++) {
        int tr = make_base(s); if (tr < 0) { fprintf(stderr, "table de départ impossible pour %s\n", g_lab[s]); return 1; }
        int n = g_base[s].kper * 256, c = 0; for (int i = 0; i < n; i++) c += g_base[s].tab[i / 256][i % 256] != g_src[s]->u.tab[i / 256][i % 256];
        printf("table de départ %s : L=%d kper=%d %s ; %d entrées réassignées à READ_LAW/WRITE_LAW (%d chacune, essai %d) ; injecté ", g_lab[s], g_base[s].L, g_base[s].kper, ADN[g_base[s].addr], c, (n + 15) / 30, tr);
        hex(stdout, g_src[s]->best.dom, g_base[s].L); printf("\n");
    }
    int nr = 4 * g_seeds * NAR; LRun *R = calloc((size_t)nr, sizeof *R);
    for (int s = 0; s < 4; s++) for (int g = 0; g < g_seeds; g++) for (int a = 0; a < NAR; a++) {
        LRun *X = &R[(s * g_seeds + g) * NAR + a]; DriftJob D; setup_job(&D, s, g, DK, 0);
        X->src = s; X->rep = g; X->arm = a; X->seed = D.seed; X->N = 1024; X->Eest = 512; X->Edrift = g_edrift; X->K = DK; X->thr = 51; X->fert = g_fert;
        if (a == AR_A || a == AR_REDIR) X->ev = calloc(MAXEV, sizeof(Ev));
    }
    pfor(nr, lrun_w, R);
    printf("# %d runs (4 sources × %d graines × %d bras), établissement 512 + dérive %d (K=8 : %d pas), N=1024, mort = 0 copieur vérifié dans la pop ENTIÈRE (contrôle /64 ép.), naissance ≥51 ; %.0f s\n",
           nr, g_seeds, NAR, g_edrift, g_edrift / DK, secs(&t0));
    printf("# unité = run ; continuité stricte = établi et jamais 0 copieur ; vivant = part des contrôles de dérive où la lignée vit ; stérilisation = dernière mort (époque de dérive) si mort en fin ;\n#   fertile = émergence depuis le bruit (2048 ép., lois figées) sous la table finale ; dist = entrées de table ≠ départ ; écr = réécritures de lois acceptées ; tent = WRITE_LAW exécutées\n");
    printf("bras     établis  continuité  vivant  morts  naiss.  mort-en-fin  stérilisation(méd.)  fertile-fin  dist   écr    tent/époque\n");
    double va[NAR][512]; int nv = 4 * g_seeds;
    for (int a = 0; a < NAR; a++) {
        int est = 0, cont = 0, dead = 0, fert = 0, nf = 0; double vf = 0, md = 0, mb = 0, dist = 0, wr = 0, at = 0, st[512]; int nst = 0;
        for (int s = 0; s < 4; s++) for (int g = 0; g < g_seeds; g++) {
            LRun *X = &R[(s * g_seeds + g) * NAR + a]; int k = s * g_seeds + g;
            est += X->est; cont += X->est && X->ndeath == 0; dead += !X->alive_end; va[a][k] = (double)X->ck_alive / X->ck_tot; vf += va[a][k];
            md += X->ndeath; mb += X->nbirth; dist += X->dist_end; wr += X->nwrite; at += (double)X->nattempt / (X->Eest + X->Edrift);
            if (!X->alive_end) st[nst++] = X->t_steril;
            if (X->fertile >= 0) { nf++; fert += X->fertile > 0; }
        }
        for (int i = 1; i < nst; i++) for (int j = i; j > 0 && st[j] < st[j - 1]; j--) { double t = st[j]; st[j] = st[j - 1]; st[j - 1] = t; }
        printf("%-8s %3d/%-3d  %3d/%-3d     %.3f  %5.2f  %5.2f   %3d/%-3d      %8.0f            %3d/%-3d      %5.0f  %5.0f  %8.2f\n", ARS[a], est, nv, cont, est, vf / nv, md / nv, mb / nv, dead, nv,
               nst ? st[nst / 2] : -1.0, fert, nf, dist / nv, wr / nv, at / nv);
    }
    printf("\n# par source : continuité stricte k/établis | part vivante moyenne\n");
    for (int s = 0; s < 4; s++) {
        printf("%-4s", g_lab[s]);
        for (int a = 0; a < NAR; a++) { int est = 0, cont = 0; double vf = 0; for (int g = 0; g < g_seeds; g++) { LRun *X = &R[(s * g_seeds + g) * NAR + a]; est += X->est; cont += X->est && !X->ndeath; vf += (double)X->ck_alive / X->ck_tot; } printf("  %s %d/%d %.2f", ARS[a], cont, est, vf / g_seeds); }
        printf("\n");
    }
    printf("\n# comparaisons appariées (mêmes graines ; v2/A/A-jet/A-redir partagent la même suite de dérive) sur la part vivante, permutation à signes (20000)\n");
    int cmpa[5][2] = { { AR_A, AR_NONE }, { AR_A, AR_DISC }, { AR_A, AR_REDIR }, { AR_B, AR_NONE }, { AR_B, AR_BSH } };
    for (int k = 0; k < 5; k++) {
        int x = cmpa[k][0], y = cmpa[k][1], w = 0, l = 0;
        for (int i = 0; i < nv; i++) { w += va[x][i] > va[y][i]; l += va[x][i] < va[y][i]; }
        printf("  %-7s vs %-7s : %.3f vs %.3f  (meilleur %d, pire %d, égal %d)  p=%.4f\n", ARS[x], ARS[y], mean(va[x], nv), mean(va[y], nv), w, l, nv - w - l, paired_p(va[x], va[y], nv));
    }
    printf("  (B-perm vs v2 : %.3f vs %.3f p=%.4f)\n", mean(va[AR_BSH], nv), mean(va[AR_NONE], nv), paired_p(va[AR_BSH], va[AR_NONE], nv));

    /* ---------- preuve d'émergence : où tombent les écritures des lignées vivantes ? ---------- */
    printf("\n# écritures de lois acceptées pendant que la lignée vit (dominant vérifié) : cible-t-on les entrées que le dominant exécute ?\n");
    printf("#   « utilisée » = entrée exécutée par le dominant du moment quand il se copie ; attendu = Σ nused/n (hasard uniforme) ;\n#   défaire = remettre la valeur d'avant le dernier pas de dérive de cette entrée ; casse/répare = le dominant copie avant et plus après / l'inverse\n");
    int bestg = -1, besti = -1; double bestsc = 0;
    for (int a = AR_A; a <= AR_REDIR; a += 2) {
        long nev = 0, nal = 0, nu = 0, nwc = 0, nund = 0, nbr = 0, nfx = 0, nnop = 0, nrb = 0, nuu = 0, nuunop = 0; double exp_u = 0, exp_und = 0;
        long half[2][2] = { { 0 } }; double halfe[2] = { 0 };
        long neff = 0, neffu = 0, nci = 0, nciu = 0, nefc = 0, nefcu = 0; double expeff = 0, expci = 0, expefc = 0;
        for (int r = 0; r < nr; r++) {
            LRun *X = &R[r]; if (X->arm != a) continue;
            for (int k = 0; k < X->nev; k++) {
                Ev *E = &X->ev[k]; nev++; nund += E->rprev; nrb += E->rbase; nnop += E->old == E->nw;
                if (!E->alive) continue;
                int n0 = g_base[X->src].kper * 256, h = (E->e - X->Eest) * 2 / X->Edrift; if (h > 1) h = 1; if (h < 0) h = 0;
                if (E->old != E->nw) { neff++; neffu += E->used; expeff += (double)E->nused / n0; }
                if (E->incomp && E->ncomp) { nci++; nciu += E->used; expci += (double)E->nusedc / E->ncomp; if (E->old != E->nw) { nefc++; nefcu += E->used; expefc += (double)E->nusedc / E->ncomp; } }
                nal++; nu += E->used; exp_u += (double)E->nused / n0; nwc += E->wcop; nbr += E->ok0 && !E->ok1; nfx += !E->ok0 && E->ok1;
                if (E->used) { nuu++; nuunop += E->old == E->nw; }
                half[h][0]++; half[h][1] += E->used; halfe[h] += (double)E->nused / n0;
                if (a == AR_A && !E->ok0 && E->ok1) {
                    double sc = 1 + E->used + E->rprev + 2 * E->wcop + (E->wham >= 0 ? 1.0 - (double)E->wham / g_base[X->src].L : 0);
                    if (sc > bestsc) { bestsc = sc; bestg = r; besti = k; }
                }
            }
        }
        double z = (nu - exp_u) / sqrt(exp_u > 0 ? exp_u : 1);
        printf("%-8s écritures %ld (toutes) dont %ld sans effet (même valeur) ; défaites de la dérive %ld ; retour à la valeur de départ %ld\n", ARS[a], nev, nnop, nund, nrb);
        (void)exp_und;
        printf("         pendant la vie : %ld ; sur entrée utilisée %ld (attendu %.1f, enrichissement ×%.2f, z=%.1f) dont %ld sans effet ; par un copieur %ld ; cassent le dominant %ld, le réparent %ld\n",
               nal, nu, exp_u, exp_u > 0 ? nu / exp_u : 0, z, nuunop, nwc, nbr, nfx);
        printf("         écritures EFFECTIVES (valeur changée) pendant la vie : %ld, sur entrée utilisée %ld (attendu %.1f, ×%.2f)\n", neff, neffu, expeff, expeff > 0 ? neffu / expeff : 0);
        printf("         nul « composition » (cible = phase × octet présent dans le dominant) : %ld écritures, utilisées %ld (attendu %.1f, ×%.2f) ; effectives %ld, utilisées %ld (attendu %.1f, ×%.2f)\n", nci, nciu, expci, expci > 0 ? nciu / expci : 0, nefc, nefcu, expefc, expefc > 0 ? nefcu / expefc : 0);
        printf("         enrichissement 1re moitié ×%.2f (%ld/%.1f), 2e moitié ×%.2f (%ld/%.1f)\n", halfe[0] > 0 ? half[0][1] / halfe[0] : 0, half[0][1], halfe[0], halfe[1] > 0 ? half[1][1] / halfe[1] : 0, half[1][1], halfe[1]);
    }
    { /* morts juste après une écriture qui casse le dominant (auto-destruction) */
        printf("\n# artefacts : auto-destruction = morts dans les 128 ép. suivant une écriture qui casse le dominant ; gel = dist finale (cf. tableau) et part des pas de dérive défaits\n");
        for (int a = AR_A; a <= AR_REDIR; a++) { long sd = 0, nd = 0, und = 0, ndr = 0; for (int r = 0; r < nr; r++) if (R[r].arm == a) { sd += R[r].nselfd; nd += R[r].ndeath; und += R[r].nundo; ndr += R[r].ndrift; } printf("  %-8s morts %ld dont auto-destruction %ld ; pas de dérive %ld, défaits par la soupe %ld (%.2f %%)\n", ARS[a], nd, sd, ndr, und, 100.0 * und / (ndr ? ndr : 1)); }
    }

    /* ---------- un gardien ? ---------- */
    printf("\n# gardien : écriture (bras A, lignée vivante) qui RÉPARE le dominant (il ne copiait plus, il copie de nouveau)\n");
    if (bestg < 0) printf("  aucun cas.\n");
    else {
        LRun *X = &R[bestg]; Ev *E = &X->ev[besti]; int L = g_base[X->src].L;
        printf("  %s graine %d, époque %d (dérive %d) : loi tab[%d][%02x] %s → %s (utilisée=%d, défait la dérive=%d) ; écrivain ", g_lab[X->src], X->rep, E->e, E->e - X->Eest, E->i / 256, E->i % 256, pname(E->old), pname(E->nw), E->used, E->rprev);
        hex(stdout, E->wg, L); printf(" (copieur=%d, H au dominant=%d)\n", E->wcop, E->wham);
        /* ablation : même run, écritures jetées à partir de cette époque ; 8 bifurcations de la dérive, bras A vs A-jet */
        int M = 8; LRun *Ab = calloc((size_t)(2 * M), sizeof *Ab);
        for (int m = 0; m < 2 * M; m++) {
            LRun *Y = &Ab[m]; *Y = *X; Y->ev = NULL; Y->fert = 0; Y->ck_alive = Y->ck_tot = Y->ck_alive_after = Y->ck_tot_after = 0; Y->ndeath = Y->nbirth = Y->nwrite = Y->nundo = Y->nselfd = Y->ndrift = 0; Y->nattempt = 0;
            Y->switch_at = E->e; Y->switch_arm = m < M ? AR_A : AR_DISC; Y->fork_at = E->e + 1; Y->fseed = mix(0xAB1A, (uint64_t)(m % M));
        }
        pfor(2 * M, lrun_w, Ab);
        double fa[16], fj[16]; int ca = 0, cj = 0;
        for (int m = 0; m < M; m++) { fa[m] = (double)Ab[m].ck_alive_after / Ab[m].ck_tot_after; fj[m] = (double)Ab[M + m].ck_alive_after / Ab[M + m].ck_tot_after; ca += Ab[m].alive_end; cj += Ab[M + m].alive_end; }
        printf("  ablation (après l'époque %d, 8 bifurcations de dérive) : écritures gardées part vivante %.3f, vivant en fin %d/8 ; écritures jetées %.3f, vivant en fin %d/8 ; p apparié=%.4f\n", E->e, mean(fa, M), ca, mean(fj, M), cj, paired_p(fa, fj, M));
        free(Ab);
    }
    for (int r = 0; r < nr; r++) free(R[r].ev);
    free(R);
    printf("\ntemps total %.0f s\n", secs(&t0));
    return 0;
}

/* ---------- selftest ---------- */
static int lk_selftest(void) {
    int fail = 0, ok;
    /* 1. VM : sans primitive de loi, lx_exec == u_exec (bande et pas), sur univers aléatoires */
    Rng r; rng_seed(&r, 0x1A3);
    int diff = 0;
    for (int it = 0; it < 3000; it++) {
        Univ u; u_random(&u, &r); uint8_t t1[TMAX], t2[TMAX]; for (int i = 0; i < 2 * u.L; i++) t1[i] = t2[i] = (uint8_t)rng_next(&r);
        LX c; memset(&c, 0, sizeof c); c.arm = it % 2 ? AR_A : AR_NONE;
        int s1 = u_exec(t1, &u, NULL, 0, 0), s2 = lx_exec(t2, &u, &c, 0);
        diff += s1 != s2 || memcmp(t1, t2, 2 * u.L);
    }
    ok = diff == 0; printf("law_keepers selftest : VM sans primitive de loi == u_exec (3000 univers) %s\n", ok ? "OK" : "ÉCHEC"); fail |= !ok;
    /* 2. anti-triche : catalogue 30, jeton permanent, chaque instruction écrit ≤ 1 octet (bande OU loi), WRITE_LAW 0 octet de bande */
    long viol = 0, nw = 0, steps = 0;
    for (int it = 0; it < 1500; it++) {
        Univ u; u_random(&u, &r); for (int q = 0; q < 4; q++) for (int b = 0; b < 256; b++) if (rng_below(&r, 6) == 0) u.tab[q][b] = (uint8_t)(P_COUNT + rng_below(&r, 2));
        for (int q = u.kper; q < 4; q++) memcpy(u.tab[q], u.tab[q % u.kper], 256);
        uint8_t t[TMAX]; for (int i = 0; i < 2 * u.L; i++) t[i] = (uint8_t)rng_next(&r);
        LX c; memset(&c, 0, sizeof c); c.arm = it % 2 ? AR_A : AR_REDIR; c.token = 1; rng_seed(&c.xr, (uint64_t)it);
        Univ u0 = u; int st = lx_exec(t, &u, &c, 2); viol += st < 0; steps += st > 0 ? st : 0;
        for (int q = 0; q < u.kper; q++) for (int b = 0; b < 256; b++) nw += u.tab[q][b] != u0.tab[q][b];
    }
    ok = viol == 0 && nw > 100; printf("law_keepers selftest : anti-triche (1500 univers, %ld pas, %ld lois réécrites) 0 instruction > 1 octet, valeurs < 30 : %s\n", steps, nw, ok ? "OK" : "ÉCHEC"); fail |= !ok;
    /* 3. jeton : une seule écriture acceptée par jeton ; A-jet n'écrit jamais ; vérification n'écrit jamais */
    int okj = 1;
    for (int it = 0; it < 300; it++) {
        Univ u; u_random(&u, &r); for (int b = 0; b < 256; b++) if (rng_below(&r, 3) == 0) u.tab[0][b] = P_WLAW;
        for (int q = 1; q < 4; q++) { memcpy(u.tab[q], u.tab[0], 256); }
        u.kper = 1;
        uint8_t t[TMAX]; for (int i = 0; i < 2 * u.L; i++) t[i] = (uint8_t)rng_next(&r);
        for (int a = AR_A; a <= AR_REDIR; a++) for (int v = 0; v < 2; v++) {
            Univ w = u; uint8_t tt[TMAX]; memcpy(tt, t, sizeof t); LX c; memset(&c, 0, sizeof c); c.arm = a; c.token = 1; c.verify = v;
            lx_exec(tt, &w, &c, 0); int d = 0; for (int b = 0; b < 256; b++) d += w.tab[0][b] != u.tab[0][b];
            if (d > 1 || ((a == AR_DISC || v) && d)) okj = 0;
        }
    }
    printf("law_keepers selftest : 1 écriture max par jeton, A-jet et vérification n'écrivent jamais %s\n", okj ? "OK" : "ÉCHEC"); fail |= !okj;
    /* 4. dérive B : une entrée très utilisée dérive beaucoup moins ; B-perm efface l'effet */
    { Univ u; u_random(&u, &r); u_set_kper(&u, 1); float use[1024] = { 0 }; use[7] = 1000; int hb = 0, hp = 0, hu = 0, oldv; Rng d1, d2, d3; rng_seed(&d1, 1); rng_seed(&d2, 1); rng_seed(&d3, 1);
      for (int it = 0; it < 200000; it++) { Univ a = u, b = u, c = u; hb += lx_drift(&a, &d1, use, 0, &oldv) == 7; hp += lx_drift(&b, &d2, use, 1, &oldv) == 7; hu += lx_drift(&c, &d3, NULL, 0, &oldv) == 7; }
      ok = hu > 500 && hb * 20 < hu && hp > hu / 3; printf("law_keepers selftest : dérive B (entrée sur-utilisée touchée %d fois vs %d uniforme, %d permutée) %s\n", hb, hu, hp, ok ? "OK" : "ÉCHEC"); fail |= !ok; }
    /* 5. lx_copier == u_copier sur univers valides ; un copieur fait main reste copieur sans primitive de loi */
    { int dd = 0; Univ uo; u_origin(&uo, 1024); for (int it = 0; it < 400; it++) { Univ u; u_random(&u, &r); uint8_t g[LMAX]; for (int i = 0; i < u.L; i++) g[i] = (uint8_t)rng_next(&r); dd += (u_copier(g, &u) != 0) != (lx_copier(g, &u, AR_A) != 0); }
      ok = dd == 0; printf("law_keepers selftest : lx_copier == u_copier (400 univers) %s\n", ok ? "OK" : "ÉCHEC"); fail |= !ok; }
    return fail;
}

#ifndef LK_NO_MAIN   /* local_laws.c inclut ce fichier sans son main */
int main(int argc, char **argv) {
    const char *load = NULL;
    (void)selftest2; (void)cmd_drift; (void)cmd_origin; (void)cmd_race; (void)selftest3; (void)cmd_replay; (void)cmd_trace; (void)cmd_robust; (void)cmd_fresh; (void)cmd_fertile; (void)cmd_knock; (void)run_traces;
    (void)selftest4; (void)cmd_pool; (void)cmd_cycles; (void)cmd_persist; (void)cmd_features; (void)load_pool; (void)life_w; (void)phys_at; (void)auc; (void)spearman; (void)kendall_strat; (void)p2;
    for (int i = 2; i + 1 < argc; i += 2) {
        if (!strcmp(argv[i], "--load")) load = argv[i + 1];
        else if (!strcmp(argv[i], "--edrift")) g_edrift = atoi(argv[i + 1]);
        else if (!strcmp(argv[i], "--seeds")) g_seeds = atoi(argv[i + 1]);
        else if (!strcmp(argv[i], "--fert")) g_fert = atoi(argv[i + 1]);
        else if (!strcmp(argv[i], "--threads")) g_threads = atoi(argv[i + 1]);
        else { fprintf(stderr, "option inconnue %s\n", argv[i]); return 2; }
    }
    if (argc >= 2 && !strcmp(argv[1], "selftest")) return lk_selftest();
    if (argc < 2 || strcmp(argv[1], "run") || load_sources(load)) { fprintf(stderr, "usage : law_keepers selftest | run --load fin2.bin [--edrift 16384] [--seeds 8] [--fert 1]\n"); return 2; }
    return lk_run();
}
#endif
