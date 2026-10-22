# Ensemble dispersé, plus de volume

Suite d'ensembles. Le détecteur de période voit un cycle. Ici les lois restent fixes, et on demande autre chose : des octets essentiels dont les écarts ne sont pas constants, lus dans des fenêtres qui ne tombent pas sur les coupures de 64. Plus de graines.

`src/`, `include/` et `seeds/` ne bougent pas.

## Détecteur, écrit avant la mesure

Évolution comme la soupe d'origine (adressage B, aucune dérive de table). Puis, sur la bande concaténée, 64 fenêtres de 64 octets tirées au hasard, à n'importe quel décalage.

Une fenêtre qui passe `u_copier` est un hit. Hors-grille : le décalage n'est pas un multiple de 64. Sur les 8 premiers hits, l'ablation donne les positions qui réagissent. L'épine est ces octets, dans l'ordre. Les écarts sont dispersés quand ils ne sont pas tous égaux, et qu'il y en a entre 4 et 16. L'ensemble est la liste triée de ces valeurs, sans leur place.

La période (plus petit décalage avec r ≥ 0,5, consensus répété) est mesurée à côté, comme hier.

## Pré-enregistrement

- 32 graines, N = 128, 64 époques, une copie du réplicateur dans le bruit.
  - Période qui déduit un copieur sur **au moins 24 graines sur 32**.
  - Au moins une fenêtre hors-grille sur **au moins 24 graines sur 32**.
  - Épine dispersée (4 à 16 essentiels, écarts non constants) sur **au moins 24 graines sur 32**.
  - Le même ensemble trié sur **au moins 24 graines sur 32**.
- 16 graines de bruit, même durée, 64 fenêtres : **0 hit**.
- 8 cristaux `00 11 … ff` : **0 hit**.

Un seul seuil raté réfute l'ensemble dispersé à ce volume. Pas de second détecteur après coup.

## Résultats

Mesuré. 32 + 16 + 8 runs. Deux passages, même fichier. 4,4 s.

| bras | graines | période → copieur | hors-grille | écarts dispersés | même ensemble |
|---|---:|---:|---:|---:|---:|
| fondateur | 32 | 22 | 22 | 22 | 20 |
| bruit | 16 | 0 | 0 hit | — | — |
| cristal | 8 | 0 (P = 16, pas copieur) | 0 hit | — | — |

Le seuil était 24 sur 32. Il est raté de deux graines pour la période, la hors-grille et les écarts, et de quatre pour l'ensemble commun.

Là où la lignée prend (époque 32), la signature est stable. Ensemble trié `01 03 06 07 09 1e 90`. Épine `0701030906901e`. Écarts `1.1.1.3.1.1`. Ce n'est pas un pas constant. Le paquet tient sur 8 cases, pas éparpillé dans la bande. Presque tous les hits sont hors des coupures de 64 (souvent 29 hors-grille sur 30). Deux graines (25 et 30) ajoutent `1d` à l'ensemble. Dix graines ne partent pas.

**Verdict : non.** Le volume demandé ne passe pas 24/32. Le bruit (16/16) et le cristal (8/8) restent morts. Sortie : `results.txt`.
