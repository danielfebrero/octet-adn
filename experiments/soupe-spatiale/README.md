# Soupe spatiale : rencontrer ses voisins au lieu de tout le monde

> Simulation d'évolution dans une VM, pas une conscience ni une vie libre.

## L'approche, en une phrase (écrite avant la mesure)

Une soupe d'octets où chaque programme ne rencontre que ses quatre voisins, sur une grille torique
de 64 × 64, fait naître un réplicateur héréditaire à N = 4096, taille où la soupe mélangée n'en fait
naître aucun.

## Pourquoi c'est neuf dans ce dépôt

Toutes les soupes mesurées apparient leurs programmes **au hasard dans toute la population** :
`soup-replicator` (`src/soup.c`), `universe-search` et ses suites (`sim`), `dispers-octet`,
`ensembles-octet`. `monde-v0` fait de même dans chacune de ses quatre îles de 64 places, et
`incubation-stepback` garde la même paire 8 époques, toujours tirée au hasard. Le `terrarium` est
spatial, mais il se reproduit par `SPLIT`, ou par copie depuis des fondateurs déjà nés dans une
soupe mélangée (`soup-terrarium`). Aucune mesure n'a demandé si la **localité des rencontres**
fait naître un réplicateur du bruit, là où la taille seule ne suffit pas : `soup-replicator` donne
0/8 à N = 4096 et 2/4 à N = 16384, et le cadre `universe-search` donne 0/12 à N = 4096 pour
l'univers d'origine.

## Dispositif

- Physique figée : l'univers d'origine (`u_origin` de `universe_search.c`), c'est-à-dire la soupe
  de `src/soup.c` en adressage B, L = 64, 8192 pas, mutation de fond 2,4e-4 par octet et par
  époque. Aucune dérive de table, aucun opcode nouveau. `src/`, `include/` et `seeds/` ne bougent pas.
- Même soupe initiale (bruit uniforme) pour les deux bras d'une même graine.
- **Bras G (grille).** Les 4096 programmes sont les cases d'un tore 64 × 64. À chaque époque, on tire
  une orientation (horizontale ou verticale) et une parité. Chaque case est appariée avec exactement
  une voisine (pavage en dominos). L'ordre des deux moitiés sur la bande est tiré à pile ou face.
  Tout le reste est la boucle `sim` du cadre : exécution depuis ip = 0, recoupe, mutation de fond.
- **Bras M (mélange).** La fonction `sim` du cadre, inchangée : permutation aléatoire à chaque époque.
- Naissance, copieur vérifié et ablation : ceux du cadre (`u_copier` : copie fidèle ≥ 56/64 sur
  3 bruits sur 4, et la copie copie aussi). Échantillon de 64 programmes toutes les 64 époques.
- Les réplicateurs homopolymères (≤ 2 valeurs d'octet distinctes) sont des cristaux : ils ne
  comptent pas.

## Barre, déclarée avant les chiffres

Une graine **passe** si un réplicateur non cristallin atteint ≥ 0,25 de copieurs vérifiés dans
l'échantillon à un point de contrôle, et s'il y est **encore** à la dernière époque (16384). Cela
exige une copie qui se transmet sur deux générations (`u_copier`) et une lignée qui tient, pas un
état figé.

| bras | graines | époques | seuil |
|---|---:|---:|---|
| G, grille 64 × 64 | 32 | 16384 | **≥ 8 graines sur 32** passent |
| M, mélange, mêmes soupes initiales | 32 | 16384 | **≤ 2 graines sur 32** passent |
| bruit : G sans `STORE` | 8 | 16384 | **0 naissance, 0 copieur** sur 8 |
| cristal `00 11 … ff` répété | — | — | **pas copieur** (`u_copier` = 0) ; le détecteur exclut les homopolymères |
| contrôle positif : G, 16 copies de `big_b_0` | 8 | 2048 | invasion ≥ 0,25 en fin sur **≥ 6 sur 8**, sinon le dispositif est invalide |

Le cristal ne se prête pas à un bras dynamique ici : avec la mutation de fond, chaque octet est
remplacé environ 3,9 fois en 16384 époques, et la soupe cristalline redevient du bruit. Il est donc
testé au niveau du détecteur.

**Oui** seulement si les cinq lignes passent. Un seul seuil raté réfute l'approche à ce volume. Pas de
second mécanisme après les mesures.
