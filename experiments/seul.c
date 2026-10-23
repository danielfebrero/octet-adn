/* Seul : une bande de 64 octets, sans partenaire, monde éteint.
 * Question : après une atteinte sur un de ses octets, la même organisation revient-elle ?
 * Le copieur n'est pas le score. src/ n'est pas modifié.
 *
 * Barre, déclarée avant les chiffres (macros ci-dessous) :
 *   - point fixe : une exécution de la bande réglée la laisse identique ;
 *   - active : cette exécution fait au moins un STORE (un cristal que rien n'écrit échoue) ;
 *   - au moins 3 valeurs d'octet distinctes ;
 *   - au moins REPAIR_MIN cases réparées (3 substitutions sur 4 ramènent la bande entière) ;
 *   - au moins LETHAL_MIN case létale (0 substitution sur 4 ne ramène rien : une atteinte peut tuer) ;
 *   - la même bande reste point fixe, active, et franchit les deux comptes quand les capteurs
 *     lisent 7, puis quand ils lisent le numéro du pas (indépendante du monde).
 * Tirage : DRAW_N bandes uniformes. Oui si DRAW_YES d'entre elles passent.
 * Marche : WALK_N bandes, WALK_EPOCHS époques, mutation de fond MUT puis une exécution.
 *   Oui si WALK_YES bandes finales passent.
 * Un seul des deux seuils suffit pour un oui. En dessous, le verdict est non.
 */
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "octet.h"

#define L 64
#define REPAIR_MIN 8
#define LETHAL_MIN 1
#define DISTINCT_MIN 3
#define DRAW_N 8192
#define DRAW_YES 16
#define WALK_N 32
#define WALK_EPOCHS 16384
#define WALK_YES 4
#define MUT 0.00024
#define STEPS 8192

static inline int s8(uint8_t b) { return b > 127 ? (int)b - 256 : (int)b; }

static uint8_t sense(int mode, int steps) {
    if (mode == 1) return 7;
    if (mode == 2) return (uint8_t)steps;
    return 0;
}

/* mode 0 : capteurs nuls. mode 1 : constante 7. mode 2 : numéro du pas. */
static int exec64(uint8_t *t, int addr_b, int mode, int *stores) {
    int budget = STEPS, ip = 0, steps = 0, nstore = 0;
    uint8_t a = 0, b = 0;
    while (steps < budget && ip < L) {
        uint8_t op = t[ip++];
        steps++;
        int k = 0;
        if (op_has_imm(op)) {
            if (ip >= L) break;
            k = t[ip++];
        }
        int ad = (addr_b ? (int)b + k : k) % L;
        switch (op) {
        case OP_INC_A: a++; break;
        case OP_DEC_A: a--; break;
        case OP_INC_B: b++; break;
        case OP_DEC_B: b--; break;
        case OP_CLR_A: a = 0; break;
        case OP_STORE: t[ad] = a; nstore++; break;
        case OP_LOAD: a = t[ad]; break;
        case OP_JMP: ip = (ip + k) % L; break;
        case OP_JZ: if (a == 0) ip = (ip + k) % L; break;
        case OP_JNZ: if (a != 0) ip = (ip + k) % L; break;
        case OP_SENSE: case OP_SENSE_NB: case OP_SENSE_EN: case OP_SENSE_STIM: case OP_SENSE_HIST:
            a = sense(mode, steps); break;
        case OP_NEURAL: {
            int v = s8(t[(k + 4) % L]);
            if (mode) v += sense(mode, steps);
            if (v < 0) v = 0;
            if (v > 255) v = 255;
            a = (uint8_t)v;
            break;
        }
        case OP_YIELD:
            if (stores) *stores = nstore;
            return steps;
        case OP_MOV_BA: b = a; break;
        case OP_MOV_AB: a = b; break;
        case OP_ADD: a += b; break;
        case OP_SUB: a -= b; break;
        case OP_XOR: a ^= b; break;
        case OP_LOADI: a = (uint8_t)k; break;
        case OP_LOOP: ip = 0; break;
        case OP_AND: a &= (uint8_t)k; break;
        default: break;
        }
    }
    if (stores) *stores = nstore;
    return steps;
}

static int ndistinct(const uint8_t *t) {
    int seen[256] = {0}, n = 0;
    for (int i = 0; i < L; i++) n += !seen[t[i]]++;
    return n;
}

static void classify(const uint8_t *T, int addr_b, int mode, int *repaired, int *lethal) {
    *repaired = *lethal = 0;
    for (int i = 0; i < L; i++) {
        int back = 0;
        for (int j = 0; j < 4; j++) {
            uint8_t q[L];
            memcpy(q, T, L);
            q[i] = (uint8_t)(T[i] + 1 + j * 17);
            exec64(q, addr_b, mode, NULL);
            back += memcmp(q, T, L) == 0;
        }
        if (back >= 3) (*repaired)++;
        if (back == 0) (*lethal)++;
    }
}

typedef struct { int settled, active, distinct, repaired, lethal, ok; } Probe;

static Probe probe(uint8_t *t, int addr_b) {
    Probe p = {0};
    uint8_t a[L], b[L];
    int stores = 0, stores2 = 0;
    memcpy(a, t, L);
    exec64(a, addr_b, 0, &stores);
    memcpy(b, a, L);
    exec64(b, addr_b, 0, &stores2);
    if (memcmp(a, b, L)) return p;
    p.settled = 1;
    p.active = stores2 > 0;
    p.distinct = ndistinct(a);
    memcpy(t, a, L);
    if (!p.active || p.distinct < DISTINCT_MIN) return p;
    classify(a, addr_b, 0, &p.repaired, &p.lethal);
    if (p.repaired < REPAIR_MIN || p.lethal < LETHAL_MIN) return p;
    for (int mode = 1; mode <= 2; mode++) {
        uint8_t v[L];
        int st = 0, r, l;
        memcpy(v, a, L);
        exec64(v, addr_b, mode, &st);
        if (memcmp(v, a, L) || st <= 0) return p;
        classify(a, addr_b, mode, &r, &l);
        if (r < REPAIR_MIN || l < LETHAL_MIN) return p;
    }
    p.ok = 1;
    return p;
}

/* Douze cases réécrites depuis des LOADI, puis LOOP. Le corps est réparé, le code et les
 * introns peuvent mourir. Aucun capteur : le monde ne entre pas. */
static void restorer(uint8_t *p) {
    static const uint8_t body[12] = {
        0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7, 0xA8, 0xA9, 0xAA, 0xAB
    };
    memset(p, 0xFF, L);
    int i = 0;
    for (int k = 0; k < 12; k++) {
        p[i++] = OP_LOADI;
        p[i++] = body[k];
        p[i++] = OP_STORE;
        p[i++] = (uint8_t)(49 + k);
    }
    p[i++] = OP_LOOP;
    for (int k = 0; k < 12; k++) p[49 + k] = body[k];
}

static void radiate(uint8_t *t, Rng *rng) {
    double lq = log(1.0 - MUT);
    size_t i = 0;
    for (;;) {
        double x = rng_double(rng);
        if (x <= 0) continue;
        i += (size_t)(log(1.0 - x) / lq);
        if (i >= L) break;
        t[i] = (uint8_t)rng_next(rng);
        i++;
    }
}

static int selftest(void) {
    int fail = 0;
    uint8_t p[L];
    restorer(p);
    Probe r = probe(p, 1);
    int ok = r.ok && r.repaired >= 12 && r.lethal >= 1;
    printf("restaurateur fait main : ok=%d réglé=%d actif=%d dist=%d réparées=%d létales=%d %s\n",
           r.ok, r.settled, r.active, r.distinct, r.repaired, r.lethal, ok ? "OK" : "ÉCHEC");
    fail |= !ok;

    uint8_t broken[L];
    restorer(broken);
    for (int i = 0; i < L; i++) if (broken[i] == OP_STORE) broken[i] = OP_NOP;
    Probe br = probe(broken, 1);
    printf("restaurateur sans STORE : ok=%d actif=%d %s\n", br.ok, br.active, !br.ok ? "OK" : "ÉCHEC");
    fail |= br.ok;

    uint8_t cr[L];
    memset(cr, 0x11, L);
    Probe c = probe(cr, 1);
    printf("cristal 0x11 : ok=%d actif=%d %s\n", c.ok, c.active, !c.ok ? "OK" : "ÉCHEC");
    fail |= c.ok;

    uint8_t h[L];
    soup_handmade(h, 1);
    SoupCfg cfg = { .addr_b = 1, .no_store = 0, .steps = STEPS };
    int cop = soup_copier(h, &cfg) != 0;
    Probe hp = probe(h, 1);
    printf("copieur fait main, seul : copieur_128=%d ok=%d réparées=%d létales=%d %s\n",
           cop, hp.ok, hp.repaired, hp.lethal, (cop && !hp.ok) ? "OK" : "ÉCHEC");
    fail |= !cop || hp.ok;

    static const uint8_t motif[16] = {
        0x05, 0x1d, 0x03, 0x02, 0x1c, 0x1f, 0xff, 0x07, 0x01, 0x03, 0x09, 0x03, 0xc9, 0x06, 0x90, 0x1e
    };
    uint8_t rep[L];
    for (int i = 0; i < L; i++) rep[i] = motif[i % 16];
    Probe rp = probe(rep, 1);
    /* Sur 128 octets ce motif copie vers +64 (l'autre moitié). Seul, sur 64, +64 ≡ +0
     * et l'immédiat 0x90 retombe à +16, c'est-à-dire sur la période suivante du motif.
     * Le contrôle attendu est qu'il se répare : le détecteur le voit quand le partenaire disparaît. */
    printf("motif réplicateur soupe, seul : ok=%d réparées=%d létales=%d %s\n",
           rp.ok, rp.repaired, rp.lethal, (rp.ok && rp.repaired >= 8 && rp.lethal >= 1) ? "OK" : "ÉCHEC");
    fail |= !(rp.ok && rp.repaired >= 8 && rp.lethal >= 1);

    Rng rng;
    rng_seed(&rng, 1);
    uint8_t n1[L];
    for (int i = 0; i < L; i++) n1[i] = (uint8_t)rng_next(&rng);
    Probe n = probe(n1, 1);
    printf("bruit graine 1 : ok=%d réglé=%d réparées=%d %s\n", n.ok, n.settled, n.repaired, !n.ok ? "OK" : "ÉCHEC");
    fail |= n.ok;

    printf("selftest %s\n", fail ? "ÉCHEC" : "OK");
    return fail;
}

static int run(void) {
    int draw_ok = 0, best = 0, best_i = -1, seen = 0;
    Rng rng;
    rng_seed(&rng, 0x5E010001ULL);
    for (int i = 0; i < DRAW_N; i++) {
        uint8_t t[L];
        for (int k = 0; k < L; k++) t[k] = (uint8_t)rng_next(&rng);
        Probe p = probe(t, 1);
        seen += p.settled && p.active;
        if (p.repaired > best) { best = p.repaired; best_i = i; }
        draw_ok += p.ok;
        if ((i + 1) % 1024 == 0) fprintf(stderr, "tirage %d/%d passes=%d meilleur=%d\n", i + 1, DRAW_N, draw_ok, best);
    }
    printf("tirage %d bandes : %d passent, seuil %d, meilleur réparées=%d (indice %d), actives réglées=%d\n",
           DRAW_N, draw_ok, DRAW_YES, best, best_i, seen);

    int walk_ok = 0, walk_best = 0;
    for (int s = 0; s < WALK_N; s++) {
        Rng w;
        rng_seed(&w, 0x5E01A11C0000ULL + (uint64_t)s);
        uint8_t t[L];
        for (int k = 0; k < L; k++) t[k] = (uint8_t)rng_next(&w);
        for (int e = 0; e < WALK_EPOCHS; e++) {
            radiate(t, &w);
            exec64(t, 1, 0, NULL);
        }
        Probe p = probe(t, 1);
        walk_ok += p.ok;
        if (p.repaired > walk_best) walk_best = p.repaired;
        fprintf(stderr, "marche %d/%d ok=%d réparées=%d\n", s + 1, WALK_N, p.ok, p.repaired);
    }
    printf("marche %d × %d époques, mut %g : %d passent, seuil %d, meilleur réparées=%d\n",
           WALK_N, WALK_EPOCHS, MUT, walk_ok, WALK_YES, walk_best);
    int yes = draw_ok >= DRAW_YES || walk_ok >= WALK_YES;
    printf("verdict : %s\n", yes ? "oui" : "non");
    return 0;
}

int main(int argc, char **argv) {
    if (argc > 1 && strcmp(argv[1], "selftest") == 0) return selftest();
    return run();
}
