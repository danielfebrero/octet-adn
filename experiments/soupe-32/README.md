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
