# Causales

Fourche de vie-traits : parler aux siens déplace de la nourriture ; parler aux autres pose une marque lisible au tick suivant.

## Identité et fonctionnement

- Identifiant : `vie-causales`.
- Famille : Terrariums historiques.
- Données : `experiments/vie-causales/` (locales, non archivées dans Git).
- Moteur figé : `experiments/terrarium_causales`.
- SHA-256 du fichier ELF au relevé : `8d209ea744d30be1cd821b6f989f65e28c7f87d4877dcb6fa9d0273e57b85db2`.
- Conteneur : `octet-mondes`.
- Processus vivant observé le **2026-10-25T17:04:16+00:00**, PID hôte **2053085**. Ce PID est un constat daté, pas un identifiant durable.

Commande du processus observé (information, ne pas lancer en double) :

```sh
experiments/terrarium_causales run --dir experiments/vie-causales --ticks 0
```

## Dernier état observé

### series.tsv

Dernière ligne complète de [experiments/vie-causales/series.tsv](../../../experiments/vie-causales/series.tsv) (instantané, pas un suivi automatique).

L’en-tête ayant déjà disparu du journal, schéma commun du terrarium repris de `vie-monde/series.tsv`.

| Mesure | Valeur |
| --- | ---: |
| `tick` | 56286000 |
| `pop` | 32000 |
| `n_geno` | 12371 |
| `H_geno` | 11.3253 |
| `n_raw` | 27542 |
| `persist_cum` | 573599 |
| `shadow_cum` | 1213663 |
| `new_persist` | 0 |
| `persist_alive` | 2053 |
| `len_mean` | 64.00 |
| `len_max` | 64 |
| `coding_mean` | 22.67 |
| `niches_occ` | 244 |
| `stable_niches` | 7 |
| `H_behav` | 4.9406 |
| `soc_pairs` | 143 |
| `soc_mi` | -0.03321 |
| `soc_null` | -0.02889 |
| `soc_excess` | -0.00432 |
| `births` | 0 |
| `deaths` | 0 |
| `emits` | 13271 |
| `n_comp` | 10149 |
| `comp_per_ind` | 13.7 |
| `comp_cum` | 864507 |
| `comp_new` | 0 |
| `comp_alive` | 151 |
| `rep_try` | 0 |
| `rep_ok` | 0 |
| `copier_frac` | 0.766 |
| `min_ess` | 15 |
| `pred_hits` | 0 |
| `scomp_cum` | 1535053 |
| `scomp_new` | 0 |
| `scomp_alive` | 55 |

### traits.tsv

Dernière ligne complète de [experiments/vie-causales/traits.tsv](../../../experiments/vie-causales/traits.tsv) (instantané, pas un suivi automatique).

| Mesure | Valeur |
| --- | ---: |
| `tick` | 56286000 |
| `n_traits` | 8 |
| `tenues` | STORE=0.791,SENSE=0.866,TURN=0.813,STEP=0.386,EAT=0.799,MOV_BA=0.606,LOOP=0.892,entre_siens=0.162 |
| `essayees` | STORE=0.791,SENSE=0.866,TURN=0.813,STEP=0.386,EAT=0.799,MOV_BA=0.606,LOOP=0.892,entre_siens=0.162 |

### lois.tsv

Dernière ligne complète de [experiments/vie-causales/lois.tsv](../../../experiments/vie-causales/lois.tsv) (instantané, pas un suivi automatique).

| Mesure | Valeur |
| --- | ---: |
| `tick` | 56286000 |
| `spend` | 0 |
| `partage` | 25958 |
| `marques` | 0 |
| `pompe_milli` | 1000 |
| `lois` | 0 |

## Lecture et continuité

Observer les journaux en lecture seule. Ne jamais tuer, relinker, reconstruire par-dessus le moteur, ni réinitialiser un vivant. Les fichiers d’état ne sont pas modifiés par cette documentation.

La paire moteur/données figure dans [ops/mondes/mondes.tsv](../../../ops/mondes/mondes.tsv). Le superviseur hôte et ses précautions sont décrits dans [ops/mondes/README.md](../../../ops/mondes/README.md) ; ne pas redémarrer le service pour une simple consultation. Une reprise éventuelle exige d’abord la preuve de l’absence du processus et le checkpoint existant.

## Sources

- [experiments/causales-ligne/README.md](../../../experiments/causales-ligne/README.md).

[Index des mondes](../MONDES.md).
