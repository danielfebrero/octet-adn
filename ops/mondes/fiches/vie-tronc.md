# Tronc

> **Archivé le 25 octobre 2026 sur ordre de Daniel.** Processus arrêté, reprise automatique retirée. Relevés historiques. [Archive et limites](../../archives/20261025/README.md).

Fourche de vie-traits réunissant forces 3, stress, attention, traits, dépense, causales et saison douce. Lois additionnelles chargées par plugins.

## Identité et fonctionnement

- Identifiant : `vie-tronc`.
- Famille : Terrariums historiques.
- Données : `experiments/vie-tronc/` (locales, non archivées dans Git).
- Moteur figé : `experiments/terrarium_tronc`.
- SHA-256 du fichier ELF au relevé : `8d209ea744d30be1cd821b6f989f65e28c7f87d4877dcb6fa9d0273e57b85db2`.
- Conteneur : `octet-mondes`.
- Processus vivant observé le **2026-10-25T17:04:16+00:00**, PID hôte **2053091**. Ce PID est un constat daté, pas un identifiant durable.

Commande du processus observé (information, ne pas lancer en double) :

```sh
experiments/terrarium_tronc run --dir experiments/vie-tronc --ticks 0
```

## Dernier état observé

### series.tsv

Dernière ligne complète de [experiments/vie-tronc/series.tsv](../../../experiments/vie-tronc/series.tsv) (instantané, pas un suivi automatique).

L’en-tête ayant déjà disparu du journal, schéma commun du terrarium repris de `vie-monde/series.tsv`.

| Mesure | Valeur |
| --- | ---: |
| `tick` | 56345000 |
| `pop` | 32000 |
| `n_geno` | 10674 |
| `H_geno` | 11.0685 |
| `n_raw` | 26394 |
| `persist_cum` | 575884 |
| `shadow_cum` | 1220213 |
| `new_persist` | 0 |
| `persist_alive` | 2029 |
| `len_mean` | 64.00 |
| `len_max` | 64 |
| `coding_mean` | 23.54 |
| `niches_occ` | 193 |
| `stable_niches` | 5 |
| `H_behav` | 4.3969 |
| `soc_pairs` | 108 |
| `soc_mi` | -0.01653 |
| `soc_null` | -0.02114 |
| `soc_excess` | 0.00461 |
| `births` | 0 |
| `deaths` | 0 |
| `emits` | 13249 |
| `n_comp` | 8875 |
| `comp_per_ind` | 14.5 |
| `comp_cum` | 864545 |
| `comp_new` | 0 |
| `comp_alive` | 189 |
| `rep_try` | 0 |
| `rep_ok` | 0 |
| `copier_frac` | 0.828 |
| `min_ess` | 14 |
| `pred_hits` | 0 |
| `scomp_cum` | 1535053 |
| `scomp_new` | 0 |
| `scomp_alive` | 55 |

### traits.tsv

Dernière ligne complète de [experiments/vie-tronc/traits.tsv](../../../experiments/vie-tronc/traits.tsv) (instantané, pas un suivi automatique).

| Mesure | Valeur |
| --- | ---: |
| `tick` | 56345000 |
| `n_traits` | 8 |
| `tenues` | STORE=0.913,LOAD=0.921,TURN=0.928,STEP=0.902,EAT=0.853,MOV_BA=0.789,ATTACK=0.419,entre_siens=0.377 |
| `essayees` | STORE=0.913,LOAD=0.921,TURN=0.928,STEP=0.902,EAT=0.853,MOV_BA=0.789,ATTACK=0.419,entre_siens=0.377 |

### lois.tsv

Dernière ligne complète de [experiments/vie-tronc/lois.tsv](../../../experiments/vie-tronc/lois.tsv) (instantané, pas un suivi automatique).

| Mesure | Valeur |
| --- | ---: |
| `tick` | 56345000 |
| `spend` | 0 |
| `partage` | 20341 |
| `marques` | 0 |
| `pompe_milli` | 859 |
| `lois` | 5 |

Plugins présents au relevé (la présence du fichier ne prouve pas à elle seule son activation) : `competition.so`, `heritage.so`, `memoire.so`, `osmo.so`, `renaissance.so`.

## Faits remarquables

### Cinq lois comptées et partage actif

**Observation datée — 25 octobre 2026, 17:04:16 UTC.** lois.tsv compte cinq lois, partage=20 341, marques=0 et pompe_milli=859. Les huit traits comprennent LOAD et ATTACK ; 193 niches sont occupées, dont cinq stables, pour 32 000 individus.

**Portée et limites.** Le nombre de lois et la présence des plugins ne démontrent pas que chaque plugin produit un avantage. Les effets doivent être reliés à leurs propres traces ou à une ablation.

**Preuves.** Les tableaux du [dernier état observé](#dernier-état-observé) conservent les valeurs et les chemins des journaux de cette instance. Le contexte, les définitions et les essais séparés sont dans [experiments/tronc-ligne/README.md](../../../experiments/tronc-ligne/README.md).

[Retrouver ce monde dans l’index des faits](../FAITS-REMARQUABLES.md#vie-tronc).

## Lecture et continuité

Observer les journaux en lecture seule. Ne jamais tuer, relinker, reconstruire par-dessus le moteur, ni réinitialiser un vivant. Les fichiers d’état ne sont pas modifiés par cette documentation.

La paire moteur/données figure dans [ops/mondes/mondes.tsv](../../../ops/mondes/mondes.tsv). Le superviseur hôte et ses précautions sont décrits dans [ops/mondes/README.md](../../../ops/mondes/README.md) ; ne pas redémarrer le service pour une simple consultation. Une reprise éventuelle exige d’abord la preuve de l’absence du processus et le checkpoint existant.

## Sources

- [experiments/tronc-ligne/README.md](../../../experiments/tronc-ligne/README.md).

[Index des mondes](../MONDES.md).
