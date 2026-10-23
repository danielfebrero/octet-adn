# Forces : l'univers s'étend, nourriture et prédateur naissent des graines

> Simulation dans une VM. Le monde original `vie-monde` (pid 65645) n'est pas touché.

Daniel : expansion douce, et que nourriture / prédateur soient des **résultantes de l'évolution des seeds**.
Le texte joint (M3C3) : tout part d'un rien, puis le binaire, puis les forces — le 1 avec le 0, le A avec le B.

La saison amp=1 a tué le fork (PR #24). Ici on n'injecte ni hiver, ni prédateur-environnement.

## La phrase (écrite avant le compte)

Le binaire est déjà là (la VM). Les forces, c'est le 1 qui rencontre le 0 **parce qu'un génome a muté un `ATTACK`**.
La nourriture biotique existe déjà : un cadavre laisse +4 sur sa case. Elle est invisible tant que la
repousse abiotique vaut 80 événements/tick. On la baisse. L'espace grandit : une île vide de plus,
jusqu'à KMAX=16. La migration remplit.

`--pred` (Chebyshev, nibble 15) reste éteint. Le cœur `src/` ne bouge pas.

Deux forks du même `state.ck` :

| bras | leviers |
|---|---|
| témoin | aucun — reprise telle quelle (K=4, regrow=80, species=0 partout) |
| forces | `--forces 1 --expand-every 40000 --regrow 20` |

`--forces 1` reclasse chaque créature sur son **germen** : un opcode `ATTACK` (pas un immédiat) → espèce 1,
sinon 0. `world_attack` ne frappe que l'autre espèce. Un prédateur est donc une seed qui a muté l'octet,
pas un agent collé par le CLI.

`--expand-every 40000` ajoute une île vide (nourriture primordiale, personne dessus). K 4→8 sur 160 000 ticks.
`--regrow 20` : la nappe abiotique recule, les cadavres comptent.

160 000 ticks (~4 expansions). Pid 65645 continue.

Portes, à la dernière époque du fork (écrites avant le compte) :

| verdict | condition |
|---|---|
| extinction | pop = 0 |
| effondrement | pop < 50 % de la pop au fork |
| forces_nues | 0 espèce-1, 0 attaques |
| forces_rares | espèce-1 < 1 % de la pop, attaques > 0 |
| forces_tenues | espèce-1 ≥ 1 % **et** pop ≥ 50 % du fork |
| colonise | K > K_fork **et** au moins une île nouvelle peuplée |
| tenue | pop ≥ 85 % du fork, 3 niches stables (comme PR #24) |

Un bras peut porter deux portes (colonise + forces_nues, etc.).

```console
make experiments/terrarium_run
# forks : experiments/vie-forces-ctrl  et  experiments/vie-forces  (hors git)
./experiments/terrarium_run run --dir experiments/vie-forces-ctrl --ticks T
./experiments/terrarium_run run --dir experiments/vie-forces --ticks T \
    --forces 1 --expand-every 40000 --regrow 20
```

Le choc n'entre pas dans `make test`. `selftest` ne passe pas ces flags : la reprise reste déterministe.
Les checkpoints `OADNTER3` (monde original) se relisent ; les nouveaux s'écrivent `OADNTER4`.

## Résultats

Course unique, après le commit de ce protocole. Voir `results.txt`.
