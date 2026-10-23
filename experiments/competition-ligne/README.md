# Compétition entre lignées

> Simulation dans une VM, pas une conscience ni une vie libre.

Daniel ouvre la porte malgré 7/32 (bande du milieu du débat a2946faf).

## La phrase (écrite avant le compte)

Six réplicateurs qui ont occupé un bocal de la soupe 32 — graines 0, 2, 13, 17, 24 et 29 —
sont mélangés à parts égales. La graine 10 est hors jeu : un copieur un instant, puis plus rien.
Même physique que la soupe : adressage B, N = 16384, mutation 0,00024, 4096 époques, report 256.
Chaque copieur vérifié est attribué à la lignée fondatrice la plus proche (distance de Hamming).
Graine du mélange : `0xC02E20261023`. `src/soup.c` ne bouge pas.

Portes, écrites avant le compte :

| verdict | condition à la dernière époque |
|---|---|
| monopole | une lignée ≥ 50 % des copieurs, aucune autre ≥ 10 % |
| coexistence | au moins deux lignées ≥ 10 % |
| indécis | le reste |

```console
make experiments/competition
./experiments/competition selftest
./experiments/competition mix      # écrit experiments/competition-ligne/mix.txt
```

Le mélange n'entre pas dans `make test`. Le selftest vérifie seulement que les six fondateurs
sont encore des copieurs, et qu'ils restent distincts.

## Résultats

Course unique, après le commit `d2999b3`. Selftest : 6 fondateurs encore copieurs, distincts.
Sortie brute : `mix.txt`.

| | |
|---|---:|
| copieurs à la fin | 16 317 |
| naissances | 31 912 653 |
| min. essentiels (échantillon) | 6 |
| verdict | **coexistence** |

Trois lignées restent au-dessus de 10 %. Aucune ne passe 50 %.

| lignée | copieurs | part |
|---:|---:|---:|
| 0 | 5 976 | 0,366 |
| 2 | 5 229 | 0,320 |
| 24 | 4 542 | 0,278 |
| 13 | 526 | 0,032 |
| 29 | 42 | 0,003 |
| 17 | 2 | 0,000 |

Le dominant de la fin est une descendance de la graine 0 : 388 copies, distance 24 du fondateur, 6 octets essentiels. Aucun copieur n'est plus proche d'un autre fondateur que du sien (`autre` = 0 à chaque report).

Les trois qui tiennent (0, 2, 24) sont celles qui avaient envahi leur bocal le plus tôt. Les trois tardives (13, 17, 29) passent sous 10 % et y restent.

## Faire vivre

Les trois lignées qui tiennent deviennent fondateurs d'un terrarium, reproduction par **leur** copie
(`--repro copy`) et pont d'énergie (`--osmo 1`). C'est le pont déjà mesuré dans `soup-terrarium` :
sans osmotrophie, tout s'éteint. SPLIT reste éteint : n'importe quel octet se diviserait, l'origine
soupe n'aurait plus de sens.

```console
./experiments/competition dump
make experiments/terrarium_run
./experiments/terrarium_run run --dir experiments/vie-monde \
    --repro copy --osmo 1 \
    --founders soup:experiments/competition-ligne/survivors.bin \
    --seed 1 --ticks 0
```

`--ticks 0` : le monde continue, checkpoint toutes les 2000 ticks. Relancer la même commande reprend.

Graine du terrarium : **7**. Les graines 1 à 6 et 8 s'éteignent au goulot de fondation
(600 à 2600 ticks), déjà vu dans `soup-terrarium` (6/10 tiennent). Au tick 8000, seed 7 :
1 504 créatures, 256 fondateurs tous copieurs. Le monde est dans `experiments/vie-monde/`
(hors git, checkpoint).
