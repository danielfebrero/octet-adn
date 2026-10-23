# Sonde de phase : le détecteur de réparation, contrôlé avant toute mesure de vie

> Simulation dans une VM, pas une conscience ni une vie libre.

## La phrase (écrite avant les chiffres)

Deux jumeaux d'une même bande courent la même exécution. On lèse la zone 16..31 du blessé à une
phase tirée dans 0..4095, et l'intact ne l'est pas. Une réparation n'est déclarée que si la zone du
blessé égale celle de l'intact à l'horizon de 8192 pas, et si les `STORE` du blessé, après la
lésion, ont réécrit chaque case blessée avec la valeur que l'intact y écrit. Ce détecteur doit dire
0 sur 96 témoins négatifs et reconnaître le motif soupe à au moins 3 phases fixes sur 4.

Décidé par le débat a2946faf. Cette sonde ne mesure pas la barre 28/32.

## Dispositif

- Moteur copié de `experiments/seul.c` : bande de 64, adressage B, `exec64`, budget 8192, capteurs
  nuls, arrêt sur `YIELD` ou sortie de bande. Le moteur pas à pas est vérifié contre `exec64` de
  `seul.c` sur 64 bandes, octet pour octet et compte de `STORE` pour compte de `STORE`.
- Détecteur figé dans `experiments/sonde_phase.c` avant tout compte. La zone est fixe (cases 16..31,
  une période du motif) et la même pour toutes les bandes. Chaque case de la zone reçoit une autre
  valeur (graine de lésion imprimée). D est l'ensemble des cases de la zone qui diffèrent juste après
  la lésion. Réparation si (0) D n'est pas vide, (1) la zone est égale à l'horizon, et (2) chaque
  case de D a été réécrite par le blessé après la lésion, l'a été aussi par l'intact après le même
  pas, avec la même dernière valeur.
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

- le motif-artefact, c'est-à-dire le même motif avec l'immédiat `0x10` à la place de `0x90` (la
  variante sans modulo de `experiments/motif-artefact`), aux mêmes quatre phases ;
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

Deux runs donnent un `results.txt` identique, octet pour octet.

| ligne | réparations déclarées | attendu | tenu ? |
|---|---:|---|---|
| inerte | 0 / 32 | 0 | oui |
| aléatoire | 0 / 32 | 0 | oui |
| cristal | 0 / 32 | 0 | oui |
| **témoins négatifs** | **0 / 96** | 0 | oui |
| **positif, motif `0x90`** | **1 / 4** (phase 2048 seule) | ≥ 3 | **non** |
| à part : motif-artefact `0x10` | 1 / 4 (phase 2048 seule) | — | — |
| à part : motif sans lésion | 0 / 32 | 0 | oui |

**Sonde : échec, le détecteur est trop strict.** Il ne déclare aucune fausse réparation, mais il ne
reconnaît le motif soupe qu'à une phase sur quatre. Ce n'est pas un résultat de vie.

Selftest : moteur = `exec64` OK ; positif 1/4, ÉCHEC ; 0 réparation sur les 12 négatifs à phases
fixes (restaurateur sans `STORE`, bruit de graine 1, cristal de graine 1), OK ; 0 sur 4 pour le motif
sans lésion, OK. Le selftest sort 1, donc `make test` échoue sur cette branche. Tous les contrôles
qui le précèdent passent (46/46 tests du cœur et les selftests des autres essais).

Ce que montrent les quatre phases du positif (diagnostic hors binaire, détecteur inchangé) :

| phase | B mod 64 à la lésion | écritures du blessé dans la zone | à l'horizon |
|---:|---:|---:|---|
| 0 | 0 | 16 | les cases 0, 16, 32 et 48 portent `45` au lieu de `05` |
| 1024 | 29 | 3 | 14 cases du code 0..15 diffèrent |
| 2048 | 58 | 16 | tout est revenu : réparation déclarée |
| 3072 | 23 | 0 | 2 cases du code diffèrent |

Le motif recopie la case j vers j + 16 à mesure que B avance. Quand la lésion tombe pendant que B
balaie la zone (phases 1024 et 3072), il recopie les octets lésés vers l'avant, jusque dans son
code. La case 16 est aussi la case mortelle de la carte de `motif-artefact`. À la phase 0, son
octet lésé est recopié en 32, puis en 48, puis dans le code en 0, avant que la case 16 ne soit
réécrite depuis la case 0 : l'atteinte a fait le tour des quatre périodes.

Sur les témoins négatifs, aucune zone n'est égale à l'horizon. Un seul témoin écrit dans la zone
après la lésion : l'inerte 3 (graine `0x50480003`, phase 2489). Sa bande n'a aucun `0x06`, mais la
lésion en pose un dans la zone, qui est du code chez les inertes, et 143 `STORE` s'exécutent
ensuite. La zone ne revient pas, la réparation n'est pas déclarée.

Aucun réglage du détecteur après ces comptes : changer la zone ou les phases est une nouvelle version,
à décider hors de cette sonde.
