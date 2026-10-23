/* Sonde de phase : un détecteur de réparation, contrôlé avant toute mesure de vie.
 * Moteur copié de experiments/seul.c (bande 64, adressage B, exec64, budget 8192, capteurs nuls).
 * src/, include/, seeds/ et experiments/seul.c ne bougent pas.
 *
 * DÉTECTEUR v2 (figé avant le nouveau compte). La v1 lésait les 16 cases 16..31 d'un coup et
 * détruisait la source de la copie : le motif soupe ne tenait qu'à 1 phase sur 4. Ici on lèse
 * une seule case, la cible du prochain STORE de l'intact. Sans STORE à venir, pas de blessure.
 * Réparation si : (0) la case lésée diffère ; (1) la période de 16 qui la contient est revenue,
 * chez les deux jumeaux, à sa valeur d'avant la lésion ; (2) les deux y ont fait un STORE après
 * la lésion, dernière valeur égale à celle d'avant. Pas d'étiquette. wrap_cut (hors score) ignore
 * LOAD/STORE quand B+k sort de 0..63 : c'est le motif-artefact.
 *
 *   ./experiments/sonde_phase selftest   contrôles à phases fixes, code non nul si ça rate
 *   ./experiments/sonde_phase run        écrit experiments/sonde-phase/results.txt
 */
#include <stdio.h>
#include <string.h>
#include "octet.h"

#define L 64
#define STEPS 8192
#define PHASE_MAX 4096           /* phase tirée dans 0..4095 */
#define NWIT 32                  /* témoins par classe */
#define POS_MIN 3                /* positif : au moins 3 phases sur 4 */

static const int POS_PHASES[4] = { 0, 1024, 2048, 3072 };
static const uint64_t POS_LESION_SEED = 0x504C415345ULL; /* graines neuves, v2 */
static const uint8_t MOTIF[16] = {
    0x05, 0x1d, 0x03, 0x02, 0x1c, 0x1f, 0xff, 0x07, 0x01, 0x03, 0x09, 0x03, 0xc9, 0x06, 0x90, 0x1e
};

/* ---------------- moteur de seul.c, pas à pas ---------------- */
static inline int s8(uint8_t b) { return b > 127 ? (int)b - 256 : (int)b; }

/* Référence : exec64 de seul.c, mode 0 (capteurs nuls), copié tel quel. */
static int exec64(uint8_t *t, int addr_b, int *stores) {
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
            a = 0; break;
        case OP_NEURAL: {
            int v = s8(t[(k + 4) % L]);
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

/* La même machine, une instruction à la fois, avec le journal des STORE. */
typedef struct {
    uint8_t t[L], a, b;
    int ip, steps, halted, nstore, wrap_cut;
    int wstep[L];               /* pas de la dernière écriture de la case, -1 si aucune */
    uint8_t wval[L];
} M;

static void m_init(M *m, const uint8_t *t) {
    memset(m, 0, sizeof *m);
    memcpy(m->t, t, L);
    for (int i = 0; i < L; i++) m->wstep[i] = -1;
}

static void m_step(M *m) {
    if (m->halted) return;
    if (m->steps >= STEPS || m->ip >= L) { m->halted = 1; return; }
    uint8_t op = m->t[m->ip++];
    m->steps++;
    int k = 0;
    if (op_has_imm(op)) {
        if (m->ip >= L) { m->halted = 1; return; }
        k = m->t[m->ip++];
    }
    int raw = (int)m->b + k;
    int ad = raw % L;
    int cut = m->wrap_cut && (raw < 0 || raw >= L);
    switch (op) {
    case OP_INC_A: m->a++; break;
    case OP_DEC_A: m->a--; break;
    case OP_INC_B: m->b++; break;
    case OP_DEC_B: m->b--; break;
    case OP_CLR_A: m->a = 0; break;
    case OP_STORE:
        if (!cut) {
            m->t[ad] = m->a; m->nstore++; m->wstep[ad] = m->steps; m->wval[ad] = m->a;
        }
        break;
    case OP_LOAD: if (!cut) m->a = m->t[ad]; break;
    case OP_JMP: m->ip = (m->ip + k) % L; break;
    case OP_JZ: if (m->a == 0) m->ip = (m->ip + k) % L; break;
    case OP_JNZ: if (m->a != 0) m->ip = (m->ip + k) % L; break;
    case OP_SENSE: case OP_SENSE_NB: case OP_SENSE_EN: case OP_SENSE_STIM: case OP_SENSE_HIST:
        m->a = 0; break;
    case OP_NEURAL: {
        int v = s8(m->t[(k + 4) % L]);
        if (v < 0) v = 0;
        if (v > 255) v = 255;
        m->a = (uint8_t)v;
        break;
    }
    case OP_YIELD: m->halted = 1; break;
    case OP_MOV_BA: m->b = m->a; break;
    case OP_MOV_AB: m->a = m->b; break;
    case OP_ADD: m->a += m->b; break;
    case OP_SUB: m->a -= m->b; break;
    case OP_XOR: m->a ^= m->b; break;
    case OP_LOADI: m->a = (uint8_t)k; break;
    case OP_LOOP: m->ip = 0; break;
    case OP_AND: m->a &= (uint8_t)k; break;
    default: break;
    }
    if (m->steps >= STEPS || m->ip >= L) m->halted = 1;
}

static void m_run_to(M *m, int until) { while (!m->halted && m->steps < until) m_step(m); }

/* Adresse du prochain STORE, sans avancer le jumeau. 0 s'il n'écrit plus. */
static int prochain_store(const M *src, int *addr) {
    M m = *src;
    int n0 = m.nstore, guard = 0;
    while (!m.halted && m.nstore == n0 && guard++ < STEPS) m_step(&m);
    if (m.nstore == n0) return 0;
    int best = -1, a = -1;
    for (int i = 0; i < L; i++) if (m.wstep[i] > best) { best = m.wstep[i]; a = i; }
    if (a < 0) return 0;
    *addr = a;
    return 1;
}

/* ---------------- le détecteur ---------------- */
typedef struct { int wound, zone_eq, writes_ok, repair, w_zone_writes, halted_at; } Verdict;

/* lesion = 0 : aucune atteinte (témoin décalé en phase). */
static Verdict detect(const uint8_t *tape, int phase, int lesion, uint64_t lseed, int wrap_cut) {
    Verdict v = { 0 };
    static M I, W;
    m_init(&I, tape);
    I.wrap_cut = wrap_cut;
    m_run_to(&I, phase);
    W = I;                                            /* jumeaux identiques jusqu'à la phase */
    v.halted_at = I.halted ? I.steps : -1;
    int c = -1;
    uint8_t pre_per[16], pre_c = 0;
    int base = 0;
    if (lesion && prochain_store(&I, &c)) {
        base = c & ~15;
        memcpy(pre_per, I.t + base, 16);
        pre_c = I.t[c];
        Rng r; rng_seed(&r, lseed);
        W.t[c] = (uint8_t)(W.t[c] + 1 + (int)rng_below(&r, 255));
    } else c = -1;
    v.wound = c >= 0 && W.t[c] != I.t[c];
    for (int i = 0; i < L; i++) { I.wstep[i] = -1; W.wstep[i] = -1; }
    m_run_to(&I, STEPS + 1);
    m_run_to(&W, STEPS + 1);
    if (v.wound) {
        v.zone_eq = memcmp(I.t + base, pre_per, 16) == 0 && memcmp(W.t + base, pre_per, 16) == 0;
        v.writes_ok = I.wstep[c] >= 0 && W.wstep[c] >= 0 && I.wval[c] == pre_c && W.wval[c] == pre_c;
        for (int i = base; i < base + 16; i++) v.w_zone_writes += W.wstep[i] >= 0;
    }
    v.repair = v.wound && v.zone_eq && v.writes_ok;
    return v;
}

/* ---------------- bandes ---------------- */
static void motif4(uint8_t *p, uint8_t imm) {
    for (int i = 0; i < L; i++) p[i] = MOTIF[i % 16];
    for (int i = 14; i < L; i += 16) p[i] = imm;
}

/* Restaurateur de seul.c (12 LOADI/STORE puis LOOP), STORE remplacé par NOP. */
static void restorer_nostore(uint8_t *p) {
    static const uint8_t body[12] = { 0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7, 0xA8, 0xA9, 0xAA, 0xAB };
    memset(p, 0xFF, L);
    int i = 0;
    for (int k = 0; k < 12; k++) { p[i++] = OP_LOADI; p[i++] = body[k]; p[i++] = OP_NOP; p[i++] = (uint8_t)(49 + k); }
    p[i++] = OP_LOOP;
    for (int k = 0; k < 12; k++) p[49 + k] = body[k];
}

static uint8_t no_store_byte(Rng *r) {        /* tout octet sauf 0x06 : STORE ne peut pas s'exécuter */
    uint8_t x;
    do x = (uint8_t)rng_next(r); while (x == OP_STORE);
    return x;
}

/* Inerte v : v = 0, le restaurateur sans STORE ; sinon même gabarit, constantes, adresses et
 * remplissage tirés, aucun octet 0x06 nulle part. */
static void inert(uint8_t *p, int v, uint64_t seed) {
    restorer_nostore(p);
    if (v == 0) return;
    Rng r; rng_seed(&r, seed);
    for (int i = 0; i < L; i++) if (p[i] != OP_LOADI && p[i] != OP_NOP && p[i] != OP_LOOP) p[i] = no_store_byte(&r);
    for (int k = 0; k < 12; k++) { p[4 * k + 1] = no_store_byte(&r); p[4 * k + 3] = no_store_byte(&r); }
}

static void uniform(uint8_t *p, uint64_t seed) {
    Rng r; rng_seed(&r, seed);
    for (int i = 0; i < L; i++) p[i] = (uint8_t)rng_next(&r);
}

/* Cristal de période 16 sans aucun octet 0x06 : la zone lésée est coupée de tout STORE. */
static void crystal(uint8_t *p, uint64_t seed) {
    Rng r; rng_seed(&r, seed);
    uint8_t u[16];
    for (int i = 0; i < 16; i++) u[i] = no_store_byte(&r);
    for (int i = 0; i < L; i++) p[i] = u[i % 16];
}

/* ---------------- comptes ---------------- */
static uint64_t wit_seed(int cls, int i) { return 0x504C0000ULL + ((uint64_t)cls << 8) + (uint64_t)i; }

static int has_store_byte(const uint8_t *p) { for (int i = 0; i < L; i++) if (p[i] == OP_STORE) return 1; return 0; }

static int positive_count(FILE *o, uint8_t imm, const char *name, int wrap_cut) {
    uint8_t p[L]; motif4(p, imm);
    int n = 0;
    for (int k = 0; k < 4; k++) {
        Verdict v = detect(p, POS_PHASES[k], 1, POS_LESION_SEED + (uint64_t)k, wrap_cut);
        n += v.repair;
        if (o) fprintf(o, "%s\tphase=%d\tgraine_lesion=0x%llx\tblessure=%d\tzone_egale=%d\tecritures=%d\tecritures_zone=%d\treparation=%d\n",
                       name, POS_PHASES[k], (unsigned long long)(POS_LESION_SEED + (uint64_t)k), v.wound, v.zone_eq, v.writes_ok, v.w_zone_writes, v.repair);
    }
    return n;
}

static int selftest(void) {
    int fail = 0;
    /* le moteur pas à pas rejoue exec64 de seul.c */
    {
        int same = 1;
        for (int s = 0; s < 64; s++) {
            uint8_t a[L], b[L];
            if (s == 0) motif4(a, 0x90); else if (s == 1) restorer_nostore(a); else uniform(a, 0xC0DE00ULL + (uint64_t)s);
            memcpy(b, a, L);
            int st = 0; exec64(a, 1, &st);
            M m; m_init(&m, b); m_run_to(&m, STEPS + 1);
            if (memcmp(a, m.t, L) || st != m.nstore) same = 0;
        }
        printf("moteur pas à pas = exec64 de seul.c sur 64 bandes : %s\n", same ? "OK" : "ÉCHEC");
        fail |= !same;
    }
    int pos = positive_count(NULL, 0x90, "positif", 0);
    printf("positif, motif soupe 0x90 × 4, phases 0/1024/2048/3072 : %d/4 reconnues (seuil %d) %s\n", pos, POS_MIN, pos >= POS_MIN ? "OK" : "ÉCHEC");
    fail |= pos < POS_MIN;
    uint8_t p[L];
    int neg = 0;
    for (int k = 0; k < 4; k++) {
        inert(p, 0, 0); neg += detect(p, POS_PHASES[k], 1, POS_LESION_SEED + (uint64_t)k, 0).repair;
        uniform(p, 1); neg += detect(p, POS_PHASES[k], 1, POS_LESION_SEED + (uint64_t)k, 0).repair;
        crystal(p, 1); neg += detect(p, POS_PHASES[k], 1, POS_LESION_SEED + (uint64_t)k, 0).repair;
    }
    printf("négatifs à phases fixes (restaurateur sans STORE, bruit graine 1, cristal graine 1) : %d réparation sur 12 %s\n", neg, neg == 0 ? "OK" : "ÉCHEC");
    fail |= neg != 0;
    motif4(p, 0x90);
    int nol = 0;
    for (int k = 0; k < 4; k++) nol += detect(p, POS_PHASES[k], 0, 0, 0).repair;
    printf("motif sans lésion, phases fixes : %d réparation sur 4 %s\n", nol, nol == 0 ? "OK" : "ÉCHEC");
    fail |= nol != 0;
    printf("selftest %s\n", fail ? "ÉCHEC" : "OK");
    return fail;
}

static int run(const char *path) {
    FILE *o = fopen(path, "w");
    if (!o) { perror(path); return 1; }
    fprintf(o, "# sonde de phase v2 : une case, prochain STORE, phase 0..%d, budget %d, graines 0x504C, adressage B\n",
            PHASE_MAX - 1, STEPS);
    fprintf(o, "classe\tindice\tgraine\tphase\tstore_possible\tblessure\tzone_egale\tecritures\tecritures_zone\treparation\n");
    static const char *CLS[3] = { "inerte", "aleatoire", "cristal" };
    int fp[3] = { 0 }, stores_in_cut = 0;
    for (int c = 0; c < 3; c++) for (int i = 0; i < NWIT; i++) {
        uint64_t seed = wit_seed(c, i);
        uint8_t p[L];
        if (c == 0) inert(p, i, seed); else if (c == 1) uniform(p, seed); else crystal(p, seed);
        Rng r; rng_seed(&r, seed ^ 0xFA5EULL);
        int phase = (int)rng_below(&r, PHASE_MAX);
        Verdict v = detect(p, phase, 1, seed ^ 0x1E5ULL, 0);
        int sp = has_store_byte(p);
        if (c != 1) stores_in_cut += sp;
        fp[c] += v.repair;
        fprintf(o, "%s\t%d\t0x%llx\t%d\t%d\t%d\t%d\t%d\t%d\t%d\n", CLS[c], i, (unsigned long long)seed, phase, sp,
                v.wound, v.zone_eq, v.writes_ok, v.w_zone_writes, v.repair);
    }
    fprintf(o, "\n");
    int pos = positive_count(o, 0x90, "positif", 0);
    int art90 = positive_count(o, 0x90, "artefact90", 1);
    int art = positive_count(o, 0x10, "artefact10", 1);
    fprintf(o, "\n");
    int nol = 0;
    uint8_t m[L]; motif4(m, 0x90);
    for (int i = 0; i < NWIT; i++) {
        uint64_t seed = wit_seed(3, i);
        Rng r; rng_seed(&r, seed);
        int phase = (int)rng_below(&r, PHASE_MAX);
        Verdict v = detect(m, phase, 0, 0, 0);
        nol += v.repair;
        fprintf(o, "sans_lesion\t%d\t0x%llx\t%d\t1\t%d\t%d\t%d\t%d\t%d\n", i, (unsigned long long)seed, phase,
                v.wound, v.zone_eq, v.writes_ok, v.w_zone_writes, v.repair);
    }
    int total = fp[0] + fp[1] + fp[2];
    fprintf(o, "\n# comptes\n");
    fprintf(o, "inerte : %d réparation déclarée sur %d\n", fp[0], NWIT);
    fprintf(o, "aléatoire : %d réparation déclarée sur %d\n", fp[1], NWIT);
    fprintf(o, "cristal : %d réparation déclarée sur %d\n", fp[2], NWIT);
    fprintf(o, "témoins négatifs : %d sur %d (une seule rejette la version)\n", total, 3 * NWIT);
    fprintf(o, "octets 0x06 dans les classes inerte et cristal : %d bande (attendu 0)\n", stores_in_cut);
    fprintf(o, "positif, motif 0x90, phases 0/1024/2048/3072 : %d/4 (seuil %d)\n", pos, POS_MIN);
    fprintf(o, "à part, immédiat 0x90 sans modulo : %d/4\n", art90);
    fprintf(o, "à part, immédiat 0x10 sans modulo : %d/4\n", art);
    fprintf(o, "à part, motif sans lésion, phases tirées : %d réparation déclarée sur %d\n", nol, NWIT);
    const char *ver = total > 0 ? "version rejetée (fausse réparation)"
                    : pos < POS_MIN ? "échec de sonde (détecteur trop strict)"
                    : "sonde valide";
    fprintf(o, "sonde : %s\n", ver);
    fclose(o);
    printf("négatifs %d/%d, positif %d/4, artefact %d/4, sans lésion %d/%d : %s\n", total, 3 * NWIT, pos, art, nol, NWIT, ver);
    return 0;
}

int main(int argc, char **argv) {
    if (argc > 1 && strcmp(argv[1], "selftest") == 0) return selftest();
    if (argc > 1 && strcmp(argv[1], "run") == 0) return run(argc > 2 ? argv[2] : "experiments/sonde-phase/results.txt");
    fprintf(stderr, "usage : %s selftest | run [results.txt]\n", argv[0]);
    return 2;
}
