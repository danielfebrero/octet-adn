#ifndef OCTET_ECHANGE_H
#define OCTET_ECHANGE_H

#include "octet.h"

typedef struct EchangeConfig {
    int enabled, period, cost, cooldown;
} EchangeConfig;

/* Etat autonome, sans pointeur : il peut accompagner le checkpoint d'un
 * nouvel ELF sans changer Params, World ou Creature. Un etat par ile.
 * Les ids sont locaux a ce World. L'historique garde les CMAX echanges
 * individuels les plus recents ; chaque entree porte le dernier succes.
 */
typedef struct EchangeState {
    Rng rng;
    long passages;
    long contacts, intra, inter;
    long attempts, changes, bytes_changed;
    long energy_refused, cooldown_refused;
    int n_history;
    int ids[CMAX];
    long last_exchange[CMAX];
} EchangeState;

void echange_init(EchangeState *state, uint64_t seed);
void echange_step(World *world, const EchangeConfig *config, EchangeState *state);
int echange_selftest(void);

#endif
