/* Trois physiques de vie, désactivables, pour des mondes longs (approches Claude, 2026-10-25).
 * capture    : un génome porteur d'ATTACK prend un segment d'ADN à un voisin et lui retire de l'énergie.
 * parente    : la propension à donner de l'énergie à un voisin affamé est un octet hérité.
 * senescence : l'âge maximal est un octet hérité ; au-delà, la créature meurt.
 * Chaque monde suit aussi un octet neutre (len-3) comme témoin de dérive.
 * Bras sénescence (débat 4881cc62) : senescence = 1 règle actuelle, 2 âge max découplé de l'octet
 * (octet fixé à VIVRE_LIFE_FIXED), 3 règle inversée (octet haut = vie courte). Nouveau-né = âge < life_base :
 * aucune mort d'âge possible avant, donc pas de biais de survie par l'âge sur ce relevé. État POD, RNG propre :
 * aucun tirage du monde n'est consommé, aucun changement de Params, World ou Creature. */
#ifndef OCTET_VIVRE_H
#define OCTET_VIVRE_H
#include "octet.h"

#define VIVRE_SEN_ACTUEL   1
#define VIVRE_SEN_DECOUPLE 2
#define VIVRE_SEN_INVERSE  3
#define VIVRE_LIFE_FIXED   128   /* préenregistré : moyenne initiale 128,6 du monde vie-senescence-3a1923e */
#define VIVRE_NNEUTRES     4
extern const int VIVRE_NEUTRES[VIVRE_NNEUTRES];   /* len-1, len-4, len-6, len-10 : distances 1, 2, 4, 8 du locus len-2 */

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
    int young;                            /* âge < life_base */
    double young_life, young_low;         /* moyenne de l'octet de vie chez les nouveau-nés ; fraction < 32 (opcode) */
    double young_neutral[VIVRE_NNEUTRES], neutral[VIVRE_NNEUTRES];
} VivreSample;

typedef struct {                          /* nés depuis le dernier relevé (âge 0, parent connu) */
    long n;
    double life, low, nz[VIVRE_NNEUTRES];
} VivreBirths;

void vivre_init(VivreState *s, uint64_t seed);
/* Avant world_step : capture, don, mort par âge. Retourne 1 si un génome a changé. */
int vivre_step(World *w, const VivreConfig *cfg, VivreState *s);
void vivre_sample_cfg(const World *w, const VivreConfig *cfg, VivreSample *out);
void vivre_sample(const World *w, VivreSample *out);
int vivre_tail(const Creature *c, int k);   /* octet len-k, 0 si absent */
int vivre_max_age(const VivreConfig *cfg, const Creature *c);
void vivre_births(const World *w, VivreBirths *b);   /* avant vivre_step : ajoute les nés du tick précédent */
int vivre_selftest(void);

#endif
