# Trois approches pour des mondes longs : capture, parenté, sénescence

> Simulation dans une VM, pas une conscience ni une vie libre.

Approches Claude (Opus 5.5), 25 octobre 2026, ordre de Daniel : un quinzième monde et trois
approches dans de nouveaux mondes. Code : `vivre.c`, `vivre.h`, profils `capture`, `parente`,
`senescence` du pilote `rencontres.c`. `src/`, `Params` (416 octets), `World`, `Creature` et
l'ISA ne changent pas. Aucun nouvel opcode. Chaque levier a son RNG et ne tire rien quand il est
éteint (selftest). Le format du trailer de checkpoint passe en version 2 ; les ELF déjà en
marche gardent leur version 1 et leurs mondes.

## Les trois leviers

| profil | ce qui est posé | ce qui est hérité et peut évoluer |
|---|---|---|
| `capture` | Tous les 16 ticks, un génome qui porte `ATTACK` comme opcode prend à un voisin (Chebyshev ≤ 1, ADN différent) au plus 8 octets, en bouclant. La proie perd 4 d'énergie et garde son ADN ; le chasseur paie 2 et gagne 2. Une proie à ≤ 4 n'est jamais tuée pour copier. | Les octets capturés entrent dans le soma et le germen du chasseur, donc dans sa descendance. |
| `parente` | Tous les 16 ticks, un vivant à plus de 60 d'énergie, voisin d'un affamé (< 40), lui donne 10 avec la probabilité `octet(len-1)/255`. Le don coûte 11 au donneur. | La propension elle-même : rien n'oblige à donner. |
| `senescence` | Une créature meurt à l'âge `100 + 8 × octet(len-2)` (100 à 2140 ticks). | L'âge maximal. |

Dans chaque monde, `octet(len-3)` n'a aucun effet : c'est le **témoin de dérive**, soumis aux
mêmes mutations du moteur.

## Ce qui sera lu dans `vivre.tsv`

Colonnes : `tick pop captures capture_bytes capture_refused capture_dist_mean donations donated
donation_refused old_deaths give_byte life_byte neutral_byte frac_attack frac_xor`.

Déclaré avant les mondes longs : un levier **porte une sélection** si l'octet qui le règle
(`give_byte` pour la parenté, `life_byte` pour la sénescence) s'écarte durablement du témoin
`neutral_byte` du même monde, dans le même sens, sur au moins trois relevés de 200 000 ticks
d'affilée. Un écart qui change de signe ou qui disparaît reste de la dérive. Pour la capture, la
lecture est `capture_dist_mean` : si la distance entre chasseur et proie au moment de la capture
baisse durablement, l'ADN circule et la population converge ; sinon la capture n'est qu'un
prélèvement d'énergie. Un monde seul ne vaut pas une loi : ces mondes sont des observations,
pas un test à plusieurs graines.

## Premier passage court (graine 101, 6000 ticks, monde de mesure du pilote)

| profil | pop finale | compteurs | octet réglé | témoin neutre |
|---|---:|---|---:|---:|
| capture | 160 | 48 340 captures, 203 415 octets copiés, distance moyenne 225,8 | — | 111,8 → 55,4 |
| parente | 160 | 1 607 dons (16 070 d'énergie), 1 062 refus par propension | 147,2 → 149,6 | 126,7 → 153,1 |
| senescence | 159 | 548 morts d'âge | 128,6 → 219,9 | 173,1 → 23,9 |

Une seule graine courte : rien n'est conclu. Deux faits de dispositif : `frac_attack` vaut
0,99 à 1,00 dès le départ (`ATTACK` est un octet commun des génomes fondateurs, comme l'était
la porte statique des rendez-vous) ; la capture teste donc la circulation de l'ADN, pas la
diffusion d'un trait chasseur.
