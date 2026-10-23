/* Univers à lois locales figées, sans génome et sans ISA.
 * La barre est dans experiments/univers-innovant/README.md, avant les chiffres.
 * src/, include/ et seeds/ ne bougent pas. Ce fichier ne les inclut pas.
 *
 * Huit familles, 32 tirages, 8 graines de bruit. Une loi passe si au moins
 * 4 graines partagent le même motif canonique. L'approche passe à 4 lois.
 */
#include <stdio.h>
#include <string.h>
#include <stdint.h>

#define L 64
#define KINDS 8
#define DRAWS 32
#define SEEDS 8
#define NEED_SEEDS 4
#define NEED_LAWS 4
#define BURN 128
#define TAU_MAX 16
#define DIST_MIN 3
#define DIST_MAX 16
#define P_MIN 2
#define P_MAX (L / 2)

static uint64_t xs;

static uint32_t rnd(void) {
    xs ^= xs << 7;
    xs ^= xs >> 9;
    xs ^= xs << 8;
    return (uint32_t)xs;
}

static uint8_t med3(uint8_t a, uint8_t b, uint8_t c) {
    if (a > b) { uint8_t t = a; a = b; b = t; }
    if (b > c) { uint8_t t = b; b = c; c = t; }
    if (a > b) { uint8_t t = a; a = b; b = t; }
    return b;
}

/* kind 1..8, p[4] consommé dans l'ordre du tirage. */
static uint8_t cell(int kind, const uint8_t p[4], uint8_t left, uint8_t s, uint8_t right) {
    switch (kind) {
    case 1:
        return (uint8_t)((unsigned)p[0] * left + (unsigned)p[1] * s
                         + (unsigned)p[2] * right + p[3]);
    case 2:
        return (uint8_t)(med3(left, s, right) ^ p[0]);
    case 3:
        if ((unsigned)left + s + right > p[0]) return (uint8_t)(s + p[1]);
        return (uint8_t)(s - p[2]);
    case 4: {
        int m = 3 + (p[0] % 14);
        int v = s % m, n = (v + 1) % m;
        int lv = left % m, rv = right % m;
        return (uint8_t)((lv == n || rv == n) ? n : v);
    }
    case 5: {
        int m = 3 + (p[0] % 6);
        int v = s % m, lv = left % m, rv = right % m;
        if (v == 0) return (uint8_t)((lv == 1 || rv == 1) ? 1 : 0);
        return (uint8_t)((v + 1) % m);
    }
    case 6:
        return (uint8_t)((s & p[0]) | ((left ^ right ^ p[2]) & p[1]));
    case 7:
        if (left == right) return left;
        return (uint8_t)(s + p[0]);
    case 8:
        return (uint8_t)((int)left + (int)right - (int)s + (int)p[0]);
    default:
        return s;
    }
}

static void step(int kind, const uint8_t p[4], const uint8_t *cur, uint8_t *nxt) {
    for (int i = 0; i < L; i++) {
        uint8_t left = cur[(i + L - 1) % L];
        uint8_t right = cur[(i + 1) % L];
        nxt[i] = cell(kind, p, left, cur[i], right);
    }
}

static int ndistinct(const uint8_t *t) {
    int seen[256] = {0}, n = 0;
    for (int i = 0; i < L; i++) n += !seen[t[i]]++;
    return n;
}

static int majority(const uint8_t *t) {
    int cnt[256] = {0}, m = 0;
    for (int i = 0; i < L; i++) {
        int c = ++cnt[t[i]];
        if (c > m) m = c;
    }
    return m;
}

/* Plus petite période temporelle dans 1..TAU_MAX, après BURN pas. 0 sinon. */
static int settle(int kind, const uint8_t p[4], uint8_t *t) {
    uint8_t nxt[L], hist[TAU_MAX][L];
    for (int s = 0; s < BURN; s++) {
        step(kind, p, t, nxt);
        memcpy(t, nxt, L);
    }
    for (int tau = 1; tau <= TAU_MAX; tau++) {
        memcpy(hist[tau - 1], t, L);
        step(kind, p, t, nxt);
        memcpy(t, nxt, L);
        if (memcmp(t, hist[0], L) == 0) return tau;
    }
    return 0;
}

static int spatial(const uint8_t *t, int *cover_out) {
    for (int d = P_MIN; d <= P_MAX; d++) {
        int same = 0;
        for (int i = 0; i < L; i++) same += t[i] == t[(i + d) % L];
        if (same * 2 < L) continue;
        uint8_t mot[P_MAX];
        int md = 0;
        for (int ph = 0; ph < d; ph++) {
            int cnt[256] = {0}, best = 0, bv = 0, nph = 0;
            for (int i = ph; i < L; i += d) {
                int c = ++cnt[t[i]];
                nph++;
                if (c > best) { best = c; bv = t[i]; }
            }
            (void)nph;
            mot[ph] = (uint8_t)bv;
            if (cnt[bv] > md) md = cnt[bv];
        }
        int seen[256] = {0}, nd = 0, cover = 0;
        for (int ph = 0; ph < d; ph++) nd += !seen[mot[ph]]++;
        if (nd < DIST_MIN) continue;
        for (int i = 0; i < L; i++) cover += t[i] == mot[i % d];
        if (cover * 2 < L) continue;
        *cover_out = cover;
        return d;
    }
    return 0;
}

static void motif_of(const uint8_t *t, int P, uint8_t *mot) {
    for (int ph = 0; ph < P; ph++) {
        int cnt[256] = {0}, best = 0, bv = 0;
        for (int i = ph; i < L; i += P) {
            int c = ++cnt[t[i]];
            if (c > best) { best = c; bv = t[i]; }
        }
        mot[ph] = (uint8_t)bv;
    }
}

static void canon(const uint8_t *mot, int P, char *out) {
    char best[P_MAX * 2 + 1];
    int have = 0;
    best[0] = 0;
    for (int rev = 0; rev < 2; rev++) {
        uint8_t seq[P_MAX];
        for (int i = 0; i < P; i++) seq[i] = rev ? mot[P - 1 - i] : mot[i];
        for (int rot = 0; rot < P; rot++) {
            char cur[P_MAX * 2 + 1];
            int k = 0;
            for (int i = 0; i < P; i++)
                k += sprintf(cur + k, "%02x", seq[(rot + i) % P]);
            if (!have || strcmp(cur, best) < 0) {
                memcpy(best, cur, (size_t)k + 1);
                have = 1;
            }
        }
    }
    memcpy(out, best, strlen(best) + 1);
}

typedef struct {
    int ok;
    int tau, P, dist, maj, cover;
    char key[16 + P_MAX * 2];
} Hit;

static Hit eval_state(int kind, const uint8_t p[4], uint8_t *t) {
    Hit h;
    memset(&h, 0, sizeof h);
    h.tau = settle(kind, p, t);
    if (!h.tau) return h;
    h.dist = ndistinct(t);
    if (h.dist < DIST_MIN || h.dist > DIST_MAX) return h;
    h.maj = majority(t);
    if (h.maj * 4 > L * 3) return h;
    h.P = spatial(t, &h.cover);
    if (!h.P) return h;
    uint8_t mot[P_MAX];
    char hex[P_MAX * 2 + 1];
    motif_of(t, h.P, mot);
    canon(mot, h.P, hex);
    sprintf(h.key, "%d:%d:%s", h.P, h.tau, hex);
    h.ok = 1;
    return h;
}

static void fill_noise(uint8_t *t, uint64_t seed) {
    xs = seed;
    for (int i = 0; i < L; i++) t[i] = (uint8_t)rnd();
}

static int crystal_key(int kind, const uint8_t p[4], char *key) {
    uint8_t t[L];
    memset(t, 0x11, L);
    Hit h = eval_state(kind, p, t);
    if (!h.ok) {
        key[0] = 0;
        return 0;
    }
    memcpy(key, h.key, sizeof h.key);
    return 1;
}

static const char *kind_name(int kind) {
    switch (kind) {
    case 1: return "affine";
    case 2: return "mediane";
    case 3: return "seuil";
    case 4: return "cyclique";
    case 5: return "excitable";
    case 6: return "nibble";
    case 7: return "accord";
    case 8: return "onde";
    default: return "?";
    }
}

/* 1 si le détecteur voit l'ensemble planté, refuse le bruit gelé et la peinture. */
static int device_ok(void) {
    static const uint8_t plant[8] = {1, 2, 4, 8, 16, 32, 64, 128};
    uint8_t p[4] = {0, 0, 0, 0};
    uint8_t t[L];
    int planted = 0, frozen = 0, painted = 0;

    for (int i = 0; i < L; i++) t[i] = plant[i % 8];
    planted = eval_state(0, p, t).ok;

    for (int s = 1; s <= SEEDS; s++) {
        fill_noise(t, 1000u + (unsigned)s);
        if (eval_state(0, p, t).ok) frozen++;
    }

    for (int i = 0; i < L; i++) t[i] = (uint8_t)(i * 17 + 3);
    /* kind spécial : peinture constante, codée par p[0] avec kind 0 modifié.
     * On force la bande à 0x11 et kind 0 : distinct = 1, doit échouer. */
    memset(t, 0x11, L);
    painted = eval_state(0, p, t).ok;

    printf("etalon plante : %s\n", planted ? "vu" : "manque");
    printf("etalon bruit gele : %d/%d vus\n", frozen, SEEDS);
    printf("etalon cristal : %s\n", painted ? "vu" : "refuse");
    return planted && frozen == 0 && !painted;
}

int main(void) {
    int dev = device_ok();
    printf("dispositif : %s\n", dev ? "ok" : "invalide");

    xs = 1;
    int npass = 0, nshare2 = 0, best = 0;
    for (int kind = 1; kind <= KINDS; kind++) {
        for (int draw = 0; draw < DRAWS; draw++) {
            uint8_t p[4];
            for (int i = 0; i < 4; i++) p[i] = (uint8_t)rnd();
            char paint[16 + P_MAX * 2];
            int painted = crystal_key(kind, p, paint);
            Hit hits[SEEDS];
            int nqual = 0;
            for (int s = 0; s < SEEDS; s++) {
                uint8_t t[L];
                fill_noise(t, 1000u + (unsigned)(s + 1));
                hits[s] = eval_state(kind, p, t);
                if (hits[s].ok && painted && strcmp(hits[s].key, paint) == 0)
                    hits[s].ok = 0;
                nqual += hits[s].ok;
            }
            int counted[SEEDS] = {0};
            int law_best = 0;
            char law_key[16 + P_MAX * 2] = "";
            for (int i = 0; i < SEEDS; i++) {
                if (!hits[i].ok || counted[i]) continue;
                int g = 0;
                for (int j = i; j < SEEDS; j++) {
                    if (hits[j].ok && strcmp(hits[i].key, hits[j].key) == 0) {
                        counted[j] = 1;
                        g++;
                    }
                }
                if (g > law_best) {
                    law_best = g;
                    memcpy(law_key, hits[i].key, sizeof law_key);
                }
            }
            if (law_best > best) best = law_best;
            if (law_best >= 2) {
                nshare2++;
                printf("journal %s#%d graines %d/%d cle %s params %02x%02x%02x%02x\n",
                       kind_name(kind), draw, law_best, SEEDS, law_key,
                       p[0], p[1], p[2], p[3]);
            }
            if (law_best >= NEED_SEEDS) npass++;
            (void)nqual;
        }
    }
    printf("lois a 2 graines ou plus : %d\n", nshare2);
    printf("lois a %d graines ou plus : %d\n", NEED_SEEDS, npass);
    printf("meilleur bassin : %d/%d\n", best, SEEDS);
    if (!dev) printf("verdict : dispositif invalide\n");
    else if (npass >= NEED_LAWS) printf("verdict : oui\n");
    else printf("verdict : non\n");
    return dev ? 0 : 2;
}
