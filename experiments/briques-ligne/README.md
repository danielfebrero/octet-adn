# Briques : l'espace et les technologies naissent des octets

> Simulation dans une VM. Les trois mondes vivants restent intacts :
> `vie-monde` (pid 375), `vie-forces` (pid 397), `vie-causes` (pid 504).

Le plateau ~4000 n'est pas une loi. C'est 4 îles × `max_pop` 1000. Le cœur
(`CMAX` 1024 dans `src/`) tient une liste par île ; on ne le touche pas.
`KMAX` passe à 32. `--expand-every` était une horloge. `--forces 3` donne
la même expansion aux seeds.

`EMIT` est le maçon (1 % chez les originaux). La recette de la brique est
les autres opcodes du germen. Une composition 2×2 de recettes différentes
irrigue. 64 briques sur une île ouvrent une île vide (coût : 32 briques
consommées). La migration la colonise.

| octet (opcode, pas un immédiat) | cause → effet |
|---|---|
| `EMIT` | pose une brique sur la case (si vide, −2 énergie) |
| `EMIT` + `YIELD` | recette ferme : +1 nourriture / tick, cap 14 |
| `EMIT` + `XOR` | recette répulsion (bit 2) |
| `EMIT` + `ATTACK` | recette force (bit 3) |
| 2×2 recettes distinctes | irrigation des 4 cases |
| 64 briques / île | +1 île vide, jusqu'à `KMAX` 32 |

## La phrase (écrite avant le compte)

On copie `state.ck` du monde original. Les trois pids continuent. Deux bras,
80 000 ticks, **pas d'horloge `--expand-every`** :

| bras | leviers |
|---|---|
| témoin | aucun |
| briques | `--forces 3` |

Portes, à la dernière époque :

| verdict | condition |
|---|---|
| extinction | pop = 0 |
| effondrement | pop < 50 % du fork |
| tenue | pop ≥ 85 % du fork |
| briques_nues | 0 pose |
| briques_posees | poses > 0 |
| tech_composee | tech > 0 |
| espace_bati | K_fin > K_fork |
| plateau_leve | pop_fin > 4000 |

```console
make experiments/terrarium_run
./experiments/terrarium_run run --dir experiments/vie-briques-ctrl --ticks T
./experiments/terrarium_run run --dir experiments/vie-briques --ticks T --forces 3
```

`selftest` ne passe pas `--forces 3`. Les checkpoints OADNTER4 se relisent.
Le cœur `src/` est le même binaire.
