/* Mesure, pas opérateur de naissance : on s'arrête au premier transfert minimal.
 * Depuis la racine : make experiments/general_transfer
 * ./experiments/general_transfer /tmp/general-transfer.adn
 * ./octet-adn general /tmp/general-transfer.adn ; echo $?
 * Les trois entrées sont des génomes de naissance du pool au cycle 60 de main
 * 382747b, --labs general --seed 0|3|2 --no-reload, 200 ticks/cycle. */
#include "octet.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static PoolEntry load(const char *name) {
    PoolEntry g = {0}; char path[256];
    snprintf(path, sizeof path, "experiments/general-transfer/%s.adn", name);
    FILE *f = fopen(path, "rb");
    if (!f) { perror(path); exit(1); }
    g.len = (int)fread(g.dna, 1, MAX_LEN, f);
    if (ferror(f) || g.len != 172 || fgetc(f) != EOF) {
        fprintf(stderr, "%s : attendu 172 octets\n", path); exit(1);
    }
    fclose(f); return g;
}

static int measure(const char *name, const PoolEntry *g) {
    PoolEntry seed; seed_entry(&seed, "general", 0);
    int d = abs(g->len - seed.len); double v[GN_CHECK_N];
    int e = gn_check(g, v);
    for (int i = 0; i < g->len && i < seed.len; i++) d += g->dna[i] != seed.dna[i];
    printf("%s len=%d distance=%d m2=%.9f m3=%.9f frozen=%.9f/%.9f "
           "no_reward=%.9f/%.9f known=%.9f train=%.9f code=%d\n",
           name, g->len, d, v[0], v[1], v[2], v[3], v[4], v[5], v[6], v[7], e ? 0 : 3);
    return e;
}

int main(int argc, char **argv) {
    if (argc != 2) { fprintf(stderr, "usage : %s SORTIE.adn\n", argv[0]); return 2; }
    PoolEntry donors[2] = { load("seed0-pool12"), load("seed3-pool3") };
    PoolEntry base = load("seed2-pool3"), seed, winner = {0};
    seed_entry(&seed, "general", 0);
    puts("Différences graine 0 / graine 3 (offset : valeur 0 / valeur 3) :");
    for (int i = 0; i < donors[0].len; i++)
        if (donors[0].dna[i] != donors[1].dna[i])
            printf("%02x : %02x / %02x\n", i, donors[0].dna[i], donors[1].dna[i]);
    measure("seed", &seed); measure("seed0-pool12", &donors[0]);
    measure("seed3-pool3", &donors[1]);
    if (measure("seed2-pool3 (ensemble vide)", &base)) {
        fprintf(stderr, "La base passe déjà : minimum non établi par ce protocole.\n"); return 1;
    }
    /* Tous les transferts d'un octet depuis l'un OU l'autre donneur : les deux
     * allèles sont essayés s'ils diffèrent ; les allèles identiques ne comptent
     * qu'une fois. Chaque candidat passe gn_check, 40 tâches par condition,
     * pour les DEUX mémoires. Le vide échoue : un passant à un octet est donc
     * un minimum global en cardinalité, sans hypothèse de monotonie. */
    int tried = 0, passed = 0;
    for (int i = 0; i < base.len; i++) for (int j = 0; j < 2; j++) {
        uint8_t allele = donors[j].dna[i];
        if (allele == base.dna[i] || (j && allele == donors[0].dna[i])) continue;
        PoolEntry g = base; g.dna[i] = allele; char label[80];
        snprintf(label, sizeof label, "ablation %02x:%02x>%02x (donneur %d)",
                 i, base.dna[i], allele, j ? 3 : 0);
        tried++;
        if (measure(label, &g)) { passed++; winner = g; }
    }
    printf("Transferts simples mesurés : %d ; passants : %d\n", tried, passed);
    if (passed != 1) { fprintf(stderr, "Attendu un transfert minimal unique.\n"); return 1; }
    FILE *out = fopen(argv[1], "wb");
    if (!out) { perror(argv[1]); return 1; }
    int ok = fwrite(winner.dna, 1, (size_t)winner.len, out) == (size_t)winner.len;
    if (fclose(out)) ok = 0;
    return ok ? 0 : 1;
}
