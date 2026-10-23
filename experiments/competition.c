/* Compétition entre lignées. Daniel ouvre la porte malgré 7/32 (bande du milieu).
 * Moteur : soup_exec / soup_copier de src/soup.c. src/, include/, seeds/ ne bougent pas.
 *
 * PHRASE (écrite avant le compte). Six réplicateurs dominants des graines qui ont envahi
 * la soupe 32 (0, 2, 13, 17, 24, 29). La graine 10 est hors jeu : un copieur, puis plus rien.
 * On les mélange à parts égales, N = 16384, adressage B, mutation 0,00024, 4096 époques.
 * Chaque copieur vérifié est attribué à la lignée fondatrice la plus proche (Hamming).
 *
 * Portes, écrites avant :
 *   monopole    : une lignée ≥ 50 % des copieurs, aucune autre ≥ 10 %
 *   coexistence : au moins deux lignées ≥ 10 %
 *   indécis     : le reste
 *
 *   ./experiments/competition selftest
 *   ./experiments/competition mix
 */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "octet.h"

#define NF 6
#define N 16384
#define EPOCHS 4096
#define REPORT 256
#define MUT 0.00024
#define MIX_SEED 0xC02E20261023ULL
#define SHARE_WIN 0.50
#define SHARE_HOLD 0.10

static const int ORIG[NF] = { 0, 2, 13, 17, 24, 29 };
static const char *HEX[NF] = {
    "3453502a4e720701039b2f5106901e423453502a4e720701039b2f5106901e423453502a4e720701039b2f5106901e423453502a4e720701039b2f5106901e42",
    "0483cd59eab0b0074078f306000ebb817cb67d7c8e8c1e508e5308bfe1261852b3640bd7fd9bef7ad9f4a104451adb39544688694d591bd79bec768e7b458100",
    "04e0cdff008800cdd20200ff05035c00031b011f01fe00427d04010302df86d8fba31903ff07fea304ffff034bff0406bf00cdfeaa0108623f002800ff01fefd",
    "0202bfbf01bf06bf0100ff3f0100f8ff03fe0107ff2cfd011e009500724f01000050698301c1fd856c83fd00007f827dbd7bffab7601697300fffb6d88f901ff",
    "c2de070094f0ec3a0bbca17725deaf06c0272fd48313ad00ccc446fdc503431ea61ec5d10452c17578e9d0255a85140f3be448bf07ece8c3d609aa3a3389a789",
    "17fcde0418c144260823b42f8e6f4407200600a4871ea5512975a34d9fcb501017fcde0418c144260823b42f8e6f4407200600a4871ea5512975a34d9fcb5010",
};

static uint8_t F[NF][SOUP_PROG];
static SoupCfg VM = { .addr_b = 1, .steps = 8192 };

static int hexv(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}
static int load_founders(void) {
    for (int k = 0; k < NF; k++) {
        const char *h = HEX[k];
        if ((int)strlen(h) != 2 * SOUP_PROG) return 0;
        for (int i = 0; i < SOUP_PROG; i++) {
            int a = hexv(h[2 * i]), b = hexv(h[2 * i + 1]);
            if (a < 0 || b < 0) return 0;
            F[k][i] = (uint8_t)((a << 4) | b);
        }
    }
    return 1;
}
static int ham(const uint8_t *a, const uint8_t *b) {
    int d = 0;
    for (int i = 0; i < SOUP_PROG; i++) d += a[i] != b[i];
    return d;
}
static int nearest(const uint8_t *p, int *dh) {
    int best = 0, d0 = ham(p, F[0]);
    for (int k = 1; k < NF; k++) {
        int d = ham(p, F[k]);
        if (d < d0) { d0 = d; best = k; }
    }
    if (dh) *dh = d0;
    return best;
}

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

static int same64(const uint8_t *a, const uint8_t *b) {
    int n = 0;
    for (int i = 0; i < SOUP_PROG; i++) n += a[i] == b[i];
    return n;
}

static int mix(int n, int epochs, int report, uint64_t seed, FILE *out) {
    n &= ~1;
    if (n < NF * 2) return 1;
    uint8_t *s = xmalloc((size_t)n * SOUP_PROG);
    int *perm = xmalloc(sizeof(int) * (size_t)n), *ord = xmalloc(sizeof(int) * (size_t)n);
    Uniq *u = xmalloc(sizeof(Uniq) * (size_t)n);
    int each = n / NF;
    int i = 0;
    for (int k = 0; k < NF; k++) {
        int m = each + (k < n % NF);
        for (int j = 0; j < m; j++, i++) memcpy(s + (size_t)i * SOUP_PROG, F[k], SOUP_PROG);
    }
    Rng rng;
    rng_seed(&rng, seed ^ 0xC02E5055ULL);
    for (int p = 0; p < n; p++) perm[p] = p;
    for (int p = n - 1; p > 0; p--) {
        int j = (int)rng_below(&rng, (uint32_t)(p + 1)), t = perm[p];
        perm[p] = perm[j]; perm[j] = t;
    }
    { uint8_t *tmp = xmalloc((size_t)n * SOUP_PROG);
      for (int p = 0; p < n; p++) memcpy(tmp + (size_t)p * SOUP_PROG, s + (size_t)perm[p] * SOUP_PROG, SOUP_PROG);
      memcpy(s, tmp, (size_t)n * SOUP_PROG); free(tmp); }
    for (int p = 0; p < n; p++) perm[p] = p;
    if (out) {
        fprintf(out, "# compétition N=%d seed=0x%llx époques=%d report=%d mut=%g\n",
                n, (unsigned long long)seed, epochs, report, MUT);
        fprintf(out, "# fondateurs");
        for (int k = 0; k < NF; k++) fprintf(out, " %d", ORIG[k]);
        fprintf(out, "\n# epoch copieurs");
        for (int k = 0; k < NF; k++) fprintf(out, "\tg%d", ORIG[k]);
        fprintf(out, "\tautre\tmin_ess\n");
    }
    long births = 0;
    int last_fam[NF];
    memset(last_fam, 0, sizeof last_fam);
    int last_cop = 0, last_ess = -1;
    for (long e = 1; e <= epochs; e++) {
        for (int p = n - 1; p > 0; p--) {
            int j = (int)rng_below(&rng, (uint32_t)(p + 1)), t = perm[p];
            perm[p] = perm[j]; perm[j] = t;
        }
        for (int p = 0; p < n; p += 2) {
            uint8_t tape[SOUP_TAPE], old[SOUP_TAPE], wr[SOUP_TAPE] = { 0 };
            uint8_t *pa = s + (size_t)perm[p] * SOUP_PROG, *pb = s + (size_t)perm[p + 1] * SOUP_PROG;
            memcpy(tape, pa, SOUP_PROG); memcpy(tape + SOUP_PROG, pb, SOUP_PROG);
            memcpy(old, tape, SOUP_TAPE);
            soup_exec(tape, &VM, wr);
            for (int h = 0; h < 2; h++) {
                const uint8_t *dst = tape + h * SOUP_PROG, *src = old + (1 - h) * SOUP_PROG, *d0 = old + h * SOUP_PROG;
                int nw = 0;
                for (int j = 0; j < SOUP_PROG; j++) nw += wr[h * SOUP_PROG + j] && dst[j] != d0[j];
                if (nw >= 8 && same64(dst, src) >= SOUP_FID && same64(d0, src) < SOUP_FID) births++;
            }
            memcpy(pa, tape, SOUP_PROG); memcpy(pb, tape + SOUP_PROG, SOUP_PROG);
        }
        if (MUT > 0) {
            double lq = log(1.0 - MUT);
            size_t tot = (size_t)n * SOUP_PROG, ix = 0;
            for (;;) {
                double x = rng_double(&rng);
                ix += (size_t)(log(1.0 - x) / lq);
                if (ix >= tot) break;
                s[ix] = (uint8_t)rng_next(&rng); ix++;
            }
        }
        if (e % report && e != epochs) continue;
        for (int p = 0; p < n; p++) ord[p] = p;
        g_soup = s; qsort(ord, (size_t)n, sizeof(int), cmp_prog);
        int nu = 0;
        for (int p = 0; p < n; p++) {
            if (p && !memcmp(s + (size_t)ord[p] * SOUP_PROG, s + (size_t)ord[p - 1] * SOUP_PROG, SOUP_PROG)) u[nu - 1].cnt++;
            else { u[nu].idx = ord[p]; u[nu].cnt = 1; nu++; }
        }
        qsort(u, (size_t)nu, sizeof(Uniq), cmp_uniq);
        int fam[NF];
        memset(fam, 0, sizeof fam);
        int copiers = 0, mine = -1, abl = 0;
        for (int p = 0; p < nu; p++) {
            const uint8_t *q = s + (size_t)u[p].idx * SOUP_PROG;
            if (!soup_copier(q, &VM)) continue;
            copiers += u[p].cnt;
            fam[nearest(q, NULL)] += u[p].cnt;
            if (abl < 4) {
                uint8_t ess[SOUP_PROG];
                int k = soup_ablate(q, &VM, ess);
                abl++;
                if (k >= 0 && (mine < 0 || k < mine)) mine = k;
            }
        }
        memcpy(last_fam, fam, sizeof fam);
        last_cop = copiers;
        last_ess = mine;
        if (out) {
            fprintf(out, "%ld %d", e, copiers);
            int tagged = 0;
            for (int k = 0; k < NF; k++) { fprintf(out, "\t%d", fam[k]); tagged += fam[k]; }
            fprintf(out, "\t%d\t%d\n", copiers - tagged, mine);
            fflush(out);
        }
        if (e == epochs && out) {
            for (int p = 0; p < nu; p++) {
                const uint8_t *q = s + (size_t)u[p].idx * SOUP_PROG;
                int o = soup_copier(q, &VM);
                if (!o) continue;
                uint8_t ess[SOUP_PROG];
                int k = soup_ablate(q, &VM, ess);
                int d, lin = nearest(q, &d);
                fprintf(out, "# dominant : %d copies, lignée g%d, ham %d, %d essentiels\n# octets  : ",
                        u[p].cnt, ORIG[lin], d, k);
                for (int j = 0; j < SOUP_PROG; j++) fprintf(out, "%02x", q[j]);
                fprintf(out, "\n");
                break;
            }
        }
    }
    const char *ver = "indécis";
    int nhold = 0, win = -1;
    if (last_cop > 0) {
        for (int k = 0; k < NF; k++) {
            double sh = (double)last_fam[k] / (double)last_cop;
            if (sh >= SHARE_HOLD) nhold++;
            if (sh >= SHARE_WIN) win = k;
        }
        if (win >= 0 && nhold == 1) ver = "monopole";
        else if (nhold >= 2) ver = "coexistence";
    }
    if (out) {
        fprintf(out, "copieurs_fin %d min_ess %d naissances %ld\n", last_cop, last_ess, births);
        for (int k = 0; k < NF; k++) {
            double sh = last_cop ? (double)last_fam[k] / (double)last_cop : 0;
            fprintf(out, "g%d\t%d\t%.3f\n", ORIG[k], last_fam[k], sh);
        }
        fprintf(out, "verdict : %s\n", ver);
    }
    printf("compétition : copieurs %d, %s\n", last_cop, ver);
    free(s); free(perm); free(ord); free(u);
    return 0;
}

static int selftest(void) {
    if (!load_founders()) { printf("selftest : hex fondateurs\n"); return 1; }
    int bad = 0;
    for (int k = 0; k < NF; k++) {
        if (!soup_copier(F[k], &VM)) { printf("fondateur g%d n'est plus copieur\n", ORIG[k]); bad++; }
    }
    for (int a = 0; a < NF; a++) for (int b = a + 1; b < NF; b++) {
        if (ham(F[a], F[b]) < 8) { printf("g%d et g%d trop proches\n", ORIG[a], ORIG[b]); bad++; }
    }
    if (bad) return 1;
    printf("selftest : 6 fondateurs copieurs, distincts OK\n");
    return 0;
}

int main(int argc, char **argv) {
    if (!load_founders()) { fprintf(stderr, "hex fondateurs\n"); return 1; }
    if (argc > 1 && !strcmp(argv[1], "selftest")) return selftest();
    if (argc > 1 && !strcmp(argv[1], "mix")) {
        const char *path = argc > 2 ? argv[2] : "experiments/competition-ligne/mix.txt";
        FILE *o = fopen(path, "w");
        if (!o) { perror(path); return 1; }
        int rc = mix(N, EPOCHS, REPORT, MIX_SEED, o);
        fclose(o);
        return rc;
    }
    fprintf(stderr, "usage : %s selftest | mix [mix.txt]\n", argv[0]);
    return 2;
}
