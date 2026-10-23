# Perturbation : une saison sur le monde vivant

> Simulation dans une VM. Le monde original `vie-monde` n'est pas touché.

Daniel demande une perturbation pour voir la réaction. Les prédateurs exterminent
les copieurs-soupe à la fondation (soup-terrarium, t ≤ 1400). Ici le choc est
**climatique** : une vague de nourriture, sur une copie du checkpoint à ~10 M ticks.

## La phrase (écrite avant le compte)

On copie `state.ck` du monde seed 7 (copy + osmo, 4 îles). Pid 65645 continue.
Deux forks du même fichier, 120 000 ticks (~30 périodes) :

| bras | choc |
|---|---|
| témoin | aucun — reprise telle quelle |
| saison | `--season-amp 1 --season-period 4000` : la repousse oscille de 0 à 2× |

Portes, à la dernière époque du fork (écrites avant le compte) :

| verdict | condition |
|---|---|
| extinction | pop = 0 |
| effondrement | pop < 50 % de la pop au fork |
| tenue | pop ≥ 85 % de la pop au fork, 3 niches stables |
| réaction | le reste |

```console
make experiments/terrarium_run
# forks : experiments/vie-choc-ctrl  et  experiments/vie-choc  (hors git)
./experiments/terrarium_run run --dir experiments/vie-choc-ctrl --ticks T
./experiments/terrarium_run run --dir experiments/vie-choc --ticks T \
    --season-amp 1 --season-period 4000
```

Le choc n'entre pas dans `make test`. `selftest` ne passe pas ces flags : la reprise
reste déterministe.

## Résultats

Course unique, après le commit `2854a61`. Fork t = 11 204 000, pop = 3 878.
Pid 65645 laissé vivant.

| bras | pop finale | verdict |
|---|---:|---|
| témoin, +120 000 ticks | 3 883 (min 3 801, max 3 952) | tenue |
| saison amp=1, période 4 000 | **0** à t = 11 207 000 | **extinction** |

Ils remplissent le bocal pendant la crue (3 998 / 4 000, naissances qui s'effondrent).
Au premier creux (repousse → 0), la mort dépasse la naissance : 3 972 → 1 947 → 778 → 116 → 0
en 800 ticks. Les 3 niches tiennent jusqu'au bout. Pas de fuite vers une case encore pleine.

Sortie brute : `results.txt`. Le monde original continue.
