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

## Barre 28/32

Détecteur v2 inchangé. Trente-deux phases neuves, graine `0xB72820261023`, tirées avant le compte.
Seuil écrit avant : au moins 28 réparations.

```console
./experiments/sonde_phase barre
```

| | |
|---|---:|
| réparations | 31 / 32 |
| seuil | 28 |
| ratée | indice 20, phase 1373 |

**Barre tenue.** La suite du débat est les variantes héritables, pas un nouveau réglage du détecteur.

## Fuzz : variantes héritables

Détecteur v2 inchangé. On part du motif soupe fixé (immédiat `0x90`, deux `exec64`, point fixe, 744 `STORE`). Chaque octet est remplacé par chacune des 255 autres valeurs : 16 320 essais. Une forme est héritable si elle se fixe, si la bande diffère encore du parent, et si un exemple repasse la barre 28/32 sur les mêmes 32 phases (graine `0xB72820261023`). L'exemple d'une case est la plus petite valeur qui reste. Seuil écrit avant le compte : au moins 2 formes.

```console
./experiments/sonde_phase fuzz
```

| issue | nombre |
|---|---:|
| guéries, reviennent au parent | 12 256 |
| ne se fixent pas | 173 |
| fixées sans `STORE` | 1 087 |
| cases qui gardent une mutation | 16 |
| exemples ≥ 28/32 | 12 |

| case | rôle dans la période | allèles | exemple | barre |
|---:|---|---:|---:|---:|
| 0 | `CLR_A` | 3 | `04` | 32/32 |
| 1 | `SENSE_HIST` | 252 | `00` | 29/32 |
| 2 | immédiat | 254 | `01` | 31/32 |
| 3 | `DEC_A` | 250 | `01` | 31/32 |
| 4 | `LOADI` | 251 | `01` | 31/32 |
| 5 | immédiat | 254 | `01` | 31/32 |
| 6 | intron | 243 | `01` | 31/32 |
| 7 | `LOAD` | 4 | `04` | 32/32 |
| 8 | immédiat | 35 | `02` | 0/32 |
| 9 | `INC_B` | 239 | `00` | 32/32 |
| 10 | `JZ` | 8 | `04` | 26/32 |
| 11 | immédiat | 255 | `00` | 31/32 |
| 12 | intron | 236 | `04` | 32/32 |
| 14 | immédiat `90` | 19 | `00` | 9/32 |
| 15 | `LOOP` | 252 | `01` | 0/32 |
| 16 | octet 0 de la période suivante | 249 | `01` | 31/32 |

La case 13 (`STORE`) ne garde aucun allèle. Les cases 17 à 63 non plus. Quatre exemples restent sous 28 : cases 8, 10, 14 et 15. Sortie brute : `fuzz.txt`.

**Au moins deux formes héritables.** Douze exemples passent. Le détecteur n'a pas été retouché. La suite du débat est les 32 graines en parallèle.
