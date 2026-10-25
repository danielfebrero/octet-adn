/* Trois physiques de vie, désactivables, pour des mondes longs (approches Claude, 2026-10-25).
 * capture    : un génome porteur d'ATTACK prend un segment d'ADN à un voisin et lui retire de l'énergie.
 * parente    : la propension à donner de l'énergie à un voisin affamé est un octet hérité.
 * senescence : l'âge maximal est un octet hérité ; au-delà, la créature meurt.
 * Chaque monde suit aussi un octet neutre (len-3) comme témoin de dérive. État POD, RNG propre :
 * aucun tirage du monde n'est consommé, aucun changement de Params, World ou Creature. */
#ifndef OCTET_VIVRE_H
#define OCTET_VIVRE_H
#include "octet.h"

typedef struct {
    int capture, parente, senescence;     /* drapeaux */
    int period;                           /* passage tous les period ticks (capture, parente) */
    int capture_cost, capture_take, capture_gain;
    int give, give_rich, give_poor;       /* don, seuil du donneur, seuil du receveur */
    int life_base, life_step;             /* âge max = life_base + life_step × octet */
} VivreConfig;

typedef struct {
    Rng rng;
    long passages;
    long captures, capture_bytes, capture_refused, capture_dist_sum;
    long donations, donated, donation_refused;
    long old_deaths;
} VivreState;

typedef struct {                          /* relevé d'un échantillon, calculé à la demande */
    int n;
    double give_byte, life_byte, neutral_byte, frac_attack, frac_xor;
} VivreSample;

void vivre_init(VivreState *s, uint64_t seed);
/* Avant world_step : capture, don, mort par âge. Retourne 1 si un génome a changé. */
int vivre_step(World *w, const VivreConfig *cfg, VivreState *s);
void vivre_sample(const World *w, VivreSample *out);
int vivre_selftest(void);

#endif
