# Recettes + CMAX : plus de briques, plus de créatures par île

> Simulation dans une VM. Les trois mondes vivants restent intacts :
> `vie-monde` (pid 375), `vie-forces` (pid 397), `vie-causes` (pid 504).

Deux leviers demandés ensemble.

**Recettes.** `EMIT` pose toujours la pierre. `LOOP`, `NEURAL`, `SENSE`
entrent dans l'octet-recette (bits 16/32/64), comme YIELD/XOR/ATTACK.
`LOOP` sur la case : sol motorisé (+1 énergie, le métabolisme s'annule).
`NEURAL` : puits (nourriture < 8). `SENSE` colore la recette (100 % chez
les originaux, donc presque toute brique le porte).

**CMAX.** Le cœur passe de 1024 à 4096 créatures par liste. `--max-pop`
devient un choc à la reprise. Les checkpoints OADNTER4 (World 1024) se
relisent ; l'écriture est OADNTER5.

## La phrase (écrite avant le compte)

On copie `state.ck` du monde original. Les trois pids continuent. Deux bras,
80 000 ticks :

| bras | leviers |
|---|---|
| témoin | aucun (max_pop 1000, forces 0) |
| cmax | `--forces 3 --max-pop 2000` |

Portes, à la dernière époque :

| verdict | condition |
|---|---|
| extinction | pop = 0 |
| tenue | pop ≥ 85 % du fork |
| recettes_actives | LOOP/NEURAL/SENSE dans les briques (dump) |
| ile_plus_dense | une île ncs > 1000 |
| plateau_leve | pop_fin > 4000 |

`T` est le tick du fork plus 80 000. Le binaire est `terrarium_cmax` : `terrarium_run` des trois mondes vivants ne bouge pas.

```console
./experiments/terrarium_cmax run --dir experiments/vie-cmax-ctrl --ticks T
./experiments/terrarium_cmax run --dir experiments/vie-cmax --ticks T --forces 3 --max-pop 2000
```
