/* RNG déterministe : splitmix64 pour l'amorçage, xoshiro256** pour le flux. */
#include <math.h>
#include "octet.h"

static uint64_t splitmix(uint64_t *x) {
    uint64_t z = (*x += 0x9e3779b97f4a7c15ULL);
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
    return z ^ (z >> 31);
}
void rng_seed(Rng *r, uint64_t seed) {
    for (int i = 0; i < 4; i++) r->s[i] = splitmix(&seed);
}
static inline uint64_t rotl(uint64_t x, int k) { return (x << k) | (x >> (64 - k)); }
uint64_t rng_next(Rng *r) {
    uint64_t *s = r->s, res = rotl(s[1] * 5, 7) * 9, t = s[1] << 17;
    s[2] ^= s[0]; s[3] ^= s[1]; s[1] ^= s[2]; s[0] ^= s[3]; s[2] ^= t; s[3] = rotl(s[3], 45);
    return res;
}
double rng_double(Rng *r) { return (double)(rng_next(r) >> 11) * 0x1.0p-53; }
uint32_t rng_below(Rng *r, uint32_t n) {
    return n ? (uint32_t)(((unsigned __int128)rng_next(r) * n) >> 64) : 0;
}
int rng_int(Rng *r, int a, int b) { return a + (int)rng_below(r, (uint32_t)(b - a + 1)); }
double rng_gauss(Rng *r, double mu, double sigma) {
    double u1 = 1.0 - rng_double(r), u2 = rng_double(r);
    return mu + sigma * sqrt(-2.0 * log(u1)) * cos(2.0 * M_PI * u2);
}
uint64_t fnv1a(const void *p, size_t n, uint64_t h) {
    const uint8_t *b = p;
    while (n--) { h ^= *b++; h *= 0x100000001b3ULL; }
    return h;
}
