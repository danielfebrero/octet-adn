# Soupe à 32 graines : le taux d'origine du réplicateur

> Simulation dans une VM, pas une conscience ni une vie libre.

## La phrase (écrite avant le compte)

La soupe publiée à 2 graines sur 4 (`experiments/soup-replicator`, adressage B, N = 16384)
passe à 32 graines, mêmes lois. Une graine passe si `octet-adn soup` annonce un réplicateur :
au moins un copieur vérifié à une époque de report (`t_first` ≥ 0). Le copieur est celui du
moteur : copie fidèle ≥ 56/64 sur 3 partenaires bruités sur 4, et la copie copie aussi.

Physique figée, identique au 2/4 :

- adressage B, N = 16384, 16384 époques, report toutes les 256, mutation 0,00024, budget 8192
- graines `0` à `31`, dans cet ordre, y compris les quatre déjà publiées
- `src/`, `include/`, `seeds/` et `src/soup.c` ne bougent pas

Portes, écrites avant le compte (débat a2946faf, prise de Claude) :

| compte | lecture |
|---|---|
| ≤ 4 / 32 | l'origine est rare : on sélectionne la fidélité de copie |
| ≥ 12 / 32 | on passe à la compétition entre lignées |
| 5 à 11 | aucune des deux portes : le taux est le compte |

Un raté se publie tel quel. On ne raccourcit pas les époques et on ne change pas le détecteur
après avoir vu des graines.

```console
bash experiments/soupe-32/run.sh
```

Le script n'entre pas dans `make test`.

## Résultats

Mesuré après le commit `7530f4e`. Trente-deux courses jusqu'au bout. Sortie brute : `results.txt`,
une course par fichier dans `runs/`.

| | |
|---|---:|
| réplicateur (`t_first` ≥ 0) | **7 / 32** |
| dont la soupe passe à moitié | 6 / 32 |
| porte | 5 à 11 : le taux est le compte |

Les graines déjà publiées se rejouent : 0 et 2 passent (`t_first` 5120 et 256), 1 et 3 non.

| graine | première époque | moitié | min. essentiels | copieurs à la fin |
|---:|---:|---:|---:|---:|
| 0 | 5120 | 5120 | 6 | 16 212 |
| 2 | 256 | 256 | 5 | 16 153 |
| 10 | 15 872 | — | 9 | 0 |
| 13 | 15 616 | 15 616 | 11 | 15 411 |
| 17 | 15 360 | 15 360 | 9 | 16 206 |
| 24 | 512 | 512 | 5 | 16 288 |
| 29 | 9216 | 9216 | 6 | 16 072 |

La graine 10 a un copieur vérifié à l'époque 15 872 (1 copieur, 9 octets essentiels) et plus aucun
à 16 128 ni à la fin. La phrase écrite avant la compte dans les 7. Elle n'occupe pas la soupe.
Les six autres finissent au-dessus de 15 000 copieurs.

**Porte du milieu.** Ni l'origine rare (≤ 4), ni la compétition (≥ 12). Le taux est 7/32.
