/* Levier « regs » : chemins à la main depuis la seed, puis tous les mutants à un octet.
 * Mesure, pas opérateur de naissance. Depuis la racine : make experiments/general_regs
 * ./experiments/general_regs            chemins à la main (secondes)
 * ./experiments/general_regs --scan     + tous les mutants à un octet des deux bases (long : > 20 min) */
#include "octet.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int show(const char *name, const PoolEntry *g, double *v) {
    int e = gn_check(g, v);
    printf("%-34s m2=%.3f m3=%.3f gelé=%.3f/%.3f sans_récompense=%.3f/%.3f known=%.3f code=%d\n",
           name, v[0], v[1], v[2], v[3], v[4], v[5], v[6], e ? 0 : 3);
    return e;
}

int main(int argc, char **argv) {
    PoolEntry seed, g; double v[GN_CHECK_N];
    setvbuf(stdout, NULL, _IOLBF, 0);
    seed_entry(&seed, "general", 0);
    show("seed", &seed, v);
    g = seed; g.dna[0x93] = OPX_RPUSH; show("0x93 STORE→RPUSH", &g, v);
    g.dna[0x77] = OPX_RPEEK; show("+ 0x77 LOAD→RPEEK (k=20, r0)", &g, v);
    PoolEntry base[2];
    for (int lag = 1; lag <= 2; lag++) {
        PoolEntry h = g; char nm[64];
        h.dna[0x78] = (uint8_t)(0x14 + lag);
        snprintf(nm, sizeof nm, "+ 0x78 k=%d (r%d, p%d)", 0x14 + lag, lag, lag + 1); show(nm, &h, v);
        h.dna[0x79] = 0xcf;
        snprintf(nm, sizeof nm, "+ 0x79 ADD→cf (p%d seul)", lag + 1); show(nm, &h, v);
        base[lag - 1] = h;
    }
    if (argc < 2 || strcmp(argv[1], "--scan")) return 0;
    /* Tous les mutants à un octet de chaque base : meilleur m2, meilleur m3, meilleur min. */
    for (int b = 0; b < 2; b++) {
        double bm2 = 0, bm3 = 0, bmin = 0; int i2 = 0, i3 = 0, im = 0, x2 = 0, x3 = 0, xm = 0, pass = 0;
        for (int i = 0; i < 172; i++) for (int x = 0; x < 256; x++) {
            if (x == base[b].dna[i]) continue;
            PoolEntry h = base[b]; h.dna[i] = (uint8_t)x;
            int e = gn_check(&h, v); pass += e != 0;
            if (v[0] > bm2) { bm2 = v[0]; i2 = i; x2 = x; }
            if (v[1] > bm3) { bm3 = v[1]; i3 = i; x3 = x; }
            double mn = v[0] < v[1] ? v[0] : v[1];
            if (mn > bmin) { bmin = mn; im = i; xm = x; }
        }
        printf("base p%d : 43 860 mutants, passants %d ; meilleur m2 %.3f (%02x=%02x) ; meilleur m3 %.3f (%02x=%02x) ; meilleur min %.3f (%02x=%02x)\n",
               b + 2, pass, bm2, i2, x2, bm3, i3, x3, bmin, im, xm);
    }
    return 0;
}
