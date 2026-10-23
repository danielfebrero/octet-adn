# Causes : plus de forces dans les octets

> Simulation dans une VM. Les deux mondes vivants restent intacts :
> `vie-monde` (pid 65645) et `vie-forces` (K=8, `--ticks 0`).

Daniel : les laisser vivre et forker ; analyser l'évolution chez les originaux ;
puis plus de forces et de causes à effet dans les octets.

`--forces 1` a révélé `ATTACK` déjà présent dans les germens. `--forces 2` étend
la même règle à d'autres opcodes **déjà dans l'ISA**, lus sur le germen, sans
toucher `src/` :

| octet (opcode, pas un immédiat) | cause → effet |
|---|---|
| `ATTACK` | espèce 1, prédation (comme `--forces 1`) |
| `YIELD` | +1 nourriture sur la case, −1 énergie (producteur) |
| `AND` / `ADD` | orientation vers le voisin 3×3 le plus semblable (attraction) |
| `XOR` | orientation à l'opposé du voisin le plus différent (répulsion) |
| `SUB` | −1 énergie au voisin le plus différent, +1 pour soi (drain) |

Le 1 avec le 0, le A avec le B : les valeurs s'attirent et se rejettent.
Rien n'est injecté comme agent. L'octet, s'il est dans la seed, fait quelque chose au monde.

## La phrase (écrite avant le compte)

On copie `state.ck` du monde original (seed 7, K=4, regrow 80, osmo). Pid 65645 continue.
Pid du fork forces aussi. Deux bras, 80 000 ticks, **même nappe, pas d'expansion** :

| bras | leviers |
|---|---|
| témoin | aucun |
| causes | `--forces 2` |

Portes, à la dernière époque :

| verdict | condition |
|---|---|
| extinction | pop = 0 |
| effondrement | pop < 50 % du fork |
| tenue | pop ≥ 85 % du fork, 3 niches stables |
| causes_nues | 0 événement yield/attr/rep/drain |
| causes_actives | au moins un de ces événements > 0 |
| producteurs | porteurs YIELD ≥ 1 % **et** food_fin > food du témoin |
| forces_rares | espèce-1 < 1 %, attaques > 0 |
| forces_tenues | espèce-1 ≥ 1 % et pop ≥ 50 % du fork |

```console
make experiments/terrarium_run
./experiments/terrarium_run dump experiments/vie-monde   # lecture, le pid continue
./experiments/terrarium_run run --dir experiments/vie-causes-ctrl --ticks T
./experiments/terrarium_run run --dir experiments/vie-causes --ticks T --forces 2
```

`selftest` ne passe pas `--forces 2`. Les checkpoints OADNTER3 se relisent.

## Originaux

Voir `originaux.txt` (dump du vivant, avant le compte).

## Résultats

Course unique après le commit de ce protocole. Voir `results.txt`.
