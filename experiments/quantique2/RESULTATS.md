# Q2 — résultats de la série préenregistrée (25 octobre 2026)

ELF statique `experiments/quantique-build/20261025-q2/monde2` (sha256 `44ef273a…`, commit `1e71114`, non versionné),
80 mondes (20 graines 1001–1020 × 4 bras), 16 384 ticks chacun, tous arrivés au bout, aucun échec. Erreur de
norme du champ < 1e-13. Exécutés hors des mondes vivants, en priorité basse ; aucun monde existant touché.

## Critères principaux (contrastes appariés par graine, IC t à 97,5 %, 19 ddl)

| critère | moyenne | IC 97,5 % | graines dans le sens prédit |
|---|---:|---|---:|
| C1 : H_e(E) − H_e(H) | +0,281 | [0,262 ; 0,299] | 20/20 |
| C2 : L(E) − L(H) | +0,406 | [0,336 ; 0,475] | 20/20 |

**Les deux tiennent.** Selon la règle écrite d'avance : les ensembles Q2 sont des unités d'évolution minimales.
Ils se multiplient (fission et empreinte), transmettent une valeur héritable par l'observation et varient par
le bruit de copie.

## Par bras (moyennes des 20 graines)

| bras | H_e empreinte | L chaînes fidèles | H fission | ensembles | plus grand | ordre de phase |
|---|---:|---:|---:|---:|---:|---:|
| E empreinte, champ relaxé | 0,303 | 1,45 | 0,410 | 17,3 | 5,8 | 0,968 |
| H valeur au hasard | 0,023 | 1,05 | 0,125 | 14,8 | 3,9 | 0,232 |
| S aucune écriture | — | 0,07 | 0,404 | 15,6 | 4,4 | 0,216 |
| U empreinte, champ unitaire (Q1) | 0,108 | 1,26 | 0,245 | 15,1 | 4,4 | 0,237 |

Diagnostics (sans décision) : E − U, H_e +0,196 [0,179 ; 0,213] 20/20 et L +0,19 [0,13 ; 0,25] 19/20 ; E − S, L +1,38
20/20 ; ordre E − H +0,74, E − S +0,75, 20/20.

## Lecture

- **L'observation devient héritage.** La valeur tirée par la projection, écrite dans un octet libre, porte
  l'information de l'ensemble qui a bouclé avec le point. Avec la même fréquence d'écriture mais une valeur au
  hasard (H), il ne reste rien : H_e ≈ 0,02.
- **Le champ unitaire de Q1 transmet déjà un peu** (U : 0,11), mais il oscille sans se poser. Le relaxer par le
  même bain que les octets multiplie la fidélité par près de trois. C'est le changement qui compte le plus après l'empreinte elle-même.
- **Une phase envahit le monde** dans E (ordre 0,97) et nulle part ailleurs (≈ 0,22). C'est une course de recopie,
  pas une adaptation : aucune phase n'a d'avantage écrit dans la règle. Il n'y a donc **pas de sélection démontrée**.
- **L'hérédité reste courte.** L = 1,45 signifie des chaînes fidèles de 1 à 2 générations en moyenne (≈ 1 sous
  l'hypothèse nulle). Le bruit de copie coupe vite les lignées. La génération maximale (50–138) n'est pas une
  preuve : elle est aussi haute dans H, où les marques existent sans contenu hérité.
- Les fissions transmettent la phase dans tous les bras (le même matériau se partage). Ce n'est pas une copie,
  c'est pourquoi le critère C1 porte sur les naissances par empreinte, où la matière est neuve.

## Limites

Champ moyen, maillage fixe, relaxation non unitaire (renormalisée), queue répulsive et bain ajoutés : chaque ajout
est un réglage qui redonne Q1 à 0, mais le monde n'est plus « une seule loi » au sens strict de Q1. Pas de
métabolisme au-delà du bain, pas de conscience, pas de raisonnement.

## Reproduire

```sh
g++ -std=c++17 -O2 -Wall -Wextra -Wpedantic -Werror -static experiments/quantique2/monde2.cpp -o ELF_NEUF
ELF_NEUF selftest
sh experiments/quantique2/serie.sh ELF_NEUF RACINE_NEUVE 8
sh experiments/quantique2/analyse.sh RACINE_NEUVE
```
