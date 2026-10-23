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
