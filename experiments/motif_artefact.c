/* Artefact du motif soupe, posé seul.
 * Le 48/15 de seul-duree est-il l'écho d'une période de 16, ou une organisation ?
 * src/ ne bouge pas. La barre est dans experiments/motif-artefact/README.md, avant les chiffres.
 *
 * Levier période : un STORE dont la valeur est déjà l'octet 16 cases en amont n'écrit pas.
 * Levier wrap : LOAD/STORE n'agissent que si B+k est déjà dans 0..63. Les sauts restent modulo.
 * Levier tours : LOOP s'arrête (il ne remet pas ip à 0), budget 8192 ;
 *                puis LOOP intact et budget 48.
 * Carte : sur la sonde normale, les cases réparées sont dans 16..63, les mortelles dans 0..15.
 * Un levier est nul si le restaurateur fait main tombe. Le copieur, le cristal et le bruit
 * de graine 1 ne passent pas la sonde normale.
 */
#include <stdio.h>
#include <string.h>
#include "octet.h"

#define L 64
#define REPAIR_MIN 8
#define LETHAL_MIN 1
#define DISTINCT_MIN 3
#define STEPS 8192
#define F_ECHO 1
#define F_NOWRAP 2
#define F_NOLOOP 4

static inline int s8(uint8_t b) { return b > 127 ? (int)b - 256 : (int)b; }

static uint8_t sense(int mode, int steps) {
    if (mode == 1) return 7;
    if (mode == 2) return (uint8_t)steps;
    return 0;
}

static int exec64(uint8_t *t, int mode, int flags, int budget, int *stores) {
    int ip = 0, steps = 0, nstore = 0;
    uint8_t a = 0, b = 0;
    while (steps < budget && ip < L) {
        uint8_t op = t[ip++];
        steps++;
        int k = 0;
        if (op_has_imm(op)) {
            if (ip >= L) break;
            k = t[ip++];
        }
        int raw = (int)b + k;
        int in = raw >= 0 && raw < L;
        int ad = raw % L;
        switch (op) {
        case OP_INC_A: a++; break;
        case OP_DEC_A: a--; break;
        case OP_INC_B: b++; break;
        case OP_DEC_B: b--; break;
        case OP_CLR_A: a = 0; break;
        case OP_STORE:
            if ((flags & F_NOWRAP) && !in) break;
            if ((flags & F_ECHO) && a == t[(ad - 16 + L) % L]) break;
            t[ad] = a;
            nstore++;
            break;
        case OP_LOAD:
            if ((flags & F_NOWRAP) && !in) break;
            a = t[ad];
            break;
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
        case OP_LOOP:
            if (flags & F_NOLOOP) {
                if (stores) *stores = nstore;
                return steps;
            }
            ip = 0;
            break;
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

static void backs(const uint8_t *T, int mode, int flags, int budget, int *back) {
    for (int i = 0; i < L; i++) {
        int n = 0;
        for (int j = 0; j < 4; j++) {
            uint8_t q[L];
            memcpy(q, T, L);
            q[i] = (uint8_t)(T[i] + 1 + j * 17);
            exec64(q, mode, flags, budget, NULL);
            n += memcmp(q, T, L) == 0;
        }
        back[i] = n;
    }
}

typedef struct { int settled, active, distinct, repaired, lethal, ok; } Probe;

static Probe probe(uint8_t *t, int flags, int budget) {
    Probe p = {0};
    uint8_t a[L], b[L];
    int stores = 0, stores2 = 0, back[L];
    memcpy(a, t, L);
    exec64(a, 0, flags, budget, &stores);
    memcpy(b, a, L);
    exec64(b, 0, flags, budget, &stores2);
    if (memcmp(a, b, L)) return p;
    p.settled = 1;
    p.active = stores2 > 0;
    p.distinct = ndistinct(a);
    memcpy(t, a, L);
    if (!p.active || p.distinct < DISTINCT_MIN) return p;
    backs(a, 0, flags, budget, back);
    for (int i = 0; i < L; i++) {
        if (back[i] >= 3) p.repaired++;
        if (back[i] == 0) p.lethal++;
    }
    if (p.repaired < REPAIR_MIN || p.lethal < LETHAL_MIN) return p;
    for (int mode = 1; mode <= 2; mode++) {
        uint8_t v[L];
        int st = 0, r = 0, l = 0;
        memcpy(v, a, L);
        exec64(v, mode, flags, budget, &st);
        if (memcmp(v, a, L) || st <= 0) return p;
        backs(a, mode, flags, budget, back);
        for (int i = 0; i < L; i++) {
            if (back[i] >= 3) r++;
            if (back[i] == 0) l++;
        }
        if (r < REPAIR_MIN || l < LETHAL_MIN) return p;
    }
    p.ok = 1;
    return p;
}

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

static void motif(uint8_t *p, uint8_t imm) {
    static const uint8_t m[16] = {
        0x05, 0x1d, 0x03, 0x02, 0x1c, 0x1f, 0xff, 0x07, 0x01, 0x03, 0x09, 0x03, 0xc9, 0x06, 0x90, 0x1e
    };
    for (int i = 0; i < L; i++) p[i] = m[i % 16];
    for (int i = 14; i < L; i += 16) p[i] = imm;
}

static void noise(uint8_t *p) {
    Rng rng;
    rng_seed(&rng, 1);
    for (int i = 0; i < L; i++) p[i] = (uint8_t)rng_next(&rng);
}

static void show(const char *name, const Probe *p) {
    printf("  %s : ok=%d réglé=%d actif=%d dist=%d réparées=%d létales=%d\n",
           name, p->ok, p->settled, p->active, p->distinct, p->repaired, p->lethal);
}

/* 1 = le motif tombe et le restaurateur tient (écho). 0 = le motif tient. -1 = restaurateur tombé. */
static int lever(const char *title, int flags, int budget, int expect_imm10) {
    uint8_t mot[L], imm10[L], res[L];
    motif(mot, 0x90);
    motif(imm10, 0x10);
    restorer(res);
    Probe pm = probe(mot, flags, budget);
    Probe pr = probe(res, flags, budget);
    Probe p10 = probe(imm10, flags, budget);
    printf("%s\n", title);
    show("motif 0x90", &pm);
    show("motif 0x10", &p10);
    show("restaurateur", &pr);
    if (!pr.ok) {
        printf("  levier nul : le restaurateur tombe\n");
        return -1;
    }
    if (expect_imm10) {
        int echo = !pm.ok && p10.ok;
        printf("  %s\n", echo ? "écho : 0x90 tombe, 0x10 tient" : "pas un écho sur ce levier");
        return echo;
    }
    int echo = !pm.ok;
    printf("  %s\n", echo ? "écho : le motif tombe" : "pas un écho : le motif tient");
    return echo;
}

static int carte(void) {
    uint8_t mot[L];
    int back[L], rep_lo = 0, rep_hi = 0, let_lo = 0, let_hi = 0;
    motif(mot, 0x90);
    Probe p = probe(mot, 0, STEPS);
    /* probe a réglé mot. On recompte sur cette bande, capteurs nuls. */
    backs(mot, 0, 0, STEPS, back);
    printf("carte, capteurs nuls, sonde normale (motif ok=%d réparées=%d létales=%d)\n",
           p.ok, p.repaired, p.lethal);
    printf("  retours :");
    for (int i = 0; i < L; i++) printf(" %d", back[i]);
    printf("\n");
    for (int i = 0; i < L; i++) {
        if (back[i] >= 3) { if (i < 16) rep_lo++; else rep_hi++; }
        if (back[i] == 0) { if (i < 16) let_lo++; else let_hi++; }
    }
    int ok = rep_lo == 0 && let_hi == 0 && rep_hi >= REPAIR_MIN && let_lo >= LETHAL_MIN;
    printf("  réparées 0..15=%d 16..63=%d ; létales 0..15=%d 16..63=%d ; %s\n",
           rep_lo, rep_hi, let_lo, let_hi, ok ? "carte tenue" : "carte ratée");
    return ok;
}

int main(void) {
    int base_ok = 1;
    uint8_t mot[L], res[L], cop[L], cry[L], noi[L];
    motif(mot, 0x90);
    restorer(res);
    soup_handmade(cop, 1);
    memset(cry, 0x11, L);
    noise(noi);
    Probe pm = probe(mot, 0, STEPS);
    Probe pr = probe(res, 0, STEPS);
    Probe pc = probe(cop, 0, STEPS);
    Probe py = probe(cry, 0, STEPS);
    Probe pn = probe(noi, 0, STEPS);
    printf("base, sonde de seul\n");
    show("motif", &pm);
    show("restaurateur", &pr);
    show("copieur fait main", &pc);
    show("cristal", &py);
    show("bruit graine 1", &pn);
    if (!pm.ok || !pr.ok || pc.ok || py.ok || pn.ok) {
        printf("dispositif invalide\n");
        base_ok = 0;
    }
    int map_ok = carte();
    int periode = lever("période : STORE étouffé si la valeur est déjà 16 cases en amont", F_ECHO, STEPS, 0);
    int wrap = lever("wrap : LOAD/STORE sans modulo", F_NOWRAP, STEPS, 1);
    int loop = lever("tours : LOOP s'arrête, budget 8192", F_NOLOOP, STEPS, 0);
    int court = lever("tours : LOOP intact, budget 48", 0, 48, 0);
    int echo = base_ok && map_ok && periode == 1 && wrap == 1 && loop == 1 && court == 1;
    int nul = periode < 0 || wrap < 0 || loop < 0 || court < 0 || !base_ok;
    if (!base_ok) printf("verdict : dispositif invalide\n");
    else if (nul) printf("verdict : levier nul\n");
    else if (echo) printf("verdict : écho\n");
    else printf("verdict : pas un écho\n");
    return base_ok ? 0 : 1;
}
