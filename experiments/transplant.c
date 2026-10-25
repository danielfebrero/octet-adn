/* Rencontres entre mondes : jardin commun et téléportation. Lire experiments/transplant/PROTOCOLE.md.
 *
 * Les mondes vivants ne sont jamais ouverts en écriture : leurs state.ck sont lus en lecture seule
 * (fopen "rb"), toute expérience écrit dans un répertoire neuf. Aucun processus vivant n'est touché.
 *
 * Origine d'une lignée : table par île, indexée par l'identifiant de créature. Un fondateur reçoit son
 * origine (1 = A, 2 = B) ; un né hérite de celle de son parent (champ parent = id du parent, posé par
 * world_split et copy_attempt). 3 = inconnue (ne devrait pas arriver : migration coupée dans le jardin). */
#include <stddef.h>
#include <stdint.h>
static void tp_before(void *I);
static void tp_sample(void *T);
#define TERRA_BEFORE_WORLD(I) tp_before(I)
#define TERRA_AFTER_SAMPLE(T) tp_sample(T)
#define main terrarium_original_main
#include "terrarium.c"
#undef main

static uint8_t *orig[KMAX]; static long norig[KMAX];
static FILE *orig_out;
static uint8_t *orig_slot(int k, long id) {
    if (id < 0) return NULL;
    if (id >= norig[k]) {
        long n = norig[k] ? norig[k] : 4096; while (n <= id) n *= 2;
        uint8_t *p = realloc(orig[k], (size_t)n); if (!p) exit(2);
        memset(p + norig[k], 0, (size_t)(n - norig[k])); orig[k] = p; norig[k] = n;
    }
    return &orig[k][id];
}
static void tp_before(void *ptr) {
    Island *I = ptr; World *w = I->w; int k = I->idx;
    for (int i = 0; i < w->ncs; i++) {
        Creature *c = w->cs[i]; uint8_t *s = orig_slot(k, c->id);
        if (s && !*s) { uint8_t *p = orig_slot(k, c->parent); *s = (p && *p) ? *p : 3; }
    }
}
static void count_orig(const Terra *T, long n[4]) {
    n[0] = n[1] = n[2] = n[3] = 0;
    for (int k = 0; k < T->P.K; k++) {
        World *w = T->I[k].w;
        for (int i = 0; i < w->ncs; i++) {
            Creature *c = w->cs[i]; if (c->e <= 0) continue;
            uint8_t *s = orig_slot(k, c->id); int o = s && *s ? *s : 0;
            if (!o) { uint8_t *p = orig_slot(k, c->parent); o = p && *p ? *p : 3; }
            n[o]++;
        }
    }
}
static void tp_sample(void *ptr) {
    Terra *T = ptr; if (!orig_out) return;
    long n[4]; count_orig(T, n);
    long g = 0; for (int k = 0; k < T->P.K; k++) g += T->I[k].w->ncs;
    fprintf(orig_out, "%ld\t%ld\t%ld\t%ld\t%ld\n", T->t, n[1], n[2], n[3], g);
    fflush(orig_out);
}

/* ---------- lecture seule d'un monde ---------- */
static Terra *read_world(const char *dir) {
    Terra *T = calloc(1, sizeof *T); if (!T) exit(2);
    char ck[700]; snprintf(ck, sizeof ck, "%s/state.ck", dir);
    if (!load(T, ck)) { fprintf(stderr, "%s illisible pour ce moteur\n", ck); free(T); return NULL; }
    return T;
}
static int genetics(const Params *P) { return P->repro ? 2 : 1; }   /* 1 = SPLIT (longueur libre), 2 = copie de bandes 64 o */
static int cmd_info(int argc, char **argv) {
    for (int a = 2; a < argc; a++) {
        Terra *T = read_world(argv[a]); if (!T) { printf("%s\tlisible=non\n", argv[a]); continue; }
        long pop = 0, len = 0;
        for (int k = 0; k < T->P.K; k++) { World *w = T->I[k].w; for (int i = 0; i < w->ncs; i++) { pop++; len += w->cs[i]->germ_len; } }
        const Params *P = &T->P;
        printf("%s\tlisible=oui\tt=%ld\tK=%d\t%dx%d\tpop=%ld\tlong_moy=%.1f\tgenetique=%s\trepro=%d\tforces=%d\tstress=%d\tattention=%d\tosmo=%d\tpred=%d\tmigrate=%d\texpand=%d\tregrow=%d\tmax_pop=%d\tpop0=%d\n",
               argv[a], T->t, P->K, P->w, P->h, pop, pop ? (double)len / pop : 0, genetics(P) == 1 ? "split" : "bandes64",
               P->repro, P->forces, P->stress, P->attention, P->osmo, P->pred, P->migrate, P->expand_every, P->regrow, P->max_pop, P->pop0);
        terra_free(T); free(T);
    }
    return 0;
}

/* ---------- réservoir de génomes d'un monde (lecture seule) ---------- */
typedef struct { int n; PoolEntry *g; } Pool;
static Pool pool_of(const Terra *T) {
    Pool p = { 0, NULL }; long tot = 0;
    for (int k = 0; k < T->P.K; k++) tot += T->I[k].w->ncs;
    p.g = calloc((size_t)(tot ? tot : 1), sizeof(PoolEntry)); if (!p.g) exit(2);
    for (int k = 0; k < T->P.K; k++) { World *w = T->I[k].w;
        for (int i = 0; i < w->ncs; i++) { Creature *c = w->cs[i]; if (c->e <= 0 || c->germ_len < MIN_LEN) continue;
            PoolEntry *g = &p.g[p.n++]; g->species = c->species; g->len = c->germ_len; memcpy(g->dna, c->germ, (size_t)c->germ_len); } }
    return p;
}
/* Donne à la créature c le génome g, comme à une naissance : soma = germe, état VM remis à zéro. */
static void implant(const Params *P, Creature *c, const PoolEntry *g) {
    c->len = c->germ_len = g->len; memcpy(c->dna, g->dna, (size_t)g->len); memcpy(c->germ, g->dna, (size_t)g->len);
    memset(c->dirty, 0, sizeof c->dirty); c->ip = 0; c->a = c->b = 0; memset(c->r, 0, sizeof c->r);
    c->species = P->stress ? role_mask(c->germ, g->len < SOUP_PROG ? g->len : SOUP_PROG)
               : P->forces ? (tape_has_attack(c->germ, g->len < SOUP_PROG ? g->len : SOUP_PROG) ? 1 : 0) : g->species;
    genotype(c);
}
static int open_orig(const char *dir) {
    char p[700]; snprintf(p, sizeof p, "%s/origine.tsv", dir);
    orig_out = fopen(p, "wx"); if (!orig_out) { perror(p); return 0; }
    fputs("t\torigine_A\torigine_B\tinconnue\tpop\n", orig_out); return 1;
}

/* ---------- jardin commun : un îlot neuf aux règles de l'hôte, moitié A, moitié B ---------- */
static int cmd_jardin(const char *host, const char *A, const char *B, const char *dir, long ticks, uint64_t seed, int pop0) {
    Terra *H = read_world(host), *TA = read_world(A), *TB = read_world(B); if (!H || !TA || !TB) return 2;
    if (genetics(&TA->P) != genetics(&H->P) || genetics(&TB->P) != genetics(&H->P)) { fputs("génétiques incompatibles (split / bandes64)\n", stderr); return 2; }
    Params P = H->P; Pool pa = pool_of(TA), pb = pool_of(TB);
    terra_free(H); free(H); terra_free(TA); free(TA); terra_free(TB); free(TB);
    if (!pa.n || !pb.n) { fputs("un des mondes n'a aucun vivant\n", stderr); return 2; }
    /* un seul îlot, sans migration ni expansion : l'origine reste lisible */
    P.founders = F_RANDOM;   /* fondateurs écrasés juste après : pas de fichier soupe à relire */
    P.K = 1; P.migrate = 0; P.expand_every = 0; P.seed = seed; P.pop0 = pop0 > 0 ? pop0 : (P.pop0 > 0 ? P.pop0 : 64);
    if (P.pop0 > P.max_pop) P.pop0 = P.max_pop;
    P.ck_every = P.ck_every > 0 ? P.ck_every : 2000; if (P.ck_every % P.sample) P.ck_every = P.sample * 10;
    if (mkdir(dir, 0775)) { perror(dir); return 2; }
    static Terra T; terra_new(&T, &P); snprintf(T.dir, sizeof T.dir, "%s", dir);
    Island *I = &T.I[0]; World *w = I->w; Rng pick; rng_seed(&pick, seed ^ 0x7A4D5EEDULL);
    I->nsh = 0; I->nsg = 0;
    for (int i = 0; i < w->ncs; i++) {
        Creature *c = w->cs[i]; int from = i & 1; const Pool *src = from ? &pb : &pa;
        implant(&T.P, c, &src->g[rng_below(&pick, (uint32_t)src->n)]);
        *orig_slot(0, c->id) = (uint8_t)(1 + from);
        I->sh[I->nsh++] = FH(c);
        struct SG *g = &I->sg[I->nsg++]; g->species = c->species; g->len = c->germ_len; memcpy(g->dna, c->germ, (size_t)c->germ_len);
    }
    free(pa.g); free(pb.g);
    char note[900]; snprintf(note, sizeof note, "%s/jardin.txt", dir);
    FILE *f = fopen(note, "wx"); if (!f) return 2;
    fprintf(f, "hote=%s\nA=%s\nB=%s\ngraine=%llu\npop0=%d\nticks=%ld\nK=1 migrate=0 expand=0\n", host, A, B, (unsigned long long)seed, T.P.pop0, ticks);
    fclose(f);
    if (!open_orig(dir) || !open_outputs(&T, 0)) return 2;
    tp_sample(&T);
    int rc = terra_run(&T, ticks, 1);
    if (!save(&T)) rc = 1;
    long n[4]; count_orig(&T, n);
    printf("jardin %s t=%ld A=%ld B=%ld inconnue=%ld\n", dir, T.t, n[1], n[2], n[3]);
    fclose(orig_out); orig_out = NULL; terra_free(&T);
    return rc;
}

/* ---------- téléportation : fourche d'un monde (copie mémoire), retrait ou arrivée, ou témoin ---------- */
static int cmd_teleport(const char *src, const char *from, const char *dir, const char *mode, double frac, long ticks, uint64_t seed) {
    Terra *S = read_world(src); if (!S) return 2;
    static Terra T; T = *S; free(S);               /* fourche : la mémoire chargée devient le monde de l'expérience */
    snprintf(T.dir, sizeof T.dir, "%s", dir);
    if (mkdir(dir, 0775)) { perror(dir); return 2; }
    for (int k = 0; k < T.P.K; k++) { World *w = T.I[k].w; for (int i = 0; i < w->ncs; i++) *orig_slot(k, w->cs[i]->id) = 1; }
    Rng r; rng_seed(&r, seed ^ 0x7E1E7047ULL); long touched = 0;
    if (!strcmp(mode, "retrait")) {
        for (int k = 0; k < T.P.K; k++) { World *w = T.I[k].w;
            for (int i = 0; i < w->ncs; i++) if (rng_double(&r) < frac) { w->cs[i]->e = 0; touched++; } }
    } else if (!strcmp(mode, "arrivee")) {
        Terra *F = from ? read_world(from) : NULL; if (!F) return 2;
        if (genetics(&F->P) != genetics(&T.P)) { fputs("génétiques incompatibles\n", stderr); return 2; }
        Pool pb = pool_of(F); terra_free(F); free(F);
        if (!pb.n) return 2;
        /* les arrivants remplacent une fraction des résidents, sur place : même effectif, même énergie */
        for (int k = 0; k < T.P.K; k++) { World *w = T.I[k].w;
            for (int i = 0; i < w->ncs; i++) if (rng_double(&r) < frac) {
                implant(&T.P, w->cs[i], &pb.g[rng_below(&r, (uint32_t)pb.n)]);
                *orig_slot(k, w->cs[i]->id) = 2; touched++; } }
        free(pb.g);
    } else if (strcmp(mode, "temoin")) { fputs("mode : retrait | arrivee | temoin\n", stderr); return 2; }
    char note[900]; snprintf(note, sizeof note, "%s/teleport.txt", dir);
    FILE *f = fopen(note, "wx"); if (!f) return 2;
    fprintf(f, "source=%s\nmode=%s\nfrac=%g\ndepuis=%s\ngraine=%llu\nt0=%ld\ntouches=%ld\nticks=%ld\n", src, mode, frac, from ? from : "-",
            (unsigned long long)seed, T.t, touched, ticks);
    fclose(f);
    if (!open_orig(dir) || !open_outputs(&T, 0)) return 2;
    tp_sample(&T);
    int rc = terra_run(&T, T.t + ticks, 1);
    if (!save(&T)) rc = 1;
    long n[4]; count_orig(&T, n);
    printf("teleport %s %s t=%ld A=%ld B=%ld touches=%ld\n", mode, dir, T.t, n[1], n[2], touched);
    fclose(orig_out); orig_out = NULL; terra_free(&T);
    return rc;
}

int main(int argc, char **argv) {
    if (argc >= 3 && !strcmp(argv[1], "info")) return cmd_info(argc, argv);
    if (argc == 9 && !strcmp(argv[1], "jardin")) return cmd_jardin(argv[2], argv[3], argv[4], argv[5], atol(argv[6]), strtoull(argv[7], NULL, 10), atoi(argv[8]));
    if (argc == 9 && !strcmp(argv[1], "teleport"))
        return cmd_teleport(argv[2], strcmp(argv[3], "-") ? argv[3] : NULL, argv[4], argv[5], atof(argv[6]), atol(argv[7]), strtoull(argv[8], NULL, 10));
    fprintf(stderr, "usage : transplant info MONDE...\n"
                    "        transplant jardin HOTE A B DIR_NEUF TICKS GRAINE POP0(0=hôte)\n"
                    "        transplant teleport SOURCE DEPUIS|- DIR_NEUF retrait|arrivee|temoin FRACTION TICKS GRAINE\n"
                    "Les mondes sont lus en lecture seule ; rien n'est écrit hors de DIR_NEUF.\n");
    return 2;
}
