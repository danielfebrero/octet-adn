# Monde v0

Quatre pièces déjà mesurées, proposées ensemble (avis Grok Bot, photo du 2026-10-22). Écrit avant la mesure.

1. Le réplicateur que la soupe fait naître en adressage B (`big_b_0`, archivé). Une copie sur l'île 0. Le bras bruit n'en a pas.
2. Chaque vivant porte sa table et la transmet quand il réécrit l'autre moitié (≥ L/2). La dérive est celle de `local-sans-W`, ralentie 64 fois : p = 0,9/512 par vivant et par époque. `join-wheel` a montré que p = 0,9/8 éteint la lignée et rend l'identité de table inatteignable.
3. Quatre îles de 64 places, 16 occupées au départ. Chaque époque, une île regagne 8 de nourriture, plafond 32. Une rencontre coûte 2. La nourriture ne dépend pas de ce que font les programmes. Un vivant part vers une place vide de l'île suivante avec p = 0,02, et emporte son génome et ses deux tables.
4. Quand la réécriture transmet la table, une entrée reprend la valeur de la table écrasée avec p = 0,25. C'est le transfert horizontal noté à la fin de `local-laws/`. L'ombre, à chaque transmission, copie la table d'un vivant tiré au hasard sur la même île, puis la même greffe depuis sa propre table écrasée.

Pas de `WRITE_LAW`. Pas de `SPLIT`. L'adressage reste B. `src/`, `include/` et `seeds/` ne bougent pas.

## Pré-enregistrement

N = 4 × 64 places, 16 vivants par île, 1024 époques, graines 0–3. Recensement final de chaque vivant (`u_copier` sous sa table). Bras bruit : graine 0, même durée, aucun fondateur.

- Copieurs en fin ≥ 16 sur 64, sur **au moins 3 graines sur 4**.
- Au moins **2 îles** portent un copieur, sur **au moins 3 graines sur 4**.
- Tables nouvelles persistantes (3 porteurs, 4 échantillons de 64 époques, hors table d'origine) : le réel est **strictement au-dessus** de l'ombre sur **au moins 3 graines sur 4**.
- Distance moyenne à la table d'origine : le réel est **plus bas** que l'ombre sur **au moins 3 graines sur 4**.
- Bras bruit : **0 copieur**.

Un seul seuil raté réfute le monde à cette échelle. On ne rajoute pas de mécanisme après coup.

## Résultats

Mesuré. 1024 époques, recensement de chaque vivant. Deux passages, même fichier. La courbe est un sondage de 16 vivants toutes les 64 époques. `greffes` additionne les entrées reprises, table réelle et ombre.

| graine | prise | copieurs | îles | espèces réel | ombre | dist réel | dist ombre | naissances |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| 0 | 64 | 63/64 | 4 (19/15/8/21) | 3 | 4 | 1,48 | 1,73 | 610 |
| 1 | 64 | 63/64 | 4 (20/11/18/14) | 3 | 1 | 1,59 | 1,61 | 480 |
| 2 | 64 | 62/64 | 4 (18/10/19/15) | 2 | 1 | 1,83 | 1,89 | 696 |
| 3 | — | 0/64 | 0 | 0 | 0 | 1,89 | 1,92 | 0 |
| bruit | — | 0/64 | 0 | 0 | 0 | 1,80 | 1,80 | 0 |

La distance vaut environ 2 entrées sur 256 : c'est le débit posé (1024 × 0,9/512 ≈ 1,8 coup par vivant). Ce n'est pas la distance de `local-laws` (centaines d'entrées).

**Verdict : non.** Un seuil est raté. Les tables nouvelles ne passent devant l'ombre que sur 2 graines sur 4.

Ce qui tient, à cette échelle : la lignée (3 graines sur 4, 62 à 63 copieurs sur 64), la colonisation des quatre îles, le bruit à 0, et une distance à l'origine plus basse que l'ombre sur 4 graines sur 4. L'écart de distance est une fraction d'entrée. La graine 3 ne copie jamais (0 naissance). Sortie : `results.txt`.
