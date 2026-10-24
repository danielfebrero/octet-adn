# Tronc

Un monde où les lois déjà vivantes tournent ensemble, plus les trois
stimulations, plus ce qu'on y déposera ensuite.

Allumé au départ : forces 3, stress, attention, traits, dépense,
causales, saison douce (amplitude 0,20, période 40 000). Parent :
vie-traits. Binaire : `experiments/terrarium_tronc`. Les sept processus
déjà lancés gardent leur binaire.

## Brancher une expérience

Déposer un `quelque-chose.so` dans `experiments/vie-tronc/lois/`.
Au relevé suivant (200 ticks), le processus le charge avec `dlopen` et
appelle `loi_registre`. Le contrat est `loi.h` : version 1, un
échantillon, et des accès à la nourriture, aux briques, à la chaleur,
à une marque. Rien à relinker. Un fichier illisible est noté
`loi_rejet` et le monde continue.

`exemple.c` s'annonce une fois. Il n'est pas dans `lois/` tant qu'on
ne l'y copie pas.

Les expériences plus anciennes (soupe, compétition, recherche
d'univers, mémoire) ne sont pas rejouées dans ce processus. Elles
entrent le jour où un `.so` les porte.
