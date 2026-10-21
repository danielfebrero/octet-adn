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
