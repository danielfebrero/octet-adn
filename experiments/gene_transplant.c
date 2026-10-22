/* Gene-calling statistique par matrice de transplantation (lab general).
 * Généralise experiments/general-transfer (« un octet de la lignée 3 débloque la lignée 2 ») :
 * chaque motif candidat (k octets contigus, k = 1..4, pris chez un génome passant) est greffé
 * à la même position dans chaque fond génétique du panel, et l'effet est mesuré par gn_check
 * (m2/m3 sur 40 tâches tenues à l'écart). Mesure seulement : aucun opérateur, aucune course.
 *
 * Depuis la racine : make experiments/gene_transplant
 *   ./experiments/gene_transplant panel DIR S0.ck S1.ck ...   extrait le panel (lignée = rang de l'argument)
 *   ./experiments/gene_transplant matrix DIR [MATRICE.tsv]    matrice effet[motif, fond] + verdict sur stdout
 * OADN_JOBS=N fixe le nombre de processus (défaut : nombre de cœurs). Le résultat n'en dépend pas. */
#include "octet.h"
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>

#define L0       172          /* longueur de la seed general ; le panel garde cette longueur */
#define MAXB     512          /* fonds */
#define MAXC     8192         /* motifs (réels + nuls) */
#define THR      0.05         /* effet positif : Δ ≥ +0,05 sur la mémoire cible (≈ 3,5 écarts-types) */
#define ELIG     0.95         /* fond éligible pour la mémoire t si m_t < 0,95 (de la place pour gagner) */
#define NLIN     16

typedef struct { char name[32]; int lin; PoolEntry g; double v[GN_CHECK_N]; int code; } Bg;
typedef struct {
    int pos, k, t, null, ndon, donlin, twin;    /* t : 0 = m2, 1 = m3 ; twin : motif réel ↔ nul */
    uint8_t b[4]; char don[32];
} Motif;
/* résultat d'une cellule */
typedef struct { float dm, doth; int8_t st; } Cell;  /* st : 0 mesuré, 1 sauvetage, -1 porteur, -2 inéligible */

static Bg B[MAXB]; static int nb;
static Motif M[MAXC]; static int nm;
static PoolEntry SEED;

static int dist_seed(const PoolEntry *g) {
    int d = abs(g->len - SEED.len);
    for (int i = 0; i < g->len && i < SEED.len; i++) d += g->dna[i] != SEED.dna[i];
    return d;
}

/* panel : par lignée, les génomes de pool distincts (longueur 172, ≠ seed), puis K représentants
 * choisis par point le plus éloigné (Hamming) : le premier est le plus éloigné de la seed. Les
 * quasi-doublons d'un même pool gonfleraient sinon le nombre de « fonds » sans ajouter de diversité. */
#define KREP 5
static int ham(const PoolEntry *a, const PoolEntry *b) { int d = 0; for (int i = 0; i < L0; i++) d += a->dna[i] != b->dna[i]; return d; }
static int cmd_panel(int argc, char **argv) {
    static Checkpoint ck; static PoolEntry all[MAXB], cand[POOL_MAX + 1]; int nall = 0, nw = 0;
    for (int a = 3; a < argc; a++) {
        if (ck_load(argv[a], &ck)) { fprintf(stderr, "%s : checkpoint invalide\n", argv[a]); return 1; }
        int nc = 0, idx[POOL_MAX + 1];
        for (int i = 0; i <= ck.npool; i++) {
            const PoolEntry *g = i < ck.npool ? &ck.pool[i] : (ck.has_best ? &ck.best : NULL);
            if (!g || g->len != L0 || !dist_seed(g)) continue;
            int dup = 0;
            for (int j = 0; j < nall && !dup; j++) dup = !memcmp(all[j].dna, g->dna, L0);
            for (int j = 0; j < nc && !dup; j++) dup = !memcmp(cand[j].dna, g->dna, L0);
            if (!dup) { idx[nc] = i; cand[nc++] = *g; }
        }
        int pick[KREP], np = 0, used[POOL_MAX + 1] = {0};
        while (np < KREP && np < nc) {
            int best = -1, bd = -1;
            for (int j = 0; j < nc; j++) if (!used[j]) {
                int d = np ? 1 << 30 : dist_seed(&cand[j]);
                for (int q = 0; q < np; q++) { int h = ham(&cand[j], &cand[pick[q]]); if (h < d) d = h; }
                if (d > bd) { bd = d; best = j; }
            }
            used[best] = 1; pick[np++] = best;
        }
        for (int q = 0; q < np; q++) {
            const PoolEntry *g = &cand[pick[q]];
            if (nall == MAXB) break;
            all[nall++] = *g;
            char p[4096]; snprintf(p, sizeof p, "%s/s%d-p%02d.adn", argv[2], a - 3, idx[pick[q]]);
            FILE *f = fopen(p, "wb");
            if (!f || fwrite(g->dna, 1, L0, f) != L0 || fclose(f)) { perror(p); return 1; }
            nw++;
        }
        fprintf(stderr, "lignée %d : %d distincts, %d gardés\n", a - 3, nc, np);
    }
    fprintf(stderr, "panel : %d génomes écrits dans %s\n", nw, argv[2]);
    return 0;
}

static int load_panel(const char *dir) {
    struct dirent **ls; int n = scandir(dir, &ls, NULL, alphasort);
    if (n < 0) { perror(dir); return 1; }
    nb = 0;
    B[nb].lin = -1; snprintf(B[nb].name, sizeof B[nb].name, "seed"); B[nb++].g = SEED;
    for (int i = 0; i < n; i++) {
        const char *s = ls[i]->d_name; size_t l = strlen(s);
        if (l > 4 && !strcmp(s + l - 4, ".adn") && s[0] == 's' && nb < MAXB) {
            char p[4096]; snprintf(p, sizeof p, "%s/%s", dir, s);
            FILE *f = fopen(p, "rb"); Bg *b = &B[nb];
            if (!f) { perror(p); return 1; }
            memset(&b->g, 0, sizeof b->g);
            b->g.len = (int)fread(b->g.dna, 1, MAX_LEN, f); fclose(f);
            if (b->g.len != L0) { fprintf(stderr, "%s : attendu %d octets\n", p, L0); return 1; }
            b->lin = atoi(s + 1);
            if (b->lin < 0 || b->lin >= NLIN) { fprintf(stderr, "%s : lignée hors bornes\n", p); return 1; }
            snprintf(b->name, sizeof b->name, "%.*s", (int)(l - 4), s); nb++;
        }
        free(ls[i]);
    }
    free(ls);
    return 0;
}

static int carries(const Bg *b, const Motif *m) {
    for (int j = 0; j < m->k; j++) if (b->g.dna[m->pos + j] != m->b[j]) return 0;
    return 1;
}
static void add_motif(const Bg *don, int pos, int k, int t) {
    Motif m = { .pos = pos, .k = k, .t = t };
    memcpy(m.b, don->g.dna + pos, (size_t)k);
    for (int i = 0; i < nm; i++)
        if (!M[i].null && M[i].pos == pos && M[i].k == k && M[i].t == t && !memcmp(M[i].b, m.b, (size_t)k)) {
            M[i].ndon++; M[i].donlin |= 1 << don->lin; return;
        }
    if (nm + 2 > MAXC) { fprintf(stderr, "trop de motifs\n"); exit(1); }
    m.ndon = 1; m.donlin = 1 << don->lin; snprintf(m.don, sizeof m.don, "%s", don->name);
    M[nm++] = m;
}

/* parallélisme : chaque cellule est indépendante ; les processus écrivent dans une zone partagée */
static void run_par(int ntask, void (*fn)(int)) {
    const char *e = getenv("OADN_JOBS");
    int jobs = e ? atoi(e) : (int)sysconf(_SC_NPROCESSORS_ONLN);
    if (jobs < 1) jobs = 1;
    if (jobs > 64) jobs = 64;
    pid_t pid[64];
    for (int w = 0; w < jobs; w++) {
        pid[w] = fork();
        if (pid[w] < 0) { perror("fork"); exit(1); }
        if (!pid[w]) { for (int i = w; i < ntask; i += jobs) fn(i); _exit(0); }
    }
    for (int w = 0; w < jobs; w++) {
        int st; waitpid(pid[w], &st, 0);
        if (!WIFEXITED(st) || WEXITSTATUS(st)) { fprintf(stderr, "processus %d en échec\n", w); exit(1); }
    }
}
static double *bgv; static int *bgc; static Cell *C;
static void eval_bg(int i) { bgc[i] = gn_check(&B[i].g, bgv + i * GN_CHECK_N); }
static void eval_cell(int x) {
    const Motif *m = &M[x / nb]; const Bg *b = &B[x % nb]; Cell *c = &C[x];
    if (b->v[m->t] >= ELIG) { c->st = -2; return; }
    if (carries(b, m)) { c->st = -1; return; }
    PoolEntry g = b->g; double v[GN_CHECK_N];
    memcpy(g.dna + m->pos, m->b, (size_t)m->k);
    int e = gn_check(&g, v);
    c->dm = (float)(v[m->t] - b->v[m->t]); c->doth = (float)(v[1 - m->t] - b->v[1 - m->t]);
    c->st = e == 2 + m->t ? 1 : 0;
}

typedef struct { int elig, pos, neg, rescue, linpos, nlinelig, carr, posoth; double med, mean, solo, soloth, othmed, othpos; int solost, cls; } Prof;
enum { GENE, COND, INTER, HACK, SOLO, INERT, NCLS };
static const char *CLS[NCLS] = { "GÈNE PORTABLE", "portable non autonome", "deux lignées", "HACK (1 lignée)", "solo seul", "inerte" };

static int cmpd(const void *a, const void *b) { double x = *(const double *)a, y = *(const double *)b; return (x > y) - (x < y); }
static Prof profile(int i) {
    Prof p = {0}; double d[MAXB], o[MAXB], op[MAXB]; int linelig = 0, linp = 0, t = M[i].t;
    for (int j = 0; j < nb; j++) {
        const Cell *c = &C[i * nb + j];
        if (j == 0) { p.solost = c->st; p.solo = c->st >= 0 ? c->dm : 0; p.soloth = c->st >= 0 ? c->doth : 0; continue; }
        p.carr += c->st == -1;
        if (c->st < 0) continue;
        d[p.elig] = c->dm; o[p.elig] = c->doth; p.elig++; p.mean += c->dm;
        linelig |= 1 << B[j].lin;
        if (c->dm >= THR) { op[p.pos++] = c->doth; linp |= 1 << B[j].lin; p.posoth += B[j].v[1 - t] >= 0.85; }
        if (c->dm <= -THR) p.neg++;
        p.rescue += c->st == 1;
    }
    if (p.elig) {
        qsort(d, (size_t)p.elig, sizeof *d, cmpd); qsort(o, (size_t)p.elig, sizeof *o, cmpd);
        p.med = p.elig & 1 ? d[p.elig / 2] : 0.5 * (d[p.elig / 2 - 1] + d[p.elig / 2]);
        p.othmed = p.elig & 1 ? o[p.elig / 2] : 0.5 * (o[p.elig / 2 - 1] + o[p.elig / 2]);
        p.mean /= p.elig;
    }
    if (p.pos) { qsort(op, (size_t)p.pos, sizeof *op, cmpd); p.othpos = p.pos & 1 ? op[p.pos / 2] : 0.5 * (op[p.pos / 2 - 1] + op[p.pos / 2]); }
    p.linpos = __builtin_popcount(linp); p.nlinelig = __builtin_popcount(linelig);
    int solo = p.solost >= 0 && p.solo >= THR;
    p.cls = p.linpos >= 3 ? (solo && p.med > 0 ? GENE : COND)
          : p.linpos == 2 ? INTER : p.linpos == 1 ? HACK : solo ? SOLO : INERT;
    return p;
}

static void motif_str(const Motif *m, char *s, size_t cap) {
    int n = snprintf(s, cap, "%s@%02x:", m->t ? "m3" : "m2", m->pos);
    for (int j = 0; j < m->k && n < (int)cap; j++)
        n += snprintf(s + n, cap - (size_t)n, "%s%02x>%02x", j ? "," : "", SEED.dna[m->pos + j], m->b[j]);
}

static int uf[MAXC];
static int find(int x) { while (uf[x] != x) x = uf[x] = uf[uf[x]]; return x; }
static Prof P[MAXC];
static int rank_cmp(const void *a, const void *b) {
    const Prof *x = &P[*(const int *)a], *y = &P[*(const int *)b];
    if (x->linpos != y->linpos) return y->linpos - x->linpos;
    if (x->med != y->med) return x->med < y->med ? 1 : -1;
    if (x->solo != y->solo) return x->solo < y->solo ? 1 : -1;
    return *(const int *)a - *(const int *)b;
}

/* contrôle positif : le fait fondateur (general-transfer) doit être retrouvé par la même greffe */
static int positive_control(void) {
    PoolEntry g = {0}; double v0[GN_CHECK_N], v1[GN_CHECK_N];
    FILE *f = fopen("experiments/general-transfer/seed2-pool3.adn", "rb");
    if (!f) { perror("experiments/general-transfer/seed2-pool3.adn"); return 1; }
    g.len = (int)fread(g.dna, 1, MAX_LEN, f); fclose(f);
    if (g.len != L0 || g.dna[0x79] != 0x17) { fprintf(stderr, "contrôle positif : génome inattendu\n"); return 1; }
    int e0 = gn_check(&g, v0); g.dna[0x79] = 0xcf; int e1 = gn_check(&g, v1);
    printf("\n## Contrôle positif (general-transfer/seed2-pool3, greffe 0x79:17>cf)\n");
    printf("avant m2=%.3f m3=%.3f code=%d ; après m2=%.3f m3=%.3f code=%d ; Δm2=%+.3f %s\n", v0[0], v0[1], e0, v1[0], v1[1], e1,
           v1[0] - v0[0], e1 == 2 && !e0 ? "(retrouvé)" : "(NON retrouvé)");
    return 0;
}

static int cmd_matrix(int argc, char **argv) {
    if (load_panel(argv[2])) return 1;
    bgv = mmap(NULL, sizeof(double) * GN_CHECK_N * nb, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    bgc = mmap(NULL, sizeof(int) * nb, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (bgv == MAP_FAILED || bgc == MAP_FAILED) { perror("mmap"); return 1; }
    run_par(nb, eval_bg);
    for (int i = 0; i < nb; i++) { memcpy(B[i].v, bgv + i * GN_CHECK_N, sizeof B[i].v); B[i].code = bgc[i]; }

    puts("# gene-transplant-matrix : gene-calling par transplantation (lab general)");
    printf("# effet = Δ m_t (justesse tenue à l'écart, 40 tâches, gn_check) après greffe du motif à sa position\n");
    printf("# positif : Δ ≥ +%.2f ; fond éligible pour t si m_t < %.2f ; porteur (motif déjà présent) = non mesuré\n", THR, ELIG);
    printf("\n## Panel : %d fonds (seed + %d génomes de pool distincts, longueur %d)\n", nb, nb - 1, L0);
    int ndon = 0, linmask = 0;
    for (int i = 0; i < nb; i++) {
        printf("FOND %-10s lignée=%-4s dist_seed=%2d m2=%.3f m3=%.3f known=%.3f %s\n", B[i].name,
               B[i].lin < 0 ? "seed" : (char[8]){ 'g', (char)('0' + B[i].lin), 0 }, dist_seed(&B[i].g),
               B[i].v[0], B[i].v[1], B[i].v[6], B[i].code == 2 ? "PASSANT-m2" : B[i].code == 3 ? "PASSANT-m3" : "");
        if (B[i].code) ndon++;
        if (B[i].lin >= 0) linmask |= 1 << B[i].lin;
    }
    printf("passants (donneurs) : %d ; lignées du panel : %d\n", ndon, __builtin_popcount(linmask));
    if (positive_control()) return 1;

    /* candidats : chez chaque passant, octets ≠ seed (k = 1) et fenêtres k = 2..4 dont les deux bords ≠ seed */
    for (int i = 0; i < nb; i++) {
        if (!B[i].code) continue;
        int t = B[i].code - 2;
        for (int k = 1; k <= 4; k++)
            for (int p = 0; p + k <= L0; p++) {
                if (B[i].g.dna[p] == SEED.dna[p] || B[i].g.dna[p + k - 1] == SEED.dna[p + k - 1]) continue;
                add_motif(&B[i], p, k, t);
            }
    }
    int nreal = nm; Rng r; rng_seed(&r, 0x6E756C6CULL);
    /* contrôle nul : mêmes positions, mêmes octets inchangés, allèle tiré au hasard ailleurs */
    for (int i = 0; i < nreal; i++) {
        Motif m = M[i]; m.null = 1; m.twin = i; M[i].twin = nm; snprintf(m.don, sizeof m.don, "nul");
        for (int j = 0; j < m.k; j++) {
            uint8_t s = SEED.dna[m.pos + j];
            if (M[i].b[j] == s) continue;
            uint8_t x; do x = (uint8_t)rng_below(&r, 256); while (x == s || x == M[i].b[j]);
            m.b[j] = x;
        }
        M[nm++] = m;
    }
    int nk[5] = {0};
    for (int i = 0; i < nreal; i++) nk[M[i].k]++;
    printf("\n## Candidats : %d motifs réels (k=1 : %d, k=2 : %d, k=3 : %d, k=4 : %d) + %d motifs nuls appariés\n",
           nreal, nk[1], nk[2], nk[3], nk[4], nm - nreal);

    C = mmap(NULL, sizeof(Cell) * (size_t)nm * nb, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (C == MAP_FAILED) { perror("mmap"); return 1; }
    run_par(nm * nb, eval_cell);
    long meas = 0;
    for (long x = 0; x < (long)nm * nb; x++) meas += C[x].st >= 0;
    printf("cellules mesurées (gn_check) : %ld sur %d × %d\n", meas, nm, nb);

    for (int i = 0; i < nm; i++) P[i] = profile(i);
    int cnt[2][NCLS] = {{0}}, cntk[2][5] = {{0}};
    for (int i = 0; i < nm; i++) { cnt[M[i].null][P[i].cls]++; if (P[i].cls == GENE) cntk[M[i].null][M[i].k]++; }

    printf("\n## Définition opérationnelle (lignée = graine ; la seed est le fond « solo »)\n");
    printf("# GÈNE PORTABLE      : positif dans ≥ 3 lignées distinctes ET positif en solo sur la seed ET Δ médian > 0\n");
    printf("# portable non auto. : ≥ 3 lignées mais pas en solo (dépend d'un fond déjà modifié)\n");
    printf("# deux lignées       : positif dans exactement 2 lignées\n");
    printf("# HACK               : positif dans 1 seule lignée\n");
    printf("# solo seul / inerte : positif seulement sur la seed / nulle part\n");
    printf("\n## Classement : motifs réels vs nuls appariés (même position, allèle aléatoire)\n");
    printf("%-22s %6s %6s\n", "classe", "réels", "nuls");
    for (int c = 0; c < NCLS; c++) printf("%-22s %6d %6d\n", CLS[c], cnt[0][c], cnt[1][c]);
    printf("gènes portables par k (réels) : k1=%d k2=%d k3=%d k4=%d ; (nuls) : k1=%d k2=%d k3=%d k4=%d\n",
           cntk[0][1], cntk[0][2], cntk[0][3], cntk[0][4], cntk[1][1], cntk[1][2], cntk[1][3], cntk[1][4]);

    /* gènes : motif réel ; « emboîté » si un gène plus court du même t est contenu dans sa fenêtre */
    static int ord[MAXC]; int no = 0;
    for (int i = 0; i < nreal; i++) ord[no++] = i;
    qsort(ord, (size_t)no, sizeof *ord, rank_cmp);
    printf("\n## Motifs réels positifs (≥ 1 lignée), classés : lignées positives, Δ médian, solo\n");
    printf("# lin+ : lignées où positif ; linE : lignées éligibles ; fond+/élig/fond- : fonds positifs/mesurés/négatifs ;\n"
           "# port : fonds éligibles porteurs (motif déjà présent, non mesurés) ; sauv : fonds qui deviennent passants ;\n"
           "# Δméd/Δmoy : sur les fonds mesurés ; solo : Δ sur la seed ; nul:méd : Δ médian du motif nul apparié ;\n"
           "# Δautre+ : Δ médian de l'AUTRE mémoire sur les fonds positifs ; ctx+ : fonds positifs dont l'autre mémoire ≥ 0,85\n");
    printf("%-34s %-14s %4s %4s %5s %5s %4s %4s %4s %7s %7s %7s %7s %7s %5s %s\n", "motif", "classe", "lin+", "linE",
           "fond+", "élig", "fond-", "port", "sauv", "Δméd", "Δmoy", "solo", "nul:méd", "Δautre+", "ctx+", "donneur(s) / note");
    int top[3], ntop = 0, ngene_indep = 0;
    for (int o = 0; o < no; o++) {
        int i = ord[o]; const Prof *p = &P[i]; const Motif *m = &M[i];
        if (p->linpos == 0 && p->cls != SOLO) continue;
        char s[64]; motif_str(m, s, sizeof s);
        int nested = -1;
        if (p->cls == GENE)
            for (int q = 0; q < nreal && nested < 0; q++)
                if (q != i && P[q].cls == GENE && M[q].t == m->t && M[q].k < m->k && M[q].pos >= m->pos
                    && M[q].pos + M[q].k <= m->pos + m->k && !memcmp(M[q].b, m->b + (M[q].pos - m->pos), (size_t)M[q].k))
                    nested = q;
        char note[96] = "";
        if (nested >= 0) { char t2[64]; motif_str(&M[nested], t2, sizeof t2); snprintf(note, sizeof note, " ⊃ %s", t2); }
        if (p->cls == GENE && nested < 0) { ngene_indep++; if (ntop < 3) top[ntop++] = i; }
        printf("%-34s %-14.14s %4d %4d %5d %5d %4d %4d %4d %+7.3f %+7.3f %+7.3f %+7.3f %+7.3f %5d %s ×%d (%d lignées)%s\n",
               s, CLS[p->cls], p->linpos, p->nlinelig, p->pos, p->elig, p->neg, p->carr, p->rescue, p->med, p->mean,
               p->solost >= 0 ? p->solo : 0.0, P[m->twin].med, p->othpos, p->posoth, m->don, m->ndon,
               __builtin_popcount((unsigned)m->donlin), note);
    }

    /* familles : graphe biparti motif↔fond (arêtes = Δ ≥ seuil) ; motifs réels liés si même mémoire
     * et Jaccard(fonds positifs) ≥ 0,5 ; composantes connexes = familles */
    for (int i = 0; i < nm; i++) uf[i] = i;
    for (int i = 0; i < nreal; i++) if (P[i].pos)
        for (int j = i + 1; j < nreal; j++) if (P[j].pos && M[j].t == M[i].t) {
            int inter = 0, uni = 0;
            for (int b = 1; b < nb; b++) {
                int x = C[i * nb + b].st >= 0 && C[i * nb + b].dm >= THR, y = C[j * nb + b].st >= 0 && C[j * nb + b].dm >= THR;
                inter += x && y; uni += x || y;
            }
            if (uni && 2 * inter >= uni) uf[find(i)] = find(j);
        }
    printf("\n## Familles (Jaccard des fonds positifs ≥ 0,5, même mémoire ; familles d'au moins 2 motifs ou contenant un gène)\n");
    int fam = 0;
    for (int root = 0; root < nreal; root++) {
        if (find(root) != root || !P[root].pos) continue;
        int sz = 0, genes = 0, lo = L0, hi = 0, bgs[MAXB] = {0};
        for (int i = 0; i < nreal; i++) if (P[i].pos && find(i) == root) {
            sz++; genes += P[i].cls == GENE;
            if (M[i].pos < lo) lo = M[i].pos;
            if (M[i].pos + M[i].k - 1 > hi) hi = M[i].pos + M[i].k - 1;
            for (int b = 1; b < nb; b++) bgs[b] += C[i * nb + b].st >= 0 && C[i * nb + b].dm >= THR;
        }
        if (sz < 2 && !genes) continue;
        int nbg = 0, lm = 0; char lst[512] = ""; int n = 0;
        for (int b = 1; b < nb; b++) if (2 * bgs[b] >= sz) {
            nbg++; lm |= 1 << B[b].lin;
            if (n < 400) n += snprintf(lst + n, sizeof lst - (size_t)n, "%s ", B[b].name);
        }
        printf("FAMILLE %d : %s, %d motifs (%d gènes), positions %02x..%02x, cœur = %d fonds / %d lignées : %s\n",
               ++fam, M[root].t ? "m3" : "m2", sz, genes, lo, hi, nbg, __builtin_popcount((unsigned)lm), lst);
        int shown = 0;
        for (int o = 0; o < no && shown < 6; o++) {
            int i = ord[o];
            if (!P[i].pos || find(i) != root) continue;
            char s[64]; motif_str(&M[i], s, sizeof s);
            printf("    %-34s lin+=%d fond+=%d Δméd=%+.3f solo=%+.3f\n", s, P[i].linpos, P[i].pos, P[i].med, P[i].solo);
            shown++;
        }
    }

    printf("\n## Verdict\n");
    printf("gènes portables (réels) : %d, dont %d non emboîtés (sans gène plus court dans leur fenêtre) ; nuls classés gène : %d sur %d\n",
           cnt[0][GENE], ngene_indep, cnt[1][GENE], nm - nreal);
    printf("hacks (1 lignée) : %d réels / %d nuls ; inertes : %d réels / %d nuls\n", cnt[0][HACK], cnt[1][HACK], cnt[0][INERT], cnt[1][INERT]);
    /* portables (≥ 3 lignées) : locus distincts (k = 1) et antagonisme (l'autre mémoire chute sur les fonds positifs) */
    int nport = 0, nant = 0, loci[L0] = {0}, nloci = 0;
    for (int i = 0; i < nreal; i++) if (P[i].cls == GENE || P[i].cls == COND) {
        nport++; nant += P[i].othpos <= -THR;
        if (M[i].k == 1 && !loci[M[i].pos]++) nloci++;
    }
    printf("portables (≥ 3 lignées, autonomes ou non) : %d réels / %d nuls ; locus k=1 distincts : %d ; antagonistes (Δautre+ ≤ −%.2f) : %d\n",
           nport, cnt[1][GENE] + cnt[1][COND], nloci, THR, nant);
    if (!ntop) {
        printf("aucun gène portable : TOP = meilleurs motifs réels k=1, toutes classes (même classement)\n");
        for (int o = 0; o < no && ntop < 3; o++) if (M[ord[o]].k == 1) top[ntop++] = ord[o];
    }
    for (int i = 0; i < ntop; i++) {
        const Prof *p = &P[top[i]]; char s[64]; motif_str(&M[top[i]], s, sizeof s);
        printf("TOP %d : %s [%s] positif dans %d/%d lignées, %d/%d fonds (%d porteurs), Δ médian %+.3f, Δ moyen %+.3f, "
               "solo %+.3f (autre mémoire %+.3f), sauvetages %d, Δautre+ %+.3f, fonds+ à autre mémoire ≥ 0,85 : %d/%d, nul apparié Δméd %+.3f\n",
               i + 1, s, CLS[p->cls], p->linpos, p->nlinelig, p->pos, p->elig, p->carr, p->med, p->mean, p->solo, p->soloth,
               p->rescue, p->othpos, p->posoth, p->pos, P[M[top[i]].twin].med);
    }

    if (argc > 3) {
        FILE *f = fopen(argv[3], "w");
        if (!f) { perror(argv[3]); return 1; }
        fprintf(f, "motif\tnul\tclasse");
        for (int b = 0; b < nb; b++) fprintf(f, "\t%s", B[b].name);
        fputc('\n', f);
        for (int i = 0; i < nm; i++) {
            char s[64]; motif_str(&M[i], s, sizeof s);
            fprintf(f, "%s\t%d\t%s", s, M[i].null, CLS[P[i].cls]);
            for (int b = 0; b < nb; b++) {
                const Cell *c = &C[i * nb + b];
                if (c->st == -2) fputs("\tNA", f); else if (c->st == -1) fputs("\tP", f); else fprintf(f, "\t%+.4f", c->dm);
            }
            fputc('\n', f);
        }
        if (fclose(f)) { perror(argv[3]); return 1; }
    }
    return 0;
}

int main(int argc, char **argv) {
    seed_entry(&SEED, "general", 0);
    if (SEED.len != L0) { fprintf(stderr, "seed general : %d octets, attendu %d\n", SEED.len, L0); return 1; }
    if (argc >= 4 && !strcmp(argv[1], "panel")) return cmd_panel(argc, argv);
    if ((argc == 3 || argc == 4) && !strcmp(argv[1], "matrix")) return cmd_matrix(argc, argv);
    fprintf(stderr, "usage : %s panel DIR S0.ck S1.ck ... | matrix DIR [MATRICE.tsv]\n", argv[0]);
    return 2;
}
