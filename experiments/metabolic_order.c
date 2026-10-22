/* Paramètre d'ordre métabolique : un vivant capte et STOCKE de l'énergie mieux qu'un marcheur
 * aveugle de même motricité, et le maintient dans le temps.
 *
 * Aveugle au code : on ne lit que des observables (énergie, pas, virages, divisions).
 *   1. Run génome : COPIES copies du génome dans un monde cfg_default sans labo, T ticks.
 *      Biomasse B_g(t) = Σ énergie des vivants. On mesure aussi, par créature-tick, la motricité :
 *      λ_step (pas/tick), p_turn (changements de direction ×4/3), p_split (divisions / ticks où e ≥ seuil).
 *   2. Run nul, MÊME graine monde (nombres aléatoires communs) : des marcheurs dont le génome est
 *      réduit à YIELD ; à chaque tick ils tournent au hasard (p_turn), font λ_step pas, MANGENT
 *      toujours (manger à vide est gratuit), se divisent avec p_split si e ≥ seuil.
 *      → même motricité, même coût, aucune information sur le monde. Biomasse B_w(t).
 *   M = moyenne sur t ∈ (T/2, T] de (B_g − B_w) / (COPIES · E0), moyenne sur R répliques.
 *   Tenue S = fraction des ticks de la 2e moitié où B_g > B_w (soutenu, pas un pic).
 *   M_glouton : même chose contre un marcheur GLOUTON (mange après chaque pas, vide sa case) : témoin dur.
 *   M > 0 : le génome fait mieux que le hasard matché. Génome mort ou inerte : M = 0 (ou < 0).
 * Gene-calling : chaque octet → 0xFF (intron sûr), ΔM.
 * Le cœur n'est pas touché (hook before_vm + scratch ls[] hors labo) : labos historiques identiques. */
#include "lifeprobe.h"

void world_move(World *w, Creature *c);
void world_eat(World *w, Creature *c);
void world_split(World *w, Creature *c);

#define T_H 800
#define REPS 6
#define COPIES 4

/* ---- hooks ---- */
static double m_steps, m_turns, m_ticks, m_splits, m_splitok;   /* motricité observée (run génome) */
static double w_lambda, w_pturn, w_psplit; static Rng w_rng; static int w_greedy;     /* paramètres du marcheur */

static void hook_obs(World *w, Creature *c) {
    (void)w;
    if (c->ls[5]) {                       /* tick précédent observé : compter les deltas */
        m_ticks += 1;
        m_steps += c->steps - c->ls[0];
        m_turns += c->d != c->ls[1];
        m_splits += c->splits - c->ls[2];
        m_splitok += c->ls[3];
    }
    c->ls[0] = c->steps; c->ls[1] = c->d; c->ls[2] = c->splits;
    c->ls[3] = c->e >= w->cfg.split_energy; c->ls[5] = 1;
}
static void hook_walk(World *w, Creature *c) {
    c->len = 1; c->dna[0] = OP_YIELD; c->ip = 0;        /* il ne pense pas : il dépense juste comme l'autre */
    if (rng_double(&w_rng) < w_pturn) c->d = (int)rng_below(&w_rng, 4);
    int k = (int)w_lambda; if (rng_double(&w_rng) < w_lambda - k) k++;
    for (int i = 0; i < k && c->e > 0; i++) { world_move(w, c); if (w_greedy) world_eat(w, c); }
    world_eat(w, c);
    if (w_greedy) { world_eat(w, c); world_eat(w, c); }   /* glouton : vide la case (≤ 3 bouchées de 6) */
    if (c->e >= w->cfg.split_energy && rng_double(&w_rng) < w_psplit) world_split(w, c);
}

static double biomass(const World *w) { double s = 0; for (int i = 0; i < w->ncs; i++) s += w->cs[i]->e > 0 ? w->cs[i]->e : 0; return s; }

typedef struct { double m, hold, bg, bw, surv, lam, pturn, m2, bw2; } MoScore;

static MoScore mo_score(const PoolEntry *g) {
    MoScore r = { 0 };
    static double bg[T_H + 1];
    double E0 = cfg_default().start_energy * (double)COPIES;
    for (int k = 0; k < REPS; k++) {
        uint64_t seed = 0x3E7AB000ULL + (uint64_t)k;
        Rng wr; rng_seed(&wr, seed);
        World *w = lp_arena(g, COPIES, &wr);
        w->before_vm = hook_obs;
        m_steps = m_turns = m_ticks = m_splits = m_splitok = 0;
        int alive_end = 0;
        for (int t = 1; t <= T_H; t++) { world_step(w); bg[t] = biomass(w); }
        alive_end = w->ncs > 0;
        world_free(w);
        w_lambda = m_ticks > 0 ? m_steps / m_ticks : 0;
        w_pturn  = m_ticks > 0 ? fmin(1.0, m_turns / m_ticks * 4.0 / 3.0) : 0;
        w_psplit = m_splitok > 0 ? fmin(1.0, m_splits / m_splitok) : 0;
        double h = T_H / 2.0;
        for (w_greedy = 0; w_greedy < 2; w_greedy++) {
            rng_seed(&wr, seed); rng_seed(&w_rng, seed ^ 0x9A1CULL);
            w = lp_arena(g, COPIES, &wr);
            w->before_vm = hook_walk;
            double acc = 0, hold = 0, sg = 0, sw = 0;
            for (int t = 1; t <= T_H; t++) {
                world_step(w);
                if (t > T_H / 2) {
                    double b = biomass(w);
                    acc += (bg[t] - b) / E0; hold += bg[t] > b; sg += bg[t] / E0; sw += b / E0;
                }
            }
            world_free(w);
            if (!w_greedy) { r.m += acc / h / REPS; r.hold += hold / h / REPS; r.bg += sg / h / REPS; r.bw += sw / h / REPS; }
            else { r.m2 += acc / h / REPS; r.bw2 += sw / h / REPS; }
        }
        r.surv += (double)alive_end / REPS; r.lam += w_lambda / REPS; r.pturn += w_pturn / REPS;
    }
    return r;
}

static int load_arg(const char *a, PoolEntry *g) {
    if (!strncmp(a, "seed:", 5)) { if (!seed_get(a + 5)) return -1; seed_entry(g, a + 5, 0); return 0; }
    return lp_load_one(a, g);
}
static void pr(const char *name, int len, const MoScore *s) {
    printf("%-28s len=%4d M=%8.4f M_glouton=%8.4f tenue=%.2f B_g=%7.4f B_w=%7.4f B_wg=%7.4f survie=%.2f λ_step=%.2f p_turn=%.2f\n",
           name, len, s->m, s->m2, s->hold, s->bg, s->bw, s->bw2, s->surv, s->lam, s->pturn);
}

static void genes(const char *name, const PoolEntry *g, int top) {
    MoScore base = mo_score(g);
    printf("# gènes métaboliques de %s (%d octets) : M=%.4f tenue=%.2f\n", name, g->len, base.m, base.hold);
    double *d = malloc(sizeof(double) * (size_t)g->len); int *ix = malloc(sizeof(int) * (size_t)g->len), cnt = 0;
    for (int k = 0; k < g->len; k++) {
        PoolEntry v = *g; v.dna[k] = 0xFF;
        MoScore s = mo_score(&v);
        d[k] = base.m - s.m; ix[k] = k; cnt += d[k] > 0.05;
    }
    for (int i = 0; i < g->len; i++) for (int j = i + 1; j < g->len; j++) if (d[ix[j]] > d[ix[i]]) { int t = ix[i]; ix[i] = ix[j]; ix[j] = t; }
    printf("  octets dont l'ablation coûte > 0.05 : %d / %d\n", cnt, g->len);
    for (int i = 0; i < top && i < g->len; i++) {
        int k = ix[i];
        printf("  0x%03x  %02x %-10s ΔM = %+.4f\n", k, g->dna[k], op_name(g->dna[k]), -d[k]);
    }
    free(d); free(ix);
}

int main(int argc, char **argv) {
    setvbuf(stdout, NULL, _IOLBF, 0);
    const char *cmd = argc > 1 ? argv[1] : "controls";
    if (!strcmp(cmd, "mo")) {
        for (int a = 2; a < argc; a++) {
            PoolEntry g; if (load_arg(argv[a], &g)) { fprintf(stderr, "%s : illisible\n", argv[a]); return 1; }
            MoScore s = mo_score(&g); pr(argv[a], g.len, &s);
        }
        return 0;
    }
    if (!strcmp(cmd, "genes")) {
        for (int a = 2; a < argc; a++) { PoolEntry g; if (load_arg(argv[a], &g)) return 1; genes(argv[a], &g, 12); }
        return 0;
    }
    if (!strcmp(cmd, "selftest")) {
        PoolEntry g; seed_entry(&g, "forager", 0);
        MoScore a = mo_score(&g), b = mo_score(&g);
        int det = !memcmp(&a, &b, sizeof a);
        PoolEntry z = { 0, 16, { 0 } };                /* 16 NOP : meurt comme son marcheur immobile → 0 */
        MoScore zs = mo_score(&z);
        int ok = det && fabs(zs.m) < 1e-9;
        printf("selftest : déterminisme %s, génome inerte M=%.6f → %s\n", det ? "OK" : "KO", zs.m, ok ? "OK" : "KO");
        return ok ? 0 : 1;
    }
    if (!strcmp(cmd, "controls")) {
        const char *ctl = argc > 2 ? argv[2] : "experiments/controls/evolved", *elf = argc > 3 ? argv[3] : "/bin/ls";
        static Probe p[1200], q[1200]; static double sc[1200], qs[1200];
        int n = lp_controls(p, 1200, ctl, elf, 200, 0xC0FFEEULL);
        MoScore *ms = malloc(sizeof(MoScore) * (size_t)n);
        for (int i = 0; i < n; i++) { ms[i] = mo_score(&p[i].g); sc[i] = ms[i].m; }
        printf("# metabolic-order : %d sondes, T=%d ticks, %d copies, %d répliques, ELF=%s\n", n, T_H, COPIES, REPS, elf);
        printf("\n## M = biomasse génome − biomasse marcheur matché, 2e moitié, en unités d'énergie initiale\n"); lp_report(stdout, p, sc, n, "E0");
        for (int i = 0; i < n; i++) sc[i] = ms[i].m2;
        printf("\n## M_glouton : même chose contre un marcheur glouton (mange après chaque pas et vide sa case) — témoin plus dur\n"); lp_report(stdout, p, sc, n, "E0");
        for (int i = 0; i < n; i++) sc[i] = ms[i].surv;
        printf("\n## (confusion) survie seule : fraction des répliques où la lignée vit à T\n"); lp_report(stdout, p, sc, n, "fraction");
        for (int i = 0; i < n; i++) sc[i] = ms[i].hold;
        printf("\n## tenue : fraction des ticks de la 2e moitié où B_g > B_w\n"); lp_report(stdout, p, sc, n, "fraction");
        for (int i = 0; i < n; i++) sc[i] = ms[i].bg;
        printf("\n## (pour lecture) biomasse brute du génome B_g, sans soustraire le marcheur\n"); lp_report(stdout, p, sc, n, "E0");
        for (int i = 0; i < n; i++) sc[i] = ms[i].bw;
        printf("\n## (pour lecture) biomasse du marcheur matché B_w\n"); lp_report(stdout, p, sc, n, "E0");
        int m = 0;
        for (int i = 0; i < n; i++) if (ms[i].surv >= 0.99) { q[m] = p[i]; qs[m] = ms[i].m; m++; }
        printf("\n## à survie égale : seulement les sondes dont la lignée vit jusqu'au bout dans toutes les répliques\n"); lp_report(stdout, q, qs, m, "E0");
        printf("\n## détail seeds + évolués\n");
        for (int i = 0; i < n; i++) if (p[i].grp == 'S' || p[i].grp == 'E') pr(p[i].name, p[i].g.len, &ms[i]);
        printf("\n## top-10 hors seeds/évolués (faux positifs potentiels)\n");
        int *ix = malloc(sizeof(int) * (size_t)n), k = 0;
        for (int i = 0; i < n; i++) if (p[i].grp != 'S' && p[i].grp != 'E') ix[k++] = i;
        for (int i = 0; i < k; i++) for (int j = i + 1; j < k; j++) if (ms[ix[j]].m > ms[ix[i]].m) { int t = ix[i]; ix[i] = ix[j]; ix[j] = t; }
        for (int i = 0; i < 10 && i < k; i++) pr(p[ix[i]].name, p[ix[i]].g.len, &ms[ix[i]]);
        return 0;
    }
    fprintf(stderr, "usage : metabolic_order controls|mo|genes|selftest\n");
    return 2;
}
