# Monde initial

Terrarium historique : génomes en octets, nourriture, déplacements, reproduction, mutation et migration entre îles. Référence des fourches suivantes.

## Identité et fonctionnement

- Identifiant : `vie-monde`.
- Famille : Terrariums historiques.
- Données : `experiments/vie-monde/` (locales, non archivées dans Git).
- Moteur figé : `experiments/terrarium_run`.
- SHA-256 du fichier ELF au relevé : `d91bd4d8b929c500258581579bd05e18f61ccbf52524affb5157251cf0de6d78`.
- Conteneur : `octet-mondes`.
- Processus vivant observé le **2026-10-25T17:04:16+00:00**, PID hôte **2053065**. Ce PID est un constat daté, pas un identifiant durable.

Commande du processus observé (information, ne pas lancer en double) :

```sh
experiments/terrarium_run run --dir experiments/vie-monde --ticks 0
```

## Dernier état observé

### series.tsv

Dernière ligne complète de [experiments/vie-monde/series.tsv](../../../experiments/vie-monde/series.tsv) (instantané, pas un suivi automatique).

| Mesure | Valeur |
| --- | ---: |
| `tick` | 47370800 |
| `pop` | 3833 |
| `n_geno` | 480 |
| `H_geno` | 6.8913 |
| `n_raw` | 2096 |
| `persist_cum` | 1051590 |
| `shadow_cum` | 3361952 |
| `new_persist` | 6 |
| `persist_alive` | 87 |
| `len_mean` | 64.00 |
| `len_max` | 64 |
| `coding_mean` | 17.27 |
| `niches_occ` | 26 |
| `stable_niches` | 3 |
| `H_behav` | 1.5609 |
| `soc_pairs` | 1005 |
| `soc_mi` | -0.08720 |
| `soc_null` | -0.09083 |
| `soc_excess` | 0.00363 |
| `births` | 5958 |
| `deaths` | 5917 |
| `emits` | 5400 |
| `n_comp` | 834 |
| `comp_per_ind` | 10.0 |
| `comp_cum` | 336381 |
| `comp_new` | 2 |
| `comp_alive` | 149 |
| `rep_try` | 19136 |
| `rep_ok` | 5964 |
| `copier_frac` | 0.984 |
| `min_ess` | 7 |
| `pred_hits` | 0 |
| `scomp_cum` | 1417198 |
| `scomp_new` | 2 |
| `scomp_alive` | 164 |

## Lecture et continuité

Observer les journaux en lecture seule. Ne jamais tuer, relinker, reconstruire par-dessus le moteur, ni réinitialiser un vivant. Les fichiers d’état ne sont pas modifiés par cette documentation.

La paire moteur/données figure dans [ops/mondes/mondes.tsv](../../../ops/mondes/mondes.tsv). Le superviseur hôte et ses précautions sont décrits dans [ops/mondes/README.md](../../../ops/mondes/README.md) ; ne pas redémarrer le service pour une simple consultation. Une reprise éventuelle exige d’abord la preuve de l’absence du processus et le checkpoint existant.

## Sources

- [experiments/terrarium/README.md](../../../experiments/terrarium/README.md).

[Index des mondes](../MONDES.md).
