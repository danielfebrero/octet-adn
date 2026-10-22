/* Mutations : substitution, micro-dérive, insertion, délétion, duplication de bloc, crossover. */
#include <string.h>
#include "octet.h"

int duplicate_block(uint8_t *buf, int n, Rng *r) {
    int hi = n / 4 < 16 ? n / 4 : 16;
    if (hi < 2) hi = 2;
    int blen = rng_int(r, 2, hi);
    if (n + blen > MAX_LEN) return n;
    int a = (int)rng_below(r, (uint32_t)(n - blen + 1));
    int dst = (int)rng_below(r, (uint32_t)(n + 1));
    uint8_t block[16];
    memcpy(block, buf + a, (size_t)blen);
    memmove(buf + dst + blen, buf + dst, (size_t)(n - dst));
    memcpy(buf + dst, block, (size_t)blen);
    return n + blen;
}

int mutate(const uint8_t *in, int n, Rng *r, double rate, double dup_rate, uint8_t *out) {
    static const int drift[4] = { -3, -1, 1, 3 };
    memcpy(out, in, (size_t)n);
    int i = 0;
    while (i < n) {
        if (rng_double(r) < rate) {
            int k = (int)rng_below(r, 6);
            if (k <= 1) out[i] = (uint8_t)rng_below(r, OP_COUNT);           /* codon */
            else if (k == 2) out[i] = (uint8_t)rng_below(r, 256);           /* brut (synapses) */
            else if (k == 3) out[i] = (uint8_t)(out[i] + drift[rng_below(r, 4)]); /* dérive poids */
            else if (k == 4 && n < MAX_LEN) {                               /* insertion */
                memmove(out + i + 1, out + i, (size_t)(n - i));
                out[i] = (uint8_t)rng_below(r, OP_COUNT);
                n++; i++;
            } else if (k == 5 && n > MIN_LEN) {                             /* délétion */
                memmove(out + i, out + i + 1, (size_t)(n - i - 1));
                n--;
                continue;
            }
        }
        i++;
    }
    if (rng_double(r) < dup_rate && n >= 8) n = duplicate_block(out, n, r);
    return n;
}

/* Crossover à un point aligné (préserve les adresses absolues avant le point). */
int crossover(const uint8_t *a, int la, const uint8_t *b, int lb, Rng *r, uint8_t *out) {
    int n = la < lb ? la : lb;
    if (n < 2) { memcpy(out, a, (size_t)la); return la; }
    int cut = 1 + (int)rng_below(r, (uint32_t)(n - 1));
    memcpy(out, a, (size_t)cut);
    memcpy(out + cut, b + cut, (size_t)(lb - cut));
    return lb;
}

/* self-evolve : le génome porte ses paramètres d'évolution aux octets 2..5 (la seed commence par
 * JMP +4 pour les sauter). Bornes dures : substitution ≤ 12 %, indel ≤ 6 %, zone protégée ≤ 16 o
 * et ≤ moitié du génome. La zone ne couvre jamais les octets 2..5 : ils dérivent toujours (±1/±3, saturé)
 * (±4/±16) à EVO_META ; protéger coûte prot/4 d'énergie à chaque SPLIT (world_split). Un taux 0 ne reste donc pas 0. */
EvoParams evo_params(const uint8_t *dna, int len) {
    EvoParams p = { 0, 0, 0, 0 };
    if (len < 6) return p;
    p.mut = dna[2] / 255.0 * EVO_MUT_MAX;
    p.indel = dna[3] / 255.0 * EVO_INDEL_MAX;
    p.xover = dna[4] & 1;
    p.prot = dna[5] > EVO_PROT_MAX ? EVO_PROT_MAX : dna[5];
    if (p.prot > len / 2) p.prot = len / 2;
    return p;
}
int mutate_evo(const uint8_t *in, int n, Rng *r, const EvoParams *p, uint8_t *out) {
    static const int drift[4] = { -3, -1, 1, 3 }, pdrift[4] = { -16, -4, 4, 16 };   /* paramètres : pas ×5 (±1/±3 les figeait) */
    memcpy(out, in, (size_t)n);
    int i = 0;
    while (i < n) {
        if (i >= 2 && i < 6) {                         /* paramètres : jamais protégés, taux méta fixe */
            if (rng_double(r) < EVO_META) { int v = out[i] + pdrift[rng_below(r, 4)]; out[i] = (uint8_t)(v < 0 ? 0 : v > 255 ? 255 : v); }   /* saturé, pas de bouclage 0↔255 */
            i++; continue;
        }
        if (i < p->prot) { i++; continue; }            /* zone protégée (hors paramètres) */
        if (rng_double(r) < p->mut) {
            int k = (int)rng_below(r, 4);
            if (k <= 1) out[i] = (uint8_t)rng_below(r, OP_COUNT);
            else if (k == 2) out[i] = (uint8_t)rng_below(r, 256);
            else out[i] = (uint8_t)(out[i] + drift[rng_below(r, 4)]);
        }
        if (i >= 6 && rng_double(r) < p->indel) {
            if (rng_below(r, 2) && n < MAX_LEN) {
                memmove(out + i + 1, out + i, (size_t)(n - i));
                out[i] = (uint8_t)rng_below(r, OP_COUNT);
                n++; i++;
            } else if (n > MIN_LEN) {
                memmove(out + i, out + i + 1, (size_t)(n - i - 1));
                n--;
                continue;
            }
        }
        i++;
    }
    return n;
}

/* Idiome « MOV_BA ; ADD ; MOV_BA ; ADD ; MOV_BA ; LOADI k ; ADD ; STORE case ».
 * On le réécrit, à la même place : la case précédente reçoit l'ancienne valeur,
 * la case reçoit 32 + le registre B. Aucune insertion : un octet de plus décalerait
 * les STORE absolus et casserait la table. */
int stagger_cell(uint8_t *dna, int n) {
    if (n < 10) return 0;
    for (int i = 0; i + 10 <= n; i++) {
        if (dna[i] != 0x15 || dna[i + 1] != 0x17 || dna[i + 2] != 0x15 || dna[i + 3] != 0x17
            || dna[i + 4] != 0x15 || dna[i + 5] != 0x1c || dna[i + 7] != 0x17 || dna[i + 8] != 0x06)
            continue;
        uint8_t cell = dna[i + 9];
        if (cell == 0) return 0;
        dna[i + 1] = 0x07; dna[i + 2] = cell; dna[i + 3] = 0x06; dna[i + 4] = (uint8_t)(cell - 1);
        dna[i + 5] = 0x1c; dna[i + 6] = 32; dna[i + 7] = 0x17; dna[i + 8] = 0x06; dna[i + 9] = cell;
        return 1;
    }
    return 0;
}

/* Le LOAD de la case décalée qui se trouve avant la copie. Pas le LOAD interne de la copie :
 * celui-là doit continuer à lire la case qu'on est en train d'écraser. */
int retarget_prev(uint8_t *dna, int n) {
    if (n < 10) return 0;
    for (int i = 0; i + 10 <= n; i++) {
        uint8_t cell = dna[i + 2], prev = dna[i + 4];
        if (dna[i] != 0x15 || dna[i + 1] != 0x07 || dna[i + 3] != 0x06 || dna[i + 5] != 0x1c
            || dna[i + 6] != 32 || dna[i + 7] != 0x17 || dna[i + 8] != 0x06 || dna[i + 9] != cell
            || prev != (uint8_t)(cell - 1) || cell == 0)
            continue;
        for (int j = 0; j + 1 < i; j++) {
            if (dna[j] == 0x07 && dna[j + 1] == cell) { dna[j + 1] = prev; return 1; }
        }
        return 0;
    }
    return 0;
}
