/* Reproduction locale différée, réservée au nouvel exécutable rencontres.
 * Aucun changement de WorldConfig, Creature ou Params ; état intégralement POD. */
#ifndef RENCONTRES_NAISSANCE_H
#define RENCONTRES_NAISSANCE_H
#include "octet.h"

#define NAISSANCE_VERSION 1
#define NAISSANCE_EGGS 32

typedef struct {
    int enabled, period, cost, food_cost, delay, season_period, season_open, cap;
    double mutation;              /* substitutions seulement : adresses et longueur conservées */
} NaissanceConfig;

typedef struct {
    long due;
    int x, y, parent_a, parent_b, species, len, energy, crossed;
    uint8_t dna[MAX_LEN];
} NaissanceEgg;

typedef struct {
    uint32_t version;
    Rng rng;                     /* ne consomme jamais l'aléa de la VM */
    NaissanceEgg pending[NAISSANCE_EGGS];
    int npending;
    int cooldown_id[CMAX], ncooldown;
    long cooldown_until[CMAX];
    long eggs, hatched, crossed;  /* conceptions ; éclosions ; éclosions recombinées AVANT mutation */
    long refused_cap, refused_place, refused_energy, refused_food, refused_season;
    long refused_partner, refused_cooldown, refused_queue, refused_config;
    long energy_paid, food_paid;
} NaissanceState;

void naissance_init(NaissanceState *s, uint64_t seed);
/* Après world_step, avant le classement des nouveau-nés par genotype().
 * cap est le vrai plafond, même si le runner réserve des places pendant la VM.
 * Une conception au plus par période ; deux parents vivants, distincts, à distance
 * de Chebyshev torique <= 1. Les parents restent vivants après paiement.
 * Les oeufs attendent sans remboursement ni nouveau paiement ; une éclosion
 * exige encore une place sous cap ET une case voisine du nid entièrement vide.
 * Saison = restriction des conceptions ; les oeufs déjà payés continuent à mûrir.
 * refused_cap/place comptent les oeufs mûrs bloqués à chaque appel, pas des
 * oeufs perdus ; les autres refus comptent des fenêtres de conception. */
void naissance_step(World *w, const NaissanceConfig *cfg, NaissanceState *s);
/* Retourne le nombre d'échecs ; ne lance et ne remplace aucun terrarium. */
int naissance_selftest(void);
#endif
