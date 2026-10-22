/* Détecteur de vie par compression : un vivant modélise son monde, donc ses actions portent de
 * l'information sur ce qu'il perçoit — et, s'il a de la mémoire, sur ce qu'il a perçu AVANT.
 *
 * Aveugle au code : on ne lit que la trace observable d'UN individu posé seul dans un monde par
 * défaut sans labo (le fondateur ; à sa mort, le plus vieux vivant de sa lignée prend le relais). Par tick :
 *   action  A_t = (EMIT & 15 ou « rien ») × (a bougé, a mangé, a tourné, s'est divisé)  — 272 symboles
 *   percept S_t = stimulus du monde (0..15, processus de Markov : 1 chance sur 4 de changer par tick)
 *           F_t = nourriture devant lui au début de son tour (0 / 1..7 / ≥ 8)
 *           P1,P2 = les deux dernières valeurs de stimulus DIFFÉRENTES de S_t (le passé)
 * Codeur séquentiel adaptatif (mélange de Dirichlet hiérarchique, β = 0.5, contextes hachés),
 * longueurs de code en bits — aucun sur-apprentissage possible, le coût d'apprentissage est payé :
 *   L_B = L(A | A_{t-1}, A_{t-2})                    (le comportement prédit par lui-même)
 *   L_R = L(A | …, S_t, F_t)                         (+ le présent du monde)
 *   L_F = L(A | …, S_t, F_t, P1, P2)                 (+ le passé du monde)
 *   I_react = (L_B − L_R)/T,  I_mem = (L_R − L_F)/T  (bits/tick, T = horizon : mourir tôt coûte)
 * Témoin nul : la même trace codée avec les percepts d'une AUTRE réplique (monde indépendant).
 * Score MI = (I_react + I_mem) − même quantité sous le témoin nul, moyenne sur R répliques.
 * Persistance : le même score (débiaisé) sur le dernier quart de l'horizon seul (q4).
 *
 *   make experiments/mi_detector
 *   ./experiments/mi_detector controls [ctl_dir] [elf]   scores des 5 groupes de contrôle (résumé + AUC)
 *   ./experiments/mi_detector mi <génome.adn|.ck>...     score d'un génome
 *   ./experiments/mi_detector genes <génome|seed:NOM>    ablation octet par octet (octet → 0xFF intron)
 *   ./experiments/mi_detector selftest                   déterminisme + témoins analytiques */
#include "lifeprobe.h"

#define T_H    400
#define REPS   8
#define ALPHA  272
#define START  ALPHA            /* symbole « avant la naissance » */
#define BETA   0.5
#define NLEV   5

typedef struct { int len; uint16_t a[T_H]; uint8_t s[T_H], f[T_H]; } Trace;

/* ---- table de comptes hachée ---- */
#define TCAP 8192
typedef struct { uint64_t key; uint32_t tot; uint16_t cnt[ALPHA]; } Cell;
static Cell *tab; static int used[TCAP], nused;
static Cell *cell(uint64_t key) {
    uint64_t h = key * 0x9E3779B97F4A7C15ULL;
    for (int i = (int)(h >> 51) & (TCAP - 1);; i = (i + 1) & (TCAP - 1)) {
        if (tab[i].key == key && tab[i].tot) return &tab[i];
        if (!tab[i].tot) { tab[i].key = key; used[nused++] = i; return &tab[i]; }
    }
}
static void tab_reset(void) { for (int i = 0; i < nused; i++) memset(&tab[used[i]], 0, sizeof(Cell)); nused = 0; }
static uint64_t mix(uint64_t h, uint64_t v) { return (h ^ (v + 0x9E3779B97F4A7C15ULL + (h << 6) + (h >> 2))) * 0xff51afd7ed558ccdULL; }

/* Longueurs de code (bits) des trois modèles emboîtés, sur [t0, t1) ; les comptes apprennent dès t = 0.
 * ps/pf : percepts (peuvent venir d'une autre réplique, indexés modulo leur longueur). */
static void codelen(const Trace *tr, const uint8_t *ps, const uint8_t *pf, int plen, int t0, int t1, double L[3]) {
    tab_reset(); L[0] = L[1] = L[2] = 0;
    int p1 = 16, p2 = 16, last = -1;
    for (int t = 0; t < tr->len; t++) {
        int s = ps[t % plen], f = pf[t % plen];
        if (last >= 0 && s != last) { p2 = p1; p1 = last; }
        last = s;
        int a1 = t > 0 ? tr->a[t - 1] : START, a2 = t > 1 ? tr->a[t - 2] : START, a = tr->a[t];
        uint64_t k[NLEV];
        k[0] = 1; k[1] = mix(2, (uint64_t)a1); k[2] = mix(k[1], (uint64_t)a2 + 3);
        k[3] = mix(k[2], (uint64_t)(s * 4 + f) + 7); k[4] = mix(k[3], (uint64_t)(p1 * 17 + p2) + 11);
        double p = 1.0 / ALPHA;
        Cell *c[NLEV];
        for (int l = 0; l < NLEV; l++) {
            c[l] = cell(mix(k[l], (uint64_t)l));
            p = (c[l]->cnt[a] + BETA * p) / (c[l]->tot + BETA);
            if (t >= t0 && t < t1 && l >= 2) L[l - 2] += -log2(p);
        }
        for (int l = 0; l < NLEV; l++) { c[l]->cnt[a]++; c[l]->tot++; }
    }
}

/* ---- trace d'une lignée : le fondateur, puis à sa mort le plus vieux vivant (tous descendent de lui) ----
 * Instantané avant le tour de chaque créature, rangé dans ls[] (scratch des labos, inutilisé sans labo). */
static void hook(World *w, Creature *c) {
    c->ls[0] = c->steps; c->ls[1] = c->eaten; c->ls[2] = c->splits; c->ls[3] = c->d;
    c->ls[4] = food_ahead(w, c); c->ls[5] = (int)w->t;
}
static void record(const PoolEntry *g, uint64_t seed, Trace *tr) {
    Rng wr, sr; rng_seed(&wr, seed); rng_seed(&sr, seed ^ 0x5717ULL);
    World *w = lp_arena(g, 1, &wr);
    w->before_vm = hook;
    int focal = w->cs[0]->id;
    int stim = (int)rng_below(&sr, 16);
    tr->len = 0;
    for (int t = 0; t < T_H; t++) {
        if (rng_below(&sr, 4) == 0) stim = (int)rng_below(&sr, 16);
        w->stimulus = stim;
        world_step(w);
        Creature *c = NULL;
        for (int i = 0; i < w->ncs; i++) if (w->cs[i]->id == focal) { c = w->cs[i]; break; }
        if (!c)                                        /* relais : le plus vieux vivant qui a joué ce tick */
            for (int i = 0; i < w->ncs; i++)
                if (w->cs[i]->ls[5] == (int)w->t && (!c || w->cs[i]->age > c->age)) c = w->cs[i];
        if (!c) break;                                 /* lignée éteinte : la trace s'arrête */
        focal = c->id;
        int em = c->emitted < 0 ? 16 : (c->emitted & 15);
        int mv = (c->steps != c->ls[0]) | (c->eaten != c->ls[1]) << 1 | (c->d != c->ls[3]) << 2 | (c->splits != c->ls[2]) << 3;
        tr->a[t] = (uint16_t)(em * 16 + mv);
        tr->s[t] = (uint8_t)stim;
        tr->f[t] = (uint8_t)(c->ls[4] == 0 ? 0 : c->ls[4] < 8 ? 1 : 2);
        tr->len++;
    }
    world_free(w);
}

typedef struct { double mi, react, mem, null, q4, life; } MiScore;
static MiScore mi_score(const PoolEntry *g) {
    static Trace tr[REPS];
    MiScore r = { 0 };
    for (int k = 0; k < REPS; k++) record(g, 0xA11CE000ULL + (uint64_t)k, &tr[k]);
    for (int k = 0; k < REPS; k++) {
        const Trace *t = &tr[k], *o = &tr[(k + 1) % REPS];
        double L[3], N[3], Q[3];
        r.life += (double)t->len / T_H / REPS;
        if (t->len < 2) continue;
        codelen(t, t->s, t->f, t->len, 0, t->len, L);
        if (o->len >= 1) codelen(t, o->s, o->f, o->len, 0, t->len, N); else N[0] = N[1] = N[2] = 0;
        codelen(t, t->s, t->f, t->len, 3 * T_H / 4, T_H, Q);
        double QN[3] = { 0, 0, 0 };
        if (o->len >= 1) codelen(t, o->s, o->f, o->len, 3 * T_H / 4, T_H, QN);
        r.react += (L[0] - L[1]) / T_H / REPS;
        r.mem   += (L[1] - L[2]) / T_H / REPS;
        r.null  += (N[0] - N[2]) / T_H / REPS;
        r.q4    += ((Q[0] - Q[2]) - (QN[0] - QN[2])) / (T_H / 4) / REPS;
    }
    r.mi = r.react + r.mem - r.null;
    return r;
}

static int load_arg(const char *a, PoolEntry *g) {
    if (!strncmp(a, "seed:", 5)) { if (!seed_get(a + 5)) return -1; seed_entry(g, a + 5, 0); return 0; }
    return lp_load_one(a, g);
}

static void genes(const char *name, const PoolEntry *g, int top) {
    MiScore base = mi_score(g);
    printf("# gènes MI de %s (%d octets) : MI=%.4f react=%.4f mem=%.4f vie=%.2f\n", name, g->len, base.mi, base.react, base.mem, base.life);
    double *d = malloc(sizeof(double) * (size_t)g->len); int *ix = malloc(sizeof(int) * (size_t)g->len);
    for (int i = 0; i < g->len; i++) {
        PoolEntry h = *g; h.dna[i] = 0xFF;
        d[i] = g->dna[i] == 0xFF ? 0 : base.mi - mi_score(&h).mi; ix[i] = i;
    }
    for (int i = 0; i < g->len; i++) for (int j = i + 1; j < g->len; j++) if (d[ix[j]] > d[ix[i]]) { int t = ix[i]; ix[i] = ix[j]; ix[j] = t; }
    int nz = 0; for (int i = 0; i < g->len; i++) nz += d[i] > 0.01;
    printf("  octets dont l'ablation coûte > 0.01 bit/tick : %d / %d\n", nz, g->len);
    for (int i = 0; i < top && i < g->len; i++) {
        int k = ix[i]; if (d[k] <= 0) break;
        printf("  0x%03x  %02x %-10s ΔMI = -%.4f\n", k, g->dna[k], op_name(g->dna[k]), d[k]);
    }
    free(d); free(ix);
}

int main(int argc, char **argv) {
    setvbuf(stdout, NULL, _IOLBF, 0);
    tab = calloc(TCAP, sizeof(Cell));
    const char *cmd = argc > 1 ? argv[1] : "controls";
    if (!strcmp(cmd, "mi")) {
        for (int a = 2; a < argc; a++) {
            PoolEntry g; if (load_arg(argv[a], &g)) { fprintf(stderr, "%s : illisible\n", argv[a]); return 1; }
            MiScore s = mi_score(&g);
            printf("%s len=%d MI=%.4f react=%.4f mem=%.4f null=%.4f q4=%.4f vie=%.2f\n", argv[a], g.len, s.mi, s.react, s.mem, s.null, s.q4, s.life);
        }
        return 0;
    }
    if (!strcmp(cmd, "genes")) {
        for (int a = 2; a < argc; a++) { PoolEntry g; if (load_arg(argv[a], &g)) return 1; genes(argv[a], &g, 12); }
        return 0;
    }
    if (!strcmp(cmd, "selftest")) {
        PoolEntry g; seed_entry(&g, "echo", 0);
        MiScore a = mi_score(&g), b = mi_score(&g);
        int ok = !memcmp(&a, &b, sizeof a);
        PoolEntry z = { 0, 16, { 0 } };                /* 16 NOP : inerte → 0 */
        MiScore zs = mi_score(&z);
        ok &= fabs(zs.mi) < 1e-9;
        printf("selftest : déterminisme %s, génome inerte MI=%.6f → %s\n", !memcmp(&a, &b, sizeof a) ? "OK" : "KO", zs.mi, ok ? "OK" : "KO");
        return ok ? 0 : 1;
    }
    if (!strcmp(cmd, "controls")) {
        const char *ctl = argc > 2 ? argv[2] : "experiments/controls/evolved", *elf = argc > 3 ? argv[3] : "/bin/ls";
        static Probe p[1200]; static double sc[1200];
        int n = lp_controls(p, 1200, ctl, elf, 200, 0xC0FFEEULL);
        MiScore *ms = malloc(sizeof(MiScore) * (size_t)n);
        for (int i = 0; i < n; i++) { ms[i] = mi_score(&p[i].g); sc[i] = ms[i].mi; }
        printf("# mi-detector : %d sondes, T=%d ticks, %d répliques, β=%.1f, ELF=%s\n", n, T_H, REPS, BETA, elf);
        printf("\n## score MI (bits/tick, débiaisé du témoin nul)\n"); lp_report(stdout, p, sc, n, "bits/tick");
        for (int i = 0; i < n; i++) sc[i] = ms[i].mem;
        printf("\n## composante mémoire seule I_mem (passé du monde au-delà du présent)\n"); lp_report(stdout, p, sc, n, "bits/tick");
        for (int i = 0; i < n; i++) sc[i] = ms[i].q4;
        printf("\n## persistance : score MI (débiaisé) sur le dernier quart de l'horizon seul\n"); lp_report(stdout, p, sc, n, "bits/tick");
        for (int i = 0; i < n; i++) sc[i] = ms[i].life;
        printf("\n## (pour lecture) fraction de l'horizon vécue par le fondateur\n"); lp_report(stdout, p, sc, n, "fraction");
        {   /* confusion possible : vivre longtemps gonfle le score (normalisé par l'horizon). À vie égale ? */
            static Probe q[1200]; static double qs[1200]; int m = 0;
            for (int i = 0; i < n; i++) if (ms[i].life >= 0.95) { q[m] = p[i]; qs[m] = ms[i].mi; m++; }
            printf("\n## à vie égale : seulement les sondes dont la lignée vit ≥ 95 %% de l'horizon\n"); lp_report(stdout, q, qs, m, "bits/tick");
        }
        printf("\n## détail seeds + évolués\n");
        for (int i = 0; i < n; i++) if (p[i].grp == 'S' || p[i].grp == 'E')
            printf("%-28s len=%4d MI=%7.4f react=%7.4f mem=%7.4f null=%7.4f q4=%7.4f vie=%.2f\n", p[i].name, p[i].g.len,
                   ms[i].mi, ms[i].react, ms[i].mem, ms[i].null, ms[i].q4, ms[i].life);
        printf("\n## top-10 hors seeds/évolués (faux positifs potentiels)\n");
        int *ix = malloc(sizeof(int) * (size_t)n), k = 0;
        for (int i = 0; i < n; i++) if (p[i].grp != 'S' && p[i].grp != 'E') ix[k++] = i;
        for (int i = 0; i < k; i++) for (int j = i + 1; j < k; j++) if (ms[ix[j]].mi > ms[ix[i]].mi) { int t = ix[i]; ix[i] = ix[j]; ix[j] = t; }
        for (int i = 0; i < 10 && i < k; i++) printf("%-28s len=%4d MI=%7.4f react=%7.4f mem=%7.4f vie=%.2f\n", p[ix[i]].name, p[ix[i]].g.len,
                   ms[ix[i]].mi, ms[ix[i]].react, ms[ix[i]].mem, ms[ix[i]].life);
        return 0;
    }
    fprintf(stderr, "usage : mi_detector controls|mi|genes|selftest\n");
    return 2;
}
