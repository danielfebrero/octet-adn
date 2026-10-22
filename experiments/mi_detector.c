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
 * FUITE (correctif) : F_t n'est pas exogène — la nourriture devant dépend des pas et des repas passés de
 * l'agent (boucle fermée). I(A;F) peut donc refléter l'auto-corrélation motrice, pas la perception, et le
 * témoin nul (F d'une autre réplique) casse ce lien : il sous-estime la fuite. Deux mesures en plus :
 *   MI_S = [L(A|A₋₁A₋₂) − L(A|A₋₁A₋₂,S,P1,P2)] − même quantité avec le S d'une autre réplique.
 *          S est tiré par un RNG à part, jamais touché par l'agent : AUCUNE fuite motrice possible.
 *   I_F  = L(A|A₋₁A₋₂) − L(A|A₋₁A₋₂,F)  calculé sur trois F :
 *          F réel (boucle fermée), F_nul (autre réplique), F_ouvert (BOUCLE OUVERTE : la trajectoire
 *          enregistrée de l'agent — position, cap, repas — est rejouée par une marionnette aveugle dans le
 *          monde d'une autre réplique ; le F qu'elle rencontre est causé par ses actions mais n'a pas pu
 *          les causer). fuite = I_F(ouvert) − I_F(nul) ; perception de F ≈ I_F(réel) − I_F(ouvert).
 *   Fuite d'ISSUE (découverte par les témoins aveugles) : le bit « a mangé » de A_t n'est pas un choix,
 *          c'est le résultat physique d'EAT, qui réussit ssi F_t > 0. Un génome aveugle STEP-EAT a donc
 *          MI ≈ 0.33 sans aucun capteur, et la boucle ouverte ne le voit pas (elle rejoue les repas, mais le
 *          F rejoué est indépendant de leur succès). On recalcule tout sur A⁻ = A sans ce bit (MI⁻, I_F⁻) :
 *          fuite d'issue = [I_F(réel)−I_F(nul)] − [I_F⁻(réel)−I_F⁻(nul)].
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
#define ALL    0xFFFF
#define NO_EAT 0xFFFD           /* A⁻ : action sans le bit « a mangé » (issue mécanique de F, pas un choix) */
#define BETA   0.5
#define NLEV   5

typedef struct { int len; uint16_t a[T_H]; uint8_t s[T_H], f[T_H];
                 uint8_t x[T_H], y[T_H], d[T_H], ate[T_H]; int x0, y0, d0; } Trace;
static const uint8_t K_SF = 0, K_S = 1, K_F = 2;   /* jeux de percepts du codeur */

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
static void codelen_k(const Trace *tr, const uint8_t *ps, const uint8_t *pf, int plen, int t0, int t1, double L[3], uint8_t kind, uint16_t mask) {
    tab_reset(); L[0] = L[1] = L[2] = 0;
    int p1 = 16, p2 = 16, last = -1;
    for (int t = 0; t < tr->len; t++) {
        int s = kind == K_F ? 0 : ps[t % plen], f = kind == K_S ? 0 : pf[t % plen];
        if (kind != K_F && last >= 0 && s != last) { p2 = p1; p1 = last; }
        last = s;
        int a1 = t > 0 ? tr->a[t - 1] & mask : START, a2 = t > 1 ? tr->a[t - 2] & mask : START, a = tr->a[t] & mask;
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
static void codelen(const Trace *tr, const uint8_t *ps, const uint8_t *pf, int plen, int t0, int t1, double L[3]) {
    codelen_k(tr, ps, pf, plen, t0, t1, L, K_SF, 0xFFFF);
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
    tr->x0 = w->cs[0]->x; tr->y0 = w->cs[0]->y; tr->d0 = w->cs[0]->d;
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
        tr->x[t] = (uint8_t)c->x; tr->y[t] = (uint8_t)c->y; tr->d[t] = (uint8_t)(c->d & 3); tr->ate[t] = c->eaten != c->ls[1];
        tr->len++;
    }
    world_free(w);
}

/* ---- boucle ouverte : une marionnette aveugle rejoue la trajectoire de la lignée dans un autre monde ----
 * Génome = un YIELD (n'agit jamais d'elle-même) ; avant son tour, on note F (nourriture devant), puis on la
 * pose à la position/cap enregistrés et elle mange si l'agent a mangé ce tick-là (approximation : le repas
 * est compté sur la case finale du tick). Son énergie est maintenue : elle ne meurt pas. */
void world_eat(World *w, Creature *c);
static const Trace *pup_tr; static uint8_t *pup_f; static int pup_t;
static void pup_hook(World *w, Creature *c) {
    if (pup_t >= pup_tr->len) return;
    int fa = food_ahead(w, c);
    pup_f[pup_t] = (uint8_t)(fa == 0 ? 0 : fa < 8 ? 1 : 2);
    w->occ[c->y][c->x]--;
    c->x = pup_tr->x[pup_t] % w->cfg.w; c->y = pup_tr->y[pup_t] % w->cfg.h; c->d = pup_tr->d[pup_t];
    w->occ[c->y][c->x]++;
    if (pup_tr->ate[pup_t]) world_eat(w, c);
    c->e = 200;
}
static void open_loop(const Trace *tr, uint64_t seed, uint8_t *fout) {
    Rng wr; rng_seed(&wr, seed);
    PoolEntry pup = { 0, 1, { OP_YIELD } };
    World *w = lp_arena(&pup, 1, &wr);
    Creature *c = w->cs[0];
    w->occ[c->y][c->x]--; c->x = tr->x0; c->y = tr->y0; c->d = tr->d0; w->occ[c->y][c->x]++;
    w->before_vm = pup_hook; pup_tr = tr; pup_f = fout;
    for (pup_t = 0; pup_t < tr->len; pup_t++) world_step(w);
    world_free(w);
}

typedef struct { double mi, react, mem, null, q4, life;
                 double mis, mis_react, mis_mem, mis_null;      /* stimulus exogène seul */
                 double if_real, if_null, if_open, leak;
                 double mi_ne, ne_real, ne_null, ne_open, leak_ne; } MiScore;    /* idem sur A⁻ (sans « a mangé ») */
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
        /* S seul (exogène) */
        double LS[3], NS[3] = { 0, 0, 0 };
        codelen_k(t, t->s, t->f, t->len, 0, t->len, LS, K_S, ALL);
        if (o->len >= 1) codelen_k(t, o->s, o->f, o->len, 0, t->len, NS, K_S, ALL);
        r.mis_react += (LS[0] - LS[1]) / T_H / REPS;
        r.mis_mem   += (LS[1] - LS[2]) / T_H / REPS;
        r.mis_null  += (NS[0] - NS[2]) / T_H / REPS;
        /* F seul : réel, nul (autre réplique), boucle ouverte (trajectoire rejouée dans le monde d'une autre réplique) */
        static uint8_t fo[T_H];
        double LF[3], NF[3] = { 0, 0, 0 }, OF[3];   /* NF reste nul si l'autre réplique est vide */
        open_loop(t, 0xA11CE000ULL + (uint64_t)((k + 1) % REPS), fo);
        codelen_k(t, t->s, t->f, t->len, 0, t->len, LF, K_F, ALL);
        if (o->len >= 1) codelen_k(t, o->s, o->f, o->len, 0, t->len, NF, K_F, ALL);
        codelen_k(t, t->s, fo, t->len, 0, t->len, OF, K_F, ALL);
        r.if_real += (LF[0] - LF[1]) / T_H / REPS;
        r.if_null += (NF[0] - NF[1]) / T_H / REPS;
        r.if_open += (OF[0] - OF[1]) / T_H / REPS;
        /* A⁻ : mêmes mesures sans le bit « a mangé » */
        double LM[3], NM[3] = { 0, 0, 0 };
        codelen_k(t, t->s, t->f, t->len, 0, t->len, LM, K_SF, NO_EAT);
        if (o->len >= 1) codelen_k(t, o->s, o->f, o->len, 0, t->len, NM, K_SF, NO_EAT);
        r.mi_ne += ((LM[0] - LM[2]) - (NM[0] - NM[2])) / T_H / REPS;
        codelen_k(t, t->s, t->f, t->len, 0, t->len, LF, K_F, NO_EAT);
        if (o->len >= 1) codelen_k(t, o->s, o->f, o->len, 0, t->len, NF, K_F, NO_EAT); else NF[0] = NF[1] = 0;
        codelen_k(t, t->s, fo, t->len, 0, t->len, OF, K_F, NO_EAT);
        r.ne_real += (LF[0] - LF[1]) / T_H / REPS;
        r.ne_null += (NF[0] - NF[1]) / T_H / REPS;
        r.ne_open += (OF[0] - OF[1]) / T_H / REPS;
    }
    r.leak_ne = r.ne_open - r.ne_null;
    r.mi = r.react + r.mem - r.null;
    r.mis = r.mis_react + r.mis_mem - r.mis_null;
    r.leak = r.if_open - r.if_null;
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
            printf("%s len=%d MI=%.4f react=%.4f mem=%.4f null=%.4f q4=%.4f vie=%.2f | MI_S=%.4f (react %.4f mem %.4f) | I_F réel %.4f ouvert %.4f nul %.4f fuite=%.4f\n",
                   argv[a], g.len, s.mi, s.react, s.mem, s.null, s.q4, s.life, s.mis, s.mis_react, s.mis_mem, s.if_real, s.if_open, s.if_null, s.leak);
            printf("    A⁻ (sans « a mangé ») : MI⁻=%.4f I_F⁻ réel %.4f ouvert %.4f nul %.4f\n", s.mi_ne, s.ne_real, s.ne_open, s.ne_null);
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
        ok &= fabs(zs.mi) < 1e-9 && fabs(zs.mis) < 1e-9 && fabs(zs.leak) < 1e-4 && fabs(zs.mi_ne) < 1e-9;   /* F varie sous A constant : arrondi de log2 */
        printf("selftest : déterminisme %s, génome inerte MI=%.3g MI_S=%.3g fuite=%.3g → %s\n", !memcmp(&a, &b, sizeof a) ? "OK" : "KO", zs.mi, zs.mis, zs.leak, ok ? "OK" : "KO");
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
        for (int i = 0; i < n; i++) sc[i] = ms[i].mis;
        printf("\n## MI_S : stimulus exogène S seul (+ son passé P1,P2), sans F — aucune fuite motrice possible\n"); lp_report(stdout, p, sc, n, "bits/tick");
        for (int i = 0; i < n; i++) sc[i] = ms[i].mis_react;
        printf("\n## MI_S, part réactive seule I(A;S_t) (non débiaisée)\n"); lp_report(stdout, p, sc, n, "bits/tick");
        {
            static Probe q[1200]; static double qs[1200]; int m = 0;
            for (int i = 0; i < n; i++) if (ms[i].life >= 0.95) { q[m] = p[i]; qs[m] = ms[i].mis; m++; }
            printf("\n## MI_S à vie égale (lignées ≥ 95 %% de l'horizon)\n"); lp_report(stdout, q, qs, m, "bits/tick");
        }
        for (int i = 0; i < n; i++) sc[i] = ms[i].leak;
        printf("\n## fuite estimée = I_F(boucle ouverte) − I_F(témoin nul) : info motrice sur F sans perception\n"); lp_report(stdout, p, sc, n, "bits/tick");
        for (int i = 0; i < n; i++) sc[i] = ms[i].if_real - ms[i].if_null;
        printf("\n## I_F(réel) − I_F(nul) : ce que l'ancien témoin attribuait à F\n"); lp_report(stdout, p, sc, n, "bits/tick");
        for (int i = 0; i < n; i++) sc[i] = ms[i].if_real - ms[i].if_open;
        printf("\n## perception de F débarrassée de la fuite = I_F(réel) − I_F(ouvert)\n"); lp_report(stdout, p, sc, n, "bits/tick");
        for (int i = 0; i < n; i++) sc[i] = (ms[i].if_real - ms[i].if_null) - (ms[i].ne_real - ms[i].ne_null);
        printf("\n## fuite d'ISSUE : part de I_F(réel)−I_F(nul) portée par le seul bit « a mangé » (mécanique : EAT réussit ssi F>0)\n"); lp_report(stdout, p, sc, n, "bits/tick");
        for (int i = 0; i < n; i++) sc[i] = ms[i].leak_ne;
        printf("\n## fuite motrice sur A⁻ = I_F⁻(ouvert) − I_F⁻(nul)\n"); lp_report(stdout, p, sc, n, "bits/tick");
        for (int i = 0; i < n; i++) sc[i] = ms[i].ne_real - ms[i].ne_open;
        printf("\n## perception de F sur A⁻, sans fuite d'issue ni motrice = I_F⁻(réel) − I_F⁻(ouvert)\n"); lp_report(stdout, p, sc, n, "bits/tick");
        for (int i = 0; i < n; i++) sc[i] = ms[i].mi_ne;
        printf("\n## MI⁻ : score MI complet (S,F,P) sur A⁻ (sans « a mangé »), même témoin nul\n"); lp_report(stdout, p, sc, n, "bits/tick");
        {
            static Probe q[1200]; static double qs[1200]; int m = 0;
            for (int i = 0; i < n; i++) if (ms[i].life >= 0.95) { q[m] = p[i]; qs[m] = ms[i].mi_ne; m++; }
            printf("\n## MI⁻ à vie égale (lignées ≥ 95 %% de l'horizon)\n"); lp_report(stdout, q, qs, m, "bits/tick");
        }
        {   /* témoins analytiques : génomes AVEUGLES (aucun capteur) dont le mouvement crée de l'auto-corrélation */
            static const struct { const char *nm; int len; uint8_t b[12]; } bl[] = {
                { "aveugle:STEP-EAT", 3, { OP_STEP, OP_EAT, OP_YIELD } },
                { "aveugle:STEP-EAT-TURN", 6, { OP_STEP, OP_EAT, OP_INC_A, OP_TURN, OP_STEP, OP_YIELD } },
                { "aveugle:EAT-SPLIT-STEP", 5, { OP_EAT, OP_EAT, OP_SPLIT, OP_STEP, OP_YIELD } },
                { "aveugle:EAT-x3-STEP", 5, { OP_EAT, OP_YIELD, OP_EAT, OP_YIELD, OP_STEP } },
            };
            printf("\n## témoins aveugles (aucun capteur : toute info sur F est de la fuite)\n");
            for (size_t b = 0; b < sizeof bl / sizeof *bl; b++) {
                PoolEntry g = { 0, bl[b].len, { 0 } }; memcpy(g.dna, bl[b].b, (size_t)bl[b].len);
                MiScore s = mi_score(&g);
                printf("%-24s MI=%7.4f MI_S=%7.4f MI⁻=%7.4f | I_F réel−nul %7.4f fuite motrice %7.4f | A⁻ : réel−nul %7.4f motrice %7.4f vie=%.2f\n", bl[b].nm, s.mi, s.mis,
                       s.mi_ne, s.if_real - s.if_null, s.leak, s.ne_real - s.ne_null, s.leak_ne, s.life);
            }
        }
        printf("\n## détail seeds + évolués\n");
        for (int i = 0; i < n; i++) if (p[i].grp == 'S' || p[i].grp == 'E')
            printf("%-28s len=%4d MI=%7.4f react=%7.4f mem=%7.4f null=%7.4f q4=%7.4f vie=%.2f MI_S=%7.4f MI⁻=%7.4f fuite_mot=%7.4f fuite_issue=%7.4f\n", p[i].name, p[i].g.len,
                   ms[i].mi, ms[i].react, ms[i].mem, ms[i].null, ms[i].q4, ms[i].life, ms[i].mis, ms[i].mi_ne, ms[i].leak,
                   (ms[i].if_real - ms[i].if_null) - (ms[i].ne_real - ms[i].ne_null));
        printf("\n## top-10 hors seeds/évolués (faux positifs potentiels)\n");
        int *ix = malloc(sizeof(int) * (size_t)n), k = 0;
        for (int i = 0; i < n; i++) if (p[i].grp != 'S' && p[i].grp != 'E') ix[k++] = i;
        for (int i = 0; i < k; i++) for (int j = i + 1; j < k; j++) if (ms[ix[j]].mi > ms[ix[i]].mi) { int t = ix[i]; ix[i] = ix[j]; ix[j] = t; }
        for (int i = 0; i < 10 && i < k; i++) printf("%-28s len=%4d MI=%7.4f react=%7.4f mem=%7.4f vie=%.2f MI_S=%7.4f fuite=%7.4f\n", p[ix[i]].name, p[ix[i]].g.len,
                   ms[ix[i]].mi, ms[ix[i]].react, ms[ix[i]].mem, ms[ix[i]].life, ms[ix[i]].mis, ms[ix[i]].leak);
        printf("\n## top-10 MI_S hors seeds/évolués\n");
        for (int i = 0; i < k; i++) for (int j = i + 1; j < k; j++) if (ms[ix[j]].mis > ms[ix[i]].mis) { int t = ix[i]; ix[i] = ix[j]; ix[j] = t; }
        for (int i = 0; i < 10 && i < k; i++) printf("%-28s len=%4d MI_S=%7.4f MI=%7.4f fuite=%7.4f vie=%.2f\n", p[ix[i]].name, p[ix[i]].g.len,
                   ms[ix[i]].mis, ms[ix[i]].mi, ms[ix[i]].leak, ms[ix[i]].life);
        return 0;
    }
    fprintf(stderr, "usage : mi_detector controls|mi|genes|selftest\n");
    return 2;
}
