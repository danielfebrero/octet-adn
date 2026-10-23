# Sonde de phase : le détecteur de réparation, contrôlé avant toute mesure de vie

> Simulation dans une VM, pas une conscience ni une vie libre.

## La phrase (v2, écrite avant le nouveau compte)

La v1 lésait d'un coup les cases 16..31. Sur un réplicateur de période 16, cela détruit la source
de la copie : le motif soupe ne tenait qu'à 1 phase sur 4, sans aucune fausse réparation sur 96.
La correction lèse une seule case, la cible du prochain `STORE` de l'intact, à une phase dans
0..4095. La réparation est déclarée si la période de 16 qui contient cette case revient, chez les
deux jumeaux, à sa valeur d'avant la lésion, et si les deux l'ont réécrite par un `STORE` avec
cette valeur. Graines neuves, préfixe `0x504C`. Les quatre phases du positif restent 0, 1024,
2048 et 3072. Seuil inchangé : 0 sur 96, et au moins 3 phases sur 4.

Décidé par le débat a2946faf. Cette sonde ne mesure pas la barre 28/32.

## Dispositif

- Moteur copié de `experiments/seul.c` : bande de 64, adressage B, `exec64`, budget 8192, capteurs
  nuls, arrêt sur `YIELD` ou sortie de bande. Le moteur pas à pas est vérifié contre `exec64` de
  `seul.c` sur 64 bandes, octet pour octet et compte de `STORE` pour compte de `STORE`.
- Détecteur v2 figé dans `experiments/sonde_phase.c` avant le nouveau compte. La case lésée est la
  cible du prochain `STORE` de l'intact. S'il n'écrit plus, il n'y a pas de blessure. Réparation si
  (0) la case diffère, (1) sa période de 16 est revenue chez les deux à la valeur d'avant la lésion,
  (2) les deux y ont fait un `STORE` après, avec cette valeur.
- Le détecteur ne lit aucune étiquette de classe.

Témoins négatifs, 32 par classe, phase tirée dans 0..4095 (graines dans `results.txt`) :

| classe | construction |
|---|---|
| inerte | le restaurateur de `seul.c` avec ses `STORE` remplacés par `NOP` ; 31 variantes du même gabarit, constantes, adresses et remplissage tirés, aucun octet `0x06` |
| aléatoire | 64 octets uniformes, aucun motif planté |
| cristal | une unité de 16 octets répétée quatre fois, aucun octet `0x06` : la zone lésée est coupée de tout `STORE` |

Positif : le motif soupe `05 1d 03 02 1c 1f ff 07 01 03 09 03 c9 06 90 1e` répété quatre fois, aux
phases fixes 0, 1024, 2048 et 3072. Au moins 3 sur 4 doivent être reconnues. En dessous, le
détecteur est trop strict : c'est un échec de sonde, pas un résultat de vie.

À part, hors des 96 et hors du 3/4 :

- le motif-artefact, machine sans modulo (`LOAD`/`STORE` ignorés quand `B+k` sort de 0..63) : l'immédiat
  `0x90` ne peut plus écrire, l'immédiat `0x10` le peut encore. Mêmes quatre phases. Hors score ;
- 32 témoins sans lésion : le motif `0x90`, phase tirée, aucune atteinte. Attendu : 0.

Une seule réparation déclarée sur les 96 rejette la version.

## Commande

```console
make experiments/sonde_phase
./experiments/sonde_phase selftest      # code non nul si un contrôle à phase fixe rate
./experiments/sonde_phase run           # écrit experiments/sonde-phase/results.txt
```

`make test` lance le selftest, en dernier.

## Résultats

v2, graines `0x504C`, après la correction écrite ci-dessus. Sortie brute : `results.txt`.

| ligne | réparations déclarées | attendu | tenu ? |
|---|---:|---|---|
| inerte | 0 / 32 | 0 | oui |
| aléatoire | 0 / 32 | 0 | oui |
| cristal | 0 / 32 | 0 | oui |
| **témoins négatifs** | **0 / 96** | 0 | oui |
| **positif, motif `0x90`** | **4 / 4** | ≥ 3 | oui |
| à part : `0x90` sans modulo | 0 / 4 | — | — |
| à part : `0x10` sans modulo | 1 / 4 | — | — |
| à part : motif sans lésion | 0 / 32 | 0 | oui |

**Sonde valide.** Le détecteur reconnaît le motif soupe aux quatre phases et ne déclare aucune
réparation sur les 96. Sans modulo, l'immédiat `0x90` n'écrit plus : 0 sur 4. L'immédiat `0x10`
reste à part, 1 sur 4, et n'entre ni dans les 96 ni dans le 4/4.

Selftest : moteur = `exec64` OK ; positif 4/4 OK ; 0 réparation sur les 12 négatifs à phases fixes
OK ; 0 sur 4 sans lésion OK. Le selftest sort 0.

La barre 28/32 n'est pas mesurée ici.
