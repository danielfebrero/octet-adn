# Nouveauté au lieu de la tâche : MAP-Elites sur `general` (verdict : négatif pour l'émergence)

**Question.** Une pression de nouveauté sur le comportement, au lieu de la pression de tâche, fait-elle apparaître la mémoire 2 / 3 tenue à l'écart comme effet de bord ? Le critère est `gn_check` : ≥ 0.95, puis une chute d'au moins 0.5 gelé et sans récompense.

**Réponse : non.** Sur 4 seeds × 2 grilles × 50 000 itérations, aucun génome ne passe `gn_check` (0 passeur). Le meilleur m2 tenu à l'écart jamais vu est 0.528 et le meilleur m3 0.505. La seed part de 0.366 / 0.401, le hasard est à 0.25. C'est le même plafond que la sélection directe (`experiments/general-joint-negative` : 0.50–0.54).

En revanche, MAP-Elites monte plus haut que le hill-climbing de même budget, qui reste à 0.41. Il le fait **en sacrifiant le curriculum** : les élites à meilleure mémoire ont q = 0.34–0.44, contre 0.508 pour la seed. La couverture n'est pas saturée à 50 000 itérations.

## Ce qui est implémenté (sans toucher la sélection historique)

- `gn_behavior(g, d)` (labs_h.c, nouvelle fonction) rejoue 8 vies sur des tâches d'entraînement fixes : 4 tirages du niveau 1, plus mémoire 2 et mémoire 3 deux fois chacune. Elle mesure, **sans aucune note de tâche** :
  - l'entropie de la réponse (5 symboles : 0..3 et silence) ;
  - la fraction d'essais émis ;
  - I(réponse ; indice il y a L essais), pour L = 0..3 ;
  - les STORE par essai ;
  - les octets du soma ≠ génome en fin de vie (profondeur de réécriture) ;
  - les codons réécrits puis exécutés.
- `src/novelty.c` porte le MAP-Elites. Trois grilles :
  - `agn` (agnostique, 512 cases) : entropie de réponse (8) × STORE/essai (8, log) × dérive du soma (8, log) ;
  - `mem` (320 cases) : retard dominant de la dépendance réponse/indice (aucun, 0, 1, 2, 3) × force en bits (8) × STORE/essai (8). Cette grille est « informée » : elle récompense la diversité de retard, pas la justesse ;
  - `none` (1 case) : témoin. C'est un hill-climbing élitiste strict, avec le même budget, les mêmes mutations et la même qualité.
- **Qualité** : `gn_train(g, 1, 0)`, le curriculum d'entraînement. Le jeu tenu à l'écart n'entre jamais dans la sélection.
- **Mutation** : `mutate()` aux taux de `hl_gn_cfg` (0.005 / dup 0.01), sans opérateurs de naissance. L'initialisation se fait avec 32 mutants de la seed `general`. Un parent est tiré uniformément parmi les cases occupées. Un enfant prend sa case s'il est strictement meilleur.
- **Mesure seule** : chaque élite inséré est rejoué sur m2 et m3 tenus à l'écart (`gn_eval` held, 40 tâches). S'il atteint 0.95 sur l'un des deux, il passe le `gn_check` complet.
- Sous-commande : `./octet-adn novelty --grid agn|mem|none --seed S --iters N --report R --init K`. La sortie est déterministe, avec un hash d'archive en fin de run.

## Résultats (50 000 itérations, seeds 0–3)

| grille | seed | couverture finale | QD-score | best q | max m2 held-out | max m3 held-out | passeurs |
|---|---|---|---|---|---|---|---|
| agn | 0 | 354/512 (0.69) | 103.98 | 0.5386 | 0.487 | 0.452 | 0 |
| agn | 1 | 354/512 (0.69) | 105.88 | 0.5166 | **0.528** | 0.495 | 0 |
| agn | 2 | 338/512 (0.66) | 100.40 | 0.5337 | 0.449 | 0.486 | 0 |
| agn | 3 | 364/512 (0.71) | 112.08 | 0.5244 | 0.495 | **0.505** | 0 |
| mem | 0 | 228/320 (0.71) | 69.84 | 0.5327 | 0.423 | 0.440 | 0 |
| mem | 1 | 203/320 (0.63) | 61.74 | 0.5278 | 0.470 | 0.462 | 0 |
| mem | 2 | 219/320 (0.68) | 63.71 | 0.5327 | 0.419 | 0.459 | 0 |
| mem | 3 | 189/320 (0.59) | 56.70 | 0.5303 | 0.466 | 0.463 | 0 |
| none | 0 | 1/1 | 0.564 | 0.5640 | 0.395 | 0.412 | 0 |
| none | 1 | 1/1 | 0.559 | 0.5591 | 0.377 | 0.408 | 0 |
| none | 2 | 1/1 | 0.561 | 0.5610 | 0.406 | 0.405 | 0 |
| none | 3 | 1/1 | 0.551 | 0.5513 | 0.412 | 0.402 | 0 |

« max m2/m3 » est le maximum sur **tous** les élites insérés pendant le run (1 500–1 750 pour agn, 1 100–1 470 pour mem, 25–34 pour none). Avec 40 tâches × 32 essais, l'écart-type d'une mesure est d'environ 0.014. Le maximum de ~1 500 tirages gonfle d’environ +0.045 : les 0.45–0.53 sont réels, pas du bruit.

**Courbes d'ouverture** (couverture / QD-score, moyenne des 4 seeds) :

| itérations | agn couverture | agn QD | mem couverture | mem QD |
|---|---|---|---|---|
| 1 000 | 0.146 | 17.2 | 0.170 | 12.9 |
| 5 000 | 0.285 | 36.9 | 0.327 | 26.4 |
| 10 000 | 0.385 | 52.2 | 0.423 | 35.4 |
| 20 000 | 0.528 | 74.5 | 0.530 | 47.5 |
| 50 000 | 0.688 | 105.6 | 0.655 | 63.0 |

La couverture croît encore entre 20 000 et 50 000 itérations, sans plateau. Au sens « l'espace de comportements continue de s'ouvrir », le système est ouvert. La qualité, elle, ne suit pas.

## Lecture

1. **Pas d'émergence.** Il y a 0 passeur sur 8 runs de nouveauté et 4 témoins. L'écart avec le seuil reste énorme : 0.53 contre 0.95.
2. **La nouveauté explore mieux que la tâche, mais vers le même plafond.** MAP-Elites dépasse le hill-climbing de +0.04 à +0.15 sur m2/m3 (même seed) tenus à l'écart. Pourtant, son meilleur q est plus bas (0.52–0.54 contre 0.55–0.56). Les génomes à meilleure mémoire sont des élites à q faible (0.34–0.44) : ils déplacent la dépendance réponse/indice vers p2/p3 et perdent l'association courante. C'est le même compromis que dans `general-joint-negative`.
3. **La grille informée (`mem`) ne fait pas mieux que l'agnostique.** Son max m2/m3 est de 0.42–0.47, contre 0.45–0.53 pour `agn`. Avoir une case « retard 2/3 » ne suffit pas. Il faut un mécanisme de mémoire à deux registres, qui exige plusieurs octets coordonnés : la nouveauté ne le rend pas plus probable qu'une mutation ponctuelle.

## Reproduire

```console
make
for g in agn mem none; do for s in 0 1 2 3; do
  ./octet-adn novelty --grid $g --seed $s --iters 50000 --report 1000 > nov_${g}_$s.txt
done; done
```

Les sorties brutes, avec leurs hash, sont dans `results.txt`. Un run de 50 000 itérations prend 4 à 8 min sur une machine chargée. `make test` vérifie le déterminisme et les bornes des descripteurs (test `test_novelty_map_elites`).

## Prochaine expérience utile

Il faudrait garder l'archive `agn`, mais définir la qualité par case comme la **mémoire la plus profonde atteinte sur les tâches d'entraînement**, c'est-à-dire `gn_mem`, sur tables non réservées. Des mutations « macro » (duplication d'un bloc STORE/LOAD existant, qui crée un second registre) devraient être tirées au lieu des mutations ponctuelles. Le plafond 0.5 vient de l'absence d'un second registre fonctionnel, pas du manque de diversité.
