# Artefact du motif : écho de période, ou organisation

> Simulation dans une VM, pas une conscience ni une vie libre.

## L'approche, en une phrase (écrite avant la mesure)

Le 48/15 du motif de la soupe, posé seul, est l'écho d'une période de 16 : un `STORE` recopie
l'octet d'il y a 16 cases, le modulo fait tomber `0x90` dans la bande, et `LOOP` paie les tours.
Couper l'un de ces trois ressorts fait tomber le motif. Le restaurateur écrit à la main, qui
réécrit des constantes, tient.

## Dispositif

Même sonde que `experiments/seul.c` : bande de 64, adressage B, point fixe, au moins un `STORE`,
au moins 3 valeurs, au moins 8 cases réparées (3 substitutions sur 4), au moins 1 case mortelle,
puis les capteurs à 7 et au numéro du pas. `src/`, `include/` et `seeds/` ne bougent pas.

| levier | coupe | écho si |
|---|---|---|
| période | un `STORE` n'écrit pas quand la valeur est déjà l'octet 16 cases en amont | le motif `0x90` ne passe pas, le restaurateur passe |
| wrap | `LOAD`/`STORE` seulement si `B+k` est déjà dans 0..63 ; les sauts restent modulo | le motif `0x90` ne passe pas, le même immédiat réécrit `0x10` passe, le restaurateur passe |
| tours, reboot | `LOOP` s'arrête au lieu de remettre ip à 0, budget 8192 | le motif ne passe pas, le restaurateur passe |
| tours, budget | `LOOP` intact, budget 48 | le motif ne passe pas, le restaurateur passe |
| carte | sonde normale, capteurs nuls | aucune case réparée dans 0..15, aucune case mortelle dans 16..63, au moins 8 réparées dans 16..63, au moins 1 mortelle dans 0..15 |

Base, sinon le dispositif est invalide : le motif et le restaurateur passent la sonde normale ;
le copieur fait main, le cristal `0x11` et le bruit de graine 1 ne passent pas.

Un levier dont le restaurateur tombe est nul. Il ne compte ni comme écho ni comme réfutation.
**Verdict écho** seulement si la base tient, la carte tient, et les quatre leviers sont des échos.
Sinon, si aucun levier n'est nul : **pas un écho**.

## Reproduire

```
make experiments/motif_artefact
./experiments/motif_artefact > experiments/motif-artefact/results.txt
```

## Résultats

Base valide. Sortie brute : `results.txt`.

| bande, sonde normale | réparées | mortelles | passe |
|---|---:|---:|---|
| motif `0x90` | 48 | 15 | oui |
| restaurateur | 12 | 52 | oui |
| copieur fait main | 0 | 64 | non |
| cristal `0x11` | — | — | non |
| bruit, graine 1 | — | — | non |

| levier | motif | restaurateur | lecture |
|---|---|---|---|
| période, `STORE` étouffé | actif 0, 0 réparée | 12 / 52, passe | écho |
| wrap, sans modulo | `0x90` tombe ; `0x10` passe, 48 / 16 | passe | écho |
| `LOOP` s'arrête | 1 réparée | passe | écho |
| budget 48 | 4 réparées | passe | écho |

Carte, capteurs nuls. Retours par case : la case 0 revient 3 fois, les cases 1 à 5 reviennent 0 fois, la case 6 revient 1 fois, les cases 7 à 16 reviennent 0 fois, les cases 17 à 63 reviennent 4 fois.

| zone | réparées | mortelles |
|---|---:|---:|
| 0..15 | 1 | 14 |
| 16..63 | 47 | 1 |

La case 0 revient 3 fois sur 4. La case 6 revient 1 fois. La case 16 est mortelle. La barre demandait zéro réparée dans 0..15 et zéro mortelle dans 16..63. Elle rate d'une case de chaque côté.

**Verdict : pas un écho.** Les quatre leviers sont des échos. La carte, écrite avant la mesure, ne tient pas. La conjonction demande les deux.
