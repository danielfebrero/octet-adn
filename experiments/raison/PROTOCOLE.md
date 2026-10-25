# Raison — dix milieux vers le raisonnement

Préenregistré le 25 octobre 2026, avant la série. Moteur `experiments/raison.c`, ELF neuf, même VM que les
mondes (`src/vm.c`, ISA, RNG). Aucun monde vivant, ELF ou checkpoint existant n'est lu ni écrit. Validé par
Daniel (« je valide l'ensemble ») sur la liste des dix suites proposées le même jour.

## Ce qui change par rapport à E1 (éteint)

E1 a omis 128 décisions sur 256 et s'est éteint. Ici : **la décision ne peut pas être omise** (premier EMIT
du tick de décision, sinon A & 1) ; le **coût total d'un épisode vaut 8**, y compris le 1 par tick que la VM
prélève, pour une récompense de 20 : un aveugle (50 %) gagne +2 par épisode et la population sature le plafond
de 256. La sélection passe par la **vitesse de reproduction** (naissance à 200, parent −100, enfant 80, génome
germinal muté 1 % par octet), le plafond par un **retrait uniforme**, neutre. Fondateurs : les 16 graines
`seeds/*.adn` en 16 copies chacune, sans contrôle écrit injecté.

## Les dix milieux (bras info ; le témoin rend l'information inutile, mêmes coûts)

| # | nom | épisode | mesure principale P | témoin |
|---|---|---|---|---|
| 1 | memoire | indice fiable à 75 % au tick 0 (lecture 1), décision au tick 5 | réussite | indice au hasard |
| 2 | composition | u puis v (0..19), H = parité de u XOR v, décision tick 6 | réussite sur les 80 couples **jamais vus** | H au hasard |
| 3 | inversion | indice exact, règle cachée k qui bascule tous les 16 cycles ; retour possible par l'énergie | réussite | k au hasard à chaque épisode |
| 4 | observation | q gratuit annonce si l'indice (lecture 4) est fiable | lecture si fiable − lecture si non fiable | q sans rapport avec la fiabilité |
| 5 | patience | s annonce si attendre (+18 deux épisodes plus tard) sera payé ; prendre = +10 | attente si court − attente si long | annulation 25 % quel que soit s |
| 6 | chainage | décision 1 = déverrouiller (tick 1), indice exact tick 2, décision 2 tick 5 | P(déverrouille et juste) − ½ P(déverrouille) | paiement sans déverrouillage |
| 7 | transfert | x (0..255), H = bit 4 ; quartets bas 12..15 jamais vus | réussite sur x **jamais vus** | H au hasard |
| 8 | signal | paires ; l'émetteur voit H, son EMIT des ticks 1–2 arrive au récepteur au tick 3 | réussite du récepteur | canal remplacé par du bruit |
| 9 | autrui | paires ; l'acteur joue la mémoire, l'observateur voit son action au tick 3 | réussite de l'observateur | action montrée au hasard |
| 10 | relation | x puis y, H = (x = y) ; valeurs 192..255 jamais vues | réussite sur valeurs **jamais vues** | H au hasard |

Les contrôles écrits du selftest valident le dispositif : chaque milieu est soluble par un programme de la VM
(0,75 pour mémoire et autrui, 1 pour les autres), l'aveugle est à 0,5, le témoin ramène le contrôle à 0,5.

## Calibration (déclarée, graines 1 et 2, jamais réutilisées)

Coût total 10 : les populations s'éteignent (processus de branchement critique) ; passé à 8. Graine 1,
4 096 cycles : composition 0,79 et autrui 0,69 contre 0,5. Graine 2, 32 768 cycles : transfert 0,86, patience
écart 0,57, autrui 0,71, relation 0,59, mémoire 0,54 ; chaînage déverrouille (0,99) sans utiliser l'indice,
d'où la mesure P(déverrouille et juste) − ½ P(déverrouille), nulle si l'indice n'est pas lié au déverrouillage.

## Série

10 milieux × 2 bras × 10 graines (1001..1010), 65 536 cycles. Évaluation finale figée : `raison eval DIR`,
lecture seule du checkpoint, 128 épisodes par individu sur clones, entrées de test (jamais vues pour 2, 7, 10).

**Critère par milieu** : Δ = P(info) − P(témoin), apparié par graine (mêmes fondateurs), intervalle t à 99,5 %
(Bonferroni sur 10 milieux, 9 ddl, t = 3,690). **Tenu si la borne basse est > 0.** Chaque milieu est rapporté,
tenu ou non ; aucun milieu n'est retiré.

## Ce qu'un critère tenu veut dire, et ne veut pas dire

Tenu = la sélection a produit, dans cette population, un usage de l'information que le témoin ne produit pas.
Pour 2, 7, 10 : sur des entrées jamais vues, donc une **règle**, pas une table. Pour 4 et 5 : une décision
**conditionnelle** (s'informer, attendre). Pour 8 et 9 : de l'information qui passe **par un autre**. Pour 3 : une
adaptation **pendant la vie**. Ce n'est pas une preuve de raisonnement au sens humain : le mécanisme peut être
court (quelques instructions). L'étape suivante, si un milieu tient, est de lire le génome et d'ablater.
