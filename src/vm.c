/* La VM (le « ribosome ») : lit le génome octet par octet. Déterministe : tout l'aléa
 * passe par world->rng. */
#include "octet.h"

void world_move(World *w, Creature *c);
void world_eat(World *w, Creature *c);
void world_split(World *w, Creature *c);
void world_attack(World *w, Creature *c);
void world_emit(World *w, Creature *c, int v);

static inline int s8(uint8_t b) { return b > 127 ? (int)b - 256 : (int)b; }
static inline int floordiv16(int v) { return v >= 0 ? v / 16 : -((-v + 15) / 16); }
static inline int imin(int a, int b) { return a < b ? a : b; }

/* Un neurone : 4 entrées capteurs (0..15) × 4 poids int8 + biais → A (0..255). */
static int neural(const Creature *c, const World *w, int addr) {
    int L = c->len;
    int x[4] = { imin(15, food_ahead(w, c)), imin(15, 2 * neighbors(w, c)),
                 imin(15, c->e >> 4), w->stimulus & 15 };
    int s = 0;
    for (int j = 0; j < 4; j++) s += s8(c->dna[(addr + j) % L]) * x[j];
    int v = floordiv16(s + 8) + s8(c->dna[(addr + 4) % L]);
    return v < 0 ? 0 : v > 255 ? 255 : v;
}

static inline int imm8(Creature *c) {
    int v = c->dna[c->ip % c->len];
    c->ip = (c->ip + 1) % c->len;
    return v;
}

int vm_run(Creature *c, World *w) {
    if (c->len <= 0 || c->e <= 0) return 0;
    c->age++;
    c->emitted = -1;
    int budget = OPS_PER_TICK, skips = 0;
    uint8_t *dna = c->dna;
    while (budget > 0 && c->e > 0) {
        int L = c->len, i = c->ip % L, k;
        uint8_t op = dna[i];
        c->ip = (i + 1) % L;
        if (c->dirty[i >> 3] & (1u << (i & 7))) {      /* codon réécrit puis exécuté */
            c->dirty[i >> 3] &= (uint8_t)~(1u << (i & 7));
            c->useful_stores++;
            w->st.useful_stores++;
        }
        if (op == OP_NOP || !op_valid(op)) {           /* intron : gratuit, borné */
            if (++skips > L) break;
            continue;
        }
        budget--;
        switch (op) {
        case OP_INC_A: c->a++; break;
        case OP_DEC_A: c->a--; break;
        case OP_INC_B: c->b++; break;
        case OP_DEC_B: c->b--; break;
        case OP_CLR_A: c->a = 0; break;
        case OP_STORE:                                 /* genome[imm] = A */
            k = imm8(c);
            if (c->win_len > 0 && k >= 0xE0) { c->win[(k - 0xE0) % c->win_len] = c->a; break; }   /* code-repair */
            k %= L;
            if (dna[k] != c->a) {
                dna[k] = c->a;
                c->stores++;
                c->dirty[k >> 3] |= (uint8_t)(1u << (k & 7));
                w->st.store_writes++;
            }
            break;
        case OP_LOAD:
            k = imm8(c);
            c->a = (c->win_len > 0 && k >= 0xE0) ? c->win[(k - 0xE0) % c->win_len] : dna[k % L];
            break;
        case OP_JMP: k = imm8(c); c->ip = (c->ip + k) % L; break;
        case OP_JZ: k = imm8(c); if (c->a == 0) c->ip = (c->ip + k) % L; break;
        case OP_SENSE: c->a = (uint8_t)food_ahead(w, c); break;
        case OP_TURN: c->d = c->a % 4; break;
        case OP_STEP: world_move(w, c); break;
        case OP_EAT: world_eat(w, c); break;
        case OP_SPLIT: world_split(w, c); break;
        case OP_YIELD: budget = 0; break;
        case OP_SENSE_NB: c->a = (uint8_t)imin(255, neighbors(w, c)); break;
        case OP_SENSE_EN: c->a = (uint8_t)imin(255, c->e >> 3); break;
        case OP_SENSE_STIM: c->a = (uint8_t)w->stimulus; break;
        case OP_NEURAL: c->a = (uint8_t)neural(c, w, imm8(c)); break;
        case OP_EMIT:
            if (c->emitted < 0) { c->emitted = c->a; world_emit(w, c, c->a); }
            break;
        case OP_MOV_BA: c->b = c->a; break;
        case OP_MOV_AB: c->a = c->b; break;
        case OP_ADD: c->a += c->b; break;
        case OP_SUB: c->a -= c->b; break;
        case OP_XOR: c->a ^= c->b; break;
        case OP_ATTACK: world_attack(w, c); break;
        case OP_JNZ: k = imm8(c); if (c->a != 0) c->ip = (c->ip + k) % L; break;
        case OP_LOADI: c->a = (uint8_t)imm8(c); break;
        case OP_SENSE_HIST: c->a = (uint8_t)history_at(w, imm8(c)); break;
        case OP_LOOP: c->ip = 0; break;
        case OP_AND: c->a &= (uint8_t)imm8(c); break;
        }
        if (op == OP_YIELD) break;
    }
    c->e -= 1;                                         /* métabolisme de base */
    return c->e > 0;
}
