/* Guidage optionnel par des marques locales, sans modification du moteur. */
#ifndef OCTET_RENDEZVOUS_H
#define OCTET_RENDEZVOUS_H

#include "octet.h"

typedef struct {
    int enabled, period, cost;
} RendezvousConfig;

/* POD : peut être sauvegardé avec le checkpoint du pilote. Les marques propres
 * proviennent uniquement d'EMIT et expirent après 24 ticks sans réémission. */
typedef struct {
    uint64_t seed, reads, moves, blocked, paid, deposited, expired;
    int64_t last_fields_tick;
    uint8_t marks[GRID_H][GRID_W], ttl[GRID_H][GRID_W];
} RendezvousState;

void rendezvous_init(RendezvousState *state, uint64_t seed);

/* Un tick VM sur copie profonde du monde, des créatures et du RNG : exige un
 * capteur réellement exécuté (SENSE/LOAD/NEURAL), puis STEP dans ce même tick.
 * Aucun effet sur le monde original ni sur le watcher de son vrai tick.
 * Ce test constate une séquence exécutée, pas une dépendance causale de STEP
 * au capteur. Il repart de l'ip et des registres courants, pas de l'octet zéro. */
int rendezvous_capable(const World *w, const Creature *c);

/* Avant la VM : un génome qui passe le tick témoin ci-dessus peut lire une
 * marque à distance Manhattan <= 4, s'orienter et payer un pas.
 * Les immédiats ne sont pas des opcodes. Aucune marque => aucun changement.
 * Une marque sous la créature supprime ce pas guidé ; la VM peut ensuite
 * repartir. Les marques
 * externes, si fournies, priment cellule par cellule sur les traces EMIT. */
void rendezvous_before(World *w, Creature *c, const RendezvousConfig *cfg,
                       RendezvousState *state,
                       const uint8_t marks[GRID_H][GRID_W]);

/* Après world_step : vieillissement des traces et dépôt des EMIT survivants.
 * out_marks est une copie optionnelle vers un tableau de travail dédié. Passer
 * NULL pour garder les marques dans state ; ne pas fournir la grille de lois
 * tierces, car cette copie remplace intégralement le tableau de destination. */
void rendezvous_fields(World *w, const RendezvousConfig *cfg,
                       RendezvousState *state,
                       uint8_t out_marks[GRID_H][GRID_W]);

/* Zéro = succès ; vérifie neutralité, opcodes/immediats, tore, coût, durée,
 * immobilité sans marque et déterminisme sans consommer le RNG du monde. */
int rendezvous_selftest(void);

#endif
