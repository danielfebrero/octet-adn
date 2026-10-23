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

Course unique, après le commit `68cd69a`. Fork t = 13 036 000, pop = 3 878.
Pid 65645 laissé vivant.

| bras | pop finale | K | sp1 | verdict |
|---|---:|---:|---:|---|
| témoin, +160 000 ticks | 3 890 (min 3 799, max 3 943) | 4 | 0 | **tenue** |
| forces + expand + regrow 20 | **836** (min 355) | **8** | 1 (max 43, 9,4 %) | **effondrement + colonise + forces_rares** |

Le gène `ATTACK` était déjà dans les germens du plateau 13 M. `--forces 1` n'injecte personne :
à t+400 ticks, 5 créatures sont espèce 1 et les attaques ont commencé (événement `forces`).
Elles tiennent ≥ 1 % de la pop sur 162 / 800 échantillons (max 9,4 % à t = 13 084 800),
puis retombent à 1 individu à la porte. Ce n'est pas un prédateur collé par le CLI.

La chute 3 878 → 740 en 200 ticks est la nappe `--regrow 20` (4× moins d'événements abiotiques),
pas les 5 attaquants. Le témoin, même checkpoint, reste plein. Les cadavres (+4) n'empêchent
pas le trou ; la pop se repose vers ~800–900 pendant que l'espace grandit.

Quatre îles vides à t = 13 040 000, 13 080 000, 13 120 000, 13 160 000. À la fin : K = 8,
8 / 8 peuplées, copier_frac = 0,98. L'univers s'est étendu. Ils l'ont colonisé.

Sortie brute : `results.txt`. Le monde original continue.
