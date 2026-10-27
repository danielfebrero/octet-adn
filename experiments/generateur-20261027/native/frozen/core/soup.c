/* Soupe d'octets sans SPLIT (experiments/soup-replicator). Question : un réplicateur émerge-t-il
 * spontanément depuis du bruit quand la division n'est plus offerte ?
 *
 * Monde : N programmes de 64 octets (une bande partagée de N × 64 octets). À chaque époque, les
 * programmes sont appariés au hasard ; chaque paire (a, b) est concaténée en une bande de 128 octets
 * a‖b, exécutée depuis l'octet 0, puis recoupée en deux. C'est le seul endroit où un octet peut en
 * écrire un autre. Aucune énergie, aucune récompense, aucune sélection : un motif ne devient
 * fréquent que s'il se fait recopier.
 *
 * ISA : les opcodes 0x00..0x1F de vm.c, avec la même sémantique pour A, B, sauts, LOADI, AND,
 * ADD/SUB/XOR, LOOP (ip = 0), YIELD (arrêt). Le « génome » adressé par LOAD/STORE est la bande de
 * 128 octets (addr_b = 0 : k % 128, comme genome[k % len] ; addr_b = 1 : variante documentée
 * (B + k) % 128). Il n'y a pas de monde : SENSE, SENSE_NB/EN/STIM/HIST lisent 0, NEURAL voit des
 * capteurs nuls, TURN/STEP/EAT/ATTACK/EMIT sont sans effet, et SPLIT EST DÉSACTIVÉ (sans effet).
 * Différences assumées avec vm.c : chaque octet exécuté (introns compris) coûte un pas ; une
 * exécution s'arrête quand ip sort de la bande en avançant (les sauts restent modulo 128, comme
 * modulo len), sur YIELD, ou après le budget (8192 pas). */
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "octet.h"

static inline int s8(uint8_t b) { return b > 127 ? (int)b - 256 : (int)b; }

int soup_exec(uint8_t *t, const SoupCfg *c, uint8_t *written) {
    int budget = c->steps > 0 ? c->steps : 8192, ip = 0, steps = 0, k;
    uint8_t a = 0, b = 0;
    while (steps < budget && ip < SOUP_TAPE) {
        uint8_t op = t[ip++];
        steps++;
        if (op_has_imm(op)) { if (ip >= SOUP_TAPE) break; k = t[ip++]; } else k = 0;
        switch (op) {
        case OP_INC_A: a++; break;
        case OP_DEC_A: a--; break;
        case OP_INC_B: b++; break;
        case OP_DEC_B: b--; break;
        case OP_CLR_A: a = 0; break;
        case OP_STORE: {
            int ad = (c->addr_b ? b + k : k) % SOUP_TAPE;
            if (!c->no_store) { t[ad] = a; if (written) written[ad] = 1; }
            break;
        }
        case OP_LOAD: a = t[(c->addr_b ? b + k : k) % SOUP_TAPE]; break;
        case OP_JMP: ip = (ip + k) % SOUP_TAPE; break;
        case OP_JZ: if (a == 0) ip = (ip + k) % SOUP_TAPE; break;
        case OP_JNZ: if (a != 0) ip = (ip + k) % SOUP_TAPE; break;
        case OP_SENSE: case OP_SENSE_NB: case OP_SENSE_EN: case OP_SENSE_STIM: case OP_SENSE_HIST: a = 0; break;
        case OP_NEURAL: { int v = s8(t[(k + 4) % SOUP_TAPE]); a = (uint8_t)(v < 0 ? 0 : v); break; }  /* capteurs nuls */
        case OP_YIELD: return steps;
        case OP_MOV_BA: b = a; break;
        case OP_MOV_AB: a = b; break;
        case OP_ADD: a += b; break;
        case OP_SUB: a -= b; break;
        case OP_XOR: a ^= b; break;
        case OP_LOADI: a = (uint8_t)k; break;
        case OP_LOOP: ip = 0; break;
        case OP_AND: a &= (uint8_t)k; break;
        default: break;   /* NOP, introns, TURN, STEP, EAT, ATTACK, EMIT, SPLIT (désactivé) */
        }
    }
    return steps;
}

static int same(const uint8_t *x, const uint8_t *y) { int m = 0; for (int i = 0; i < SOUP_PROG; i++) m += x[i] == y[i]; return m; }
static void noise(uint8_t *o, int t) { Rng r; rng_seed(&r, 0x50555045ULL + (uint64_t)t); for (int i = 0; i < SOUP_PROG; i++) o[i] = (uint8_t)rng_next(&r); }
/* p dans la moitié ord (0 ou 1) face au bruit t ; child = l'autre moitié après exécution. 1 si copie fidèle. */
static int copy_once(const uint8_t *p, int ord, int t, const SoupCfg *c, uint8_t *child) {
    uint8_t tape[SOUP_TAPE];
    memcpy(tape + ord * SOUP_PROG, p, SOUP_PROG);
    noise(tape + (1 - ord) * SOUP_PROG, t);
    soup_exec(tape, c, NULL);
    if (child) memcpy(child, tape + (1 - ord) * SOUP_PROG, SOUP_PROG);
    return same(tape + (1 - ord) * SOUP_PROG, p) >= SOUP_FID;
}
static int copies(const uint8_t *p, int ord, int t0, const SoupCfg *c, uint8_t *child) {
    int ok = 0;
    for (int t = 0; t < 4; t++) {
        ok += copy_once(p, ord, t0 + t, c, t == 0 ? child : NULL);
        if (ok + (3 - t) < 3) return 0;               /* 3 sur 4 impossible */
    }
    return ok >= 3 && (!child || same(child, p) >= SOUP_FID);
}
int soup_copier(const uint8_t *p, const SoupCfg *c) {
    for (int ord = 0; ord < 2; ord++) {
        uint8_t ch[SOUP_PROG];
        if (copies(p, ord, 0, c, ch) && copies(ch, ord, 4, c, NULL)) return 1 + ord;   /* la copie copie aussi */
    }
    return 0;
}
int soup_ablate(const uint8_t *p, const SoupCfg *c, uint8_t *ess) {
    if (!soup_copier(p, c)) return -1;
    int n = 0;
    Rng r; rng_seed(&r, 0xAB1A7EULL);
    for (int i = 0; i < SOUP_PROG; i++) {
        int broke = 0;
        for (int j = 0; j < 4; j++) {
            uint8_t q[SOUP_PROG]; memcpy(q, p, SOUP_PROG);
            q[i] = (uint8_t)(p[i] + 1 + rng_below(&r, 255));   /* toujours ≠ p[i] */
            broke += !soup_copier(q, c);
        }
        ess[i] = broke >= 3;
        n += ess[i];
    }
    return n;
}

void soup_handmade(uint8_t *p, int addr_b) {
    memset(p, 0, SOUP_PROG);
    if (addr_b) {   /* B = 0 ; boucle : A = bande[B] ; bande[B + 64] = A ; B++ ; tant que B & 63 */
        static const uint8_t h[] = { 0x05, 0x15, 0x07, 0x00, 0x06, 0x40, 0x03, 0x16, 0x1F, 0x3F, 0x1B, 117, 0x0F };
        memcpy(p, h, sizeof h);
    } else {        /* sémantique d'origine : la boucle incrémente les imm de son propre LOAD / STORE */
        static const uint8_t h[] = {
            0x05, 0x06, 0x08, 0x1C, 0x40, 0x06, 0x0A,          /* CLR_A ; STORE 8 ; LOADI 64 ; STORE 10 */
            0x07, 0x00, 0x06, 0x40,                            /* 7 : LOAD src ; STORE dst */
            0x07, 0x08, 0x01, 0x06, 0x08,                      /* src++ */
            0x07, 0x0A, 0x01, 0x06, 0x0A,                      /* dst++ */
            0x07, 0x08, 0x1F, 0x3F, 0x1B, 108, 0x0F };         /* tant que src & 63 ; YIELD */
        memcpy(p, h, sizeof h);
    }
}

/* ---- mesures ---- */
static const uint8_t *g_soup;
static int cmp_prog(const void *x, const void *y) {
    int a = *(const int *)x, b = *(const int *)y;
    int d = memcmp(g_soup + (size_t)a * SOUP_PROG, g_soup + (size_t)b * SOUP_PROG, SOUP_PROG);
    return d ? d : a - b;
}
typedef struct { int idx, cnt; } Uniq;
static int cmp_uniq(const void *x, const void *y) {
    const Uniq *a = x, *b = y;
    return a->cnt != b->cnt ? b->cnt - a->cnt : a->idx - b->idx;
}
#define KM 8
#define KH (1 << 20)
static double top_kmer(const uint8_t *s, int n, uint64_t *km_out) {
    uint64_t *key = xcalloc(KH, sizeof *key); int *cnt = xcalloc(KH, sizeof *cnt);
    long tot = 0; int best = 0; uint64_t bk = 0;
    for (int p = 0; p < n; p++)
        for (int i = 0; i + KM <= SOUP_PROG; i++) {
            uint64_t k; memcpy(&k, s + (size_t)p * SOUP_PROG + i, KM);
            uint64_t h = (k * 0x9E3779B97F4A7C15ULL) >> 44;
            while (cnt[h] && key[h] != k) h = (h + 1) & (KH - 1);
            key[h] = k; cnt[h]++; tot++;
            if (cnt[h] > best || (cnt[h] == best && k < bk)) { best = cnt[h]; bk = k; }
        }
    free(key); free(cnt);
    *km_out = bk;
    return tot ? (double)best / tot : 0;
}

int soup_run(const SoupRun *r, FILE *out, SoupResult *res) {
    int n = r->nprog & ~1;
    if (n < 2) return -1;
    uint8_t *s = xmalloc((size_t)n * SOUP_PROG);
    int *perm = xmalloc(sizeof(int) * (size_t)n), *ord = xmalloc(sizeof(int) * (size_t)n);
    Uniq *u = xmalloc(sizeof(Uniq) * (size_t)n);
    Rng rng; rng_seed(&rng, r->seed ^ 0x50555045ULL);
    for (size_t i = 0; i < (size_t)n * SOUP_PROG; i++) s[i] = (uint8_t)rng_next(&rng);
    for (int k = 0; k < r->inject && k < n; k++) soup_handmade(s + (size_t)k * SOUP_PROG, r->vm.addr_b);   /* témoin positif */
    for (int i = 0; i < n; i++) perm[i] = i;
    memset(res, 0, sizeof *res); res->t_first = res->t_half = -1; res->min_ess = -1;
    long births = 0;
    double pmut = r->mut;
    if (out) fprintf(out, "# soupe N=%d seed=%llu addr=%s mut=%g steps=%d inject=%d no_store=%d\n"
                          "# epoch births_win births_tot uniq top_prog copiers copier_variants min_ess_win top8mer_frac top8mer entropy_bits\n",
                     n, (unsigned long long)r->seed, r->vm.addr_b ? "b" : "abs", pmut, r->vm.steps ? r->vm.steps : 8192, r->inject, r->vm.no_store);
    long bwin = 0;
    for (long e = 1; e <= r->epochs; e++) {
        for (int i = n - 1; i > 0; i--) { int j = (int)rng_below(&rng, (uint32_t)(i + 1)), t = perm[i]; perm[i] = perm[j]; perm[j] = t; }
        for (int i = 0; i < n; i += 2) {
            uint8_t tape[SOUP_TAPE], old[SOUP_TAPE], wr[SOUP_TAPE] = { 0 };
            uint8_t *pa = s + (size_t)perm[i] * SOUP_PROG, *pb = s + (size_t)perm[i + 1] * SOUP_PROG;
            memcpy(tape, pa, SOUP_PROG); memcpy(tape + SOUP_PROG, pb, SOUP_PROG);
            memcpy(old, tape, SOUP_TAPE);
            soup_exec(tape, &r->vm, wr);
            for (int h = 0; h < 2; h++) {   /* naissance : la moitié h devient (et n'était pas) une copie de l'autre */
                const uint8_t *dst = tape + h * SOUP_PROG, *src = old + (1 - h) * SOUP_PROG, *d0 = old + h * SOUP_PROG;
                int nw = 0; for (int j = 0; j < SOUP_PROG; j++) nw += wr[h * SOUP_PROG + j] && dst[j] != d0[j];
                if (nw >= 8 && same(dst, src) >= SOUP_FID && same(d0, src) < SOUP_FID) { births++; bwin++; }
            }
            memcpy(pa, tape, SOUP_PROG); memcpy(pb, tape + SOUP_PROG, SOUP_PROG);
        }
        if (pmut > 0) {                          /* rayonnement de fond : nombre de mutations ~ géométrique */
            double lq = log(1.0 - pmut);
            size_t tot = (size_t)n * SOUP_PROG, i = 0;
            for (;;) {
                double x = rng_double(&rng);
                i += (size_t)(log(1.0 - x) / lq);
                if (i >= tot) break;
                s[i] = (uint8_t)rng_next(&rng); i++;
            }
        }
        if (e % r->report && e != r->epochs) continue;
        /* programmes distincts, par abondance */
        for (int i = 0; i < n; i++) ord[i] = i;
        g_soup = s; qsort(ord, (size_t)n, sizeof(int), cmp_prog);
        int nu = 0;
        for (int i = 0; i < n; i++) {
            if (i && !memcmp(s + (size_t)ord[i] * SOUP_PROG, s + (size_t)ord[i - 1] * SOUP_PROG, SOUP_PROG)) u[nu - 1].cnt++;
            else { u[nu].idx = ord[i]; u[nu].cnt = 1; nu++; }
        }
        qsort(u, (size_t)nu, sizeof(Uniq), cmp_uniq);
        int copiers = 0, variants = 0, mine = -1, abl = 0;
        for (int i = 0; i < nu; i++) {
            const uint8_t *p = s + (size_t)u[i].idx * SOUP_PROG;
            if (!soup_copier(p, &r->vm)) continue;
            copiers += u[i].cnt; variants++;
            if (abl < 4) { uint8_t ess[SOUP_PROG]; int k = soup_ablate(p, &r->vm, ess); abl++; if (k >= 0 && (mine < 0 || k < mine)) mine = k; }
        }
        if (mine >= 0 && (res->min_ess < 0 || mine < res->min_ess)) res->min_ess = mine;
        if (copiers > 0 && res->t_first < 0) res->t_first = e;
        if (copiers * 2 >= n && res->t_half < 0) res->t_half = e;
        res->copiers_final = copiers;
        uint64_t km; double kf = top_kmer(s, n, &km);
        long hist[256] = { 0 }; for (size_t i = 0; i < (size_t)n * SOUP_PROG; i++) hist[s[i]]++;
        double H = 0; for (int i = 0; i < 256; i++) if (hist[i]) { double q = (double)hist[i] / ((double)n * SOUP_PROG); H -= q * log2(q); }
        if (out) {
            fprintf(out, "%ld %ld %ld %d %d %d %d %d %.5f ", e, bwin, births, nu, u[0].cnt, copiers, variants, mine, kf);
            for (int i = 0; i < KM; i++) fprintf(out, "%02x", (unsigned)((km >> (8 * i)) & 0xff));
            fprintf(out, " %.3f\n", H);
            fflush(out);
        }
        bwin = 0;
        if (e == r->epochs && out) {             /* fin : le réplicateur le plus abondant, ses octets essentiels */
            for (int i = 0; i < nu; i++) {
                const uint8_t *p = s + (size_t)u[i].idx * SOUP_PROG;
                int o = soup_copier(p, &r->vm);
                if (!o) continue;
                uint8_t ess[SOUP_PROG]; int k = soup_ablate(p, &r->vm, ess);
                fprintf(out, "# réplicateur dominant : %d copies, moitié %d, %d octets essentiels\n# octets  : ", u[i].cnt, o, k);
                for (int j = 0; j < SOUP_PROG; j++) fprintf(out, "%02x", p[j]);
                fprintf(out, "\n# essentiels : ");
                for (int j = 0; j < SOUP_PROG; j++) fprintf(out, "%s", ess[j] ? "^^" : "..");
                fprintf(out, "\n");
                char d[4096]; disasm(p, SOUP_PROG, 1, d, sizeof d);
                fprintf(out, "%s", d);
                break;
            }
        }
    }
    if (r->dump) {                           /* experiments/soup-terrarium : la soupe finale, octets bruts */
        FILE *f = fopen(r->dump, "wb");
        if (!f || fwrite(s, SOUP_PROG, (size_t)n, f) != (size_t)n) { if (f) fclose(f); free(s); free(perm); free(ord); free(u); return -1; }
        fclose(f);
    }
    res->births = births;
    res->hash = fnv1a(s, (size_t)n * SOUP_PROG, FNV_INIT);
    free(s); free(perm); free(ord); free(u);
    return 0;
}
